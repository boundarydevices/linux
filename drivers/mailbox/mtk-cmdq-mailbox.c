// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/of.h>

#define CMDQ_OP_CODE_MASK		(0xff << CMDQ_OP_CODE_SHIFT)
#define CMDQ_NUM_CMD(t)			(t->cmd_buf_size / CMDQ_INST_SIZE)
#define CMDQ_GCE_CLK_NUM_MAX		(3)

#define CMDQ_CURR_IRQ_STATUS		0x10
#define CMDQ_SYNC_TOKEN_ID		0x60
#define CMDQ_SYNC_TOKEN_VALUE		0x64
#define CMDQ_SYNC_TOKEN_UPDATE		0x68
#define CMDQ_THR_SLOT_CYCLES		0x30
#define CMDQ_THR_BASE			0x100
#define CMDQ_THR_SIZE			0x80
#define CMDQ_THR_WARM_RESET		0x00
#define CMDQ_THR_ENABLE_TASK		0x04
#define CMDQ_THR_SUSPEND_TASK		0x08
#define CMDQ_THR_CURR_STATUS		0x0c
#define CMDQ_THR_IRQ_STATUS		0x10
#define CMDQ_THR_IRQ_ENABLE		0x14
#define CMDQ_THR_CURR_ADDR		0x20
#define CMDQ_THR_END_ADDR		0x24
#define CMDQ_THR_WAIT_TOKEN		0x30
#define CMDQ_THR_PRIORITY		0x40

#define GCE_GCTL_VALUE			0x48
#define GCE_CTRL_BY_SW				GENMASK(2, 0)
#define GCE_DDR_EN				GENMASK(18, 16)

#define CMDQ_THR_ACTIVE_SLOT_CYCLES	0x3200
#define CMDQ_THR_ENABLED		0x1
#define CMDQ_THR_DISABLED		0x0
#define CMDQ_THR_SUSPEND		0x1
#define CMDQ_THR_RESUME			0x0
#define CMDQ_THR_STATUS_SUSPENDED	BIT(1)
#define CMDQ_THR_DO_WARM_RESET		BIT(0)
#define CMDQ_THR_IRQ_DONE		0x1
#define CMDQ_THR_IRQ_ERROR		0x12
#define CMDQ_THR_IRQ_EN			(CMDQ_THR_IRQ_ERROR | CMDQ_THR_IRQ_DONE)
#define CMDQ_THR_IS_WAITING		BIT(31)

#define CMDQ_JUMP_BY_OFFSET		0x10000000
#define CMDQ_JUMP_BY_PA			0x10000001

struct cmdq_thread {
	struct mbox_chan	*chan;
	void __iomem		*base;
	struct list_head	task_busy_list;
	struct timer_list	timeout;
	u32			timeout_ms;
	struct work_struct	timeout_work;
	u64			timer_mod;
	u32			priority;
	u32			idx;
};

struct cmdq_task {
	struct cmdq		*cmdq;
	struct list_head	list_entry;
	dma_addr_t		pa_base;
	struct cmdq_thread	*thread;
	struct cmdq_pkt		*pkt; /* the packet sent from mailbox client */
};

struct cmdq {
	struct mbox_controller	mbox;
	void __iomem		*base;
	int			irq;
	u32			irq_mask;
	const struct gce_plat	*pdata;
	struct cmdq_thread	*thread;
	struct clk_bulk_data	clocks[CMDQ_GCE_CLK_NUM_MAX];
	bool			suspended;
	struct workqueue_struct	*timeout_wq;
	spinlock_t		event_lock;
	struct cmdq_backup_event_list	*cmdq_backup_event_list;
};

struct gce_plat {
	u32 thread_nr;
	u8 shift;
	bool control_by_sw;
	bool sw_ddr_en;
	u32 gce_num;
	bool gce_timer_en;
};

static void cmdq_sw_ddr_enable(struct cmdq *cmdq, bool enable)
{
	WARN_ON(clk_bulk_enable(cmdq->pdata->gce_num, cmdq->clocks));

	if (enable)
		writel(GCE_DDR_EN | GCE_CTRL_BY_SW, cmdq->base + GCE_GCTL_VALUE);
	else
		writel(GCE_CTRL_BY_SW, cmdq->base + GCE_GCTL_VALUE);

	clk_bulk_disable(cmdq->pdata->gce_num, cmdq->clocks);
}

u8 cmdq_get_shift_pa(struct mbox_chan *chan)
{
	struct cmdq *cmdq = container_of(chan->mbox, struct cmdq, mbox);

	return cmdq->pdata->shift;
}
EXPORT_SYMBOL(cmdq_get_shift_pa);

void cmdq_set_event(void *chan, u16 event_id)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);
	unsigned long flags;

	spin_lock_irqsave(&cmdq->event_lock, flags);

	writel((1L << 16) | event_id, cmdq->base + CMDQ_SYNC_TOKEN_UPDATE);

	spin_unlock_irqrestore(&cmdq->event_lock, flags);
}
EXPORT_SYMBOL(cmdq_set_event);

u32 cmdq_get_event(void *chan, u16 event_id)
{
	struct cmdq *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);
	unsigned long flags;
	u32 val = 0;

	spin_lock_irqsave(&cmdq->event_lock, flags);

	writel(0x3FF & event_id, cmdq->base + CMDQ_SYNC_TOKEN_ID);
	val = readl(cmdq->base + CMDQ_SYNC_TOKEN_VALUE);

	spin_unlock_irqrestore(&cmdq->event_lock, flags);

	return val;
}
EXPORT_SYMBOL(cmdq_get_event);

static int cmdq_thread_suspend(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	u32 status;

	writel(CMDQ_THR_SUSPEND, thread->base + CMDQ_THR_SUSPEND_TASK);

	/* If already disabled, treat as suspended successful. */
	if (!(readl(thread->base + CMDQ_THR_ENABLE_TASK) & CMDQ_THR_ENABLED))
		return 0;

	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_CURR_STATUS,
			status, status & CMDQ_THR_STATUS_SUSPENDED, 0, 10)) {
		dev_err(cmdq->mbox.dev, "suspend GCE thread 0x%x failed\n",
			(u32)(thread->base - cmdq->base));
		return -EFAULT;
	}

	return 0;
}

static void cmdq_thread_resume(struct cmdq_thread *thread)
{
	writel(CMDQ_THR_RESUME, thread->base + CMDQ_THR_SUSPEND_TASK);
}

static void cmdq_init(struct cmdq *cmdq)
{
	int i;
	u32 gctl_regval = 0;

	if (cmdq->pdata->control_by_sw)
		gctl_regval = GCE_CTRL_BY_SW;
	if (cmdq->pdata->sw_ddr_en)
		gctl_regval |= GCE_DDR_EN;

	if (gctl_regval)
		writel(gctl_regval, cmdq->base + GCE_GCTL_VALUE);

	writel(CMDQ_THR_ACTIVE_SLOT_CYCLES, cmdq->base + CMDQ_THR_SLOT_CYCLES);
	for (i = 0; i <= CMDQ_MAX_EVENT; i++)
		writel(i, cmdq->base + CMDQ_SYNC_TOKEN_UPDATE);
}

static int cmdq_thread_reset(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	u32 warm_reset;

	writel(CMDQ_THR_DO_WARM_RESET, thread->base + CMDQ_THR_WARM_RESET);
	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_WARM_RESET,
			warm_reset, !(warm_reset & CMDQ_THR_DO_WARM_RESET),
			0, 10)) {
		dev_err(cmdq->mbox.dev, "reset GCE thread 0x%x failed\n",
			(u32)(thread->base - cmdq->base));
		return -EFAULT;
	}

	return 0;
}

static void cmdq_thread_disable(struct cmdq *cmdq, struct cmdq_thread *thread)
{
	cmdq_thread_reset(cmdq, thread);
	writel(CMDQ_THR_DISABLED, thread->base + CMDQ_THR_ENABLE_TASK);
}

/* notify GCE to re-fetch commands by setting GCE thread PC */
static void cmdq_thread_invalidate_fetched_data(struct cmdq_thread *thread)
{
	writel(readl(thread->base + CMDQ_THR_CURR_ADDR),
	       thread->base + CMDQ_THR_CURR_ADDR);
}

static void cmdq_task_insert_into_thread(struct cmdq_task *task)
{
	struct device *dev = task->cmdq->mbox.dev;
	struct cmdq_thread *thread = task->thread;
	struct cmdq_task *prev_task = list_last_entry(
			&thread->task_busy_list, typeof(*task), list_entry);
	u64 *prev_task_base = prev_task->pkt->va_base;

	/* let previous task jump to this task */
	dma_sync_single_for_cpu(dev, prev_task->pa_base,
				prev_task->pkt->cmd_buf_size, DMA_TO_DEVICE);
	prev_task_base[CMDQ_NUM_CMD(prev_task->pkt) - 1] =
		(u64)CMDQ_JUMP_BY_PA << 32 |
		(task->pa_base >> task->cmdq->pdata->shift);
	dma_sync_single_for_device(dev, prev_task->pa_base,
				   prev_task->pkt->cmd_buf_size, DMA_TO_DEVICE);

	cmdq_thread_invalidate_fetched_data(thread);
}

static bool cmdq_thread_is_in_wfe(struct cmdq_thread *thread)
{
	return readl(thread->base + CMDQ_THR_WAIT_TOKEN) & CMDQ_THR_IS_WAITING;
}

static void cmdq_task_exec_done(struct cmdq_task *task, int sta)
{
	struct cmdq_cb_data data;

	data.sta = sta;
	data.pkt = task->pkt;
	mbox_chan_received_data(task->thread->chan, &data);

	list_del(&task->list_entry);
}

static void cmdq_task_handle_error(struct cmdq_task *task)
{
	struct cmdq_thread *thread = task->thread;
	struct cmdq_task *next_task;
	struct cmdq *cmdq = task->cmdq;

	dev_err(cmdq->mbox.dev, "task 0x%p error\n", task);
	WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);
	next_task = list_first_entry_or_null(&thread->task_busy_list,
			struct cmdq_task, list_entry);
	if (next_task)
		writel(next_task->pa_base >> cmdq->pdata->shift,
		       thread->base + CMDQ_THR_CURR_ADDR);
	cmdq_thread_resume(thread);
}

static void cmdq_thread_irq_handler(struct cmdq *cmdq,
				    struct cmdq_thread *thread)
{
	struct cmdq_task *task, *tmp, *curr_task = NULL;
	u32 curr_pa, irq_flag, task_end_pa;
	bool err;

	irq_flag = readl(thread->base + CMDQ_THR_IRQ_STATUS);
	writel(~irq_flag, thread->base + CMDQ_THR_IRQ_STATUS);

	/*
	 * When ISR call this function, another CPU core could run
	 * "release task" right before we acquire the spin lock, and thus
	 * reset / disable this GCE thread, so we need to check the enable
	 * bit of this GCE thread.
	 */
	if (!(readl(thread->base + CMDQ_THR_ENABLE_TASK) & CMDQ_THR_ENABLED))
		return;

	if (irq_flag & CMDQ_THR_IRQ_ERROR)
		err = true;
	else if (irq_flag & CMDQ_THR_IRQ_DONE)
		err = false;
	else
		return;

	curr_pa = readl(thread->base + CMDQ_THR_CURR_ADDR) << cmdq->pdata->shift;

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
				 list_entry) {
		task_end_pa = task->pa_base + task->pkt->cmd_buf_size;
		if (curr_pa >= task->pa_base && curr_pa < task_end_pa)
			curr_task = task;

		if (!curr_task || curr_pa == task_end_pa - CMDQ_INST_SIZE) {
			cmdq_task_exec_done(task, 0);
			kfree(task);
		} else if (err) {
			cmdq_task_exec_done(task, -ENOEXEC);
			cmdq_task_handle_error(curr_task);
			kfree(task);
		}

		if (curr_task)
			break;
	}

	task = list_first_entry_or_null(&thread->task_busy_list,
					struct cmdq_task, list_entry);

	if (!task) {
		cmdq_thread_disable(cmdq, thread);
		pr_debug("empty task thread:%u", thread->idx);
	} else {
		mod_timer(&thread->timeout, jiffies +
			  msecs_to_jiffies(thread->timeout_ms));
		thread->timer_mod = sched_clock();
		pr_debug("mod_timer pkt:0x%p timeout:%u thread:%u",
			task->pkt, thread->timeout_ms, thread->idx);
	}
}

static bool cmdq_thread_timeout_exceed(struct cmdq_thread *thread)
{
	u64 duration;

	/* If first time exec time stamp smaller than timeout value,
	 * it is last round timeout. Skip it.
	 */
	duration = div_s64(sched_clock() - thread->timer_mod, 1000000);
	if (duration < thread->timeout_ms) {
		mod_timer(&thread->timeout, jiffies +
		    msecs_to_jiffies(thread->timeout_ms - duration));
		thread->timer_mod = sched_clock();
		pr_info("thread:%u mod time:%llu dur:%llu timeout not exceed",
			thread->idx, thread->timer_mod, duration);
		return false;
	}

	return true;
}

static void cmdq_thread_handle_timeout_work(struct work_struct *work_item)
{
	struct cmdq_thread *thread = container_of(work_item,
	    struct cmdq_thread, timeout_work);
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	struct cmdq_task *task, *tmp, *timeout_task = NULL;
	unsigned long flags;
	dma_addr_t pa_curr;
	struct list_head removes;

	INIT_LIST_HEAD(&removes);

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_busy_list)) {
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	/* Check before suspend thread to prevent hurt performance. */
	if (!cmdq_thread_timeout_exceed(thread)) {
		spin_unlock_irqrestore(&thread->chan->lock, flags);
		return;
	}

	WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);

	/*
	 * Although IRQ is disabled, GCE continues to execute.
	 * It may have pending IRQ before GCE thread is suspended,
	 * so check this condition again.
	 */
	cmdq_thread_irq_handler(cmdq, thread);

	if (list_empty(&thread->task_busy_list)) {
		pr_err("thread:%u empty after irq handle in timeout", thread->idx);
		goto unlock_free_done;
	}

	/* After IRQ, first task may change. */
	if (!cmdq_thread_timeout_exceed(thread)) {
		cmdq_thread_resume(thread);
		goto unlock_free_done;
	}

	pr_err("timeout for thread:0x%p idx:%u", thread->base, thread->idx);

	pa_curr = readl(thread->base + CMDQ_THR_CURR_ADDR) << cmdq->pdata->shift;

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
				 list_entry) {
		u32 task_end_pa = task->pa_base + task->pkt->cmd_buf_size;

		if (pa_curr >= task->pa_base && pa_curr < task_end_pa) {
			timeout_task = task;
			break;
		}

		pr_info("ending not curr in timeout pkt:0x%p curr_pa:%pa", task->pkt, &pa_curr);
		cmdq_task_exec_done(task, 0);
		kfree(task);
	}

	if (timeout_task) {
		spin_unlock_irqrestore(&thread->chan->lock, flags);

		cmdq_task_exec_done(timeout_task, -ETIMEDOUT);

		spin_lock_irqsave(&thread->chan->lock, flags);

		task = list_first_entry_or_null(&thread->task_busy_list,
		    struct cmdq_task, list_entry);
		if (timeout_task == task) {
			cmdq_task_exec_done(task, -ETIMEDOUT);
			kfree(task);
		} else {
			pr_err("task list changed");
		}
	}

	task = list_first_entry_or_null(&thread->task_busy_list,
					struct cmdq_task, list_entry);
	if (task) {
		mod_timer(&thread->timeout, jiffies +
			msecs_to_jiffies(thread->timeout_ms));
		thread->timer_mod = sched_clock();
		cmdq_thread_reset(cmdq, thread);
		cmdq_thread_resume(thread);
	} else {
		cmdq_thread_resume(thread);
		cmdq_thread_disable(cmdq, thread);
	}

unlock_free_done:
	spin_unlock_irqrestore(&thread->chan->lock, flags);

	list_for_each_entry_safe(task, tmp, &removes, list_entry) {
		list_del(&task->list_entry);
		kfree(task);
	}
}

static void cmdq_thread_handle_timeout(struct timer_list *t)
{
	struct cmdq_thread *thread = from_timer(thread, t, timeout);
	struct cmdq *cmdq = container_of(thread->chan->mbox, struct cmdq, mbox);
	unsigned long flags;
	bool empty;

	spin_lock_irqsave(&thread->chan->lock, flags);
	empty = list_empty(&thread->task_busy_list);
	spin_unlock_irqrestore(&thread->chan->lock, flags);
	if (empty)
		return;

	if (!work_pending(&thread->timeout_work)) {
		pr_debug("queue cmdq timeout thread:%u", thread->idx);
		queue_work(cmdq->timeout_wq, &thread->timeout_work);
	} else {
		pr_info("ignore cmdq timeout thread:%u", thread->idx);
	}
}

static irqreturn_t cmdq_irq_handler(int irq, void *dev)
{
	struct cmdq *cmdq = dev;
	unsigned long irq_status, flags = 0L;
	int bit;

	irq_status = readl(cmdq->base + CMDQ_CURR_IRQ_STATUS) & cmdq->irq_mask;
	if (!(irq_status ^ cmdq->irq_mask))
		return IRQ_NONE;

	for_each_clear_bit(bit, &irq_status, cmdq->pdata->thread_nr) {
		struct cmdq_thread *thread = &cmdq->thread[bit];

		spin_lock_irqsave(&thread->chan->lock, flags);
		cmdq_thread_irq_handler(cmdq, thread);
		spin_unlock_irqrestore(&thread->chan->lock, flags);
	}

	return IRQ_HANDLED;
}

static int cmdq_suspend(struct device *dev)
{
	struct cmdq *cmdq = dev_get_drvdata(dev);
	struct cmdq_thread *thread;
	int i;
	bool task_running = false;

	cmdq->suspended = true;

	for (i = 0; i < cmdq->pdata->thread_nr; i++) {
		thread = &cmdq->thread[i];
		if (!list_empty(&thread->task_busy_list)) {
			task_running = true;
			break;
		}
	}

	if (task_running)
		dev_warn(dev, "exist running task(s) in suspend\n");

	if (cmdq->pdata->sw_ddr_en)
		cmdq_sw_ddr_enable(cmdq, false);

	clk_bulk_disable_unprepare(cmdq->pdata->gce_num, cmdq->clocks);

	return 0;
}

static int cmdq_resume(struct device *dev)
{
	struct cmdq *cmdq = dev_get_drvdata(dev);

	WARN_ON(clk_bulk_prepare_enable(cmdq->pdata->gce_num, cmdq->clocks));
	cmdq->suspended = false;

	if (cmdq->pdata->sw_ddr_en)
		cmdq_sw_ddr_enable(cmdq, true);

	return 0;
}

static int cmdq_remove(struct platform_device *pdev)
{
	struct cmdq *cmdq = platform_get_drvdata(pdev);

	if (cmdq->pdata->sw_ddr_en)
		cmdq_sw_ddr_enable(cmdq, false);

	clk_bulk_disable_unprepare(cmdq->pdata->gce_num, cmdq->clocks);
	return 0;
}

static int cmdq_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data;
	struct cmdq_thread *thread = (struct cmdq_thread *)chan->con_priv;
	struct cmdq *cmdq = dev_get_drvdata(chan->mbox->dev);
	struct cmdq_task *task;
	unsigned long curr_pa, end_pa;

	/* Client should not flush new tasks if suspended. */
	WARN_ON(cmdq->suspended);

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	if (!task)
		return -ENOMEM;

	task->cmdq = cmdq;
	INIT_LIST_HEAD(&task->list_entry);
	task->pa_base = pkt->pa_base;
	task->thread = thread;
	task->pkt = pkt;

	if (list_empty(&thread->task_busy_list)) {
		/*
		 * The thread reset will clear thread related register to 0,
		 * including pc, end, priority, irq, suspend and enable. Thus
		 * set CMDQ_THR_ENABLED to CMDQ_THR_ENABLE_TASK will enable
		 * thread and make it running.
		 */
		WARN_ON(cmdq_thread_reset(cmdq, thread) < 0);

		writel(task->pa_base >> cmdq->pdata->shift,
		       thread->base + CMDQ_THR_CURR_ADDR);
		writel((task->pa_base + pkt->cmd_buf_size) >> cmdq->pdata->shift,
		       thread->base + CMDQ_THR_END_ADDR);

		writel(thread->priority, thread->base + CMDQ_THR_PRIORITY);
		writel(CMDQ_THR_IRQ_EN, thread->base + CMDQ_THR_IRQ_ENABLE);
		writel(CMDQ_THR_ENABLED, thread->base + CMDQ_THR_ENABLE_TASK);
		if (thread->timeout_ms != 0) {
			mod_timer(&thread->timeout, jiffies +
				msecs_to_jiffies(thread->timeout_ms));
			thread->timer_mod = sched_clock();
		}
	} else {
		WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);
		curr_pa = readl(thread->base + CMDQ_THR_CURR_ADDR) <<
			cmdq->pdata->shift;
		end_pa = readl(thread->base + CMDQ_THR_END_ADDR) <<
			cmdq->pdata->shift;
		/* check boundary */
		if (curr_pa == end_pa - CMDQ_INST_SIZE ||
		    curr_pa == end_pa) {
			/* set to this task directly */
			writel(task->pa_base >> cmdq->pdata->shift,
			       thread->base + CMDQ_THR_CURR_ADDR);
		} else {
			cmdq_task_insert_into_thread(task);
			smp_mb(); /* modify jump before enable thread */
		}
		writel((task->pa_base + pkt->cmd_buf_size) >> cmdq->pdata->shift,
		       thread->base + CMDQ_THR_END_ADDR);
		cmdq_thread_resume(thread);
	}
	list_move_tail(&task->list_entry, &thread->task_busy_list);

	return 0;
}

static int cmdq_mbox_startup(struct mbox_chan *chan)
{
	return 0;
}

static void cmdq_mbox_shutdown(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = (struct cmdq_thread *)chan->con_priv;
	struct cmdq *cmdq = dev_get_drvdata(chan->mbox->dev);
	struct cmdq_task *task, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_busy_list))
		goto done;

	WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);

	/* make sure executed tasks have success callback */
	cmdq_thread_irq_handler(cmdq, thread);
	if (list_empty(&thread->task_busy_list))
		goto done;

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
				 list_entry) {
		cmdq_task_exec_done(task, -ECONNABORTED);
		kfree(task);
	}

	cmdq_thread_disable(cmdq, thread);

done:
	/*
	 * The thread->task_busy_list empty means thread already disable. The
	 * cmdq_mbox_send_data() always reset thread which clear disable and
	 * suspend statue when first pkt send to channel, so there is no need
	 * to do any operation here, only unlock and leave.
	 */
	spin_unlock_irqrestore(&thread->chan->lock, flags);
}

static int cmdq_mbox_flush(struct mbox_chan *chan, unsigned long timeout)
{
	struct cmdq_thread *thread = (struct cmdq_thread *)chan->con_priv;
	struct cmdq_cb_data data;
	struct cmdq *cmdq = dev_get_drvdata(chan->mbox->dev);
	struct cmdq_task *task, *tmp;
	unsigned long flags;
	u32 enable;

	spin_lock_irqsave(&thread->chan->lock, flags);
	if (list_empty(&thread->task_busy_list))
		goto out;

	WARN_ON(cmdq_thread_suspend(cmdq, thread) < 0);
	if (!cmdq_thread_is_in_wfe(thread))
		goto wait;

	list_for_each_entry_safe(task, tmp, &thread->task_busy_list,
				 list_entry) {
		data.sta = -ECONNABORTED;
		data.pkt = task->pkt;
		mbox_chan_received_data(task->thread->chan, &data);
		list_del(&task->list_entry);
		kfree(task);
	}

	cmdq_thread_resume(thread);
	cmdq_thread_disable(cmdq, thread);

out:
	spin_unlock_irqrestore(&thread->chan->lock, flags);
	return 0;

wait:
	cmdq_thread_resume(thread);
	spin_unlock_irqrestore(&thread->chan->lock, flags);
	if (readl_poll_timeout_atomic(thread->base + CMDQ_THR_ENABLE_TASK,
				      enable, enable == 0, 1, timeout)) {
		dev_err(cmdq->mbox.dev, "Fail to wait GCE thread 0x%x done\n",
			(u32)(thread->base - cmdq->base));

		return -EFAULT;
	}
	return 0;
}

static const struct mbox_chan_ops cmdq_mbox_chan_ops = {
	.send_data = cmdq_mbox_send_data,
	.startup = cmdq_mbox_startup,
	.shutdown = cmdq_mbox_shutdown,
	.flush = cmdq_mbox_flush,
};

static struct mbox_chan *cmdq_xlate(struct mbox_controller *mbox,
		const struct of_phandle_args *sp)
{
	int ind = sp->args[0];
	struct cmdq_thread *thread;

	if (ind >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	thread = (struct cmdq_thread *)mbox->chans[ind].con_priv;
	thread->priority = sp->args[1];
	thread->chan = &mbox->chans[ind];

	return &mbox->chans[ind];
}

static int cmdq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cmdq *cmdq;
	int err, i;
	struct device_node *phandle = dev->of_node;
	struct device_node *node;
	int alias_id = 0;
	static const char * const clk_name = "gce";
	static const char * const clk_names[] = { "gce0", "gce1" };
	static const char * const timer_clk_name = "gce-timer";

	cmdq = devm_kzalloc(dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	cmdq->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(cmdq->base))
		return PTR_ERR(cmdq->base);

	cmdq->irq = platform_get_irq(pdev, 0);
	if (cmdq->irq < 0)
		return cmdq->irq;

	cmdq->pdata = device_get_match_data(dev);
	if (!cmdq->pdata) {
		dev_err(dev, "failed to get match data\n");
		return -EINVAL;
	}

	cmdq->irq_mask = GENMASK(cmdq->pdata->thread_nr - 1, 0);

	dev_dbg(dev, "cmdq device: addr:0x%p, va:0x%p, irq:%d\n",
		dev, cmdq->base, cmdq->irq);

	if (cmdq->pdata->gce_num > 1) {
		for_each_child_of_node(phandle->parent, node) {
			alias_id = of_alias_get_id(node, clk_name);
			if (alias_id >= 0 && alias_id < cmdq->pdata->gce_num) {
				cmdq->clocks[alias_id].id = clk_names[alias_id];
				cmdq->clocks[alias_id].clk = of_clk_get(node, 0);
				if (IS_ERR(cmdq->clocks[alias_id].clk)) {
					of_node_put(node);
					return dev_err_probe(dev,
							     PTR_ERR(cmdq->clocks[alias_id].clk),
							     "failed to get gce clk: %d\n",
							     alias_id);
				}
			}
		}
	} else {
		cmdq->clocks[alias_id].id = clk_name;
		cmdq->clocks[alias_id].clk = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(cmdq->clocks[alias_id].clk)) {
			return dev_err_probe(dev, PTR_ERR(cmdq->clocks[alias_id].clk),
					     "failed to get gce clk\n");
		}
	}

	if (cmdq->pdata->gce_timer_en) {
		int gce_timer_idx = cmdq->pdata->gce_num - 1;

		cmdq->clocks[gce_timer_idx].id = timer_clk_name;
		cmdq->clocks[gce_timer_idx].clk = devm_clk_get(&pdev->dev, timer_clk_name);
		if (IS_ERR(cmdq->clocks[cmdq->pdata->gce_num].clk)) {
			return dev_err_probe(dev, PTR_ERR(cmdq->clocks[gce_timer_idx].clk),
					     "failed to get gce timer clk\n");
		}
	}

	cmdq->mbox.dev = dev;
	cmdq->mbox.chans = devm_kcalloc(dev, cmdq->pdata->thread_nr,
					sizeof(*cmdq->mbox.chans), GFP_KERNEL);
	if (!cmdq->mbox.chans)
		return -ENOMEM;

	cmdq->mbox.num_chans = cmdq->pdata->thread_nr;
	cmdq->mbox.ops = &cmdq_mbox_chan_ops;
	cmdq->mbox.of_xlate = cmdq_xlate;

	/* make use of TXDONE_BY_ACK */
	cmdq->mbox.txdone_irq = false;
	cmdq->mbox.txdone_poll = false;

	cmdq->thread = devm_kcalloc(dev, cmdq->pdata->thread_nr,
					sizeof(*cmdq->thread), GFP_KERNEL);
	if (!cmdq->thread)
		return -ENOMEM;

	for (i = 0; i < cmdq->pdata->thread_nr; i++) {
		cmdq->thread[i].idx = i;
		cmdq->thread[i].base = cmdq->base + CMDQ_THR_BASE +
				CMDQ_THR_SIZE * i;
		cmdq->thread[i].timeout_ms = CMDQ_TIMEOUT_DEFAULT;
		INIT_LIST_HEAD(&cmdq->thread[i].task_busy_list);
		cmdq->mbox.chans[i].con_priv = (void *)&cmdq->thread[i];
		timer_setup(&cmdq->thread[i].timeout, cmdq_thread_handle_timeout, 0);
		INIT_WORK(&cmdq->thread[i].timeout_work, cmdq_thread_handle_timeout_work);
	}

	err = devm_mbox_controller_register(dev, &cmdq->mbox);
	if (err < 0) {
		dev_err(dev, "failed to register mailbox: %d\n", err);
		return err;
	}

	cmdq->timeout_wq = create_singlethread_workqueue("cmdq_timeout_handler");

	platform_set_drvdata(pdev, cmdq);

	WARN_ON(clk_bulk_prepare_enable(cmdq->pdata->gce_num, cmdq->clocks));
	spin_lock_init(&cmdq->event_lock);

	cmdq_init(cmdq);

	err = devm_request_irq(dev, cmdq->irq, cmdq_irq_handler, IRQF_SHARED,
			       "mtk_cmdq", cmdq);
	if (err < 0) {
		dev_err(dev, "failed to register ISR (%d)\n", err);
		return err;
	}

	return 0;
}

static const struct dev_pm_ops cmdq_pm_ops = {
	.suspend = cmdq_suspend,
	.resume = cmdq_resume,
};

static const struct gce_plat gce_plat_v2 = {
	.thread_nr = 16,
	.shift = 0,
	.control_by_sw = false,
	.gce_num = 1
};

static const struct gce_plat gce_plat_v3 = {
	.thread_nr = 24,
	.shift = 0,
	.control_by_sw = false,
	.gce_num = 1
};

static const struct gce_plat gce_plat_v4 = {
	.thread_nr = 24,
	.shift = 3,
	.control_by_sw = false,
	.gce_num = 1
};

static const struct gce_plat gce_plat_v5 = {
	.thread_nr = 24,
	.shift = 3,
	.control_by_sw = true,
	.gce_num = 1
};

static const struct gce_plat gce_plat_v6 = {
	.thread_nr = 24,
	.shift = 3,
	.control_by_sw = true,
	.gce_num = 3, // 2 if gce_timer_en == false
	.gce_timer_en = true
};

static const struct gce_plat gce_plat_v7 = {
	.thread_nr = 24,
	.shift = 3,
	.control_by_sw = true,
	.sw_ddr_en = true,
	.gce_num = 1
};

static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,mt8173-gce", .data = (void *)&gce_plat_v2},
	{.compatible = "mediatek,mt8183-gce", .data = (void *)&gce_plat_v3},
	{.compatible = "mediatek,mt8186-gce", .data = (void *)&gce_plat_v7},
	{.compatible = "mediatek,mt6779-gce", .data = (void *)&gce_plat_v4},
	{.compatible = "mediatek,mt8192-gce", .data = (void *)&gce_plat_v5},
	{.compatible = "mediatek,mt8195-gce", .data = (void *)&gce_plat_v6},
	{}
};

static struct platform_driver cmdq_drv = {
	.probe = cmdq_probe,
	.remove = cmdq_remove,
	.driver = {
		.name = "mtk_cmdq",
		.pm = &cmdq_pm_ops,
		.of_match_table = cmdq_of_ids,
	}
};

static int __init cmdq_drv_init(void)
{
	return platform_driver_register(&cmdq_drv);
}

static void __exit cmdq_drv_exit(void)
{
	platform_driver_unregister(&cmdq_drv);
}

subsys_initcall(cmdq_drv_init);
module_exit(cmdq_drv_exit);

MODULE_LICENSE("GPL v2");

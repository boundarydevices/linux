// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/eventfd.h>
#include <linux/device/driver.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/gunyah.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/printk.h>

#include <uapi/linux/gunyah.h>

struct gunyah_irqfd {
	struct gunyah_resource *ghrsc;
	struct gunyah_vm_resource_ticket ticket;
	struct gunyah_vm_function_instance *f;

	bool level;

	struct eventfd_ctx *ctx;
	wait_queue_entry_t wait;
	poll_table pt;
};

static int irqfd_wakeup(wait_queue_entry_t *wait, unsigned int mode, int sync,
			void *key)
{
	struct gunyah_irqfd *irqfd =
		container_of(wait, struct gunyah_irqfd, wait);
	__poll_t flags = key_to_poll(key);
	int ret = 0;

	if (flags & EPOLLIN) {
		if (irqfd->ghrsc) {
			ret = gunyah_hypercall_bell_send(irqfd->ghrsc->capid, 1,
							 NULL);
			if (ret)
				pr_err_ratelimited(
					"Failed to inject interrupt %d: %d\n",
					irqfd->ticket.label, ret);
		} else
			pr_err_ratelimited(
				"Premature injection of interrupt\n");
	}

	return 0;
}

static void irqfd_ptable_queue_proc(struct file *file, wait_queue_head_t *wqh,
				    poll_table *pt)
{
	struct gunyah_irqfd *irq_ctx =
		container_of(pt, struct gunyah_irqfd, pt);

	add_wait_queue(wqh, &irq_ctx->wait);
}

static bool gunyah_irqfd_populate(struct gunyah_vm_resource_ticket *ticket,
				  struct gunyah_resource *ghrsc)
{
	struct gunyah_irqfd *irqfd =
		container_of(ticket, struct gunyah_irqfd, ticket);
	int ret;

	if (irqfd->ghrsc) {
		pr_warn("irqfd%d already got a Gunyah resource. Check if multiple resources with same label were configured.\n",
			irqfd->ticket.label);
		return false;
	}

	irqfd->ghrsc = ghrsc;
	if (irqfd->level) {
		/* Configure the bell to trigger when bit 0 is asserted (see
		 * irq_wakeup) and for bell to automatically clear bit 0 once
		 * received by the VM (ack_mask).  need to make sure bit 0 is cleared right away,
		 * otherwise the line will never be deasserted. Emulating edge
		 * trigger interrupt does not need to set either mask
		 * because irq is listed only once per gunyah_hypercall_bell_send
		 */
		ret = gunyah_hypercall_bell_set_mask(irqfd->ghrsc->capid, 1, 1);
		if (ret)
			pr_warn("irq %d couldn't be set as level triggered. Might cause IRQ storm if asserted\n",
				irqfd->ticket.label);
	}

	return true;
}

static void gunyah_irqfd_unpopulate(struct gunyah_vm_resource_ticket *ticket,
				    struct gunyah_resource *ghrsc)
{
}

static long gunyah_irqfd_bind(struct gunyah_vm_function_instance *f)
{
	struct gunyah_fn_irqfd_arg *args = f->argp;
	struct gunyah_irqfd *irqfd;
	__poll_t events;
	struct fd fd;
	long r;

	if (f->arg_size != sizeof(*args))
		return -EINVAL;

	/* All other flag bits are reserved for future use */
	if (args->flags & ~GUNYAH_IRQFD_FLAGS_LEVEL)
		return -EINVAL;

	irqfd = kzalloc(sizeof(*irqfd), GFP_KERNEL);
	if (!irqfd)
		return -ENOMEM;

	irqfd->f = f;
	f->data = irqfd;

	fd = fdget(args->fd);
	if (!fd_file(fd)) {
		kfree(irqfd);
		return -EBADF;
	}

	irqfd->ctx = eventfd_ctx_fileget(fd_file(fd));
	if (IS_ERR(irqfd->ctx)) {
		r = PTR_ERR(irqfd->ctx);
		goto err_fdput;
	}

	if (args->flags & GUNYAH_IRQFD_FLAGS_LEVEL)
		irqfd->level = true;

	init_waitqueue_func_entry(&irqfd->wait, irqfd_wakeup);
	init_poll_funcptr(&irqfd->pt, irqfd_ptable_queue_proc);

	irqfd->ticket.resource_type = GUNYAH_RESOURCE_TYPE_BELL_TX;
	irqfd->ticket.label = args->label;
	irqfd->ticket.owner = THIS_MODULE;
	irqfd->ticket.populate = gunyah_irqfd_populate;
	irqfd->ticket.unpopulate = gunyah_irqfd_unpopulate;

	r = gunyah_vm_add_resource_ticket(f->ghvm, &irqfd->ticket);
	if (r)
		goto err_ctx;

	events = vfs_poll(fd_file(fd), &irqfd->pt);
	if (events & EPOLLIN)
		pr_warn("Premature injection of interrupt\n");
	fdput(fd);

	return 0;
err_ctx:
	eventfd_ctx_put(irqfd->ctx);
err_fdput:
	fdput(fd);
	kfree(irqfd);
	return r;
}

static void gunyah_irqfd_unbind(struct gunyah_vm_function_instance *f)
{
	struct gunyah_irqfd *irqfd = f->data;
	u64 cnt;

	eventfd_ctx_remove_wait_queue(irqfd->ctx, &irqfd->wait, &cnt);
	gunyah_vm_remove_resource_ticket(irqfd->f->ghvm, &irqfd->ticket);
	eventfd_ctx_put(irqfd->ctx);
	kfree(irqfd);
}

static bool gunyah_irqfd_compare(const struct gunyah_vm_function_instance *f,
				 const void *arg, size_t size)
{
	const struct gunyah_fn_irqfd_arg *instance = f->argp, *other = arg;

	if (sizeof(*other) != size)
		return false;

	return instance->label == other->label;
}

DECLARE_GUNYAH_VM_FUNCTION_INIT(irqfd, GUNYAH_FN_IRQFD, 2, gunyah_irqfd_bind,
				gunyah_irqfd_unbind, gunyah_irqfd_compare);
MODULE_DESCRIPTION("Gunyah irqfd VM Function");
MODULE_LICENSE("GPL");

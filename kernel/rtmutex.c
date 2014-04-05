/*
 * RT-Mutexes: simple blocking mutual exclusion locks with PI support
 *
 * started by Ingo Molnar and Thomas Gleixner.
 *
 *  Copyright (C) 2004-2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *  Copyright (C) 2005-2006 Timesys Corp., Thomas Gleixner <tglx@timesys.com>
 *  Copyright (C) 2005 Kihon Technologies Inc., Steven Rostedt
 *  Copyright (C) 2006 Esben Nielsen
 *
 * Adaptive Spinlocks:
 *  Copyright (C) 2008 Novell, Inc., Gregory Haskins, Sven Dietrich,
 *                                   and Peter Morreale,
 * Adaptive Spinlocks simplification:
 *  Copyright (C) 2008 Red Hat, Inc., Steven Rostedt <srostedt@redhat.com>
 *
 *  See Documentation/rt-mutex-design.txt for details.
 */
#include <linux/spinlock.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/timer.h>

#include "rtmutex_common.h"

/*
 * lock->owner state tracking:
 *
 * lock->owner holds the task_struct pointer of the owner. Bit 0
 * is used to keep track of the "lock has waiters" state.
 *
 * owner	bit0
 * NULL		0	lock is free (fast acquire possible)
 * NULL		1	lock is free and has waiters and the top waiter
 *				is going to take the lock*
 * taskpointer	0	lock is held (fast release possible)
 * taskpointer	1	lock is held and has waiters**
 *
 * The fast atomic compare exchange based acquire and release is only
 * possible when bit 0 of lock->owner is 0.
 *
 * (*) It also can be a transitional state when grabbing the lock
 * with ->wait_lock is held. To prevent any fast path cmpxchg to the lock,
 * we need to set the bit0 before looking at the lock, and the owner may be
 * NULL in this small time, hence this can be a transitional state.
 *
 * (**) There is a small time when bit 0 is set but there are no
 * waiters. This can happen when grabbing the lock in the slow path.
 * To prevent a cmpxchg of the owner releasing the lock, we need to
 * set this bit before looking at the lock.
 */

static void
rt_mutex_set_owner(struct rt_mutex *lock, struct task_struct *owner)
{
	unsigned long val = (unsigned long)owner;

	if (rt_mutex_has_waiters(lock))
		val |= RT_MUTEX_HAS_WAITERS;

	lock->owner = (struct task_struct *)val;
}

static inline void clear_rt_mutex_waiters(struct rt_mutex *lock)
{
	lock->owner = (struct task_struct *)
			((unsigned long)lock->owner & ~RT_MUTEX_HAS_WAITERS);
}

static void fixup_rt_mutex_waiters(struct rt_mutex *lock)
{
	if (!rt_mutex_has_waiters(lock))
		clear_rt_mutex_waiters(lock);
}

static int rt_mutex_real_waiter(struct rt_mutex_waiter *waiter)
{
	return waiter && waiter != PI_WAKEUP_INPROGRESS &&
		waiter != PI_REQUEUE_INPROGRESS;
}

/*
 * We can speed up the acquire/release, if the architecture
 * supports cmpxchg and if there's no debugging state to be set up
 */
#if defined(__HAVE_ARCH_CMPXCHG) && !defined(CONFIG_DEBUG_RT_MUTEXES)
# define rt_mutex_cmpxchg(l,c,n)	(cmpxchg(&l->owner, c, n) == c)
static inline void mark_rt_mutex_waiters(struct rt_mutex *lock)
{
	unsigned long owner, *p = (unsigned long *) &lock->owner;

	do {
		owner = *p;
	} while (cmpxchg(p, owner, owner | RT_MUTEX_HAS_WAITERS) != owner);
}
#else
# define rt_mutex_cmpxchg(l,c,n)	(0)
static inline void mark_rt_mutex_waiters(struct rt_mutex *lock)
{
	lock->owner = (struct task_struct *)
			((unsigned long)lock->owner | RT_MUTEX_HAS_WAITERS);
}
#endif

static inline void init_lists(struct rt_mutex *lock)
{
	if (unlikely(!lock->wait_list.node_list.prev))
		plist_head_init(&lock->wait_list);
}

/*
 * Calculate task priority from the waiter list priority
 *
 * Return task->normal_prio when the waiter list is empty or when
 * the waiter is not allowed to do priority boosting
 */
int rt_mutex_getprio(struct task_struct *task)
{
	if (likely(!task_has_pi_waiters(task)))
		return task->normal_prio;

	return min(task_top_pi_waiter(task)->pi_list_entry.prio,
		   task->normal_prio);
}

/*
 * Called by sched_setscheduler() to check whether the priority change
 * is overruled by a possible priority boosting.
 */
int rt_mutex_check_prio(struct task_struct *task, int newprio)
{
	if (!task_has_pi_waiters(task))
		return 0;

	return task_top_pi_waiter(task)->pi_list_entry.prio <= newprio;
}

/*
 * Adjust the priority of a task, after its pi_waiters got modified.
 *
 * This can be both boosting and unboosting. task->pi_lock must be held.
 */
static void __rt_mutex_adjust_prio(struct task_struct *task)
{
	int prio = rt_mutex_getprio(task);

	if (task->prio != prio)
		rt_mutex_setprio(task, prio);
}

/*
 * Adjust task priority (undo boosting). Called from the exit path of
 * rt_mutex_slowunlock() and rt_mutex_slowlock().
 *
 * (Note: We do this outside of the protection of lock->wait_lock to
 * allow the lock to be taken while or before we readjust the priority
 * of task. We do not use the spin_xx_mutex() variants here as we are
 * outside of the debug path.)
 */
static void rt_mutex_adjust_prio(struct task_struct *task)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&task->pi_lock, flags);
	__rt_mutex_adjust_prio(task);
	raw_spin_unlock_irqrestore(&task->pi_lock, flags);
}

static void rt_mutex_wake_waiter(struct rt_mutex_waiter *waiter)
{
	if (waiter->savestate)
		wake_up_lock_sleeper(waiter->task);
	else
		wake_up_process(waiter->task);
}

/*
 * Max number of times we'll walk the boosting chain:
 */
int max_lock_depth = 1024;

/*
 * Adjust the priority chain. Also used for deadlock detection.
 * Decreases task's usage by one - may thus free the task.
 * Returns 0 or -EDEADLK.
 */
static int rt_mutex_adjust_prio_chain(struct task_struct *task,
				      int deadlock_detect,
				      struct rt_mutex *orig_lock,
				      struct rt_mutex_waiter *orig_waiter,
				      struct task_struct *top_task)
{
	struct rt_mutex *lock;
	struct rt_mutex_waiter *waiter, *top_waiter = orig_waiter;
	int detect_deadlock, ret = 0, depth = 0;
	unsigned long flags;

	detect_deadlock = debug_rt_mutex_detect_deadlock(orig_waiter,
							 deadlock_detect);

	/*
	 * The (de)boosting is a step by step approach with a lot of
	 * pitfalls. We want this to be preemptible and we want hold a
	 * maximum of two locks per step. So we have to check
	 * carefully whether things change under us.
	 */
 again:
	if (++depth > max_lock_depth) {
		static int prev_max;

		/*
		 * Print this only once. If the admin changes the limit,
		 * print a new message when reaching the limit again.
		 */
		if (prev_max != max_lock_depth) {
			prev_max = max_lock_depth;
			printk(KERN_WARNING "Maximum lock depth %d reached "
			       "task: %s (%d)\n", max_lock_depth,
			       top_task->comm, task_pid_nr(top_task));
		}
		put_task_struct(task);

		return deadlock_detect ? -EDEADLK : 0;
	}
 retry:
	/*
	 * Task can not go away as we did a get_task() before !
	 */
	raw_spin_lock_irqsave(&task->pi_lock, flags);

	waiter = task->pi_blocked_on;
	/*
	 * Check whether the end of the boosting chain has been
	 * reached or the state of the chain has changed while we
	 * dropped the locks.
	 */
	if (!rt_mutex_real_waiter(waiter))
		goto out_unlock_pi;

	/*
	 * Check the orig_waiter state. After we dropped the locks,
	 * the previous owner of the lock might have released the lock.
	 */
	if (orig_waiter && !rt_mutex_owner(orig_lock))
		goto out_unlock_pi;

	/*
	 * Drop out, when the task has no waiters. Note,
	 * top_waiter can be NULL, when we are in the deboosting
	 * mode!
	 */
	if (top_waiter && (!task_has_pi_waiters(task) ||
			   top_waiter != task_top_pi_waiter(task)))
		goto out_unlock_pi;

	/*
	 * When deadlock detection is off then we check, if further
	 * priority adjustment is necessary.
	 */
	if (!detect_deadlock && waiter->list_entry.prio == task->prio)
		goto out_unlock_pi;

	lock = waiter->lock;
	if (!raw_spin_trylock(&lock->wait_lock)) {
		raw_spin_unlock_irqrestore(&task->pi_lock, flags);
		cpu_relax();
		goto retry;
	}

	/* Deadlock detection */
	if (lock == orig_lock || rt_mutex_owner(lock) == top_task) {
		debug_rt_mutex_deadlock(deadlock_detect, orig_waiter, lock);
		raw_spin_unlock(&lock->wait_lock);
		ret = deadlock_detect ? -EDEADLK : 0;
		goto out_unlock_pi;
	}

	top_waiter = rt_mutex_top_waiter(lock);

	/* Requeue the waiter */
	plist_del(&waiter->list_entry, &lock->wait_list);
	waiter->list_entry.prio = task->prio;
	plist_add(&waiter->list_entry, &lock->wait_list);

	/* Release the task */
	raw_spin_unlock_irqrestore(&task->pi_lock, flags);
	if (!rt_mutex_owner(lock)) {
		struct rt_mutex_waiter *lock_top_waiter;

		/*
		 * If the requeue above changed the top waiter, then we need
		 * to wake the new top waiter up to try to get the lock.
		 */
		lock_top_waiter = rt_mutex_top_waiter(lock);
		if (top_waiter != lock_top_waiter)
			rt_mutex_wake_waiter(lock_top_waiter);
		raw_spin_unlock(&lock->wait_lock);
		goto out_put_task;
	}
	put_task_struct(task);

	/* Grab the next task */
	task = rt_mutex_owner(lock);
	get_task_struct(task);
	raw_spin_lock_irqsave(&task->pi_lock, flags);

	if (waiter == rt_mutex_top_waiter(lock)) {
		/* Boost the owner */
		plist_del(&top_waiter->pi_list_entry, &task->pi_waiters);
		waiter->pi_list_entry.prio = waiter->list_entry.prio;
		plist_add(&waiter->pi_list_entry, &task->pi_waiters);
		__rt_mutex_adjust_prio(task);

	} else if (top_waiter == waiter) {
		/* Deboost the owner */
		plist_del(&waiter->pi_list_entry, &task->pi_waiters);
		waiter = rt_mutex_top_waiter(lock);
		waiter->pi_list_entry.prio = waiter->list_entry.prio;
		plist_add(&waiter->pi_list_entry, &task->pi_waiters);
		__rt_mutex_adjust_prio(task);
	}

	raw_spin_unlock_irqrestore(&task->pi_lock, flags);

	top_waiter = rt_mutex_top_waiter(lock);
	raw_spin_unlock(&lock->wait_lock);

	if (!detect_deadlock && waiter != top_waiter)
		goto out_put_task;

	goto again;

 out_unlock_pi:
	raw_spin_unlock_irqrestore(&task->pi_lock, flags);
 out_put_task:
	put_task_struct(task);

	return ret;
}


#define STEAL_NORMAL  0
#define STEAL_LATERAL 1

/*
 * Note that RT tasks are excluded from lateral-steals to prevent the
 * introduction of an unbounded latency
 */
static inline int lock_is_stealable(struct task_struct *task,
				    struct task_struct *pendowner, int mode)
{
    if (mode == STEAL_NORMAL || rt_task(task)) {
	    if (task->prio >= pendowner->prio)
		    return 0;
    } else if (task->prio > pendowner->prio)
	    return 0;
    return 1;
}

/*
 * Try to take an rt-mutex
 *
 * Must be called with lock->wait_lock held.
 *
 * @lock:   the lock to be acquired.
 * @task:   the task which wants to acquire the lock
 * @waiter: the waiter that is queued to the lock's wait list. (could be NULL)
 */
static int
__try_to_take_rt_mutex(struct rt_mutex *lock, struct task_struct *task,
		       struct rt_mutex_waiter *waiter, int mode)
{
	/*
	 * We have to be careful here if the atomic speedups are
	 * enabled, such that, when
	 *  - no other waiter is on the lock
	 *  - the lock has been released since we did the cmpxchg
	 * the lock can be released or taken while we are doing the
	 * checks and marking the lock with RT_MUTEX_HAS_WAITERS.
	 *
	 * The atomic acquire/release aware variant of
	 * mark_rt_mutex_waiters uses a cmpxchg loop. After setting
	 * the WAITERS bit, the atomic release / acquire can not
	 * happen anymore and lock->wait_lock protects us from the
	 * non-atomic case.
	 *
	 * Note, that this might set lock->owner =
	 * RT_MUTEX_HAS_WAITERS in the case the lock is not contended
	 * any more. This is fixed up when we take the ownership.
	 * This is the transitional state explained at the top of this file.
	 */
	mark_rt_mutex_waiters(lock);

	if (rt_mutex_owner(lock))
		return 0;

	/*
	 * It will get the lock because of one of these conditions:
	 * 1) there is no waiter
	 * 2) higher priority than waiters
	 * 3) it is top waiter
	 */
	if (rt_mutex_has_waiters(lock)) {
		struct task_struct *pown = rt_mutex_top_waiter(lock)->task;

		if (task != pown && !lock_is_stealable(task, pown, mode))
			return 0;
	}

	/* We got the lock. */

	if (waiter || rt_mutex_has_waiters(lock)) {
		unsigned long flags;
		struct rt_mutex_waiter *top;

		raw_spin_lock_irqsave(&task->pi_lock, flags);

		/* remove the queued waiter. */
		if (waiter) {
			plist_del(&waiter->list_entry, &lock->wait_list);
			task->pi_blocked_on = NULL;
		}

		/*
		 * We have to enqueue the top waiter(if it exists) into
		 * task->pi_waiters list.
		 */
		if (rt_mutex_has_waiters(lock)) {
			top = rt_mutex_top_waiter(lock);
			top->pi_list_entry.prio = top->list_entry.prio;
			plist_add(&top->pi_list_entry, &task->pi_waiters);
		}
		raw_spin_unlock_irqrestore(&task->pi_lock, flags);
	}

	debug_rt_mutex_lock(lock);

	rt_mutex_set_owner(lock, task);

	rt_mutex_deadlock_account_lock(lock, task);

	return 1;
}

static inline int
try_to_take_rt_mutex(struct rt_mutex *lock, struct task_struct *task,
		     struct rt_mutex_waiter *waiter)
{
	return __try_to_take_rt_mutex(lock, task, waiter, STEAL_NORMAL);
}

/*
 * Task blocks on lock.
 *
 * Prepare waiter and propagate pi chain
 *
 * This must be called with lock->wait_lock held.
 */
static int task_blocks_on_rt_mutex(struct rt_mutex *lock,
				   struct rt_mutex_waiter *waiter,
				   struct task_struct *task,
				   int detect_deadlock)
{
	struct task_struct *owner = rt_mutex_owner(lock);
	struct rt_mutex_waiter *top_waiter = waiter;
	unsigned long flags;
	int chain_walk = 0, res;

	raw_spin_lock_irqsave(&task->pi_lock, flags);

	/*
	 * In the case of futex requeue PI, this will be a proxy
	 * lock. The task will wake unaware that it is enqueueed on
	 * this lock. Avoid blocking on two locks and corrupting
	 * pi_blocked_on via the PI_WAKEUP_INPROGRESS
	 * flag. futex_wait_requeue_pi() sets this when it wakes up
	 * before requeue (due to a signal or timeout). Do not enqueue
	 * the task if PI_WAKEUP_INPROGRESS is set.
	 */
	if (task != current && task->pi_blocked_on == PI_WAKEUP_INPROGRESS) {
		raw_spin_unlock_irqrestore(&task->pi_lock, flags);
		return -EAGAIN;
	}

	BUG_ON(rt_mutex_real_waiter(task->pi_blocked_on));

	__rt_mutex_adjust_prio(task);
	waiter->task = task;
	waiter->lock = lock;
	plist_node_init(&waiter->list_entry, task->prio);
	plist_node_init(&waiter->pi_list_entry, task->prio);

	/* Get the top priority waiter on the lock */
	if (rt_mutex_has_waiters(lock))
		top_waiter = rt_mutex_top_waiter(lock);
	plist_add(&waiter->list_entry, &lock->wait_list);

	task->pi_blocked_on = waiter;

	raw_spin_unlock_irqrestore(&task->pi_lock, flags);

	if (!owner)
		return 0;

	if (waiter == rt_mutex_top_waiter(lock)) {
		raw_spin_lock_irqsave(&owner->pi_lock, flags);
		plist_del(&top_waiter->pi_list_entry, &owner->pi_waiters);
		plist_add(&waiter->pi_list_entry, &owner->pi_waiters);

		__rt_mutex_adjust_prio(owner);
		if (rt_mutex_real_waiter(owner->pi_blocked_on))
			chain_walk = 1;
		raw_spin_unlock_irqrestore(&owner->pi_lock, flags);
	}
	else if (debug_rt_mutex_detect_deadlock(waiter, detect_deadlock))
		chain_walk = 1;

	if (!chain_walk)
		return 0;

	/*
	 * The owner can't disappear while holding a lock,
	 * so the owner struct is protected by wait_lock.
	 * Gets dropped in rt_mutex_adjust_prio_chain()!
	 */
	get_task_struct(owner);

	raw_spin_unlock(&lock->wait_lock);

	res = rt_mutex_adjust_prio_chain(owner, detect_deadlock, lock, waiter,
					 task);

	raw_spin_lock(&lock->wait_lock);

	return res;
}

/*
 * Wake up the next waiter on the lock.
 *
 * Remove the top waiter from the current tasks waiter list and wake it up.
 *
 * Called with lock->wait_lock held.
 */
static void wakeup_next_waiter(struct rt_mutex *lock)
{
	struct rt_mutex_waiter *waiter;
	unsigned long flags;

	raw_spin_lock_irqsave(&current->pi_lock, flags);

	waiter = rt_mutex_top_waiter(lock);

	/*
	 * Remove it from current->pi_waiters. We do not adjust a
	 * possible priority boost right now. We execute wakeup in the
	 * boosted mode and go back to normal after releasing
	 * lock->wait_lock.
	 */
	plist_del(&waiter->pi_list_entry, &current->pi_waiters);

	rt_mutex_set_owner(lock, NULL);

	raw_spin_unlock_irqrestore(&current->pi_lock, flags);

	rt_mutex_wake_waiter(waiter);
}

/*
 * Remove a waiter from a lock and give up
 *
 * Must be called with lock->wait_lock held and
 * have just failed to try_to_take_rt_mutex().
 */
static void remove_waiter(struct rt_mutex *lock,
			  struct rt_mutex_waiter *waiter)
{
	int first = (waiter == rt_mutex_top_waiter(lock));
	struct task_struct *owner = rt_mutex_owner(lock);
	unsigned long flags;
	int chain_walk = 0;

	raw_spin_lock_irqsave(&current->pi_lock, flags);
	plist_del(&waiter->list_entry, &lock->wait_list);
	current->pi_blocked_on = NULL;
	raw_spin_unlock_irqrestore(&current->pi_lock, flags);

	if (!owner)
		return;

	if (first) {

		raw_spin_lock_irqsave(&owner->pi_lock, flags);

		plist_del(&waiter->pi_list_entry, &owner->pi_waiters);

		if (rt_mutex_has_waiters(lock)) {
			struct rt_mutex_waiter *next;

			next = rt_mutex_top_waiter(lock);
			plist_add(&next->pi_list_entry, &owner->pi_waiters);
		}
		__rt_mutex_adjust_prio(owner);

		if (rt_mutex_real_waiter(owner->pi_blocked_on))
			chain_walk = 1;

		raw_spin_unlock_irqrestore(&owner->pi_lock, flags);
	}

	WARN_ON(!plist_node_empty(&waiter->pi_list_entry));

	if (!chain_walk)
		return;

	/* gets dropped in rt_mutex_adjust_prio_chain()! */
	get_task_struct(owner);

	raw_spin_unlock(&lock->wait_lock);

	rt_mutex_adjust_prio_chain(owner, 0, lock, NULL, current);

	raw_spin_lock(&lock->wait_lock);
}

/*
 * Recheck the pi chain, in case we got a priority setting
 *
 * Called from sched_setscheduler
 */
void rt_mutex_adjust_pi(struct task_struct *task)
{
	struct rt_mutex_waiter *waiter;
	unsigned long flags;

	raw_spin_lock_irqsave(&task->pi_lock, flags);

	waiter = task->pi_blocked_on;
	if (!rt_mutex_real_waiter(waiter) ||
	    waiter->list_entry.prio == task->prio) {
		raw_spin_unlock_irqrestore(&task->pi_lock, flags);
		return;
	}

	/* gets dropped in rt_mutex_adjust_prio_chain()! */
	get_task_struct(task);
	raw_spin_unlock_irqrestore(&task->pi_lock, flags);
	rt_mutex_adjust_prio_chain(task, 0, NULL, NULL, task);
}

#ifdef CONFIG_PREEMPT_RT_FULL
/*
 * preemptible spin_lock functions:
 */
static inline void rt_spin_lock_fastlock(struct rt_mutex *lock,
					 void  (*slowfn)(struct rt_mutex *lock))
{
	might_sleep();

	if (likely(rt_mutex_cmpxchg(lock, NULL, current)))
		rt_mutex_deadlock_account_lock(lock, current);
	else
		slowfn(lock);
}

static inline void rt_spin_lock_fastunlock(struct rt_mutex *lock,
					   void  (*slowfn)(struct rt_mutex *lock))
{
	if (likely(rt_mutex_cmpxchg(lock, current, NULL)))
		rt_mutex_deadlock_account_unlock(current);
	else
		slowfn(lock);
}

#ifdef CONFIG_SMP
/*
 * Note that owner is a speculative pointer and dereferencing relies
 * on rcu_read_lock() and the check against the lock owner.
 */
static int adaptive_wait(struct rt_mutex *lock,
			 struct task_struct *owner)
{
	int res = 0;

	rcu_read_lock();
	for (;;) {
		if (owner != rt_mutex_owner(lock))
			break;
		/*
		 * Ensure that owner->on_cpu is dereferenced _after_
		 * checking the above to be valid.
		 */
		barrier();
		if (!owner->on_cpu) {
			res = 1;
			break;
		}
		cpu_relax();
	}
	rcu_read_unlock();
	return res;
}
#else
static int adaptive_wait(struct rt_mutex *lock,
			 struct task_struct *orig_owner)
{
	return 1;
}
#endif

# define pi_lock(lock)			raw_spin_lock_irq(lock)
# define pi_unlock(lock)		raw_spin_unlock_irq(lock)

/*
 * Slow path lock function spin_lock style: this variant is very
 * careful not to miss any non-lock wakeups.
 *
 * We store the current state under p->pi_lock in p->saved_state and
 * the try_to_wake_up() code handles this accordingly.
 */
static void  noinline __sched rt_spin_lock_slowlock(struct rt_mutex *lock)
{
	struct task_struct *lock_owner, *self = current;
	struct rt_mutex_waiter waiter, *top_waiter;
	int ret;

	rt_mutex_init_waiter(&waiter, true);

	raw_spin_lock(&lock->wait_lock);
	init_lists(lock);

	if (__try_to_take_rt_mutex(lock, self, NULL, STEAL_LATERAL)) {
		raw_spin_unlock(&lock->wait_lock);
		return;
	}

	BUG_ON(rt_mutex_owner(lock) == self);

	/*
	 * We save whatever state the task is in and we'll restore it
	 * after acquiring the lock taking real wakeups into account
	 * as well. We are serialized via pi_lock against wakeups. See
	 * try_to_wake_up().
	 */
	pi_lock(&self->pi_lock);
	self->saved_state = self->state;
	__set_current_state(TASK_UNINTERRUPTIBLE);
	pi_unlock(&self->pi_lock);

	ret = task_blocks_on_rt_mutex(lock, &waiter, self, 0);
	BUG_ON(ret);

	for (;;) {
		/* Try to acquire the lock again. */
		if (__try_to_take_rt_mutex(lock, self, &waiter, STEAL_LATERAL))
			break;

		top_waiter = rt_mutex_top_waiter(lock);
		lock_owner = rt_mutex_owner(lock);

		raw_spin_unlock(&lock->wait_lock);

		debug_rt_mutex_print_deadlock(&waiter);

		if (top_waiter != &waiter || adaptive_wait(lock, lock_owner))
			schedule_rt_mutex(lock);

		raw_spin_lock(&lock->wait_lock);

		pi_lock(&self->pi_lock);
		__set_current_state(TASK_UNINTERRUPTIBLE);
		pi_unlock(&self->pi_lock);
	}

	/*
	 * Restore the task state to current->saved_state. We set it
	 * to the original state above and the try_to_wake_up() code
	 * has possibly updated it when a real (non-rtmutex) wakeup
	 * happened while we were blocked. Clear saved_state so
	 * try_to_wakeup() does not get confused.
	 */
	pi_lock(&self->pi_lock);
	__set_current_state(self->saved_state);
	self->saved_state = TASK_RUNNING;
	pi_unlock(&self->pi_lock);

	/*
	 * try_to_take_rt_mutex() sets the waiter bit
	 * unconditionally. We might have to fix that up:
	 */
	fixup_rt_mutex_waiters(lock);

	BUG_ON(rt_mutex_has_waiters(lock) && &waiter == rt_mutex_top_waiter(lock));
	BUG_ON(!plist_node_empty(&waiter.list_entry));

	raw_spin_unlock(&lock->wait_lock);

	debug_rt_mutex_free_waiter(&waiter);
}

/*
 * Slow path to release a rt_mutex spin_lock style
 */
static void  noinline __sched rt_spin_lock_slowunlock(struct rt_mutex *lock)
{
	raw_spin_lock(&lock->wait_lock);

	debug_rt_mutex_unlock(lock);

	rt_mutex_deadlock_account_unlock(current);

	if (!rt_mutex_has_waiters(lock)) {
		lock->owner = NULL;
		raw_spin_unlock(&lock->wait_lock);
		return;
	}

	wakeup_next_waiter(lock);

	raw_spin_unlock(&lock->wait_lock);

	/* Undo pi boosting.when necessary */
	rt_mutex_adjust_prio(current);
}

void __lockfunc rt_spin_lock(spinlock_t *lock)
{
	rt_spin_lock_fastlock(&lock->lock, rt_spin_lock_slowlock);
	spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);
}
EXPORT_SYMBOL(rt_spin_lock);

void __lockfunc __rt_spin_lock(struct rt_mutex *lock)
{
	rt_spin_lock_fastlock(lock, rt_spin_lock_slowlock);
}
EXPORT_SYMBOL(__rt_spin_lock);

#ifdef CONFIG_DEBUG_LOCK_ALLOC
void __lockfunc rt_spin_lock_nested(spinlock_t *lock, int subclass)
{
	rt_spin_lock_fastlock(&lock->lock, rt_spin_lock_slowlock);
	spin_acquire(&lock->dep_map, subclass, 0, _RET_IP_);
}
EXPORT_SYMBOL(rt_spin_lock_nested);
#endif

void __lockfunc rt_spin_unlock(spinlock_t *lock)
{
	/* NOTE: we always pass in '1' for nested, for simplicity */
	spin_release(&lock->dep_map, 1, _RET_IP_);
	rt_spin_lock_fastunlock(&lock->lock, rt_spin_lock_slowunlock);
}
EXPORT_SYMBOL(rt_spin_unlock);

void __lockfunc __rt_spin_unlock(struct rt_mutex *lock)
{
	rt_spin_lock_fastunlock(lock, rt_spin_lock_slowunlock);
}
EXPORT_SYMBOL(__rt_spin_unlock);

/*
 * Wait for the lock to get unlocked: instead of polling for an unlock
 * (like raw spinlocks do), we lock and unlock, to force the kernel to
 * schedule if there's contention:
 */
void __lockfunc rt_spin_unlock_wait(spinlock_t *lock)
{
	spin_lock(lock);
	spin_unlock(lock);
}
EXPORT_SYMBOL(rt_spin_unlock_wait);

int __lockfunc rt_spin_trylock(spinlock_t *lock)
{
	int ret = rt_mutex_trylock(&lock->lock);

	if (ret)
		spin_acquire(&lock->dep_map, 0, 1, _RET_IP_);
	return ret;
}
EXPORT_SYMBOL(rt_spin_trylock);

int __lockfunc rt_spin_trylock_bh(spinlock_t *lock)
{
	int ret;

	local_bh_disable();
	ret = rt_mutex_trylock(&lock->lock);
	if (ret) {
		migrate_disable();
		spin_acquire(&lock->dep_map, 0, 1, _RET_IP_);
	} else
		local_bh_enable();
	return ret;
}
EXPORT_SYMBOL(rt_spin_trylock_bh);

int __lockfunc rt_spin_trylock_irqsave(spinlock_t *lock, unsigned long *flags)
{
	int ret;

	*flags = 0;
	migrate_disable();
	ret = rt_mutex_trylock(&lock->lock);
	if (ret)
		spin_acquire(&lock->dep_map, 0, 1, _RET_IP_);
	else
		migrate_enable();
	return ret;
}
EXPORT_SYMBOL(rt_spin_trylock_irqsave);

int atomic_dec_and_spin_lock(atomic_t *atomic, spinlock_t *lock)
{
	/* Subtract 1 from counter unless that drops it to 0 (ie. it was 1) */
	if (atomic_add_unless(atomic, -1, 1))
		return 0;
	migrate_disable();
	rt_spin_lock(lock);
	if (atomic_dec_and_test(atomic))
		return 1;
	rt_spin_unlock(lock);
	migrate_enable();
	return 0;
}
EXPORT_SYMBOL(atomic_dec_and_spin_lock);

void
__rt_spin_lock_init(spinlock_t *lock, char *name, struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held lock:
	 */
	debug_check_no_locks_freed((void *)lock, sizeof(*lock));
	lockdep_init_map(&lock->dep_map, name, key, 0);
#endif
}
EXPORT_SYMBOL(__rt_spin_lock_init);

#endif /* PREEMPT_RT_FULL */

/**
 * __rt_mutex_slowlock() - Perform the wait-wake-try-to-take loop
 * @lock:		 the rt_mutex to take
 * @state:		 the state the task should block in (TASK_INTERRUPTIBLE
 *			 or TASK_UNINTERRUPTIBLE)
 * @timeout:		 the pre-initialized and started timer, or NULL for none
 * @waiter:		 the pre-initialized rt_mutex_waiter
 *
 * lock->wait_lock must be held by the caller.
 */
static int __sched
__rt_mutex_slowlock(struct rt_mutex *lock, int state,
		    struct hrtimer_sleeper *timeout,
		    struct rt_mutex_waiter *waiter)
{
	int ret = 0;

	for (;;) {
		/* Try to acquire the lock: */
		if (try_to_take_rt_mutex(lock, current, waiter))
			break;

		/*
		 * TASK_INTERRUPTIBLE checks for signals and
		 * timeout. Ignored otherwise.
		 */
		if (unlikely(state == TASK_INTERRUPTIBLE)) {
			/* Signal pending? */
			if (signal_pending(current))
				ret = -EINTR;
			if (timeout && !timeout->task)
				ret = -ETIMEDOUT;
			if (ret)
				break;
		}

		raw_spin_unlock(&lock->wait_lock);

		debug_rt_mutex_print_deadlock(waiter);

		schedule_rt_mutex(lock);

		raw_spin_lock(&lock->wait_lock);
		set_current_state(state);
	}

	return ret;
}

/*
 * Slow path lock function:
 */
static int __sched
rt_mutex_slowlock(struct rt_mutex *lock, int state,
		  struct hrtimer_sleeper *timeout,
		  int detect_deadlock)
{
	struct rt_mutex_waiter waiter;
	int ret = 0;

	rt_mutex_init_waiter(&waiter, false);

	raw_spin_lock(&lock->wait_lock);
	init_lists(lock);

	/* Try to acquire the lock again: */
	if (try_to_take_rt_mutex(lock, current, NULL)) {
		raw_spin_unlock(&lock->wait_lock);
		return 0;
	}

	set_current_state(state);

	/* Setup the timer, when timeout != NULL */
	if (unlikely(timeout)) {
		hrtimer_start_expires(&timeout->timer, HRTIMER_MODE_ABS);
		if (!hrtimer_active(&timeout->timer))
			timeout->task = NULL;
	}

	ret = task_blocks_on_rt_mutex(lock, &waiter, current, detect_deadlock);

	if (likely(!ret))
		ret = __rt_mutex_slowlock(lock, state, timeout, &waiter);

	set_current_state(TASK_RUNNING);

	if (unlikely(ret))
		remove_waiter(lock, &waiter);

	/*
	 * try_to_take_rt_mutex() sets the waiter bit
	 * unconditionally. We might have to fix that up.
	 */
	fixup_rt_mutex_waiters(lock);

	raw_spin_unlock(&lock->wait_lock);

	/* Remove pending timer: */
	if (unlikely(timeout))
		hrtimer_cancel(&timeout->timer);

	debug_rt_mutex_free_waiter(&waiter);

	return ret;
}

/*
 * Slow path try-lock function:
 */
static inline int
rt_mutex_slowtrylock(struct rt_mutex *lock)
{
	int ret = 0;

	raw_spin_lock(&lock->wait_lock);
	init_lists(lock);

	if (likely(rt_mutex_owner(lock) != current)) {

		ret = try_to_take_rt_mutex(lock, current, NULL);
		/*
		 * try_to_take_rt_mutex() sets the lock waiters
		 * bit unconditionally. Clean this up.
		 */
		fixup_rt_mutex_waiters(lock);
	}

	raw_spin_unlock(&lock->wait_lock);

	return ret;
}

/*
 * Slow path to release a rt-mutex:
 */
static void __sched
rt_mutex_slowunlock(struct rt_mutex *lock)
{
	raw_spin_lock(&lock->wait_lock);

	debug_rt_mutex_unlock(lock);

	rt_mutex_deadlock_account_unlock(current);

	if (!rt_mutex_has_waiters(lock)) {
		lock->owner = NULL;
		raw_spin_unlock(&lock->wait_lock);
		return;
	}

	wakeup_next_waiter(lock);

	raw_spin_unlock(&lock->wait_lock);

	/* Undo pi boosting if necessary: */
	rt_mutex_adjust_prio(current);
}

/*
 * debug aware fast / slowpath lock,trylock,unlock
 *
 * The atomic acquire/release ops are compiled away, when either the
 * architecture does not support cmpxchg or when debugging is enabled.
 */
static inline int
rt_mutex_fastlock(struct rt_mutex *lock, int state,
		  int detect_deadlock,
		  int (*slowfn)(struct rt_mutex *lock, int state,
				struct hrtimer_sleeper *timeout,
				int detect_deadlock))
{
	if (!detect_deadlock && likely(rt_mutex_cmpxchg(lock, NULL, current))) {
		rt_mutex_deadlock_account_lock(lock, current);
		return 0;
	} else
		return slowfn(lock, state, NULL, detect_deadlock);
}

static inline int
rt_mutex_timed_fastlock(struct rt_mutex *lock, int state,
			struct hrtimer_sleeper *timeout, int detect_deadlock,
			int (*slowfn)(struct rt_mutex *lock, int state,
				      struct hrtimer_sleeper *timeout,
				      int detect_deadlock))
{
	if (!detect_deadlock && likely(rt_mutex_cmpxchg(lock, NULL, current))) {
		rt_mutex_deadlock_account_lock(lock, current);
		return 0;
	} else
		return slowfn(lock, state, timeout, detect_deadlock);
}

static inline int
rt_mutex_fasttrylock(struct rt_mutex *lock,
		     int (*slowfn)(struct rt_mutex *lock))
{
	if (likely(rt_mutex_cmpxchg(lock, NULL, current))) {
		rt_mutex_deadlock_account_lock(lock, current);
		return 1;
	}
	return slowfn(lock);
}

static inline void
rt_mutex_fastunlock(struct rt_mutex *lock,
		    void (*slowfn)(struct rt_mutex *lock))
{
	if (likely(rt_mutex_cmpxchg(lock, current, NULL)))
		rt_mutex_deadlock_account_unlock(current);
	else
		slowfn(lock);
}

/**
 * rt_mutex_lock - lock a rt_mutex
 *
 * @lock: the rt_mutex to be locked
 */
void __sched rt_mutex_lock(struct rt_mutex *lock)
{
	might_sleep();

	rt_mutex_fastlock(lock, TASK_UNINTERRUPTIBLE, 0, rt_mutex_slowlock);
}
EXPORT_SYMBOL_GPL(rt_mutex_lock);

/**
 * rt_mutex_lock_interruptible - lock a rt_mutex interruptible
 *
 * @lock:		the rt_mutex to be locked
 * @detect_deadlock:	deadlock detection on/off
 *
 * Returns:
 *  0		on success
 * -EINTR	when interrupted by a signal
 * -EDEADLK	when the lock would deadlock (when deadlock detection is on)
 */
int __sched rt_mutex_lock_interruptible(struct rt_mutex *lock,
						 int detect_deadlock)
{
	might_sleep();

	return rt_mutex_fastlock(lock, TASK_INTERRUPTIBLE,
				 detect_deadlock, rt_mutex_slowlock);
}
EXPORT_SYMBOL_GPL(rt_mutex_lock_interruptible);

/**
 * rt_mutex_lock_killable - lock a rt_mutex killable
 *
 * @lock:		the rt_mutex to be locked
 * @detect_deadlock:	deadlock detection on/off
 *
 * Returns:
 *  0		on success
 * -EINTR	when interrupted by a signal
 * -EDEADLK	when the lock would deadlock (when deadlock detection is on)
 */
int __sched rt_mutex_lock_killable(struct rt_mutex *lock,
				   int detect_deadlock)
{
	might_sleep();

	return rt_mutex_fastlock(lock, TASK_KILLABLE,
				 detect_deadlock, rt_mutex_slowlock);
}
EXPORT_SYMBOL_GPL(rt_mutex_lock_killable);

/**
 * rt_mutex_timed_lock - lock a rt_mutex interruptible
 *			the timeout structure is provided
 *			by the caller
 *
 * @lock:		the rt_mutex to be locked
 * @timeout:		timeout structure or NULL (no timeout)
 * @detect_deadlock:	deadlock detection on/off
 *
 * Returns:
 *  0		on success
 * -EINTR	when interrupted by a signal
 * -ETIMEDOUT	when the timeout expired
 * -EDEADLK	when the lock would deadlock (when deadlock detection is on)
 */
int
rt_mutex_timed_lock(struct rt_mutex *lock, struct hrtimer_sleeper *timeout,
		    int detect_deadlock)
{
	might_sleep();

	return rt_mutex_timed_fastlock(lock, TASK_INTERRUPTIBLE, timeout,
				       detect_deadlock, rt_mutex_slowlock);
}
EXPORT_SYMBOL_GPL(rt_mutex_timed_lock);

/**
 * rt_mutex_trylock - try to lock a rt_mutex
 *
 * @lock:	the rt_mutex to be locked
 *
 * Returns 1 on success and 0 on contention
 */
int __sched rt_mutex_trylock(struct rt_mutex *lock)
{
	return rt_mutex_fasttrylock(lock, rt_mutex_slowtrylock);
}
EXPORT_SYMBOL_GPL(rt_mutex_trylock);

/**
 * rt_mutex_unlock - unlock a rt_mutex
 *
 * @lock: the rt_mutex to be unlocked
 */
void __sched rt_mutex_unlock(struct rt_mutex *lock)
{
	rt_mutex_fastunlock(lock, rt_mutex_slowunlock);
}
EXPORT_SYMBOL_GPL(rt_mutex_unlock);

/**
 * rt_mutex_destroy - mark a mutex unusable
 * @lock: the mutex to be destroyed
 *
 * This function marks the mutex uninitialized, and any subsequent
 * use of the mutex is forbidden. The mutex must not be locked when
 * this function is called.
 */
void rt_mutex_destroy(struct rt_mutex *lock)
{
	WARN_ON(rt_mutex_is_locked(lock));
#ifdef CONFIG_DEBUG_RT_MUTEXES
	lock->magic = NULL;
#endif
}

EXPORT_SYMBOL_GPL(rt_mutex_destroy);

/**
 * __rt_mutex_init - initialize the rt lock
 *
 * @lock: the rt lock to be initialized
 *
 * Initialize the rt lock to unlocked state.
 *
 * Initializing of a locked rt lock is not allowed
 */
void __rt_mutex_init(struct rt_mutex *lock, const char *name)
{
	lock->owner = NULL;
	plist_head_init(&lock->wait_list);

	debug_rt_mutex_init(lock, name);
}
EXPORT_SYMBOL(__rt_mutex_init);

/**
 * rt_mutex_init_proxy_locked - initialize and lock a rt_mutex on behalf of a
 *				proxy owner
 *
 * @lock: 	the rt_mutex to be locked
 * @proxy_owner:the task to set as owner
 *
 * No locking. Caller has to do serializing itself
 * Special API call for PI-futex support
 */
void rt_mutex_init_proxy_locked(struct rt_mutex *lock,
				struct task_struct *proxy_owner)
{
	rt_mutex_init(lock);
	debug_rt_mutex_proxy_lock(lock, proxy_owner);
	rt_mutex_set_owner(lock, proxy_owner);
	rt_mutex_deadlock_account_lock(lock, proxy_owner);
}

/**
 * rt_mutex_proxy_unlock - release a lock on behalf of owner
 *
 * @lock: 	the rt_mutex to be locked
 *
 * No locking. Caller has to do serializing itself
 * Special API call for PI-futex support
 */
void rt_mutex_proxy_unlock(struct rt_mutex *lock,
			   struct task_struct *proxy_owner)
{
	debug_rt_mutex_proxy_unlock(lock);
	rt_mutex_set_owner(lock, NULL);
	rt_mutex_deadlock_account_unlock(proxy_owner);
}

/**
 * rt_mutex_start_proxy_lock() - Start lock acquisition for another task
 * @lock:		the rt_mutex to take
 * @waiter:		the pre-initialized rt_mutex_waiter
 * @task:		the task to prepare
 * @detect_deadlock:	perform deadlock detection (1) or not (0)
 *
 * Returns:
 *  0 - task blocked on lock
 *  1 - acquired the lock for task, caller should wake it up
 * <0 - error
 *
 * Special API call for FUTEX_REQUEUE_PI support.
 */
int rt_mutex_start_proxy_lock(struct rt_mutex *lock,
			      struct rt_mutex_waiter *waiter,
			      struct task_struct *task, int detect_deadlock)
{
	int ret;

	raw_spin_lock(&lock->wait_lock);

	if (try_to_take_rt_mutex(lock, task, NULL)) {
		raw_spin_unlock(&lock->wait_lock);
		return 1;
	}

#ifdef CONFIG_PREEMPT_RT_FULL
	/*
	 * In PREEMPT_RT there's an added race.
	 * If the task, that we are about to requeue, times out,
	 * it can set the PI_WAKEUP_INPROGRESS. This tells the requeue
	 * to skip this task. But right after the task sets
	 * its pi_blocked_on to PI_WAKEUP_INPROGRESS it can then
	 * block on the spin_lock(&hb->lock), which in RT is an rtmutex.
	 * This will replace the PI_WAKEUP_INPROGRESS with the actual
	 * lock that it blocks on. We *must not* place this task
	 * on this proxy lock in that case.
	 *
	 * To prevent this race, we first take the task's pi_lock
	 * and check if it has updated its pi_blocked_on. If it has,
	 * we assume that it woke up and we return -EAGAIN.
	 * Otherwise, we set the task's pi_blocked_on to
	 * PI_REQUEUE_INPROGRESS, so that if the task is waking up
	 * it will know that we are in the process of requeuing it.
	 */
	raw_spin_lock_irq(&task->pi_lock);
	if (task->pi_blocked_on) {
		raw_spin_unlock_irq(&task->pi_lock);
		raw_spin_unlock(&lock->wait_lock);
		return -EAGAIN;
	}
	task->pi_blocked_on = PI_REQUEUE_INPROGRESS;
	raw_spin_unlock_irq(&task->pi_lock);
#endif

	ret = task_blocks_on_rt_mutex(lock, waiter, task, detect_deadlock);

	if (ret && !rt_mutex_owner(lock)) {
		/*
		 * Reset the return value. We might have
		 * returned with -EDEADLK and the owner
		 * released the lock while we were walking the
		 * pi chain.  Let the waiter sort it out.
		 */
		ret = 0;
	}

	if (unlikely(ret))
		remove_waiter(lock, waiter);

	raw_spin_unlock(&lock->wait_lock);

	debug_rt_mutex_print_deadlock(waiter);

	return ret;
}

/**
 * rt_mutex_next_owner - return the next owner of the lock
 *
 * @lock: the rt lock query
 *
 * Returns the next owner of the lock or NULL
 *
 * Caller has to serialize against other accessors to the lock
 * itself.
 *
 * Special API call for PI-futex support
 */
struct task_struct *rt_mutex_next_owner(struct rt_mutex *lock)
{
	if (!rt_mutex_has_waiters(lock))
		return NULL;

	return rt_mutex_top_waiter(lock)->task;
}

/**
 * rt_mutex_finish_proxy_lock() - Complete lock acquisition
 * @lock:		the rt_mutex we were woken on
 * @to:			the timeout, null if none. hrtimer should already have
 * 			been started.
 * @waiter:		the pre-initialized rt_mutex_waiter
 * @detect_deadlock:	perform deadlock detection (1) or not (0)
 *
 * Complete the lock acquisition started our behalf by another thread.
 *
 * Returns:
 *  0 - success
 * <0 - error, one of -EINTR, -ETIMEDOUT, or -EDEADLK
 *
 * Special API call for PI-futex requeue support
 */
int rt_mutex_finish_proxy_lock(struct rt_mutex *lock,
			       struct hrtimer_sleeper *to,
			       struct rt_mutex_waiter *waiter,
			       int detect_deadlock)
{
	int ret;

	raw_spin_lock(&lock->wait_lock);

	set_current_state(TASK_INTERRUPTIBLE);

	ret = __rt_mutex_slowlock(lock, TASK_INTERRUPTIBLE, to, waiter);

	set_current_state(TASK_RUNNING);

	if (unlikely(ret))
		remove_waiter(lock, waiter);

	/*
	 * try_to_take_rt_mutex() sets the waiter bit unconditionally. We might
	 * have to fix that up.
	 */
	fixup_rt_mutex_waiters(lock);

	raw_spin_unlock(&lock->wait_lock);

	return ret;
}

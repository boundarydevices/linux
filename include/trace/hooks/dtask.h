/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM dtask
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DTASK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DTASK_H

#include <trace/hooks/vendor_hooks.h>

/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */

struct mutex;
struct rt_mutex;
struct rt_mutex_base;
struct rw_semaphore;
struct task_struct;
struct percpu_rw_semaphore;

DECLARE_HOOK(android_vh_mutex_wait_start,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_mutex_wait_finish,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_mutex_opt_spin_start,
	TP_PROTO(struct mutex *lock, bool *time_out, int *cnt),
	TP_ARGS(lock, time_out, cnt));
DECLARE_HOOK(android_vh_mutex_opt_spin_finish,
	TP_PROTO(struct mutex *lock, bool taken),
	TP_ARGS(lock, taken));
DECLARE_HOOK(android_vh_mutex_can_spin_on_owner,
	TP_PROTO(struct mutex *lock, int *retval),
	TP_ARGS(lock, retval));
DECLARE_HOOK(android_vh_mutex_init,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));

DECLARE_HOOK(android_vh_rtmutex_wait_start,
	TP_PROTO(struct rt_mutex_base *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_rtmutex_wait_finish,
	TP_PROTO(struct rt_mutex_base *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_rt_mutex_steal,
	TP_PROTO(int waiter_prio, int top_waiter_prio, bool *ret),
	TP_ARGS(waiter_prio, top_waiter_prio, ret));

DECLARE_HOOK(android_vh_rwsem_read_wait_start,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_read_wait_finish,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_write_wait_start,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_write_wait_finish,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_opt_spin_start,
	TP_PROTO(struct rw_semaphore *sem, bool *time_out, int *cnt, bool chk_only),
	TP_ARGS(sem, time_out, cnt, chk_only));
DECLARE_HOOK(android_vh_rwsem_opt_spin_finish,
	TP_PROTO(struct rw_semaphore *sem, bool taken),
	TP_ARGS(sem, taken));
DECLARE_HOOK(android_vh_rwsem_can_spin_on_owner,
	TP_PROTO(struct rw_semaphore *sem, bool *ret),
	TP_ARGS(sem, ret));

DECLARE_HOOK(android_vh_sched_show_task,
	TP_PROTO(struct task_struct *task),
	TP_ARGS(task));
DECLARE_HOOK(android_vh_percpu_rwsem_wq_add,
	TP_PROTO(struct percpu_rw_semaphore *sem, bool reader),
	TP_ARGS(sem, reader));
DECLARE_HOOK(android_vh_percpu_rwsem_down_read,
	TP_PROTO(struct percpu_rw_semaphore *sem, bool try, bool *ret),
	TP_ARGS(sem, try, ret));
DECLARE_HOOK(android_vh_percpu_rwsem_up_write,
	TP_PROTO(struct percpu_rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_RESTRICTED_HOOK(android_rvh_percpu_rwsem_wait_complete,
	TP_PROTO(struct percpu_rw_semaphore *sem, long state, bool *complete),
	TP_ARGS(sem, state, complete), 1);

struct mutex_waiter;
DECLARE_HOOK(android_vh_alter_mutex_list_add,
	TP_PROTO(struct mutex *lock,
		struct mutex_waiter *waiter,
		struct list_head *list,
		bool *already_on_list),
	TP_ARGS(lock, waiter, list, already_on_list));
DECLARE_HOOK(android_vh_mutex_unlock_slowpath,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_mutex_unlock_slowpath_before_wakeq,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));

DECLARE_HOOK(android_vh_exit_signal_whether_wake,
	TP_PROTO(struct task_struct *p, bool *wake),
	TP_ARGS(p, wake));

DECLARE_HOOK(android_vh_exit_check,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));

DECLARE_HOOK(android_vh_freeze_whether_wake,
	TP_PROTO(struct task_struct *t, bool *wake),
	TP_ARGS(t, wake));

struct rt_mutex_waiter;
struct ww_acquire_ctx;
DECLARE_HOOK(android_vh_task_blocks_on_rtmutex,
	TP_PROTO(struct rt_mutex_base *lock, struct rt_mutex_waiter *waiter,
		struct task_struct *task, struct ww_acquire_ctx *ww_ctx,
		unsigned int *chwalk),
	TP_ARGS(lock, waiter, task, ww_ctx, chwalk));
DECLARE_HOOK(android_vh_rtmutex_waiter_prio,
	TP_PROTO(struct task_struct *task, int *waiter_prio),
	TP_ARGS(task, waiter_prio));
DECLARE_HOOK(android_vh_record_mutex_lock_starttime,
	TP_PROTO(struct mutex *lock, unsigned long settime_jiffies),
	TP_ARGS(lock, settime_jiffies));
DECLARE_HOOK(android_vh_record_rtmutex_lock_starttime,
	TP_PROTO(struct rt_mutex *lock, unsigned long settime_jiffies),
	TP_ARGS(lock, settime_jiffies));
DECLARE_HOOK(android_vh_record_rwsem_lock_starttime,
	TP_PROTO(struct rw_semaphore *sem, unsigned long settime_jiffies),
	TP_ARGS(sem, settime_jiffies));
DECLARE_HOOK(android_vh_record_pcpu_rwsem_starttime,
	TP_PROTO(struct percpu_rw_semaphore *sem, unsigned long settime_jiffies),
	TP_ARGS(sem, settime_jiffies));

DECLARE_HOOK(android_vh_read_lazy_flag,
	TP_PROTO(int *thread_lazy_flag, unsigned long *thread_flags),
	TP_ARGS(thread_lazy_flag, thread_flags));

DECLARE_HOOK(android_vh_set_tsk_need_resched_lazy,
	TP_PROTO(struct task_struct *p, struct rq *rq, int *need_lazy),
	TP_ARGS(p, rq, need_lazy));
#endif /* _TRACE_HOOK_DTASK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

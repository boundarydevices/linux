/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM futex
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_FUTEX_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_FUTEX_H
#include <trace/hooks/vendor_hooks.h>
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */
struct plist_node;
struct plist_head;
DECLARE_HOOK(android_vh_alter_futex_plist_add,
	TP_PROTO(struct plist_node *node,
		 struct plist_head *head,
		 bool *already_on_hb),
	TP_ARGS(node, head, already_on_hb));
DECLARE_HOOK(android_vh_futex_sleep_start,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));
DECLARE_HOOK(android_vh_do_futex,
	TP_PROTO(int cmd,
		 unsigned int *flags,
		 u32 __user *uaddr2),
	TP_ARGS(cmd, flags, uaddr2));
DECLARE_HOOK(android_vh_futex_wait_start,
	TP_PROTO(unsigned int flags,
		 u32 bitset),
	TP_ARGS(flags, bitset));
DECLARE_HOOK(android_vh_futex_wait_end,
	TP_PROTO(unsigned int flags,
		 u32 bitset),
	TP_ARGS(flags, bitset));
DECLARE_HOOK(android_vh_futex_wake_traverse_plist,
	TP_PROTO(struct plist_head *chain, int *target_nr,
		 union futex_key key, u32 bitset),
	TP_ARGS(chain, target_nr, key, bitset));
DECLARE_HOOK(android_vh_futex_wake_this,
	TP_PROTO(int ret, int nr_wake, int target_nr,
		 struct task_struct *p),
	TP_ARGS(ret, nr_wake, target_nr, p));
DECLARE_HOOK(android_vh_futex_wake_up_q_finish,
	TP_PROTO(int nr_wake, int target_nr),
	TP_ARGS(nr_wake, target_nr));
DECLARE_HOOK(android_vh_futex_wait_queue_start,
	TP_PROTO(u32 __user *uaddr,
		 unsigned int flags,
		 u32 bitset),
	TP_ARGS(uaddr, flags, bitset));
#endif /* _TRACE_HOOK_FUTEX_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

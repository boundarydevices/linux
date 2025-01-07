/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM syscall_check

#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_SYSCALL_CHECK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_SYSCALL_CHECK_H
#include <trace/hooks/vendor_hooks.h>
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */
struct file;
union bpf_attr;
struct timespec64;

DECLARE_HOOK(android_vh_check_mmap_file,
	TP_PROTO(const struct file *file, unsigned long prot,
		unsigned long flag, unsigned long ret),
	TP_ARGS(file, prot, flag, ret));

DECLARE_HOOK(android_vh_check_file_open,
	TP_PROTO(const struct file *file),
	TP_ARGS(file));

DECLARE_HOOK(android_vh_check_bpf_syscall,
	TP_PROTO(int cmd, const union bpf_attr *attr, unsigned int size),
	TP_ARGS(cmd, attr, size));

DECLARE_HOOK(android_vh_check_nanosleep_syscall,
    TP_PROTO(struct timespec64 *tu),
    TP_ARGS(tu));

DECLARE_HOOK(android_vh_destroy_inode,
	TP_PROTO(struct inode *inode),
	TP_ARGS(inode));

#endif /* _TRACE_HOOK_SYSCALL_CHECK_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

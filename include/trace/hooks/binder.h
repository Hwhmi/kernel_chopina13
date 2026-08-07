/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM binder
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_BINDER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_BINDER_H

#include <trace/hooks/vendor_hooks.h>
#include <linux/mutex.h>
#include <linux/list.h>

struct binder_alloc;
struct binder_proc;
struct binder_thread;
struct binder_transaction;
struct binder_transaction_data;

DECLARE_RESTRICTED_HOOK(android_vh_binder_alloc_new_buf_locked,
	TP_PROTO(size_t size, struct binder_alloc *alloc, int is_async),
	TP_ARGS(size, alloc, is_async), 1);

DECLARE_RESTRICTED_HOOK(android_vh_binder_reply,
	TP_PROTO(struct binder_proc *target_proc, struct binder_proc *proc,
		struct binder_thread *thread, struct binder_transaction_data *tr),
	TP_ARGS(target_proc, proc, thread, tr), 1);

DECLARE_RESTRICTED_HOOK(android_vh_binder_trans,
	TP_PROTO(struct binder_proc *target_proc, struct binder_proc *proc,
		struct binder_thread *thread, struct binder_transaction_data *tr),
	TP_ARGS(target_proc, proc, thread, tr), 1);

DECLARE_RESTRICTED_HOOK(android_vh_binder_wait_for_work,
	TP_PROTO(bool do_proc_work, struct binder_thread *thread,
		struct binder_proc *proc),
	TP_ARGS(do_proc_work, thread, proc), 1);

DECLARE_RESTRICTED_HOOK(android_vh_binder_preset,
	TP_PROTO(struct hlist_head *hhead, struct mutex *lock),
	TP_ARGS(hhead, lock), 1);

#endif /* _TRACE_HOOK_BINDER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

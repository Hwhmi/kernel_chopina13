/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2020. All rights reserved.
 *
 * File name: oem_binder.c
 * Description: millet-binder-driver
 * Author: guchao1@xiaomi.com
 * Version: 1.0
 * Date:  2020/9/9
 */
#define pr_fmt(fmt) "millet-binder_gki: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/freezer.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>
#include <linux/sched/task.h>
#include <linux/wait.h>
#include "millet.h"
#include "binder_oem.h"
#include <trace/hooks/binder.h>
#include <../../android/binder_internal.h>
#include <../../android/binder_alloc.h>
#include <../../android/dbitmap.h>

static struct hlist_head * get_binder_hhead = NULL;
static struct mutex * get_binder_lock = NULL;
static bool bgot = false;

struct oem_binder_hook oem_binder_hook_set = {
	.oem_wahead_thresh = 0,
	.oem_wahead_space = 0,
	.oem_reply_hook = NULL,
	.oem_trans_hook = NULL,
	.oem_wait4_hook = NULL,
	.oem_query_st_hook = NULL,
	.oem_buf_overflow_hook = NULL,
};

/*
 * The binder vendor hooks pass binder internal objects, but this kernel keeps
 * their full definitions private to drivers/android/binder.c.  Millet only
 * reads a small set of fields from those objects, so keep matching local
 * definitions here instead of moving Binder core internals into a public API.
 */
enum binder_stat_types {
	BINDER_STAT_PROC,
	BINDER_STAT_THREAD,
	BINDER_STAT_NODE,
	BINDER_STAT_REF,
	BINDER_STAT_DEATH,
	BINDER_STAT_TRANSACTION,
	BINDER_STAT_TRANSACTION_COMPLETE,
	BINDER_STAT_COUNT
};

struct binder_stats {
	atomic_t br[_IOC_NR(BR_ONEWAY_SPAM_SUSPECT) + 1];
	atomic_t bc[_IOC_NR(BC_REPLY_SG) + 1];
	atomic_t obj_created[BINDER_STAT_COUNT];
	atomic_t obj_deleted[BINDER_STAT_COUNT];
};

struct binder_work {
	struct list_head entry;

	enum binder_work_type {
		BINDER_WORK_TRANSACTION = 1,
		BINDER_WORK_TRANSACTION_COMPLETE,
		BINDER_WORK_TRANSACTION_ONEWAY_SPAM_SUSPECT,
		BINDER_WORK_RETURN_ERROR,
		BINDER_WORK_NODE,
		BINDER_WORK_DEAD_BINDER,
		BINDER_WORK_DEAD_BINDER_AND_CLEAR,
		BINDER_WORK_CLEAR_DEATH_NOTIFICATION,
	} type;
};

struct binder_error {
	struct binder_work work;
	uint32_t cmd;
};

struct binder_priority {
	unsigned int sched_policy;
	int prio;
};

struct binder_proc {
	struct hlist_node proc_node;
	struct rb_root threads;
	struct rb_root nodes;
	struct rb_root refs_by_desc;
	struct rb_root refs_by_node;
	struct list_head waiting_threads;
	int pid;
	struct task_struct *tsk;
	struct files_struct *files;
	struct mutex files_lock;
	const struct cred *cred;
	struct hlist_node deferred_work_node;
	int deferred_work;
	bool is_dead;
	struct list_head todo;
	struct binder_stats stats;
	struct list_head delivered_death;
	u32 max_threads;
	int requested_threads;
	int requested_threads_started;
	int tmp_ref;
	struct binder_priority default_priority;
	struct dentry *debugfs_entry;
	struct binder_alloc alloc;
	struct binder_context *context;
	spinlock_t inner_lock;
	spinlock_t outer_lock;
	struct dentry *binderfs_entry;
	bool oneway_spam_detection_enabled;
	struct dbitmap dmap;
};

struct binder_thread {
	struct binder_proc *proc;
	struct rb_node rb_node;
	struct list_head waiting_thread_node;
	int pid;
	int looper;
	bool looper_need_return;
	struct binder_transaction *transaction_stack;
	struct list_head todo;
	bool process_todo;
	struct binder_error return_error;
	struct binder_error reply_error;
	wait_queue_head_t wait;
	struct binder_stats stats;
	atomic_t tmp_ref;
	bool is_dead;
	struct task_struct *task;
};

struct binder_transaction {
	int debug_id;
	struct binder_work work;
	struct binder_thread *from;
	struct binder_transaction *from_parent;
	struct binder_proc *to_proc;
	struct binder_thread *to_thread;
	struct binder_transaction *to_parent;
	unsigned need_reply:1;
	struct binder_buffer *buffer;
	unsigned int code;
	unsigned int flags;
	struct binder_priority priority;
	struct binder_priority saved_priority;
	bool set_priority_called;
	kuid_t sender_euid;
	binder_uintptr_t security_ctx;
	spinlock_t lock;
#ifdef BINDER_WATCHDOG
	enum wait_on_reason wait_on;
	enum wait_on_reason bark_on;
	struct rb_node rb_node;
	struct timespec bark_time;
	struct timespec exe_timestamp;
	char service[MAX_SERVICE_NAME_LEN];
	pid_t fproc;
	pid_t fthrd;
	pid_t tproc;
	pid_t tthrd;
	unsigned int log_idx;
#endif
#ifdef BINDER_USER_TRACKING
	struct timespec timestamp;
	struct timeval tv;
#endif
#ifdef CONFIG_MTK_TASK_TURBO
	struct task_struct *inherit_task;
#endif
};

/**
 * binder_inner_proc_lock() - Acquire inner lock for given binder_proc
 * @proc:         struct binder_proc to acquire
 *
 * Acquires proc->inner_lock. Used to protect todo lists
 */
#define binder_inner_proc_lock(proc) _binder_inner_proc_lock(proc, __LINE__)
static void
_binder_inner_proc_lock(struct binder_proc *proc, int line)
	__acquires(&proc->inner_lock)
{
	spin_lock(&proc->inner_lock);
}

/**
 * binder_inner_proc_unlock() - Release inner lock for given binder_proc
 * @proc:         struct binder_proc to acquire
 *
 * Release lock acquired via binder_inner_proc_lock()
 */
#define binder_inner_proc_unlock(proc) _binder_inner_proc_unlock(proc, __LINE__)
static void
_binder_inner_proc_unlock(struct binder_proc *proc, int line)
	__releases(&proc->inner_lock)
{
	spin_unlock(&proc->inner_lock);
}

static bool binder_worklist_empty_ilocked(struct list_head *list)
{
	return list_empty(list);
}


static enum BINDER_STAT query_binder_stat(struct binder_proc *proc)
{
	struct rb_node *n = NULL;
	struct binder_thread *thread = NULL;
	struct binder_transaction *t;
	int pid, tid, uid = 0;
	enum BINDER_STAT stat;
	struct task_struct *tsk;

	if (!oem_binder_hook_set.oem_query_st_hook)
		return BINDER_IN_IDLE;

	if (proc->tsk)
		uid = task_uid(proc->tsk).val;
	else
		return BINDER_IN_IDLE;
	binder_inner_proc_lock(proc);
	if (proc->tsk && !binder_worklist_empty_ilocked(&proc->todo)) {
		tsk = proc->tsk;
		tid = tsk->pid;
		pid = task_pid_nr(tsk);
		stat = BINDER_PROC_IN_BUSY;
		goto busy;
	}

	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n)) {
		thread = rb_entry(n, struct binder_thread, rb_node);
		if (!thread->task)
			continue;

		if (!binder_worklist_empty_ilocked(&thread->todo)) {
			tsk = thread->task;
			pid = task_tgid_nr(tsk);
			tid = thread->pid;
			stat = BINDER_THREAD_IN_BUSY;
			goto busy;
		}

		t = READ_ONCE(thread->transaction_stack);
		if (!t)
			continue;

		spin_lock(&t->lock);
		if (READ_ONCE(thread->transaction_stack) == t &&
				t->to_thread == thread) {
			tsk = thread->task;
			pid = task_tgid_nr(tsk);
			tid = thread->pid;
			stat = BINDER_IN_TRANSACTION;
			spin_unlock(&t->lock);
			goto busy;
		}
		spin_unlock(&t->lock);
	}

	binder_inner_proc_unlock(proc);
	return BINDER_IN_IDLE;
busy:
	binder_inner_proc_unlock(proc);
//	oem_binder_hook_set.oem_query_st_hook(uid, tsk, tid, pid, stat);
	return stat;
}

void query_binder_app_stat(int uid)
{
	struct binder_proc *proc;
	bool idle_f = true;
	enum BINDER_STAT stat;
	if (!oem_binder_hook_set.oem_query_st_hook ||
		!get_binder_lock ||!get_binder_hhead)
		return;
	mutex_lock(get_binder_lock);
	hlist_for_each_entry(proc, get_binder_hhead, proc_node) {
		if (proc != NULL && proc->tsk
			&& (task_uid(proc->tsk).val == uid)) {
			if (query_binder_stat(proc) != BINDER_IN_IDLE)
				idle_f = false;
		}
	}

	if (idle_f)
		stat = BINDER_IN_IDLE;
	else
		stat = BINDER_IN_BUSY;

	oem_binder_hook_set.oem_query_st_hook(uid, current, 0, current->pid, stat);
	mutex_unlock(get_binder_lock);
}
EXPORT_SYMBOL_GPL(query_binder_app_stat);


struct task_struct *binder_buff_owner(struct binder_alloc *alloc)
{
	struct binder_proc *proc = NULL;
	if (!alloc)
		return NULL;

	proc = container_of(alloc, struct binder_proc, alloc);
	return proc->tsk;
}



void mi_binder_alloc_new_buf_locked(void *data, size_t size,
	struct binder_alloc *alloc, int is_async)
{
	if (oem_binder_hook_set.oem_buf_overflow_hook && is_async
		&& ((alloc->free_async_space < oem_binder_hook_set.oem_wahead_thresh
		* (size + sizeof(struct binder_buffer)))
		|| (alloc->free_async_space < oem_binder_hook_set.oem_wahead_space))) {
			struct task_struct *owner;
			owner = binder_buff_owner(alloc);

			if (owner)
				oem_binder_hook_set.oem_buf_overflow_hook(owner, current,
						current->pid, false, 0);
	}
}

void mi_binder_replay(void *data, struct binder_proc *target_proc,
	struct binder_proc *proc, struct binder_thread *thread,
	struct binder_transaction_data *tr)
{
	if (oem_binder_hook_set.oem_reply_hook && target_proc && target_proc->tsk
			&& proc && proc->tsk && thread && tr)
		oem_binder_hook_set.oem_reply_hook(target_proc->tsk, proc->tsk,
				thread->pid, tr->flags & TF_ONE_WAY,
				tr->code);
}

void mi_binder_transaction(void *data, struct binder_proc *target_proc,
	struct binder_proc *proc, struct binder_thread *thread,
	struct binder_transaction_data *tr)
{
	if (oem_binder_hook_set.oem_trans_hook && target_proc && target_proc->tsk
			&& proc && proc->tsk && thread && tr)
		oem_binder_hook_set.oem_trans_hook(target_proc->tsk, proc->tsk,
				thread->pid, tr->flags & TF_ONE_WAY,
				tr->code);
}

void mi_binder_wait_for_work(void *data, bool do_proc_work,
	struct binder_thread *thread, struct binder_proc *proc)
{
	struct task_struct *dst;
	struct binder_transaction *t;
	bool oneway;
	int code;

	if (!thread || !proc || !proc->tsk)
		return;

	if (!oem_binder_hook_set.oem_wait4_hook)
		return;

	/*
	 * The vendor hook is called after binder_inner_proc_unlock() in
	 * binder_thread_read(), so transaction_stack is read without any
	 * lock protection. Without inner_lock, another thread can pop and
	 * kfree(t) between READ_ONCE and spin_lock(&t->lock), causing a
	 * use-after-free on t->lock.
	 *
	 * Acquire proc->inner_lock to safely read transaction_stack and
	 * extract all needed data. While inner_lock is held:
	 *  - binder_pop_transaction_ilocked() cannot run (needs inner_lock)
	 *  - t cannot be freed (freeing happens only after popping)
	 *  - t->to_proc cannot be set to NULL (only in binder_thread_release
	 *    which also needs inner_lock)
	 *
	 * No need for t->lock: the only writer to t->to_proc under
	 * inner_lock is binder_thread_release, which we block by holding
	 * inner_lock. This also avoids the lock-ordering issue between
	 * inner_lock and t->lock.
	 */
	binder_inner_proc_lock(proc);
	t = thread->transaction_stack;
	if (!t || thread->is_dead || !t->to_proc || !t->to_proc->tsk) {
		binder_inner_proc_unlock(proc);
		return;
	}

	dst = t->to_proc->tsk;
	oneway = t->flags & TF_ONE_WAY;
	code = t->code;
	get_task_struct(dst);
	binder_inner_proc_unlock(proc);

	oem_binder_hook_set.oem_wait4_hook(dst,
				proc->tsk,
				thread->pid,
				oneway,
				code);
	put_task_struct(dst);
}

void mi_get_hhead_and_lock(void *data, struct hlist_head *hhead,
	struct mutex *lock)
{
	if(!bgot) {
		 if(hhead)
		 	get_binder_hhead= hhead;
		 if(lock)
		 	get_binder_lock= lock;
		 bgot= true;
	}
}

void oem_register_binder_hook(struct oem_binder_hook *set)
{
	if (!set)
		return;

	oem_binder_hook_set.oem_wahead_thresh = set->oem_wahead_thresh;
	oem_binder_hook_set.oem_wahead_space = set->oem_wahead_space;
	oem_binder_hook_set.oem_reply_hook = set->oem_reply_hook;
	oem_binder_hook_set.oem_trans_hook = set->oem_trans_hook;
	oem_binder_hook_set.oem_wait4_hook = set->oem_wait4_hook;
	oem_binder_hook_set.oem_query_st_hook = set->oem_query_st_hook;
	oem_binder_hook_set.oem_buf_overflow_hook = set->oem_buf_overflow_hook;
}
EXPORT_SYMBOL_GPL(oem_register_binder_hook);


static int __init init_binder_gki(void)
{
	pr_info("enter init_binder_gki func!\n");
	register_trace_android_vh_binder_alloc_new_buf_locked(mi_binder_alloc_new_buf_locked, NULL);
	register_trace_android_vh_binder_reply(mi_binder_replay, NULL);
	register_trace_android_vh_binder_trans(mi_binder_transaction, NULL);
	register_trace_android_vh_binder_wait_for_work(mi_binder_wait_for_work, NULL);
	register_trace_android_vh_binder_preset(mi_get_hhead_and_lock, NULL);

	return 0;
}

module_init(init_binder_gki);

MODULE_LICENSE("GPL");

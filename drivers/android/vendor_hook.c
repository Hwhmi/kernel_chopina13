// SPDX-License-Identifier: GPL-2.0
/*
 * vendor_hook.c - Android vendor hook tracepoints
 *
 * This file defines and exports the tracepoints used by vendor hook
 * modules (e.g. Millet binder_gki, millet_sig).
 *
 * We use DEFINE_TRACE directly instead of CREATE_TRACE_POINTS to avoid
 * define_trace.h conflicts between multiple hook headers with the same
 * TRACE_SYSTEM.
 */

#include <linux/tracepoint.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/signal.h>

/* Define tracepoints */
DEFINE_TRACE(android_vh_binder_alloc_new_buf_locked);
DEFINE_TRACE(android_vh_binder_reply);
DEFINE_TRACE(android_vh_binder_trans);
DEFINE_TRACE(android_vh_binder_wait_for_work);
DEFINE_TRACE(android_vh_binder_preset);
DEFINE_TRACE(android_vh_do_send_sig_info);

/* Export for module use */
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_alloc_new_buf_locked);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_reply);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_trans);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_wait_for_work);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_preset);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_do_send_sig_info);

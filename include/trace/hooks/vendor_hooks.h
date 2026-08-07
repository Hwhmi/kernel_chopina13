/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Vendor hook header for kernel 4.14 (without static_call)
 *
 * This provides a simplified DECLARE_RESTRICTED_HOOK that maps to
 * standard DECLARE_TRACE, which already generates register_trace_*,
 * trace_* and EXPORT_TRACEPOINT_SYMBOL_GPL for module use.
 */

#ifndef __TRACE_HOOKS_VENDOR_HOOKS_H
#define __TRACE_HOOKS_VENDOR_HOOKS_H

#include <linux/tracepoint.h>

#define DECLARE_HOOK DECLARE_TRACE

/*
 * DECLARE_RESTRICTED_HOOK maps directly to DECLARE_TRACE.
 * In standard DECLARE_TRACE, the probe callback signature is:
 *   void (*probe)(void *__data, proto...)
 * which matches what the Millet modules expect.
 */
#define DECLARE_RESTRICTED_HOOK(name, proto, args, cond) \
	DECLARE_TRACE(name, PARAMS(proto), PARAMS(args))

#endif /* __TRACE_HOOKS_VENDOR_HOOKS_H */

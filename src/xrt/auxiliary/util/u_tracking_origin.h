// Copyright 2026, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Helpers for @ref xrt_tracking_origin.
 * @author Jakob Bornecrantz <tbornecrantz@nvidia.com>
 * @ingroup aux_util
 */

#pragma once

#include "xrt/xrt_compiler.h"
#include "xrt/xrt_tracking.h"


#ifdef __cplusplus
extern "C" {
#endif


/*!
 * Initialize a tracking origin, to be called by whatever allocates the
 * tracking origin, IPC clients instead get the ID from the service. Clears
 * the tracking origin, assigns a newly generated @ref xrt_tracking_origin::id,
 * sets the type, resets the initial offset to identity and formats the name,
 * truncating it to fit in @ref XRT_TRACKING_NAME_LEN bytes.
 *
 * @ingroup aux_util
 */
void
u_tracking_origin_init(struct xrt_tracking_origin *xto, enum xrt_tracking_type type, const char *fmt, ...)
    XRT_PRINTF_FORMAT(3, 4);


#ifdef __cplusplus
}
#endif

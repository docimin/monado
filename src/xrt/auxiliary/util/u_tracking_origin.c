// Copyright 2026, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Helpers for @ref xrt_tracking_origin.
 * @author Jakob Bornecrantz <tbornecrantz@nvidia.com>
 * @ingroup aux_util
 */

#include "util/u_tracking_origin.h"
#include "util/u_misc.h"
#include "util/u_tracking_origin_id.h"

#include <stdarg.h>
#include <stdio.h>


/*
 *
 * 'Exported' functions.
 *
 */

void
u_tracking_origin_init(struct xrt_tracking_origin *xto, enum xrt_tracking_type type, const char *fmt, ...)
{
	U_ZERO(xto);

	xto->id = u_tracking_origin_id_generate();
	xto->type = type;
	xto->initial_offset = (struct xrt_pose)XRT_POSE_IDENTITY;

	va_list args;
	va_start(args, fmt);
	vsnprintf(xto->name, sizeof(xto->name), fmt, args);
	va_end(args);
}

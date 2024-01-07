/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

/**
 * @file intel_dpst.h
 *
 * Wrapper functions for dpst Ioctl.
 */

#ifndef INTEL_DPST_H
#define INTEL_DPST_H

/* dpst ioctl wrapper functions */
int drm_intel_dpst_context_init(int fd,
				uint32_t *threshold_gb,
				uint32_t gb_delay,
				uint32_t *pipe_n,
				uint32_t *image_res,
				uint32_t sig_num,
				uint32_t hist_reg_values);
int drm_intel_dpst_context_enable(int fd);
int drm_intel_dpst_context_disable(int fd);
int drm_intel_dpst_context_get_bin(int fd,
				   uint32_t *pipe_n,
				   uint32_t *status,
				   uint32_t *dpst_disable);
int drm_intel_dpst_context_get_bin_legacy(int fd,
				   uint32_t *pipe_n,
				   uint32_t *status);
int drm_intel_dpst_context_apply_luma(int fd,
				      uint32_t dpst_blc_factor,
				      const uint32_t *factor_present,
				      uint32_t factor_scalar);
int drm_intel_dpst_context_histogram_reset(int fd);

#endif /* INTEL_DPST_H */


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
 */

#include <stdio.h>
#include <stdint.h>

#include "i915_drm.h"
#include <xf86drm.h>

#include "intel_dpst.h"

/**
 * DPST wrapper functions
 */
int
drm_intel_dpst_context_init(int fd,
			    uint32_t *threshold_gb,
			    uint32_t gb_delay,
			    uint32_t *pipe_n,
			    uint32_t *image_res,
			    uint32_t sig_num,
			    uint32_t hist_reg_values)
{
	int ret=0;

	struct dpst_initialize_context dpst_context_ioctl;

	dpst_context_ioctl.dpst_ioctl_type = DPST_INIT_DATA;
	dpst_context_ioctl.init_data.gb_delay = gb_delay;

	dpst_context_ioctl.init_data.image_res = *image_res;
	dpst_context_ioctl.init_data.sig_num = sig_num;
	dpst_context_ioctl.init_data.hist_reg_values = hist_reg_values;
	ret = drmIoctl(fd, DRM_IOCTL_I915_DPST_CONTEXT, &dpst_context_ioctl);

	if (ret != 0)
		return ret;

	*threshold_gb = dpst_context_ioctl.init_data.threshold_gb;
	*image_res = dpst_context_ioctl.init_data.image_res;
	*pipe_n = dpst_context_ioctl.init_data.pipe_n;

	return ret;
}

int
drm_intel_dpst_context_enable(int fd)
{
	struct dpst_initialize_context dpst_context_ioctl;
	dpst_context_ioctl.dpst_ioctl_type = DPST_ENABLE;
	return drmIoctl(fd, DRM_IOCTL_I915_DPST_CONTEXT, &dpst_context_ioctl);
}

int
drm_intel_dpst_context_disable(int fd)
{
	struct dpst_initialize_context dpst_context_ioctl;
	dpst_context_ioctl.dpst_ioctl_type = DPST_DISABLE;
	return drmIoctl(fd, DRM_IOCTL_I915_DPST_CONTEXT, &dpst_context_ioctl);
}

int
drm_intel_dpst_context_get_bin(int fd,
			       uint32_t *pipe_n,
			       uint32_t *status,
			       uint32_t *dpst_disable)
{
	int ret;
	unsigned int index = 0;
	struct dpst_initialize_context dpst_context_ioctl;

	dpst_context_ioctl.hist_status.pipe_n = *pipe_n;
	dpst_context_ioctl.hist_status.dpst_disable = *dpst_disable;
	dpst_context_ioctl.dpst_ioctl_type = DPST_GET_BIN_DATA;

	ret = drmIoctl(fd, DRM_IOCTL_I915_DPST_CONTEXT, &dpst_context_ioctl);
	if (ret != 0)
		return ret;

	*pipe_n = dpst_context_ioctl.hist_status.pipe_n;
	*dpst_disable = dpst_context_ioctl.hist_status.dpst_disable;

	for (index = 0; index <32; index++)
		status[index] = dpst_context_ioctl.hist_status.histogram_bins.status[index];

	return ret;
}

int
drm_intel_dpst_context_get_bin_legacy(int fd,
			       uint32_t *pipe_n,
			       uint32_t *status)
{
	int ret;
	unsigned int index = 0;
	struct dpst_initialize_context dpst_context_ioctl;

	dpst_context_ioctl.hist_status_legacy.pipe_n = *pipe_n;
	dpst_context_ioctl.dpst_ioctl_type = DPST_GET_BIN_DATA_LEGACY;

	ret = drmIoctl(fd, DRM_IOCTL_I915_DPST_CONTEXT, &dpst_context_ioctl);
	if (ret != 0)
		return ret;

	*pipe_n = dpst_context_ioctl.hist_status_legacy.pipe_n;

	for (index = 0; index <32; index++)
		status[index] = dpst_context_ioctl.hist_status_legacy.histogram_bins.status[index];

	return ret;
}

int
drm_intel_dpst_context_apply_luma(int fd,
				  uint32_t dpst_blc_factor,
				  const uint32_t *factor_present,
				  uint32_t factor_scalar)
{
	int ret;
	unsigned int index = 0;
	struct dpst_initialize_context dpst_context_ioctl;

	dpst_context_ioctl.ie_container.dpst_blc_factor = dpst_blc_factor;
	for (index = 0; index < DPST_DIET_ENTRY_COUNT; index++)
		dpst_context_ioctl.ie_container.dpst_ie_st.factor_present[index] = factor_present[index];
	dpst_context_ioctl.ie_container.dpst_ie_st.factor_scalar = factor_scalar;

	dpst_context_ioctl.dpst_ioctl_type = DPST_APPLY_LUMA;

	return drmIoctl(fd, DRM_IOCTL_I915_DPST_CONTEXT, &dpst_context_ioctl);
}

int
drm_intel_dpst_context_histogram_reset(int fd)
{
	struct dpst_initialize_context dpst_context_ioctl;
	dpst_context_ioctl.dpst_ioctl_type = DPST_RESET_HISTOGRAM_STATUS;
	return drmIoctl(fd, DRM_IOCTL_I915_DPST_CONTEXT, &dpst_context_ioctl);
}

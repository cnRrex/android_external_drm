/*
 * Copyright © 2007 Intel Corporation
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
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <drm.h>
#include <i915_drm.h>
#include <pciaccess.h>
#include "intel_bufmgr.h"
#include "intel_bufmgr_priv.h"
#include "xf86drm.h"
#include <unistd.h>

/** @file intel_bufmgr.c
 *
 * Convenience functions for buffer management methods.
 */

drm_intel_bo *drm_intel_bo_alloc(drm_intel_bufmgr *bufmgr, const char *name,
				 unsigned long size, unsigned int alignment)
{
	return bufmgr->bo_alloc(bufmgr, name, size, alignment);
}

drm_intel_bo *drm_intel_bo_alloc_for_render(drm_intel_bufmgr *bufmgr,
					    const char *name,
					    unsigned long size,
					    unsigned int alignment)
{
	return bufmgr->bo_alloc_for_render(bufmgr, name, size, alignment);
}

drm_intel_bo *drm_intel_bo_alloc_userptr(drm_intel_bufmgr *bufmgr,
					  const char *name, void *addr,
					  uint32_t tiling_mode,
					  uint32_t stride,
					  unsigned long size,
					  unsigned long flags)
{
	if (bufmgr->bo_alloc_userptr)
		return bufmgr->bo_alloc_userptr(bufmgr, name, addr, tiling_mode,
						stride, size, flags);
	return NULL;
}

drm_intel_bo *
drm_intel_bo_alloc_tiled(drm_intel_bufmgr *bufmgr, const char *name,
                        int x, int y, int cpp, uint32_t *tiling_mode,
                        unsigned long *pitch, unsigned long flags)
{
	return bufmgr->bo_alloc_tiled(bufmgr, name, x, y, cpp,
				      tiling_mode, pitch, flags);
}

void drm_intel_bo_reference(drm_intel_bo *bo)
{
	bo->bufmgr->bo_reference(bo);
}

void drm_intel_bo_unreference(drm_intel_bo *bo)
{
	if (bo == NULL)
		return;

	bo->bufmgr->bo_unreference(bo);
}

int drm_intel_bo_map(drm_intel_bo *buf, int write_enable)
{
	return buf->bufmgr->bo_map(buf, write_enable);
}

int drm_intel_bo_unmap(drm_intel_bo *buf)
{
	return buf->bufmgr->bo_unmap(buf);
}

int
drm_intel_bo_subdata(drm_intel_bo *bo, unsigned long offset,
		     unsigned long size, const void *data)
{
	return bo->bufmgr->bo_subdata(bo, offset, size, data);
}

int
drm_intel_bo_get_subdata(drm_intel_bo *bo, unsigned long offset,
			 unsigned long size, void *data)
{
	int ret;
	if (bo->bufmgr->bo_get_subdata)
		return bo->bufmgr->bo_get_subdata(bo, offset, size, data);

	if (size == 0 || data == NULL)
		return 0;

	ret = drm_intel_bo_map(bo, 0);
	if (ret)
		return ret;
	memcpy(data, (unsigned char *)bo->virtual + offset, size);
	drm_intel_bo_unmap(bo);
	return 0;
}

void drm_intel_bo_wait_rendering(drm_intel_bo *bo)
{
	bo->bufmgr->bo_wait_rendering(bo);
}

void drm_intel_bufmgr_destroy(drm_intel_bufmgr *bufmgr)
{
	bufmgr->destroy(bufmgr);
}

int
drm_intel_bo_exec(drm_intel_bo *bo, int used,
		  drm_clip_rect_t * cliprects, int num_cliprects, int DR4)
{
	return bo->bufmgr->bo_exec(bo, used, cliprects, num_cliprects, DR4, -1, NULL);
}

int
drm_intel_bo_fence_exec(drm_intel_bo *bo, int used,
		  drm_clip_rect_t * cliprects, int num_cliprects, int DR4,
			int fence_in, int* fence_out)
{
	if (fence_out)
		*fence_out = -1;

	return bo->bufmgr->bo_exec(bo, used, cliprects, num_cliprects, DR4,
				fence_in, fence_out);
}

int
drm_intel_bo_mrb_exec(drm_intel_bo *bo, int used,
		drm_clip_rect_t *cliprects, int num_cliprects, int DR4,
		unsigned int rings)
{
	if (bo->bufmgr->bo_mrb_exec)
		return bo->bufmgr->bo_mrb_exec(bo, used,
					cliprects, num_cliprects, DR4,
					rings, -1, NULL);

	switch (rings) {
	case I915_EXEC_DEFAULT:
	case I915_EXEC_RENDER:
		return bo->bufmgr->bo_exec(bo, used,
					   cliprects, num_cliprects, DR4,
						-1, NULL);
	default:
		return -ENODEV;
	}
}

int
drm_intel_bo_mrb_fence_exec(drm_intel_bo *bo, int used,
		drm_clip_rect_t *cliprects, int num_cliprects, int DR4,
		unsigned int rings, int fence_in, int* fence_out)
{
	if (fence_out)
		*fence_out = -1;

	if (bo->bufmgr->bo_mrb_exec)
		return bo->bufmgr->bo_mrb_exec(bo, used,
					cliprects, num_cliprects, DR4,
					rings, fence_in, fence_out);

	switch (rings) {
	case I915_EXEC_DEFAULT:
	case I915_EXEC_RENDER:
		return bo->bufmgr->bo_exec(bo, used,
					   cliprects, num_cliprects, DR4,
						fence_in, fence_out);
	default:
		return -ENODEV;
	}
}

void drm_intel_bufmgr_set_debug(drm_intel_bufmgr *bufmgr, int enable_debug)
{
	bufmgr->debug = enable_debug;
}

int drm_intel_bufmgr_check_aperture_space(drm_intel_bo ** bo_array, int count)
{
	return bo_array[0]->bufmgr->check_aperture_space(bo_array, count);
}

int drm_intel_bo_flink(drm_intel_bo *bo, uint32_t * name)
{
	if (bo->bufmgr->bo_flink)
		return bo->bufmgr->bo_flink(bo, name);

	return -ENODEV;
}
int drm_intel_bo_prime(drm_intel_bo *bo, uint32_t * name)
{
	if (bo->bufmgr->bo_prime)
		return bo->bufmgr->bo_prime(bo, name);

	return -ENODEV;
}
int
drm_intel_bo_emit_reloc(drm_intel_bo *bo, uint32_t offset,
			drm_intel_bo *target_bo, uint32_t target_offset,
			uint32_t read_domains, uint32_t write_domain)
{
	return bo->bufmgr->bo_emit_reloc(bo, offset,
					 target_bo, target_offset,
					 read_domains, write_domain);
}

/* For fence registers, not GL fences */
int
drm_intel_bo_emit_reloc_fence(drm_intel_bo *bo, uint32_t offset,
			      drm_intel_bo *target_bo, uint32_t target_offset,
			      uint32_t read_domains, uint32_t write_domain)
{
	return bo->bufmgr->bo_emit_reloc_fence(bo, offset,
					       target_bo, target_offset,
					       read_domains, write_domain);
}


int drm_intel_bo_pin(drm_intel_bo *bo, uint32_t alignment)
{
	if (bo->bufmgr->bo_pin)
		return bo->bufmgr->bo_pin(bo, alignment);

	return -ENODEV;
}

int drm_intel_bo_unpin(drm_intel_bo *bo)
{
	if (bo->bufmgr->bo_unpin)
		return bo->bufmgr->bo_unpin(bo);

	return -ENODEV;
}

int drm_intel_bo_set_tiling(drm_intel_bo *bo, uint32_t * tiling_mode,
			    uint32_t stride)
{
	if (bo->bufmgr->bo_set_tiling)
		return bo->bufmgr->bo_set_tiling(bo, tiling_mode, stride);

	*tiling_mode = I915_TILING_NONE;
	return 0;
}

int drm_intel_bo_get_tiling(drm_intel_bo *bo, uint32_t * tiling_mode,
			    uint32_t * swizzle_mode)
{
	if (bo->bufmgr->bo_get_tiling)
		return bo->bufmgr->bo_get_tiling(bo, tiling_mode, swizzle_mode);

	*tiling_mode = I915_TILING_NONE;
	*swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
	return 0;
}

int drm_intel_bo_set_userdata(drm_intel_bo *bo, uint32_t userdata)
{
	if (bo->bufmgr->bo_set_userdata)
		return bo->bufmgr->bo_set_userdata(bo, userdata);

	return 0;
}

int drm_intel_bo_get_userdata(drm_intel_bo *bo, uint32_t *userdata)
{
	if (bo->bufmgr->bo_get_userdata)
		return bo->bufmgr->bo_get_userdata(bo, userdata);

	*userdata = 0;
	return 0;
}

int drm_intel_bo_set_datatype(drm_intel_bo *bo, uint32_t userdata)
{
	if (bo->bufmgr->bo_set_userdata)
		return bo->bufmgr->bo_set_userdata(bo, userdata);

	return 0;
}

int drm_intel_bo_get_datatype(drm_intel_bo *bo, uint32_t *userdata)
{
	if (bo->bufmgr->bo_get_userdata)
		return bo->bufmgr->bo_get_userdata(bo, userdata);

	*userdata = 0;
	return 0;
}

int drm_intel_bo_create_userdata_blk(drm_intel_bo *bo,
				     uint16_t      flags,
				     uint32_t      bytes,
				     const void   *data,
				     uint32_t     *avail_bytes)
{
	if (bo->bufmgr->bo_create_userdata_blk)
		return bo->bufmgr->bo_create_userdata_blk(bo, flags, 
							  bytes, data,
							  avail_bytes);

	return -ENODEV;
}


int drm_intel_bo_set_userdata_blk(drm_intel_bo *bo,
				  uint32_t      offset,
				  uint32_t      bytes,
				  const void   *data,
				  uint32_t     *avail_bytes)
{
	if (bo->bufmgr->bo_set_userdata_blk)
		return bo->bufmgr->bo_set_userdata_blk(bo, offset,
						       bytes, data,
						       avail_bytes);

	return -ENODEV;
}

int drm_intel_bo_get_userdata_blk(drm_intel_bo *bo,
				  uint32_t      offset,
				  uint32_t      bytes,
				  void         *data,
				  uint32_t     *avail_bytes)
{
	if (bo->bufmgr->bo_get_userdata_blk)
		return bo->bufmgr->bo_get_userdata_blk(bo, offset,
						       bytes, data,
						       avail_bytes);

	return -ENODEV;
}

int drm_intel_bo_fallocate(drm_intel_bo *bo,
				      uint32_t mode,
				      uint64_t offset,
				      uint64_t bytes)
{
	if (bo->bufmgr->bo_fallocate)
		return bo->bufmgr->bo_fallocate(bo, mode,
						       offset, bytes);

	return -ENODEV;
}


int drm_intel_bo_disable_reuse(drm_intel_bo *bo)
{
	if (bo->bufmgr->bo_disable_reuse)
		return bo->bufmgr->bo_disable_reuse(bo);
	return 0;
}

int drm_intel_bo_is_reusable(drm_intel_bo *bo)
{
	if (bo->bufmgr->bo_is_reusable)
		return bo->bufmgr->bo_is_reusable(bo);
	return 0;
}

int drm_intel_bo_busy(drm_intel_bo *bo)
{
	if (bo->bufmgr->bo_busy)
		return bo->bufmgr->bo_busy(bo);
	return 0;
}

int drm_intel_bo_madvise(drm_intel_bo *bo, int madv)
{
	if (bo->bufmgr->bo_madvise)
		return bo->bufmgr->bo_madvise(bo, madv);
	return -1;
}

int drm_intel_bo_references(drm_intel_bo *bo, drm_intel_bo *target_bo)
{
	return bo->bufmgr->bo_references(bo, target_bo);
}

int drm_intel_bo_pad_to_size(drm_intel_bo *bo, uint64_t pad_to_size)
{
       return bo->bufmgr->bo_pad_to_size(bo, pad_to_size);
}

int drm_intel_get_pipe_from_crtc_id(drm_intel_bufmgr *bufmgr, int crtc_id)
{
	if (bufmgr->get_pipe_from_crtc_id)
		return bufmgr->get_pipe_from_crtc_id(bufmgr, crtc_id);
	return -1;
}

static size_t
drm_intel_probe_agp_aperture_size(int fd)
{
	struct pci_device *pci_dev;
	size_t size = 0;
	int ret;

	ret = pci_system_init();
	if (ret)
		goto err;

	/* XXX handle multiple adaptors? */
	pci_dev = pci_device_find_by_slot(0, 0, 2, 0);
	if (pci_dev == NULL)
		goto err;

	ret = pci_device_probe(pci_dev);
	if (ret)
		goto err;

	size = pci_dev->regions[2].size;
err:
	pci_system_cleanup ();
	return size;
}

int drm_intel_get_aperture_sizes(int fd,
				 size_t *mappable,
				 size_t *total)
{

	struct drm_i915_gem_get_aperture aperture;
	int ret;

	ret = drmIoctl(fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture);
	if (ret)
		return ret;

	*mappable = 0;
	/* XXX add a query for the kernel value? */
	if (*mappable == 0)
		*mappable = drm_intel_probe_agp_aperture_size(fd);
	if (*mappable == 0)
		*mappable = 64 * 1024 * 1024; /* minimum possible value */
	*total = aperture.aper_size;
	return 0;
}

int drm_intel_cmd_parser_append(int fd,
				uint32_t ring,
				uint32_t cmd_count,
				cmd_descriptor *cmds,
				uint32_t *regs,
				uint32_t reg_count)
{
	int ret;
	struct drm_i915_cmd_parser_append append;

	append.ring = ring;
	append.cmd_count = cmd_count;
	append.cmds = (uintptr_t)cmds;
	append.regs = (uintptr_t)regs;
	append.reg_count = reg_count;

	ret = drmIoctl(fd, DRM_IOCTL_I915_CMD_PARSER_APPEND, &append);

	return ret;
}

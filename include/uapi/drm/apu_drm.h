/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef __UAPI_APU_DRM_H__
#define __UAPI_APU_DRM_H__

#include "drm.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define APU_JOB_COMPLETED 0x80000000

/*
 * Please note that modifications to all structs defined here are
 * subject to backwards-compatibility constraints.
 */

/*
 * Firmware request, must be aligned with the one defined in firmware.
 * @id: Request id, used in the case of reply, to find the pending request
 * @cmd: The command id to execute in the firmware
 * @result: The result of the command executed on the firmware
 * @size: The size of the data available in this request
 * @count: The number of shared buffer
 * @data: Contains the data attached with the request if size is greater than
 *        zero, and the addresses of shared buffers if count is greater than
 *        zero. Both the data and the shared buffer could be read and write
 *        by the APU.
 */
struct  apu_dev_request {
	u16 id;
	u16 cmd;
	u16 result;
	u16 size_in;
	u16 size_out;
	u16 count;
	u8 data[0];
} __packed;

struct drm_apu_gem_new {
	__u32 size;			/* in */
	__u32 flags;			/* in */
	__u32 handle;			/* out */
	__u64 offset;			/* out */
};

struct drm_apu_gem_queue {
	__u32 device;
	__u32 cmd;
	__u32 out_sync;
	__u64 bo_handles;
	__u32 bo_handle_count;
	__u16 size_in;
	__u16 size_out;
	__u64 data;
};

struct drm_apu_gem_dequeue {
	__u32 out_sync;
	__u16 result;
	__u16 size;
	__u64 data;
};

struct drm_apu_gem_iommu_map {
	__u64 bo_handles;
	__u32 bo_handle_count;
	__u64 bo_device_addresses;
};

struct apu_job_event {
	struct drm_event base;
	__u32 out_sync;
};

#define APU_ONLINE		BIT(0)
#define APU_CRASHED		BIT(1)
#define APU_TIMEDOUT		BIT(2)

struct drm_apu_state {
	__u32 device;
	__u32 flags;
};

#define DRM_APU_GEM_NEW			0x00
#define DRM_APU_GEM_QUEUE		0x01
#define DRM_APU_GEM_DEQUEUE		0x02
#define DRM_APU_GEM_IOMMU_MAP		0x03
#define DRM_APU_GEM_IOMMU_UNMAP		0x04
#define DRM_APU_STATE			0x05
#define DRM_APU_NUM_IOCTLS		0x06

#define DRM_IOCTL_APU_GEM_NEW		DRM_IOWR(DRM_COMMAND_BASE + DRM_APU_GEM_NEW, struct drm_apu_gem_new)
#define DRM_IOCTL_APU_GEM_USER_NEW	DRM_IOWR(DRM_COMMAND_BASE + DRM_APU_GEM_USER_NEW, struct drm_apu_gem_user_new)
#define DRM_IOCTL_APU_GEM_QUEUE		DRM_IOWR(DRM_COMMAND_BASE + DRM_APU_GEM_QUEUE, struct drm_apu_gem_queue)
#define DRM_IOCTL_APU_GEM_DEQUEUE	DRM_IOWR(DRM_COMMAND_BASE + DRM_APU_GEM_DEQUEUE, struct drm_apu_gem_dequeue)
#define DRM_IOCTL_APU_GEM_IOMMU_MAP	DRM_IOWR(DRM_COMMAND_BASE + DRM_APU_GEM_IOMMU_MAP, struct drm_apu_gem_iommu_map)
#define DRM_IOCTL_APU_GEM_IOMMU_UNMAP	DRM_IOWR(DRM_COMMAND_BASE + DRM_APU_GEM_IOMMU_UNMAP, struct drm_apu_gem_iommu_map)
#define DRM_IOCTL_APU_STATE		DRM_IOWR(DRM_COMMAND_BASE + DRM_APU_STATE, struct drm_apu_state)

#if defined(__cplusplus)
}
#endif

#endif /* __UAPI_APU_DRM_H__ */

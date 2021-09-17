/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __APU_INTERNAL_H__
#define __APU_INTERNAL_H__

#include <linux/iova.h>

#include <drm/drm_drv.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/gpu_scheduler.h>

struct apu_gem_object {
	struct drm_gem_cma_object base;
	struct mutex mutex;
	struct sg_table *iommu_sgt;
	int iommu_refcount;
	size_t size;
	u32 iova;
	u32 offset;
};

struct apu_sched;
struct apu_core {
	int device_id;
	struct device *dev;
	struct rproc *rproc;
	struct apu_drm_ops *ops;
	struct apu_drm *apu_drm;

	spinlock_t ctx_lock;
	struct list_head requests;

	struct list_head node;
	void *priv;

	struct apu_sched *sched;
	u32 flags;
};

struct apu_drm {
	struct device *dev;
	struct drm_device *drm;

	struct iommu_domain *domain;
	struct iova_domain iovad;
	int iova_limit_pfn;

	struct list_head apu_cores;
	struct list_head node;
};

static inline struct apu_gem_object *to_apu_bo(struct drm_gem_object *obj)
{
	return container_of(to_drm_gem_cma_obj(obj), struct apu_gem_object,
			    base);
}

struct apu_gem_object *to_apu_bo(struct drm_gem_object *obj);
struct drm_gem_object *apu_gem_create_object(struct drm_device *dev,
					     size_t size);

int apu_bo_iommu_map(struct apu_drm *apu_drm, struct drm_gem_object *obj);
void apu_bo_iommu_unmap(struct apu_drm *apu_drm, struct apu_gem_object *obj);
struct drm_gem_object *apu_gem_create_object(struct drm_device *dev,
					     size_t size);
int ioctl_gem_new(struct drm_device *dev, void *data,
		  struct drm_file *file_priv);
int ioctl_gem_user_new(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int ioctl_gem_iommu_map(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int ioctl_gem_iommu_unmap(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
int ioctl_gem_queue(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);
int ioctl_gem_dequeue(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
int ioctl_apu_state(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);
struct dma_buf *apu_gem_prime_export(struct drm_gem_object *gem,
				     int flags);

struct apu_job;

int apu_drm_job_init(struct apu_core *core);
void apu_sched_fini(struct apu_core *core);
int apu_job_push(struct apu_job *job);
void apu_job_put(struct apu_job *job);

#endif /* __APU_INTERNAL_H__ */

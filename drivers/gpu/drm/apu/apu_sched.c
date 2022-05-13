// SPDX-License-Identifier: GPL-2.0
//
// Copyright 2020 BayLibre SAS

#include <drm/apu_drm.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_syncobj.h>
#include <drm/gpu_scheduler.h>

#include <uapi/drm/apu_drm.h>

#include "apu_internal.h"

struct apu_queue_state {
	struct drm_gpu_scheduler sched;

	u64 fence_context;
	u64 seqno;
};

struct apu_request {
	struct list_head node;
	void *job;
};

struct apu_sched {
	struct apu_queue_state apu_queue;
	spinlock_t job_lock;
	struct drm_sched_entity sched_entity;
};

struct apu_event {
	struct drm_pending_event pending_event;
	union {
		struct drm_event base;
		struct apu_job_event job_event;
	};
};

struct apu_job {
	struct drm_sched_job base;

	struct kref refcount;

	struct apu_core *apu_core;
	struct apu_drm *apu_drm;

	/* Fence to be signaled by IRQ handler when the job is complete. */
	struct dma_fence *done_fence;

	__u32 cmd;

	/* Exclusive fences we have taken from the BOs to wait for */
	struct dma_fence **implicit_fences;
	struct drm_gem_object **bos;
	u32 bo_count;

	/* Fence to be signaled by drm-sched once its done with the job */
	struct dma_fence *render_done_fence;

	void *data_in;
	uint16_t size_in;
	void *data_out;
	uint16_t size_out;
	uint16_t result;
	uint16_t id;

	struct list_head node;
	struct drm_syncobj *sync_out;

	struct apu_event *event;
};

static DEFINE_IDA(req_ida);
static LIST_HEAD(complete_node);

int apu_drm_callback(struct apu_core *apu_core, void *data, int len)
{
	struct apu_request *apu_req, *tmp;
	struct apu_dev_request *hdr = data;
	unsigned long flags;

	spin_lock_irqsave(&apu_core->ctx_lock, flags);
	list_for_each_entry_safe(apu_req, tmp, &apu_core->requests, node) {
		struct apu_job *job = apu_req->job;

		if (job && hdr->id == job->id) {
			kref_get(&job->refcount);
			job->result = hdr->result;
			if (job->size_out)
				memcpy(job->data_out, hdr->data + job->size_in,
				       min(job->size_out, hdr->size_out));
			job->size_out = hdr->size_out;
			list_add(&job->node, &complete_node);
			list_del(&apu_req->node);
			ida_simple_remove(&req_ida, hdr->id);
			kfree(apu_req);
			drm_send_event(job->apu_drm->drm,
				       &job->event->pending_event);
			dma_fence_signal_locked(job->done_fence);
		}
	}
	spin_unlock_irqrestore(&apu_core->ctx_lock, flags);

	return 0;
}

void apu_sched_fini(struct apu_core *core)
{
	drm_sched_fini(&core->sched->apu_queue.sched);
	devm_kfree(core->dev, core->sched);
	core->flags &= ~APU_ONLINE;
	core->sched = NULL;
}

static void apu_job_cleanup(struct kref *ref)
{
	struct apu_job *job = container_of(ref, struct apu_job,
					   refcount);
	unsigned int i;

	if (job->implicit_fences) {
		for (i = 0; i < job->bo_count; i++)
			dma_fence_put(job->implicit_fences[i]);
		kvfree(job->implicit_fences);
	}
	dma_fence_put(job->done_fence);
	dma_fence_put(job->render_done_fence);

	if (job->bos) {
		for (i = 0; i < job->bo_count; i++) {
			struct apu_gem_object *apu_obj;

			apu_obj = to_apu_bo(job->bos[i]);
			apu_bo_iommu_unmap(job->apu_drm, apu_obj);
			drm_gem_object_put(job->bos[i]);
		}

		kvfree(job->bos);
	}

	kfree(job->data_out);
	kfree(job->data_in);
	kfree(job);
}

void apu_job_put(struct apu_job *job)
{
	kref_put(&job->refcount, apu_job_cleanup);
}

static void apu_acquire_object_fences(struct drm_gem_object **bos,
				      int bo_count,
				      struct dma_fence **implicit_fences)
{
	int i;

	for (i = 0; i < bo_count; i++)
		implicit_fences[i] = dma_fence_get(dma_resv_excl_fence(bos[i]->resv));
}

static void apu_attach_object_fences(struct drm_gem_object **bos,
				     int bo_count, struct dma_fence *fence)
{
	int i;

	for (i = 0; i < bo_count; i++)
		dma_resv_add_excl_fence(bos[i]->resv, fence);
}

int apu_job_push(struct apu_job *job)
{
	struct drm_sched_entity *entity = &job->apu_core->sched->sched_entity;
	struct ww_acquire_ctx acquire_ctx;
	int ret = 0;

	ret = drm_gem_lock_reservations(job->bos, job->bo_count, &acquire_ctx);
	if (ret)
		return ret;

	ret = drm_sched_job_init(&job->base, entity, NULL);
	if (ret)
		goto unlock;

	job->render_done_fence = dma_fence_get(&job->base.s_fence->finished);

	kref_get(&job->refcount);	/* put by scheduler job completion */

	apu_acquire_object_fences(job->bos, job->bo_count,
				  job->implicit_fences);

	drm_sched_entity_push_job(&job->base, entity);

	apu_attach_object_fences(job->bos, job->bo_count,
				 job->render_done_fence);

unlock:
	drm_gem_unlock_reservations(job->bos, job->bo_count, &acquire_ctx);

	return ret;
}

static const char *apu_fence_get_driver_name(struct dma_fence *fence)
{
	return "apu";
}

static const char *apu_fence_get_timeline_name(struct dma_fence *fence)
{
	return "apu-0";
}

static void apu_fence_release(struct dma_fence *f)
{
	kfree(f);
}

static const struct dma_fence_ops apu_fence_ops = {
	.get_driver_name = apu_fence_get_driver_name,
	.get_timeline_name = apu_fence_get_timeline_name,
	.release = apu_fence_release,
};

static struct dma_fence *apu_fence_create(struct apu_sched *sched)
{
	struct dma_fence *fence;
	struct apu_queue_state *apu_queue = &sched->apu_queue;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return ERR_PTR(-ENOMEM);

	dma_fence_init(fence, &apu_fence_ops, &sched->job_lock,
		       apu_queue->fence_context, apu_queue->seqno++);

	return fence;
}

static struct apu_job *to_apu_job(struct drm_sched_job *sched_job)
{
	return container_of(sched_job, struct apu_job, base);
}

static struct dma_fence *apu_job_dependency(struct drm_sched_job *sched_job,
					    struct drm_sched_entity *s_entity)
{
	struct apu_job *job = to_apu_job(sched_job);
	struct dma_fence *fence;
	unsigned int i;

	/* Implicit fences, max. one per BO */
	for (i = 0; i < job->bo_count; i++) {
		if (job->implicit_fences[i]) {
			fence = job->implicit_fences[i];
			job->implicit_fences[i] = NULL;
			return fence;
		}
	}

	return NULL;
}

static int apu_job_hw_submit(struct apu_job *job)
{
	int ret;
	struct apu_core *apu_core = job->apu_core;
	struct apu_dev_request *dev_req;
	struct apu_request *apu_req;
	unsigned long flags;

	int size = sizeof(*dev_req) + sizeof(u32) * job->bo_count * 2;
	u32 *dev_req_da;
	u32 *dev_req_buffer_size;
	int i;

	dev_req = kmalloc(size + job->size_in + job->size_out, GFP_KERNEL);
	if (!dev_req)
		return -ENOMEM;

	dev_req->cmd = job->cmd;
	dev_req->size_in = job->size_in;
	dev_req->size_out = job->size_out;
	dev_req->count = job->bo_count;
	dev_req_da =
	    (u32 *) (dev_req->data + dev_req->size_in + dev_req->size_out);
	dev_req_buffer_size = (u32 *) (dev_req_da + dev_req->count);
	memcpy(dev_req->data, job->data_in, job->size_in);

	apu_req = kzalloc(sizeof(*apu_req), GFP_KERNEL);
	if (!apu_req)
		return -ENOMEM;

	for (i = 0; i < job->bo_count; i++) {
		struct apu_gem_object *obj = to_apu_bo(job->bos[i]);

		dev_req_da[i] = obj->iova + obj->offset;
		dev_req_buffer_size[i] = obj->size;
	}

	ret = ida_simple_get(&req_ida, 0, 0xffff, GFP_KERNEL);
	if (ret < 0)
		goto err_free_memory;

	dev_req->id = ret;

	job->id = dev_req->id;
	apu_req->job = job;
	spin_lock_irqsave(&apu_core->ctx_lock, flags);
	list_add(&apu_req->node, &apu_core->requests);
	spin_unlock_irqrestore(&apu_core->ctx_lock, flags);
	ret =
	    apu_core->ops->send(apu_core, dev_req,
				size + dev_req->size_in + dev_req->size_out);
	if (ret < 0)
		goto err;
	kfree(dev_req);

	return 0;

err:
	list_del(&apu_req->node);
	ida_simple_remove(&req_ida, dev_req->id);
err_free_memory:
	kfree(apu_req);
	kfree(dev_req);

	return ret;
}

static struct dma_fence *apu_job_run(struct drm_sched_job *sched_job)
{
	struct apu_job *job = to_apu_job(sched_job);
	struct dma_fence *fence = NULL;

	if (unlikely(job->base.s_fence->finished.error))
		return NULL;

	fence = apu_fence_create(job->apu_core->sched);
	if (IS_ERR(fence))
		return NULL;

	job->done_fence = dma_fence_get(fence);

	apu_job_hw_submit(job);

	return fence;
}

static void apu_update_rpoc_state(struct apu_core *core)
{
	if (core->rproc) {
		if (core->rproc->state == RPROC_CRASHED)
			core->flags |= APU_CRASHED;
		if (core->rproc->state == RPROC_OFFLINE)
			core->flags &= ~APU_ONLINE;
	}
}

static enum drm_gpu_sched_stat apu_job_timedout(struct drm_sched_job *sched_job)
{
	struct apu_request *apu_req, *tmp;
	struct apu_job *job = to_apu_job(sched_job);

	if (dma_fence_is_signaled(job->done_fence))
		return DRM_GPU_SCHED_STAT_NOMINAL;

	list_for_each_entry_safe(apu_req, tmp, &job->apu_core->requests, node) {
		/* Remove the request and notify user about timeout */
		if (apu_req->job == job) {
			kref_get(&job->refcount);
			job->apu_core->flags |= APU_TIMEDOUT;
			apu_update_rpoc_state(job->apu_core);
			job->result = ETIMEDOUT;
			list_add(&job->node, &complete_node);
			list_del(&apu_req->node);
			ida_simple_remove(&req_ida, job->id);
			kfree(apu_req);
			drm_send_event(job->apu_drm->drm,
				       &job->event->pending_event);
			dma_fence_signal_locked(job->done_fence);
		}
	}

	return DRM_GPU_SCHED_STAT_NOMINAL;
}

static void apu_job_free(struct drm_sched_job *sched_job)
{
	struct apu_job *job = to_apu_job(sched_job);

	drm_sched_job_cleanup(sched_job);

	apu_job_put(job);
}

static const struct drm_sched_backend_ops apu_sched_ops = {
	.dependency = apu_job_dependency,
	.run_job = apu_job_run,
	.timedout_job = apu_job_timedout,
	.free_job = apu_job_free
};

int apu_drm_job_init(struct apu_core *core)
{
	int ret;
	struct apu_sched *apu_sched;
	struct drm_gpu_scheduler *sched;

	apu_sched = devm_kzalloc(core->dev, sizeof(*apu_sched), GFP_KERNEL);
	if (!apu_sched)
		return -ENOMEM;

	sched = &apu_sched->apu_queue.sched;
	apu_sched->apu_queue.fence_context = dma_fence_context_alloc(1);
	ret = drm_sched_init(sched, &apu_sched_ops,
			     1, 0, msecs_to_jiffies(500),
			     NULL, NULL, "apu_js");
	if (ret) {
		dev_err(core->dev, "Failed to create scheduler: %d.", ret);
		return ret;
	}

	ret = drm_sched_entity_init(&apu_sched->sched_entity,
				    DRM_SCHED_PRIORITY_NORMAL,
				    &sched, 1, NULL);

	core->sched = apu_sched;
	core->flags = APU_ONLINE;

	return ret;
}

static struct apu_core *get_apu_core(struct apu_drm *apu_drm, int device_id)
{
	struct apu_core *apu_core;

	list_for_each_entry(apu_core, &apu_drm->apu_cores, node) {
		if (apu_core->device_id == device_id)
			return apu_core;
	}

	return NULL;
}

static int apu_core_is_running(struct apu_core *core)
{
	return core->ops && core->priv && core->sched;
}

static int
apu_lookup_bos(struct drm_device *dev,
	       struct drm_file *file_priv,
	       struct drm_apu_gem_queue *args, struct apu_job *job)
{
	void __user *bo_handles;
	unsigned int i;
	int ret;

	job->bo_count = args->bo_handle_count;

	if (!job->bo_count)
		return 0;

	job->implicit_fences = kvmalloc_array(job->bo_count,
					      sizeof(struct dma_fence *),
					      GFP_KERNEL | __GFP_ZERO);
	if (!job->implicit_fences)
		return -ENOMEM;

	bo_handles = (void __user *)(uintptr_t) args->bo_handles;
	ret = drm_gem_objects_lookup(file_priv, bo_handles,
				     job->bo_count, &job->bos);
	if (ret)
		return ret;

	for (i = 0; i < job->bo_count; i++) {
		ret = apu_bo_iommu_map(job->apu_drm, job->bos[i]);
		if (ret) {
			/* TODO: handle error */
			break;
		}
	}

	return ret;
}

int ioctl_gem_queue(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct apu_drm *apu_drm = dev->dev_private;
	struct drm_apu_gem_queue *args = data;
	struct apu_event *event;
	struct apu_core *core;
	struct drm_syncobj *sync_out = NULL;
	struct apu_job *job;
	int ret = 0;

	core = get_apu_core(apu_drm, args->device);
	if (!apu_core_is_running(core))
		return -ENODEV;

	if (args->out_sync > 0) {
		sync_out = drm_syncobj_find(file_priv, args->out_sync);
		if (!sync_out)
			return -ENODEV;
	}

	job = kzalloc(sizeof(*job), GFP_KERNEL);
	if (!job) {
		ret = -ENOMEM;
		goto fail_out_sync;
	}

	kref_init(&job->refcount);

	job->apu_drm = apu_drm;
	job->apu_core = core;
	job->cmd = args->cmd;
	job->size_in = args->size_in;
	job->size_out = args->size_out;
	job->sync_out = sync_out;
	if (job->size_in) {
		job->data_in = kmalloc(job->size_in, GFP_KERNEL);
		if (!job->data_in) {
			ret = -ENOMEM;
			goto fail_job;
		}

		ret =
		    copy_from_user(job->data_in,
				   (void __user *)(uintptr_t) args->data,
				   job->size_in);
		if (ret)
			goto fail_job;
	}

	if (job->size_out) {
		job->data_out = kmalloc(job->size_out, GFP_KERNEL);
		if (!job->data_out) {
			ret = -ENOMEM;
			goto fail_job;
		}
	}

	ret = apu_lookup_bos(dev, file_priv, args, job);
	if (ret)
		goto fail_job;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	event->base.length = sizeof(struct apu_job_event);
	event->base.type = APU_JOB_COMPLETED;
	event->job_event.out_sync = args->out_sync;
	job->event = event;
	ret = drm_event_reserve_init(dev, file_priv, &job->event->pending_event,
				     &job->event->base);
	if (ret)
		goto fail_job;

	ret = apu_job_push(job);
	if (ret) {
		drm_event_cancel_free(dev, &job->event->pending_event);
		goto fail_job;
	}

	/* Update the return sync object for the job */
	if (sync_out)
		drm_syncobj_replace_fence(sync_out, job->render_done_fence);

fail_job:
	apu_job_put(job);
fail_out_sync:
	if (sync_out)
		drm_syncobj_put(sync_out);

	return ret;
}

int ioctl_gem_dequeue(struct drm_device *dev, void *data,
		      struct drm_file *file_priv)
{
	struct drm_apu_gem_dequeue *args = data;
	struct drm_syncobj *sync_out = NULL;
	struct apu_job *job;
	int ret = 0;

	if (args->out_sync > 0) {
		sync_out = drm_syncobj_find(file_priv, args->out_sync);
		if (!sync_out)
			return -ENODEV;
	}

	list_for_each_entry(job, &complete_node, node) {
		if (job->sync_out == sync_out) {
			if (job->data_out) {
				ret = copy_to_user((void __user *)(uintptr_t)
						   args->data, job->data_out,
						   job->size_out);
				args->size = job->size_out;
			}
			args->result = job->result;
			list_del(&job->node);
			apu_job_put(job);
			drm_syncobj_put(sync_out);

			return ret;
		}
	}

	if (sync_out)
		drm_syncobj_put(sync_out);

	return 0;
}

int ioctl_apu_state(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct apu_drm *apu_drm = dev->dev_private;
	struct drm_apu_state *args = data;
	struct apu_core *core;

	args->flags = 0;

	core = get_apu_core(apu_drm, args->device);
	if (!core)
		return -ENODEV;
	args->flags |= core->flags;

	/* Reset APU flags */
	core->flags &= ~(APU_TIMEDOUT | APU_CRASHED);

	return 0;
}

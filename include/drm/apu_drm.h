/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __APU_DRM_H__
#define __APU_DRM_H__

#include <linux/iova.h>
#include <linux/remoteproc.h>

struct apu_core;
struct apu_drm;

struct apu_drm_ops {
	int (*send)(struct apu_core *apu_core, void *data, int len);
	int (*callback)(struct apu_core *apu_core, void *data, int len);
};

#ifdef CONFIG_DRM_APU

struct apu_core *apu_drm_register_core(struct rproc *rproc,
				       struct apu_drm_ops *ops, void *priv);
int apu_drm_reserve_iova(struct apu_core *apu_core, u64 start, u64 size);
int apu_drm_unregister_core(void *priv);
int apu_drm_callback(struct apu_core *apu_core, void *data, int len);
void *apu_drm_priv(struct apu_core *apu_core);

#else /* CONFIG_DRM_APU */

static inline
struct apu_core *apu_drm_register_core(struct rproc *rproc,
				       struct apu_drm_ops *ops, void *priv)
{
	return NULL;
}

static inline
int apu_drm_reserve_iova(struct apu_core *apu_core, u64 start, u64 size)
{
	return -ENOMEM;
}

static inline
int apu_drm_uregister_core(void *priv)
{
	return -ENODEV;
}

static inline
int apu_drm_callback(struct apu_core *apu_core, void *data, int len)
{
	return -ENODEV;
}

static inline void *apu_drm_priv(struct apu_core *apu_core)
{
	return NULL;
}
#endif /* CONFIG_DRM_APU */


#endif /* __APU_DRM_H__ */

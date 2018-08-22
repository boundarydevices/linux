
#ifndef _DRM_NOTIFIER_H_
#define _DRM_NOTIFIER_H_

struct notifier_block;
extern int drm_register_client(struct notifier_block *nb);
extern int drm_unregister_client(struct notifier_block *nb);
extern int drm_notifier_call_chain(unsigned long val, void *v);

#endif

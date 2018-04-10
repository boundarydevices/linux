/*
 * Copyright (C) 2016 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Endless Mobile
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/component.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_rect.h>
#include <drm/drm_fb_helper.h>

#include "am_meson_fbdev.h"
#ifdef CONFIG_DRM_MESON_USE_ION
#include "am_meson_gem.h"
#include "am_meson_fb.h"
#endif
#include "am_meson_drv.h"


#define DRIVER_NAME "meson"
#define DRIVER_DESC "Amlogic Meson DRM driver"


static void am_meson_fb_output_poll_changed(struct drm_device *dev)
{
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	struct meson_drm *priv = dev->dev_private;

	drm_fbdev_cma_hotplug_event(priv->fbdev);
#endif
}

static const struct drm_mode_config_funcs meson_mode_config_funcs = {
	.output_poll_changed = am_meson_fb_output_poll_changed,
	.atomic_check        = drm_atomic_helper_check,
	.atomic_commit       = drm_atomic_helper_commit,
#ifdef CONFIG_DRM_MESON_USE_ION
	.fb_create           = am_meson_fb_create,
#else
	.fb_create           = drm_fb_cma_create,
#endif
};

int am_meson_register_crtc_funcs(struct drm_crtc *crtc,
				 const struct meson_crtc_funcs *crtc_funcs)
{
	int pipe = drm_crtc_index(crtc);
	struct meson_drm *priv = crtc->dev->dev_private;

	if (pipe >= MESON_MAX_CRTC)
		return -EINVAL;

	priv->crtc_funcs[pipe] = crtc_funcs;

	return 0;
}
EXPORT_SYMBOL(am_meson_register_crtc_funcs);

void am_meson_unregister_crtc_funcs(struct drm_crtc *crtc)
{
	int pipe = drm_crtc_index(crtc);
	struct meson_drm *priv = crtc->dev->dev_private;

	if (pipe >= MESON_MAX_CRTC)
		return;

	priv->crtc_funcs[pipe] = NULL;
}
EXPORT_SYMBOL(am_meson_unregister_crtc_funcs);

static int am_meson_enable_vblank(struct drm_device *dev, unsigned int crtc)
{
	return 0;
}

static void am_meson_disable_vblank(struct drm_device *dev, unsigned int crtc)
{
}

static void am_meson_load(struct drm_device *dev)
{
#if 0
	struct meson_drm *priv = dev->dev_private;
	struct drm_crtc *crtc = priv->crtc;
	int pipe = drm_crtc_index(crtc);

	if (priv->crtc_funcs[pipe] &&
		priv->crtc_funcs[pipe]->loader_protect)
		priv->crtc_funcs[pipe]->loader_protect(crtc, true);
#endif
}

#ifdef CONFIG_DRM_MESON_USE_ION
static const struct drm_ioctl_desc meson_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MESON_GEM_CREATE, am_meson_gem_create_ioctl,
		DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
};
#endif

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= no_llseek,
#ifdef CONFIG_DRM_MESON_USE_ION
	.mmap		= am_meson_gem_mmap,
#else
	.mmap		= drm_gem_cma_mmap,
#endif
};

static struct drm_driver meson_driver = {
	/*driver_features setting move to probe functions*/
	.driver_features	= 0,
	/* Vblank */
	.enable_vblank		= am_meson_enable_vblank,
	.disable_vblank		= am_meson_disable_vblank,
	.get_vblank_counter	= drm_vblank_no_hw_counter,

#ifdef CONFIG_DRM_MESON_USE_ION
	/* PRIME Ops */
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,

	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_get_sg_table	= am_meson_gem_prime_get_sg_table,

	.gem_prime_import	= drm_gem_prime_import,
	/*
	 * If gem_prime_import_sg_table is NULL,only buffer created
	 * by meson driver can be imported ok.
	 */
	/*.gem_prime_import_sg_table = am_meson_gem_prime_import_sg_table,*/

	.gem_prime_vmap		= am_meson_gem_prime_vmap,
	.gem_prime_vunmap	= am_meson_gem_prime_vunmap,
	.gem_prime_mmap		= am_meson_gem_prime_mmap,

	/* GEM Ops */
	.dumb_create			= am_meson_gem_dumb_create,
	.dumb_destroy		= am_meson_gem_dumb_destroy,
	.dumb_map_offset		= am_meson_gem_dumb_map_offset,
	.gem_free_object_unlocked	= am_meson_gem_object_free,
	.gem_vm_ops			= &drm_gem_cma_vm_ops,
	.ioctls			= meson_ioctls,
	.num_ioctls		= ARRAY_SIZE(meson_ioctls),
#else
	/* PRIME Ops */
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_get_sg_table	= drm_gem_cma_prime_get_sg_table,
	.gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
	.gem_prime_vmap		= drm_gem_cma_prime_vmap,
	.gem_prime_vunmap	= drm_gem_cma_prime_vunmap,
	.gem_prime_mmap		= drm_gem_cma_prime_mmap,

	/* GEM Ops */
	.dumb_create		= drm_gem_cma_dumb_create,
	.dumb_destroy		= drm_gem_dumb_destroy,
	.dumb_map_offset	= drm_gem_cma_dumb_map_offset,
	.gem_free_object_unlocked = drm_gem_cma_free_object,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
#endif

	/* Misc */
	.fops			= &fops,
	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= "20180321",
	.major			= 1,
	.minor			= 0,
};

static int am_meson_drm_bind(struct device *dev)
{
	struct meson_drm *priv;
	struct drm_device *drm;
	int ret = 0;

	meson_driver.driver_features = DRIVER_HAVE_IRQ | DRIVER_GEM |
		DRIVER_MODESET | DRIVER_PRIME |
		DRIVER_ATOMIC | DRIVER_IRQ_SHARED;

	drm = drm_dev_alloc(&meson_driver, dev);
	if (!drm)
		return -ENOMEM;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_free1;
	}
	drm->dev_private = priv;
	priv->drm = drm;
	priv->dev = dev;
	dev_set_drvdata(dev, priv);

#ifdef CONFIG_DRM_MESON_USE_ION
	ret = am_meson_gem_create(priv);
	if (ret)
		goto err_free2;
#endif

	drm_mode_config_init(drm);

	/* Try to bind all sub drivers. */
	ret = component_bind_all(dev, drm);
	if (ret)
		goto err_gem;
	DRM_INFO("mode_config crtc number:%d\n", drm->mode_config.num_crtc);

	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret)
		goto err_unbind_all;

	drm_mode_config_reset(drm);
	drm->mode_config.max_width = 8192;
	drm->mode_config.max_height = 8192;
	drm->mode_config.funcs = &meson_mode_config_funcs;
	/*
	 * enable drm irq mode.
	 * - with irq_enabled = true, we can use the vblank feature.
	 */
	drm->irq_enabled = true;

	drm_kms_helper_poll_init(drm);

	am_meson_load(drm);

#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	ret = am_meson_drm_fbdev_init(drm);
	if (ret)
		goto err_poll_fini;
	drm->mode_config.allow_fb_modifiers = true;
#endif

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_fbdev_fini;

	return 0;


err_fbdev_fini:
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	am_meson_drm_fbdev_fini(drm);
err_poll_fini:
#endif
	drm_kms_helper_poll_fini(drm);
	drm->irq_enabled = false;
	drm_vblank_cleanup(drm);
err_unbind_all:
	component_unbind_all(dev, drm);
err_gem:
	drm_mode_config_cleanup(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
err_free2:
#endif
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
err_free1:
	drm_dev_unref(drm);

	return ret;
}

static void am_meson_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	am_meson_drm_fbdev_fini(drm);
#endif
	drm_kms_helper_poll_fini(drm);
	drm->irq_enabled = false;
	drm_vblank_cleanup(drm);
	component_unbind_all(dev, drm);
	drm_mode_config_cleanup(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
#endif
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_unref(drm);
}

static int compare_of(struct device *dev, void *data)
{
	struct device_node *np = data;

	return dev->of_node == np;
}

static void am_meson_add_endpoints(struct device *dev,
				   struct component_match **match,
				   struct device_node *port)
{
	struct device_node *ep, *remote;

	for_each_child_of_node(port, ep) {
		remote = of_graph_get_remote_port_parent(ep);
		if (!remote || !of_device_is_available(remote)) {
			of_node_put(remote);
			continue;
		} else if (!of_device_is_available(remote->parent)) {
			of_node_put(remote);
			continue;
		}
		component_match_add(dev, match, compare_of, remote);
		of_node_put(remote);
	}
}

static const struct component_master_ops am_meson_drm_ops = {
	.bind = am_meson_drm_bind,
	.unbind = am_meson_drm_unbind,
};

static bool am_meson_drv_use_osd(void)
{
	struct device_node *node;
	const  char *str;
	int ret;

	node = of_find_node_by_path("/meson-fb");
	if (node) {
		ret = of_property_read_string(node, "status", &str);
		if (ret) {
			DRM_INFO("get 'status' failed:%d\n", ret);
			return false;
		}

		if (strcmp(str, "okay") && strcmp(str, "ok")) {
			DRM_INFO("device %s status is %s\n",
				node->name, str);
		} else {
			DRM_INFO("device %s status is %s\n",
				node->name, str);
			return true;
		}
	}
	return false;
}

static int am_meson_drv_probe_prune(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct meson_drm *priv;
	struct drm_device *drm;
	int ret;

	/*driver_features reset to DRIVER_GEM | DRIVER_PRIME, for prune drm*/
	meson_driver.driver_features = DRIVER_GEM | DRIVER_PRIME;

	drm = drm_dev_alloc(&meson_driver, dev);
	if (!drm)
		return -ENOMEM;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_free1;
	}
	drm->dev_private = priv;
	priv->drm = drm;
	priv->dev = dev;

	platform_set_drvdata(pdev, priv);

#ifdef CONFIG_DRM_MESON_USE_ION
	ret = am_meson_gem_create(priv);
	if (ret)
		goto err_free2;
#endif

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_gem;

	return 0;

err_gem:
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
err_free2:
#endif
	drm->dev_private = NULL;
	platform_set_drvdata(pdev, NULL);
err_free1:
	drm_dev_unref(drm);
	return ret;
}

static int am_meson_drv_remove_prune(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	drm_dev_unregister(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
#endif
	drm->dev_private = NULL;
	platform_set_drvdata(pdev, NULL);
	drm_dev_unref(drm);

	return 0;
}

static int am_meson_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *port;
	struct component_match *match = NULL;
	int i;

	if (am_meson_drv_use_osd())
		return am_meson_drv_probe_prune(pdev);

	if (!np)
		return -ENODEV;

	/*
	 * Bind the crtc ports first, so that
	 * drm_of_find_possible_crtcs called from encoder .bind callbacks
	 * works as expected.
	 */
	for (i = 0;; i++) {
		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		component_match_add(dev, &match, compare_of, port->parent);
		of_node_put(port);
	}

	if (i == 0) {
		dev_err(dev, "missing 'ports' property.\n");
		return -ENODEV;
	}

	if (!match) {
		dev_err(dev, "No available vout found for display-subsystem.\n");
		return -ENODEV;
	}

	/*
	 * For each bound crtc, bind the encoders attached to its
	 * remote endpoint.
	 */
	for (i = 0;; i++) {
		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		am_meson_add_endpoints(dev, &match, port);
		of_node_put(port);
	}

	return component_master_add_with_match(dev, &am_meson_drm_ops, match);
}

static int am_meson_drv_remove(struct platform_device *pdev)
{
	if (am_meson_drv_use_osd())
		return am_meson_drv_remove_prune(pdev);

	component_master_del(&pdev->dev, &am_meson_drm_ops);
	return 0;
}

static const struct of_device_id am_meson_drm_dt_match[] = {
	{ .compatible = "amlogic,drm-subsystem" },
	{}
};
MODULE_DEVICE_TABLE(of, am_meson_drm_dt_match);

static struct platform_driver am_meson_drm_platform_driver = {
	.probe      = am_meson_drv_probe,
	.remove     = am_meson_drv_remove,
	.driver     = {
		.owner  = THIS_MODULE,
		.name   = DRIVER_NAME,
		.of_match_table = am_meson_drm_dt_match,
	},
};

module_platform_driver(am_meson_drm_platform_driver);

MODULE_AUTHOR("Jasper St. Pierre <jstpierre@mecheye.net>");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

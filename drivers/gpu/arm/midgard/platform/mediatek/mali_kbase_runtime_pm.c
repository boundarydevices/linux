#include <mali_kbase.h>
#include "mali_kbase_config_platform.h"
#include <mali_kbase_defs.h>

#include <linux/of_device.h>

struct mali_mtk_plat_data {
        struct kbase_pm_callback_conf *pm_callbacks;
        struct kbase_platform_funcs_conf *platform_funcs;
};

static const struct mali_mtk_plat_data mediatek_mt8183_data = {
	.pm_callbacks = &mt8183_pm_callbacks,
	.platform_funcs = &mt8183_platform_funcs,
};

static const struct mali_mtk_plat_data mediatek_mt8195_data = {
	.pm_callbacks = &mt8195_pm_callbacks,
	.platform_funcs = &mt8195_platform_funcs,
};

static const struct of_device_id device_ids[] = {
	{ .compatible = "mediatek,mt8183-mali", .data = &mediatek_mt8183_data },
	{ .compatible = "mediatek,mt8365-mali", .data = &mediatek_mt8183_data },
	{ .compatible = "mediatek,mt8195-mali", .data = &mediatek_mt8195_data },
	{ /* sentinel */ }
};

static int platform_init(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return -ENODEV;

	plat = id->data;
	if (plat && plat->platform_funcs &&
	    plat->platform_funcs->platform_init_func)
		return plat->platform_funcs->platform_init_func(kbdev);

	return 0;
}

static void platform_term(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return;

	plat = id->data;
	if (plat && plat->platform_funcs &&
	    plat->platform_funcs->platform_term_func)
		plat->platform_funcs->platform_term_func(kbdev);
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = platform_init,
	.platform_term_func = platform_term
};

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return -ENODEV;

	plat = id->data;
	if (plat && plat->pm_callbacks && plat->pm_callbacks->power_on_callback)
		return plat->pm_callbacks->power_on_callback(kbdev);

	return 0;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return;

	plat = id->data;
	if (plat && plat->pm_callbacks &&
	    plat->pm_callbacks->power_off_callback)
		plat->pm_callbacks->power_off_callback(kbdev);
}

static void pm_callback_resume(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return;

	plat = id->data;
	if (plat && plat->pm_callbacks &&
	    plat->pm_callbacks->power_resume_callback)
		plat->pm_callbacks->power_resume_callback(kbdev);
}

static void pm_callback_suspend(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return;

	plat = id->data;
	if (plat && plat->pm_callbacks &&
	    plat->pm_callbacks->power_suspend_callback)
		plat->pm_callbacks->power_suspend_callback(kbdev);
}

static int kbase_device_runtime_init(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return -ENODEV;

	plat = id->data;
	if (plat && plat->pm_callbacks &&
	    plat->pm_callbacks->power_runtime_init_callback)
		return plat->pm_callbacks->power_runtime_init_callback(kbdev);

	return 0;
}

static void kbase_device_runtime_disable(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return;

	plat = id->data;
	if (plat && plat->pm_callbacks &&
	    plat->pm_callbacks->power_runtime_term_callback)
		plat->pm_callbacks->power_runtime_term_callback(kbdev);
}

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return -ENODEV;

	plat = id->data;
	if (plat && plat->pm_callbacks &&
	    plat->pm_callbacks->power_runtime_on_callback)
		return plat->pm_callbacks->power_runtime_on_callback(kbdev);

	return 0;
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
	const struct of_device_id *id = of_match_device(device_ids, kbdev->dev);
	const struct mali_mtk_plat_data *plat;

	if (!id)
		return;

	plat = id->data;
	if (plat && plat->pm_callbacks &&
	    plat->pm_callbacks->power_runtime_off_callback)
		plat->pm_callbacks->power_runtime_off_callback(kbdev);
}

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = pm_callback_suspend,
	.power_resume_callback = pm_callback_resume,
#ifdef KBASE_PM_RUNTIME
	.power_runtime_init_callback = kbase_device_runtime_init,
	.power_runtime_term_callback = kbase_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
#else				/* KBASE_PM_RUNTIME */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif				/* KBASE_PM_RUNTIME */
};

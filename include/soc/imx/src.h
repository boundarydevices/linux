#ifndef __SOC_IMX_SRC_H
#define __SOC_IMX_SRC_H

#if IS_ENABLED(CONFIG_ARM)
bool imx_src_is_m4_enabled(void);
#else
static inline bool imx_src_is_m4_enabled(void) { return false; }
#endif

#endif /* __SOC_IMX_SRC_H */

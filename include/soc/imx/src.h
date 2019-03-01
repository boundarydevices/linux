#ifndef __SOC_IMX_SRC_H
#define __SOC_IMX_SRC_H

#if defined(CONFIG_HAVE_IMX_SRC)
bool imx_src_is_m4_enabled(void);
#else
static inline bool imx_src_is_m4_enabled(void) { return false; }
#endif

#endif /* __SOC_IMX_SRC_H */

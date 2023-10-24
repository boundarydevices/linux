// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright 2023 Boundary Devices
 */
#define TC358867_I2C_BUS	<&_MIPI_(I2C_BUS)>

#define concat(a, b)   _concat_(a, b)
#define _concat_(a, b)  a ## b
#define concat3(a, b, c)   _concat3_(a, b, c)
#define _concat3_(a, b, c)  a ## b ## c
#define concat4(a, b, c, d)   _concat4_(a, b, c, d)
#define _concat4_(a, b, c, d)  a ## b ## c ## d
#define concat6(a, b, c, d, e, f)   _concat6_(a, b, c, d, e, f)
#define _concat6_(a, b, c, d, e, f)  a ## b ## c ## d ## e ## f
#define concat8(a, b, c, d, e, f, g, h)   _concat8_(a, b, c, d, e, f, g, h)
#define _concat8_(a, b, c, d, e, f, g, h)  a ## b ## c ## d ## e ## f ## g ## h

#define c_(a, b)	concat3(a, _, b)

#define pinctrl(a) concat(pinctrl_, a): concat(a, grp)
#define pinctrl2(a, b) concat4(pinctrl_, a, _, b): concat(a-b, grp)
#define pinctrl3(a, b, c) concat6(pinctrl_, a, _, b, _, c): concat(a-b-c, grp)
#define pinctrl4(a, b, c, d) concat8(pinctrl_, a, _, b, _, c, _, d): concat(a-b-c-d, grp)

#define pinctrl_ref(a) concat(pinctrl_, a)
#define pinctrl_ref2(a, b) concat4(pinctrl_, a, _, b)
#define pinctrl_ref3(a, b, c) concat6(pinctrl_, a, _, b, _, c)
#define pinctrl_ref4(a, b, c, d) concat8(pinctrl_, a, _, b, _, c, _, d)

#define pinctrlm_ref(a)		c_(concat(pinctrl_, mipi), a)
#define pinctrlm(a)		pinctrlm_ref(a): concat(mipi-a, grp)

#define pinctrlm2(a, b)		pinctrlm_ref(c_(a, b)): concat(mipi-a-b, grp)
#define pinctrlm_ref2(a, b)	pinctrlm_ref(c_(a, b))

#define pinctrlm3(a, b, c)	pinctrlm_ref(c_(c_(a, b), c)): concat(mipi-a-b-c, grp)
#define pinctrlm_ref3(a, b, c)	pinctrlm_ref(c_(c_(a, b), c))

#define pinctrlm_ts(a)		pinctrl3(ts, mipi, a)
#define pinctrlm_ts_ref(a)	pinctrl_ref3(ts, mipi, a)

#define pinctrlm_ts2(a, b)	pinctrl4(ts, mipi, a, b)
#define pinctrlm_ts_ref2(a, b)	pinctrl_ref4(ts, mipi, a, b)

#define GP(a, b)	concat3(GP_, MIPI_, a)(b)
#define PD(a, b)	concat3(PD_, MIPI_, a)(b)
#define DN(a)		concat3(DN_, MIPI_, a)
#define UP(a)		concat3(UP_, MIPI_, a)
#define _MIPI_(a)	concat(MIPI_, a)
#define ts_mipi_ref(a)	concat3(ts_, mipi, concat(_, a))
#define ts_mipi_ref_grp(a) ts_mipi_ref(a): touchscreen
#define ts_mipi_ref2(a, b)	concat3(ts_, mipi, concat(_, c_(a, b)))
#define ts_mipi_ref_grp2(a, b) ts_mipi_ref(c_(a, b)): touchscreen-b

#define mipi_ref(a)	concat3(mipi, _, a)
#define mipi_ref_grp(a)	mipi_ref(a): mipi-a

#define mipi_ref2(a, b)	mipi_ref(c_(a, b))
#define mipi_ref_grp2(a, b) mipi_ref2(a, b): mipi-a-b

#define mipi_ref3(a, b, c)	mipi_ref(c_(c_(a, b), c))
#define mipi_ref_grp3(a, b, c)	mipi_ref3(a, b, c): mipi-a-b-c

#if defined(IMX8MM) || defined(IMX8MN) || defined(IMX8MP) || defined(IMX8MQ)
#define IMX
#define IOMUX iomuxc
#if defined(IMX8MQ)
#define PAD_NOPULL		0x06
#define PAD_PULLDN		0x06	/* No pulldown, need external resistor */
#define PAD_PULLUPIRQ		0xc6
#define PAD_PULLDNIRQ		0x86
#else
#define PAD_NOPULL		0x0
#define PAD_PULLDN		0x100
#define PAD_PULLUP		0x140
#define PAD_PULLDNIRQ		0x180
#define PAD_PULLUPIRQ		0x1c0
#endif
#define pins_group(name, _pins, _attr...) \
		fsl,pins = <		\
			##_pins		\
		>;
#define pd_irq_enable(_irq, _attr_irq, _enable, _attr_enable)	\
		pins_group(group,	\
			_irq		\
			_enable,	\
		)
#define pd_irq_reset(_irq, _attr_irq, _reset, _attr_reset) \
		pins_group(group,	\
			_irq		\
			_reset,		\
		)
#else
#define pins_group(name, _pins, _attr...)	\
	pins-##name {			\
		pinmux = <		\
			##_pins		\
		>;			\
		##_attr	\
	};
#define pd_irq_enable(_irq, _attr_irq, _enable, _attr_enable)	\
		pins_group(irq,					\
			_irq,					\
			_attr_irq				\
			input-enable;				\
		)						\
		pins_group(enable,				\
			_enable,				\
			_attr_enable				\
		)
#define pd_irq_reset(_irq, _attr_irq, _reset, _attr_reset)	\
		pins_group(irq,					\
			_irq,					\
			_attr_irq				\
			input-enable;				\
		)						\
		pins_group(reset,				\
			_reset,					\
			_attr_reset				\
		)
#endif

#ifndef MIPI_COMMANDS_DONE
#define dsi(a)		a
#define dsi1(a)		a
#define pinctrl_dsi(a)	c_(pinctrl, a)
#else
#undef dsi
#undef dsi1
#undef pinctrl_dsi
#define dsi(a)		concat(a, _dsi1)
#define dsi1(a)		concat(a, 1)
#define pinctrl_dsi(a)	c_(pinctrl_mipi1, a)
#endif

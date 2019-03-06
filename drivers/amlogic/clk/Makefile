#
# Makefile for Meson specific clk
#

obj-$(CONFIG_AMLOGIC_COMMON_CLK_SCPI)	+= clk-scpi.o
obj-$(CONFIG_AMLOGIC_RESET)	+= rstc.o

obj-$(CONFIG_AMLOGIC_CLK) += clk-pll.o
obj-$(CONFIG_AMLOGIC_CLK) += clk-mux.o
obj-$(CONFIG_AMLOGIC_CLK) += clk_measure.o
obj-$(CONFIG_AMLOGIC_CLK) += clk-meson-register.o

obj-$(CONFIG_AMLOGIC_GX_CLK) += clk-cpu.o clk-mpll.o clk_test.o clk-cpu-fclk-composite.o
obj-$(CONFIG_AMLOGIC_GX_CLK) += gxl/
obj-$(CONFIG_AMLOGIC_GX_CLK) += axg/
obj-$(CONFIG_AMLOGIC_GX_CLK) += txlx/
obj-$(CONFIG_AMLOGIC_GX_CLK) += txl/
obj-$(CONFIG_AMLOGIC_GX_CLK) += g12a/
obj-$(CONFIG_AMLOGIC_GX_CLK) += g12b/
obj-$(CONFIG_AMLOGIC_GX_CLK) += tl1/
obj-$(CONFIG_AMLOGIC_GX_CLK) += sm1/

obj-$(CONFIG_AMLOGIC_M8B_CLK) += m8b/

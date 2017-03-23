#
# Makefile for Meson specific clk
#

obj-$(CONFIG_AMLOGIC_COMMON_CLK_SCPI)           += clk-scpi.o
obj-$(CONFIG_AMLOGIC_RESET)	+= rstc.o

#obj-$(CONFIG_AMLOGIC_CLK) += clk-pll.o clk-cpu.o clk-mpll.o \
#			  clk_measure.o \
#			  clk_sdemmc.o clk_gpu.o clk_media.o clk_misc.o\
#			  clk-mux.o

obj-$(CONFIG_AMLOGIC_CLK) += clk-pll.o

obj-$(COMMON_AMLOGIC_GX_CLK) += clk-cpu.o clk-mpll.o clk-mux.o\
				clk_measure.o clk_sdemmc.o clk_gpu.o clk_media.o clk_misc.o\
				clk_gxl.o

obj-$(CONFIG_AMLOGIC_M8B_CLK) += m8b/clk-cpu.o m8b/meson8b.o
#obj-$(CONFIG_AMLOGIC_CLK) += clk_test.o

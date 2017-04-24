#
# Makefile for Meson specific clk
#

obj-$(CONFIG_AMLOGIC_COMMON_CLK_SCPI)           += clk-scpi.o
obj-$(CONFIG_AMLOGIC_RESET)	+= rstc.o

#obj-$(CONFIG_AMLOGIC_CLK) += clk-pll.o clk-cpu.o \
#			  clk_measure.o \
#			  clk_sdemmc.o clk_gpu.o clk_media.o clk_misc.o\
#			  clk-mux.o

obj-$(CONFIG_AMLOGIC_CLK) += clk-pll.o clk-mux.o clk_measure.o

obj-$(CONFIG_AMLOGIC_GX_CLK) += clk-cpu.o clk-mpll.o \
				clk_sdemmc.o clk_gpu.o clk_media.o clk_misc.o\
				gxl.o

obj-$(CONFIG_AMLOGIC_M8B_CLK) += m8b/clk-cpu.o m8b/meson8b.o m8b/clk_test.o \
				 m8b/clk-mpll.o m8b/clk_gpu.o m8b/clk_media.o m8b/clk_store.o m8b/clk_misc.o

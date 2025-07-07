# SPDX-License-Identifier: GPL-2.0

# common include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common

# platform driver
snd-soc-mt8189-afe-objs += \
	mt8189-afe-pcm.o \
	mt8189-afe-clk.o \
	mt8189-dai-adda.o \
	mt8189-dai-i2s.o \
	mt8189-dai-pcm.o \
	mt8189-dai-tdm.o \
	mt8189-dai-src.o

obj-$(CONFIG_SND_SOC_MT8189) += snd-soc-mt8189-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT8189_MT6359P) += mt8189-mt6359p.o

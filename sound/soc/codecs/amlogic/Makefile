KBUILD_CFLAGS += -Wlarger-than=28792
KBUILD_CFLAGS += -Wstack-usage=1856

#Amlogic
snd-soc-dummy_codec-objs := dummy_codec.o
snd-soc-pdm-dummy-objs := pdm_dummy.o
snd-soc-pcm2bt-objs  := pcm2bt.o
snd-soc-aml_t9015-objs := aml_codec_t9015.o
snd-soc-aml_t9015s-objs := aml_codec_t9015S.o
snd-soc-pmu3-objs := aml_pmu3.o
snd-soc-aml_codec_txlx_acodec-objs := aml_codec_txlx_acodec.o
snd-soc-aml_codec_tl1_acodec-objs  := aml_codec_tl1_acodec.o

#Third part codecs
snd-soc-tas5707-objs := tas5707.o
snd-soc-tlv320adc3101-objs := tlv320adc3101.o
snd-soc-pcm186x-objs := pcm186x.o pcm186x-i2c.o pcm186x-spi.o
snd-soc-ssm3515-objs := ssm3515.o
snd-soc-ssm3525-objs := ssm3525.o
snd-soc-ad82584f-objs := ad82584f.o
snd-soc-tas5805-objs := tas5805.o

# Amlogic
obj-$(CONFIG_AMLOGIC_SND_CODEC_DUMMY_CODEC) += snd-soc-dummy_codec.o
obj-$(CONFIG_AMLOGIC_SND_CODEC_PDM_DUMMY_CODEC) += snd-soc-pdm-dummy.o
obj-$(CONFIG_AMLOGIC_SND_CODEC_PCM2BT) += snd-soc-pcm2bt.o
obj-$(CONFIG_AMLOGIC_SND_CODEC_AMLT9015) += snd-soc-aml_t9015.o
obj-$(CONFIG_AMLOGIC_SND_CODEC_AMLT9015S) += snd-soc-aml_t9015s.o
obj-$(CONFIG_AMLOGIC_SND_CODEC_PMU3) += snd-soc-pmu3.o
obj-$(CONFIG_AMLOGIC_SND_CODEC_TXLX_ACODEC) += snd-soc-aml_codec_txlx_acodec.o
obj-$(CONFIG_AMLOGIC_SND_CODEC_TL1_ACODEC)  += snd-soc-aml_codec_tl1_acodec.o

#Third part codecs
obj-$(CONFIG_AMLOGIC_SND_SOC_TAS5707) += snd-soc-tas5707.o
obj-$(CONFIG_AMLOGIC_SND_SOC_TLV320ADC3101) += snd-soc-tlv320adc3101.o
obj-$(CONFIG_AMLOGIC_SND_SOC_PCM186X) += snd-soc-pcm186x.o
obj-$(CONFIG_AMLOGIC_SND_SOC_SSM3515) += snd-soc-ssm3515.o
obj-$(CONFIG_AMLOGIC_SND_SOC_SSM3525) += snd-soc-ssm3525.o
obj-$(CONFIG_AMLOGIC_SND_SOC_TAS575X) += tas575x.o
obj-$(CONFIG_AMLOGIC_SND_SOC_ES7243) += es7243.o
obj-$(CONFIG_AMLOGIC_SND_SOC_AD82584F) += ad82584f.o
obj-$(CONFIG_AMLOGIC_SND_SOC_CS42528) += cs42528.o
obj-$(CONFIG_AMLOGIC_SND_SOC_TAS5805) += snd-soc-tas5805.o

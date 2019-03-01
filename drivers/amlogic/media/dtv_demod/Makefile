
obj-$(CONFIG_AMLOGIC_DTV_DEMOD) += dtvdemod.o
#obj-y += dtvdemod.o
#obj-m += dtvdemod.o

dtvdemod-objs := demod_func.o  dvbc_func.o i2c_func.o tuner_func.o atsc_func.o dvbc_v2.o dvbc_v3.o dtmb_func.o dvbt_v2.o#dvbt_func.o

dtvdemod-objs += amlfrontend.o demod_dbg.o

dtvdemod-objs	+= aml_demod.o


ccflags-y += -I.
ccflags-y += -I$(srctree)/drivers/media/dvb-core
ccflags-y += -Idrivers/amlogic/media/dtv_demod/include
#ccflags-y += -I./include
#ccflags-y += -Idrivers/amlogic/dvb_tv/
ccflags-y += -DDVB_CORE_ORI=1 -DDVB_49=1 -DDEBUG

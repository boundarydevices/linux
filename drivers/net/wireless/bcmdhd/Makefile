#bcmdhd flags
DHDCFLAGS = -Wall -Wstrict-prototypes -Dlinux -DBCMDRIVER					\
	-DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DWLBTAMP -DBCMFILEIMAGE	\
	-DDHDTHREAD -DDHD_GPL -DDHD_SCHED -DDHD_DEBUG -DBDC -DTOE				\
	-DDHD_BCMEVENTS -DSHOW_EVENTS -DDONGLEOVERLAYS -DBCMDBG					\
	-DMMC_SDIO_ABORT -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P		\
	-DNEW_COMPAT_WIRELESS -DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT			\
	-DKEEP_ALIVE -DPKT_FILTER_SUPPORT \
	-DEMBEDDED_PLATFORM -DPNO_SUPPORT          \
	-Idrivers/net/wireless/bcmdhd -Idrivers/net/wireless/bcmdhd/include -Iarch/arm/plat-samsung/include

DHDOFILES = aiutils.o bcmsdh_sdmmc_linux.o dhd_linux.o siutils.o bcmutils.o \
	dhd_linux_sched.o bcmwifi.o dhd_sdio.o bcmevent.o dhd_bta.o hndpmu.o  	\
	bcmsdh.o dhd_cdc.o bcmsdh_linux.o dhd_common.o linux_osl.o            	\
	bcmsdh_sdmmc.o dhd_custom_gpio.o sbutils.o wldev_common.o wl_android.o

obj-$(CONFIG_BCMDHD) += bcmdhd.o
bcmdhd-objs += $(DHDOFILES)

DHDCFLAGS += -DSDIO_ISR_THREAD

ifeq ($(CONFIG_MACH_ODROID_4210),y)
	DHDOFILES += hardkernel.o
	DHDCFLAGS += -DCUSTOMER_HW
	DHDCFLAGS += -DCSCAN
#	DHDCFLAGS += -DSDIO_ISR_THREAD
	DHDCFLAGS += -DOOB_INTR_ONLY -DHARDKERNEL_OOB -DHW_OOB
	DHDCFLAGS += -DENABLE_INSMOD_NO_FW_LOAD -DSET_RANDOM_MAC_SOFTAP
#	DHDCFLAGS += -DBCMWAPI_WPI -DBCMWAPI_WAI
endif

ifneq ($(CONFIG_WIRELESS_EXT),)
bcmdhd-objs += wl_iw.o
DHDCFLAGS += -DSOFTAP -DWL_WIRELESS_EXT
endif
ifneq ($(CONFIG_CFG80211),)
bcmdhd-objs += wl_cfg80211.o wl_cfgp2p.o wl_linux_mon.o dhd_cfg80211.o
DHDCFLAGS += -DWL_CFG80211
endif
EXTRA_CFLAGS = $(DHDCFLAGS)
ifeq ($(CONFIG_BCMDHD),m)
EXTRA_LDFLAGS += --strip-debug
endif

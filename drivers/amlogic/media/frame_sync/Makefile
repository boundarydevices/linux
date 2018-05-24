obj-$(CONFIG_AMLOGIC_MEDIA_VIDEO)	+=	frame_sync.o

ccflags-y = -DMODIFY_TIMESTAMP_INC_WITH_PLL -DCALC_CACHED_TIME #-DUSE_VMASTER_MODE

frame_sync-objs	+=	ptsserv.o
frame_sync-objs	+=	timestamp.o
frame_sync-objs	+=	tsync.o
frame_sync-objs	+=	tsync_pcr.o

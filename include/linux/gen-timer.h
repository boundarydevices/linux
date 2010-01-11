#ifndef _LINUX_GEN_TIMER_H_
#define _LINUX_GEN_TIMER_H_

#define BASE_MAGIC 0xBD

#define SET_TSTAMP    _IOW(BASE_MAGIC, 0x01, unsigned long)
#define CLEAR_TSTAMP  _IOW(BASE_MAGIC, 0x02, unsigned long)
#define GET_TSTAMP    _IOR(BASE_MAGIC, 0x01, unsigned long)

#endif

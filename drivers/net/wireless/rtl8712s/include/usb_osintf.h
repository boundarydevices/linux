#ifndef __USB_OSINTF_H
#define __USB_OSINTF_H

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <usb_vendor_req.h>

#define USBD_HALTED(Status) ((ULONG)(Status) >> 30 == 3)


uint usb_dvobj_init(_adapter * adapter);
void usb_dvobj_deinit(_adapter * adapter);

unsigned int usb_inirp_init(_adapter * padapter);
unsigned int usb_inirp_deinit(_adapter * padapter);


u8 usbvendorrequest(struct dvobj_priv *pdvobjpriv, RT_USB_BREQUEST brequest, RT_USB_WVALUE wvalue, u8 windex, void* data, u8 datalen, u8 isdirectionin);

void rtl871x_intf_stop(_adapter *padapter);

#endif


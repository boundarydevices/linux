/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _HCI_OPS_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <osdep_intf.h>


#ifndef CONFIG_SDIO_HCI

#error  "CONFIG_SDIO_HCI shall be on!\n"

#endif


#ifdef PLATFORM_LINUX
#include <sdio_ops_linux.h>
#endif

#ifdef PLATFORM_OS_CE

#include <sdio_ops_ce.h>

#endif

#ifdef PLATFORM_OS_XP

#include <sdio_ops_xp.h>

#endif

#include <rtl871x_byteorder.h>

static uint __inline get_deviceid(u32 addr)
{
	_func_enter_;


	switch( (addr & 0xffff0000) )
	{
	case RTL8712_SDIO_LOCAL_BASE:
		_func_exit_;
		return DID_SDIO_LOCAL;

	case RTL8712_IOBASE_IOREG:
		_func_exit_;
		return DID_WLAN_IOREG;

	default:
		if(((addr & 0xfff00000) == RTL8712_IOBASE_FF) && ((addr & 0x000fffff)< 0xaffff))
		{
			_func_exit_;
			return DID_WLAN_FIFO;
		}
		else
		{
			RT_TRACE(_module_hci_ops_c_,_drv_err_,(" error:  something wrong in get_deviceid \n"));
		}
		break;
	}


	
	_func_exit_;
	return DID_UNDEFINE;	//Undefine Address. Register R/W Protocol is needed.
	
}



static u32 __inline ff2faddr(const u32 addr)  
{

	switch(addr)
	{
	
	case RTL8712_DMA_RX0FF:
		return ((DID_WLAN_FIFO << 15) | (( OFFSET_RX_RX0FFQ) << 2));
		
	case RTL8712_DMA_VOQ:
		return  ((DID_WLAN_FIFO << 15) | ((OFFSET_TX_VOQ)<<2));
		
	case RTL8712_DMA_VIQ:
		return ((DID_WLAN_FIFO << 15) | ((OFFSET_TX_VIQ)<<2));
		
	case RTL8712_DMA_BEQ:
		return ((DID_WLAN_FIFO << 15) | ((OFFSET_TX_BEQ)<<2));
		
	case RTL8712_DMA_BKQ:
		return ((DID_WLAN_FIFO << 15) | (( OFFSET_TX_BKQ)<<2));

				
	case RTL8712_DMA_H2CCMD:
		return ((DID_WLAN_FIFO << 15) | ((OFFSET_TX_CMDQ)<<2));
		
	case RTL8712_DMA_C2HCMD:
		return ((DID_WLAN_FIFO << 15) | ((OFFSET_RX_C2HFFQ)<<2));

		

	case RTL8712_DMA_BCNQ:
		return ((DID_WLAN_FIFO << 15) | ((OFFSET_TX_BCNQ)<<2));
		
	case RTL8712_DMA_MGTQ:
		return ((DID_WLAN_FIFO << 15) | (( OFFSET_TX_MGTQ)<<2));
		
	case RTL8712_DMA_BMCQ:
		return ((DID_WLAN_FIFO << 15) | ((OFFSET_TX_HIQ)<<2));
		
	default:
		return 0;
	}
}



 
uint __inline _cvrt2ftaddr(const u32 addr, u32 *pftaddr)  
{
	_func_enter_;



	//*********************************************//
	// For 8712, the API's return value:
	// FAIL: can't use cmd53 to access, only cmd52 is allowed
	// SUCCESS: CMD53 is acceptable
	
	switch(get_deviceid(addr))
	{

		case DID_WLAN_FIFO:
			*pftaddr = ff2faddr(addr);
			if(*pftaddr == 0){
				RT_TRACE(_module_hci_ops_c_,_drv_notice_,(" DID_WLAN_FIFO ; translate address error!!! addr = %x; *pftaddr=%x \n ", addr,*pftaddr));
				return _FAIL;
			}
			return _SUCCESS;
			
		case DID_SDIO_LOCAL:
			*pftaddr = ((DID_SDIO_LOCAL << 15) | (addr & OFFSET_SDIO_LOCAL));
			RT_TRACE(_module_hci_ops_c_,_drv_debug_,(" DID_SDIO_LOCAL;*pftaddr=%x \n ", *pftaddr));
			return _SUCCESS;
			
		case DID_WLAN_IOREG:
			*pftaddr = ((DID_WLAN_IOREG << 15) | (addr & OFFSET_WLAN_IOREG));
			RT_TRACE(_module_hci_ops_c_,_drv_debug_,(" DID_WLAN_IOREG ;*pftaddr=%x \n ", *pftaddr));
			return _SUCCESS; //using cmd52 to access

		default:
			_func_exit_;
			RT_TRACE(_module_hci_ops_c_,_drv_err_,("Error ==> The access register address = %x is wrong ###\n", addr));
			return _FAIL;
	}

	


	_func_exit_;
	return _SUCCESS;
}




void sdio_attrib_read(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	u8 i;

	u8 (*_cmd52r)(struct intf_priv *pintfpriv, u32 addr);
	
	struct _io_ops *pop = &pintfhdl->io_ops;
	
	_cmd52r = pop->_cmd52r;
	
	_func_enter_;
	if(0xff000000== (addr&0xff000000) ){
		addr=addr &0x00ffffff;
		RT_TRACE(_module_hci_ops_c_,_drv_notice_,("\n sdio_attrib_read: func1 cmd52 read addr=0x%x\n",addr));
		_cmd52r = pop->_cmdfunc152r;
	}
	
	for(i=0; i<cnt; i++){
		RT_TRACE(_module_hci_ops_c_,_drv_notice_,("\n sdio_attrib_read: func0 cmd52 read addr=0x%x\n",addr));
		rmem[i] = _cmd52r( pintfhdl->pintfpriv, addr+i);
	}
	_func_exit_;

	return;
}



void sdio_attrib_write(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{

	u8 i;

	void (*_cmd52w)(struct intf_priv *pintfpriv, u32 addr, u8 val8);

	struct _io_ops *pop = &pintfhdl->io_ops;

	_cmd52w = pop->_cmd52w;

	_func_enter_;

	if(0xff000000== (addr&0xff000000) ){
		addr=addr &0x00ffffff;
		RT_TRACE(_module_hci_ops_c_,_drv_notice_,("\n sdio_attrib_read: func1 cmd52 write addr=0x%x\n",addr));
		_cmd52w= pop->_cmdfunc152w;
	}
	

	for(i=0; i<cnt; i++)
	{
		RT_TRACE(_module_hci_ops_c_,_drv_notice_,("\n sdio_attrib_read: func0 cmd52 write addr=0x%x\n",addr));
		_cmd52w( pintfhdl->pintfpriv, addr+i, wmem[i]);
	}

	_func_exit_;
	return;
}

void sdio_func1cmd52_read(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	u8 i;

	u8 (*_cmd52r)(struct intf_priv *pintfpriv, u32 addr);
	
	struct _io_ops *pop = &pintfhdl->io_ops;
	
	_cmd52r = pop->_cmdfunc152r;
	
	_func_enter_;
	
	for(i=0; i<cnt; i++)
		rmem[i] = _cmd52r( pintfhdl->pintfpriv, addr+i);

	_func_exit_;

	return;
}



void sdio_func1cmd52_write(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{

	u8 i;

	void (*_cmd52w)(struct intf_priv *pintfpriv, u32 addr, u8 val8);

	struct _io_ops *pop = &pintfhdl->io_ops;

	_cmd52w = pop->_cmdfunc152w;

	_func_enter_;

	for(i=0; i<cnt; i++)
	{
		_cmd52w( pintfhdl->pintfpriv, addr+i, wmem[i]);
	}

	_func_exit_;
	return;
}

#ifdef PLATFORM_OS_XP

NTSTATUS
  sdbus_async_read_complete(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp,
    IN PVOID  Context
    )
{
	struct io_req	*pio_req=(struct io_req	*)Context; 
	struct io_queue *pio_q=(struct io_queue *)pio_req->cnxt;
	struct intf_priv * pintfpriv = pio_q->intf.pintfpriv;

	RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdbus_async_read_complete: \n"));


//	_memcpy(&pio_req->val,sdrp->ResponseData.AsUCHAR, 4);
	_up_sema(&pio_req->sema);



return STATUS_MORE_PROCESSING_REQUIRED;


}


static u8 sdbus_async_read
(struct io_queue *pio_q , u32 addr, u32 cnt, struct io_req	*pio_req,PIO_COMPLETION_ROUTINE  CompletionRoutine)
{
	SD_RW_EXTENDED_ARGUMENT ioarg;
	PIO_STACK_LOCATION	nextStack;
	PIO_COMPLETION_ROUTINE pcomplete_routine;
	NDIS_STATUS status = 0;

	struct intf_priv * pintfpriv = pio_q->intf.pintfpriv;
	PMDL pmdl = pio_req->pmdl;//pintfpriv->pmdl_test; // map to io_rwmem
	PIRP  pirp  = pio_req->pirp;//pintfpriv->piorw_irp_test;
	PSDBUS_REQUEST_PACKET sdrp = pio_req->sdrp;//pintfpriv->sdrp;
	struct dvobj_priv * psdiodev = (struct dvobj_priv * )pintfpriv->intf_dev;
	
	const SDCMD_DESCRIPTOR ReadWriteIoExtendedDesc = {SDCMD_IO_RW_EXTENDED,
                                                    SDCC_STANDARD,
                                                    SDTD_READ,
                                                    SDTT_SINGLE_BLOCK,
                                                    SDRT_5};
	_func_enter_;
	RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("\n=====sdbus_async_read======\n"));
	pio_req->val=0;
	pio_req->cnxt=(u8 *)pio_q;
	RtlZeroMemory(sdrp, sizeof(SDBUS_REQUEST_PACKET));

	sdrp->RequestFunction = SDRF_DEVICE_COMMAND;
	sdrp->Parameters.DeviceCommand.CmdDesc = ReadWriteIoExtendedDesc;
	if(CompletionRoutine==NULL){
		pcomplete_routine=&sdbus_async_read_complete;
	}
	else{
		pcomplete_routine=CompletionRoutine;
	}
	ioarg.u.AsULONG = 0;
	if (cnt >4)
	{
		ioarg.u.bits.Count = 0;
		sdrp->Parameters.DeviceCommand.Length = 4;
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("\n*** async cmd53 RD size >4 [cnt=%d] ***\n ",cnt));
		cnt=4;
	}
	else
	{
		ioarg.u.bits.Count = cnt;
		sdrp->Parameters.DeviceCommand.Length = cnt;  
	}
	
	ioarg.u.bits.Address = addr;
	ioarg.u.bits.OpCode  = 1;
	ioarg.u.bits.Function = psdiodev->func_number;
	ioarg.u.bits.WriteToDevice = 0;


	  //pirp = IoAllocateIrp( psdiodev->NextDeviceStackSize + 1, FALSE);

	nextStack = IoGetNextIrpStackLocation(pirp);
	nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	nextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SD_SUBMIT_REQUEST;
	nextStack->Parameters.DeviceIoControl.Type3InputBuffer = sdrp;	 
	nextStack->Parameters.DeviceIoControl.OutputBufferLength=cnt;
	//sdrp->UserContext[0] =  Adapter;

	sdrp->Parameters.DeviceCommand.Argument = ioarg.u.AsULONG;
	sdrp->Parameters.DeviceCommand.Mdl = pmdl;

	status = 
	SdBusSubmitRequestAsync (psdiodev->sdbusinft.Context,
							sdrp,
							pirp,
							pcomplete_routine,
							(PVOID)pio_req);//(PVOID)pio_q);//(&callback_context));
							


	if(status!= STATUS_PENDING){
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** async cmd53 RD failed. ***\n "));
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** STATUS = %x ***\n ", status));
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** Addr = %x ***\n ",addr));
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** Length = %d ***\n ",cnt));
		_func_exit_;
		return _FAIL;
	}
	_func_exit_;
	return _SUCCESS;

}
NTSTATUS
  sdbus_async_write_complete(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp,
    IN PVOID  Context
    )
{
	struct io_req	*pio_req=(struct io_req	*)Context; 
	struct io_queue *pio_q=(struct io_queue *)pio_req->cnxt;
	struct intf_priv * pintfpriv = pio_q->intf.pintfpriv;
	RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdbus_async_read_complete: \n"));

	_up_sema(&pio_req->sema);



	return STATUS_MORE_PROCESSING_REQUIRED;


}

static u8 sdbus_async_write
(struct io_queue *pio_q , u32 addr, u32 cnt,struct io_req	*pio_req,PIO_COMPLETION_ROUTINE  CompletionRoutine)
{

	SD_RW_EXTENDED_ARGUMENT ioarg;
	PIO_STACK_LOCATION	nextStack;
	PIO_COMPLETION_ROUTINE pcomplete_routine;
	NDIS_STATUS status = 0;

	struct intf_priv * pintfpriv = pio_q->intf.pintfpriv;
	PMDL pmdl = pio_req->pmdl;//pintfpriv->pmdl_test; // map to io_rwmem
	PIRP  pirp  =pio_req->pirp;// pintfpriv->piorw_irp_test;
	PSDBUS_REQUEST_PACKET sdrp = pio_req->sdrp;//pintfpriv->sdrp;
	struct dvobj_priv * psdiodev = (struct dvobj_priv * )pintfpriv->intf_dev;
	
	const SDCMD_DESCRIPTOR ReadWriteIoExtendedDesc = {SDCMD_IO_RW_EXTENDED,
                                                    SDCC_STANDARD,
                                                    SDTD_WRITE,
                                                    SDTT_SINGLE_BLOCK,
                                                    SDRT_5};
	_func_enter_;
	RT_TRACE(_module_hci_ops_c_,_drv_err_,("\n===sdbus_async_write====\n ",cnt));
		pio_req->cnxt=(u8 *)pio_q;
	RtlZeroMemory(sdrp, sizeof(SDBUS_REQUEST_PACKET));

	sdrp->RequestFunction = SDRF_DEVICE_COMMAND;
	sdrp->Parameters.DeviceCommand.CmdDesc = ReadWriteIoExtendedDesc;
	if(CompletionRoutine==NULL){
		pcomplete_routine=&sdbus_async_write_complete;
	}
	else{
		pcomplete_routine=CompletionRoutine;
	}
	
	ioarg.u.AsULONG = 0;
	if (cnt >4)
	{
		ioarg.u.bits.Count = 0;
		sdrp->Parameters.DeviceCommand.Length = 4;
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** async cmd53 Write size >4 [cnt=%d] ***\n ",cnt));
		cnt=4;
	}
	else
	{
		ioarg.u.bits.Count = cnt;
		sdrp->Parameters.DeviceCommand.Length = cnt;  
	}
	
	ioarg.u.bits.Address = addr;
	ioarg.u.bits.OpCode  = 1;
	ioarg.u.bits.Function = psdiodev->func_number;
	ioarg.u.bits.WriteToDevice = 1;


	  //pirp = IoAllocateIrp( psdiodev->NextDeviceStackSize + 1, FALSE);

	nextStack = IoGetNextIrpStackLocation(pirp);
	nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	nextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SD_SUBMIT_REQUEST;
	nextStack->Parameters.DeviceIoControl.Type3InputBuffer = sdrp;	 
	nextStack->Parameters.DeviceIoControl.OutputBufferLength=cnt;

	//sdrp->UserContext[0] =  Adapter;

	sdrp->Parameters.DeviceCommand.Argument = ioarg.u.AsULONG;
	sdrp->Parameters.DeviceCommand.Mdl = pmdl;

	status = 
	SdBusSubmitRequestAsync (psdiodev->sdbusinft.Context,
							sdrp,
							pirp,
							pcomplete_routine,
							(PVOID)pio_req);//(PVOID)pio_q);//(&callback_context));
							


	if(status!= STATUS_PENDING){
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** async cmd53 Write failed. ***\n "));
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** STATUS = %x ***\n ", status));
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** Addr = %x ***\n ",addr));
		RT_TRACE(_module_hci_ops_c_,_drv_err_,("*** Length = %d ***\n ",cnt));
		_func_exit_;
		return _FAIL;
	}
	_func_exit_;
	return _SUCCESS;
}


u32 sdio_read_async(struct intf_hdl *pintfhdl, u32 addr, u8 cnt)
{
	_irqL irqL;
	u32 res=0;

	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;
	struct io_queue * pio_q=((struct dvobj_priv *)pintfpriv->intf_dev)->padapter->pio_queue;
	struct io_req	*pio_req=NULL;

	_func_enter_;
	pio_req=alloc_ioreq(pio_q);
	if(pio_req==NULL){
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("\n  pio_req==NULL   \n"));
	}else{
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdio_read8: async read\n"));
		sdbus_async_read(pio_q, addr,cnt,pio_req, NULL);
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdio_read8:async read==wait sema== \n"));
		_down_sema(&pio_req->sema);
		res=cpu_to_le32(pio_req->val);
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdio_read8:async read:  pio_req->val=0x%x\n",pio_req->val));
		free_ioreq(pio_req, pio_q);
	}
	return res;
}

void sdio_write_async(struct intf_hdl *pintfhdl, u32 addr, u8 cnt,u32 val)
{
	_irqL irqL;
	u32 res=0;

	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;
	struct io_queue * pio_q=((struct dvobj_priv *)pintfpriv->intf_dev)->padapter->pio_queue;
	struct io_req	*pio_req=NULL;

	_func_enter_;
	pio_req=alloc_ioreq(pio_q);
	if(pio_req==NULL){
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("\n  pio_req==NULL   \n"));
	}else{
		pio_req->val=cpu_to_le32(val);
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdio_read8: async read\n"));
		sdbus_async_write(pio_q, addr,cnt,pio_req, NULL);
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdio_read8:async read==wait sema== \n"));
		_down_sema(&pio_req->sema);
		res=pio_req->val;
		free_ioreq(pio_req, pio_q);
	}
	return;
}



u32 sdio_read_mem_async(struct intf_hdl *pintfhdl, u32 addr, u8 cnt)
{
	_irqL irqL;
	u32 res=0;

	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;
	struct io_queue * pio_q=((struct dvobj_priv *)pintfpriv->intf_dev)->padapter->pio_queue;
	struct io_req	*pio_req=NULL;

	_func_enter_;
	pio_req=alloc_ioreq(pio_q);
	if(pio_req==NULL){
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("\n  pio_req==NULL   \n"));
	}else{
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdio_read8: async read\n"));
		sdbus_async_read(pio_q, addr,cnt,pio_req, NULL);
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdio_read8:async read==wait sema== \n"));
		_down_sema(&pio_req->sema);
		res=cpu_to_le32(pio_req->val);
		RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("sdio_read8:async read:  pio_req->val=0x%x\n",pio_req->val));
		free_ioreq(pio_req, pio_q);
	}
	return res;
}

#endif

u8 sdio_read8(struct intf_hdl *pintfhdl, u32 addr)
{
	_irqL irqL;
	u32 res;
	u8	val;
	
	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;
	
	u8 *rwmem = (u8 *)(pintfpriv->io_rwmem);
	
	u32 ftaddr = 0 ;
	struct dvobj_priv  *psdiodev = (struct dvobj_priv  *)pintfpriv->intf_dev;

	_func_enter_;
	//RT_TRACE(_module_hci_ops_c_,_drv_err_,"sdio_read8\n");
	

	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS)
	{
		res=sdbus_read_reg(pintfpriv, ftaddr, 1, &val);

		if(res!=_SUCCESS){
				RT_TRACE(_module_hci_ops_c_,_drv_emerg_,(" sdio_read8[addr=0x%x] fail!!!\n",addr));
				val=0;
			}

	}
	else
	{
	_enter_hwio_critical(&pintfpriv->rwlock, &irqL);
		sdio_func1cmd52_read(pintfhdl, ftaddr, 1, &val);
	_exit_hwio_critical(&pintfpriv->rwlock, &irqL);	
	}

	
	_func_exit_;

	return val;	

}

u16 sdio_read16(struct intf_hdl *pintfhdl, u32 addr)
{
	_irqL irqL;
	u16 val;
	u32	res;


	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;

	u8 *rwmem = (u8 *)(pintfpriv->io_rwmem);

	u32 ftaddr = 0 ;
	struct dvobj_priv  *psdiodev = (struct dvobj_priv  *)pintfpriv->intf_dev;


	_func_enter_;
	//RT_TRACE(_module_hci_ops_c_,_drv_err_,"sdio_read16\n");

	

	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS)
	{
		res=sdbus_read_reg(pintfpriv, ftaddr, 2, &val);

		if(res!=_SUCCESS){
				RT_TRACE(_module_hci_ops_c_,_drv_emerg_,(" sdio_read16[addr=0x%x] fail!!!\n",addr));
				val=0;
			}

		}	
	else
	{
	_enter_hwio_critical(&pintfpriv->rwlock, &irqL);
		sdio_func1cmd52_read(pintfhdl, ftaddr, 2, (u8*)(&val));
	_exit_hwio_critical(&pintfpriv->rwlock, &irqL);	
	}
	
	

	val = le16_to_cpu(val);

	_func_exit_;

	return val;	
}

u32 sdio_read32(struct intf_hdl *pintfhdl, u32 addr)
{
	_irqL irqL;
	
	u32 val,res;
	
	
	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;

	u8 *rwmem = (u8 *)pintfpriv->io_rwmem;

	u32 ftaddr = 0 ;
	struct dvobj_priv  *psdiodev = (struct dvobj_priv  *)pintfpriv->intf_dev;

	_func_enter_;
	//RT_TRACE(_module_hci_ops_c_,_drv_err_,"sdio_read32\n");
	
	

	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS)
	{
		res=sdbus_read_reg(pintfpriv, ftaddr, 4, &val);
			
		if(res!=_SUCCESS){

			RT_TRACE(_module_hci_ops_c_,_drv_emerg_,(" sdio_read32[addr=0x%x] fail!!!\n",addr));
			val=0;
		}
		
	}
	else
	{
	_enter_hwio_critical(&pintfpriv->rwlock, &irqL);
		sdio_func1cmd52_read(pintfhdl, ftaddr, 4, (u8*)(&val));
	_exit_hwio_critical(&pintfpriv->rwlock, &irqL);	
	}

	
	
        val = le32_to_cpu(val);
	_func_exit_;
	
	return val;	
}


void sdio_write8(struct intf_hdl *pintfhdl, u32 addr, u8 val)
{
	_irqL irqL;

	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;

	u8 *rwmem = (u8 *)pintfpriv->io_rwmem;

	u32 ftaddr = 0 ;
	struct dvobj_priv  *psdiodev = (struct dvobj_priv  *)pintfpriv->intf_dev;


	_func_enter_;
	//RT_TRACE(_module_hci_ops_c_,_drv_err_,"sdio_write8 \n\n");

	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS)
	{

		sdbus_write_reg(pintfpriv, ftaddr, 1, &val);
	}
	else
	{
		_memcpy(rwmem ,&val, 1);

	_enter_hwio_critical(&pintfpriv->rwlock, &irqL);
		sdio_func1cmd52_write(pintfhdl, ftaddr, 1, &val);
	_exit_hwio_critical(&pintfpriv->rwlock, &irqL);	
	}

	

	_func_exit_;

}



void sdio_write16(struct intf_hdl *pintfhdl, u32 addr, u16 val)
{
	_irqL irqL;

	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;

	u8 *rwmem = (u8 *)pintfpriv->io_rwmem;

	u32 ftaddr = 0 ;

	//RT_TRACE(_module_hci_ops_c_,_drv_err_,"sdio_write16 \n\n");
	struct dvobj_priv  *psdiodev = (struct dvobj_priv  *)pintfpriv->intf_dev;


	_func_enter_;
	

	val = cpu_to_le16(val);


	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS)
	{

		sdbus_write_reg(pintfpriv, ftaddr, 2,&val);
	}
	else
	{
		_memcpy(rwmem ,&val, 2);

	_enter_hwio_critical(&pintfpriv->rwlock, &irqL);
		sdio_func1cmd52_write(pintfhdl, ftaddr, 2, (u8*)(&val));
	_exit_hwio_critical(&pintfpriv->rwlock, &irqL);
	
	}
	

	_func_exit_;

}




void sdio_write32(struct intf_hdl *pintfhdl, u32 addr, u32 val)
{
	_irqL irqL;
	
	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;

	u8 *rwmem = (u8 *)pintfpriv->io_rwmem;

	u32 ftaddr = 0 ;
	struct dvobj_priv  *psdiodev = (struct dvobj_priv  *)pintfpriv->intf_dev;

	//RT_TRACE(_module_hci_ops_c_,_drv_err_,"sdio_write32 \n\n");

	_func_enter_;

	

	val = cpu_to_le32(val);


	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS)
	{

		sdbus_write_reg(pintfpriv, ftaddr, 4,&val);

	}
	else
	{
		_memcpy(rwmem ,&val, 4);

	_enter_hwio_critical(&pintfpriv->rwlock, &irqL);
		sdio_func1cmd52_write(pintfhdl, ftaddr, 4, (u8*)(&val));
	_exit_hwio_critical(&pintfpriv->rwlock, &irqL);
	}

	
	

	_func_exit_;

}

void inc_rd_cnt(const u32 addr, u32 *pftaddr,struct dvobj_priv *psdio_dev){
	switch(addr){
		case RTL8712_DMA_RX0FF:
			RT_TRACE(_module_hci_ops_c_,_drv_debug_,("\n*pftaddr=0x%x rx0_cnt=0x%x \n",*pftaddr,psdio_dev->rxfifo_cnt));
			*pftaddr=(*pftaddr)|(0x3&psdio_dev->rxfifo_cnt++);
			RT_TRACE(_module_hci_ops_c_,_drv_debug_,("\n*pftaddr=0x%x\n",*pftaddr));
			RT_TRACE(_module_hci_ops_c_,_drv_debug_,(" DID_WLAN_FIFO ;addr = %x; *pftaddr=%x \n ", addr,*pftaddr));
			break;
		case RTL8712_DMA_C2HCMD:
			RT_TRACE(_module_hci_ops_c_,_drv_debug_,("\n*pftaddr=0x%x  rx0_cnt=0x%x \n",*pftaddr,psdio_dev->cmdfifo_cnt));
			*pftaddr=(*pftaddr)|(0x3&psdio_dev->cmdfifo_cnt++);
			RT_TRACE(_module_hci_ops_c_,_drv_debug_,("\n*pftaddr=0x%x\n",*pftaddr));
			RT_TRACE(_module_hci_ops_c_,_drv_debug_,(" DID_WLAN_FIFO ;addr = %x; *pftaddr=%x \n ", addr,*pftaddr));
			break;
		default:
			break;

	}		

}
u32 sdio_read_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	
	u32	res, r_cnt, ftaddr;

	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;

	uint (*_sdbus_read_bytes_to_membuf)(struct intf_priv *pintfpriv, u32 addr, u32 cnt, u8 *pbuf);
	uint (*_sdbus_read_blocks_to_membuf)(struct intf_priv *pintfpriv, u32 addr, u32 cnt, u8 *pbuf);
	struct recv_buf	*precv_buf=(struct recv_buf	*)rmem;
	_adapter* 	padapter = (_adapter*)(pintfhdl->adapter);
	struct recv_priv *precvpriv=&padapter->recvpriv;
	u32	read_ptr=0;
	u32	maxlen = padapter->dvobjpriv.block_transfer_len;// pintfpriv->max_recvsz;

	struct dvobj_priv *psdio_dev = (struct dvobj_priv*)&padapter->dvobjpriv;

	_sdbus_read_bytes_to_membuf = pintfhdl->io_ops._sdbus_read_bytes_to_membuf;
	_sdbus_read_blocks_to_membuf = pintfhdl->io_ops._sdbus_read_blocks_to_membuf;
	
	_func_enter_;
	// Please remember, you just can't only use lock/unlock to 
	// protect the rw functions...
	// since, i/o is quite common in call-back and isr routines...

	if  (cnt == 0)
	{
		_func_exit_;
		return _FAIL;		
	}
	do {
		_cvrt2ftaddr(addr, &ftaddr);
		inc_rd_cnt(addr, &ftaddr, psdio_dev);
		RT_TRACE(_module_hci_ops_c_,_drv_info_,("sdio_read_port: addr=0x%x ftaddr=0x%x\n",addr,ftaddr));

		if (psdio_dev->rx_block_mode)	
			r_cnt = (((cnt) >> padapter->dvobjpriv.blk_shiftbits)?((cnt) >> padapter->dvobjpriv.blk_shiftbits):1) <<  padapter->dvobjpriv.blk_shiftbits;
		else
			r_cnt = (cnt > maxlen) ? maxlen:cnt;
		
		if (r_cnt)
		{
			//if ((psdio_dev->rx_block_mode)&&(RTL8712_DMA_C2HCMD!=addr))	
			if ((psdio_dev->rx_block_mode))				
				res = _sdbus_read_blocks_to_membuf(pintfpriv, ftaddr, r_cnt, rmem);
			else{
			//	if(RTL8712_DMA_C2HCMD==addr){
			//		res = _sdbus_read_bytes_to_membuf(pintfpriv, ftaddr, r_cnt,rmem);
			//		_memcpy(rmem, padapter->pio_queue->intf.pintfpriv->c2h_mem, 40);
			//		RT_TRACE(_module_hci_ops_c_,_drv_err_,(" sdio_read_port C2H FIFO: cnt=%d r_cnt=%d \n",cnt, r_cnt));
			//	}
			//	else
				{
					if(cnt >maxlen){
						RT_TRACE(_module_hci_ops_c_,_drv_err_,(" @@@@@@@@@@sdio_read_port: cnt=%d r_cnt=%d \n",cnt, r_cnt));
					}
						res = _sdbus_read_bytes_to_membuf(pintfpriv, ftaddr, r_cnt,(u8 *)&(precv_buf->pbuf[read_ptr]));
						_memcpy(&(precv_buf->pbuf[read_ptr]), &precvpriv->bytecnt_buf[0], r_cnt);
						read_ptr+=r_cnt>>2;
						RT_TRACE(_module_hci_ops_c_,_drv_err_,("^^^^^^sdio_read_port: cnt=%d r_cnt=%d read_ptr=%d\n",cnt, r_cnt,read_ptr));
					}
			}
			if (res == _FAIL){
				RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("ERROR!!!---------------sdio_read_port : FAIL \n"));
			}
		}

		if(cnt >r_cnt){
		cnt -= r_cnt;
		rmem += r_cnt;
		}
		else{
			cnt=0;
			rmem += r_cnt;
		}

		if ((psdio_dev->rx_block_mode)&&(RTL8712_DMA_C2HCMD!=addr))	
		{
			if (cnt) {
				_cvrt2ftaddr(addr, &ftaddr);
				inc_rd_cnt(addr, &ftaddr,psdio_dev);
				RT_TRACE(_module_hci_ops_c_,_drv_info_,(" addr=0x%x ftaddr=0x%x\n",addr,ftaddr));
				
				res = _sdbus_read_bytes_to_membuf(pintfpriv, ftaddr, cnt, rmem);
				if (res == _FAIL){
					RT_TRACE(_module_hci_ops_c_,_drv_emerg_,("---------------sdio_read_port : FAIL  \n"));

				}
			}
			break;
		}

	}while (cnt > 0);
	
	_func_exit_;
return _SUCCESS;

}



void sdio_read_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	_irqL irqL;
	
	u32	res;
	u32 ftaddr = 0 ;

	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;
	_func_enter_;
	_enter_hwio_critical(&pintfpriv->rwlock, &irqL);

	if ((_cvrt2ftaddr(addr, &ftaddr)) == _SUCCESS)
	{
		res=sdbus_read_reg(pintfpriv, ftaddr, cnt, rmem);
	}
	else
	{
		sdio_func1cmd52_read(pintfhdl, ftaddr, cnt, (u8*)(rmem));
	}

	
	_exit_hwio_critical(&pintfpriv->rwlock, &irqL);

	_func_exit_;

}



u32 sdio_write_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{

	u32		res,ftaddr=0,w_cnt;
	
	uint (*_sdbus_write_blocks_from_membuf)(struct intf_priv *pintfpriv, u32 addr, u32 cnt, u8 *pbuf,u8 async);

	uint (*_sdbus_write_bytes_from_membuf)(struct intf_priv *pintfpriv, u32 addr, u32 cnt, u8 *pbuf);
		
	_adapter* 		padapter = (_adapter*)(pintfhdl->adapter);
	
	struct intf_priv 	*pintfpriv = pintfhdl->pintfpriv;


	struct dvobj_priv	*psdio_dev = &padapter->dvobjpriv;
	u32 	maxlen = psdio_dev->block_transfer_len;//pintfpriv->max_xmitsz;

	_sdbus_write_blocks_from_membuf = pintfhdl->io_ops._sdbus_write_blocks_from_membuf;

	_sdbus_write_bytes_from_membuf  = pintfhdl->io_ops._sdbus_write_bytes_from_membuf;


	// Please remember, you just can't only use lock/unlock to 
	// protect the rw functions...
	// since, i/o is quite common in call-back and isr routines...
	
	_func_enter_;
	
	if (cnt == 0) {

		_func_exit_;

		return _SUCCESS;
	}

	_cvrt2ftaddr(addr, &ftaddr);
		if(addr==RTL8712_DMA_H2CCMD ){
			w_cnt=(((cnt) >>9) << 9 )+((cnt <<23)?512:0);//cnt;
			res = _sdbus_write_blocks_from_membuf(pintfpriv, ftaddr,w_cnt, wmem,_FALSE);
		}
		else 
		{
		if ((psdio_dev->tx_block_mode)/*&&(adapter->hw_init_completed)*/)	{
			//w_cnt = ((cnt) >> 9) << 9;
			w_cnt=(((cnt) >>9) << 9 )+((cnt <<23)?512:0);//cnt;
			RT_TRACE(_module_hci_ops_c_,_drv_info_,(" 8712 psdio_dev->tx_block_mode =%x _sdbus_write_blocks_from_membuf (cnt=%d) (cnt (%d)<<23 =%x)\n",psdio_dev->tx_block_mode,w_cnt,cnt,(cnt <<23)));
			if(padapter->hw_init_completed==_TRUE){
			res = _sdbus_write_blocks_from_membuf(pintfpriv, ftaddr,w_cnt, wmem,_TRUE);
			}
			else{
			res = _sdbus_write_blocks_from_membuf(pintfpriv, ftaddr,w_cnt, wmem,_FALSE);
			}
			RT_TRACE(_module_hci_ops_c_,_drv_info_,("8712(end) psdio_dev->block_mode =%x _sdbus_write_blocks_from_membuf (cnt=%d)\n",psdio_dev->tx_block_mode,w_cnt));
		}
		else{
			RT_TRACE(_module_hci_ops_c_,_drv_info_,(" 8712 psdio_dev->block_mode =%x _sdbus_write_bytes_from_membuf (cnt=%d) pbuf=0x%p\n",psdio_dev->tx_block_mode,cnt,wmem));
			do{
				w_cnt = (cnt > maxlen) ? maxlen:cnt;
				RT_TRACE(_module_hci_ops_c_,_drv_info_,(" 8712 psdio_dev->block_mode =%x _sdbus_write_bytes_from_membuf (w_cnt=%d) pbuf=0x%p\n",psdio_dev->tx_block_mode,w_cnt,wmem));
				res = _sdbus_write_bytes_from_membuf(pintfpriv, ftaddr, w_cnt, wmem);		
				if (res == _FAIL)
					break;
				cnt -= w_cnt;
				wmem += w_cnt;
			}while (cnt > 0);
		}
	}
	
	_func_exit_;

	return _SUCCESS;

}



void sdio_write_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{

	_irqL irqL;
	struct intf_priv *pintfpriv = pintfhdl->pintfpriv;

	_func_enter_;
	
	_enter_hwio_critical(&pintfpriv->rwlock, &irqL);

	cnt = _RND4(cnt);

	sdio_write_port(pintfhdl, addr, cnt, wmem);
	_exit_hwio_critical(&pintfpriv->rwlock, &irqL);

	_func_exit_;

}








void sdio_set_intf_option(u32 *poption)
{
	_func_enter_;
	//*poption = ((*poption) | _INTF_ASYNC_);
	if (*poption & _INTF_ASYNC_)
		*poption = ((*poption) ^ _INTF_ASYNC_);
	_func_exit_;
	
}


void sdio_intf_hdl_init(u8 *priv)
{
	
	_func_enter_;
	_func_exit_;
	return;
}

void sdio_intf_hdl_unload(u8 *priv)
{
	
	_func_enter_;
	_func_exit_;
	return;
}
void sdio_intf_hdl_open(u8 *priv)
{
	
	_func_enter_;
	_func_exit_;
	return;
}

void sdio_intf_hdl_close(u8 *priv)
{
	
	_func_enter_;
	_func_exit_;
	return;
	
}

void sdio_set_intf_funs(struct intf_hdl *pintf_hdl)
{
	_func_enter_;
	pintf_hdl->intf_hdl_init = &sdio_intf_hdl_init;
	pintf_hdl->intf_hdl_unload = &sdio_intf_hdl_unload;
	pintf_hdl->intf_hdl_open = &sdio_intf_hdl_open;
	pintf_hdl->intf_hdl_close = &sdio_intf_hdl_close;
	_func_exit_;
}

void sdio_unload_intf_priv(struct intf_priv *pintfpriv)
{
_func_enter_;

#ifdef PLATFORM_OS_XP

	if (pintfpriv->pmdl) {
		IoFreeMdl(pintfpriv->pmdl);
		pintfpriv->pmdl = NULL;
	}
	
	if (pintfpriv->sdrp) {	
		ExFreePool(pintfpriv->sdrp);
		pintfpriv->sdrp = NULL;
	}
	
/*	if (pintfpriv->recv_pmdl) {
		IoFreeMdl(pintfpriv->recv_pmdl);
		pintfpriv->recv_pmdl = NULL;
	}
*/	
	if (pintfpriv->recv_sdrp) {	
		ExFreePool(pintfpriv->recv_sdrp);
		pintfpriv->recv_sdrp = NULL;
	}
/*
	if (pintfpriv->xmit_pmdl) {
		IoFreeMdl(pintfpriv->xmit_pmdl);
		pintfpriv->xmit_pmdl = NULL;
	}
*/	
	if (pintfpriv->xmit_sdrp) {	
		ExFreePool(pintfpriv->xmit_sdrp);
		pintfpriv->xmit_sdrp = NULL;
	}
#endif
	
	if (pintfpriv->allocated_io_rwmem) {
		_mfree((u8*)(pintfpriv->allocated_io_rwmem), pintfpriv->max_xmitsz+4);
		pintfpriv->allocated_io_rwmem = NULL;
		pintfpriv->io_rwmem = NULL;
	}

_func_exit_;
}

/*

*/
uint sdio_init_intf_priv(struct intf_priv *pintfpriv)
{
	struct dvobj_priv  *psdiodev = (struct dvobj_priv  *)pintfpriv->intf_dev;
	
#ifdef PLATFORM_OS_XP
	PMDL	pmdl = NULL;
	PSDBUS_REQUEST_PACKET  sdrp = NULL;
	PSDBUS_REQUEST_PACKET  recv_sdrp = NULL;
	PSDBUS_REQUEST_PACKET  xmit_sdrp = NULL;
#endif

	_func_enter_;

	//pintfpriv->intf_status = _IOREADY;
	pintfpriv->max_xmitsz =  512;
	pintfpriv->max_recvsz =  512;
	//pintfpriv->max_iosz =  52; //64-12

	_rtl_rwlock_init(&pintfpriv->rwlock);
	
	pintfpriv->allocated_io_rwmem = (u8 *)_malloc(pintfpriv->max_xmitsz +4); 
    if (pintfpriv->allocated_io_rwmem == NULL)
    	goto sdio_init_intf_priv_fail;

	pintfpriv->io_rwmem = ALIGN_ADDR(pintfpriv->allocated_io_rwmem, 4);

#ifdef PLATFORM_OS_XP

	pmdl = IoAllocateMdl((u8 *)pintfpriv->io_rwmem, pintfpriv->max_xmitsz , FALSE, FALSE, NULL);
	if(pmdl == NULL){
		//RT_TRACE(_module_hci_ops_c_,_drv_err_,COMP_INIT, DBG_TRACE, ("MDL is NULL.\n "));	
		goto sdio_init_intf_priv_fail;
	}
	MmBuildMdlForNonPagedPool(pmdl);
	pintfpriv->pmdl = pmdl;
	
	sdrp = ExAllocatePool(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET));
	 if(sdrp == NULL){
	 	//RT_TRACE(_module_hci_ops_c_,_drv_err_,COMP_INIT, DBG_TRACE, ("SDRP is NULL.\n "));
		goto sdio_init_intf_priv_fail;
	 }
	pintfpriv->sdrp = sdrp;

	recv_sdrp = ExAllocatePool(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET));
	if(recv_sdrp == NULL){
		goto sdio_init_intf_priv_fail;
	}
//	pintfpriv->recv_pmdl = NULL;
	pintfpriv->recv_sdrp = recv_sdrp;
	
	xmit_sdrp = ExAllocatePool(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET));
	if(xmit_sdrp == NULL){
		goto sdio_init_intf_priv_fail;
	}
//	pintfpriv->xmit_pmdl = NULL;
	pintfpriv->xmit_sdrp = xmit_sdrp;

#endif

	_func_exit_;

	return _SUCCESS;
			
sdio_init_intf_priv_fail:
	sdio_unload_intf_priv(pintfpriv);
	_func_exit_;
		
	return _FAIL;
}

void sdio_set_intf_ops(struct _io_ops *pops)
{
	_func_enter_;

	memset((u8 *)pops, 0, sizeof(struct _io_ops));
	
#ifdef PLATFORM_LINUX

	pops->_cmd52r = &sdbus_cmd52r;
	pops->_cmd52w = &sdbus_cmd52w;
	pops->_cmdfunc152r = &sdbus_direct_read8;
	pops->_cmdfunc152w = &sdbus_direct_write8;
	pops->_sdbus_read_bytes_to_membuf = &sdbus_read_bytes_to_recvbuf;
        pops->_sdbus_read_blocks_to_membuf = &sdbus_read_blocks_to_recvbuf;		

	pops->_sdbus_write_blocks_from_membuf = &sdbus_write_blocks_from_xmitbuf;
	pops->_sdbus_write_bytes_from_membuf = &sdbus_write_bytes_from_xmitbuf;

#endif

#ifdef PLATFORM_OS_CE

	pops->_cmd52r = &sdbus_cmd52r_ce;
	pops->_cmd52w = &sdbus_cmd52w_ce;
	pops->_cmdfunc152r = &sdbus_func1cmd52r_ce;
	pops->_cmdfunc152w = &sdbus_func1cmd52w_ce;

	pops->_sdbus_read_bytes_to_membuf = &sdbus_read_bytes_to_membuf_ce;
	pops->_sdbus_read_blocks_to_membuf = &sdbus_read_blocks_to_membuf_ce;

	pops->_sdbus_write_blocks_from_membuf = &sdbus_write_blocks_from_membuf_ce;
	pops->_sdbus_write_bytes_from_membuf = &sdbus_write_bytes_from_membuf_ce;

#endif
	
#ifdef PLATFORM_OS_XP

	pops->_cmd52r = &sdbus_cmd52r_xp;
	pops->_cmd52w = &sdbus_cmd52w_xp;
	pops->_cmdfunc152r = &sdbus_func1cmd52r_xp;
	pops->_cmdfunc152w = &sdbus_func1cmd52w_xp;
	
	pops->_sdbus_read_bytes_to_membuf = &sdbus_read_bytes_to_membuf_xp;
	pops->_sdbus_read_blocks_to_membuf = &sdbus_read_blocks_to_membuf_xp;
	
	pops->_sdbus_write_blocks_from_membuf = &sdbus_write_blocks_from_membuf_xp;
	pops->_sdbus_write_bytes_from_membuf = &sdbus_write_bytes_from_membuf_xp;

#endif	

	pops->_attrib_read = &sdio_attrib_read;
	pops->_read8 = &sdio_read8;
	pops->_read16 = &sdio_read16;
	pops->_read32 = &sdio_read32;
	pops->_read_mem = &sdio_read_mem;
			
	pops->_attrib_write = &sdio_attrib_write;
	pops->_write8 = &sdio_write8;
	pops->_write16 = &sdio_write16;
	pops->_write32 = &sdio_write32;
	pops->_write_mem = &sdio_write_mem;

	pops->_sync_irp_protocol_rw = NULL;

	pops->_read_port = &sdio_read_port;
	pops->_write_port = &sdio_write_port;

	_func_exit_;
}


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
#define _RTL871X_PWRCTRL_C_

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <osdep_intf.h>


#ifdef CONFIG_PWRCTRL

/*
 * set_rpwm() MUST be called under pwrpriv->lock protection.
 */
void set_rpwm(PADAPTER padapter, u8 val8)
{
	u8 rpwm;
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;

_func_enter_;

	if (pwrpriv->rpwm == val8) {
		if (pwrpriv->rpwm_retry == 0) {
			RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("set_rpwm: Already set rpwm [0x%02x]\n", val8));
			return;
		}
	}

	if ((padapter->bDriverStopped == _TRUE) ||
	    (padapter->bSurpriseRemoved == _TRUE)) {
		//DbgPrint("xmit_thread:bDriverStopped(%d) OR bSurpriseRemoved(%d)", padapter->bDriverStopped, padapter->bSurpriseRemoved);
		RT_TRACE(_module_rtl871x_xmit_c_, _drv_err_,
			 ("set_rpwm: bDriverStopped or bSurpriseRemoved\n"));
		return;
	}

	rpwm = val8 | pwrpriv->tog;
	switch (val8)
	{
		case PS_STATE_S1:
			pwrpriv->cpwm = val8;
			break;
		case PS_STATE_S2://only for USB normal powersave mode use,temp mark some code.
		case PS_STATE_S3:
		case PS_STATE_S4:
#ifdef CONFIG_SDIO_HCI
//			pwrpriv->cpwm = val8;
			// enable firmware trigger interrupt to notice power state change
			rpwm |= BIT(6);
			_set_timer(&pwrpriv->rpwm_check_timer, 1000);
#endif
#ifdef CONFIG_USB_HCI
			pwrpriv->cpwm = val8;
			//rpwm |= BIT(6);
			//_set_timer(&pwrpriv->rpwm_check_timer, 1000);
#endif
			RT_TRACE(_module_rtl871x_xmit_c_,_drv_info_,("set_rpwm: set_timer\n"));
			break;
		default:
			RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("set_rpwm: value=0x%02x is wrong!!!\n", val8));
			return;
	}
	pwrpriv->rpwm_retry = 0;
	pwrpriv->rpwm = val8;
	
	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_notice_,("set_rpwm: rpwm=0x%02x\n", rpwm));
#ifdef CONFIG_SDIO_HCI
	write8(padapter, SDIO_HRPWM, rpwm);
#else
	write8(padapter, 0x1025FE58, rpwm);
#endif
	
	pwrpriv->tog += 0x80;

_func_exit_;
}


void set_ps_mode(PADAPTER padapter, uint ps_mode, uint smart_ps)
{
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;

_func_enter_;

	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_info_,("+Power Mode(%d), Smart_PS(%d)\n", ps_mode, smart_ps));

	if (ps_mode > PM_Card_Disable) {
		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("ps_mode:%d error\n", ps_mode));
		goto exit;
	}

	// if driver is in active state, we dont need set smart_ps.
	if(ps_mode == PS_MODE_ACTIVE)
	{
		smart_ps = 0;
	}
	else if(pwrpriv->force_active) //force driver leave ps mode, do not sleep
	{
		goto exit;
	}

	if( (pwrpriv->pwr_mode != ps_mode) || (pwrpriv->smart_ps != smart_ps))
	{		
		if(pwrpriv->pwr_mode == PS_MODE_ACTIVE)
		{
			pwrpriv->bSleep = _TRUE;
		}
		else
		{
			pwrpriv->bSleep = _FALSE;
		}
		pwrpriv->pwr_mode = ps_mode;
		pwrpriv->smart_ps = smart_ps;

		_set_workitem(&(pwrpriv->SetPSModeWorkItem));
	}

exit:	
_func_exit_;
}


void leave_ps_mode(PADAPTER padapter)
{
	u32 i, limit;
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;

_func_enter_;

	if ((padapter->bDriverStopped == _TRUE) ||
	    (padapter->bSurpriseRemoved== _TRUE)) {
		RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_err_,
			 ("leave_ps_mode: bDriverStopped or bSurpriseRemoved\n"));
		return;
	}

	_enter_pwrlock(&pwrpriv->lock);
	if (pwrpriv->pwr_mode != PS_MODE_ACTIVE)
	{
//		set_ps_mode(padapter, PS_MODE_ACTIVE, 0);
		pwrpriv->pwr_mode = PS_MODE_ACTIVE;
		set_rpwm(padapter, PS_STATE_S4);
	}
	_exit_pwrlock(&pwrpriv->lock);

	limit = 1000;
	for (i = 0; i < limit; i++) {
		if (pwrpriv->cpwm >= PS_STATE_S2) break;
		else msleep_os(1);
	}

	if (i == limit) {
		RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_err_,
			 ("leave_ps_mode: TIMEOUT! cpwn=%#x\n", pwrpriv->cpwm));
	}

_func_exit_;
}

/*
Caller:ISR handler...

This will be called when CPWM interrupt is up.

using to update cpwn of drv; and drv willl make a decision to up or down pwr level
*/
void cpwm_int_hdl(PADAPTER padapter, struct reportpwrstate_parm *preportpwrstate)
{
	struct pwrctrl_priv *pwrpriv = &(padapter->pwrctrlpriv);
	struct cmd_priv	*pcmdpriv = &(padapter->cmdpriv);
	struct xmit_priv	*pxmitpriv = &(padapter->xmitpriv);

_func_enter_;

	if(pwrpriv->cpwm_tog == ((preportpwrstate->state)&0x80)){
		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,
			 ("cpwm_int_hdl: cpwm_tog=0x%02x this time cpwm=0x%02x toggle bit didn't change!\n",
			  pwrpriv->cpwm_tog, preportpwrstate->state));
		goto exit;
	}
	
	_cancel_timer_ex(&padapter->pwrctrlpriv.rpwm_check_timer);

//	_enter_pwrlock(&pwrpriv->lock);

	pwrpriv->cpwm = (preportpwrstate->state)&0xf;

	if(pwrpriv->cpwm >= PS_STATE_S2){
		if(pwrpriv->alives & CMD_ALIVE)
			_up_sema(&(pcmdpriv->cmd_queue_sema));

		if(pwrpriv->alives & XMIT_ALIVE)
			_up_sema(&(pxmitpriv->xmit_sema));
	}
	pwrpriv->cpwm_tog=  (preportpwrstate->state)&0x80;

//	_exit_pwrlock(&pwrpriv->lock);

exit:
	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_info_,("-cpwm_int_hdl: cpwm=0x%02x\n", pwrpriv->cpwm));

_func_exit_;

}


static __inline void register_task_alive(struct pwrctrl_priv *pwrctrl, uint tag)
{
	pwrctrl->alives |= tag;
}

static __inline void unregister_task_alive(struct pwrctrl_priv *pwrctrl, uint tag)
{
	pwrctrl->alives &= ~tag;
}

void _rpwm_check_handler (PADAPTER padapter)
{
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;

_func_enter_;

	if ((padapter->bDriverStopped == _TRUE) ||
	    (padapter->bSurpriseRemoved == _TRUE)) {
		RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_err_,
			 ("_rpwm_check_handler: bDriverStopped(%d) bSurpriseRemoved(%d); Cancel timer!\n",
			  padapter->bDriverStopped, padapter->bSurpriseRemoved));
		return;
	}

	if (pwrpriv->cpwm != pwrpriv->rpwm) {
		RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_err_,
			 ("_rpwm_check_handler: Set RPWM(0x%02x) again[cpwm=0x%02x]!\n",
			  pwrpriv->rpwm, pwrpriv->cpwm));
		_set_workitem(&pwrpriv->rpwm_workitem);
	}
_func_exit_;

}
void cpwm_hdl(PADAPTER padapter);
#ifdef PLATFORM_WINDOWS

void rpwm_check_handler (
	IN	PVOID					SystemSpecific1,
	IN	PVOID					FunctionContext,
	IN	PVOID					SystemSpecific2,
	IN	PVOID					SystemSpecific3
	)
{
	_adapter *adapter = (_adapter *)FunctionContext;
	u8 bcancelled;
	if(adapter->bDriverStopped == _TRUE || adapter->bSurpriseRemoved== _TRUE){
		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("rpwm_check_handler  bDriverStopped or bSurpriseRemoved\n"));
		_cancel_timer(&adapter->pwrctrlpriv.rpwm_check_timer, &bcancelled);
		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("rpwm_check_handler  cancel timer\n"));
	}else{
		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("rpwm_check_handler\n"));
		_rpwm_check_handler(adapter);
	}
}

void rpwm_workitem_callback(
	IN NDIS_WORK_ITEM*	pWorkItem,
	IN PVOID			Context
	)
{
	_adapter			*padapter = (_adapter *)Context;
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;
	u8 cpwm=pwrpriv->cpwm;

_func_enter_;
//	msleep_os(10);
	_enter_pwrlock(&pwrpriv->lock);
	if(pwrpriv->cpwm !=pwrpriv->rpwm){
//		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("rpwm_workitem_callback: set RPWM(0x%x) again[cpwm=0x%x]!!!!\n",pwrpriv->rpwm,pwrpriv->cpwm));
		cpwm=read8(padapter, SDIO_HCPWM);
/*		if(cpwm>1 && (cpwm!=pwrpriv->cpwm)){
			reportpwrstate.state=cpwm;	
		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("rpwm_workitem_callback:RPWM(0x%x) [cpwm=0x%x]  call cpwm_hdl!!!!\n",pwrpriv->rpwm,pwrpriv->cpwm));
			
			cpwm_hdl(padapter);
		}
		else if(pwrpriv->cpwm !=pwrpriv->rpwm)
*/
		{
			
		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("rpwm_workitem_callback:RPWM(0x%x) [cpwm=0x%x]  cpwn=0x%X SET RPWM again!!!!\n",pwrpriv->rpwm,pwrpriv->cpwm,cpwn));
			pwrpriv->rpwm_retry=1;
			set_rpwm(padapter, pwrpriv->rpwm);
		}
	}
	else{
		RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("rpwm_workitem_callback: RPWM(0x%x)  [cpwm=0x%x]!!!!\n",pwrpriv->rpwm,pwrpriv->cpwm));
	}
	_exit_pwrlock(&pwrpriv->lock);
_func_exit_;
}
//-----------------------------------------------------------------------------
//	Description:
//		Work item for Set PowerSaving Mode.
//
//	Assumption:
//		Since it is callback function of SetPSModeWorkItem,
//		it should be in PASSIVE level.
//
//-----------------------------------------------------------------------------
void
SetPSModeWorkItemCallback(
	IN NDIS_WORK_ITEM*	pWorkItem,
	IN PVOID			Context
	)
{
	_adapter			*padapter = (_adapter *)Context;
	struct pwrctrl_priv *pwrpriv = &padapter->pwrctrlpriv;

_func_enter_;

	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_info_,("SetPSModeWorkItemCallback: Power Mode is :%d\n", pwrpriv->pwr_mode));

	_enter_pwrlock(&pwrpriv->lock);

	if(pwrpriv->bSleep)
	{
		if(!setpwrmode_cmd(padapter,  pwrpriv->pwr_mode, pwrpriv->smart_ps)) {
			RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("Can't invoke FW to change its power settings.\n"));
		}

		//set_rpwm(padapter, PS_STATE_S2);
		//set_rpwm(padapter, PS_STATE_S0);
	}
	else
	{
		if(pwrpriv->pwr_mode == PS_MODE_ACTIVE)
			set_rpwm(padapter, PS_STATE_S4);

		if(!setpwrmode_cmd(padapter,  pwrpriv->pwr_mode, pwrpriv->smart_ps)) {
			RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("Can't invoke FW to change its power settings.\n"));
		}
	}

	_exit_pwrlock(&pwrpriv->lock);

_func_exit_;

}
#endif

#ifdef PLATFORM_LINUX
void SetPSModeWorkItemCallback(struct work_struct *work)
{
	struct pwrctrl_priv *pwrpriv = container_of(work, struct pwrctrl_priv, SetPSModeWorkItem);
	_adapter *padapter = container_of(pwrpriv, _adapter, pwrctrlpriv);

_func_enter_;

	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_info_,("+%s: Power Mode=%d\n", __FUNCTION__, pwrpriv->pwr_mode));

	_enter_pwrlock(&pwrpriv->lock);

	if(pwrpriv->bSleep)
	{
		if(!setpwrmode_cmd(padapter,  pwrpriv->pwr_mode, pwrpriv->smart_ps)) {
			RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("Can't invoke FW to change its power settings.\n"));
		}

		//set_rpwm(padapter, PS_STATE_S2);
		//set_rpwm(padapter, PS_STATE_S0);
	}
	else
	{
		if(pwrpriv->pwr_mode == PS_MODE_ACTIVE)
			set_rpwm(padapter, PS_STATE_S4);

		if(!setpwrmode_cmd(padapter,  pwrpriv->pwr_mode, pwrpriv->smart_ps)) {
			RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("Can't invoke FW to change its power settings.\n"));
		}
	}

	_exit_pwrlock(&pwrpriv->lock);

_func_exit_;

}

void rpwm_workitem_callback(struct work_struct *work)
{
	u8 cpwm;
	struct pwrctrl_priv *pwrpriv = container_of(work, struct pwrctrl_priv, rpwm_workitem);
	PADAPTER padapter = container_of(pwrpriv, _adapter, pwrctrlpriv);

_func_enter_;

	_enter_pwrlock(&pwrpriv->lock);

	if (pwrpriv->cpwm != pwrpriv->rpwm)
	{
		cpwm = read8(padapter, SDIO_HCPWM);
		RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_err_,
			 ("rpwm_workitem_callback: set RPWM=0x%x again! cpwm=0x%x/0x%x(HW)\n",
			  pwrpriv->rpwm, pwrpriv->cpwm, cpwm));
#if 0
		if ((cpwm > 1) && (cpwm != pwrpriv->cpwm)) {
			struct reportpwrstate_parm reportpwrstate;

			reportpwrstate.state = cpwm;
			RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_err_,
				 ("rpwm_workitem_callback: RPWM=0x%x cpwm=0x%x/0x%x(HW), call cpwm_hdl!\n",
				  pwrpriv->rpwm, pwrpriv->cpwm, cpwm));
			cpwm_hdl(padapter);
		}
		else if (pwrpriv->cpwm != pwrpriv->rpwm)
#endif
		{
			pwrpriv->rpwm_retry = 1;
			set_rpwm(padapter, pwrpriv->rpwm);
		}
	}
	else {
		RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_err_,
			 ("rpwm_workitem_callback: RPWM(0x%x)==[cpwm=0x%x]!\n",
			  pwrpriv->rpwm, pwrpriv->cpwm));
	}

	_exit_pwrlock(&pwrpriv->lock);

_func_exit_;
}

void rpwm_check_handler (void *FunctionContext)
{
	PADAPTER adapter = (PADAPTER)FunctionContext;
	_rpwm_check_handler(adapter);
}

#endif /* #ifdef PLATFORM_LINUX */

#endif /* #ifdef CONFIG_PWRCTRL */


void init_pwrctrl_priv(PADAPTER padapter)
{
	struct pwrctrl_priv *pwrctrlpriv = &padapter->pwrctrlpriv;

_func_enter_;

	_memset(pwrctrlpriv, 0, sizeof(struct pwrctrl_priv));

#ifdef PLATFORM_WINDOWS
	_spinlock_init(&(pwrctrlpriv->pnp_pwr_mgnt_lock));
	pwrctrlpriv->pnp_wwirp_pending=_FALSE;

	pwrctrlpriv->pnp_current_pwr_state=NdisDeviceStateD0;

	#ifdef CONFIG_USB_HCI
	NdisInitializeEvent( &(pwrctrlpriv->pnp_wwirp_complete_evt));
	pwrctrlpriv->pnp_wwirp=NULL;
	#endif

#endif

	_init_pwrlock(&pwrctrlpriv->lock);

	pwrctrlpriv->cpwm = PS_STATE_S4;

	pwrctrlpriv->pwr_mode = PS_MODE_ACTIVE;

	pwrctrlpriv->smart_ps = 0;

	pwrctrlpriv->tog = 0x80;

	pwrctrlpriv->force_active = _FALSE;

#ifdef CONFIG_PWRCTRL

// clear RPWM to ensure driver and fw back to initial state.
#ifdef CONFIG_SDIO_HCI
	write8(padapter, SDIO_HRPWM, 0);
#else
	write8(padapter, 0x1025FE58, 0);
#endif

	_init_workitem(&(pwrctrlpriv->SetPSModeWorkItem), SetPSModeWorkItemCallback, padapter);
	_init_workitem(&(pwrctrlpriv->rpwm_workitem), rpwm_workitem_callback, padapter);
#ifdef PLATFORM_WINDOWS
	_init_timer(&(pwrctrlpriv->rpwm_check_timer), padapter->hndis_adapter, rpwm_check_handler, (u8 *)padapter);
#endif
#ifdef PLATFORM_LINUX
	_init_timer(&(pwrctrlpriv->rpwm_check_timer), padapter->pnetdev, rpwm_check_handler, (u8 *)padapter);

#endif
#endif

_func_exit_;

}


void free_pwrctrl_priv(PADAPTER adapter)
{
	struct pwrctrl_priv *pwrctrlpriv = &adapter->pwrctrlpriv;

_func_enter_;

#ifdef CONFIG_PWRCTRL
	_cancel_timer_ex(&(pwrctrlpriv->rpwm_check_timer));
#endif

	_free_pwrlock(&pwrctrlpriv->lock);

#ifdef PLATFORM_WINDOWS
	_spinlock_free(&pwrctrlpriv->pnp_pwr_mgnt_lock);
#endif

//	_memset(pwrctrlpriv, 0, sizeof(struct pwrctrl_priv));

_func_exit_;
}


/*
Caller: xmit_thread

Check if the fw_pwrstate is okay for xmit.
If not (cpwm is less than P1 state), then the sub-routine
will raise the cpwm to be greater than or equal to P1. 

Calling Context: Passive

Return Value:

_SUCCESS: xmit_thread can write fifo/txcmd afterwards.
_FAIL: xmit_thread can not do anything.
*/
sint register_tx_alive(PADAPTER padapter)
{
	uint res = _SUCCESS;
	
#ifdef CONFIG_PWRCTRL

	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

_func_enter_;

	_enter_pwrlock(&pwrctrl->lock);

	register_task_alive(pwrctrl, XMIT_ALIVE);

	if ((pwrctrl->cpwm < PS_STATE_S3) && (pwrctrl->rpwm < PS_STATE_S3)) {
		RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_notice_,
			 ("register_tx_alive: cpwm:0x%02x alives:0x%08x\n",
			  pwrctrl->cpwm, pwrctrl->alives));
		set_rpwm(padapter, PS_STATE_S3);
		res = _FAIL;
	}

	_exit_pwrlock(&pwrctrl->lock);
	
_func_exit_;

#endif

	return res;
}

/*
Caller: cmd_thread

Check if the fw_pwrstate is okay for issuing cmd.
If not (cpwm should be is less than P2 state), then the sub-routine
will raise the cpwm to be greater than or equal to P2. 

Calling Context: Passive

Return Value:

_SUCCESS: cmd_thread can issue cmds to firmware afterwards.
_FAIL: cmd_thread can not do anything.
*/
sint register_cmd_alive(PADAPTER padapter)
{
	uint res = _SUCCESS;
	
#ifdef CONFIG_PWRCTRL

	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

_func_enter_;

	_enter_pwrlock(&pwrctrl->lock);

	register_task_alive(pwrctrl, CMD_ALIVE);

	if ((pwrctrl->cpwm < PS_STATE_S2) && (pwrctrl->rpwm < PS_STATE_S2)) {
		RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_notice_,
			 ("register_cmd_alive: cpwm=0x%02x alives=0x%08x\n",
			  pwrctrl->cpwm, pwrctrl->alives));
		set_rpwm(padapter, PS_STATE_S3);
		res = _FAIL;
	}

	_exit_pwrlock(&pwrctrl->lock);

_func_exit_;

#endif

	return res;
}


/*
Caller: rx_isr

Calling Context: Dispatch/ISR

Return Value:

*/
sint register_rx_alive(PADAPTER padapter)
{

#ifdef CONFIG_PWRCTRL

	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

_func_enter_;

	_enter_pwrlock(&pwrctrl->lock);

	register_task_alive(pwrctrl, RECV_ALIVE);

	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("register_rx_alive: cpwm:%d alives:%x\n", pwrctrl->cpwm, pwrctrl->alives));

	_exit_pwrlock(&pwrctrl->lock);

_func_exit_;
	
#endif

	return _SUCCESS;
}


/*
Caller: evt_isr or evt_thread

Calling Context: Dispatch/ISR or Passive

Return Value:
*/
sint register_evt_alive(PADAPTER padapter)
{

#ifdef CONFIG_PWRCTRL

	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

_func_enter_;

	_enter_pwrlock(&pwrctrl->lock);

	register_task_alive(pwrctrl, EVT_ALIVE);

	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_info_,("register_evt_alive: cpwm:%d alives:%x\n", pwrctrl->cpwm, pwrctrl->alives));

	_exit_pwrlock(&pwrctrl->lock);

_func_exit_;

#endif

	return _SUCCESS;
}


/*
Caller: ISR

If ISR's txdone,
No more pkts for TX,
Then driver shall call this fun. to power down firmware again.
*/

void unregister_tx_alive(PADAPTER padapter)
{
#ifdef CONFIG_PWRCTRL

	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

_func_enter_;

	_enter_pwrlock(&pwrctrl->lock);

	unregister_task_alive(pwrctrl, XMIT_ALIVE);

	if (pwrctrl->pwr_mode > PS_MODE_ACTIVE) {
		if ((pwrctrl->alives == 0) && (pwrctrl->cpwm >= PS_STATE_S2) &&
		    (pwrctrl->rpwm >= PS_STATE_S2)) {
			RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_notice_,
				 ("unregister_tx_alive: cpwm:0x%02x alives:0x%08x\n",
				  pwrctrl->cpwm, pwrctrl->alives));
			set_rpwm(padapter, PS_STATE_S0);
		}
	}
	
	_exit_pwrlock(&pwrctrl->lock);

_func_exit_;

#endif
}

/*
Caller: ISR

If ISR's txdone,
No more pkts for TX,
Then driver shall call this fun. to power down firmware again.
*/

void unregister_cmd_alive(PADAPTER padapter)
{
#ifdef CONFIG_PWRCTRL

	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

_func_enter_;

	_enter_pwrlock(&pwrctrl->lock);

	unregister_task_alive(pwrctrl, CMD_ALIVE);

	if (pwrctrl->pwr_mode > PS_MODE_ACTIVE) {
		if ((check_fwstate(&padapter->mlmepriv, _FW_UNDER_LINKING) == _FALSE) &&
		    (pwrctrl->alives == 0) && (pwrctrl->cpwm >= PS_STATE_S2) &&
		    (pwrctrl->rpwm >= PS_STATE_S2)) {
			RT_TRACE(_module_rtl871x_pwrctrl_c_, _drv_notice_,
				 ("unregister_cmd_alive: cpwm=0x%02x alives=0x%08x\n",
				  pwrctrl->cpwm, pwrctrl->alives));
			set_rpwm(padapter, PS_STATE_S0);
		}
	}

	_exit_pwrlock(&pwrctrl->lock);

_func_exit_;

#endif
}

/*

Caller: ISR

*/
void unregister_rx_alive(PADAPTER padapter)
{
#ifdef CONFIG_PWRCTRL

	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

_func_enter_;

	_enter_pwrlock(&pwrctrl->lock);

	unregister_task_alive(pwrctrl, RECV_ALIVE);

	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("unregister_rx_alive: cpwm:%d alives:%x\n", pwrctrl->cpwm, pwrctrl->alives));
	
	_exit_pwrlock(&pwrctrl->lock);

_func_exit_;

#endif
}


void unregister_evt_alive(PADAPTER padapter)
{
#ifdef CONFIG_PWRCTRL

	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;

_func_enter_;

	_enter_pwrlock(&pwrctrl->lock);

	unregister_task_alive(pwrctrl, EVT_ALIVE);

	RT_TRACE(_module_rtl871x_pwrctrl_c_,_drv_err_,("unregister_evt_alive: cpwm:%d alives:%x\n", pwrctrl->cpwm, pwrctrl->alives));
	
	_exit_pwrlock(&pwrctrl->lock);

_func_exit_;

#endif
}

/*
 * Don't do any IO and anything would be blocked,
 * because this function will be called in timer (software interrupt),
 * and no need to use pwrctrl->lock to protect.
 */
void wdg_pwrctrl(PADAPTER padapter)
{
#ifdef CONFIG_PWRCTRL
	struct pwrctrl_priv *pwrctrl = &padapter->pwrctrlpriv;


	if (pwrctrl->pwr_mode == PS_MODE_ACTIVE) 
		return;

	if (padapter->mlmepriv.sitesurveyctrl.traffic_busy == _TRUE)
		register_task_alive(pwrctrl, BUSY_ALIVE);
	else
		unregister_task_alive(pwrctrl, BUSY_ALIVE);
#endif
}


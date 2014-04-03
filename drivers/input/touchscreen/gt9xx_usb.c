/* drivers/input/touchscreen/gt9xx.c
 * 
 * 2010 - 2013 Goodix Technology.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be a reference 
 * to you, when you are integrating the GOODiX's CTP IC into your system, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details.
 * 
 * Version: 0.9
 * Release Date: 2014/03/14
 * Revision record:
 *      V0.9:   
 *          first Release. By Scott, 2014/03/14
 */

#include <linux/irq.h>
#include "gt9xx_usb.h"

#if GTP_ICS_SLOT_REPORT
    #include <linux/input/mt.h>
#endif

static const char *goodix_ts_name = "goodix-ts";
static u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
                = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};

#if GTP_HAVE_TOUCH_KEY
    static const u16 touch_key_array[] = GTP_KEY_TAB;
    #define GTP_MAX_KEY_NUM  (sizeof(touch_key_array)/sizeof(touch_key_array[0]))
    
#if 1 //GTP_DEBUG_ON
    static const int  key_codes[] = {KEY_HOME, KEY_BACK, KEY_MENU, KEY_SEARCH};
    static const char *key_names[] = {"Key_Home", "Key_Back", "Key_Menu", "Key_Search"};
#endif
    
#endif

s32 gtp_send_cfg(struct goodix_ts_data *ts);

//static struct usb_driver goodix_ts_driver;

#ifdef CONFIG_HAS_EARLYSUSPEND
static int goodix_ts_early_suspend
        (struct usb_interface *intf, pm_message_t message);
    static int goodix_ts_late_resume(struct usb_interface *intf);
#endif
 
u8 grp_cfg_version = 0;


#define READ_COORD          0x01
#define READ_DIFF           0x06
#define READ_VERSION        0x08
#define READ_CONFIG         0x0A
#define ADDRESS_DIRECT      0x20
#define READ_SENSORID       0x30
#define READ_CONFIG_VER_AND_RESOLUTION 0x31
#define READ_FW_CHKSUM      0x32

s32 gtp_usb_write(struct goodix_ts_data *ts, u8* data, u8 cmd, s32 len)
{
    struct usb_device *dev = interface_to_usbdev(ts->interface);
    s32 actual_len = 0;
    s32 send_len = 0;
    s32 ret = -1;
    s32 i = 0;
    s32 pkgs = 0;
    u8 buffer[256];
    
    GTP_DEBUG_FUNC();

    if (!ts->out_ep)
    {
        return ret;
    }

    GTP_DEBUG("send length:%d", len);

    pkgs = len / 61;

    buffer[0] = cmd;
    do{
        if ((pkgs - i) > 0)
        {
            buffer[1] = 1;
            buffer[2] = i;
            buffer[3] = 60;
        }
        else
        {
            buffer[1] = 0;
            buffer[2] = i;
            buffer[3] = len - i * 60 ;
        }

        memcpy(&buffer[4], &data[i * 60], buffer[3]);

        GTP_DEBUG_ARRAY(buffer, buffer[3] + 4);

        ret = usb_bulk_msg(dev, usb_sndbulkpipe(dev, ts->out_ep->bEndpointAddress),
                           buffer, buffer[3] + 4, &actual_len, 5000);
        if (ret < 0)
        {
            GTP_ERROR("Send cmd error.error code:%d", ret);
            return ret;
        }
        send_len += actual_len;

        GTP_DEBUG("send msg in bulk way, send bytes:%d", actual_len);
        msleep(10);

        i++;
    }while((len - i * 60) > 0);

    return send_len;
}

s32 gtp_usb_just_write(struct goodix_ts_data *ts, u8* data, s32 len)
{
    struct usb_device *dev = interface_to_usbdev(ts->interface);
    s32 actual_len = 0;
    s32 ret = -1;
    
    GTP_DEBUG_FUNC();

    if (!ts->out_ep)
    {
        return ret;
    }

    ret = usb_bulk_msg(dev, usb_sndbulkpipe(dev, ts->out_ep->bEndpointAddress),
                       data, len, &actual_len, 5000);
    if (ret < 0)
    {
        GTP_ERROR("Send cmd error.error code:%d", ret);
        return ret;
    }
    GTP_DEBUG("send msg in bulk way, send bytes:%d", actual_len);
    GTP_DEBUG("CMD HEAD:%x",data[0]);
    msleep(10);

    return actual_len;
}

s32 gtp_usb_just_read(struct goodix_ts_data *ts, u8* buf, s32 len)
{
    struct usb_device *dev = interface_to_usbdev(ts->interface);
    s32 ret = -1;
    s32 actual_len = 0;
    
    GTP_DEBUG_FUNC();

	ret = usb_bulk_msg(dev, usb_rcvbulkpipe(dev, ts->in_ep->bEndpointAddress),
            ts->buffer, len, &actual_len, 0);

    GTP_DEBUG("just read msg, return:%d, recieve bytes:%d", 
          ret, actual_len);

    GTP_DEBUG_ARRAY(ts->buffer, actual_len);

    return 0;
}

/*******************************************************
Function:
    Send config.
Input:
    client: i2c device.
Output:
    result of i2c write operation. 
        1: succeed, otherwise: failed
*********************************************************/
s32 gtp_send_cfg(struct goodix_ts_data *ts)
{
#define SEND_CONFIG         0x09

    s32 ret = 2;

    GTP_DEBUG_FUNC();
#if GTP_DRIVER_SEND_CFG

    if (ts->pnl_init_error)
    {
        GTP_INFO("Error occured in init_panel, no config sent");
        return 0;
    }

    GTP_INFO("Driver send config.");
    config[0] = GTP_REG_CONFIG_DATA >> 8;
    config[1] = GTP_REG_CONFIG_DATA & 0xFF;
    config[2] = ts->gtp_cfg_len >> 8;
    config[3] = ts->gtp_cfg_len & 0xFF;

    GTP_DEBUG_ARRAY(config, ts->gtp_cfg_len + GTP_ADDR_LENGTH);
    ret = gtp_usb_write(ts, config, SEND_CONFIG, ts->gtp_cfg_len + GTP_ADDR_LENGTH);
#endif
    return ret;
}

/*******************************************************
Function:
    Report touch point event 
Input:
    ts: goodix i2c_client private data
    id: trackId
    x:  input x coordinate
    y:  input y coordinate
    w:  input pressure
Output:
    None.
*********************************************************/
static void gtp_touch_down(struct goodix_ts_data* ts,s32 id,s32 x,s32 y,s32 w)
{
#if GTP_CHANGE_X2Y
    GTP_SWAP(x, y);
#endif

#if GTP_ICS_SLOT_REPORT
    input_mt_slot(ts->input_dev, id);
    input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
    input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
    input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
#else
    input_report_key(ts->input_dev, BTN_TOUCH, 1);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
    input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
    input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
    input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
    input_mt_sync(ts->input_dev);
#endif

    GTP_DEBUG("ID:%d, X:%d, Y:%d, W:%d", id, x, y, w);
}

/*******************************************************
Function:
    Report touch release event
Input:
    ts: goodix i2c_client private data
Output:
    None.
*********************************************************/
static void gtp_touch_up(struct goodix_ts_data* ts, s32 id)
{
#if GTP_ICS_SLOT_REPORT
    input_mt_slot(ts->input_dev, id);
    input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
    GTP_DEBUG("Touch id[%2d] release!", id);
#else
    input_report_key(ts->input_dev, BTN_TOUCH, 0);
#endif
}


/*******************************************************
Function:
    Goodix touchscreen work function
Input:
    work: work struct of goodix_workqueue
Output:
    None.
*********************************************************/
static void goodix_ts_work_func(struct goodix_ts_data *ts)
{
    static u8  point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1]={GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF};
    static u8 data_left = 0;
    static u8 pkg_id = 0xff;
    u8  touch_num = 0;
    u8  finger = 0;
    static u16 pre_touch = 0;
    static u8 pre_key = 0;
    u8  key_value = 0;
    u8* coor_data = NULL;
    s32 input_x = 0;
    s32 input_y = 0;
    s32 input_w = 0;
    s32 id = 0;
    s32 i  = 0;

    GTP_DEBUG_FUNC();
    
    if (ts->data[1] & 0x01)
    {
        memcpy(point_data, &ts->data[4], 60);
        pkg_id = ts->data[2];
        data_left = 1;

        return;
    }
    else
    {
        if (data_left == 1)
        {
            if ((++pkg_id) == ts->data[2])
            {
                memcpy(&point_data[60], &ts->data[4], 60);
                data_left = 0;
                pkg_id = 0xff;
            }
            else
                return;
        }
        else
            memcpy(point_data, &ts->data[4], 60);
    }

    finger = point_data[0];
    GTP_DEBUG("finger msg:%x", finger);

    if (finger == 0x00)
    {
        return;
    }

    if((finger & 0x80) == 0)
    {
        return;
    }

    touch_num = finger & 0x0f;
    if (touch_num > GTP_MAX_TOUCH)
    {
        return;
    }

    GTP_DEBUG_ARRAY(&point_data[1], touch_num * 8);

#if (GTP_HAVE_TOUCH_KEY)
    key_value = point_data[1 + 8 * touch_num];
    GTP_DEBUG("KEY VALUE:%x", key_value);
    if(key_value || pre_key)
    {
    #if GTP_HAVE_TOUCH_KEY
        if (!pre_touch)
        {
            int j = 0;
            for (i = 0; i < GTP_MAX_KEY_NUM; i++)
            {
            #if GTP_DEBUG_ON
                for (j = 0; j < 4; ++j)
                {
                    if (key_codes[j] == touch_key_array[i])
                    {
                        GTP_DEBUG("Key: %s %s", key_names[j], (key_value & (0x01 << i)) ? "Down" : "Up");
                        break;
                    }
                }
            #endif
                input_report_key(ts->input_dev, touch_key_array[i], key_value & (0x01<<i));   
            }
            touch_num = 0;  // shield fingers
        }
    #endif
    }
#endif
    pre_key = key_value;

    GTP_DEBUG("pre_touch:%02x, finger:%02x.", pre_touch, finger);

#if GTP_ICS_SLOT_REPORT

    if (pre_touch || touch_num)
    {
        s32 pos = 0;
        u16 touch_index = 0;
        u8 report_num = 0;
        coor_data = &point_data[1];
        
        if(touch_num)
        {
            id = coor_data[pos] & 0x0F;
            touch_index |= (0x01<<id);
        }
        
        GTP_DEBUG("id = %d,touch_index = 0x%x, pre_touch = 0x%x\n",id, touch_index,pre_touch);
        for (i = 0; i < GTP_MAX_TOUCH; i++)
        {
            if ((touch_index & (0x01<<i)))
            {
                input_x  = coor_data[pos + 1] | (coor_data[pos + 2] << 8);
                input_y  = coor_data[pos + 3] | (coor_data[pos + 4] << 8);
                input_w  = coor_data[pos + 5] | (coor_data[pos + 6] << 8);

                gtp_touch_down(ts, id, input_x, input_y, input_w);
                pre_touch |= 0x01 << i;
                
                report_num++;
                if (report_num < touch_num)
                {
                    pos += 8;
                    id = coor_data[pos] & 0x0F;
                    touch_index |= (0x01<<id);
                }
            }
            else
            {
                gtp_touch_up(ts, i);
                pre_touch &= ~(0x01 << i);
            }
        }
    }
#else

    if (touch_num)
    {
        for (i = 0; i < touch_num; i++)
        {
            coor_data = &point_data[i * 8 + 1];

            id = coor_data[0] & 0x0F;
            input_x  = coor_data[1] | (coor_data[2] << 8);
            input_y  = coor_data[3] | (coor_data[4] << 8);
            input_w  = coor_data[5] | (coor_data[6] << 8);

            gtp_touch_down(ts, id, input_x, input_y, input_w);
        }
    }
    else if (pre_touch)
    {
        GTP_DEBUG("Touch Release!");
        gtp_touch_up(ts, 0);
    }

    pre_touch = touch_num;
#endif

    input_sync(ts->input_dev);
}

/*******************************************************
Function:
    Initialize gtp.
Input:
    ts: goodix private data
Output:
    Executive outcomes.
        0: succeed, otherwise: failed
*******************************************************/
static s32 gtp_init_panel(struct goodix_ts_data *ts)
{
    s32 ret = -1;

#if GTP_DRIVER_SEND_CFG
    s32 i = 0;
    u8 check_sum = 0;
    u8 sensor_id = 0; 
    
    u8 cfg_info_group1[] = CTP_CFG_GROUP1;
    u8 cfg_info_group2[] = CTP_CFG_GROUP2;
    u8 cfg_info_group3[] = CTP_CFG_GROUP3;
    u8 cfg_info_group4[] = CTP_CFG_GROUP4;
    u8 cfg_info_group5[] = CTP_CFG_GROUP5;
    u8 cfg_info_group6[] = CTP_CFG_GROUP6;
    u8 *send_cfg_buf[] = {cfg_info_group1, cfg_info_group2, cfg_info_group3,
                        cfg_info_group4, cfg_info_group5, cfg_info_group6};
    u8 cfg_info_len[] = { CFG_GROUP_LEN(cfg_info_group1),
                          CFG_GROUP_LEN(cfg_info_group2),
                          CFG_GROUP_LEN(cfg_info_group3),
                          CFG_GROUP_LEN(cfg_info_group4),
                          CFG_GROUP_LEN(cfg_info_group5),
                          CFG_GROUP_LEN(cfg_info_group6)};

    GTP_DEBUG_FUNC();
    GTP_DEBUG("Config Groups\' Lengths: %d, %d, %d, %d, %d, %d", 
        cfg_info_len[0], cfg_info_len[1], cfg_info_len[2], cfg_info_len[3],
        cfg_info_len[4], cfg_info_len[5]);

    if ((!cfg_info_len[1]) && (!cfg_info_len[2]) && 
        (!cfg_info_len[3]) && (!cfg_info_len[4]) && 
        (!cfg_info_len[5]))
    {
        sensor_id = 0; 
    }
    else
    {
        sensor_id = 0;
        GTP_INFO("Sensor_ID: %d", sensor_id);
    }
    ts->gtp_cfg_len = cfg_info_len[sensor_id];
    GTP_INFO("CTP_CONFIG_GROUP%d used, config length: %d", sensor_id + 1, ts->gtp_cfg_len);
    
    if (ts->gtp_cfg_len < GTP_CONFIG_MIN_LENGTH)
    {
        GTP_ERROR("Config Group%d is INVALID CONFIG GROUP(Len: %d)! NO Config Sent! You need to check you header file CFG_GROUP section!", sensor_id+1, ts->gtp_cfg_len);
        ts->pnl_init_error = 1;
        return -1;
    }

    memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
    memcpy(&config[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id], ts->gtp_cfg_len);

#if GTP_CUSTOM_CFG
    config[RESOLUTION_LOC]     = (u8)GTP_MAX_WIDTH;
    config[RESOLUTION_LOC + 1] = (u8)(GTP_MAX_WIDTH>>8);
    config[RESOLUTION_LOC + 2] = (u8)GTP_MAX_HEIGHT;
    config[RESOLUTION_LOC + 3] = (u8)(GTP_MAX_HEIGHT>>8);
    
#endif  // GTP_CUSTOM_CFG
    
    check_sum = 0;
    for (i = GTP_ADDR_LENGTH; i < (ts->gtp_cfg_len + GTP_ADDR_LENGTH - 2); i++)
    {
        check_sum += config[i];
    }
    config[ts->gtp_cfg_len + GTP_ADDR_LENGTH - 2] = (~check_sum) + 1;

#else // driver not send config
#if 0
    ts->gtp_cfg_len = GTP_CONFIG_MAX_LENGTH;
    ret = gtp_i2c_read(ts->client, config, ts->gtp_cfg_len + GTP_ADDR_LENGTH);
    if (ret < 0)
    {
        GTP_ERROR("Read Config Failed, Using Default Resolution & INT Trigger!");
        ts->abs_x_max = GTP_MAX_WIDTH;
        ts->abs_y_max = GTP_MAX_HEIGHT;
    }
#endif    
#endif // GTP_DRIVER_SEND_CFG

    if ((ts->abs_x_max == 0) && (ts->abs_y_max == 0))
    {
        ts->abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
        ts->abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
    }

    {
    #if GTP_DRIVER_SEND_CFG
        ret = gtp_send_cfg(ts);
        if (ret < 0)
        {
            GTP_ERROR("Send config error.");
        }
    #endif
        GTP_INFO("X_MAX: %d, Y_MAX: %d, TRIGGER: 0x%02x", ts->abs_x_max,ts->abs_y_max,ts->int_trigger_type);
    }
    
    msleep(10);
    return 0;
}

/*******************************************************
Function:
    Request input device Function.
Input:
    ts:private data.
Output:
    Executive outcomes.
        0: succeed, otherwise: failed.
*******************************************************/
static s8 gtp_request_input_dev(struct goodix_ts_data *ts)
{
    s8 ret = -1;
    s8 phys[32];
#if GTP_HAVE_TOUCH_KEY
    u8 index = 0;
#endif
  
    GTP_DEBUG_FUNC();
  
    ts->input_dev = input_allocate_device();
    if (ts->input_dev == NULL)
    {
        GTP_ERROR("Failed to allocate input device.");
        return -ENOMEM;
    }

    ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
#if GTP_ICS_SLOT_REPORT
    input_mt_init_slots(ts->input_dev, 16);     // in case of "out of memory"
#else
    ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif
    __set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

#if GTP_HAVE_TOUCH_KEY
    for (index = 0; index < GTP_MAX_KEY_NUM; index++)
    {
        input_set_capability(ts->input_dev, EV_KEY, touch_key_array[index]);  
    }
#endif

#if GTP_CHANGE_X2Y
    GTP_SWAP(ts->abs_x_max, ts->abs_y_max);
#endif

    GTP_DEBUG("ts->abs_x_max:%d", ts->abs_x_max);
    GTP_DEBUG("ts->abs_y_max:%d", ts->abs_y_max);

    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, 3800, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, 2850, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

    sprintf(phys, "input/ts");
    ts->input_dev->name = goodix_ts_name;
    ts->input_dev->phys = phys;
    ts->input_dev->id.bustype = BUS_USB;
    ts->input_dev->id.vendor = 0xDEAD;
    ts->input_dev->id.product = 0xBEEF;
    ts->input_dev->id.version = 10427;
    
    ret = input_register_device(ts->input_dev);
    if (ret)
    {
        GTP_ERROR("Register %s input device failed", ts->input_dev->name);
        return -ENODEV;
    }
    
#ifdef CONFIG_HAS_EARLYSUSPEND
    ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ts->early_suspend.suspend = goodix_ts_early_suspend;
    ts->early_suspend.resume = goodix_ts_late_resume;
    register_early_suspend(&ts->early_suspend);
#endif

    return 0;
}

static void goodix_ts_irq_handler(struct urb *urb)
{
    struct goodix_ts_data *ts = urb->context;
    s32 retval = -1;
    
    GTP_DEBUG_FUNC();

    GTP_DEBUG("urb status:%d", urb->status);
    GTP_DEBUG("data type:%x", ts->data[0]);
    GTP_DEBUG("data length:%d",  urb->actual_length);

    GTP_DEBUG_ARRAY(ts->data, urb->actual_length);
    
    if (urb->status == 0)
    {
        //success
        switch (ts->data[0])
        {
            case READ_COORD:
            goodix_ts_work_func(ts);
            //process the coordinates.
            break;

            case READ_FW_CHKSUM:
           // gtp_set_chksum(ts);
            break;

            case READ_VERSION:
          //  gtp_set_version(ts);
            //process the version
            break;

            case READ_CONFIG:
          //  gtp_set_config(ts);

            //process the config
            break;
            
            case ADDRESS_DIRECT:
            ts->sensor_id = ts->data[0];
            break;
        }
        
    }
    else if (urb->status == -ETIME || urb->status == -ECONNRESET ||
             urb->status == -ENOENT || urb->status == -ESHUTDOWN ||
             urb->status == -EPIPE)
    {
        GTP_ERROR("URB status error.error code:%d", urb->status);

       return;
    }

    usb_mark_last_busy(interface_to_usbdev(ts->interface));
    retval = usb_submit_urb(urb, GFP_ATOMIC);
    if (retval)
        GTP_ERROR("usb_submit_urb failed with result: %d", retval);
}

#define RECEPT_SIZE 64
static s32 gtp_init_usb_para(struct goodix_ts_data *ts)
{
	struct usb_device *udev = interface_to_usbdev(ts->interface);
    s32 ret = -1;

    GTP_DEBUG_FUNC();

	ts->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!ts->irq)
	{
        GTP_ERROR("ts->irq memory error.");
        return -1;
    }

    if (usb_endpoint_type(ts->in_ep) == USB_ENDPOINT_XFER_INT)
        usb_fill_int_urb(ts->irq, udev,
                usb_rcvintpipe(udev, ts->in_ep->bEndpointAddress),
                ts->data, RECEPT_SIZE/**/, goodix_ts_irq_handler, ts, 
                ts->in_ep->bInterval);
    else
        usb_fill_bulk_urb(ts->irq, udev,
                usb_rcvbulkpipe(udev, ts->in_ep->bEndpointAddress),
                ts->data, RECEPT_SIZE, goodix_ts_irq_handler, ts);

	ts->irq->dev = udev;
	ts->irq->transfer_dma = ts->data_dma;
	ts->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	usb_set_intfdata(ts->interface, ts);

    ret = usb_submit_urb(ts->irq, GFP_KERNEL);
    if (ret)
    {
        GTP_ERROR("usb_submit_urb[%s] error.", __func__);
        goto free_buffer_data;
    }/**/
	return 0;

free_buffer_data:
	kfree(ts->buffer);
	
    return -1;
}

static s32 gtp_init_usb_endpoint(struct goodix_ts_data *ts)
{
	struct usb_host_interface *interface = ts->interface->cur_altsetting;
    s32 i;

    GTP_DEBUG_FUNC();

	for (i = 0; i < interface->desc.bNumEndpoints; i++)
	{
        if (!ts->in_ep&&
    	    usb_endpoint_dir_in(&interface->endpoint[i].desc))
            ts->in_ep = &interface->endpoint[i].desc;

        if (!ts->out_ep&&
            usb_endpoint_dir_out(&interface->endpoint[i].desc))
            ts->out_ep = &interface->endpoint[i].desc;/**/
	}

    GTP_DEBUG("input endpoint address:%x", ts->in_ep->bEndpointAddress);
    GTP_DEBUG("input pipe:%x", ts->in_ep->bEndpointAddress);
    if (ts->out_ep)
    {
        GTP_DEBUG("output endpoint address:%x", ts->out_ep->bEndpointAddress);
        GTP_DEBUG("input pipe:%x", ts->in_ep->bEndpointAddress);
    }
    
	if (!ts->in_ep || !ts->out_ep)
	{
        GTP_ERROR("init pipe error.");
        return -ENXIO;
    }

	return 0;
}

s32 gtp_switch_gp(struct goodix_ts_data *ts)
{
    s32 ret = -1;
    u8 buf[64];
    u8 gt91xx_read_coord[] = {READ_COORD, 0, 0, 0x0a, 1, 0x81, 0x4e, 0, 0x52, 0x20, 0x80, 0x40, 1, 0};

    GTP_DEBUG_FUNC();

    ret = gtp_usb_just_write(ts, gt91xx_read_coord, 32);
    gtp_usb_just_read(ts, buf, 64);
    
    return ret;
}

s32 gtp_read_version(struct goodix_ts_data *ts, u16* version)
{
    s32 ret = -1;
    u8 buf[64];
    u8 pid[5];
    u8 gt91xx_read_version[] = {0x20, 0, 0, 9, 2, 0x81, 0x40, 0, 6, 0x90, 0xff, 0xff, 0};

    GTP_DEBUG_FUNC();

    gtp_usb_just_write(ts, gt91xx_read_version, 32);
    ret = gtp_usb_just_read(ts, buf, 6);

    memcpy(pid, &buf[4], 4);
    pid[4] = 0;
    *version = buf[9] << 8 | buf[8];

    GTP_DEBUG("PID: %s\tVID:%x", pid, *version);

    return ret;
}

s32 gtp_request_mem(struct goodix_ts_data *ts)
{
	struct usb_device *udev = interface_to_usbdev(ts->interface);
    GTP_DEBUG_FUNC();

	ts->data = usb_alloc_coherent(udev, RECEPT_SIZE,
	                              GFP_KERNEL, &ts->data_dma);
	if (!ts->data)
	{
	    GTP_ERROR("ts->data memory error.");
        return -ENOMEM;
    }

    ts->buffer = kmalloc(RECEPT_SIZE, GFP_KERNEL);
    if (!ts->buffer)
    {
        GTP_ERROR("ts->buffer memory error.");
        usb_free_coherent(udev, RECEPT_SIZE, ts->data, ts->data_dma);
        return -ENOMEM;
    }

    return 0;
}

/*******************************************************
Function:
    USB probe.
Input:
    intf: USB interface.
    id: device id.
Output:
    Executive outcomes. 
        0: succeed.
*******************************************************/
static int goodix_ts_probe(struct usb_interface *intf,
                           const struct usb_device_id *id)
{
    s32 ret = -1;
    struct goodix_ts_data *ts;
    
    GTP_DEBUG_FUNC();
    
    //do NOT remove these logs
    GTP_INFO("GTP Driver Version: %s", GTP_DRIVER_VERSION);
    GTP_INFO("GTP Driver Built@%s, %s", __TIME__, __DATE__);

	/* some devices are ignored */
	if (id->driver_info != 0x5D)
		return -ENODEV;
    
    ts = kzalloc(sizeof(*ts), GFP_KERNEL);
    if (ts == NULL)
    {
        GTP_ERROR("Alloc GFP_KERNEL memory failed.");
        return -ENOMEM;
    }
    
    memset(ts, 0, sizeof(*ts));
    ts->interface = intf;
	ts->usbdev = usb_get_dev(interface_to_usbdev(intf));

    ret = gtp_init_usb_endpoint(ts);
    if (ret < 0)
    {
        kfree(ts);
        return -ENXIO;
    }
    else
    {
        int size = 0;

        size = le16_to_cpu(ts->in_ep->wMaxPacketSize);

        GTP_DEBUG("MAX SIZE:%d", size);
    }

    ret = gtp_request_mem(ts);
    if (ret < 0)
    {
        kfree(ts);
        return ret;
    }
    
    ret = gtp_switch_gp(ts);
    if (ret < 0)
    {
        kfree(ts);
        return ret;
    }
/*    
    ret = gtp_read_version(ts, &version_info);
    if (ret < 0)
    {
        kfree(ts);
        return ret;
    //      return 0;
    }*/
    
    ret = gtp_init_panel(ts);
    if (ret < 0)
    {
        kfree(ts);
        return ret;
    }

    ret = gtp_request_input_dev(ts);
    if (ret < 0)
    {
        kfree(ts);
        return ret;
    }

    ret = gtp_init_usb_para(ts);
    if (ret < 0)
    {
        kfree(ts);
        return ret;
    }
    
   //  printk("function address:%x\n", goodix_ts_driver.disconnect);
    return 0;
}

s32 gtp_free_mem(struct usb_device *udev, struct goodix_ts_data *ts)
{
	usb_free_coherent(udev, RECEPT_SIZE,
			  ts->data, ts->data_dma);
	kfree(ts->buffer);
	
    return 0;
}

/*******************************************************
Function:
    Goodix touchscreen driver release function.
Input:
    client: i2c device struct.
Output:
    Executive outcomes. 0---succeed.
*******************************************************/
static void goodix_ts_disconnect(struct usb_interface *intf)
{
    struct goodix_ts_data *ts = usb_get_intfdata(intf);

    GTP_DEBUG("usb touch disconnect");
    GTP_DEBUG_FUNC();
    
    if (!ts)
        return;

#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&ts->early_suspend);
#endif
        
    usb_set_intfdata(intf, NULL);
    usb_kill_urb(ts->irq);
    GTP_DEBUG_FUNC();
    /* this will stop IO via close */
    input_unregister_device(ts->input_dev);
    GTP_DEBUG_FUNC();
    usb_free_urb(ts->irq);
    gtp_free_mem(interface_to_usbdev(intf), ts);
    GTP_DEBUG_FUNC();
    kfree(ts);
}


#ifdef CONFIG_HAS_EARLYSUSPEND
/*******************************************************
Function:
    Early suspend function.
Input:
    h: early_suspend struct.
Output:
    None.
*******************************************************/
static int goodix_ts_early_suspend
        (struct usb_interface *intf, pm_message_t message)
{
    GTP_DEBUG_FUNC();
    struct goodix_ts_data *ts = usb_get_intfdata(intf);

    usb_kill_urb(ts->irq);

    return 0;
}


/*******************************************************
Function:
    Late resume function.
Input:
    h: early_suspend struct.
Output:
    None.
*******************************************************/
static int goodix_ts_late_resume(struct usb_interface *intf)
{
    struct goodix_ts_data *ts = usb_get_intfdata(intf);
    struct input_dev *input = ts->input_dev;
    int result = 0;
    
    mutex_lock(&input->mutex);

    result = usb_submit_urb(ts->irq, GFP_NOIO);
    mutex_unlock(&input->mutex);

    return result;
}

#endif

static int goodix_ts_reset_resume(struct usb_interface *intf)
{
    struct goodix_ts_data *ts = usb_get_intfdata(intf);
    struct input_dev *input = ts->input_dev;
	int err = 0;

	/* restart IO if needed */
	mutex_lock(&input->mutex);
	if (input->users)
		err = usb_submit_urb(ts->irq, GFP_NOIO);
	mutex_unlock(&input->mutex);

	return err;
}

static const struct usb_device_id goodix_ts_id[] = {
    {USB_DEVICE(0x27c6, 0x0004), .driver_info = 0x5D},
    { },
};

//NEW
MODULE_DEVICE_TABLE(usb, goodix_ts_id);

struct usb_driver goodix_ts_driver = {
    .name       = "goodix-ts",
    .probe      = goodix_ts_probe,
    .disconnect = goodix_ts_disconnect,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend    = goodix_ts_early_suspend,
    .resume     = goodix_ts_late_resume,
#endif
	.reset_resume   = goodix_ts_reset_resume,

    .id_table   = goodix_ts_id,
};

/*******************************************************    
Function:
    Driver Install function.
Input:
    None.
Output:
    Executive Outcomes. 0---succeed.
********************************************************/
static int __init goodix_ts_init(void)
{
    s32 ret;

    GTP_DEBUG_FUNC();   
    GTP_INFO("GTP driver installing...");

    ret = usb_register(&goodix_ts_driver);
    return ret; 
}


/*******************************************************    
Function:
    Driver uninstall function.
Input:
    None.
Output:
    Executive Outcomes. 0---succeed.
********************************************************/
static void __exit goodix_ts_exit(void)
{
    GTP_DEBUG_FUNC();
    GTP_INFO("GTP driver exited.");
    usb_deregister(&goodix_ts_driver);
}

late_initcall(goodix_ts_init);
module_exit(goodix_ts_exit);

MODULE_DESCRIPTION("GTP USB Driver");
MODULE_LICENSE("GPL");

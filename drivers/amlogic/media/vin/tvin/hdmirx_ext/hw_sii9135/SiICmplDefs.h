/*------------------------------------------------------------------------------
 * Copyright © 2002-2005, Silicon Image, Inc.  All rights reserved.
 *
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of: Silicon Image, Inc.,
 * 1060 East Arques Avenue, Sunnyvale, California 94085
 *----------------------------------------------------------------------------
 */

#ifndef _SII_CMPLDEFS_
#define _SII_CMPLDEFS_

/* Compiling with standard ANSI-C */
#undef _STD_C_
/* Compiling with 8052 microcontrollers (256 bytes RAM) */
/* #define _8051_ */


/* #define SII_DUMP_UART    // Sending of debug data into UART (RS 232) */

#undef  SII_USE_RX_PIN_INTERRUPT
/* Using interrupt pin for detection of interrupts
 * from HDMI RX, otherwise interrupt status bit is used
 */

#define SII_REQ_TASK_CALL_TIME 25
/* Maximum reqomended interval time between calling SiI_RX_DoTasks()
 * Do not modify this parameter without consulting
 */

#undef SII_FIXED_TASK_CALL_TIME
/* Use this define when no system timer cannot be used
 * If FIXED_TASK_CALL_TIME is defined, then TASK_CALL_TIME will be defined, too
 * be sure TASK_CALL_TIME correspond to correct value
 */

#ifdef SII_FIXED_TASK_CALL_TIME
#define SII_TASK_CALL_TIME 22
/*  Actual interval time between calling SiI_RX_DoTasks() */
#endif

#define SII_SYS_TICK_TIME 250
/* System timer has been called with specified  (SYS_TICK_TIME) time (in us) */

#define F_OSC_28_3
#ifdef F_OSC_28_3
/* XCLOCK is used for measureament of Vertical Refreshment Rate */
#define SII_XCLOCK_OSC_SCALED_AND_MUL100 (2810073437u)
/*   ( F_OSC * 100  * 1016 ) / 1024
 * where  F_OSC = 28322000
 */

#define SII_XCLOCK_OSC_SCALED_FOR_CALK_FPIX (28775)
/* ((F_OSC * 1016 )/1000) */


#define SII_XCLOCK_OSC_SCALED2047_AND_MUL100 (2832200000u)
/* F_OSC * 100 where F_OSC = 28322000 */
#define SII_XCLOCK_OSC_SCALED2047_FOR_CALK_FPIX (464027)
/* ( (F_OSC * 16384 )/1000000) to yield units of MHz for pixel clock */

#else

/* XCLOCK is used for measureament of Vertical Refreshment Rate */
#define SII_XCLOCK_OSC_SCALED_AND_MUL100 (2679233671u)
/*   ( F_OSC * 100  * 1016 ) / 1024
 * where  F_OSC = 27003300
 */
#define SII_XCLOCK_OSC_SCALED_FOR_CALK_FPIX (27434)
/* ( (F_OSC * 1016 )/1000) */


#endif

#define SII_PCMODES
/* if defined, then project is compiled with PC Resolution tables */
#define SII_861C_MODES
#define SII_ANALOG_DIG_AUDIO_MAX
/* if defined, analog mux is used for feeding audio in DVI mode */

#undef SII_OUTPUT_VFILTER
/* if defined, Video Filters are used with Analog Video Output */

#undef SII_BUG_PHOEBE_AUTOSW_BUG   /* if define to apply SiI9023/33 fix */
#undef SII_BUG_BETTY_PORT1_BUG
#define SII_NO_RESOLUTION_DETECTION
/* if defined data resolution data are taken from registers */

#define SII_I2C_ADAC_IF

#endif


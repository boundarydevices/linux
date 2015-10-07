
#ifndef IQS5XX_H
#define IQS5XX_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>

// i2c slave device address
#define IQS5xx_ADDR		0x74

// Definitions for the driver
#define IQS_MAJOR		248
#define IQS_MINOR		0
#define IQS_DEV_NUMS		1
#define IQS_NUM_RW_REGS         26
#define DEVICE_NAME             "iqs5xx"

// Definitions of Address-commands implemented on IQS5xx
#define	VERSION_INFO		0x00	// Read
#define	XY_DATA			0x01	// Read
#define	PROX_STATUS		0x02	// Read
#define	TOUCH_STATUS		0x03	// Read
#define	COUNT_VALUES		0x04	// Read
#define	LTA_VALUES		0x05	// Read
#define	ATI_COMP		0x06	// Read / Write
#define	PORT_CONTROL		0x07	// Write
#define	SNAP_STATUS		0x08	// Read

#define	CONTROL_SETTINGS	0x10	// Write
#define	THRESHOLD_SETTINGS	0x11	// Write
#define	ATI_SETTINGS		0x12	// Write
#define	FILTER_SETTINGS		0x13	// Write
#define	TIMING_SETTINGS		0x14	// Write
#define	CHANNEL_SETUP		0x15	// Write
#define	HW_CONFIG_SETTINGS	0x16	// Write
#define	ACTIVE_CHANNELS		0x17	// Write
#define	DB_SETTINGS		0x18	// Write

#define	PM_PROX_STATUS		0x20	// Read
#define	PM_COUNT_VALUES		0x21	// Read
#define	PM_LTA_VALUES		0x22	// Read
#define	PM_ATI_COMP		0x23	// Read / Write
#define	PM_ATI_SETTINGS		0x24	// Write

// BIT DEFINITIONS FOR IQS5xx
// XYInfoByte0
#define	NO_OF_FINGERS0		0x01  	// Indicates how many co-ordinates are available
#define	NO_OF_FINGERS1		0x02  	// Indicates how many co-ordinates are available
#define	NO_OF_FINGERS2		0x04  	// Indicates how many co-ordinates are available
#define	SNAP_OUTPUT		0x08  	// 0 = no snap outputs / 1 = at least one snap output
#define	LP_STATUS		0x10  	// 0 = Charging full-speed  /  1 = Charging in LP duty cycle
#define	NOISE_STATUS		0x20  	// 0 = No noise  /  1 = Noise affected data
#define	MODE_INDICATOR		0x40  	// 0 = Normal Charging  /  1 = ProxMode charging
#define	SHOW_RESET		0x80  	// Indicates reset has occurrd

// Bit definitions - ControlSettings0
#define	EVENT_MODE		0x01  	// 0 = no event mode / 1 = event mode active
#define	TRACKPAD_RESEED		0x02  	// Reseed all the normal mode channels
#define	AUTO_ATI		0x04  	// Perform AutoATI routine (depend on Mode selected)
#define	MODE_SELECT		0x08  	// 0 = Normal Mode  /  1 = ProxMode
#define	PM_RESEED		0x10  	// Reseed the Prox Mode channels

#define	AUTO_MODES		0x40  	// 0 = Normal/PM manual, 1=Auto switch between NM and PM
#define	ACK_RESET		0x80  	// clear the SHOW_RESET flag

// Bit definitions - ControlSettings1
#define	SNAP_EN			0x01  	// 0 = snaps calculated / 1 = not calculated
#define	LOW_POWER		0x02  	// 0 = normal power charging 1 = low power charging
#define	SLEEP_EN		0x04  	// 0 = no sleep added / 1 = permanent sleep time added
#define	REVERSE_EN		0x08  	// 0 = disabled (conventional prox detection) / 1=enabled (prox trips both ways)
#define	DIS_PMPROX_EVENT	0x10  	// 0 = PMProx event enabled / 1 = enabled for EventMode
#define	DIS_SNAP_EVENT		0x20  	// 0 = Snap event enabled / 1 = disabled for EventMode
#define	DIS_TOUCH_EVENT		0x40  	// 0 = Touch event enabled / 1 = disabled for EventMode

// Bit definitions - FilterSettings0
#define	DIS_TOUCH_FILTER	0x01  	// 0 = enabled 1 = disabled
#define	DIS_HOVER_FILTER	0x02  	// 0 = enabled 1 = disabled
#define	SELECT_TOUCH_FILTER	0x04  	// 0 = Dynamic filter 1 = fixed beta
#define	DIS_PM_FILTER		0x08  	// 0 = CS filtered in PM 1 = CS raw in PM
#define	DIS_NM_FILTER		0x10  	// 0 = CS filtered in NM 1 = CS raw in NM

// Bit definitions - PMSetup0
#define	RX_SELECT0		0x01  	// Decimal value selects an INDIVIDUAL Rx for ProxMode
#define	RX_SELECT1		0x02  	//
#define	RX_SELECT2		0x04  	//
#define	RX_SELECT3		0x08  	//

#define	RX_GROUP		0x40  	// 0 = RxA  /  1 = RxB
#define	CHARGE_TYPE		0x80  	// 0 = Projected  /  1 = Self / surface

// Bit definitions - ProxSettings0
#define	NOISE_EN		0x20  	// 0 = noise detection disabled / 1 = noise detection enabled

#define MAX_TOUCHES 		5	// max number of touches

/* Structure to keep track of all the touches	*/
struct IQS5xx_fingers_structure {
	u8 ID;			// the ID of the touch, to keep track of multiple touches
	u16 XPos;		// the X-coordinate of the reported touch
	u16 YPos;		// the Y-coordinate of the reported touch
	u16 prev_x;		// the previous X-coordinate for filters
	u16 prev_y;		// the previous Y-coordinate for filters
	u16 touchStrength; 	// the 'hardness' of the touch
};

#endif

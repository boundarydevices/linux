//Sends Wiegand signals out J46. GPIO7=Data0 and GPIO8=Data1
 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>       // Required for the GPIO functions
#include <linux/kobject.h>    // Using kobjects for the sysfs bindings
#include <linux/kthread.h>    // Using kthreads for the flashing functionality
#include <linux/delay.h>      // Using this header for the msleep() function
#include <linux/time.h>       // gettimeofday
#include <linux/gpio.h>

#include <linux/hrtimer.h>
#include <linux/sched.h>

//Timer Variables
static enum hrtimer_restart function_timer(struct hrtimer *);
static struct hrtimer htimer;
static ktime_t kt_periode;
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ian Coolidge");
MODULE_DESCRIPTION("Wiegand input/output reader/generator for Boundary Devices Nitrogen6x-Lite");
MODULE_VERSION("0.1");
 
static unsigned int data0 = 7;              /// GPIO7 = Data0
static unsigned int data1 = 8;              /// GPIO8 = Data1
module_param(data0, uint, S_IRUGO);         /// S_IRUGO can be read/not changed
module_param(data1, uint, S_IRUGO);
MODULE_PARM_DESC(data0, " Wiegand Data0 = GPIO7");
MODULE_PARM_DESC(data1, " Wiegand Data1 = GPIO8");

static unsigned int curTime = 0;            ///Current time within 2050us period
static unsigned int lock = 0;               ///lock on inputs, user must read 0 before sending a hexval
static unsigned int length = 26;            ///Wiegand26
module_param(length, uint, S_IRUGO);

static unsigned long long int thisScan = 0;
static unsigned long long int lastScan = 1; ///Default different value at startup than thisScan
static unsigned long long int thisPayload;  ///Stores payload of bits, same as thisScan without top and bottom bits of parity
static struct timespec lastTime = {0};
// 26-37 bit parity variables
static bool evenParity;
static bool oddParity;
// 48 bit parity variables
static bool odd48AllParity;
static bool even48Parity;
static bool odd48Parity;
static unsigned long long int tMask;

// Sending states
#define IDLE 0
#define DRIVE 1
#define STOP 2
#define END 3

static int state = IDLE;

/*
//even parity - top bits
//parity is first floor( (length-2)/2 ) bits
static bool checkEvenParity(void){
   int i;
   unsigned int parity = 0;
   unsigned long long int mask = 1ULL << ( (length-2)-1 );
   int parityBits = ( (length-2)/2 );
   for(i = 0; i < parityBits; i++){
      if(thisScan & mask)
         parity++;
      mask >>= 1;
   }
   return (parity % 2) == 1;
}
//odd parity - bottom bits
//parity is the rest of the bits
//This is unlike the documentation for the converter, but works with the converter.
static bool checkOddParity(void){
   int i;
   unsigned int parity = 0;
   unsigned long long int mask = 1ULL << ( (length-2)-1 );
   int parityBits = ( (length-2)/2 );
   mask >>= parityBits;
   for(i = 0; i < ( (length-2)-parityBits ); i++){
      if(thisScan & mask)
         parity++;
      mask >>= 1;
   }
   return (parity % 2) == 0;
}*/

//======================================================================
// 26-37 bit parity verification functions
//======================================================================
//even parity - top bits
//parity is first floor( (length-2)/2 ) bits
static bool verifyEvenParity(void){
   int i;
   unsigned int parity = 0;
   unsigned long long int mask = 1ULL << ( (length-2)-1 );
   int parityBits = ( (length-2)/2 );
   for(i = 0; i < parityBits; i++){
      if(thisPayload & mask)
         parity++;
      mask >>= 1;
   }
   return (parity % 2) == 1;
}

//odd parity - bottom bits
//parity is the rest of the bits
//This is unlike the documentation for the converter, but works with the converter.
static bool verifyOddParity(void){
   int i;
   unsigned int parity = 0;
   unsigned long long int mask = 1ULL << ( (length-2)-1 );
   int parityBits = ( (length-2)/2 );
   mask >>= parityBits;
   for(i = 0; i < ( (length-2)-parityBits ); i++){
      if(thisPayload & mask)
         parity++;
      mask >>= 1;
   }
   return (parity % 2) == 0;
}

static bool verifyParity(void){
   bool result;
   result = verifyEvenParity();
   if(result != evenParity)
      return false;
   result = verifyOddParity();
   if(result != oddParity)
      return false;
   return true;
}
// End 26-37 bit parity verification functions

//======================================================================
// 48 bit parity verification functions
//======================================================================
// O(47*X)
static bool verify48OddAllParity(void){
	int i;
	unsigned int parity = 0;
	unsigned long long int mask = 1ULL << (length-2); // Mask goes from 0X0...0 down to 0...X
	int parityBits = 47;
	for(i = 0; i < parityBits; i++){
		if(thisScan & mask)
			parity++;
		mask >>= 1;
	}
	return (parity % 2) == 0;
}
// ..(15*XX.)O
//         PP110110110110110110110110110110110110110110110P
// Mask goes X000000000000000000000000000000000000000000000
// Down to   00000000000000000000000000000000000000000000X0
// parityBits = 45 (48 - 3 parity bits)
static bool verify48OddParity(void){
	int i;
	unsigned int parity = 0;
	// mask is 110110110110110110110110110110110110110110110 = 30158033218998 in decimal
	// payload XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
	// result  ?????????????????????????????????????????????
	// iter    X00000000000000000000000000000000000000000000
	// shift = 45 times
	unsigned long long int mask = 1ULL << (length-3);
	int parityBits = 45;
	for(i = 0; i < parityBits/3; i++){
		if(thisScan & mask)
			parity++;
		mask >>= 1;
		if(thisScan & mask)
			parity++;
		mask >>= 2;
	}
	return (parity % 2) == 0;
}
//buggy?
/*static bool verify48OddParity(void){
	int i;
	unsigned int parity = 0;
	// mask is 110110110110110110110110110110110110110110110 = 30158033218998 in decimal
	// payload XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
	// result  ?????????????????????????????????????????????
	// iter    X00000000000000000000000000000000000000000000
	// shift = 45 times
	unsigned long long int mask = 30158033218998ULL;
	unsigned long long int payloadResult = mask & thisPayload;
	unsigned long long int payloadResultMask = 1ULL < 44;
	int parityBits = 45;
	for(i = 0; i < parityBits; i++){
		if(payloadResult & payloadResultMask)
			parity++;
		payloadResultMask >>= 1;
	}
	return (parity % 2) == 0;
}*/
// .E(15*.XX).
// mask is         011011011011011011011011011011011011011011011 = 15079016609499 in decimal
// payload is      XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// result is       ?????????????????????????????????????????????
// iter is         X00000000000000000000000000000000000000000000
// shift = 45 times
static bool verify48EvenParity(void){
	int i;
	unsigned int parity = 0;
	unsigned long long int mask = 1ULL << (length-3);
	int parityBits = 45;
	for(i = 0; i < parityBits/3; i++){
		mask >>= 1;
		if(thisScan & mask)
			parity++;
		mask >>= 1;
		if(thisScan & mask)
			parity++;
		mask >>= 1;
	}
	return (parity % 2) == 1;
}
//buggy?
/*static bool verify48EvenParity(void){
	int i;
	unsigned int parity = 0;
	unsigned long long int mask = 15079016609499ULL;
	unsigned long long int payloadResult = mask & thisPayload;
	unsigned long long int payloadResultMask = 1ULL < 44;
	int parityBits = 45;
	for(i = 0; i < parityBits; i++){
		if(payloadResult & payloadResultMask)
			parity++;
		payloadResultMask >>= 1;
	}
	return (parity % 2) == 1;
}*/

static bool verify48Parity(void){
	bool result;
	result = verify48OddAllParity();
	if(result != odd48AllParity){
		printk(KERN_INFO "Wiegand: failed 48oddAllParity\n");
		return false;
	}
	result = verify48OddParity();
	if(result != odd48Parity){
		printk(KERN_INFO "Wiegand: failed 48oddParity\n");
		return false;
	}
	result = verify48EvenParity();
	if(result != even48Parity){
		printk(KERN_INFO "Wiegand: failed 48evenParity\n");
		return false;
	}
	return true;
}

//Function called when /sys/kernel/wiegand/length file is read
static ssize_t length_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", length);
}

//Function called when /sys/kernel/wiegand/length file is written to
static ssize_t length_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){
   unsigned int lengthsetting;                     // Using a variable to validate the data sent
   sscanf(buf, "%du", &lengthsetting);             // Read in the inputsetting as an unsigned int
   if( lengthsetting >= 26 )
		length = lengthsetting;
   return lengthsetting;
}

static ssize_t lock_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", lock);
}

static ssize_t hexval_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%llx\n", thisScan);
}

// Only store when unlocked and either 5 seconds has passed or a new scan is detected that is not a duplicate.
static ssize_t hexval_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){
   bool result;
   unsigned long long int mask;
   struct timespec thisTime;
   getnstimeofday(&thisTime);
   if(lock == 0){ // Only take hex input when a scan has cleared
      sscanf(buf,"%llx", &thisScan);
      if( (thisScan != lastScan) || (thisTime.tv_sec > lastTime.tv_sec+5) ){ //Debounce inputs for 5 seconds
				printk(KERN_INFO "Wiegand: Got hexval input: %llx\n",thisScan);
				printk(KERN_INFO "Wiegand: Transmitting %d length without confirming parity.\n", length);
				curTime = 0;
				lock = 1;
				// Enable the timer
				state = DRIVE;
				tMask = 1ULL << (length-1);
				kt_periode = ktime_set(0, 50000);
				hrtimer_init (& htimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
				htimer.function = &function_timer;
				hrtimer_start(& htimer, kt_periode, HRTIMER_MODE_REL);
			}
   }
   return count;
}

static struct kobj_attribute length_attr = __ATTR(length, 0666, length_show, length_store);
static struct kobj_attribute lock_attr = __ATTR(lock, 0666, lock_show, NULL); // No lock_store function, it is controlled internally
static struct kobj_attribute hexval_attr = __ATTR(hexval, 0666, hexval_show, hexval_store);
 
static struct attribute *wiegand_attrs[] = {
   &length_attr.attr,                         // WiegandNN where 37 >= NN >= 26
   &lock_attr.attr,
   &hexval_attr.attr,
   NULL,
};

static struct attribute_group attr_group = {
   .attrs = wiegand_attrs,                      // The attributes array defined just above
};
 
static struct kobject *wiegand_kobj;
static unsigned set0 = 0;
static unsigned set1 = 0;

//Since Wiegand is active-low, writing 1 is actually 0v
//Hold desired bit low for 50us, then wait 2ms, then repeat
//Trail with 50ms before new data.
static enum hrtimer_restart function_timer(struct hrtimer * timer)
{
	ktime_t kt_now;
	ktime_t now, done;

	switch(state)
	{

		case DRIVE:

			if(thisScan & tMask){
				gpio_set_value(data1,1);
				set1 = 1;
			}else{
				gpio_set_value(data0,1);
				set0 = 1;
			}
			tMask >>= 1;

			/* same as hrtimer_cb_get_time, but works even if CONFIG_HIGH_RES_TIMERS not set */
			now = timer->base->get_time();
			done = ktime_add_ns(now, 50000);

			// Busy wait to prevent OS from switching context, allows more precise timing
			while (1) {
				now = timer->base->get_time();
				if (ktime_compare(now, done) >= 0)
					break;
			}

			if(set0){
				gpio_set_value(data0,0);
				set0 = 0;
			}else if(set1){
				gpio_set_value(data1,0);
				set1 = 0;
			}
			if(tMask){
				state = DRIVE;
				kt_periode = ktime_set(0, 2000000); //2ms time between bit drives
			}else{
				state = END;
				kt_periode = ktime_set(0, 50000000); //50ms time between word transmits
			}
			kt_now = hrtimer_cb_get_time(&htimer);
			hrtimer_forward(&htimer, kt_now, kt_periode);
			state = tMask ? DRIVE : END;
			return HRTIMER_RESTART;
		break;

		case END:

			state = IDLE;
			lock = 0;
			return HRTIMER_NORESTART;
		break;
	}

	// Should never get here, but just in case stop the timer.
	return HRTIMER_NORESTART;
}

static int __init wiegand_init(void){
   int result = 0;
   getnstimeofday(&lastTime);
 
   printk(KERN_INFO "Wiegand: Initializing the Wiegand LKM\n");
 
   wiegand_kobj = kobject_create_and_add("wiegand", kernel_kobj); // /sys/kernel/wiegand/ setup there
   if(!wiegand_kobj){
      printk(KERN_ALERT "Wiegand: failed to create kobject\n");
      return -ENOMEM;
   }
   // add the attributes to /sys/kernel/wiegand/ -- for example, /sys/kernel/wiegand/mode
   result = sysfs_create_group(wiegand_kobj, &attr_group);
   if(result) {
      printk(KERN_ALERT "Wiegand: failed to create sysfs group\n");
      kobject_put(wiegand_kobj);                // clean up -- remove the kobject sysfs entry
      return result;
   }

   //Request GPIOS
   result = gpio_request(data0, "Wiegand Data0");
   if(result){
      printk(KERN_ALERT "Wiegand: Error requesting Data0. %d.\n", result);
      kobject_put(wiegand_kobj);                // clean up -- remove the kobject sysfs entry
      return result;
   }
   result = gpio_request(data1, "Wiegand Data1");
   if(result){
      printk(KERN_ALERT "Wiegand: Error requesting Data1. %d.\n", result);
      kobject_put(wiegand_kobj);                // clean up -- remove the kobject sysfs entry
      return result;
   }

   //Set GPIOs as output
   result = gpio_direction_output(data0, 0);
   if(result){
      printk(KERN_ALERT "Wiegand: Error Setting direction output Data0. %d.\n", result);
      kobject_put(wiegand_kobj);                // clean up -- remove the kobject sysfs entry
      return result;
   }
   result = gpio_direction_output(data1, 0);
   if(result){
      printk(KERN_ALERT "Wiegand: Error Setting direction output Data1. %d.\n", result);
      kobject_put(wiegand_kobj);                // clean up -- remove the kobject sysfs entry
      return result;
   }

   //Set GPIOs active low
   result = gpio_sysfs_set_active_low(data0, 1);
   if(result){
      printk(KERN_ALERT "Wiegand: Error setting Data0 output. %d.\n",result);
      return result;
   }
   result = gpio_sysfs_set_active_low(data1, 1);
   if(result){
      printk(KERN_ALERT "Wiegand: Error setting Data1 output. %d.\n",result);
      return result;
   }

   //export to /sys/class/gpio/gpio# for useful debugging
   gpio_export(data0, false);               // Causes /sys/class/gpio/gpio7 to appear
   gpio_export(data1, false);               // Causes /sys/class/gpio/gpio8 to appear
                                            // the second argument prevents the direction from being changed

   return result;
}
 
static void __exit wiegand_exit(void){
   hrtimer_cancel(&htimer);                 // Cancel timer
   kobject_put(wiegand_kobj);               // clean up -- remove the kobject sysfs entry
   gpio_unexport(data0);                    // Unexport data0
   gpio_unexport(data1);                    // Unexport data1
   gpio_free(data0);                        // Free data0
   gpio_free(data1);                        // Free data1
   printk(KERN_INFO "Wiegand: Shutting down.\n");
}
 
/// This next calls are  mandatory -- they identify the initialization function
/// and the cleanup function (as above).
module_init(wiegand_init);
module_exit(wiegand_exit);

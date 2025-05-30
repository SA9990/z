#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#define CONFIG_I2C_MODULE
#include <linux/i2c.h>
#include "device.h"
#include "memory.h"
#include "serial_bus.h"
#include "main.h"
#include "event.h"
#include "hardware.h"
#include "sysfs.h"
#include "utils.h"
#include "config.h"
#include "debug.h"
#include "sonacomm.h"
#include "workqueue.h"
#include "irq.h"

#include <linux/proc_fs.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
//#include <linux/wakelock.h>
#include "grip_Wakelock.h"
#include "locking.h"
#include "customize.h"

#include <linux/timer.h>

static int SntSensor_miscOpen(struct inode *inode, struct file *file);
static int SntSensor_miscRelease(struct inode *inode, struct file *file);
static long SntSensor_miscIoctl(struct file *file, unsigned int cmd, unsigned long arg);

void grip_enable_func_noLock(int val);
void grip_tap1_enable_func(int val);
void grip_tap2_enable_func(int val);
void grip_tap3_enable_func(int val);
void grip_tap1_force_func(int val);
void grip_tap2_force_func(int val);
void grip_tap3_force_func(int val);
void grip_tap1_min_position_func(int val);
void grip_tap2_min_position_func(int val);
void grip_tap3_min_position_func(int val);
void grip_tap1_max_position_func(int val);
void grip_tap2_max_position_func(int val);
void grip_tap3_max_position_func(int val);

void grip_squeeze1_enable_func(int val);
void grip_squeeze1_force_func(int val);
void grip_squeeze1_short_dur_func(int val);
void grip_squeeze1_long_dur_func(int val);
void grip_squeeze1_up_rate_func(int val);
void grip_squeeze1_up_total_func(int val);
void grip_squeeze1_down_rate_func(int val);
void grip_squeeze1_down_total_func(int val);
void grip_squeeze2_enable_func(int val);
void grip_squeeze2_force_func(int val);
void grip_squeeze2_short_dur_func(int val);
void grip_squeeze2_long_dur_func(int val);
void grip_squeeze2_up_rate_func(int val);
void grip_squeeze2_up_total_func(int val);
void grip_squeeze2_down_rate_func(int val);
void grip_squeeze2_down_total_func(int val);

void grip_slide1_enable_func(int val);
void grip_slide2_enable_func(int val);
void grip_slide1_dist_func(int val);
void grip_slide2_dist_func(int val);
void grip_slide1_force_func(int val);
void grip_slide2_force_func(int val);

void grip_swipe1_enable_func(int val);
void grip_swipe2_enable_func(int val);
void grip_swipe1_velocity_func(int val);
void grip_swipe2_velocity_func(int val);
void grip_swipe1_len_func(int val);
void grip_swipe2_len_func(int val);

void Wait_Wake_For_RegW(void);
void Into_DeepSleep_fun(void);
void grip_dump_status_func(struct work_struct *work);
void Reset_Func(struct work_struct *work);
static int Health_Check_Enable_No_Delay(int en);
//static void Enable_tap_sensitive(const char *buf, size_t count);
static void Grip_Apply_Golden_K(void);
static void Read_Grip_K_data(void);
//Check grip all gesture status, 1: enable, 0:disable
static int grip_all_gesture_status(void);

//Check grip game gesture (tap/slide/swipe) status, 1: enable, 0:disable
int grip_game_gesture_status(void);

//vibrator en/disable func
extern int aw8697_trig_control(int num, bool enable);

/* ASUS BSP Clay: setting vibrator trig1 & trig2 register for different tap modes +++ */
extern int G_grip_tap_vib1_reg;
extern int G_grip_tap_vib2_reg;
static void gripVibratorSetting(int val, int tap_id);
/* ASUS BSP Clay--- */

#ifdef DYNAMIC_PWR_CTL
extern int snt_activity_request(void);
#endif


/* Workaround for stucked semaphore */
void check_stuck_semaphore(struct work_struct *work);
void Power_Control(int en);
extern void snt_set_pinctrl(struct device *dev, char *str);
struct delayed_work check_stuck_wake;
struct workqueue_struct *asus_wq;

/* 
    param. fw_loading_status:
    0: when charger/recorver or the other mode, grip fw will load fail 
    1: load fw success
int fw_loading_status = 0;
*/

/* Factory: Calibration Test */
static int bar_test_force = 0;
static int bar_test_tolerance = 0;

/* init 1V2_2V8 power status */
static int Snt_Power_State = 1;

static uint32_t ms_start=0, ms_end=0;

struct file_operations sentons_snt_fops = {
  .owner = THIS_MODULE,
  .open = SntSensor_miscOpen,
  .release = SntSensor_miscRelease,
  .unlocked_ioctl = SntSensor_miscIoctl,
  .compat_ioctl = SntSensor_miscIoctl
};
struct miscdevice sentons_snt_misc = {
  .minor = MISC_DYNAMIC_MINOR,
  .name = "sentons_grip_sensor",
  .fops = &sentons_snt_fops
};

/* K data related parameters */
static const uint16_t Grip_Golden = 0x5F69; /* v1 is used before WW PR2 and CN PR */
static const uint16_t Grip_Golden_MP = 0x6E5A; /* v1 is used before WW PR2 and CN PR */
static const float Grip_K_Scaling_factory = 1; /* v1 is used before WW PR2 and CN PR */

static const uint16_t Grip_Golden_V1 = 0x5E46; /* v1 is used before WW PR2 and CN PR */
static const uint16_t Grip_Golden_V2 = 0x945A;
static const uint16_t Grip_Golden_addr = 0x34;
static bool G_Golden_K_flag = 0;

/* init record helath check result */
uint16_t FPC_value = 0;

/* init record factory force value */
uint16_t Grip_B0_F_value = 50;
uint16_t Grip_B1_F_value = 100;
uint16_t Grip_B2_F_value = 80;


static uint16_t Tap_sense_data1[3] = { 
		0x0003,
		0x0000,
		0x8062 
};
static uint16_t Tap_sense_data2[3] = { 
		0x0014,
		0x0000,
		0x804b 
};

static uint16_t Tap_sense_data3[3] = { 
		0x0028,
		0x0000,
		0x8053 
};

static uint16_t Tap_sense_reset_data1[3] = { 
		0x0032,
		0x0000,
		0x804b 
};
static uint16_t Tap_sense_reset_data2[3] = { 
		0x003c,
		0x0000,
		0x8053 
};

static uint16_t Slide_Swipe_sense_data[3] = { 
		0x0014,
		0x0000,
		0x804b 
};

static uint16_t Slide_Swipe_sense_reset_data[3] = { 
		0x0032,
		0x0000,
		0x804b 
};
static uint16_t sys_param_addr[3] = { 
		0x42, 
		0x43, 
		0x44  
};
extern int track_report_count;


/* after chip reset,  recovery according to status which records from 
property */
struct delayed_work rst_recovery_wk;

/* reset chip when i2c error or chip can't be waked up  */
struct delayed_work rst_gpio_wk;

static int Grip_Check_FW_Status(void){
	if(snt8100fsr_g->chip_reset_flag != GRIP_FW_DL_END){
		mutex_unlock(&snt8100fsr_g->ap_lock);
		snt8100fsr_g->Recovery_Flag = 1;
		return 1;
	}else{
		return 0;
	}
}

void Grip_Chip_IRQ_EN(bool flag){
	uint16_t irq_val = 0;
	if(flag){
		irq_val = 0x1;
		PRINT_INFO("Enable Chip iRQ, Reg 0x1 = 0x%x", irq_val);
		write_register(snt8100fsr_g,
		        REGISTER_ENABLE,
		        &irq_val);
	}else{
		irq_val = 0x0;
		PRINT_INFO("Disable Chip IRQ, Reg 0x1 = 0x%x", irq_val);
		write_register(snt8100fsr_g,
		        REGISTER_ENABLE,
		        &irq_val);
	}
}
void Grip_Driver_IRQ_EN(bool flag){
	if(flag){
		PRINT_INFO("Enable Driver IRQ");
		enable_irq_wake(gpio_to_irq(snt8100fsr_g->hostirq_gpio));
	}else{
		PRINT_INFO("Disable Driver IRQ");
		disable_irq_wake(gpio_to_irq(snt8100fsr_g->hostirq_gpio));
	}
}

/* for squeeze/tap cancel missing when grip reset */
extern int write_fail_count;
void Reset_Func(struct work_struct *work){
	if(snt8100fsr_g->grip_fw_loading_status == false){	
		PRINT_INFO("1. load continue or 2. fw load fail at init, chip broken case, don't retry");
		return;
	}
    if(snt8100fsr_g->chip_reset_flag == GRIP_RST_FW_DL){
		PRINT_INFO("Chip reset is ongoing, skip request");
		return;
    }
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
    if(snt8100fsr_g->chip_reset_flag == GRIP_FW_DL_END){
		PRINT_INFO("Starting chip reset");
	    ASUSEvtlog("[Grip] Workaround : reset chip\n");
	    snt8100fsr_g->chip_reset_flag = GRIP_RST_FW_DL;
	    snt8100fsr_g->grip_fw_loading_status = false; /* reset fw_loading status */
	    write_fail_count = 0; /* reset i2c write failed count */
	    snt8100fsr_g->snt_state = GRIP_WAKEUP;
		Grip_Driver_IRQ_EN(1);
	    Power_Control(1);
    }else{
		/* Do nothing */
    }
}

void check_stuck_semaphore(struct work_struct *work){
	int ret;
	/* prevent chip reset fail */
	if(snt8100fsr_g->chip_reset_flag != GRIP_FW_DL_END){
		workqueue_cancel_work(&check_stuck_wake);
		PRINT_INFO("Don't wake chip during chip reset & long fw");
		snt8100fsr_g->stuck_retry_count = 0;
		return;
	}
	if(snt8100fsr_g->stuck_flag == true){
		PRINT_INFO("Used to solve wailting semaphore!!! retry times = %d", 
		snt8100fsr_g->stuck_retry_count);
		/* when retry occurs, check listened gpio status */
		PRINT_INFO("GPIO: %d=%d", IRQ_GPIO, gpio_get_value(IRQ_GPIO));
		MUTEX_LOCK(&snt8100fsr_g->sb_lock);
 		ret = sb_wake_device(snt8100fsr_g);
		mutex_unlock(&snt8100fsr_g->sb_lock);
	        if (ret) {
	            PRINT_CRIT("sb_wake_device() failed");
	        }
		//retry check
		if(snt8100fsr_g->stuck_retry_count < snt8100fsr_g->stuck_retry_limit){
			workqueue_cancel_work(&check_stuck_wake);
			workqueue_queue_work(&check_stuck_wake, 200);
			snt8100fsr_g->stuck_retry_count++;
		}else{
		  	ASUSEvtlog("[Grip] driver is failed to wait semaphore due to non-wakeable chip\n"); 
        	up(&snt8100fsr_g->wake_rsp);
			if (down_trylock(&snt8100fsr_g->wake_req)){
				PRINT_INFO("Wake Req alread consumed");
				if(down_trylock(&snt8100fsr_g->wake_rsp)){
					PRINT_INFO("Wake Rsq alread consumed");
				}
	    	}
#ifdef FACTORY_FLAG
#else
    			queue_delayed_work(asus_wq, &rst_gpio_wk, msecs_to_jiffies(0));
#endif
		}
	}else{
		PRINT_INFO("None used");
		snt8100fsr_g->stuck_retry_count = 0;
		workqueue_cancel_work(&check_stuck_wake);
	}
}
static int Health_Check_Enable_No_Delay(int en){
	uint16_t En_fpc;
	int ret;
	ret = read_register(snt8100fsr_g,
                 0x003d,
                 &En_fpc);
	if(ret < 0) {
		PRINT_ERR("Read Reg 3d Failed");
		return -1;	
	}
	//PRINT_INFO("0x003d:%x ", En_fpc);
	if(en == 0){
		En_fpc = En_fpc | 0x0004;
	}else if(en ==1){
		En_fpc = En_fpc & 0xfffb;
	}else{
		PRINT_ERR("en=%d, out of 1 or 2 ", en);
		return -1;
	}
	
	ret = write_register(snt8100fsr_g,
                 0x003d,
                 &En_fpc);
	if(ret < 0) {
		PRINT_ERR("Read Reg 3d Failed");
		return -1;
	}
	PRINT_INFO("Health Check EN=%d", en);
	return 0;
}


int Health_Check_Enable(int en){
	uint16_t En_fpc;
	int ret;
	ret = read_register(snt8100fsr_g,
                 0x003d,
                 &En_fpc);
	if(ret < 0) {
		PRINT_ERR("Read Reg 3d Failed");
		return -1;	
	}
	//PRINT_INFO("0x003d:%x ", En_fpc);
	if(en == 0){
		En_fpc = En_fpc | 0x0004;
	}else if(en ==1){
		En_fpc = En_fpc & 0xfffb;
	}else{
		PRINT_ERR("en=%d, out of 1 or 2 ", en);
		return -1;
	}
	
	ret = write_register(snt8100fsr_g,
                 0x003d,
                 &En_fpc);
	if(ret < 0) {
		PRINT_ERR("Read Reg 3d Failed");
		return -1;
	}
	msleep(100);
	ret = read_register(snt8100fsr_g,
                 0x003d,
                 &En_fpc);
	if(ret < 0) {
		PRINT_ERR("Read Reg 3d Failed");
		return -1;
	}
	PRINT_INFO("Health Check EN=%d", en);
	return 0;
}
int Health_Check(uint16_t val){
	int ret;
	uint16_t FPC_status;
	//Enable Health Check
	ret = Health_Check_Enable(0);
	ret = Health_Check_Enable(1);
	
	ret = read_register(snt8100fsr_g,
                 REGISTER_PHY_STAT_LSB,
                 &FPC_status);
        if(ret < 0) {
		PRINT_ERR("Read 0x03 Fail");
		Health_Check_Enable(0);
		return -1;
	}
	PRINT_INFO("0x03: 0x%x, expect: 0x%x", FPC_status, FPC_status |val);
	FPC_value = FPC_status;
	if (FPC_status != (FPC_status | val)) {
		PRINT_INFO("Health Check Fail!!!");
		Health_Check_Enable(0);
		return -1;
	}
	ret = Health_Check_Enable(0);
	return 0;
}
/*---BSP Clay proc asusGripDebug Interface---*/
void print_current_report(int i){
	PRINT_INFO("%u %u %u %u %u %u %u\n",
				snt8100fsr_g->track_reports_frame,
	                        snt8100fsr_g->track_reports[i].bar_id,
	                        snt8100fsr_g->track_reports[i].trk_id,
	                        snt8100fsr_g->track_reports[i].force_lvl,
	                        snt8100fsr_g->track_reports[i].top,
	                        snt8100fsr_g->track_reports[i].center,
	                        snt8100fsr_g->track_reports[i].bottom);
}
int check_report_force(int force){
	int force_top, force_floor;
	if(bar_test_tolerance == 100){
		return 1;
	}else if((bar_test_tolerance > 0) && (bar_test_tolerance < 100)){
		force_top = bar_test_force * (100 + bar_test_tolerance) /100;
		force_floor = bar_test_force * (100 - bar_test_tolerance) /100;
	}else{
		force_top = bar_test_force;
		force_floor = bar_test_force;
	}
	PRINT_INFO("force check: force = %d, threshould = %d, tolerance = %d percent, top = %d, floor= %d", 
				force, bar_test_force, bar_test_tolerance, force_top, force_floor);
	if(bar_test_force > 0){
		if(force >= force_floor && force <= force_top){
			return 1;
		}else{
			return 0;
		}
	} else {
		return 1;
	}
}
extern struct delayed_work event_wq;
void Into_DeepSleep_fun(void){
#ifdef FACTORY_FLAG
#else
	int ret = 0;
	int frame_rate=65535;
	//int frame_rate=20;
	if(snt8100fsr_g->chip_reset_flag != GRIP_FW_DL_END){
		PRINT_INFO("skip during fw download");
	}else if(grip_status_g->G_EN==0 || grip_status_g->G_EN==-1){
		//Disable irq when driver requests chip into deep sleep mode
		//Grip_Chip_IRQ_EN(0);
		MUTEX_LOCK(&snt8100fsr_g->IRQ_WAKE_SLEEP_LOCK);
		snt8100fsr_g->snt_state = GRIP_DEEPSLEEP;
		workqueue_cancel_work(&event_wq);
		ret = write_register(snt8100fsr_g,
	                 REGISTER_FRAME_RATE,
	                 &frame_rate);
	    if(ret < 0) {
			PRINT_ERR("Grip register_enable write fail");
	    }else{
			PRINT_INFO("Grip_EN = %d, Grip_Frame = %d", grip_status_g->G_EN, frame_rate);
	    }
		//msleep(10);
		Grip_Driver_IRQ_EN(0);
		ms_start = get_time_in_ms();
		mutex_unlock(&snt8100fsr_g->IRQ_WAKE_SLEEP_LOCK);
	}
#endif
}

/*
void Wake_device_func(void){
	int ret;
	MUTEX_LOCK(&snt8100fsr_g->sb_lock);
	ret = sb_wake_device(snt8100fsr_g);
	if (ret) {
		PRINT_CRIT("sb_wake_device() failed");
		mutex_unlock(&snt8100fsr_g->sb_lock);
		return;
	}
	mutex_unlock(&snt8100fsr_g->sb_lock);
	msleep(100);
}
*/
void Wait_Wake_For_RegW(void){
#ifdef DYNAMIC_PWR_CTL
        //sb_wake_device(snt8100fsr_g);
	if(snt8100fsr_g->grip_fw_loading_status == true){
	    if (snt_activity_request() != 0) {
        	PRINT_CRIT("snt_activity_request() failed");
	    }
	}else{
		PRINT_INFO("Load FW Fail, skip wakeup request");
	}
#endif
}
/* write DPC function */
void DPC_write_func(int flag){
#ifdef DYNAMIC_PWR_CTL
	int ret;
	grip_status_g->G_DPC_STATUS = flag;
	if(flag == 1){
		if(grip_game_gesture_status()){
			PRINT_INFO("Don't Enable DPC since tap/slide/swipe enable");
		}else{
			PRINT_INFO("Enable DPC, write 0x%x, 0x%x, 0x%x", 
				Grip_DPC_status_g->High, Grip_DPC_status_g->Low, Grip_DPC_status_g->Condition);
			ret = write_register(snt8100fsr_g, REGISTER_DPC_HIGH_FRAME, &Grip_DPC_status_g->High);
			if (ret) {
			    PRINT_CRIT("set DPC 0x%x failed", REGISTER_DPC_HIGH_FRAME);
			}
			ret = write_register(snt8100fsr_g, REGISTER_DPC_LOW_FRAME, &Grip_DPC_status_g->Low);
			if (ret) {
			    PRINT_CRIT("set DPC 0x%x failed", REGISTER_DPC_LOW_FRAME);
			}
			ret = write_register(snt8100fsr_g, REGISTER_DPC_CONDITION, &Grip_DPC_status_g->Condition);
			if (ret) {
			    PRINT_CRIT("set DPC 0x%x failed", REGISTER_DPC_CONDITION);
			}
		}
	}else{
		PRINT_INFO("Disable DPC");
		ret = write_register(snt8100fsr_g, REGISTER_DPC_CONDITION, &flag);
		if (ret) {
		    PRINT_CRIT("set DPC 0x%x failed", REGISTER_DPC_CONDITION);
		}
		ret = write_register(snt8100fsr_g, REGISTER_DPC_HIGH_FRAME, &flag);
		if (ret) {
		    PRINT_CRIT("set DPC 0x%x failed", REGISTER_DPC_HIGH_FRAME);
		}
		ret = write_register(snt8100fsr_g, REGISTER_DPC_LOW_FRAME, &flag);
		if (ret) {
		    PRINT_CRIT("set DPC 0x%x failed", REGISTER_DPC_LOW_FRAME);
		}
	}
#endif
}
void Check_Tap_sense_val(void){
	int i=0;
	uint32_t RegRead_t;
	msleep(50);
	for(i = 0; i < 3; i++){
		read_register(snt8100fsr_g, sys_param_addr[i], &RegRead_t);
		PRINT_INFO("Reg: 0x%x, Val: 0x%x", sys_param_addr[i], RegRead_t);
	}
}
void Tap_sense_write_func(int flag){
	int i = 0;
	if(grip_status_g->G_TAP_SENSE_EN != flag){
		grip_status_g->G_TAP_SENSE_EN = flag;
	}else{
		PRINT_INFO("TAP SENSE=%d, Already set before=======", flag);
		return;
	}
	if(flag == 1){
		PRINT_INFO("[Enable] Tap Sense data");
		for(i = 0; i < 3; i++){
			write_register(snt8100fsr_g, sys_param_addr[i], &Tap_sense_data1[i]);
		}
		Check_Tap_sense_val();
		for(i = 0; i < 3; i++){
			write_register(snt8100fsr_g, sys_param_addr[i], &Tap_sense_data2[i]);
		}
		Check_Tap_sense_val();
		
		for(i = 0; i < 3; i++){
			write_register(snt8100fsr_g, sys_param_addr[i], &Tap_sense_data3[i]);
		}
		Check_Tap_sense_val();
		
	}else{
		PRINT_INFO("[Disable] Tap Sense data");
		for(i = 0; i < 3; i++){
			write_register(snt8100fsr_g, sys_param_addr[i], &Tap_sense_reset_data1[i]);
		}
		Check_Tap_sense_val();
		for(i = 0; i < 3; i++){
			write_register(snt8100fsr_g, sys_param_addr[i], &Tap_sense_reset_data2[i]);
		}
		Check_Tap_sense_val();
	}
}

//Used in Tap function
static void grip_check_DPC_and_sensitivity_func(void){
	int ret = 0;
	
	/********* DPC part **********/
	if(grip_game_gesture_status()){
		//there exist tap which is on
		//Disable DPC
		if(grip_status_g->G_DPC_STATUS==1){
			snt8100fsr_g->frame_rate = 100;
			
			PRINT_INFO("game gesture enable, DPC turn off from on state and set frame_rate = %d", 
			snt8100fsr_g->frame_rate);
			DPC_write_func(0);
		        ret = write_register(snt8100fsr_g, REGISTER_FRAME_RATE,
		                             &snt8100fsr_g->frame_rate);
		        if (ret) {
		            PRINT_CRIT("write_register(REGISTER_FRAME_RATE) failed");
		        }
		}else{
			PRINT_INFO("DPC already off");
		}
	}else{ 
		if(grip_status_g->G_DPC_STATUS==0){
			PRINT_INFO("Enable DPC when all taps disable");
			DPC_write_func(1);
		}
	}

	/********* sensitivity part **********/	
	if(grip_status_g->G_TAP_EN[0] <= 0 && grip_status_g->G_TAP_EN[1] <= 0){
		//Disable tap sense
		if(grip_status_g->G_TAP_SENSE_SET == 1 && grip_status_g->G_TAP_SENSE_EN == 1){
			Tap_sense_write_func(0);
		}else{
			//Do nothing
		}
	}else if(grip_status_g->G_TAP_EN[0] == 1 && grip_status_g->G_TAP_EN[1] == 1){
		if(grip_status_g->G_TAP_SENSE_SET == 1 && grip_status_g->G_TAP_SENSE_EN <= 0){
			Tap_sense_write_func(1);
		}else{
			Tap_sense_write_func(0);
		}
	}else{
		//Do nothing
	}
}

static void grip_set_sys_param(const char *buf){
	int l = (int)strlen(buf) + 1;
	uint32_t val;
	uint32_t id;
	int status;
	int ret;
	PRINT_INFO("buf=%s, size=%d", buf, l);
	
	status  = ReadNumFromBuf((const uint8_t**)&buf, &l, &id);
	if (status != 0) {
		PRINT_CRIT("Could not parse param_id %d", status);
		goto errexit;
	}

	status = ReadNumFromBuf((const uint8_t**)&buf, &l, &val);
	if (status != 0) {
		PRINT_CRIT("Could not parse param_val %d", status);
		goto errexit;
	}
	if (enable_set_sys_param(snt8100fsr_g, id, val) != 0) {
		PRINT_DEBUG("send of set sys param failed");
		goto errexit;
	}

	// wait for response from driver irpt thread
	PRINT_DEBUG("SetSysParam Rsp -- wait");
	do {
		//ret= down_timeout(&snt8100fsr_g->sc_wf_rsp, msecs_to_jiffies(3*MSEC_PER_SEC);
		ret = down_interruptible(&snt8100fsr_g->sc_wf_rsp);
		PRINT_DEBUG("SetSysParam Rsp -- acquired %d", ret);
	} while (ret == -EINTR);

errexit:
    PRINT_DEBUG("done.");
    return;
}

static void grip_slide_swipe_status_check(void){
	int i = 0;
	uint16_t enable_sensitive_boost = 0x84;
	uint16_t disable_sensitive_boost = 0x4;
	uint16_t boost_addr = 0x3d;
	static int swipe_status = 0, gesture_status = 0, slide_status = 0;
	const char *swipe_buf_on = "110 1\n";
	const char *slide_buf_on = "110 5\n";
	const char *buf_off = "110 0\n";
	
	if(grip_status_g->G_SLIDE_EN[0] == 1 || grip_status_g->G_SLIDE_EN[1] == 1
		|| grip_status_g->G_SWIPE_EN[0] == 1 || grip_status_g->G_SWIPE_EN[1] == 1){
		if(gesture_status != 1){
			gesture_status = 1;
			for(i = 0; i < 3; i++){
				write_register(snt8100fsr_g, sys_param_addr[i], &Slide_Swipe_sense_data[i]);
			}
			PRINT_INFO("write 0x%x=0x%x, 0x%x, 0x%x", 
				sys_param_addr[0], Slide_Swipe_sense_data[0],
				Slide_Swipe_sense_data[1], Slide_Swipe_sense_data[2]);
		}
		/* Slide priority > Swipe */
		if(grip_status_g->G_SLIDE_EN[0] == 1 || grip_status_g->G_SLIDE_EN[1] == 1){
			if(0 == slide_status){
				slide_status = 1;
				write_register(snt8100fsr_g, boost_addr, &disable_sensitive_boost);
				msleep(50);
				grip_set_sys_param(slide_buf_on);
			}
		}else{
			if(1 == slide_status){
				slide_status = 0;
			}
			if(grip_status_g->G_SWIPE_EN[0] == 1 || grip_status_g->G_SWIPE_EN[1] == 1){
				write_register(snt8100fsr_g, boost_addr, &enable_sensitive_boost);
				if(0 == swipe_status){
					swipe_status = 1;
					msleep(50);
					grip_set_sys_param(swipe_buf_on);
				}
			}
		}
	}else{
		if(gesture_status != 0){
			gesture_status = 0;
			for(i = 0; i < 3; i++){
				write_register(snt8100fsr_g, sys_param_addr[i], &Slide_Swipe_sense_reset_data[i]);
			}
			PRINT_INFO("write 0x%x=0x%x, 0x%x, 0x%x", 
				sys_param_addr[0], Slide_Swipe_sense_reset_data[0],
				Slide_Swipe_sense_reset_data[1], Slide_Swipe_sense_reset_data[2]);
			swipe_status = 0;
			slide_status = 0;
			write_register(snt8100fsr_g, boost_addr, &disable_sensitive_boost);
			msleep(50);
			grip_set_sys_param(buf_off);
		}
	}
/*
	if(grip_status_g->G_SLIDE_EN[0] == 1 || grip_status_g->G_SLIDE_EN[1] == 1){
		if(0 == slide_status){
			slide_status = 1;
			write_register(snt8100fsr_g, boost_addr, &disable_sensitive_boost);
			msleep(50);
			grip_set_sys_param(slide_buf_on);
		}
	}else { //If  all slide gestures are disabled
		slide_status = 0;
		if(grip_status_g->G_SWIPE_EN[0] == 1 || grip_status_g->G_SWIPE_EN[1] == 1){
			if(swipe_status != 1){
				swipe_status = 1;
				write_register(snt8100fsr_g, boost_addr, &enable_sensitive_boost);
				msleep(50);
				grip_set_sys_param(swipe_buf_on);
			}
		}else{
		}
	}
*/
	PRINT_INFO("Done");
}
/**************** +++ wrtie DPC & Frame function +++ **************/
void grip_frame_rate_func(int val){
	int ret;
	uint16_t RegRead_t = 0;
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	Wait_Wake_For_RegW();
	if(val == 10  || val == 20 || val == 25  || val == 40  || val == 50  || val 
	== 80  || val == 100){
		PRINT_INFO("val = %d", val);
		if(grip_status_g->G_DPC_STATUS==1){
			Grip_DPC_status_g->High = val;
			RegRead_t = val;
			snt8100fsr_g->frame_rate = val;
			ret = write_register(snt8100fsr_g,
		                 REGISTER_DPC_HIGH_FRAME,
		                 &RegRead_t);
		        if(ret < 0) {
				PRINT_ERR("Write reg 0x%X faill", REGISTER_DPC_HIGH_FRAME);	
		        }else{
		        	PRINT_INFO("Write DPC High: 0x%x", RegRead_t);
		        }
		}else{				
			snt8100fsr_g->frame_rate = val;
			ret = write_register(snt8100fsr_g,
		                 REGISTER_FRAME_RATE,
		                 &snt8100fsr_g->frame_rate);
		        if(ret < 0) {
				PRINT_ERR("Write reg 0x%X faill", REGISTER_FRAME_RATE);	
		        }else{
		        	PRINT_INFO("Write frame rate: 0x%x", RegRead_t);
		        }
		}
		mutex_unlock(&snt8100fsr_g->ap_lock);
	}else{
		PRINT_INFO("Not in defined frame rate range, skip val = %d", val);
		mutex_unlock(&snt8100fsr_g->ap_lock);
		return;
	}
}

/**************** ---wrtie DPC & Frame function --- **************/

/**************** +++ wrtie gesture function +++ **************/
void grip_enable_func_noLock(int val){

	if(Snt_Power_State == 0){
		grip_status_g->G_EN= 0;
		PRINT_INFO("Grip Sensor Power off, skip grip enable function");
		return;
	}
//	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	if(snt8100fsr_g->chip_reset_flag != GRIP_FW_DL_END){ //reseting
		//mutex_unlock(&snt8100fsr_g->ap_lock);
		grip_status_g->G_EN= val;
		PRINT_INFO("chip_reset_flag = %d", snt8100fsr_g->chip_reset_flag);
		return;
	}

	if(snt8100fsr_g->grip_fw_loading_status == false){
		grip_status_g->G_EN= 0;
		PRINT_INFO("Load fw fail, skip grip enable function");
		return;
	}
	if(val == 1){ //turn on
		if(grip_status_g->G_EN <= 0){ //Need turn on
#ifdef FACTORY_FLAG
#else
		        //Power_Control(val);
#endif
			// We mutex lock here since we're calling sb_wake_device which never locks
			Grip_Driver_IRQ_EN(1);

			/* Check Time before wake up */
			ms_end = get_time_in_ms();
			if((ms_end-ms_start)< snt8100fsr_g->sleep_ms_time){
				PRINT_INFO("ms_start=%u, ms_end=%u", ms_start, ms_end);
				msleep(snt8100fsr_g->sleep_ms_time-(ms_end-ms_start));
			}

			
			Wait_Wake_For_RegW();
			//Grip_Chip_IRQ_EN(1);
			Grip_DPC_status_g->Low = 5;
			DPC_write_func(1);
			write_register(snt8100fsr_g,
		                 REGISTER_FRAME_RATE,
		                 &snt8100fsr_g->frame_rate);
			PRINT_INFO("Grip_EN = %d , Grip_Frame = %d", grip_status_g->G_EN, snt8100fsr_g->frame_rate);
		}
	}else{
		if(grip_status_g->G_EN == 1){
			grip_status_g->G_EN= val;
			Wait_Wake_For_RegW();
			DPC_write_func(0);
			Into_DeepSleep_fun();
		}
	}
	grip_status_g->G_EN= val;
	//mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_raw_enable_func(int val){
	int ret;
	uint16_t RegRead_t = 0;
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	if(grip_status_g->G_RAW_EN == val){
		mutex_unlock(&snt8100fsr_g->ap_lock);
		return;
	}
	grip_status_g->G_RAW_EN = val;
	if(Grip_Check_FW_Status()){ return; }
	
	Wait_Wake_For_RegW();
	if(val == 0){
		val = 1;
	}else{
		val = 0;
	}
	RegRead_t = val << 3;
	ret = write_register(snt8100fsr_g,
                 REGISTR_RAW_DATA,
                 &RegRead_t);
        if(ret < 0) {
		PRINT_ERR("Write reg 0x%X faill", REGISTR_RAW_DATA);	
        }else{
        }
	track_report_count = 0;
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

static int grip_all_gesture_status(void){
	if(grip_status_g->G_SQUEEZE_EN[0] == 1 || grip_status_g->G_SQUEEZE_EN[1] == 1
		|| grip_status_g->G_TAP_EN[0] == 1 || grip_status_g->G_TAP_EN[1] == 1
		|| grip_status_g->G_TAP_EN[2] == 1 || grip_status_g->G_TAP_EN[3] == 1
		|| grip_status_g->G_SWIPE_EN[0] == 1 || grip_status_g->G_SWIPE_EN[1] == 1
		|| grip_status_g->G_SLIDE_EN[0] == 1 || grip_status_g->G_SLIDE_EN[1] == 1){
		return 1;
	}else{ /* No gesture or raw enable */
		return 0;
	}
}

int grip_game_gesture_status(void){
	if(grip_status_g->G_TAP_EN[0] == 1 || grip_status_g->G_TAP_EN[1] == 1
		|| grip_status_g->G_TAP_EN[2] == 1 || grip_status_g->G_TAP_EN[3] == 1
		|| grip_status_g->G_SWIPE_EN[0] == 1 || grip_status_g->G_SWIPE_EN[1] == 1
		|| grip_status_g->G_SLIDE_EN[0] == 1 || grip_status_g->G_SLIDE_EN[1] == 1){
		return 1;
	}else{ /* No gesture or raw enable */
		return 0;
	}
}

static void grip_checkToLowPower_noLock(void){
	if(grip_all_gesture_status()){
		/* Do nothing */
	}else{ /* No gesture or raw enable */
		grip_enable_func_noLock(0);
	}
}

int write_registers_fifo(int reg, int num, void *value) {
  int ret=0;
  mutex_lock(&snt8100fsr_g->sb_lock);
  ret = sb_write_fifo(snt8100fsr_g, reg, num*2, value);
  if (ret) {
    PRINT_CRIT("write_registers_fifo() failed (%d)", ret);
  }
  mutex_unlock(&snt8100fsr_g->sb_lock);
  return ret;
}

int read_registers_fifo(int reg, int num, void *value) {
  int ret=0;
  mutex_lock(&snt8100fsr_g->sb_lock);
  ret = sb_read_fifo(snt8100fsr_g, reg, num*2, value);
  if (ret) {
    PRINT_CRIT("cust_read_registers() failed (%d)", ret);
  } else {
  }
  mutex_unlock(&snt8100fsr_g->sb_lock);
  return ret;
}

int tap_reg_num = 16;
int squeeze_reg_num = 18;
int slide_reg_num = 12;
int swipe_reg_num = 10;

//Tap part start===========================================
void get_tap_gesture(uint16_t tap_id, uint16_t reg_val, int index, int len){
    //int ret=0;
    //int bytes=0;
    int i=0;
  uint16_t cfg_bank_write[3] = { 0, 0, 0x0801};
  uint16_t buf[255];
  write_registers_fifo(0x2c, 3, cfg_bank_write);
  read_registers_fifo(0x200, len, buf);
  
  for(i = 0; i < len; i++){
  	//if(i%8==1 || i%8==2){
		PRINT_INFO("reg_val=0x%x", buf[i]);
  	//}
  }
}


void set_tap_gesture(uint16_t tap_id, uint16_t reg_val, int index){
  uint16_t cfg_bank_write[3] = { tap_id*tap_reg_num + index*2, 2, 0x0802};
  uint16_t cfg_bank_commit[3] = { 0, 0, 0x0803};
  write_registers_fifo(0x2c, 3, cfg_bank_write);
  write_registers_fifo(0x200, 1, &reg_val);
  write_registers_fifo(0x2c, 3, cfg_bank_commit);
}

void grip_tap_enable_func(int tap_id, int val, uint16_t* reg ) {
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);

	if( val==1 ) {
		grip_enable_func_noLock(1);
	}
	grip_status_g->G_TAP_EN[tap_id] = val;
	if(grip_status_g->G_EN == -1)
		grip_status_g->G_EN = 0;
		
	/* when G_EN=0 which means that all gestures close, don't wakeup chip and do setting */
	if(grip_status_g->G_EN == 1){
		if(Grip_Check_FW_Status()){ return; }
		
		Wait_Wake_For_RegW();
		*reg = (*reg & 0xFFFE) | ( val & 0x0001);
		set_tap_gesture(tap_id, *reg, 0);

		/* ASUS BSP Clay: setting vibrator trig1 & trig2 register for different tap modes +++ */
		gripVibratorSetting(val, tap_id);
		/* ASUS BSP Clay:--- */
		
		//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
		grip_check_DPC_and_sensitivity_func();
		
		if( val==0) { //turn off, check if (all off) goto low power
			grip_checkToLowPower_noLock();
		}
	}else{
		PRINT_INFO("No gesture enable, skip it");
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);
}
void grip_tapX_enable_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_enable_func(id, val, reg_val);
       PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_tap1_enable_func(int val){
	int id = 0;
	grip_tap_enable_func(id, val, &TAP0_BIT0);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT0);
	//combine tap2 & tap3
	//grip_tap3_enable_func(val);
}
void grip_tap2_enable_func(int val){
	int id = 1;
	grip_tap_enable_func(id, val, &TAP1_BIT0);
        PRINT_INFO("Write tap1 reg: %x", TAP1_BIT0);
	//combine tap2 & tap3
	//grip_tap3_enable_func(val);
}

void grip_tap3_enable_func(int val){
	int id = 2;
	grip_tap_enable_func(id, val, &TAP2_BIT0);
        PRINT_INFO("Write tap1 reg: %x", TAP2_BIT0);
}

void grip_tap_force_func(int tap_id, int val, uint16_t* reg ) {
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_FORCE[tap_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	val = val << 8;
	*reg = (val & 0xFF00) | ( *reg & 0x00FF);
	set_tap_gesture(tap_id, *reg, 0);
	//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
	/* check bar scan behavior */
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_tapX_force_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_force_func(id, val, reg_val);
       PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}
void grip_tap1_force_func(int val){
	int id = 0;
	grip_tap_force_func(id, val, &TAP0_BIT0);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT0);
}

void grip_tap2_force_func(int val){
	int id = 1;
	grip_tap_force_func(id, val, &TAP1_BIT0);
        PRINT_INFO("Write tap2 reg: %x", TAP1_BIT0);
	//combin tap2&tap3
	//grip_tap3_force_func(val);
}

void grip_tap3_force_func(int val){
	int id = 2;
	grip_tap_force_func(id, val, &TAP2_BIT0);
        PRINT_INFO("Write tap3 reg: %x", TAP2_BIT0);
}

void grip_tap_min_position_func(int tap_id, int val, uint16_t* reg ) {
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_MIN_POS[tap_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val;
	set_tap_gesture(tap_id, *reg, 2);
	//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
	/* check bar scan behavior */
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_tapX_min_position_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_min_position_func(id, val, reg_val);
       PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}
void grip_tap1_min_position_func(int val){
	int id = 0;
	grip_tap_min_position_func(id, val, &TAP0_BIT2);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT2);
}

void grip_tap2_min_position_func(int val){
	int id = 1;
	grip_tap_min_position_func(id, val, &TAP1_BIT2);
        PRINT_INFO("Write tap2 reg: %x", TAP1_BIT2);
	//combin tap2&tap3
	//grip_tap3_min_position_func(val);
}

void grip_tap3_min_position_func(int val){
	int id = 2;
	grip_tap_min_position_func(id, val, &TAP2_BIT2);
        PRINT_INFO("Write tap3 reg: %x", TAP2_BIT2);
}

void grip_tap_max_position_func(int tap_id, int val, uint16_t* reg ) {
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_MAX_POS[tap_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val;
	set_tap_gesture(tap_id, *reg, 3);
	//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_tapX_max_position_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_max_position_func(id, val, reg_val);
       PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}
void grip_tap1_max_position_func(int val){
	int id = 0;
	grip_tap_max_position_func(id, val, &TAP0_BIT3);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT3);
}

void grip_tap2_max_position_func(int val){
	int id = 1;
	grip_tap_max_position_func(id, val, &TAP1_BIT3);
        PRINT_INFO("Write tap2 reg: %x", TAP1_BIT3);
	//combin tap2&tap3
	//grip_tap3_max_position_func(val);
}

void grip_tap3_max_position_func(int val){
	int id = 2;
	grip_tap_max_position_func(id, val, &TAP2_BIT3);
        PRINT_INFO("Write tap3 reg: %x", TAP2_BIT3);
}

void grip_tap_sense_enable_func(int val){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_SENSE_SET= val;

	if(Grip_Check_FW_Status()){return;}

	Wait_Wake_For_RegW();
	grip_check_DPC_and_sensitivity_func();
	mutex_unlock(&snt8100fsr_g->ap_lock);
}
void grip_tap_slope_window_func(int tap_id, int val, uint16_t* reg ) {
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_SLOPE_WINDOW[tap_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val | (*reg & 0xFF00);
	set_tap_gesture(tap_id, *reg, 1);
	//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_tapX_slope_window_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_slope_window_func(id, val, reg_val);
       PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_tap1_slope_window_func(int val){
	int id = 0;
	grip_tap_slope_window_func(id, val, &TAP0_BIT1);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT1);
}

void grip_tap3_slope_window_func(int val){
	int id = 2;
	grip_tap_slope_window_func(id, val, &TAP2_BIT1);
        PRINT_INFO("Write tap3 reg: %x", TAP2_BIT1);
}

void grip_tap2_slope_window_func(int val){
	int id = 1;
	grip_tap_slope_window_func(id, val, &TAP1_BIT1);
        PRINT_INFO("Write tap2 reg: %x", TAP1_BIT1);
	//combin tap2&tap3
	//grip_tap3_slope_window_func(val);
}

void grip_tap_slope_release_force_func(int tap_id, int val, uint16_t* reg ) {
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_SLOPE_RELEASE_FORCE[tap_id] = val;

	if(Grip_Check_FW_Status()){return;}

	Wait_Wake_For_RegW();
	*reg = val<<8 | (*reg & 0x00FF);
	set_tap_gesture(tap_id, *reg, 5);
	//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_tapX_slope_release_force_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_slope_release_force_func(id, val, reg_val);
       PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_tap1_slope_release_force_func(int val){
	int id = 0;
	grip_tap_slope_release_force_func(id, val, &TAP0_BIT5);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT5);
}

void grip_tap3_slope_release_force_func(int val){
	int id = 2;
	grip_tap_slope_release_force_func(id, val, &TAP2_BIT5);
        PRINT_INFO("Write tap3 reg: %x", TAP2_BIT5);
}

void grip_tap2_slope_release_force_func(int val){
	int id = 1;
	grip_tap_slope_release_force_func(id, val, &TAP1_BIT5);
        PRINT_INFO("Write tap2 reg: %x", TAP1_BIT5);
	//combin tap2&tap3
	//grip_tap3_slope_release_force_func(val);
}

void grip_tap_slope_tap_force_func(int tap_id, int val, uint16_t* reg ) {
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_SLOPE_TAP_FORCE[tap_id] = val;
	

	if(Grip_Check_FW_Status()){return;}

	Wait_Wake_For_RegW();
	*reg = val | (*reg & 0xFF00);
	set_tap_gesture(tap_id, *reg, 5);
	//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_tapX_slope_tap_force_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_slope_tap_force_func(id, val, reg_val);
       PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_tap1_slope_tap_force_func(int val){
	int id = 0;
	grip_tap_slope_tap_force_func(id, val, &TAP0_BIT5);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT5);
}

void grip_tap3_slope_tap_force_func(int val){
	int id = 2;
	grip_tap_slope_tap_force_func(id, val, &TAP2_BIT5);
        PRINT_INFO("Write tap3 reg: %x", TAP2_BIT5);
}

void grip_tap2_slope_tap_force_func(int val){
	int id = 1;
	grip_tap_slope_tap_force_func(id, val, &TAP1_BIT5);
        PRINT_INFO("Write tap2 reg: %x", TAP1_BIT5);
	//combin tap2&tap3
	//grip_tap3_slope_tap_force_func(val);
}

void grip_tap_delta_tap_force_func(int tap_id, int val, uint16_t* reg ) {
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_DELTA_TAP_FORCE[tap_id] = val;
	
	if(Grip_Check_FW_Status()){return;}

	Wait_Wake_For_RegW();
	*reg = val | (*reg & 0xFF00);
	set_tap_gesture(tap_id, *reg, 4);
	//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_tapX_delta_tap_force_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_delta_tap_force_func(id, val, reg_val);
       PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_tap1_delta_tap_force_func(int val){
	int id = 0;
	grip_tap_delta_tap_force_func(id, val, &TAP0_BIT4);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT4);
}

void grip_tap3_delta_tap_force_func(int val){
	int id = 2;
	grip_tap_delta_tap_force_func(id, val, &TAP2_BIT4);
        PRINT_INFO("Write tap3 reg: %x", TAP2_BIT4);
}

void grip_tap2_delta_tap_force_func(int val){
	int id = 1;
	grip_tap_delta_tap_force_func(id, val, &TAP1_BIT4);
        PRINT_INFO("Write tap2 reg: %x", TAP1_BIT4);
	//combin tap2&tap3
	//grip_tap3_delta_tap_force_func(val);
}

void grip_tap_delta_release_force_func(int tap_id, int val, uint16_t* reg ) {
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP_DELTA_RELEASE_FORCE[tap_id] = val;
	
	if(Grip_Check_FW_Status()){return;}

	Wait_Wake_For_RegW();
	*reg = val<<8  | (*reg & 0x00FF);
	set_tap_gesture(tap_id, *reg, 4);
	//get_tap_gesture(0, *reg, 0, tap_id*8 +9);
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_tapX_delta_release_force_func(int val, int id, uint16_t *reg_val ){
	
	//int id = 0;
	grip_tap_delta_release_force_func(id, val, reg_val);
        PRINT_INFO("Write tap id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_tap1_delta_release_force_func(int val){
	int id = 0;
	grip_tap_delta_release_force_func(id, val, &TAP0_BIT4);
        PRINT_INFO("Write tap1 reg: %x", TAP0_BIT4);
}

void grip_tap3_delta_release_force_func(int val){
	int id = 2;
	grip_tap_delta_release_force_func(id, val, &TAP2_BIT4);
        PRINT_INFO("Write tap3 reg: %x", TAP2_BIT4);
}

void grip_tap2_delta_release_force_func(int val){
	int id = 1;
	grip_tap_delta_release_force_func(id, val, &TAP1_BIT4);
        PRINT_INFO("Write tap2 reg: %x", TAP1_BIT4);
	//combin tap2&tap3
	//grip_tap3_delta_release_force_func(val);
}

/* ASUS BSP Clay: setting vibrator trig1 & trig2 register for different tap modes +++ */
static void gripVibratorSetting(int val, int tap_id){
	static uint16_t reg_val[4] = { 0x1, 0x2, 0x8, 0x20 };
	PRINT_INFO("Tap_id=%d, EN=%d, VIB_EN=%d, 0x%x, 0x%x", 
		tap_id,	grip_status_g->G_TAP_EN[tap_id],
		grip_status_g->G_TAP_VIB_EN[tap_id],
		G_grip_tap_vib1_reg, G_grip_tap_vib2_reg);
	
	if(g_ASUS_hwID >= HW_REV_PR){
		if(val && grip_status_g->G_TAP_EN[tap_id]==1 && grip_status_g->G_TAP_VIB_EN[tap_id]==1){
			if(tap_id == 1){
				if(grip_status_g->G_TAP_EN[tap_id]==1 && grip_status_g->G_TAP_EN[tap_id+2]==1){
					PRINT_INFO("Double tap case");
					/* tap2 */
					G_grip_tap_vib1_reg = G_grip_tap_vib1_reg & (0xffff - reg_val[tap_id]);
					G_grip_tap_vib2_reg = G_grip_tap_vib2_reg | reg_val[tap_id];
				}else{
					G_grip_tap_vib1_reg = G_grip_tap_vib1_reg | reg_val[tap_id];
					G_grip_tap_vib2_reg = G_grip_tap_vib2_reg & (0xffff - reg_val[tap_id]);
				}
			}else if(tap_id == 3){
				if(grip_status_g->G_TAP_EN[tap_id-2]==1	&& grip_status_g->G_TAP_VIB_EN[tap_id-2]==1){
					PRINT_INFO("Double tap case");
					/* tap2 */
					G_grip_tap_vib1_reg = G_grip_tap_vib1_reg & (0xffff - reg_val[tap_id-2]);
					G_grip_tap_vib2_reg = G_grip_tap_vib2_reg | reg_val[tap_id-2];
				}
				G_grip_tap_vib1_reg = G_grip_tap_vib1_reg | reg_val[tap_id];
			}else if(tap_id == 2){
				G_grip_tap_vib2_reg = G_grip_tap_vib2_reg | reg_val[tap_id];
			}else{
				G_grip_tap_vib1_reg = G_grip_tap_vib1_reg | reg_val[tap_id];
			}			
		}else{
			G_grip_tap_vib1_reg = G_grip_tap_vib1_reg & (0xffff - reg_val[tap_id]);
			G_grip_tap_vib2_reg = G_grip_tap_vib2_reg & (0xffff - reg_val[tap_id]);
			//tap3 off or tap3_vib off
			//recovery case: tap2 on and tap2_vib on 
			if(tap_id==3 && grip_status_g->G_TAP_EN[tap_id-2]==1 
				&& grip_status_g->G_TAP_VIB_EN[tap_id-2]==1){
				PRINT_INFO("Double tap to One tap");
				G_grip_tap_vib1_reg = G_grip_tap_vib1_reg | reg_val[tap_id-2];
				G_grip_tap_vib2_reg = G_grip_tap_vib2_reg & (0xffff - reg_val[tap_id-2]);
			}
		}
	}else{
		if(val && grip_status_g->G_TAP_EN[tap_id]==1 && grip_status_g->G_TAP_VIB_EN[tap_id]==1){
			G_grip_tap_vib1_reg = G_grip_tap_vib1_reg | reg_val[tap_id];
		}else{
			G_grip_tap_vib1_reg = G_grip_tap_vib1_reg & (0xffff - reg_val[tap_id]);
		}
	}
	Wait_Wake_For_RegW();
	write_register(snt8100fsr_g, REGISTER_TIRGGER_LINK1, &G_grip_tap_vib1_reg);
	write_register(snt8100fsr_g, REGISTER_TIRGGER_LINK2, &G_grip_tap_vib2_reg);
	PRINT_INFO("[0x%x] = 0x%x, [0x%x] = 0x%x", 
		REGISTER_TIRGGER_LINK1, G_grip_tap_vib1_reg,
		REGISTER_TIRGGER_LINK2, G_grip_tap_vib2_reg);
}
/* ASUS BSP--- */
void grip_tapX_vibrator_enable_func(int val, int tap_id){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	PRINT_INFO("val = %d, tap_id=%d", val, tap_id);
	grip_status_g->G_TAP_VIB_EN[tap_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	/* ASUS BSP Clay: setting vibrator trig1 & trig2 register for different tap modes +++ */
	gripVibratorSetting(val, tap_id);
	/* ASUS BSP--- */
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_tap1_vibrator_enable_func(int val){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP1_VIB_EN = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	/* 0x1a Bit 1 overwrite */
	if(val == 0)
		G_grip_tap_vib1_reg = G_grip_tap_vib1_reg & 0xFFFE;
	else 
		G_grip_tap_vib1_reg = G_grip_tap_vib1_reg | 0x1;
	
	write_register(snt8100fsr_g, REGISTER_TIRGGER_LINK1, &G_grip_tap_vib1_reg);	
	PRINT_INFO("addr[0x%x] = 0x%x", REGISTER_TIRGGER_LINK1, G_grip_tap_vib1_reg);
	mutex_unlock(&snt8100fsr_g->ap_lock);		
}

void grip_tap2_vibrator_enable_func(int val){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP2_VIB_EN = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	/* 0x1a Bit 2 overwrite */
	if(val == 0)
		G_grip_tap_vib1_reg = G_grip_tap_vib1_reg & 0xFFFD;
	else 
		G_grip_tap_vib1_reg = G_grip_tap_vib1_reg | 0x2;
	
	write_register(snt8100fsr_g, REGISTER_TIRGGER_LINK1, &G_grip_tap_vib1_reg);
	PRINT_INFO("addr[0x%x] = 0x%x", REGISTER_TIRGGER_LINK1, G_grip_tap_vib1_reg);
	/*
	if(val == 0){
		if(g_ASUS_hwID < ZS660KL_ER2)
			write_register(snt8100fsr_g, REGISTER_TIRGGER_LINK, &val);
		else
			set_tap_gesture(2, val, 1);
	}else{
		if(g_ASUS_hwID < ZS660KL_ER2)
			write_register(snt8100fsr_g, REGISTER_TIRGGER_LINK, &G_grip_tap_vib1_reg);
		else
			set_tap_gesture(2, TAP2_BIT1, 1);
	}
	*/
	mutex_unlock(&snt8100fsr_g->ap_lock);		
}

static const uint16_t TAP1_SLOPE_FORCE=0x0205;
static const uint16_t TAP1_DELTA_FORCE=0x0205;
static const uint16_t TAP2_SLOPE_FORCE=0x0205;
static const uint16_t TAP2_DELTA_FORCE=0x0205;
void grip_tap1_finger_reseting_enable_func(int val){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP1_REST_EN = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	if(val == 0){
		set_tap_gesture(0, TAP0_BIT4, 4);
		set_tap_gesture(0, TAP0_BIT5, 5);
	}else{
		set_tap_gesture(0, TAP1_SLOPE_FORCE, 4);
		set_tap_gesture(0, TAP1_DELTA_FORCE, 5);
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);		
}

void grip_tap2_finger_reseting_enable_func(int val){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_TAP2_REST_EN = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	if(val == 0){
		set_tap_gesture(1, TAP1_BIT4, 4);
		set_tap_gesture(1, TAP1_BIT5, 5);
		set_tap_gesture(2, TAP2_BIT4, 4);
		set_tap_gesture(2, TAP2_BIT5, 5);
	}else{
		set_tap_gesture(1, TAP2_SLOPE_FORCE, 4);
		set_tap_gesture(1, TAP2_DELTA_FORCE, 5);
		set_tap_gesture(2, TAP2_SLOPE_FORCE, 4);
		set_tap_gesture(2, TAP2_DELTA_FORCE, 5);
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);		
}

//Tap part down===========================================

//Squeeze part start===========================================
void get_sq_gesture(uint16_t tap_id, uint16_t reg_val, int index, int len){
    //int ret=0;
    //int bytes=0;
    int i=0;
  uint16_t cfg_bank_read[3] = { 0, 0, 0x0a01};
  uint16_t buf[255];
  write_registers_fifo(0x2c, 3, cfg_bank_read);
  read_registers_fifo(0x200, len, buf);
  
  for(i = 0; i < len; i++){
	PRINT_INFO("reg_val=0x%x", buf[i]);
  }
}

void set_sq_gesture(uint16_t sq_id, uint16_t reg_val, int index){
  uint16_t cfg_bank_write[3] = { sq_id*squeeze_reg_num + index * 2, 2, 0x0a02};
  uint16_t cfg_bank_commit[3] = { 0, 0, 0x0a03};
  write_registers_fifo(0x2c, 3, cfg_bank_write);
  write_registers_fifo(0x200, 1, &reg_val);
  write_registers_fifo(0x2c, 3, cfg_bank_commit);
}
void grip_squeeze_enable_func(int sq_id, int val, uint16_t* reg){
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);

	if( val==1) {
		grip_enable_func_noLock(1);
	}
	
	grip_status_g->G_SQUEEZE_EN[sq_id] = val;
	if(grip_status_g->G_EN == -1)
		grip_status_g->G_EN = 0;
	
	/* when G_EN=0 which means that all gestures close, don't wakeup chip and do setting */
	if(grip_status_g->G_EN == 1){
		
		if(Grip_Check_FW_Status()){return;}
		
		Wait_Wake_For_RegW();
		val = val << 15;
		* reg = (val & 0x8000) | (* reg & 0x7FFF);
		set_sq_gesture(sq_id, * reg, 0);
		//get_sq_gesture(0, * reg, 0, 2);

		if( val==0) { //turn off, check if (all off) goto low power
			grip_checkToLowPower_noLock();
		}
	}else{
		PRINT_INFO("No gesture enable, skip it");
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_squeezeX_enable_func(int val, int id, uint16_t *reg_val ){
	grip_squeeze_enable_func(id, val, reg_val);
    PRINT_INFO("Write sq id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_squeeze1_enable_func(int val){
	int id=0;
	grip_squeeze_enable_func(id, val, &SQ1_BIT0);
        PRINT_INFO("Write sq1 reg: %x", SQ1_BIT0);
}

void grip_squeeze2_enable_func(int val){
	int id=1;
	grip_squeeze_enable_func(id, val, &SQ2_BIT0);
        PRINT_INFO("Write sq2 reg: %x", SQ2_BIT0);
}
	
void grip_squeeze_force_func(int sq_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	
	grip_status_g->G_SQUEEZE_FORCE[sq_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = (val & 0x00FF) | (*reg & 0xFF00);
	set_sq_gesture(sq_id, * reg, 0);
	//get_sq_gesture(0, * reg, 0, 2);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_squeezeX_force_func(int val, int id, uint16_t *reg_val ){
	grip_squeeze_force_func(id, val, reg_val);
        PRINT_INFO("Write sq id=%d, val=%d, reg: %x", id, val, *reg_val);
}
void grip_squeeze1_force_func(int val){
	int id=0;
	grip_squeeze_force_func(id, val, &SQ1_BIT0);
        PRINT_INFO("Write sq1_B0 reg: %x", SQ1_BIT0);
}
void grip_squeeze2_force_func(int val){
	int id=1;
	grip_squeeze_force_func(id, val, &SQ2_BIT0);
	PRINT_INFO("Write sq2_B0 reg: %x", SQ2_BIT0);
}

bool G_Skip_Sq1_Long = 0;
bool G_Skip_Sq2_Long = 0;
	
void grip_squeeze_short_dur_func(int sq_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	
	grip_status_g->G_SQUEEZE_SHORT[sq_id]= val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = ((val/20) << 8) | (*reg & 0x00FF);
	set_sq_gesture(sq_id, *reg, 5);
	//get_sq_gesture(0, * reg, 0, 2);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_squeezeX_short_dur_func(int val, int id, uint16_t *reg_val ){
	grip_squeeze_short_dur_func(id, val, reg_val);
        PRINT_INFO("Write sq id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_squeeze1_short_dur_func(int val){
	int id=0;
	grip_squeeze_short_dur_func(id, val, &SQ1_BIT5);
        PRINT_INFO("Write sq1_B5 reg: %x", SQ1_BIT5);
}
	
void grip_squeeze2_short_dur_func(int val){
	int id=1;
	grip_squeeze_short_dur_func(id, val, &SQ2_BIT5);
	PRINT_INFO("Write sq2_B5 reg: %x", SQ2_BIT5);
}

void grip_squeeze_long_dur_func(int sq_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	
	if(sq_id == 0){
		grip_status_g->G_SQUEEZE_LONG[sq_id]= val;
		if(val == 0){
			G_Skip_Sq1_Long = 1;
			mutex_unlock(&snt8100fsr_g->ap_lock);
			return;
		}else
			G_Skip_Sq1_Long = 0;
	}else if(sq_id ==1){
		grip_status_g->G_SQUEEZE_LONG[sq_id] = val;
		if(val == 0){
			G_Skip_Sq2_Long = 1;
			mutex_unlock(&snt8100fsr_g->ap_lock);
			return;
		}else
			G_Skip_Sq2_Long = 0;
	}
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = (val/20) | (*reg & 0xFF00);
	set_sq_gesture(sq_id, *reg, 5);
	//get_sq_gesture(0, *reg, 0, 6);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_squeezeX_long_dur_func(int val, int id, uint16_t *reg_val ){
	grip_squeeze_long_dur_func(id, val, reg_val);
        PRINT_INFO("Write sq id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_squeeze1_long_dur_func(int val){
	int id=0;
	grip_squeeze_long_dur_func(id, val, &SQ1_BIT5);
        PRINT_INFO("Write sq1_B5 reg: %x", SQ1_BIT5);
}
	
void grip_squeeze2_long_dur_func(int val){
	int id=1;
	grip_squeeze_long_dur_func(id, val, &SQ2_BIT5);
	PRINT_INFO("Write sq2_B5 reg: %x", SQ2_BIT5);
}

void grip_squeeze_up_rate_func(int sq_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SQUEEZE_UP_RATE[sq_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val<<8  | (*reg & 0x00FF);
	set_sq_gesture(sq_id, *reg, 6);
	PRINT_INFO("Write Squeeze_up_rate: 0x%x", * reg);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_squeezeX_up_rate_func(int val, int id, uint16_t *reg_val ){
	grip_squeeze_up_rate_func(id, val, reg_val);
        PRINT_INFO("Write sq id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_squeeze1_up_rate_func(int val){
	int id=0;
	grip_squeeze_up_rate_func(id, val, &SQ1_BIT6);
        PRINT_INFO("Write sq1_B6 reg: %x", SQ1_BIT6);
}
	
void grip_squeeze2_up_rate_func(int val){
	int id=1;
	grip_squeeze_up_rate_func(id, val, &SQ2_BIT6);
	PRINT_INFO("Write sq2_B6 reg: %x", SQ2_BIT6);
}

void grip_squeeze_up_total_func(int sq_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SQUEEZE_UP_TOTAL[sq_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val  | (*reg & 0xFF00);
	set_sq_gesture(sq_id, *reg, 6);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_squeezeX_up_total_func(int val, int id, uint16_t *reg_val ){
	grip_squeeze_up_total_func(id, val, reg_val);
        PRINT_INFO("Write sq id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_squeeze1_up_total_func(int val){
	int id=0;
	grip_squeeze_up_total_func(id, val, &SQ1_BIT6);
        PRINT_INFO("Write sq1_B6 reg: %x", SQ1_BIT6);
}
	
void grip_squeeze2_up_total_func(int val){
	int id=1;
	grip_squeeze_up_total_func(id, val, &SQ2_BIT6);
	PRINT_INFO("Write sq2_B6 reg: %x", SQ2_BIT6);
}

void grip_squeeze_drop_rate_func(int sq_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SQUEEZE_DROP_RATE[sq_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val<<8  | (*reg & 0x00FF);
	set_sq_gesture(sq_id, *reg, 7);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_squeezeX_drop_rate_func(int val, int id, uint16_t *reg_val ){
	grip_squeeze_drop_rate_func(id, val, reg_val);
        PRINT_INFO("Write sq id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_squeeze1_drop_rate_func(int val){
	int id=0;
	grip_squeeze_drop_rate_func(id, val, &SQ1_BIT7);
        PRINT_INFO("Write sq1_B7 reg: %x", SQ1_BIT7);
}
	
void grip_squeeze2_drop_rate_func(int val){
	int id=1;
	grip_squeeze_drop_rate_func(id, val, &SQ2_BIT7);
	PRINT_INFO("Write sq2_B7 reg: %x", SQ2_BIT7);
}

void grip_squeeze_drop_total_func(int sq_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SQUEEZE_DROP_TOTAL[sq_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val  | (*reg & 0xFF00);
	set_sq_gesture(sq_id, *reg, 7);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_squeezeX_drop_total_func(int val, int id, uint16_t *reg_val ){
	grip_squeeze_drop_total_func(id, val, reg_val);
        PRINT_INFO("Write sq id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_squeeze1_drop_total_func(int val){
	int id=0;
	grip_squeeze_drop_total_func(id, val, &SQ1_BIT7);
        PRINT_INFO("Write sq1_B7 reg: %x", SQ1_BIT7);
}
	
void grip_squeeze2_drop_total_func(int val){
	int id=1;
	grip_squeeze_drop_total_func(id, val, &SQ2_BIT7);
	PRINT_INFO("Write sq2_B7 reg: %x", SQ2_BIT7);
}

//Squeeze part down===========================================

/******************  Slide  ********************************/
void get_slide_gesture(uint16_t tap_id, uint16_t reg_val, int index, int len){
    //int ret=0;
    //int bytes=0;
    int i=0;
  uint16_t cfg_bank_read[3] = { 0, 0, 0x0b01};
  uint16_t buf[255];
  write_registers_fifo(0x2c, 3, cfg_bank_read);
  read_registers_fifo(0x200, len, buf);
  
  for(i = 0; i < len; i++){
	PRINT_INFO("reg_val=0x%x", buf[i]);
  }
}
void set_slide_gesture(uint16_t slide_id, uint16_t reg_val, int index){
  uint16_t cfg_bank_write[3] = { slide_id*slide_reg_num + index * 2, 2, 0x0b02};
  uint16_t cfg_bank_commit[3] = { 0, 0, 0x0b03};
  write_registers_fifo(0x2c, 3, cfg_bank_write);
  write_registers_fifo(0x200, 1, &reg_val);
  write_registers_fifo(0x2c, 3, cfg_bank_commit);
}

void grip_slide_enable_func(int slide_id, int val, uint16_t* reg){
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);

	if( val==1) {
		grip_enable_func_noLock(1);
	}
	
	grip_status_g->G_SLIDE_EN[slide_id] = val;
	if(grip_status_g->G_EN == -1)
		grip_status_g->G_EN = 0;
	
	/* when G_EN=0 which means that all gestures close, don't wakeup chip and do setting */
	if(grip_status_g->G_EN == 1){		
		
		if(Grip_Check_FW_Status()){return;}
		
		Wait_Wake_For_RegW();
		val = val << 15;
		* reg = (val & 0x8000) | (* reg & 0x7FFF);
		set_slide_gesture(slide_id, *reg, 0);
		grip_check_DPC_and_sensitivity_func();
		grip_slide_swipe_status_check();

		if( val==0) { //turn off, check if (all off) goto low power
			grip_checkToLowPower_noLock();
		}
	}else{
		PRINT_INFO("No gesture enable, skip it");
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_slideX_enable_func(int val, int id, uint16_t *reg_val){
	grip_slide_enable_func(id, val, reg_val);
	PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_slide1_enable_func(int val){
	grip_slide_enable_func(0, val, &SLIDE0_BIT0);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SLIDE0_BIT0);
}
void grip_slide2_enable_func(int val){
	grip_slide_enable_func(1, val, &SLIDE1_BIT0);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SLIDE1_BIT0);
}

void grip_slide_dist_func(int slide_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SLIDE_DIST[slide_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	val = val << 8;
	*reg = (val & 0xFF00) | (*reg & 0x00FF);
	set_slide_gesture(slide_id, *reg, 3);
	PRINT_INFO("Write Slide1_Dist: 0x%x", *reg);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_slideX_dist_func(int val, int id, uint16_t *reg_val){
	grip_slide_dist_func(id, val, reg_val);
	PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_slide1_dist_func(int val){
	grip_slide_dist_func(0, val, &SLIDE0_BIT3);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SLIDE0_BIT3);
}

void grip_slide2_dist_func(int val){
	grip_slide_dist_func(1, val, &SLIDE1_BIT3);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SLIDE1_BIT3);
}

void grip_slide_force_func(int slide_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SLIDE_FORCE[slide_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = (val & 0x00FF) | (*reg & 0xFF00);
	set_slide_gesture(slide_id, *reg, 3);
	PRINT_INFO("Write Slide1_Force: 0x%x", *reg);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_slideX_force_func(int val, int id, uint16_t *reg_val){
	grip_slide_force_func(id, val, reg_val);
	PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_slide1_force_func(int val){
	grip_slide_force_func(0, val, &SLIDE0_BIT3);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SLIDE0_BIT3);
}

void grip_slide2_force_func(int val){
	grip_slide_force_func(1, val, &SLIDE1_BIT3);
        PRINT_INFO("Write slide id=1, val=%d, reg:%x", val, SLIDE1_BIT3);
}

void grip_slide_2nd_dist_func(int slide_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SLIDE_2ND_DIST[slide_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = (val & 0x00FF) | (*reg & 0xFF00);
	set_slide_gesture(slide_id, *reg, 4);
	PRINT_INFO("Write Slide1_Force: 0x%x", *reg);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_slideX_2nd_dist_func(int val, int id, uint16_t *reg_val){
	grip_slide_2nd_dist_func(id, val, reg_val);
        PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_slide1_2nd_dist_func(int val){
	grip_slide_2nd_dist_func(0, val, &SLIDE0_BIT4);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SLIDE0_BIT4);
}

void grip_slide2_2nd_dist_func(int val){
	grip_slide_2nd_dist_func(1, val, &SLIDE1_BIT4);
        PRINT_INFO("Write slide id=1, val=%d, reg:%x", val, SLIDE1_BIT4);
}

void grip_slide_vibrator_enable_func(int slide_id, int val){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SLIDE_VIB_EN[slide_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	PRINT_INFO("SLIDE1 Do nothing");
	mutex_unlock(&snt8100fsr_g->ap_lock);	
}

void grip_slideX_vibrator_enable_func(int val, int id){
	grip_slide_vibrator_enable_func(id, val);
        PRINT_INFO("Write slide id=%d, val=%d", id, val);
}

void grip_slide1_vibrator_enable_func(int val){
	grip_slide_vibrator_enable_func(0, val);
        PRINT_INFO("Write slide id=0, val=%d", val);
}

void grip_slide2_vibrator_enable_func(int val){
	grip_slide_vibrator_enable_func(1, val);
        PRINT_INFO("Write slide id=1, val=%d", val);
}

void grip_slide_min_position_func(int slide_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SLIDE_MIN_POS[slide_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val;
	set_slide_gesture(slide_id, *reg, 1);
	PRINT_INFO("Write Slide1_Force: 0x%x", *reg);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_slideX_min_position_func(int val, int id, uint16_t *reg_val){
	grip_slide_min_position_func(id, val, reg_val);
        PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_slide1_min_position_func(int val){
	grip_slide_min_position_func(0, val, &SLIDE0_BIT1);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SLIDE0_BIT1);
}

void grip_slide2_min_position_func(int val){
	grip_slide_min_position_func(1, val, &SLIDE1_BIT1);
        PRINT_INFO("Write slide id=1, val=%d, reg:%x", val, SLIDE1_BIT1);
}

void grip_slide_max_position_func(int slide_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SLIDE_MAX_POS[slide_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val;
	set_slide_gesture(slide_id, *reg, 2);
	PRINT_INFO("Write Slide1_Force: 0x%x", *reg);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_slideX_max_position_func(int val, int id, uint16_t *reg_val){
	grip_slide_max_position_func(id, val, reg_val);
        PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_slide1_max_position_func(int val){
	grip_slide_max_position_func(0, val, &SLIDE0_BIT2);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SLIDE0_BIT2);
}

void grip_slide2_max_position_func(int val){
	grip_slide_max_position_func(1, val, &SLIDE1_BIT2);
        PRINT_INFO("Write slide id=1, val=%d, reg:%x", val, SLIDE1_BIT2);
}
/******************  SWIPE  ********************************//******************  SWIPE  ********************************/
void get_swipe_gesture(uint16_t swipe_id, uint16_t reg_val, int index, int len){
    //int ret=0;
    //int bytes=0;
    int i=0;
  uint16_t cfg_bank_read[3] = { 0, 0, 0x0901};
  uint16_t buf[255];
  write_registers_fifo(0x2c, 3, cfg_bank_read);
  read_registers_fifo(0x200, len, buf);
  
  for(i = 0; i < len; i++){
	PRINT_INFO("reg_val=0x%x", buf[i]);
  }
}

void set_swipe_gesture(uint16_t swipe_id, uint16_t reg_val, int index){
  uint16_t cfg_bank_write[3] = { swipe_id*swipe_reg_num + index * 2, 2, 0x0902};
  uint16_t cfg_bank_commit[3] = { 0, 0, 0x0903};
  write_registers_fifo(0x2c, 3, cfg_bank_write);
  write_registers_fifo(0x200, 1, &reg_val);
  write_registers_fifo(0x2c, 3, cfg_bank_commit);
}

void grip_swipe_enable_func(int swipe_id, int val, uint16_t* reg){
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	
	if( val==1) {
		grip_enable_func_noLock(1);
	}
	
	grip_status_g->G_SWIPE_EN[swipe_id] = val;
	if(grip_status_g->G_EN == -1)
		grip_status_g->G_EN = 0;
	
	/* when G_EN=0 which means that all gestures close, don't wakeup chip and do setting */
	if(grip_status_g->G_EN == 1){
		
		if(Grip_Check_FW_Status()){return;}
		
		Wait_Wake_For_RegW();
		val = val << 15;
		* reg = (val & 0x8000) | (* reg & 0x7FFF);
		set_swipe_gesture(swipe_id, *reg, 0);
		 PRINT_INFO("Write Swipe1_En: 0x%x", *reg);
		grip_check_DPC_and_sensitivity_func();
		grip_slide_swipe_status_check();

		if( val==0) { //turn off, check if (all off) goto low power
			grip_checkToLowPower_noLock();
		}
	}else{
		PRINT_INFO("No gesture enable, skip it");
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_swipeX_enable_func(int val, int id, uint16_t *reg_val){
	grip_swipe_enable_func(id, val, reg_val);
	PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_swipe1_enable_func(int val){
	grip_swipe_enable_func(0, val, &SWIPE1_BIT0);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE1_BIT0);
}

void grip_swipe2_enable_func(int val){
	grip_swipe_enable_func(1, val, &SWIPE2_BIT0);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE2_BIT0);
}
void grip_swipe_velocity_func(int swipe_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SWIPE_VELOCITY[swipe_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	* reg = (val & 0x00FF) | (* reg & 0xFF00);
	set_swipe_gesture(swipe_id, * reg, 0);
	 PRINT_INFO("Write Swipe_Velocity[%d]: 0x%x", swipe_id, *reg);
	 mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_swipeX_velocity_func(int val, int id, uint16_t *reg_val){
	grip_swipe_velocity_func(id, val, reg_val);
	PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_swipe1_velocity_func(int val){
	grip_swipe_velocity_func(0, val, &SWIPE1_BIT0);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE1_BIT0);
}

void grip_swipe2_velocity_func(int val){
	grip_swipe_velocity_func(1, val, &SWIPE2_BIT0);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE2_BIT0);
}

void grip_swipe_len_func(int swipe_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SWIPE_LEN[swipe_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val;
	set_swipe_gesture(swipe_id, *reg, 3);
	PRINT_INFO("Write Swipe_Len[%d]: 0x%x", swipe_id, *reg);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_swipeX_len_func(int val, int id, uint16_t *reg_val){
	grip_swipe_len_func(id, val, reg_val);
	PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_swipe1_len_func(int val){
	grip_swipe_len_func(0, val, &SWIPE1_BIT3);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE1_BIT3);
}
void grip_swipe2_len_func(int val){
	grip_swipe_len_func(1, val, &SWIPE2_BIT3);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE2_BIT3);
}

void grip_swipe_min_position_func(int swipe_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SWIPE_MIN_POS[swipe_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val;
	set_swipe_gesture(swipe_id, *reg, 1);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_swipeX_min_position_func(int val, int id, uint16_t *reg_val){
	grip_swipe_min_position_func(id, val, reg_val);
	PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_swipe1_min_position_func(int val){
	grip_swipe_min_position_func(0, val, &SWIPE1_BIT1);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE1_BIT1);
}
void grip_swipe2_min_position_func(int val){
	grip_swipe_min_position_func(1, val, &SWIPE2_BIT1);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE2_BIT1);
}

void grip_swipe_max_position_func(int swipe_id, int val, uint16_t* reg){
	if(grip_status_g->G_EN <= 0){
		PRINT_INFO("Skip setting when grip off");
		return;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	grip_status_g->G_SWIPE_MAX_POS[swipe_id] = val;
	
	if(Grip_Check_FW_Status()){return;}
	
	Wait_Wake_For_RegW();
	*reg = val;
	set_swipe_gesture(swipe_id, *reg, 2);
	mutex_unlock(&snt8100fsr_g->ap_lock);
}

void grip_swipeX_max_position_func(int val, int id, uint16_t *reg_val){
	grip_swipe_max_position_func(id, val, reg_val);
	PRINT_INFO("Write slide id=%d, val=%d, reg: %x", id, val, *reg_val);
}

void grip_swipe1_max_position_func(int val){
	grip_swipe_max_position_func(0, val, &SWIPE1_BIT2);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE1_BIT2);
}
void grip_swipe2_max_position_func(int val){
	grip_swipe_max_position_func(1, val, &SWIPE2_BIT2);
        PRINT_INFO("Write slide id=0, val=%d, reg:%x", val, SWIPE2_BIT2);
}
/******************  SWIPE  ********************************/
/* When K data is applied, squeeze should re-enable to make K data work */
static void Grip_ReEnable_Squeeze_Check(void){
	int count = 0;
	int sq_num = 2;
	/* apply K data to squeeze algorithm */
	for(count=0;count < sq_num;count++){
		if(grip_status_g->G_SQUEEZE_EN[count] > 0){
			grip_squeezeX_enable_func(grip_status_g->G_SQUEEZE_EN[count], count, &SQ_BIT0[count]);
		}
	}
}

static void Grip_K_data_recovery(void){
	uint16_t K_data = 0;
	if(Grip_B1_F_value == 100 || Grip_B2_F_value == 50){
	  	PRINT_INFO("Bar1/Bar2 is default data, apply golden!");
		Grip_Apply_Golden_K();
		Read_Grip_K_data();
	}else{
		K_data =(Grip_B1_F_value << 8) | Grip_B0_F_value ;
		PRINT_INFO("Grip recovery K data, 0x%x", K_data);
		write_register(snt8100fsr_g, Grip_Golden_addr, &K_data);
	}
}

/* Recovery status after reset */
void grip_dump_status_func(struct work_struct *work){
	int count=0;
	int sq_num=2, tap_num=4, swipe_num=2, slide_num=2;
	int need_en=0;
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	PRINT_INFO("framework setting recovery");
	PRINT_INFO("EN:%d, RAW_EN:%d, DPC_EN:%d",
		grip_status_g->G_EN, grip_status_g->G_RAW_EN, 
		grip_status_g->G_DPC_STATUS);
	for(count=0;count < sq_num;count++){
		PRINT_INFO("SQ[%d], EN:%d, Force:%d, Short:%d, Long:%d",
			count, grip_status_g->G_SQUEEZE_EN[count], grip_status_g->G_SQUEEZE_FORCE[count], 
			grip_status_g->G_SQUEEZE_SHORT[count], grip_status_g->G_SQUEEZE_LONG[count]);
	}
	for(count=0;count < tap_num;count++){
		PRINT_INFO("TAP[%d], EN:%d, Force:%d, min_pos:%d, max_pos:%d",
			count, grip_status_g->G_TAP_EN[count], grip_status_g->G_TAP_FORCE[count], 
			grip_status_g->G_TAP_MIN_POS[count], grip_status_g->G_TAP_MAX_POS[count]);
		
		PRINT_INFO("slope_window:%d, slope_tap:%d, slope_release:%d, delta_tap:%d, delta_release:%d",
			grip_status_g->G_TAP_SLOPE_WINDOW[count],
			grip_status_g->G_TAP_SLOPE_TAP_FORCE[count], grip_status_g->G_TAP_SLOPE_RELEASE_FORCE[count], 
			grip_status_g->G_TAP_DELTA_TAP_FORCE[count], grip_status_g->G_TAP_DELTA_RELEASE_FORCE[count]);
	}
	for(count=0;count < slide_num;count++){
		PRINT_INFO("SLIDE[%d], EN:%d, DIST:%d, Force:%d",
			count, grip_status_g->G_SLIDE_EN[count], grip_status_g->G_SLIDE_DIST[count], 
			grip_status_g->G_SLIDE_FORCE[count]);
	}
	for(count=0;count < swipe_num;count++){
		PRINT_INFO("SWIPE[%d], EN:%d, Velocity:%d, LEN:%d",
			count, grip_status_g->G_SWIPE_EN[count], grip_status_g->G_SWIPE_VELOCITY[count], 
			grip_status_g->G_SWIPE_LEN[count]);
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);

	Grip_Driver_IRQ_EN(1);
	Wait_Wake_For_RegW();
	
	if(grip_status_g->G_EN >  0){
		grip_enable_func_noLock(grip_status_g->G_EN);
	}else{ //when en = 0, enable grip to prevent register recovery fail, and disable grip at final
		need_en = 1;
	}
	if(need_en==1){
		PRINT_INFO("Enable grip at first due to G_EN != 1");
		grip_enable_func_noLock(1);
	}
	if(grip_status_g->G_RAW_EN > 0){
		grip_raw_enable_func(grip_status_g->G_RAW_EN);
	}

	/* Squeeze Part */
	for(count=0;count < sq_num;count++){
		if(grip_status_g->G_SQUEEZE_EN[count] > 0){
			grip_squeezeX_enable_func(grip_status_g->G_SQUEEZE_EN[count], count, &SQ_BIT0[count]);
		}
		if(grip_status_g->G_SQUEEZE_SHORT[count] > 0){
			grip_squeezeX_short_dur_func(grip_status_g->G_SQUEEZE_SHORT[count], count, &SQ_BIT5[count]);
		}
		if(grip_status_g->G_SQUEEZE_LONG[count]  >  0){
			grip_squeezeX_long_dur_func(grip_status_g->G_SQUEEZE_LONG[count], count, &SQ_BIT5[count]);
		}
		if(grip_status_g->G_SQUEEZE_FORCE[count]  >  0){
			grip_squeezeX_force_func(grip_status_g->G_SQUEEZE_FORCE[count], count, &SQ_BIT0[count]);
		}

		if(grip_status_g->G_SQUEEZE_UP_RATE[count]  >  0){
			grip_squeezeX_up_rate_func(grip_status_g->G_SQUEEZE_UP_RATE[count], count, &SQ_BIT6[count]);
		}
		if(grip_status_g->G_SQUEEZE_UP_TOTAL[count]  >  0){
			grip_squeezeX_up_total_func(grip_status_g->G_SQUEEZE_UP_TOTAL[count], count, &SQ_BIT6[count]);
		}
		if(grip_status_g->G_SQUEEZE_DROP_RATE[count]  >  0){
			grip_squeezeX_drop_rate_func(grip_status_g->G_SQUEEZE_DROP_RATE[count], count, &SQ_BIT7[count]);
		}
		if(grip_status_g->G_SQUEEZE_DROP_TOTAL[count]  >  0){
			grip_squeezeX_drop_total_func(grip_status_g->G_SQUEEZE_DROP_TOTAL[count], count, &SQ_BIT7[count]);
		}			
	}

	
	/* Tap Part */
	for(count=0;count < tap_num;count++){
		if(grip_status_g->G_TAP_EN[count]  >  0){
			grip_tapX_enable_func(grip_status_g->G_TAP_EN[count], count, &TAP_BIT0[count]);
		}
		if(grip_status_g->G_TAP_FORCE[count]  >  0){
			grip_tapX_force_func(grip_status_g->G_TAP_FORCE[count], count, &TAP_BIT0[count]);
		}
		if(grip_status_g->G_TAP_MIN_POS[count] >  0){
			grip_tapX_min_position_func(grip_status_g->G_TAP_MIN_POS[count], count, &TAP_BIT2[count]);
		}
		if(grip_status_g->G_TAP_MAX_POS[count] >  0){
			grip_tapX_max_position_func(grip_status_g->G_TAP_MAX_POS[count], count, &TAP_BIT3[count]);
		}
		if(grip_status_g->G_TAP_DELTA_RELEASE_FORCE[count] >  0)
			grip_tapX_delta_release_force_func(grip_status_g->G_TAP_DELTA_RELEASE_FORCE[count], count, &TAP_BIT4[count]);
		if(grip_status_g->G_TAP_DELTA_TAP_FORCE[count] >  0)
			grip_tapX_delta_tap_force_func(grip_status_g->G_TAP_DELTA_TAP_FORCE[count], count, &TAP_BIT4[count]);
		if(grip_status_g->G_TAP_SLOPE_RELEASE_FORCE[count] >  0)
			grip_tapX_slope_release_force_func(grip_status_g->G_TAP_SLOPE_RELEASE_FORCE[count], count, &TAP_BIT5[count]);
		if(grip_status_g->G_TAP_SLOPE_TAP_FORCE[count] >  0)
			grip_tapX_slope_tap_force_func(grip_status_g->G_TAP_SLOPE_TAP_FORCE[count], count, &TAP_BIT5[count]);
		if(grip_status_g->G_TAP_SLOPE_WINDOW[count] >  0)
			grip_tapX_slope_window_func(grip_status_g->G_TAP_SLOPE_WINDOW[count], count, &TAP_BIT1[count]);
	}
	
	if(grip_status_g->G_TAP_SENSE_SET >  0){
		grip_tap_sense_enable_func(0);
		grip_tap_sense_enable_func(1);
	}

	/* Slide Part */
	for(count=0;count < slide_num;count++){
		if(grip_status_g->G_SLIDE_EN[count] >  0)
			grip_slideX_enable_func(grip_status_g->G_SLIDE_EN[count], count, &SLIDE_BIT0[count]);
		if(grip_status_g->G_SLIDE_DIST[count] >  0)
			grip_slideX_dist_func(grip_status_g->G_SLIDE_DIST[count], count, &SLIDE_BIT3[count]);
		if(grip_status_g->G_SLIDE_FORCE[count] >  0)
			grip_slideX_force_func(grip_status_g->G_SLIDE_FORCE[count], count, &SLIDE_BIT3[count]);
	}
	
	/* Swipe Part */
	for(count=0;count < swipe_num;count++){
		if(grip_status_g->G_SWIPE_EN[count] >  0)
			grip_swipeX_enable_func(grip_status_g->G_SWIPE_EN[count], count, &SWIPE_BIT0[count]);
		if(grip_status_g->G_SWIPE_VELOCITY[count] >  0)
			grip_swipeX_velocity_func(grip_status_g->G_SWIPE_VELOCITY[count], count, &SWIPE_BIT0[count]);
		if(grip_status_g->G_SWIPE_LEN[count] >  0)
			grip_swipeX_len_func(grip_status_g->G_SWIPE_LEN[count], count, &SWIPE_BIT3[count]);
	}
	
	/* Bar control, Health check and tap status*/
	PRINT_INFO("Reset check: Bar control, Health check and tap status ");
	Health_Check_Enable_No_Delay(0);
	grip_check_DPC_and_sensitivity_func();
	Grip_K_data_recovery();
	Grip_ReEnable_Squeeze_Check();
	
	if(need_en==1){
		PRINT_INFO("Disable grip at final due to G_EN != 1");
		grip_status_g->G_EN=0;
	}
	Into_DeepSleep_fun();
}

void Power_Control(int en){
	if(en == 0){
		Snt_Power_State = 0;
		PRINT_INFO("Set pinctl: 1V2 2V8 down");
		snt_set_pinctrl(snt8100fsr_g->dev, GRIP_1V2_OFF);
		snt8100fsr_g->mgrip_asus_func->grip_regulator_disable();
		PRINT_INFO("Set pinctl: RST down");
		snt_set_pinctrl(snt8100fsr_g->dev, GRIP_RST_OFF);
		msleep(50);
	}else if(en == 1){
		Snt_Power_State = 1;
		//disable vibrator trig pin
		if(aw8697_trig_control(1, 0)==0)
			PRINT_INFO("Disable vib trig1");
		else
			PRINT_INFO("Failed to disable vib trig1");
		if(aw8697_trig_control(2, 0)==0)
			PRINT_INFO("Disable vib trig2");
		else
			PRINT_INFO("Failed to disable vib trig2");
		
		PRINT_INFO("Set pinctl: RST down");
		snt_set_pinctrl(snt8100fsr_g->dev, GRIP_RST_OFF);
		msleep(50);
		PRINT_INFO("Set pinctl: 1V2 up");
		snt_set_pinctrl(snt8100fsr_g->dev, GRIP_1V2_ON);
		msleep(50);
		PRINT_INFO("Set pinctl: 2V8 up");
		snt8100fsr_g->mgrip_asus_func->grip_regulator_enable();
		msleep(50);
		PRINT_INFO("Set pinctl: RST up");
		PRINT_INFO("RST=%d", gpio_get_value(snt8100fsr_g->rst_gpio));
		snt_set_pinctrl(snt8100fsr_g->dev, GRIP_RST_ON);
	}
	
}

/*************** ASUS BSP Clay: ioctl +++ *******************/
#define ASUS_GRIP_SENSOR_DATA_SIZE 3
#define ASUS_GRIP_SENSOR_D1TEST_DATA_SIZE	784
#define ASUS_GRIP_SENSOR_NAME_SIZE 32
#define ASUS_GRIP_SENSOR_IOC_MAGIC                      ('L')///< Grip sensor ioctl magic number 
#define ASUS_GRIP_SENSOR_IOCTL_ONOFF           _IOR(ASUS_GRIP_SENSOR_IOC_MAGIC, 1, int)///< Grip sensor ioctl command - Set on/off
#define ASUS_GRIP_SENSOR_IOCTL_SET_FRAM_RATE           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 2, int)///< Grip sensor ioctl command - Set frame rate
#define ASUS_GRIP_SENSOR_IOCTL_SET_PRESSURE_THRESHOLD           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 3, int)///< Grip sensor ioctl command - Set pressure threshold
#define ASUS_GRIP_SENSOR_IOCTL_GET_DEBUG_MODE           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 4, int)///< Grip sensor ioctl command - Set Debug Mode
#define ASUS_GRIP_SENSOR_IOCTL_DATA_READ           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 5, int[ASUS_GRIP_SENSOR_DATA_SIZE])///< Grip sensor ioctl command - Data Read
#define ASUS_GRIP_SENSOR_IOCTL_MODULE_NAME           _IOR(ASUS_GRIP_SENSOR_IOC_MAGIC, 6, char[ASUS_GRIP_SENSOR_NAME_SIZE])///< GRIP sensor ioctl command - Get module name
#define ASUS_GRIP_SENSOR_IOCTL_D1TEST_DATA_READ           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 7, unsigned char[ASUS_GRIP_SENSOR_D1TEST_DATA_SIZE])	///< Grip sensor ioctl command - D1Test Data Read
#define ASUS_GRIP_SENSOR_IOCTL_BAR0_TEST           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 8, int[2])	///< Grip sensor ioctl command - Bar Test
#define ASUS_GRIP_SENSOR_IOCTL_BAR1_TEST           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 9, int[2])	///< Grip sensor ioctl command - Bar Test
#define ASUS_GRIP_SENSOR_IOCTL_BAR2_TEST           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 10, int[2])	///< Grip sensor ioctl command - Bar Test
#define ASUS_GRIP_SENSOR_IOCTL_I2C_TEST           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 11, bool)	///< Grip sensor ioctl command - I2C Test
#define ASUS_GRIP_SENSOR_IOCTL_BAR0_STATUS           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 12, bool)	///< Grip sensor ioctl command - Bar Status
#define ASUS_GRIP_SENSOR_IOCTL_BAR1_STATUS           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 13, bool)	///< Grip sensor ioctl command - Bar Status
#define ASUS_GRIP_SENSOR_IOCTL_BAR2_STATUS           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 14, bool)	///< Grip sensor ioctl command - Bar Status
#define ASUS_GRIP_SENSOR_IOCTL_BAR_TEST_FORCE           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 15, int)	///< Grip sensor ioctl command - Bar Test Force value
#define ASUS_GRIP_SENSOR_IOCTL_BAR_TEST_TOLERANCE           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 16, int)	///< Grip sensor ioctl command - Bar Test tolerance %
#define ASUS_GRIP_SENSOR_IOCTL_SWITCH_ONOFF           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 17, int)	///< Grip sensor ioctl command - Bar Test Force value
#define ASUS_GRIP_SENSOR_IOCTL_GET_ONOFF           _IOW(ASUS_GRIP_SENSOR_IOC_MAGIC, 18, bool)	///< Grip sensor ioctl command - Bar Test tolerance % 

static int SntSensor_miscOpen(struct inode *inode, struct file *file)
{
  PRINT_INFO("misc test");
#if 0
  int ret;

  if(!snt8100fsr_g){
    PRINT_CRIT("%s: null pointer, probe might not finish!", __func__);
  }
  
  PRINT_FUNC();
  // We don't mutex lock here, due to write_register locking
  
  PRINT_DEBUG("Setting frame rate to %d",
	      snt8100fsr_g->suspended_frame_rate);
  ret = write_register(snt8100fsr_g,
		       MAX_REGISTER_ADDRESS,
		       &snt8100fsr_g->suspended_frame_rate);
  if (ret) {
    PRINT_CRIT("write_register(REGISTER_FRAME_RATE) failed");
  }

  PRINT_DEBUG("done");
#endif
  return 0;
}

static int SntSensor_miscRelease(struct inode *inode, struct file *file)
{
  //int ret;
  if(!snt8100fsr_g){
    PRINT_CRIT("%s: null pointer, probe might not finish!", __func__);
  }
  PRINT_FUNC();

  /*
  // We mutex lock here since we're calling sb_wake_device which never locks
  MUTEX_LOCK(&snt8100fsr_g->sb_lock);

  ret = sb_wake_device(snt8100fsr_g);

  if (ret) {
    PRINT_CRIT("sb_wake_device() failed");
    mutex_unlock(&snt8100fsr_g->sb_lock);
    return ret;
  }

  mutex_unlock(&snt8100fsr_g->sb_lock);
  */
  PRINT_DEBUG("done");
  return 0;
}

static int d1test_size = 784;
extern struct sc_command *sc_cmd;
extern int snt_read_sc_rsp(struct snt8100fsr *snt8100fsr);
static long SntSensor_miscIoctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  int ret = 0, i = 0;
  //int snt_onoff = 0;
  int snt_frame_rate = 0;
  int pressure_threshold = 0;
  int dataSNT[ASUS_GRIP_SENSOR_DATA_SIZE];
  char nameSNT[ASUS_GRIP_SENSOR_NAME_SIZE];
  unsigned char d1test_ioctl[784];
  int bar_test_result[2];
  int cur_frame_rate, i2c_flag = 1, grip_en_status = 0, switch_en;
  int fpc_status = 1;
  uint16_t RegRead_t = 0;
  switch (cmd) {
    /* onoff is done by open/release */
    #if 0
  case ASUS_GRIP_SENSOR_IOCTL_ONOFF:
    ret = copy_from_user(&snt_onoff, (int __user*)arg, sizeof(snt_onoff));
    if( ret < 0) {
      PRINT_CRIT("%s: cmd = ONOFF, copy_from_user error(%d)\n", __func__, ret);
      goto end;
    }
    break;
    #endif
  case ASUS_GRIP_SENSOR_IOCTL_SET_FRAM_RATE:
    ret = copy_from_user(&snt_frame_rate, (int __user*)arg, sizeof(snt_frame_rate));
    if( ret < 0) {
      PRINT_CRIT("%s: cmd = SET_FRAM_RATE, copy_from_user error(%d)\n", __func__, snt8100fsr_g->frame_rate);
      goto end;
    }
    snt8100fsr_g->frame_rate = snt_frame_rate;
    break;
  case ASUS_GRIP_SENSOR_IOCTL_SET_PRESSURE_THRESHOLD:
    ret = copy_from_user(&pressure_threshold, (int __user*)arg, sizeof(pressure_threshold));
    if( ret < 0) {
      PRINT_CRIT("%s: cmd = SET_PRESSURE_THRESHOLD, copy_from_user error(%d)\n", __func__, snt8100fsr_g->frame_rate);
      goto end;
    }
    snt8100fsr_g->pressure_threshold = pressure_threshold;
    break;
  case ASUS_GRIP_SENSOR_IOCTL_DATA_READ:
    dataSNT[0] = snt8100fsr_g->frame_rate;
    dataSNT[1] = snt8100fsr_g->pressure_threshold;
    dataSNT[2] = snt8100fsr_g->suspended_frame_rate;
    PRINT_INFO("%s: cmd = DATA_READ, data[0] = %d, data[1] = %d, data[2] = %d\n"
	       , __func__, dataSNT[0], dataSNT[1], dataSNT[2]);
	ret = copy_to_user((int __user*)arg, &dataSNT, sizeof(dataSNT));
    break;
  case ASUS_GRIP_SENSOR_IOCTL_D1TEST_DATA_READ:
	    MUTEX_LOCK(&snt8100fsr_g->sb_lock);

	    ret = snt_read_sc_rsp(snt8100fsr_g);
        if(log_d1test_file != NULL) {
		//Clear char array
		memset(d1test_ioctl, 0, sizeof(d1test_ioctl));
		//each sc_cmd->data[] is 4bytes
		for(i = 0; i < (d1test_size/4); i++){
			strcat(&d1test_ioctl[4*i],  (unsigned char*)&sc_cmd->data[i]);
			PRINT_DEBUG("IOCTL: data[%d]: %s", i, (unsigned char*)&sc_cmd->data[i]);
		}
	}
	mutex_unlock(&snt8100fsr_g->sb_lock);
		
	ret = copy_to_user((int __user*)arg, &d1test_ioctl, sizeof(d1test_ioctl));
	PRINT_DEBUG("IOCTL: done");
    break;
  case ASUS_GRIP_SENSOR_IOCTL_MODULE_NAME:
    snprintf(nameSNT, sizeof(nameSNT), "%s", SYSFS_NAME);
    PRINT_INFO("%s: cmd = MODULE_NAME, name = %s\n", __func__, nameSNT);
    ret = copy_to_user((int __user*)arg, &nameSNT, sizeof(nameSNT));
    break;
  case ASUS_GRIP_SENSOR_IOCTL_BAR0_TEST:
  	bar_test_result[0] = 0;
 	bar_test_result[1] = 0;
	ret = read_register(snt8100fsr_g,
                 Grip_Golden_addr,
                 &RegRead_t);
	RegRead_t = RegRead_t & 0x00FF;
	bar_test_force = RegRead_t;
  	for (i = 0; i < snt8100fsr_g->track_reports_count; i++) {
		if(snt8100fsr_g->track_reports[i].bar_id == 0 && snt8100fsr_g->track_reports[i].force_lvl != 0){
			//center 56~166
			if(snt8100fsr_g->track_reports[i].center > 0 && snt8100fsr_g->track_reports[i].center < 9999 ){
				bar_test_result[1] = snt8100fsr_g->track_reports[i].force_lvl;
				bar_test_result[0] = check_report_force(bar_test_result[1]);
				print_current_report(i);
				break;
			}
		}
    	}
	PRINT_INFO("Bar_0 Result = %d, Force = %d", bar_test_result[0], bar_test_result[1]);
	ret = copy_to_user((int __user*)arg, &bar_test_result, sizeof(bar_test_result));
  	break;
  case ASUS_GRIP_SENSOR_IOCTL_BAR1_TEST:
  	bar_test_result[0] = 0;
 	bar_test_result[1] = 0;
	ret = read_register(snt8100fsr_g,
                 Grip_Golden_addr,
                 &RegRead_t);
	RegRead_t = RegRead_t >> 8;
	bar_test_force = RegRead_t;
  	for (i = 0; i < snt8100fsr_g->track_reports_count; i++) {
		if(snt8100fsr_g->track_reports[i].bar_id == 1 && snt8100fsr_g->track_reports[i].force_lvl != 0){
			//center 92~604
			if(snt8100fsr_g->track_reports[i].center > 0 && snt8100fsr_g->track_reports[i].center < 9999 ){
				bar_test_result[1] = snt8100fsr_g->track_reports[i].force_lvl;
				bar_test_result[0] = check_report_force(bar_test_result[1]);
				print_current_report(i);
				break;
			}
		}
    	}
	PRINT_INFO("Bar_1 Result = %d, Force = %d", bar_test_result[0], bar_test_result[1]);
	ret = copy_to_user((int __user*)arg, &bar_test_result, sizeof(bar_test_result));
  	break;
  case ASUS_GRIP_SENSOR_IOCTL_BAR2_TEST:
  	bar_test_result[0] = 0;
 	bar_test_result[1] = 0;
	ret = read_register(snt8100fsr_g,
                 0x35,
                 &RegRead_t);
	RegRead_t = RegRead_t & 0x00FF;
	bar_test_force = RegRead_t;
  	for (i = 0; i < snt8100fsr_g->track_reports_count; i++) {
		if(snt8100fsr_g->track_reports[i].bar_id == 2 && snt8100fsr_g->track_reports[i].force_lvl != 0){
			//center 77 ~384
			if(snt8100fsr_g->track_reports[i].center > 0 && snt8100fsr_g->track_reports[i].center < 9999 ){
				bar_test_result[1] = snt8100fsr_g->track_reports[i].force_lvl;
				bar_test_result[0] = check_report_force(bar_test_result[1]);
				print_current_report(i);
				break;
			}
		}
    	}
	PRINT_INFO("Bar_2 Result = %d, Force = %d", bar_test_result[0], bar_test_result[1]);
	ret = copy_to_user((int __user*)arg, &bar_test_result, sizeof(bar_test_result));
  	break;
  case ASUS_GRIP_SENSOR_IOCTL_BAR_TEST_FORCE:
	ret = copy_from_user(&bar_test_force, (int __user*)arg, sizeof(bar_test_force));
	if( ret < 0) {
		PRINT_CRIT("%s: cmd = ASUS_GRIP_SENSOR_IOCTL_BAR_TEST_FORCE, copy_from_user error(%d)\n", __func__, bar_test_force);
      		goto end;
	}
	PRINT_INFO("set bar_test force = %d", bar_test_force);
	break;
  case ASUS_GRIP_SENSOR_IOCTL_BAR_TEST_TOLERANCE:
	ret = copy_from_user(&bar_test_tolerance, (int __user*)arg, sizeof(bar_test_tolerance));
	if( ret < 0) {
		PRINT_CRIT("%s: cmd = ASUS_GRIP_SENSOR_IOCTL_BAR_TEST_TOLERANCE, copy_from_user error(%d)\n", __func__, bar_test_tolerance);
      		goto end;
	}
	PRINT_INFO("set bar_test tolerance = %d", bar_test_tolerance);
	break;
  case ASUS_GRIP_SENSOR_IOCTL_I2C_TEST:
    Wait_Wake_For_RegW();
	if(snt8100fsr_g->grip_fw_loading_status == true){
		ret = read_register (snt8100fsr_g,
	                 REGISTER_FRAME_RATE,
	                 &cur_frame_rate);
	    if(ret < 0) {
			PRINT_ERR("Grip I2c no ack");	
			i2c_flag = 0;
		}
	}else{
		i2c_flag = 0;
	}
	PRINT_INFO("I2C status = %d, frame_rate=%d", i2c_flag, cur_frame_rate);
	ret = copy_to_user((int __user*)arg, &i2c_flag, sizeof(i2c_flag));
  	break;
  case ASUS_GRIP_SENSOR_IOCTL_GET_ONOFF:
	grip_en_status = Snt_Power_State;
	ret = copy_to_user((int __user*)arg, &grip_en_status, sizeof(i2c_flag));
  	break;
  case ASUS_GRIP_SENSOR_IOCTL_SWITCH_ONOFF:
	ret = copy_from_user(&switch_en, (int __user*)arg, sizeof(switch_en));
	if( ret < 0) {
		PRINT_CRIT("%s: cmd = ASUS_GRIP_SENSOR_IOCTL_SWITCH_ONOFF, copy_from_user error(%d)\n", __func__, bar_test_tolerance);
      		goto end;
	}
	if(switch_en == 0 && Snt_Power_State == 1){
		Grip_Driver_IRQ_EN(1);
		Wait_Wake_For_RegW();
	}
	Power_Control(switch_en);
	PRINT_INFO("set power = %d", switch_en);
	break;
  case ASUS_GRIP_SENSOR_IOCTL_BAR0_STATUS:	
	Wait_Wake_For_RegW();
  	if(Health_Check(0x0003)!=0){
		fpc_status = 0;
  	}
	ret = copy_to_user((int __user*)arg, &fpc_status, sizeof(fpc_status));
  	break;
  case ASUS_GRIP_SENSOR_IOCTL_BAR1_STATUS:	
	Wait_Wake_For_RegW();
  	if(Health_Check(0x003C)!=0){
		fpc_status = 0;
  	}
	ret = copy_to_user((int __user*)arg, &fpc_status, sizeof(fpc_status));
  	break;
  case ASUS_GRIP_SENSOR_IOCTL_BAR2_STATUS:	
	Wait_Wake_For_RegW();
  	if(Health_Check(0x01C0)!=0){
		fpc_status = 0;
  	}
	ret = copy_to_user((int __user*)arg, &fpc_status, sizeof(fpc_status));
  	break;
  default:
    ret = -1;
    PRINT_INFO("%s: default\n", __func__);
  }
 end:
  return ret;
}

int sntSensor_miscRegister(void)
{
  int rtn = 0;
  /* in sys/class/misc/ */
  rtn = misc_register(&sentons_snt_misc);
  if (rtn < 0) {
    PRINT_CRIT("[%s] Unable to register misc deive\n", __func__);
    misc_deregister(&sentons_snt_misc);
  }
  return rtn;
}
/*************** ASUS BSP Clay: ioctl --- *******************/

/*************** Golden K related func +++ *******************/
static void Grip_Apply_Golden_K(void){
	int ret=0;
	uint16_t Golden_K = 0;
	G_Golden_K_flag = 1;
	if(g_ASUS_hwID < HW_REV_MP){
		Golden_K = Grip_Golden;
	}else{
		Golden_K = Grip_Golden_MP;
	}
	PRINT_INFO("Grip Apply Golden K, 0x%x", Golden_K);
	ret = write_register(snt8100fsr_g, Grip_Golden_addr, &Golden_K);
	if (ret) {
		PRINT_CRIT("set 0x%x init failed", Grip_Golden_addr);
	}
}
#ifndef FACTORY_FLAG
static void Read_Grip_K_data(void){
	uint16_t Read_reg = 0;
	int ret=0;
	/* Read Squeeze factory */
	ret = read_register (snt8100fsr_g,
		Grip_Golden_addr,
		 &Read_reg);
	Grip_B0_F_value = Read_reg & 0x00ff;
	Grip_B1_F_value = Read_reg >> 8;
	ret = read_register (snt8100fsr_g,
		0x35,
		 &Read_reg);
	Grip_B2_F_value = Read_reg & 0x00ff;
	PRINT_INFO("B0:0x%x, B1:0x%x", Grip_B0_F_value, Grip_B1_F_value);
}
static void GripK_magnification(void){
  /*
	float scale_factory = 0;
	uint16_t Real_K = 0;
	
	scale_factory = Grip_K_Scaling_factory;
	
	if(Grip_B0_F_value * scale_factory > 255)
		Grip_B0_F_value = 255;
	else
		Grip_B0_F_value = Grip_B0_F_value * scale_factory;
	if(Grip_B1_F_value * scale_factory > 255)
		Grip_B1_F_value = 255;
	else
		Grip_B1_F_value = Grip_B1_F_value * scale_factory;
	Real_K = Grip_B1_F_value << 8  | Grip_B0_F_value;
	write_register(snt8100fsr_g, Grip_Golden_addr, &Real_K);
	PRINT_INFO("Scaling_Factory:%f, B0:0x%x, B1:0x%x, Real_K:0x%x", scale_factory, Grip_B0_F_value, Grip_B1_F_value, Real_K);
  */
}
#endif
/*************** Golden K related func --- *******************/

/*************** ASUS BSP Clay: proc file +++ *******************/
/*+++BSP Clay proc asusGripDebug Interface+++*/
static int g_debugMode=0;
int asusGripDebug_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", g_debugMode);
	return 0;
}
int asusGripDebug_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, asusGripDebug_proc_read, NULL);
}

ssize_t asusGripDebug_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int count=0, val[2] = { -1, -1};
	char *token = (char *) kmalloc(sizeof(char), GFP_KERNEL);
	char *s = (char *) kmalloc(sizeof(char), GFP_KERNEL);
	
	Wait_Wake_For_RegW();
	if (len > 256) {
		len = 256;
	}
	
	if (copy_from_user(s, buff, len)) {
		return -EFAULT;
	}
	//strlcpy(s, messages, sizeof(len));

	do {
		token = strsep(&s, " ");
		if(token!= NULL)
			val[count] = (int)simple_strtol(token, NULL, 10);
		else
			break;
		count++;
	}while(token != NULL);

	if(count ==2){
		PRINT_INFO("val=%d, %d", val[0], val[1]);
	}else{
		PRINT_INFO("count = %d, Do nothing!", count);
	}
	
	if(token != NULL)
		kfree(token);
	if(s != NULL)
		kfree(s);
	return len;
}
void create_asusGripDebug_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  asusGripDebug_proc_open,
		.write = asusGripDebug_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_Debug_Flag", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
/*---BSP Clay proc asusGripDebug Interface---*/


/*Calibration File read operation */
static int Grip_Calibration_raw_data_proc_read(struct seq_file *buf, void *v)
{
	int ret,  i;

	
	    MUTEX_LOCK(&snt8100fsr_g->sb_lock);

	    ret = snt_read_sc_rsp(snt8100fsr_g);
        if(log_d1test_file != NULL) {
		//each sc_cmd->data[] is 4bytes
		for(i = 0; i < (d1test_size/4); i++){
			PRINT_INFO("IOCTL: data[%d]: %s", i, (unsigned char*)&sc_cmd->data[i]);	
			seq_printf(buf, "%s",  (unsigned char*)&sc_cmd->data[i]);
		}
	}
	mutex_unlock(&snt8100fsr_g->sb_lock);
		
	PRINT_INFO("proc_data: done");
	
	return 0;
}

static int Grip_Calibration_raw_data_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Calibration_raw_data_proc_read, NULL);
}

void create_Grip_Calibration_raw_data_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Calibration_raw_data_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_D1test", 0666, NULL, &proc_fops);
	if (!proc_file) {
		PRINT_ERR("%s failed!\n", __func__);
	}
	return;
}
/*Calibration File read operation*/

/* +++ BSP Clay proc i2c check +++ */
static int Grip_I2c_Check_proc_read(struct seq_file *buf, void *v)
{
	int ret, i2c_status;
	bool flag = 1;
	Wait_Wake_For_RegW();
	ret = read_register (snt8100fsr_g,
                 REGISTER_FRAME_RATE,
                 &i2c_status);
        if(ret < 0) {
		PRINT_ERR("Grip I2c no ack");	
		flag = 0;
		goto Report;
	}
		
Report:
	if(flag == 1){
		seq_printf(buf, "%d\n", flag);
	}else{
		seq_printf(buf, "%d\n", flag);
	}
	return 0;
}
static int Grip_I2c_Check_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_I2c_Check_proc_read, NULL);
}
void create_Grip_I2c_Check_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_I2c_Check_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_I2c_Check", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
/* --- BSP Clay proc i2c check --- */
/* +++ BSP Clay proc FPC check +++ */
extern bool g_Charger_mode;
static ssize_t Grip_FPC_Check_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
#ifdef FACTORY_FLAG
#else
	int val;
	char messages[256];
	
	if(g_Charger_mode){
		PRINT_INFO("Charger mode, do nothing");
		return len;
	}
	if(G_Golden_K_flag == 1){
		PRINT_INFO("Alreadly apply golen value");
		return len;
	}

	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	if(val == 1){
		PRINT_INFO("Panel Uniqe ID changed");
		G_Golden_K_flag = 1;
		Grip_Driver_IRQ_EN(1);
		Wait_Wake_For_RegW();
		//Grip_Chip_IRQ_EN(1);
		
		Grip_Apply_Golden_K();
		Read_Grip_K_data();
		GripK_magnification();
		Into_DeepSleep_fun();
	}else{
		PRINT_INFO("Panel Uniqe ID doesn't change");
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);
	Grip_ReEnable_Squeeze_Check();
#endif
	return len;
}

static int Grip_FPC_Check_proc_read(struct seq_file *buf, void *v)
{
	PRINT_INFO("Proc: FPC Check 0x%x", FPC_value);
	seq_printf(buf, "0x%x\n", FPC_value);
	return 0;
}
static int Grip_FPC_Check_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_FPC_Check_proc_read, NULL);
}
void create_Grip_FPC_Check_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_FPC_Check_proc_open,
		.write = Grip_FPC_Check_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_FPC_Check", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}


static ssize_t Grip_ReadK_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	
#ifdef FACTORY_FLAG
#else
	int val;
	char messages[256];
	
	if(g_Charger_mode){
		PRINT_INFO("Charger mode, do nothing");
		return len;
	}
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	if(aw8697_trig_control(1, 1)==0)
		PRINT_INFO("Enable vib trig1");
	else
		PRINT_INFO("Failed to enable vib trig1");
	
	if(aw8697_trig_control(2, 1)==0)
		PRINT_INFO("Enable vib trig2");
	else
		PRINT_INFO("Failed to enable vib trig2");
	
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	Grip_Driver_IRQ_EN(1);
	Wait_Wake_For_RegW();
	//Grip_Chip_IRQ_EN(1);
		
	G_Golden_K_flag = 0;
	//PRINT_INFO("Str: %s, %zu bytes", messages, len);
    	enable_boot_init_reg_req(snt8100fsr_g, messages, len);
	Read_Grip_K_data();
	GripK_magnification();
	Into_DeepSleep_fun();
	mutex_unlock(&snt8100fsr_g->ap_lock);
	Grip_ReEnable_Squeeze_Check();
#endif
	return len;
}

static int Grip_ReadK_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "0\n");
	return 0;
}
static int Grip_ReadK_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_ReadK_proc_read, NULL);
}
void create_Grip_ReadK_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_ReadK_proc_open,
		.write = Grip_ReadK_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_ReadK", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

/* +++ BSP Clay proc FPC check --- */
static int Grip_Apply_GoldenK_proc_read(struct seq_file *buf, void *v)
{
	PRINT_INFO("Apply Golden K flag = %d", G_Golden_K_flag);
	seq_printf(buf, "%d\n", G_Golden_K_flag);
	return 0;
}

static ssize_t Grip_Apply_GoldenK_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
#ifdef FACTORY_FLAG
#else
	int val;
	char messages[256];
	
	if(g_Charger_mode){
		PRINT_INFO("Charger mode, do nothing");
		return len;
	}
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	if(snt8100fsr_g->grip_fw_loading_status == false){
		return -1;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	if(aw8697_trig_control(1, 1)==0)
		PRINT_INFO("Enable vib trig1");
	else
		PRINT_INFO("Failed to enable vib trig1");
	
	if(aw8697_trig_control(2, 1)==0)
		PRINT_INFO("Enable vib trig2");
	else
		PRINT_INFO("Failed to enable vib trig2");
	
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	if(val == 1){
		Grip_Driver_IRQ_EN(1);
		G_Golden_K_flag = 1;
		Wait_Wake_For_RegW();
		//Grip_Chip_IRQ_EN(1);
		
		Grip_Apply_Golden_K();
		Read_Grip_K_data();
		GripK_magnification();
		Into_DeepSleep_fun();
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);
	Grip_ReEnable_Squeeze_Check();
#endif
	return len;
}

static int Grip_Apply_GoldenK_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Apply_GoldenK_proc_read, NULL);
}
void create_Grip_Apply_GoldenK_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Apply_GoldenK_proc_open,
		.write =  Grip_Apply_GoldenK_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_Apply_GoldenK_F", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}


/* +++ BSP Clay proc Squeeze factory read +++ */
static int Grip_SQ_Bar0_factory_proc_read(struct seq_file *buf, void *v)
{
	PRINT_INFO("Proc: Grip SQ Bar0 Factory 0x%x", Grip_B0_F_value);
	seq_printf(buf, "%d\n", Grip_B0_F_value);
	return 0;
}
static int Grip_SQ_Bar0_factory_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_SQ_Bar0_factory_proc_read, NULL);
}
void create_Grip_SQ_Bar0_factory_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_SQ_Bar0_factory_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_SQ_B0_factor", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
static int Grip_SQ_Bar1_factory_proc_read(struct seq_file *buf, void *v)
{
	PRINT_INFO("Proc: Grip SQ Bar1 Factory 0x%x", Grip_B1_F_value);
	seq_printf(buf, "%d\n", Grip_B1_F_value);
	return 0;
}
static int Grip_SQ_Bar1_factory_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_SQ_Bar1_factory_proc_read, NULL);
}
void create_Grip_SQ_Bar1_factory_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_SQ_Bar1_factory_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_SQ_B1_factor", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
static int Grip_SQ_Bar2_factory_proc_read(struct seq_file *buf, void *v)
{
	PRINT_INFO("Proc: Grip SQ Bar2 Factory %d", Grip_B2_F_value);
	seq_printf(buf, "%d\n", Grip_B2_F_value);
	return 0;
}
static int Grip_SQ_Bar2_factory_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_SQ_Bar2_factory_proc_read, NULL);
}
void create_Grip_SQ_Bar2_factory_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_SQ_Bar2_factory_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_SQ_B2_factor", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
/* +++ BSP Clay proc FPC check --- */
/* +++ BSP Clay Disable wake_lock and grip event +++ */
bool wake_lock_disable_flag = 0;;
static int Grip_Disable_WakeLock_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "wake_lock_evt_flag: %d\n", wake_lock_disable_flag);
	return 0;
}

static ssize_t Grip_Disable_WakeLock_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val, ret, reg_en = 0;
	char messages[256];
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	wake_lock_disable_flag = val;
	if(wake_lock_disable_flag == 0){
		reg_en = 1;
	}else{
		wake_unlock(&(snt8100fsr_g->snt_wakelock));
	}
	/*
	ret = write_register(snt8100fsr_g,
                 REGISTER_ENABLE,
                 &reg_en);
        if(ret < 0) {
		PRINT_ERR("Grip register_enable write fail");
        }else{
        	PRINT_INFO("wake_lock_evt_flag = %d", wake_lock_disable_flag);
        }
	*/
	Wait_Wake_For_RegW();
	ret = read_register(snt8100fsr_g, REGISTER_ENABLE, &reg_en);
        if(ret < 0) {
		PRINT_ERR("Grip register_enable write fail");
        }else{
        	PRINT_INFO("reg_en = %d", reg_en);
        }	
    	
	return len;
}

static int Grip_Disable_WakeLock_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Disable_WakeLock_proc_read, NULL);
}

void create_Grip_Disable_WakeLock_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Disable_WakeLock_proc_open,
		.write =  Grip_Disable_WakeLock_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/Grip_Disable_WakeLock", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
/* --- BSP Clay R/W Temp register value --- */
/* +++ BSP Clay Frame Rate setting  +++ */
static int Grip_frame_proc_read(struct seq_file *buf, void *v)
{
	if(grip_status_g->G_DPC_STATUS==1)
		seq_printf(buf, "%d\n", Grip_DPC_status_g->High);
	else
		seq_printf(buf, "%d\n", snt8100fsr_g->frame_rate);
	return 0;
}
static ssize_t Grip_frame_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	grip_frame_rate_func(val);
	return len;
}

static int Grip_frame_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_frame_proc_read, NULL);
}

void create_Grip_frame_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_frame_proc_open,
		.write =  Grip_frame_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_frame_rate", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

/* --- BSP Clay Frame Rate setting --- */
//==============Enable Interface=============//
static int Grip_raw_en_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_RAW_EN);
	return 0;
}
static ssize_t Grip_raw_en_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	
	if(grip_status_g->G_RAW_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
		grip_raw_enable_func(val);
	return len;
}

static int Grip_raw_en_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_raw_en_proc_read, NULL);
}

void create_Grip_raw_en_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_raw_en_proc_open,
		.write =  Grip_raw_en_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_raw_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

/*+++  Sensor Grip Gesture +++ */
static int Grip_en_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_EN);
	return 0;
}
static ssize_t Grip_en_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	/*
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	grip_enable_func_noLock(val);
	*/
	PRINT_INFO("Do nothing");
	return len;
}

static int Grip_en_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_en_proc_read, NULL);
}

void create_Grip_en_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_en_proc_open,
		.write =  Grip_en_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

//Squeeze Enable Interface
static int Grip_Squeeze1_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE1_EN);
	return 0;
}

static ssize_t Grip_Squeeze1_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];

	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE1_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze1_enable_func(val);
	return len;
}

static int Grip_Squeeze1_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze1_En_proc_read, NULL);
}

void create_Grip_Squeeze1_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze1_En_proc_open,
		.write =  Grip_Squeeze1_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze1_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze2_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE2_EN);
	return 0;
}

static ssize_t Grip_Squeeze2_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];

	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE2_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze2_enable_func(val);
	return len;
}

static int Grip_Squeeze2_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze2_En_proc_read, NULL);
}

void create_Grip_Squeeze2_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze2_En_proc_open,
		.write =  Grip_Squeeze2_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze2_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze1_Force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE1_FORCE);
	return 0;
}
static ssize_t Grip_Squeeze1_Force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE1_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze1_force_func(val);
	
	return len;
}

static int Grip_Squeeze1_Force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze1_Force_proc_read, NULL);
}

void create_Grip_Squeeze1_Force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze1_Force_proc_open,
		.write =  Grip_Squeeze1_Force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze1_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze2_Force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE2_FORCE);
	return 0;
}
static ssize_t Grip_Squeeze2_Force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE2_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze2_force_func(val);
	
	return len;
}

static int Grip_Squeeze2_Force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze2_Force_proc_read, NULL);
}

void create_Grip_Squeeze2_Force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze2_Force_proc_open,
		.write =  Grip_Squeeze2_Force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze2_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

/* +++ BSP Clay Write Grip Continuous Squeeze +++ */
static int Grip_Squeeze1_up_rate_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE1_UP_RATE);
	return 0;
}
static ssize_t Grip_Squeeze1_up_rate_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE1_UP_RATE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze1_up_rate_func(val);
	
	return len;
}

static int Grip_Squeeze1_up_rate_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze1_up_rate_proc_read, NULL);
}

void create_Grip_Squeeze1_up_rate_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze1_up_rate_proc_open,
		.write =  Grip_Squeeze1_up_rate_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze1_up_rate", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze2_up_rate_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE2_UP_RATE);
	return 0;
}
static ssize_t Grip_Squeeze2_up_rate_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE2_UP_RATE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze2_up_rate_func(val);
	
	return len;
}

static int Grip_Squeeze2_up_rate_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze2_up_rate_proc_read, NULL);
}

void create_Grip_Squeeze2_up_rate_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze2_up_rate_proc_open,
		.write =  Grip_Squeeze2_up_rate_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze2_up_rate", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze1_up_total_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE1_UP_TOTAL);
	return 0;
}
static ssize_t Grip_Squeeze1_up_total_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE1_UP_TOTAL==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze1_up_total_func(val);
	
	return len;
}

static int Grip_Squeeze1_up_total_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze1_up_total_proc_read, NULL);
}

void create_Grip_Squeeze1_up_total_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze1_up_total_proc_open,
		.write =  Grip_Squeeze1_up_total_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze1_up_total", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze2_up_total_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE2_UP_TOTAL);
	return 0;
}
static ssize_t Grip_Squeeze2_up_total_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE2_UP_TOTAL==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze2_up_total_func(val);
	
	return len;
}

static int Grip_Squeeze2_up_total_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze2_up_total_proc_read, NULL);
}

void create_Grip_Squeeze2_up_total_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze2_up_total_proc_open,
		.write =  Grip_Squeeze2_up_total_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze2_up_total", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze1_drop_rate_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE1_DROP_RATE);
	return 0;
}
static ssize_t Grip_Squeeze1_drop_rate_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE1_DROP_RATE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze1_drop_rate_func(val);
	
	return len;
}

static int Grip_Squeeze1_drop_rate_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze1_drop_rate_proc_read, NULL);
}

void create_Grip_Squeeze1_drop_rate_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze1_drop_rate_proc_open,
		.write =  Grip_Squeeze1_drop_rate_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze1_drop_rate", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze2_drop_rate_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE2_DROP_RATE);
	return 0;
}
static ssize_t Grip_Squeeze2_drop_rate_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE2_DROP_RATE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze2_drop_rate_func(val);
	
	return len;
}

static int Grip_Squeeze2_drop_rate_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze2_drop_rate_proc_read, NULL);
}

void create_Grip_Squeeze2_drop_rate_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze2_drop_rate_proc_open,
		.write =  Grip_Squeeze2_drop_rate_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze2_drop_rate", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze1_drop_total_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE1_DROP_TOTAL);
	return 0;
}
static ssize_t Grip_Squeeze1_drop_total_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE1_DROP_TOTAL==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze1_drop_total_func(val);
	
	return len;
}

static int Grip_Squeeze1_drop_total_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze1_drop_total_proc_read, NULL);
}

void create_Grip_Squeeze1_drop_total_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze1_drop_total_proc_open,
		.write =  Grip_Squeeze1_drop_total_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze1_drop_total", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze2_drop_total_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE2_DROP_TOTAL);
	return 0;
}
ssize_t Grip_Squeeze2_drop_total_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE2_DROP_TOTAL==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze2_drop_total_func(val);
	
	return len;
}

int Grip_Squeeze2_drop_total_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze2_drop_total_proc_read, NULL);
}

void create_Grip_Squeeze2_drop_total_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze2_drop_total_proc_open,
		.write =  Grip_Squeeze2_drop_total_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze2_drop_total", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze1_short_dur_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE1_SHORT);
	return 0;
}

static ssize_t Grip_Squeeze1_short_dur_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE1_SHORT==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze1_short_dur_func(val);
	return len;
}

static int Grip_Squeeze1_short_dur_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze1_short_dur_proc_read, NULL);
}

void create_Grip_Squeeze1_short_dur_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze1_short_dur_proc_open,
		.write =  Grip_Squeeze1_short_dur_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = 
	proc_create("driver/grip_squeeze1_short_dur", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze2_short_dur_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE2_SHORT);
	return 0;
}

static ssize_t Grip_Squeeze2_short_dur_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE2_SHORT==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze2_short_dur_func(val);
	return len;
}

static int Grip_Squeeze2_short_dur_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze2_short_dur_proc_read, NULL);
}

void create_Grip_Squeeze2_short_dur_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze2_short_dur_proc_open,
		.write =  Grip_Squeeze2_short_dur_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = 
	proc_create("driver/grip_squeeze2_short_dur", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze1_long_dur_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE1_LONG);
	return 0;
}

static ssize_t Grip_Squeeze1_long_dur_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE1_LONG==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze1_long_dur_func(val);
	return len;
}

static int Grip_Squeeze1_long_dur_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze1_long_dur_proc_read, NULL);
}

void create_Grip_Squeeze1_long_dur_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze1_long_dur_proc_open,
		.write =  Grip_Squeeze1_long_dur_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = 
	proc_create("driver/grip_squeeze1_long_dur", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Squeeze2_long_dur_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SQUEEZE2_LONG);
	return 0;
}

static ssize_t Grip_Squeeze2_long_dur_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SQUEEZE2_LONG==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_squeeze2_long_dur_func(val);
	return len;
}

static int Grip_Squeeze2_long_dur_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze2_long_dur_proc_read, NULL);
}

void create_Grip_Squeeze2_long_dur_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze2_long_dur_proc_open,
		.write =  Grip_Squeeze2_long_dur_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = 
	proc_create("driver/grip_squeeze2_long_dur", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

/* --- BSP Clay Write Grip Squeeze duration --- */

//Tap Enable Interface
int Grip_Tap1_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_EN);
	return 0;
}

ssize_t Grip_Tap1_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];

	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_enable_func(val);
	return len;
}

int Grip_Tap1_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_En_proc_read, NULL);
}

void create_Grip_Tap1_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_En_proc_open,
		.write =  Grip_Tap1_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap2_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_EN);
	return 0;
}

ssize_t Grip_Tap2_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];

	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_enable_func(val);
	return len;
}

int Grip_Tap2_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_En_proc_read, NULL);
}

void create_Grip_Tap2_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_En_proc_open,
		.write =  Grip_Tap2_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap3_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP3_EN);
	return 0;
}

ssize_t Grip_Tap3_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];

	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP3_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap3_enable_func(val);
	return len;
}

int Grip_Tap3_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap3_En_proc_read, NULL);
}

void create_Grip_Tap3_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap3_En_proc_open,
		.write =  Grip_Tap3_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap3_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

/* +++ BSP Clay Write Grip Tap Force +++ */
int Grip_Tap1_Force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_FORCE);
	return 0;
}

ssize_t Grip_Tap1_Force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];

	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_force_func(val);
	return len;
}

int Grip_Tap1_Force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_Force_proc_read, NULL);
}

void create_Grip_Tap1_Force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_Force_proc_open,
		.write =  Grip_Tap1_Force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
int Grip_Tap2_Force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_FORCE);
	return 0;
}

ssize_t Grip_Tap2_Force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_force_func(val);
	return len;
}

int Grip_Tap2_Force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_Force_proc_read, NULL);
}

void create_Grip_Tap2_Force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_Force_proc_open,
		.write =  Grip_Tap2_Force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
int Grip_Tap3_Force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP3_FORCE);
	return 0;
}

ssize_t Grip_Tap3_Force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP3_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap3_force_func(val);
	return len;
}

int Grip_Tap3_Force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap3_Force_proc_read, NULL);
}

void create_Grip_Tap3_Force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap3_Force_proc_open,
		.write =  Grip_Tap3_Force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap3_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap1_Vibtator_enable_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_VIB_EN);
	return 0;
}

ssize_t Grip_Tap1_Vibtator_enable_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_VIB_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_vibrator_enable_func(val);
	return len;
}

int Grip_Tap1_Vibtator_enable_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_Vibtator_enable_proc_read, NULL);
}

void create_Grip_Tap1_Vibtator_enable_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_Vibtator_enable_proc_open,
		.write =  Grip_Tap1_Vibtator_enable_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_vib_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap2_Vibtator_enable_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_VIB_EN);
	return 0;
}

ssize_t Grip_Tap2_Vibtator_enable_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_VIB_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_vibrator_enable_func(val);
	return len;
}

int Grip_Tap2_Vibtator_enable_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_Vibtator_enable_proc_read, NULL);
}

void create_Grip_Tap2_Vibtator_enable_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_Vibtator_enable_proc_open,
		.write =  Grip_Tap2_Vibtator_enable_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_vib_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Slide1_Vibtator_enable_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SLIDE1_VIB_EN);
	return 0;
}

static ssize_t Grip_Slide1_Vibtator_enable_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SLIDE1_VIB_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_slide1_vibrator_enable_func(val);
	return len;
}

static int Grip_Slide1_Vibtator_enable_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide1_Vibtator_enable_proc_read, NULL);
}

void create_Grip_Slide1_Vibtator_enable_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide1_Vibtator_enable_proc_open,
		.write =  Grip_Slide1_Vibtator_enable_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide1_vib_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Slide2_Vibtator_enable_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SLIDE2_VIB_EN);
	return 0;
}

static ssize_t Grip_Slide2_Vibtator_enable_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SLIDE2_VIB_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_slide2_vibrator_enable_func(val);
	return len;
}

static int Grip_Slide2_Vibtator_enable_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide2_Vibtator_enable_proc_read, NULL);
}

void create_Grip_Slide2_Vibtator_enable_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide2_Vibtator_enable_proc_open,
		.write =  Grip_Slide2_Vibtator_enable_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide2_vib_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap1_Rest_enable_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_REST_EN);
	return 0;
}

ssize_t Grip_Tap1_Rest_enable_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_REST_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_finger_reseting_enable_func(val);
	return len;
}

int Grip_Tap1_Rest_enable_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_Rest_enable_proc_read, NULL);
}

void create_Grip_Tap1_Rest_enable_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_Rest_enable_proc_open,
		.write =  Grip_Tap1_Rest_enable_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_rest_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap2_Rest_enable_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_REST_EN);
	return 0;
}

ssize_t Grip_Tap2_Rest_enable_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_REST_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_finger_reseting_enable_func(val);
	return len;
}

int Grip_Tap2_Rest_enable_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_Rest_enable_proc_read, NULL);
}

void create_Grip_Tap2_Rest_enable_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_Rest_enable_proc_open,
		.write =  Grip_Tap2_Rest_enable_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_rest_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap1_MIN_Position_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_MIN_POS);
	return 0;
}

static ssize_t Grip_Tap1_MIN_Position_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_MIN_POS==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_min_position_func(val);
	return len;
}

static int Grip_Tap1_MIN_Position_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_MIN_Position_proc_read, NULL);
}

void create_Grip_Tap1_MIN_Position_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_MIN_Position_proc_open,
		.write =  Grip_Tap1_MIN_Position_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_min_position", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap2_MIN_Position_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_MIN_POS);
	return 0;
}

static ssize_t Grip_Tap2_MIN_Position_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_MIN_POS==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_min_position_func(val);
	return len;
}

static int Grip_Tap2_MIN_Position_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_MIN_Position_proc_read, NULL);
}

void create_Grip_Tap2_MIN_Position_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_MIN_Position_proc_open,
		.write =  Grip_Tap2_MIN_Position_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_min_position", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap1_MAX_Position_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_MAX_POS);
	return 0;
}

static ssize_t Grip_Tap1_MAX_Position_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_MAX_POS==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_max_position_func(val);
	return len;
}

static int Grip_Tap1_MAX_Position_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_MAX_Position_proc_read, NULL);
}

void create_Grip_Tap1_MAX_Position_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_MAX_Position_proc_open,
		.write =  Grip_Tap1_MAX_Position_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_max_position", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap2_MAX_Position_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_MAX_POS);
	return 0;
}

static ssize_t Grip_Tap2_MAX_Position_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_MAX_POS==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_max_position_func(val);
	return len;
}

static int Grip_Tap2_MAX_Position_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_MAX_Position_proc_read, NULL);
}

void create_Grip_Tap2_MAX_Position_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_MAX_Position_proc_open,
		.write =  Grip_Tap2_MAX_Position_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_max_position", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap1_slope_window_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_SLOPE_WINDOW);
	return 0;
}

static ssize_t Grip_Tap1_slope_window_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_SLOPE_WINDOW==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_slope_window_func(val);
	return len;
}

static int Grip_Tap1_slope_window_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_slope_window_proc_read, NULL);
}

void create_Grip_Tap1_slope_window_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_slope_window_proc_open,
		.write =  Grip_Tap1_slope_window_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_slope_window", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap2_slope_window_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_SLOPE_WINDOW);
	return 0;
}

static ssize_t Grip_Tap2_slope_window_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_SLOPE_WINDOW==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_slope_window_func(val);
	return len;
}

static int Grip_Tap2_slope_window_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_slope_window_proc_read, NULL);
}

void create_Grip_Tap2_slope_window_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_slope_window_proc_open,
		.write =  Grip_Tap2_slope_window_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_slope_window", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap1_slope_release_force_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_SLOPE_RELEASE_FORCE);
	return 0;
}

static ssize_t Grip_Tap1_slope_release_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_SLOPE_RELEASE_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_slope_release_force_func(val);
	return len;
}

static int Grip_Tap1_slope_release_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_slope_release_force_proc_read, NULL);
}

void create_Grip_Tap1_slope_release_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_slope_release_force_proc_open,
		.write =  Grip_Tap1_slope_release_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_slope_release_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap2_slope_release_force_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_SLOPE_RELEASE_FORCE);
	return 0;
}

static ssize_t Grip_Tap2_slope_release_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_SLOPE_RELEASE_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_slope_release_force_func(val);
	return len;
}

static int Grip_Tap2_slope_release_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_slope_release_force_proc_read, NULL);
}

void create_Grip_Tap2_slope_release_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_slope_release_force_proc_open,
		.write =  Grip_Tap2_slope_release_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_slope_release_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap1_slope_tap_force_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_SLOPE_TAP_FORCE);
	return 0;
}

static ssize_t Grip_Tap1_slope_tap_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_SLOPE_TAP_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_slope_tap_force_func(val);
	return len;
}

static int Grip_Tap1_slope_tap_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_slope_tap_force_proc_read, NULL);
}

void create_Grip_Tap1_slope_tap_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_slope_tap_force_proc_open,
		.write =  Grip_Tap1_slope_tap_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_slope_tap_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap2_slope_tap_force_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_SLOPE_TAP_FORCE);
	return 0;
}

static ssize_t Grip_Tap2_slope_tap_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_SLOPE_TAP_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_slope_tap_force_func(val);
	return len;
}

static int Grip_Tap2_slope_tap_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_slope_tap_force_proc_read, NULL);
}

void create_Grip_Tap2_slope_tap_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_slope_tap_force_proc_open,
		.write =  Grip_Tap2_slope_tap_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_slope_tap_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap1_delta_tap_force_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_DELTA_TAP_FORCE);
	return 0;
}

static ssize_t Grip_Tap1_delta_tap_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_DELTA_TAP_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_delta_tap_force_func(val);
	return len;
}

static int Grip_Tap1_delta_tap_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_delta_tap_force_proc_read, NULL);
}

void create_Grip_Tap1_delta_tap_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_delta_tap_force_proc_open,
		.write =  Grip_Tap1_delta_tap_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_delta_tap_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap2_delta_tap_force_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_DELTA_TAP_FORCE);
	return 0;
}

static ssize_t Grip_Tap2_delta_tap_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_DELTA_TAP_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_delta_tap_force_func(val);
	return len;
}

static int Grip_Tap2_delta_tap_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_delta_tap_force_proc_read, NULL);
}

void create_Grip_Tap2_delta_tap_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_delta_tap_force_proc_open,
		.write =  Grip_Tap2_delta_tap_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_delta_tap_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap1_delta_release_force_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP1_DELTA_RELEASE_FORCE);
	return 0;
}

static ssize_t Grip_Tap1_delta_release_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP1_DELTA_RELEASE_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap1_delta_release_force_func(val);
	return len;
}

static int Grip_Tap1_delta_release_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_delta_release_force_proc_read, NULL);
}

void create_Grip_Tap1_delta_release_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap1_delta_release_force_proc_open,
		.write =  Grip_Tap1_delta_release_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap1_delta_release_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Tap2_delta_release_force_proc_read(struct seq_file *buf, void *v)
{	
	seq_printf(buf, "%d\n", grip_status_g->G_TAP2_DELTA_RELEASE_FORCE);
	return 0;
}

static ssize_t Grip_Tap2_delta_release_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP2_DELTA_RELEASE_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap2_delta_release_force_func(val);
	return len;
}

static int Grip_Tap2_delta_release_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap2_delta_release_force_proc_read, NULL);
}

void create_Grip_Tap2_delta_release_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap2_delta_release_force_proc_open,
		.write =  Grip_Tap2_delta_release_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap2_delta_release_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

//Tap Sense Enable  Interface
int Grip_Tap_Sense_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n",  grip_status_g->G_TAP_SENSE_SET);
	return 0;
}

ssize_t Grip_Tap_Sense_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];

	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_TAP_SENSE_SET ==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_tap_sense_enable_func(val);
	return len;
}

int Grip_Tap_Sense_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_Sense_En_proc_read, NULL);
}

void create_Grip_Tap_Sense_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_Sense_En_proc_open,
		.write =  Grip_Tap_Sense_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_sense_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
//Slide Interface
static int Grip_Slide1_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SLIDE1_EN);
	return 0;
}

static ssize_t Grip_Slide1_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SLIDE1_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_slide1_enable_func(val);
	return len;
}

static int Grip_Slide1_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide1_En_proc_read, NULL);
}

void create_Grip_Slide1_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide1_En_proc_open,
		.write =  Grip_Slide1_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide1_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Slide2_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SLIDE2_EN);
	return 0;
}

static ssize_t Grip_Slide2_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SLIDE2_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_slide2_enable_func(val);
	return len;
}

static int Grip_Slide2_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide2_En_proc_read, NULL);
}

void create_Grip_Slide2_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide2_En_proc_open,
		.write =  Grip_Slide2_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide2_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Slide1_Distance_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SLIDE1_DIST);
	return 0;
}

static ssize_t Grip_Slide1_Distance_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SLIDE1_DIST==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_slide1_dist_func(val);
	return len;
}

static int Grip_Slide1_Distance_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide1_Distance_proc_read, NULL);
}

void create_Grip_Slide1_Distance_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide1_Distance_proc_open,
		.write =  Grip_Slide1_Distance_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide1_dist", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Slide2_Distance_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SLIDE2_DIST);
	return 0;
}

static ssize_t Grip_Slide2_Distance_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SLIDE2_DIST==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_slide2_dist_func(val);
	return len;
}

static int Grip_Slide2_Distance_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide2_Distance_proc_read, NULL);
}

void create_Grip_Slide2_Distance_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide2_Distance_proc_open,
		.write =  Grip_Slide2_Distance_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide2_dist", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Slide1_force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SLIDE1_FORCE);
	return 0;
}

static ssize_t Grip_Slide1_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SLIDE1_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_slide1_force_func(val);
	return len;
}

static int Grip_Slide1_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide1_force_proc_read, NULL);
}

void create_Grip_Slide1_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide1_force_proc_open,
		.write =  Grip_Slide1_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide1_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Slide2_force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SLIDE2_FORCE);
	return 0;
}

static ssize_t Grip_Slide2_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SLIDE2_FORCE==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_slide2_force_func(val);
	return len;
}

static int Grip_Slide2_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide2_force_proc_read, NULL);
}

void create_Grip_Slide2_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide2_force_proc_open,
		.write =  Grip_Slide2_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide2_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

//Swipe Enable Interface
static int Grip_Swipe1_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SWIPE1_EN);
	return 0;
}

static ssize_t Grip_Swipe1_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SWIPE1_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_swipe1_enable_func(val);
	return len;
}

static int Grip_Swipe1_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe1_En_proc_read, NULL);
}

void create_Grip_Swipe1_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe1_En_proc_open,
		.write =  Grip_Swipe1_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe1_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Swipe2_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SWIPE2_EN);
	return 0;
}

static ssize_t Grip_Swipe2_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SWIPE2_EN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_swipe2_enable_func(val);
	return len;
}

static int Grip_Swipe2_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe2_En_proc_read, NULL);
}

void create_Grip_Swipe2_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe2_En_proc_open,
		.write =  Grip_Swipe2_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe2_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Swipe1_Velocity_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SWIPE1_VELOCITY);
	return 0;
}

static ssize_t Grip_Swipe1_Velocity_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SWIPE1_VELOCITY==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_swipe1_velocity_func(val);
	return len;
}

static int Grip_Swipe1_Velocity_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe1_Velocity_proc_read, NULL);
}

void create_Grip_Swipe1_Velocity_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe1_Velocity_proc_open,
		.write =  Grip_Swipe1_Velocity_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe1_v", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Swipe2_Velocity_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SWIPE2_VELOCITY);
	return 0;
}

static ssize_t Grip_Swipe2_Velocity_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SWIPE2_VELOCITY==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_swipe2_velocity_func(val);
	return len;
}

static int Grip_Swipe2_Velocity_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe2_Velocity_proc_read, NULL);
}

void create_Grip_Swipe2_Velocity_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe2_Velocity_proc_open,
		.write =  Grip_Swipe2_Velocity_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe2_v", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Swipe1_Len_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SWIPE1_LEN);
	return 0;
}

static ssize_t Grip_Swipe1_Len_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SWIPE1_LEN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_swipe1_len_func(val);
	return len;
}

static int Grip_Swipe1_Len_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe1_Len_proc_read, NULL);
}

void create_Grip_Swipe1_Len_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe1_Len_proc_open,
		.write =  Grip_Swipe1_Len_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe1_len", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_Swipe2_Len_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", grip_status_g->G_SWIPE2_LEN);
	return 0;
}

static ssize_t Grip_Swipe2_Len_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	if(grip_status_g->G_SWIPE2_LEN==val){
		PRINT_INFO("repeat, skip it");
		return len;
	}
	grip_swipe2_len_func(val);
	return len;
}

static int Grip_Swipe2_Len_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe2_Len_proc_read, NULL);
}

void create_Grip_Swipe2_Len_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe2_Len_proc_open,
		.write =  Grip_Swipe2_Len_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe2_len", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

/* --- BSP Clay Grip Gesture --- */

//=======================================================
//=======================================================
//=======================================================
static int parse_result[2] = {-1};
static char *parse_token = NULL;
static char *string_temp = NULL;
static int Grip_node_parse_string(const char __user *buff, size_t len, int *reg_val){
	int count=0, val[2] = { -1, -1};
	int result=-1;
	char message[256];
	
	memset(message, 0, sizeof(message));
	if(parse_token == NULL){
		PRINT_DEBUG("parse_token KMALLOC MEMORY!!!!");
		parse_token = (char *) kmalloc(sizeof(char), GFP_KERNEL);
	}
	if(string_temp == NULL){
		PRINT_DEBUG("string_temp KMALLOC MEMORY!!!!");
		parse_token = (char *) kmalloc(sizeof(char), GFP_KERNEL);
	}
	
    memset(parse_result, -1, sizeof(*parse_result));
		
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(message, buff, len)) {
		result = -EFAULT;
		goto Final;
	}
	string_temp = message;
	
	do {
		if(count > 0){
			if(parse_token[0] > 57 || parse_token[0] < 48){
				PRINT_ERR("1. count=%d, parse_token=%s, string_temp=%s", count, parse_token, string_temp);
				break;
			}
		}
		
		parse_token = strsep(&string_temp, " ");
		val[count] = (int)simple_strtol(parse_token, NULL, 10);
		PRINT_DEBUG("count=%d, val[%d]=%s, parse_token=%s, string_temp=%s", count, val[count], parse_token, string_temp);
		count++;
		if(string_temp == NULL){
			PRINT_DEBUG("string_temp == NULL");
			break;
		}else if(string_temp[0] > 57 || string_temp[0] < 48){
			PRINT_ERR("2. count=%d, parse_token=%s, string_temp=%s", count, parse_token, string_temp);
			break;
		}
	}while(parse_token != NULL);
	
	if(count ==2){
		if(reg_val[val[0]]==val[1]){
			PRINT_DEBUG("repeat, skip it, input1=%d, input2=%d", val[0], val[1]);
			goto Final;
		}else
			result = 1;
		
		PRINT_DEBUG("val=%d, %d", val[0], val[1]);
		parse_result[0] = val[0];
		parse_result[1] = val[1];
	}else{
		PRINT_ERR("Error input count = %d, Do nothing!", count);
	}

Final:
	PRINT_DEBUG("Done");
	return result;
}
int Grip_Tap_En_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_EN[0], 
		grip_status_g->G_TAP_EN[1], 
		grip_status_g->G_TAP_EN[2], 
		grip_status_g->G_TAP_EN[3]);
	return 0;
}

ssize_t Grip_Tap_En_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_EN) > 0)
		grip_tapX_enable_func(parse_result[1], parse_result[0], &TAP_BIT0[parse_result[0]]);
	return len;
}

int Grip_Tap_En_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_En_proc_read, NULL);
}

void create_Grip_Tap_En_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_En_proc_open,
		.write =  Grip_Tap_En_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap_Force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_FORCE[0], 
		grip_status_g->G_TAP_FORCE[1], 
		grip_status_g->G_TAP_FORCE[2], 
		grip_status_g->G_TAP_FORCE[3]);
	return 0;
}

ssize_t Grip_Tap_Force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_FORCE) > 0)
		grip_tapX_force_func(parse_result[1], parse_result[0], &TAP_BIT0[parse_result[0]]);
	return len;
}

int Grip_Tap_Force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_Force_proc_read, NULL);
}

void create_Grip_Tap_Force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_Force_proc_open,
		.write =  Grip_Tap_Force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}


int Grip_Tap_min_pos_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_MIN_POS[0], 
		grip_status_g->G_TAP_MIN_POS[1], 
		grip_status_g->G_TAP_MIN_POS[2], 
		grip_status_g->G_TAP_MIN_POS[3]);
	return 0;
}

ssize_t Grip_Tap_min_pos_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_MIN_POS) > 0)
		grip_tapX_min_position_func(parse_result[1], parse_result[0], &TAP_BIT2[parse_result[0]]);
	return len;
}

int Grip_Tap_min_pos_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_min_pos_proc_read, NULL);
}

void create_Grip_Tap_min_pos_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_min_pos_proc_open,
		.write =  Grip_Tap_min_pos_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_min_position", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap_max_pos_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_MAX_POS[0], 
		grip_status_g->G_TAP_MAX_POS[1], 
		grip_status_g->G_TAP_MAX_POS[2], 
		grip_status_g->G_TAP_MAX_POS[3]);
	return 0;
}

ssize_t Grip_Tap_max_pos_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_MAX_POS) > 0)
		grip_tapX_max_position_func(parse_result[1], parse_result[0], &TAP_BIT3[parse_result[0]]);
	return len;
}

int Grip_Tap_max_pos_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_max_pos_proc_read, NULL);
}

void create_Grip_Tap_max_pos_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_max_pos_proc_open,
		.write =  Grip_Tap_max_pos_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_max_position", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap_slope_window_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_SLOPE_WINDOW[0],
		grip_status_g->G_TAP_SLOPE_WINDOW[1],
		grip_status_g->G_TAP_SLOPE_WINDOW[2],
		grip_status_g->G_TAP_SLOPE_WINDOW[3]);
	return 0;
}

ssize_t Grip_Tap_slope_window_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_SLOPE_WINDOW) > 0)
		grip_tapX_slope_window_func(parse_result[1], parse_result[0], &TAP_BIT1[parse_result[0]]);
	return len;
}

int Grip_Tap_slope_window_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_slope_window_proc_read, NULL);
}

void create_Grip_Tap_slope_window_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_slope_window_proc_open,
		.write =  Grip_Tap_slope_window_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_slope_window", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap_slope_tap_force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_SLOPE_TAP_FORCE[0],
		grip_status_g->G_TAP_SLOPE_TAP_FORCE[1],
		grip_status_g->G_TAP_SLOPE_TAP_FORCE[2],
		grip_status_g->G_TAP_SLOPE_TAP_FORCE[3]);
	return 0;
}

ssize_t Grip_Tap_slope_tap_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_SLOPE_TAP_FORCE) > 0)
		grip_tapX_slope_tap_force_func(parse_result[1], parse_result[0], &TAP_BIT5[parse_result[0]]);
	return len;
}

int Grip_Tap_slope_tap_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_slope_tap_force_proc_read, NULL);
}

void create_Grip_Tap_slope_tap_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_slope_tap_force_proc_open,
		.write =  Grip_Tap_slope_tap_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_slope_tap_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap_slope_release_force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_SLOPE_RELEASE_FORCE[0],
		grip_status_g->G_TAP_SLOPE_RELEASE_FORCE[1],
		grip_status_g->G_TAP_SLOPE_RELEASE_FORCE[2],
		grip_status_g->G_TAP_SLOPE_RELEASE_FORCE[3]);
	return 0;
}

ssize_t Grip_Tap_slope_release_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_SLOPE_RELEASE_FORCE) > 0)
		grip_tapX_slope_release_force_func(parse_result[1], parse_result[0], &TAP_BIT5[parse_result[0]]);
	return len;
}

int Grip_Tap_slope_release_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_slope_release_force_proc_read, NULL);
}

void create_Grip_Tap_slope_release_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_slope_release_force_proc_open,
		.write =  Grip_Tap_slope_release_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_slope_release_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap_delta_tap_force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_DELTA_TAP_FORCE[0], 
		grip_status_g->G_TAP_DELTA_TAP_FORCE[1], 
		grip_status_g->G_TAP_DELTA_TAP_FORCE[2], 
		grip_status_g->G_TAP_DELTA_TAP_FORCE[3]);
	return 0;
}

ssize_t Grip_Tap_delta_tap_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_DELTA_TAP_FORCE) > 0)
		grip_tapX_delta_tap_force_func(parse_result[1], parse_result[0], &TAP_BIT4[parse_result[0]]);
	return len;
}

int Grip_Tap_delta_tap_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_delta_tap_force_proc_read, NULL);
}

void create_Grip_Tap_delta_tap_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_delta_tap_force_proc_open,
		.write =  Grip_Tap_delta_tap_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_delta_tap_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap_delta_release_force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_DELTA_RELEASE_FORCE[0],
		grip_status_g->G_TAP_DELTA_RELEASE_FORCE[1],
		grip_status_g->G_TAP_DELTA_RELEASE_FORCE[2],
		grip_status_g->G_TAP_DELTA_RELEASE_FORCE[3]);
	return 0;
}

ssize_t Grip_Tap_delta_release_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_DELTA_RELEASE_FORCE) > 0)
		grip_tapX_delta_release_force_func(parse_result[1], parse_result[0], &TAP_BIT4[parse_result[0]]);
	return len;
}

int Grip_Tap_delta_release_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap1_delta_release_force_proc_read, NULL);
}

void create_Grip_Tap_delta_release_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_delta_release_force_proc_open,
		.write =  Grip_Tap_delta_release_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_delta_release_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Tap_vib_en_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_TAP_VIB_EN[0],
		grip_status_g->G_TAP_VIB_EN[1], 
		grip_status_g->G_TAP_VIB_EN[2],
		grip_status_g->G_TAP_VIB_EN[3]);
	return 0;
}

ssize_t Grip_Tap_vib_en_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_TAP_VIB_EN) > 0)
		grip_tapX_vibrator_enable_func(parse_result[1], parse_result[0]);
	return len;
}

int Grip_Tap_vib_en_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Tap_vib_en_proc_read, NULL);
}

void create_Grip_Tap_vib_en_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Tap_vib_en_proc_open,
		.write =  Grip_Tap_vib_en_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_tap_vib_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

//============================================================
//=======================TAP Part Done!!!==========================
//============================================================
int Grip_Squeeze_en_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SQUEEZE_EN[0],
		grip_status_g->G_SQUEEZE_EN[1]);
	return 0;
}

ssize_t Grip_Squeeze_en_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SQUEEZE_EN) > 0)
		grip_squeezeX_enable_func(parse_result[1], parse_result[0], &SQ_BIT0[parse_result[0]]);
	return len;
}

int Grip_Squeeze_en_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze_en_proc_read, NULL);
}

void create_Grip_Squeeze_en_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze_en_proc_open,
		.write =  Grip_Squeeze_en_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Squeeze_force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SQUEEZE_FORCE[0],
		grip_status_g->G_SQUEEZE_FORCE[1]);
	return 0;
}

ssize_t Grip_Squeeze_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SQUEEZE_FORCE) > 0)
		grip_squeezeX_force_func(parse_result[1], parse_result[0], &SQ_BIT0[parse_result[0]]);
	return len;
}

int Grip_Squeeze_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze_force_proc_read, NULL);
}

void create_Grip_Squeeze_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze_force_proc_open,
		.write =  Grip_Squeeze_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Squeeze_short_dur_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SQUEEZE_SHORT[0],
		grip_status_g->G_SQUEEZE_SHORT[1]);
	return 0;
}

ssize_t Grip_Squeeze_short_dur_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SQUEEZE_SHORT) > 0)
		grip_squeezeX_short_dur_func(parse_result[1], parse_result[0], &SQ_BIT5[parse_result[0]]);
	return len;
}

int Grip_Squeeze_short_dur_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze_short_dur_proc_read, NULL);
}

void create_Grip_Squeeze_short_dur_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze_short_dur_proc_open,
		.write =  Grip_Squeeze_short_dur_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze_short_dur", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Squeeze_long_dur_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SQUEEZE_LONG[0],
		grip_status_g->G_SQUEEZE_LONG[1]);
	return 0;
}

ssize_t Grip_Squeeze_long_dur_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SQUEEZE_LONG) > 0)
		grip_squeezeX_long_dur_func(parse_result[1], parse_result[0], &SQ_BIT5[parse_result[0]]);
	return len;
}

int Grip_Squeeze_long_dur_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze_long_dur_proc_read, NULL);
}

void create_Grip_Squeeze_long_dur_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze_long_dur_proc_open,
		.write =  Grip_Squeeze_long_dur_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze_long_dur", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Squeeze_drop_rate_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SQUEEZE_DROP_RATE[0],
		grip_status_g->G_SQUEEZE_DROP_RATE[1]);
	return 0;
}

ssize_t Grip_Squeeze_drop_rate_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SQUEEZE_DROP_RATE) > 0)
		grip_squeezeX_drop_rate_func(parse_result[1], parse_result[0], &SQ_BIT7[parse_result[0]]);
	return len;
}

int Grip_Squeeze_drop_rate_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze_drop_rate_proc_read, NULL);
}

void create_Grip_Squeeze_drop_rate_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze_drop_rate_proc_open,
		.write =  Grip_Squeeze_drop_rate_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze_drop_rate", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Squeeze_drop_total_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SQUEEZE_DROP_TOTAL[0],
		grip_status_g->G_SQUEEZE_DROP_TOTAL[1]);
	return 0;
}

ssize_t Grip_Squeeze_drop_total_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SQUEEZE_DROP_TOTAL) > 0)
		grip_squeezeX_drop_total_func(parse_result[1], parse_result[0], &SQ_BIT7[parse_result[0]]);
	return len;
}

int Grip_Squeeze_drop_total_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze_drop_total_proc_read, NULL);
}

void create_Grip_Squeeze_drop_total_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze_drop_total_proc_open,
		.write =  Grip_Squeeze_drop_total_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze_drop_total", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Squeeze_up_rate_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SQUEEZE_UP_RATE[0],
		grip_status_g->G_SQUEEZE_UP_RATE[1]);
	return 0;
}

ssize_t Grip_Squeeze_up_rate_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SQUEEZE_UP_RATE) > 0)
		grip_squeezeX_up_rate_func(parse_result[1], parse_result[0], &SQ_BIT6[parse_result[0]]);
	return len;
}

int Grip_Squeeze_up_rate_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze_up_rate_proc_read, NULL);
}

void create_Grip_Squeeze_up_rate_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze_up_rate_proc_open,
		.write =  Grip_Squeeze_up_rate_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze_up_rate", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Squeeze_up_total_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SQUEEZE_UP_TOTAL[0],
		grip_status_g->G_SQUEEZE_UP_TOTAL[1]);
	return 0;
}

ssize_t Grip_Squeeze_up_total_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SQUEEZE_UP_TOTAL) > 0)
		grip_squeezeX_up_total_func(parse_result[1], parse_result[0], &SQ_BIT6[parse_result[0]]);
	return len;
}

int Grip_Squeeze_up_total_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Squeeze_up_total_proc_read, NULL);
}

void create_Grip_Squeeze_up_total_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Squeeze_up_total_proc_open,
		.write =  Grip_Squeeze_up_total_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_squeeze_up_total", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
//============================================================
//=======================SQUEEZE Part Done!!!==========================
//============================================================
int Grip_Slide_en_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SLIDE_EN[0],
		grip_status_g->G_SLIDE_EN[1]);
	return 0;
}

ssize_t Grip_Slide_en_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SLIDE_EN) > 0)
		grip_slideX_enable_func(parse_result[1], parse_result[0], &SLIDE_BIT0[parse_result[0]]);
	return len;
}

int Grip_Slide_en_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide_en_proc_read, NULL);
}

void create_Grip_Slide_en_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide_en_proc_open,
		.write =  Grip_Slide_en_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Slide_dist_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SLIDE_DIST[0],
		grip_status_g->G_SLIDE_DIST[1]);
	return 0;
}

ssize_t Grip_Slide_dist_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SLIDE_DIST) > 0)
		grip_slideX_dist_func(parse_result[1], parse_result[0], &SLIDE_BIT3[parse_result[0]]);
	return len;
}

int Grip_Slide_dist_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide_dist_proc_read, NULL);
}

void create_Grip_Slide_dist_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide_dist_proc_open,
		.write =  Grip_Slide_dist_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide_dist", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Slide_2nd_dist_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SLIDE_2ND_DIST[0],
		grip_status_g->G_SLIDE_2ND_DIST[1]);
	return 0;
}

ssize_t Grip_Slide_2nd_dist_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SLIDE_2ND_DIST) > 0)
		grip_slideX_2nd_dist_func(parse_result[1], parse_result[0], &SLIDE_BIT4[parse_result[0]]);
	
	return len;
}

int Grip_Slide_2nd_dist_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide_2nd_dist_proc_read, NULL);
}

void create_Grip_Slide_2nd_dist_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide_2nd_dist_proc_open,
		.write =  Grip_Slide_2nd_dist_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide_2nd_dist", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Slide_force_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SLIDE_FORCE[0],
		grip_status_g->G_SLIDE_FORCE[1]);
	return 0;
}

ssize_t Grip_Slide_force_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SLIDE_FORCE) > 0)
		grip_slideX_force_func(parse_result[1], parse_result[0], &SLIDE_BIT3[parse_result[0]]);
	
	return len;
}

int Grip_Slide_force_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide_force_proc_read, NULL);
}

void create_Grip_Slide_force_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide_force_proc_open,
		.write =  Grip_Slide_force_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide_force", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Slide_min_pos_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SLIDE_MIN_POS[0],
		grip_status_g->G_SLIDE_MIN_POS[1]);
	return 0;
}

ssize_t Grip_Slide_min_pos_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SLIDE_MIN_POS) > 0)
		grip_slideX_min_position_func(parse_result[1], parse_result[0], &SLIDE_BIT1[parse_result[0]]);
	
	return len;
}

int Grip_Slide_min_pos_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide_min_pos_proc_read, NULL);
}

void create_Grip_Slide_min_pos_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide_min_pos_proc_open,
		.write =  Grip_Slide_min_pos_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide_min_pos", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Slide_max_pos_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SLIDE_MAX_POS[0],
		grip_status_g->G_SLIDE_MAX_POS[1]);
	return 0;
}

ssize_t Grip_Slide_max_pos_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SLIDE_MAX_POS) > 0)
		grip_slideX_max_position_func(parse_result[1], parse_result[0], &SLIDE_BIT2[parse_result[0]]);
	
	return len;
}

int Grip_Slide_max_pos_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide_max_pos_proc_read, NULL);
}

void create_Grip_Slide_max_pos_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide_max_pos_proc_open,
		.write =  Grip_Slide_max_pos_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide_max_pos", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Slide_vib_en_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d,%d,%d\n", 
		grip_status_g->G_SLIDE_VIB_EN[0],
		grip_status_g->G_SLIDE_VIB_EN[1]);
	return 0;
}

ssize_t Grip_Slide_vib_en_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SLIDE_VIB_EN) > 0)
		grip_slideX_vibrator_enable_func(parse_result[1], parse_result[0]);
	return len;
}

int Grip_Slide_vib_en_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Slide_vib_en_proc_read, NULL);
}

void create_Grip_Slide_vib_en_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Slide_vib_en_proc_open,
		.write =  Grip_Slide_vib_en_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_slide_vib_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

//============================================================
//=======================SLIDE Part Done!!!==========================
//============================================================

int Grip_Swipe_en_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SWIPE_EN[0],
		grip_status_g->G_SWIPE_EN[1]);
	return 0;
}

ssize_t Grip_Swipe_en_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SWIPE_EN) > 0)
		grip_swipeX_enable_func(parse_result[1], parse_result[0], &SWIPE_BIT0[parse_result[0]]);
	return len;
}

int Grip_Swipe_en_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe_en_proc_read, NULL);
}

void create_Grip_Swipe_en_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe_en_proc_open,
		.write =  Grip_Swipe_en_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe_en", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
int Grip_Swipe_velocity_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SWIPE_VELOCITY[0],
		grip_status_g->G_SWIPE_VELOCITY[1]);
	return 0;
}

ssize_t Grip_Swipe_velocity_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SWIPE_VELOCITY) > 0)
		grip_swipeX_velocity_func(parse_result[1], parse_result[0], &SWIPE_BIT0[parse_result[0]]);
	return len;
}

int Grip_Swipe_velocity_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe_velocity_proc_read, NULL);
}

void create_Grip_Swipe_velocity_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe_velocity_proc_open,
		.write =  Grip_Swipe_velocity_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe_v", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Swipe_len_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SWIPE_LEN[0],
		grip_status_g->G_SWIPE_LEN[1]);
	return 0;
}

ssize_t Grip_Swipe_len_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SWIPE_LEN) > 0)
		grip_swipeX_len_func(parse_result[1], parse_result[0], &SWIPE_BIT3[parse_result[0]]);
	return len;
}

int Grip_Swipe_len_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe_len_proc_read, NULL);
}

void create_Grip_Swipe_len_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe_len_proc_open,
		.write =  Grip_Swipe_len_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe_len", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Swipe_min_pos_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SWIPE_MIN_POS[0],
		grip_status_g->G_SWIPE_MIN_POS[1]);
	return 0;
}

ssize_t Grip_Swipe_min_pos_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SWIPE_MIN_POS) > 0)
		grip_swipeX_min_position_func(parse_result[1], parse_result[0], &SWIPE_BIT1[parse_result[0]]);
	return len;
}

int Grip_Swipe_min_pos_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe_min_pos_proc_read, NULL);
}

void create_Grip_Swipe_min_pos_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe_min_pos_proc_open,
		.write =  Grip_Swipe_min_pos_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe_min_pos", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

int Grip_Swipe_max_pos_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d,%d\n", 
		grip_status_g->G_SWIPE_MAX_POS[0],
		grip_status_g->G_SWIPE_MAX_POS[1]);
	return 0;
}

ssize_t Grip_Swipe_max_pos_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	if(Grip_node_parse_string(buff, len, grip_status_g->G_SWIPE_MAX_POS) > 0)
		grip_swipeX_max_position_func(parse_result[1], parse_result[0], &SWIPE_BIT2[parse_result[0]]);
	return len;
}

int Grip_Swipe_max_pos_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_Swipe_max_pos_proc_read, NULL);
}

void create_Grip_Swipe_max_pos_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_Swipe_max_pos_proc_open,
		.write =  Grip_Swipe_max_pos_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/grip_swipe_max_pos", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
//============================================================
//=======================SWIPE Part Done!!!==========================
//============================================================




//==========Gesture Thershold Interface=======//
int fw_version = 0;
static char *product_string;
static int Grip_FW_RESULT_proc_read(struct seq_file *buf, void *v)
{
	if(snt8100fsr_g->grip_fw_loading_status == true){
		seq_printf(buf, "0xffff\n");
	}else{
		seq_printf(buf, "0x0\n");
	}
	return 0;
}

static ssize_t Grip_FW_RESULT_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	return len;
}

static int Grip_FW_RESULT_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_FW_RESULT_proc_read, NULL);
}

void create_Grip_FW_RESULT_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_FW_RESULT_proc_open,
		.write =  Grip_FW_RESULT_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = 
	proc_create("driver/grip_fw_result", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}
static int Grip_FW_VER_proc_read(struct seq_file *buf, void *v)
{
	int ret=0;
	if(snt8100fsr_g->grip_fw_loading_status == false){
		seq_printf(buf, "0x0\n");
		return -1;
	}
	MUTEX_LOCK(&snt8100fsr_g->ap_lock);
	if(snt8100fsr_g->grip_fw_loading_status == true){
		Grip_Driver_IRQ_EN(1);
		Wait_Wake_For_RegW();
		//Grip_Chip_IRQ_EN(1);
		
	    product_string = memory_allocate(PRODUCT_CONFIG_MAX_LEN, 0);
	    if (product_string == NULL) {
	        PRINT_CRIT("memory_allocate(PRODUCT_CONFIG_MAX_LEN) failed");
		mutex_unlock(&snt8100fsr_g->ap_lock);
	        return -1;
	    }

	    ret = read_product_config(snt8100fsr_g, product_string);
            seq_printf(buf, "%s\n",product_string);
	    memory_free(product_string);
	    if (ret) {
	        PRINT_WARN("Unable to read product config");
	    }
	    Into_DeepSleep_fun();	
	}else{
		seq_printf(buf, "0x0\n");
	}
	mutex_unlock(&snt8100fsr_g->ap_lock);
	return 0;
}

static ssize_t Grip_FW_VER_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	fw_version = val;
	return len;
}

static int Grip_FW_VER_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_FW_VER_proc_read, NULL);
}

void create_Grip_FW_VER_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_FW_VER_proc_open,
		.write =  Grip_FW_VER_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = 
	proc_create("driver/grip_fw_ver", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

static int Grip_set_power_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "Grip 1V2_2V8 status: %d\n", Snt_Power_State);
	return 0;
}

static ssize_t Grip_set_power_proc_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *data)
{
	int val;
	char messages[256];
	
	memset(messages, 0, sizeof(messages));
	if (len > 256) {
		len = 256;
	}
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);
	
	if(val == 0 && Snt_Power_State == 1){
		Grip_Driver_IRQ_EN(1);
		Wait_Wake_For_RegW();
	}
	Power_Control(val);
	PRINT_INFO("set power = %d", val);
	return len;
}

static int Grip_set_power_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, Grip_set_power_proc_read, NULL);
}

void create_Grip_set_power_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  Grip_set_power_proc_open,
		.write =  Grip_set_power_proc_write,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = 
	proc_create("driver/grip_set_power", 0666, NULL, &proc_fops);

	if (!proc_file) {
		PRINT_CRIT("[Proc]%s failed!\n", __FUNCTION__);
	}
	return;
}

/*************** ASUS BSP Clay: proc file --- *******************/


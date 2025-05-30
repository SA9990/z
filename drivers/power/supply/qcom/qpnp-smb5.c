// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/log2.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/iio/consumer.h>
#include <linux/pmic-voter.h>
#include <linux/usb/typec.h>
#include "smb5-reg.h"
#include "smb5-lib.h"
#include "schgm-flash.h"

//ASUS BSP add include files +++
#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/reboot.h>
#include <dt-bindings/iio/qcom,spmi-vadc.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "fg-core.h"
//#include <linux/wakelock.h>
//#include <linux/msm_drm_notify.h> //Add to get drm notifier
#include <drm/drm_panel.h>
//ASUS BSP add include files ---

//ASUS BSP +++
#include <linux/extcon.h>
#include <../../../extcon/extcon.h>
//ASUS BSP ---

static struct smb_params smb5_pmi632_params = {
	.fcc			= {
		.name   = "fast charge current",
		.reg    = CHGR_FAST_CHARGE_CURRENT_CFG_REG,
		.min_u  = 0,
		.max_u  = 3000000,
		.step_u = 50000,
	},
	.fv			= {
		.name   = "float voltage",
		.reg    = CHGR_FLOAT_VOLTAGE_CFG_REG,
		.min_u  = 3600000,
		.max_u  = 4800000,
		.step_u = 10000,
	},
	.usb_icl		= {
		.name   = "usb input current limit",
		.reg    = USBIN_CURRENT_LIMIT_CFG_REG,
		.min_u  = 0,
		.max_u  = 3000000,
		.step_u = 50000,
	},
	.icl_max_stat		= {
		.name   = "dcdc icl max status",
		.reg    = ICL_MAX_STATUS_REG,
		.min_u  = 0,
		.max_u  = 3000000,
		.step_u = 50000,
	},
	.icl_stat		= {
		.name   = "input current limit status",
		.reg    = ICL_STATUS_REG,
		.min_u  = 0,
		.max_u  = 3000000,
		.step_u = 50000,
	},
	.otg_cl			= {
		.name	= "usb otg current limit",
		.reg	= DCDC_OTG_CURRENT_LIMIT_CFG_REG,
		.min_u	= 500000,
		.max_u	= 1000000,
		.step_u	= 250000,
	},
	.jeita_cc_comp_hot	= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_HOT_REG,
		.min_u	= 0,
		.max_u	= 1575000,
		.step_u	= 25000,
	},
	.jeita_cc_comp_cold	= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_COLD_REG,
		.min_u	= 0,
		.max_u	= 1575000,
		.step_u	= 25000,
	},
	.freq_switcher		= {
		.name	= "switching frequency",
		.reg	= DCDC_FSW_SEL_REG,
		.min_u	= 600,
		.max_u	= 1200,
		.step_u	= 400,
		.set_proc = smblib_set_chg_freq,
	},
	.aicl_5v_threshold		= {
		.name   = "AICL 5V threshold",
		.reg    = USBIN_5V_AICL_THRESHOLD_REG,
		.min_u  = 4000,
		.max_u  = 4700,
		.step_u = 100,
	},
	.aicl_cont_threshold		= {
		.name   = "AICL CONT threshold",
		.reg    = USBIN_CONT_AICL_THRESHOLD_REG,
		.min_u  = 4000,
		.max_u  = 8800,
		.step_u = 100,
		.get_proc = smblib_get_aicl_cont_threshold,
		.set_proc = smblib_set_aicl_cont_threshold,
	},
};

static struct smb_params smb5_pm8150b_params = {
	.fcc			= {
		.name   = "fast charge current",
		.reg    = CHGR_FAST_CHARGE_CURRENT_CFG_REG,
		.min_u  = 0,
		.max_u  = 8000000,
		.step_u = 50000,
	},
	.fv			= {
		.name   = "float voltage",
		.reg    = CHGR_FLOAT_VOLTAGE_CFG_REG,
		.min_u  = 3600000,
		.max_u  = 4790000,
		.step_u = 10000,
	},
	.usb_icl		= {
		.name   = "usb input current limit",
		.reg    = USBIN_CURRENT_LIMIT_CFG_REG,
		.min_u  = 0,
		.max_u  = 5000000,
		.step_u = 50000,
	},
	.icl_max_stat		= {
		.name   = "dcdc icl max status",
		.reg    = ICL_MAX_STATUS_REG,
		.min_u  = 0,
		.max_u  = 5000000,
		.step_u = 50000,
	},
	.icl_stat		= {
		.name   = "aicl icl status",
		.reg    = AICL_ICL_STATUS_REG,
		.min_u  = 0,
		.max_u  = 5000000,
		.step_u = 50000,
	},
	.otg_cl			= {
		.name	= "usb otg current limit",
		.reg	= DCDC_OTG_CURRENT_LIMIT_CFG_REG,
		.min_u	= 500000,
		.max_u	= 3000000,
		.step_u	= 500000,
	},
	.dc_icl		= {
		.name   = "DC input current limit",
		.reg    = DCDC_CFG_REF_MAX_PSNS_REG,
		.min_u  = 0,
		.max_u  = DCIN_ICL_MAX_UA,
		.step_u = 50000,
	},
	.jeita_cc_comp_hot	= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_HOT_REG,
		.min_u	= 0,
		.max_u	= 8000000,
		.step_u	= 25000,
		.set_proc = NULL,
	},
	.jeita_cc_comp_cold	= {
		.name	= "jeita fcc reduction",
		.reg	= JEITA_CCCOMP_CFG_COLD_REG,
		.min_u	= 0,
		.max_u	= 8000000,
		.step_u	= 25000,
		.set_proc = NULL,
	},
	.freq_switcher		= {
		.name	= "switching frequency",
		.reg	= DCDC_FSW_SEL_REG,
		.min_u	= 600,
		.max_u	= 1200,
		.step_u	= 400,
		.set_proc = smblib_set_chg_freq,
	},
	.aicl_5v_threshold		= {
		.name   = "AICL 5V threshold",
		.reg    = USBIN_5V_AICL_THRESHOLD_REG,
		.min_u  = 4000,
		.max_u  = 4700,
		.step_u = 100,
	},
	.aicl_cont_threshold		= {
		.name   = "AICL CONT threshold",
		.reg    = USBIN_CONT_AICL_THRESHOLD_REG,
		.min_u  = 4000,
		.max_u  = 11800,
		.step_u = 100,
		.get_proc = smblib_get_aicl_cont_threshold,
		.set_proc = smblib_set_aicl_cont_threshold,
	},
};

struct smb_dt_props {
	int			usb_icl_ua;
	struct device_node	*revid_dev_node;
	enum float_options	float_option;
	int			chg_inhibit_thr_mv;
	bool			no_battery;
	bool			hvdcp_disable;
	bool			hvdcp_autonomous;
	bool			adc_based_aicl;
	int			sec_charger_config;
	int			auto_recharge_soc;
	int			auto_recharge_vbat_mv;
	int			wd_bark_time;
	int			wd_snarl_time_cfg;
	int			batt_profile_fcc_ua;
	int			batt_profile_fv_uv;
	int			term_current_src;
	int			term_current_thresh_hi_ma;
	int			term_current_thresh_lo_ma;
	int			disable_suspend_on_collapse;
};

struct smb5 {
	struct smb_charger	chg;
	struct dentry		*dfs_root;
	struct smb_dt_props	dt;
};

//ASUS BSP add struct functions +++
struct smb_charger *smbchg_dev;
struct gpio_control *global_gpio;	//global gpio_control
struct timespec last_jeita_time;
//ASUS BSP add struct functions ---

//ASUS BSP : Add ASUS variables +++
bool boot_completed_flag = 0;
bool fake_er_stage_flag = 0;
int charger_limit_value = 70;
int charger_limit_enable_flag = 0;
volatile bool asus_suspend_cmd_flag = 0;
bool no_input_suspend_flag = 0;
bool usb_alert_side_flag = 0;
bool usb_alert_btm_flag = 0;
bool usb_alert_flag_ACCY = 0;
bool usb_alert_keep_suspend_flag_ACCY = 0;
volatile bool cos_alert_once_flag = 0;
int g_temp_state = 0;
int g_temp_side_state = 0;
int g_temp_btm_state = 0;
int g_temp_station_state = 0;
int g_temp_INBOX_DT_state = 0;
volatile bool ultra_bat_life_flag = 0;
volatile int g_ultra_cos_spec_time = 2880;
bool smartchg_stop_flag = 0;
bool smartchg_slow_flag = 0;
bool demo_app_property_flag = 0;
bool cn_demo_app_flag = 0;
int demo_recharge_delta = 2;
int g_force_usb_mux = 0;//Force to set the usb_mux, skip all asus_mux_setting_1_work function
int g_dt_batt_profile_fcc_ua;
volatile bool rerun_thermal_alert_flag = 0;
bool force_station_ultra_flag = 0;
volatile bool station_cap_zero_flag = 0;
bool game_type_flag = 0;
bool g_once_usb_thermal_btm = 0;
bool g_once_usb_thermal_side = 0;
bool g_smb5_probe_complete = false;
bool g_force_disable_inov = false;
//ASUS BSP : Add ASUS variables ---

//ASUS BSP : Add extern variables +++
extern struct fg_dev *g_fgDev;
extern bool g_Charger_mode;
extern int g_ftm_mode;
//extern int g_ST_SDP_mode;//Add for STATION uses ADB tool
static int g_ST_SDP_mode =0 ; // WA for waiting g_ST_SDP_mode implement
extern bool asus_flow_done_flag;
extern volatile bool asus_adapter_detecting_flag;
extern int asus_CHG_TYPE;
extern int g_adapter_id_vadc;
extern struct fg_dev * g_fgDev;
extern bool is_hall_sensor_detect; //Lijen implement power bank and balance mode
extern volatile bool dual_port_once_flag;
extern volatile enum bat_stage last_charger_statge;
extern volatile enum bat_charger_state last_charger_state;
extern volatile bool station_cable_flag;
extern volatile bool is_Station_PB;
//ASUS BSP : Add extern variables ---

// [+++] ASUS add for ROG ACCY
volatile enum POGO_ID ASUS_POGO_ID = NO_INSERT;
struct notifier_block pogo_detect_nb; //Add for usb_mux update when there is ACCY inserted
struct timespec g_last_accy_therm_int;
struct notifier_block smb5_drm_nb; //Add for station update when screen on
// [---] ASUS add for ROG ACCY

//ASUS BSP : Add for external functions +++
extern int ec_hid_event_register(struct notifier_block *nb);
extern int ec_hid_event_unregister(struct notifier_block *nb);
extern void smblib_asus_monitor_start(struct smb_charger *chg, int time);
extern bool asus_get_prop_usb_present(struct smb_charger *chg);
extern void pmic_set_pca9468_charging(bool enable);
extern void smblib_asus_monitor_start(struct smb_charger *chg, int time);
extern bool asus_get_prop_usb_present(struct smb_charger *chg);
extern int asus_get_prop_batt_capacity(struct smb_charger *chg);
extern int asus_get_prop_batt_volt(struct smb_charger *chg);
extern bool rt_chg_check_asus_vid(void);
extern bool PE_check_asus_vid(void);
extern int ec_i2c_check_interrupt(char *type, char *event);
extern int hid_to_get_battery_cap(int *cap);
extern void fg_station_attach_notifier(bool attached);
extern int asus_get_batt_status(void);
extern void write_CHGLimit_value(int input);
extern int asus_get_bottom_ufp_mode(void);
extern int asus_extcon_set_state_sync(struct extcon_dev *edev, int cable_state);
extern bool jeita_rule(void);
//ASUS BSP : Add for external functions ---

//ASUS BSP : Add for previous announcement +++
void set_qc_stat(union power_supply_propval *val);
//ASUS BSP : Add for previous announcement ---

//[+++]Add the interface for charging debug apk
extern int asus_get_prop_adapter_id(void);
extern int asus_get_prop_is_legacy_cable(void);
extern int asus_get_prop_total_fcc(void);
extern int asus_get_apsd_result_by_bit(void);
//[---]Add the interface for charging debug apk

static int __debug_mask;

static ssize_t pd_disabled_show(struct device *dev, struct device_attribute
				*attr, char *buf)
{
	struct smb5 *chip = dev_get_drvdata(dev);
	struct smb_charger *chg = &chip->chg;

	return snprintf(buf, PAGE_SIZE, "%d\n", chg->pd_disabled);
}

static ssize_t pd_disabled_store(struct device *dev, struct device_attribute
				 *attr, const char *buf, size_t count)
{
	int val;
	struct smb5 *chip = dev_get_drvdata(dev);
	struct smb_charger *chg = &chip->chg;

	if (kstrtos32(buf, 0, &val))
		return -EINVAL;

	chg->pd_disabled = val;

	return count;
}
static DEVICE_ATTR_RW(pd_disabled);

static ssize_t weak_chg_icl_ua_show(struct device *dev, struct device_attribute
				    *attr, char *buf)
{
	struct smb5 *chip = dev_get_drvdata(dev);
	struct smb_charger *chg = &chip->chg;

	return snprintf(buf, PAGE_SIZE, "%d\n", chg->weak_chg_icl_ua);
}

static ssize_t weak_chg_icl_ua_store(struct device *dev, struct device_attribute
				 *attr, const char *buf, size_t count)
{
	int val;
	struct smb5 *chip = dev_get_drvdata(dev);
	struct smb_charger *chg = &chip->chg;

	if (kstrtos32(buf, 0, &val))
		return -EINVAL;

	chg->weak_chg_icl_ua = val;

	return count;
}
static DEVICE_ATTR_RW(weak_chg_icl_ua);

static struct attribute *smb5_attrs[] = {
	&dev_attr_pd_disabled.attr,
	&dev_attr_weak_chg_icl_ua.attr,
	NULL,
};
ATTRIBUTE_GROUPS(smb5);

enum {
	BAT_THERM = 0,
	MISC_THERM,
	CONN_THERM,
	SMB_THERM,
};

static const struct clamp_config clamp_levels[] = {
	{ {0x11C6, 0x11F9, 0x13F1}, {0x60, 0x2E, 0x90} },
	{ {0x11C6, 0x11F9, 0x13F1}, {0x60, 0x2B, 0x9C} },
};

#define PMI632_MAX_ICL_UA	3000000
#define PM6150_MAX_FCC_UA	3000000
static int smb5_chg_config_init(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct pmic_revid_data *pmic_rev_id;
	struct device_node *revid_dev_node, *node = chg->dev->of_node;
	int rc = 0;

	revid_dev_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property\n");
		return -EINVAL;
	}

	pmic_rev_id = get_revid_data(revid_dev_node);
	if (IS_ERR_OR_NULL(pmic_rev_id)) {
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		rc =  -EPROBE_DEFER;
		goto out;
	}

	switch (pmic_rev_id->pmic_subtype) {
	case PM8150B_SUBTYPE:
		chip->chg.chg_param.smb_version = PM8150B_SUBTYPE;
		chg->param = smb5_pm8150b_params;
		chg->name = "pm8150b_charger";
		chg->wa_flags |= CHG_TERMINATION_WA;
		break;
	case PM7250B_SUBTYPE:
		chip->chg.chg_param.smb_version = PM7250B_SUBTYPE;
		chg->param = smb5_pm8150b_params;
		chg->name = "pm7250b_charger";
		chg->wa_flags |= CHG_TERMINATION_WA;
		chg->uusb_moisture_protection_capable = true;
		break;
	case PM6150_SUBTYPE:
		chip->chg.chg_param.smb_version = PM6150_SUBTYPE;
		chg->param = smb5_pm8150b_params;
		chg->name = "pm6150_charger";
		chg->wa_flags |= SW_THERM_REGULATION_WA | CHG_TERMINATION_WA;
		if (pmic_rev_id->rev4 >= 2)
			chg->uusb_moisture_protection_capable = true;
		chg->main_fcc_max = PM6150_MAX_FCC_UA;
		break;
	case PMI632_SUBTYPE:
		chip->chg.chg_param.smb_version = PMI632_SUBTYPE;
		chg->wa_flags |= WEAK_ADAPTER_WA | USBIN_OV_WA
				| CHG_TERMINATION_WA | USBIN_ADC_WA
				| SKIP_MISC_PBS_IRQ_WA;
		chg->param = smb5_pmi632_params;
		chg->use_extcon = true;
		chg->name = "pmi632_charger";
		/* PMI632 does not support PD */
		chg->pd_not_supported = true;
		chg->lpd_disabled = true;
		if (pmic_rev_id->rev4 >= 2)
			chg->uusb_moisture_protection_enabled = true;
		chg->hw_max_icl_ua =
			(chip->dt.usb_icl_ua > 0) ? chip->dt.usb_icl_ua
						: PMI632_MAX_ICL_UA;
		break;
	default:
		pr_err("PMIC subtype %d not supported\n",
				pmic_rev_id->pmic_subtype);
		rc = -EINVAL;
		goto out;
	}

	chg->chg_freq.freq_5V			= 600;
	chg->chg_freq.freq_6V_8V		= 800;
	chg->chg_freq.freq_9V			= 1050;
	chg->chg_freq.freq_12V                  = 1200;
	chg->chg_freq.freq_removal		= 1050;
	chg->chg_freq.freq_below_otg_threshold	= 800;
	chg->chg_freq.freq_above_otg_threshold	= 800;

	if (of_property_read_bool(node, "qcom,disable-sw-thermal-regulation"))
		chg->wa_flags &= ~SW_THERM_REGULATION_WA;

	if (of_property_read_bool(node, "qcom,disable-fcc-restriction"))
		chg->main_fcc_max = -EINVAL;

out:
	of_node_put(revid_dev_node);
	return rc;
}

#define PULL_NO_PULL	0
#define PULL_30K	30
#define PULL_100K	100
#define PULL_400K	400
static int get_valid_pullup(int pull_up)
{
	/* pull up can only be 0/30K/100K/400K) */
	switch (pull_up) {
	case PULL_NO_PULL:
		return INTERNAL_PULL_NO_PULL;
	case PULL_30K:
		return INTERNAL_PULL_30K_PULL;
	case PULL_100K:
		return INTERNAL_PULL_100K_PULL;
	case PULL_400K:
		return INTERNAL_PULL_400K_PULL;
	default:
		return INTERNAL_PULL_100K_PULL;
	}
}

#define INTERNAL_PULL_UP_MASK	0x3
static int smb5_configure_internal_pull(struct smb_charger *chg, int type,
					int pull)
{
	int rc;
	int shift = type * 2;
	u8 mask = INTERNAL_PULL_UP_MASK << shift;
	u8 val = pull << shift;

	rc = smblib_masked_write(chg, BATIF_ADC_INTERNAL_PULL_UP_REG,
				mask, val);
	if (rc < 0)
		dev_err(chg->dev,
			"Couldn't configure ADC pull-up reg rc=%d\n", rc);

	return rc;
}

#define MICRO_1P5A			1500000
#define MICRO_P1A			100000
#define MICRO_1PA			1000000
#define MICRO_3PA			3000000
#define MICRO_4PA			4000000
#define OTG_DEFAULT_DEGLITCH_TIME_MS	50
#define DEFAULT_WD_BARK_TIME		64
#define DEFAULT_WD_SNARL_TIME_8S	0x07
#define DEFAULT_FCC_STEP_SIZE_UA	100000
#define DEFAULT_FCC_STEP_UPDATE_DELAY_MS	1000
static int smb5_parse_dt_misc(struct smb5 *chip, struct device_node *node)
{
	int rc = 0, byte_len;
	struct smb_charger *chg = &chip->chg;

	of_property_read_u32(node, "qcom,sec-charger-config",
					&chip->dt.sec_charger_config);
	chg->sec_cp_present =
		chip->dt.sec_charger_config == POWER_SUPPLY_CHARGER_SEC_CP ||
		chip->dt.sec_charger_config == POWER_SUPPLY_CHARGER_SEC_CP_PL;

	chg->sec_pl_present =
		chip->dt.sec_charger_config == POWER_SUPPLY_CHARGER_SEC_PL ||
		chip->dt.sec_charger_config == POWER_SUPPLY_CHARGER_SEC_CP_PL;

	chg->step_chg_enabled = of_property_read_bool(node,
				"qcom,step-charging-enable");

	chg->typec_legacy_use_rp_icl = of_property_read_bool(node,
				"qcom,typec-legacy-rp-icl");

	chg->sw_jeita_enabled = of_property_read_bool(node,
				"qcom,sw-jeita-enable");

	chg->jeita_arb_enable = of_property_read_bool(node,
				"qcom,jeita-arb-enable");

	chg->pd_not_supported = chg->pd_not_supported ||
			of_property_read_bool(node, "qcom,usb-pd-disable");

	chg->lpd_disabled = chg->lpd_disabled ||
			of_property_read_bool(node, "qcom,lpd-disable");

	rc = of_property_read_u32(node, "qcom,wd-bark-time-secs",
					&chip->dt.wd_bark_time);
	if (rc < 0 || chip->dt.wd_bark_time < MIN_WD_BARK_TIME)
		chip->dt.wd_bark_time = DEFAULT_WD_BARK_TIME;

	rc = of_property_read_u32(node, "qcom,wd-snarl-time-config",
					&chip->dt.wd_snarl_time_cfg);
	if (rc < 0)
		chip->dt.wd_snarl_time_cfg = DEFAULT_WD_SNARL_TIME_8S;

	chip->dt.no_battery = of_property_read_bool(node,
						"qcom,batteryless-platform");

	if (of_find_property(node, "qcom,thermal-mitigation", &byte_len)) {
		chg->thermal_mitigation = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->thermal_mitigation == NULL)
			return -ENOMEM;

		chg->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"qcom,thermal-mitigation",
				chg->thermal_mitigation,
				chg->thermal_levels);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	rc = of_property_read_u32(node, "qcom,charger-temp-max",
			&chg->charger_temp_max);
	if (rc < 0)
		chg->charger_temp_max = -EINVAL;

	rc = of_property_read_u32(node, "qcom,smb-temp-max",
			&chg->smb_temp_max);
	if (rc < 0)
		chg->smb_temp_max = -EINVAL;

	rc = of_property_read_u32(node, "qcom,float-option",
						&chip->dt.float_option);
	if (!rc && (chip->dt.float_option < 0 || chip->dt.float_option > 4)) {
		pr_err("qcom,float-option is out of range [0, 4]\n");
		return -EINVAL;
	}

	chip->dt.hvdcp_disable = of_property_read_bool(node,
						"qcom,hvdcp-disable");
	chg->hvdcp_disable = chip->dt.hvdcp_disable;

	chip->dt.hvdcp_autonomous = of_property_read_bool(node,
						"qcom,hvdcp-autonomous-enable");

	chip->dt.auto_recharge_soc = -EINVAL;
	rc = of_property_read_u32(node, "qcom,auto-recharge-soc",
				&chip->dt.auto_recharge_soc);
	if (!rc && (chip->dt.auto_recharge_soc < 0 ||
			chip->dt.auto_recharge_soc > 100)) {
		pr_err("qcom,auto-recharge-soc is incorrect\n");
		return -EINVAL;
	}
	chg->auto_recharge_soc = chip->dt.auto_recharge_soc;

	chg->suspend_input_on_debug_batt = of_property_read_bool(node,
					"qcom,suspend-input-on-debug-batt");

	chg->fake_chg_status_on_debug_batt = of_property_read_bool(node,
					"qcom,fake-chg-status-on-debug-batt");

	rc = of_property_read_u32(node, "qcom,otg-deglitch-time-ms",
					&chg->otg_delay_ms);
	if (rc < 0)
		chg->otg_delay_ms = OTG_DEFAULT_DEGLITCH_TIME_MS;

	chg->fcc_stepper_enable = of_property_read_bool(node,
					"qcom,fcc-stepping-enable");

	if (chg->uusb_moisture_protection_capable)
		chg->uusb_moisture_protection_enabled =
			of_property_read_bool(node,
					"qcom,uusb-moisture-protection-enable");

	chg->hw_die_temp_mitigation = of_property_read_bool(node,
					"qcom,hw-die-temp-mitigation");

	chg->hw_connector_mitigation = of_property_read_bool(node,
					"qcom,hw-connector-mitigation");

	chg->hw_skin_temp_mitigation = of_property_read_bool(node,
					"qcom,hw-skin-temp-mitigation");

	chg->en_skin_therm_mitigation = of_property_read_bool(node,
					"qcom,en-skin-therm-mitigation");

	chg->connector_pull_up = -EINVAL;
	of_property_read_u32(node, "qcom,connector-internal-pull-kohm",
					&chg->connector_pull_up);

	chip->dt.disable_suspend_on_collapse = of_property_read_bool(node,
					"qcom,disable-suspend-on-collapse");
	chg->smb_pull_up = -EINVAL;
	of_property_read_u32(node, "qcom,smb-internal-pull-kohm",
					&chg->smb_pull_up);

	chip->dt.adc_based_aicl = of_property_read_bool(node,
					"qcom,adc-based-aicl");

	of_property_read_u32(node, "qcom,fcc-step-delay-ms",
					&chg->chg_param.fcc_step_delay_ms);
	if (chg->chg_param.fcc_step_delay_ms <= 0)
		chg->chg_param.fcc_step_delay_ms =
					DEFAULT_FCC_STEP_UPDATE_DELAY_MS;

	of_property_read_u32(node, "qcom,fcc-step-size-ua",
					&chg->chg_param.fcc_step_size_ua);
	if (chg->chg_param.fcc_step_size_ua <= 0)
		chg->chg_param.fcc_step_size_ua = DEFAULT_FCC_STEP_SIZE_UA;

	/*
	 * If property is present parallel charging with CP is disabled
	 * with HVDCP3 adapter.
	 */
	chg->hvdcp3_standalone_config = of_property_read_bool(node,
					"qcom,hvdcp3-standalone-config");

	of_property_read_u32(node, "qcom,hvdcp3-max-icl-ua",
					&chg->chg_param.hvdcp3_max_icl_ua);
	if (chg->chg_param.hvdcp3_max_icl_ua <= 0)
		chg->chg_param.hvdcp3_max_icl_ua = MICRO_3PA;

	of_property_read_u32(node, "qcom,hvdcp2-max-icl-ua",
					&chg->chg_param.hvdcp2_max_icl_ua);
	if (chg->chg_param.hvdcp2_max_icl_ua <= 0)
		chg->chg_param.hvdcp2_max_icl_ua = MICRO_3PA;

	of_property_read_u32(node, "qcom,hvdcp2-12v-max-icl-ua",
					&chg->chg_param.hvdcp2_12v_max_icl_ua);
	if (chg->chg_param.hvdcp2_12v_max_icl_ua <= 0)
		chg->chg_param.hvdcp2_12v_max_icl_ua =
			chg->chg_param.hvdcp2_max_icl_ua;

	/* Used only in Adapter CV mode of operation */
	of_property_read_u32(node, "qcom,qc4-max-icl-ua",
					&chg->chg_param.qc4_max_icl_ua);
	if (chg->chg_param.qc4_max_icl_ua <= 0)
		chg->chg_param.qc4_max_icl_ua = MICRO_4PA;

	return 0;
}

static int smb5_parse_dt_adc_channels(struct smb_charger *chg)
{
	int rc = 0;

	rc = smblib_get_iio_channel(chg, "asus_adapter_vadc", &chg->iio.asus_adapter_vadc_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "side_conn_temp", &chg->iio.side_connector_temp_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "btm_conn_temp", &chg->iio.btm_connector_temp_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "mid_voltage", &chg->iio.mid_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "usb_in_voltage",
					&chg->iio.usbin_v_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "chg_temp", &chg->iio.temp_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "usb_in_current",
					&chg->iio.usbin_i_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "sbux_res", &chg->iio.sbux_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "vph_voltage", &chg->iio.vph_v_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "die_temp", &chg->iio.die_temp_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "conn_temp",
					&chg->iio.connector_temp_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "skin_temp", &chg->iio.skin_temp_chan);
	if (rc < 0)
		return rc;

	rc = smblib_get_iio_channel(chg, "smb_temp", &chg->iio.smb_temp_chan);
	if (rc < 0)
		return rc;

	return 0;
}

static int smb5_parse_dt_currents(struct smb5 *chip, struct device_node *node)
{
	int rc = 0, tmp;
	struct smb_charger *chg = &chip->chg;

	rc = of_property_read_u32(node,
			"qcom,fcc-max-ua", &chip->dt.batt_profile_fcc_ua);
	if (rc < 0)
		chip->dt.batt_profile_fcc_ua = -EINVAL;

	rc = of_property_read_u32(node,
				"qcom,usb-icl-ua", &chip->dt.usb_icl_ua);
	if (rc < 0)
		chip->dt.usb_icl_ua = -EINVAL;
	chg->dcp_icl_ua = chip->dt.usb_icl_ua;

	rc = of_property_read_u32(node,
				"qcom,otg-cl-ua", &chg->otg_cl_ua);
	if (rc < 0)
		chg->otg_cl_ua =
			(chip->chg.chg_param.smb_version == PMI632_SUBTYPE) ?
							MICRO_1PA : MICRO_3PA;

	rc = of_property_read_u32(node, "qcom,chg-term-src",
			&chip->dt.term_current_src);
	if (rc < 0)
		chip->dt.term_current_src = ITERM_SRC_UNSPECIFIED;

	if (chip->dt.term_current_src == ITERM_SRC_ADC)
		rc = of_property_read_u32(node, "qcom,chg-term-base-current-ma",
				&chip->dt.term_current_thresh_lo_ma);

	rc = of_property_read_u32(node, "qcom,chg-term-current-ma",
			&chip->dt.term_current_thresh_hi_ma);

	chg->wls_icl_ua = DCIN_ICL_MAX_UA;
	rc = of_property_read_u32(node, "qcom,wls-current-max-ua",
			&tmp);
	if (!rc && tmp < DCIN_ICL_MAX_UA)
		chg->wls_icl_ua = tmp;

	return 0;
}

static int smb5_parse_dt_voltages(struct smb5 *chip, struct device_node *node)
{
	int rc = 0;

	rc = of_property_read_u32(node,
				"qcom,fv-max-uv", &chip->dt.batt_profile_fv_uv);
	if (rc < 0)
		chip->dt.batt_profile_fv_uv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,chg-inhibit-threshold-mv",
				&chip->dt.chg_inhibit_thr_mv);
	if (!rc && (chip->dt.chg_inhibit_thr_mv < 0 ||
				chip->dt.chg_inhibit_thr_mv > 300)) {
		pr_err("qcom,chg-inhibit-threshold-mv is incorrect\n");
		return -EINVAL;
	}

	chip->dt.auto_recharge_vbat_mv = -EINVAL;
	rc = of_property_read_u32(node, "qcom,auto-recharge-vbat-mv",
				&chip->dt.auto_recharge_vbat_mv);
	if (!rc && (chip->dt.auto_recharge_vbat_mv < 0)) {
		pr_err("qcom,auto-recharge-vbat-mv is incorrect\n");
		return -EINVAL;
	}

	return 0;
}

//ASUS_BSP +++
struct drm_panel *active_panel_asus;
int smb5_parse_dt_panel(struct smb5 *chip, struct device_node *node)
{
	int i;
	int count;
	struct device_node *panel_node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(node, "panel", NULL);
	if (count <= 0)
		return -1;

	for (i = 0; i < count; i++) {
		panel_node = of_parse_phandle(node, "panel", i);
		panel = of_drm_find_panel(panel_node);
		of_node_put(panel_node);
		if (!IS_ERR(panel)) {
			active_panel_asus = panel;
			return 0;
		}
	}

	return -1;
}
//ASUS_BSP ---

static int smb5_parse_sdam(struct smb5 *chip, struct device_node *node)
{
	struct device_node *child;
	struct smb_charger *chg = &chip->chg;
	struct property *prop;
	const char *name;
	int rc;
	u32 base;
	u8 type;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "reg", prop, name) {
			rc = of_property_read_u32(child, "reg", &base);
			if (rc < 0) {
				pr_err("Failed to read base rc=%d\n", rc);
				return rc;
			}

			rc = smblib_read(chg, base + PERPH_TYPE_OFFSET, &type);
			if (rc < 0) {
				pr_err("Failed to read type rc=%d\n", rc);
				return rc;
			}

			switch (type) {
			case SDAM_TYPE:
				chg->sdam_base = base;
				break;
			default:
				break;
			}
		}
	}

	if (!chg->sdam_base)
		pr_debug("SDAM node not defined\n");

	return 0;
}

static int smb5_parse_dt(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	int rc = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

//ASUS_BSP +++
	rc = smb5_parse_dt_panel(chip, node);
	if (rc < 0)
		CHG_DBG_E("smb5_parse_dt_panel fail\n");
//ASUS_BSP ---

	rc = smb5_parse_dt_voltages(chip, node);
	if (rc < 0)
		return rc;

	rc = smb5_parse_dt_currents(chip, node);
	if (rc < 0)
		return rc;

	rc = smb5_parse_dt_adc_channels(chg);
	if (rc < 0)
		return rc;

	rc = smb5_parse_dt_misc(chip, node);
	if (rc < 0)
		return rc;

	rc = smb5_parse_sdam(chip, node);
	if (rc < 0)
		return rc;

	return 0;
}

static int smb5_set_prop_comp_clamp_level(struct smb_charger *chg,
			     const union power_supply_propval *val)
{
	int rc = 0, i;
	struct clamp_config clamp_config;
	enum comp_clamp_levels level;

	level = val->intval;
	if (level >= MAX_CLAMP_LEVEL) {
		pr_err("Invalid comp clamp level=%d\n", val->intval);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(clamp_config.reg); i++) {
		rc = smblib_write(chg, clamp_levels[level].reg[i],
			     clamp_levels[level].val[i]);
		if (rc < 0)
			dev_err(chg->dev,
				"Failed to configure comp clamp settings for reg=0x%04x rc=%d\n",
				   clamp_levels[level].reg[i], rc);
	}

	chg->comp_clamp_level = val->intval;

	return rc;
}

/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property smb5_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_PD_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_TYPEC_POWER_ROLE,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_LOW_POWER,
	POWER_SUPPLY_PROP_PD_ACTIVE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_BOOST_CURRENT,
	POWER_SUPPLY_PROP_PE_START,
	POWER_SUPPLY_PROP_CTM_CURRENT_MAX,
	POWER_SUPPLY_PROP_HW_CURRENT_MAX,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_PD_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_PD_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_CONNECTOR_TYPE,
	POWER_SUPPLY_PROP_CONNECTOR_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT,
	POWER_SUPPLY_PROP_SMB_EN_MODE,
	POWER_SUPPLY_PROP_SMB_EN_REASON,
	POWER_SUPPLY_PROP_ADAPTER_CC_MODE,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_MOISTURE_DETECTED,
	POWER_SUPPLY_PROP_HVDCP_OPTI_ALLOWED,
	POWER_SUPPLY_PROP_QC_OPTI_DISABLE,
	POWER_SUPPLY_PROP_VOLTAGE_VPH,
	POWER_SUPPLY_PROP_THERM_ICL_LIMIT,
	POWER_SUPPLY_PROP_SKIN_HEALTH,
	POWER_SUPPLY_PROP_APSD_RERUN,
	POWER_SUPPLY_PROP_APSD_TIMEOUT,
	POWER_SUPPLY_PROP_CHARGER_STATUS,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED,
	//[+++]Add the interface for charging debug apk
	POWER_SUPPLY_PROP_ASUS_APSD_RESULT,
	POWER_SUPPLY_PROP_ASUS_ADAPTER_ID,
	POWER_SUPPLY_PROP_ASUS_IS_LEGACY_CABLE,
	POWER_SUPPLY_PROP_ASUS_ICL_SETTING,
	POWER_SUPPLY_PROP_ASUS_TOTAL_FCC,
	//[---]Add the interface for charging debug apk
#ifdef CONFIG_USBPD_PHY_QCOM
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
#endif
/* ASUS Add POWER SUPPLY PROPERTY +++ */
	POWER_SUPPLY_PROP_PD2_ACTIVE,
/* ASUS Add POWER SUPPLY PROPERTY --- */
};

static int smb5_usb_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;
	u8 reg = 0, buff[2] = {0};
	u8 stat;  //Add the interface for charging debug apk
	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblib_get_prop_usb_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = smblib_get_usb_online(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		rc = smblib_get_prop_usb_voltage_max_design(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_get_prop_usb_voltage_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT:
		if (chg->usbin_forced_max_uv)
			val->intval = chg->usbin_forced_max_uv;
		else
			smblib_get_prop_usb_voltage_max_design(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = smblib_get_prop_usb_voltage_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable, PD_VOTER);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_input_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = chg->real_charger_type;
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		rc = smblib_get_usb_prop_typec_mode(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		rc = smblib_get_prop_typec_power_role(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		rc = smblib_get_prop_typec_cc_orientation(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_SRC_RP:
		rc = smblib_get_prop_typec_select_rp(chg, val);
		break;
	case POWER_SUPPLY_PROP_LOW_POWER:
		rc = smblib_get_prop_low_power(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		val->intval = chg->pd_active;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		rc = smblib_get_prop_usb_current_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_BOOST_CURRENT:
		val->intval = chg->boost_current_ua;
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		rc = smblib_get_prop_pd_in_hard_reset(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		val->intval = chg->system_suspend_supported;
		break;
	case POWER_SUPPLY_PROP_PE_START:
		rc = smblib_get_pe_start(chg, val);
		break;
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable, CTM_VOTER);
		break;
	case POWER_SUPPLY_PROP_HW_CURRENT_MAX:
		rc = smblib_get_charge_current(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_PR_SWAP:
		rc = smblib_get_prop_pr_swap_in_progress(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		val->intval = chg->voltage_max_uv;
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		val->intval = chg->voltage_min_uv;
		break;
	case POWER_SUPPLY_PROP_SDP_CURRENT_MAX:
		val->intval = get_client_vote(chg->usb_icl_votable,
					      USB_PSY_VOTER);
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TYPE:
		val->intval = chg->connector_type;
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		val->intval = smblib_get_prop_connector_health(chg);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		rc = smblib_get_prop_scope(chg, val);
		break;
	case POWER_SUPPLY_PROP_SMB_EN_MODE:
		mutex_lock(&chg->smb_lock);
		val->intval = chg->sec_chg_selected;
		mutex_unlock(&chg->smb_lock);
		break;
	case POWER_SUPPLY_PROP_SMB_EN_REASON:
		val->intval = chg->cp_reason;
		break;
	case POWER_SUPPLY_PROP_MOISTURE_DETECTED:
		val->intval = chg->moisture_present;
		break;
	case POWER_SUPPLY_PROP_HVDCP_OPTI_ALLOWED:
		val->intval = !chg->flash_active;
		break;
	case POWER_SUPPLY_PROP_QC_OPTI_DISABLE:
		if (chg->hw_die_temp_mitigation)
			val->intval = POWER_SUPPLY_QC_THERMAL_BALANCE_DISABLE
					| POWER_SUPPLY_QC_INOV_THERMAL_DISABLE;
		if (chg->hw_connector_mitigation)
			val->intval |= POWER_SUPPLY_QC_CTM_DISABLE;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_VPH:
		rc = smblib_get_prop_vph_voltage_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_THERM_ICL_LIMIT:
		val->intval = get_client_vote(chg->usb_icl_votable,
					THERMAL_THROTTLE_VOTER);
		break;
	case POWER_SUPPLY_PROP_ADAPTER_CC_MODE:
		val->intval = chg->adapter_cc_mode;
		break;
	case POWER_SUPPLY_PROP_SKIN_HEALTH:
		val->intval = smblib_get_skin_temp_status(chg);
		break;
	case POWER_SUPPLY_PROP_APSD_RERUN:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_APSD_TIMEOUT:
		val->intval = chg->apsd_ext_timeout;
		break;
	case POWER_SUPPLY_PROP_CHARGER_STATUS:
		val->intval = 0;
		if (chg->sdam_base) {
			rc = smblib_read(chg,
				chg->sdam_base + SDAM_QC_DET_STATUS_REG, &reg);
			if (!rc)
				val->intval = reg;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED:
		val->intval = 0;
		if (chg->sdam_base) {
			rc = smblib_batch_read(chg,
				chg->sdam_base + SDAM_QC_ADC_LSB_REG, buff, 2);
			if (!rc)
				val->intval = (buff[1] << 8 | buff[0]) * 1038;
		}
		break;
   	//[+++]Add the interface for charging debug apk
	case POWER_SUPPLY_PROP_ASUS_APSD_RESULT:
		val->intval = asus_get_apsd_result_by_bit();
		break;
	case POWER_SUPPLY_PROP_ASUS_ADAPTER_ID:
		val->intval = asus_get_prop_adapter_id();
		break;
	case POWER_SUPPLY_PROP_ASUS_IS_LEGACY_CABLE:
		val->intval = asus_get_prop_is_legacy_cable();
		break;
	case POWER_SUPPLY_PROP_ASUS_ICL_SETTING:
		rc = smblib_read(chg, USBIN_CURRENT_LIMIT_CFG_REG, &stat);
		if (rc < 0) {
			pr_err("Couldn't read USBIN_CURRENT_LIMIT_CFG_REG rc=%d\n", rc);
			return rc;
		}
		val->intval = stat*50;//every stage is 50mA
		break;
	case POWER_SUPPLY_PROP_ASUS_TOTAL_FCC:
		val->intval = asus_get_prop_total_fcc();
		break;
	//[---]Add the interface for charging debug apk
#ifdef CONFIG_USBPD_PHY_QCOM
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		rc = smblib_get_prop_charging_enabled(chg, val);
		break;
#endif
/* ASUS Add POWER SUPPLY PROPERTY +++ */
	case POWER_SUPPLY_PROP_PD2_ACTIVE:
		val->intval = chg->pd2_active;
		break;
/* ASUS Add POWER SUPPLY PROPERTY --- */
	default:
		pr_err("get prop %d is not supported in usb\n", psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

#define MIN_THERMAL_VOTE_UA	500000
static int smb5_usb_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int icl, rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		rc = smblib_set_prop_pd_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		rc = smblib_set_prop_typec_power_role(chg, val);
		break;
	case POWER_SUPPLY_PROP_TYPEC_SRC_RP:
		rc = smblib_set_prop_typec_select_rp(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		rc = smblib_set_prop_pd_active(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		rc = smblib_set_prop_pd_in_hard_reset(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		chg->system_suspend_supported = val->intval;
		break;
	case POWER_SUPPLY_PROP_BOOST_CURRENT:
		rc = smblib_set_prop_boost_current(chg, val);
		break;
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
		rc = vote(chg->usb_icl_votable, CTM_VOTER,
						val->intval >= 0, val->intval);
		break;
	case POWER_SUPPLY_PROP_PR_SWAP:
		rc = smblib_set_prop_pr_swap_in_progress(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		rc = smblib_set_prop_pd_voltage_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		rc = smblib_set_prop_pd_voltage_min(chg, val);
		break;
	case POWER_SUPPLY_PROP_SDP_CURRENT_MAX:
		rc = smblib_set_prop_sdp_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		chg->connector_health = val->intval;
		power_supply_changed(chg->usb_psy);
		break;
	case POWER_SUPPLY_PROP_THERM_ICL_LIMIT:
		if (!is_client_vote_enabled(chg->usb_icl_votable,
						THERMAL_THROTTLE_VOTER)) {
			chg->init_thermal_ua = get_effective_result(
							chg->usb_icl_votable);
			icl = chg->init_thermal_ua + val->intval;
		} else {
			icl = get_client_vote(chg->usb_icl_votable,
					THERMAL_THROTTLE_VOTER) + val->intval;
		}

		if (icl >= MIN_THERMAL_VOTE_UA)
			rc = vote(chg->usb_icl_votable, THERMAL_THROTTLE_VOTER,
				(icl != chg->init_thermal_ua) ? true : false,
				icl);
		else
			rc = -EINVAL;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT:
		smblib_set_prop_usb_voltage_max_limit(chg, val);
		break;
	case POWER_SUPPLY_PROP_ADAPTER_CC_MODE:
		chg->adapter_cc_mode = val->intval;
		break;
	case POWER_SUPPLY_PROP_APSD_RERUN:
		del_timer_sync(&chg->apsd_timer);
		chg->apsd_ext_timeout = false;
		smblib_rerun_apsd(chg);
		break;
#ifdef CONFIG_USBPD_PHY_QCOM
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		CHG_DBG("set_property POWER_SUPPLY_PROP_CURRENT_MAX\n");
		rc = smblib_set_prop_usb_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		rc = smblib_set_prop_charging_enabled(chg, val);
		break;
#endif
/* ASUS Add POWER SUPPLY PROPERTY +++ */
	case POWER_SUPPLY_PROP_PD2_ACTIVE:
		rc = smblib_set_prop_pd2_active(chg, val);
		break;
/* ASUS Add POWER SUPPLY PROPERTY --- */
	default:
		pr_err("set prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int smb5_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CTM_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
	case POWER_SUPPLY_PROP_THERM_ICL_LIMIT:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT:
	case POWER_SUPPLY_PROP_ADAPTER_CC_MODE:
	case POWER_SUPPLY_PROP_APSD_RERUN:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB_PD,
	.properties = smb5_usb_props,
	.num_properties = ARRAY_SIZE(smb5_usb_props),
	.get_property = smb5_usb_get_prop,
	.set_property = smb5_usb_set_prop,
	.property_is_writeable = smb5_usb_prop_is_writeable,
};

//ASUS_BSP +++
extern int asus_get_prop_pca_enable(struct smb_charger *chg);
int rt_charger_set_usb_property_notifier(enum power_supply_property psp, int value)
{
	union power_supply_propval val = {0};
	bool pogo_ovp_stats = gpio_get_value_cansleep(global_gpio->POGO_OVP_ACOK);
	int rc = 0;
	int rt_ufp = asus_get_bottom_ufp_mode();
	int pca_enable;	

	if (!pogo_ovp_stats && rt_ufp != 0) {
		CHG_DBG_E("Set dual_port_once_flag = 1\n");
		dual_port_once_flag = 1;
	}

	if (!pogo_ovp_stats && (psp == POWER_SUPPLY_PROP_PD_CURRENT_MAX || psp == POWER_SUPPLY_PROP_PD_VOLTAGE_MAX)) {
		CHG_DBG_E("POGO_OVP exist, do not set current/voltage property from BTM\n");
		return 0;
	}

	mutex_lock(&smbchg_dev->smb_lock);

	if (psp == POWER_SUPPLY_PROP_PD_CURRENT_MAX) {
		pca_enable = asus_get_prop_pca_enable(smbchg_dev);
		CHG_DBG("pca_enable = %d\n", pca_enable);
		if (pca_enable > 1) {
			CHG_DBG_E("BTM PCA working, do not set PD_CURRENT_MAX property from BTM\n");
			mutex_unlock(&smbchg_dev->smb_lock);
			return 0;
		}
	}

	val.intval = value;

	switch (psp) {
	case POWER_SUPPLY_PROP_PD2_ACTIVE:	//183
		rc = smblib_set_prop_pd2_active(smbchg_dev, &val);
		break;
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:	//123
		rc = smblib_set_prop_pd_current_max(smbchg_dev, &val);
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:	//142
		rc = smblib_set_prop_pd_voltage_max(smbchg_dev, &val);
		break;
	default:
		break;
	}
	CHG_DBG("Rt-charger set property %d, set value = %d, rc = %d", psp, value, rc);

	mutex_unlock(&smbchg_dev->smb_lock);
	return rc;
}
//ASUS_BSP ---

static int smb5_init_usb_psy(struct smb5 *chip)
{
	struct power_supply_config usb_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_cfg.drv_data = chip;
	usb_cfg.of_node = chg->dev->of_node;
	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}

	return 0;
}

/********************************
 * USB PC_PORT PSY REGISTRATION *
 ********************************/
static enum power_supply_property smb5_usb_port_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static int smb5_usb_port_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = smblib_get_prop_usb_online(chg, val);
		if (!val->intval)
			break;

		if (((chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT) ||
		   (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB))
			&& (chg->real_charger_type == POWER_SUPPLY_TYPE_USB))
			val->intval = 1;
//Add for BTM online state +++
		else if ((!gpio_get_value_cansleep(global_gpio->BTM_OVP_ACOK))
			&& (chg->real_charger_type == POWER_SUPPLY_TYPE_USB))
			val->intval = 1;
//Add for BTM online state ---
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	default:
		pr_err_ratelimited("Get prop %d is not supported in pc_port\n",
				psp);
		return -EINVAL;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int smb5_usb_port_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int rc = 0;

	switch (psp) {
	default:
		pr_err_ratelimited("Set prop %d is not supported in pc_port\n",
				psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static const struct power_supply_desc usb_port_psy_desc = {
	.name		= "pc_port",
	.type		= POWER_SUPPLY_TYPE_USB,
	.properties	= smb5_usb_port_props,
	.num_properties	= ARRAY_SIZE(smb5_usb_port_props),
	.get_property	= smb5_usb_port_get_prop,
	.set_property	= smb5_usb_port_set_prop,
};

static int smb5_init_usb_port_psy(struct smb5 *chip)
{
	struct power_supply_config usb_port_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_port_cfg.drv_data = chip;
	usb_port_cfg.of_node = chg->dev->of_node;
	chg->usb_port_psy = devm_power_supply_register(chg->dev,
						  &usb_port_psy_desc,
						  &usb_port_cfg);
	if (IS_ERR(chg->usb_port_psy)) {
		pr_err("Couldn't register USB pc_port power supply\n");
		return PTR_ERR(chg->usb_port_psy);
	}

	return 0;
}

/*****************************
 * USB MAIN PSY REGISTRATION *
 *****************************/

static enum power_supply_property smb5_usb_main_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED,
	POWER_SUPPLY_PROP_FCC_DELTA,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_FLASH_ACTIVE,
	POWER_SUPPLY_PROP_FLASH_TRIGGER,
	POWER_SUPPLY_PROP_TOGGLE_STAT,
	POWER_SUPPLY_PROP_MAIN_FCC_MAX,
	POWER_SUPPLY_PROP_IRQ_STATUS,
	POWER_SUPPLY_PROP_FORCE_MAIN_FCC,
	POWER_SUPPLY_PROP_FORCE_MAIN_ICL,
	POWER_SUPPLY_PROP_COMP_CLAMP_LEVEL,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_HOT_TEMP,
#ifdef CONFIG_USBPD_PHY_QCOM
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
#endif
};

static int smb5_usb_main_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_get_charge_param(chg, &chg->param.fv, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smblib_get_charge_param(chg, &chg->param.fcc,
							&val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_MAIN;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		rc = smblib_get_prop_input_current_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_SETTLED:
		rc = smblib_get_prop_input_voltage_settled(chg, val);
		break;
	case POWER_SUPPLY_PROP_FCC_DELTA:
		rc = smblib_get_prop_fcc_delta(chg, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_icl_current(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_FLASH_ACTIVE:
		val->intval = chg->flash_active;
		break;
	case POWER_SUPPLY_PROP_FLASH_TRIGGER:
		val->intval = 0;
		if (chg->chg_param.smb_version == PMI632_SUBTYPE)
			rc = schgm_flash_get_vreg_ok(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TOGGLE_STAT:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_MAIN_FCC_MAX:
		val->intval = chg->main_fcc_max;
		break;
	case POWER_SUPPLY_PROP_IRQ_STATUS:
		rc = smblib_get_irq_status(chg, val);
		break;
	case POWER_SUPPLY_PROP_FORCE_MAIN_FCC:
		rc = smblib_get_charge_param(chg, &chg->param.fcc,
							&val->intval);
		break;
	case POWER_SUPPLY_PROP_FORCE_MAIN_ICL:
		rc = smblib_get_charge_param(chg, &chg->param.usb_icl,
							&val->intval);
		break;
	case POWER_SUPPLY_PROP_COMP_CLAMP_LEVEL:
		val->intval = chg->comp_clamp_level;
		break;
	/* Use this property to report SMB health */
	case POWER_SUPPLY_PROP_HEALTH:
		rc = val->intval = smblib_get_prop_smb_health(chg);
		break;
	/* Use this property to report overheat status */
	case POWER_SUPPLY_PROP_HOT_TEMP:
		val->intval = chg->thermal_overheat;
		break;
#ifdef CONFIG_USBPD_PHY_QCOM
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		rc = smblib_get_prop_charging_enabled(chg, val);
		break;
#endif
	default:
		pr_debug("get prop %d is not supported in usb-main\n", psp);
		rc = -EINVAL;
		break;
	}
	if (rc < 0)
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);

	return rc;
}

static int smb5_usb_main_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval pval = {0, };
	enum power_supply_type real_chg_type = chg->real_charger_type;
	int rc = 0, offset_ua = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		CHG_DBG("set voltage_max: %d\n", val->intval);
		rc = smblib_set_charge_param(chg, &chg->param.fv, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		CHG_DBG("set constant charge_current: %d\n", val->intval);
		/* Adjust Main FCC for QC3.0 + SMB1390 */
		rc = smblib_get_qc3_main_icl_offset(chg, &offset_ua);
		if (rc < 0)
			offset_ua = 0;

		rc = smblib_set_charge_param(chg, &chg->param.fcc,
						val->intval + offset_ua);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		CHG_DBG("set current_max: %d\n", val->intval);
		rc = smblib_set_icl_current(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_FLASH_ACTIVE:
		if ((chg->chg_param.smb_version == PMI632_SUBTYPE)
				&& (chg->flash_active != val->intval)) {
			chg->flash_active = val->intval;

			rc = smblib_get_prop_usb_present(chg, &pval);
			if (rc < 0)
				pr_err("Failed to get USB preset status rc=%d\n",
						rc);
			if (pval.intval) {
				rc = smblib_force_vbus_voltage(chg,
					chg->flash_active ? FORCE_5V_BIT
								: IDLE_BIT);
				if (rc < 0)
					pr_err("Failed to force 5V\n");
				else
					chg->pulse_cnt = 0;
			} else {
				/* USB absent & flash not-active - vote 100mA */
				vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER,
							true, SDP_CURRENT_UA);	// ASUS BSP 100->500 +++
			}

			pr_debug("flash active VBUS 5V restriction %s\n",
				chg->flash_active ? "applied" : "removed");

			/* Update userspace */
			if (chg->batt_psy)
				power_supply_changed(chg->batt_psy);
		}
		break;
	case POWER_SUPPLY_PROP_TOGGLE_STAT:
		rc = smblib_toggle_smb_en(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_MAIN_FCC_MAX:
		chg->main_fcc_max = val->intval;
		rerun_election(chg->fcc_votable);
		break;
	case POWER_SUPPLY_PROP_FORCE_MAIN_FCC:
		vote_override(chg->fcc_main_votable, CC_MODE_VOTER,
				(val->intval < 0) ? false : true, val->intval);
		if (val->intval >= 0)
			chg->chg_param.forced_main_fcc = val->intval;
		/*
		 * Remove low vote on FCC_MAIN, for WLS, to allow FCC_MAIN to
		 * rise to its full value.
		 */
		if (val->intval < 0)
			vote(chg->fcc_main_votable, WLS_PL_CHARGING_VOTER,
								false, 0);
		/* Main FCC updated re-calculate FCC */
		rerun_election(chg->fcc_votable);
		break;
	case POWER_SUPPLY_PROP_FORCE_MAIN_ICL:
		vote_override(chg->usb_icl_votable, CC_MODE_VOTER,
				(val->intval < 0) ? false : true, val->intval);
		/* Main ICL updated re-calculate ILIM */
		if (real_chg_type == POWER_SUPPLY_TYPE_USB_HVDCP_3 ||
			real_chg_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5)
			rerun_election(chg->fcc_votable);
		break;
	case POWER_SUPPLY_PROP_COMP_CLAMP_LEVEL:
		rc = smb5_set_prop_comp_clamp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_HOT_TEMP:
		rc = smblib_set_prop_thermal_overheat(chg, val->intval);
		break;
#ifdef CONFIG_USBPD_PHY_QCOM
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		rc = smblib_set_prop_charging_enabled(chg, val);
		break;
#endif
	default:
		pr_err("set prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int smb5_usb_main_prop_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_TOGGLE_STAT:
	case POWER_SUPPLY_PROP_MAIN_FCC_MAX:
	case POWER_SUPPLY_PROP_FORCE_MAIN_FCC:
	case POWER_SUPPLY_PROP_FORCE_MAIN_ICL:
	case POWER_SUPPLY_PROP_COMP_CLAMP_LEVEL:
	case POWER_SUPPLY_PROP_HOT_TEMP:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc usb_main_psy_desc = {
	.name		= "main",
	.type		= POWER_SUPPLY_TYPE_MAIN,
	.properties	= smb5_usb_main_props,
	.num_properties	= ARRAY_SIZE(smb5_usb_main_props),
	.get_property	= smb5_usb_main_get_prop,
	.set_property	= smb5_usb_main_set_prop,
	.property_is_writeable = smb5_usb_main_prop_is_writeable,
};

static int smb5_init_usb_main_psy(struct smb5 *chip)
{
	struct power_supply_config usb_main_cfg = {};
	struct smb_charger *chg = &chip->chg;

	usb_main_cfg.drv_data = chip;
	usb_main_cfg.of_node = chg->dev->of_node;
	chg->usb_main_psy = devm_power_supply_register(chg->dev,
						  &usb_main_psy_desc,
						  &usb_main_cfg);
	if (IS_ERR(chg->usb_main_psy)) {
		pr_err("Couldn't register USB main power supply\n");
		return PTR_ERR(chg->usb_main_psy);
	}

	return 0;
}

/*************************
 * DC PSY REGISTRATION   *
 *************************/

static enum power_supply_property smb5_dc_props[] = {
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_DC_RESET,
	POWER_SUPPLY_PROP_AICL_DONE,
};

static int smb5_dc_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		val->intval = get_effective_result(chg->dc_suspend_votable);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblib_get_prop_dc_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = smblib_get_prop_dc_online(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = smblib_get_prop_dc_voltage_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_get_prop_dc_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smblib_get_prop_dc_voltage_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = POWER_SUPPLY_TYPE_WIRELESS;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		rc = smblib_get_prop_voltage_wls_output(chg, val);
		break;
	case POWER_SUPPLY_PROP_DC_RESET:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_AICL_DONE:
		val->intval = chg->dcin_aicl_done;
		break;
	default:
		return -EINVAL;
	}
	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}
	return 0;
}

static int smb5_dc_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct smb5 *chip = power_supply_get_drvdata(psy);
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = vote(chg->dc_suspend_votable, WBC_VOTER,
				(bool)val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smblib_set_prop_dc_current_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		rc = smblib_set_prop_voltage_wls_output(chg, val);
		break;
	case POWER_SUPPLY_PROP_DC_RESET:
		rc = smblib_set_prop_dc_reset(chg);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int smb5_dc_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc dc_psy_desc = {
	.name = "dc",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = smb5_dc_props,
	.num_properties = ARRAY_SIZE(smb5_dc_props),
	.get_property = smb5_dc_get_prop,
	.set_property = smb5_dc_set_prop,
	.property_is_writeable = smb5_dc_prop_is_writeable,
};

static int smb5_init_dc_psy(struct smb5 *chip)
{
	struct power_supply_config dc_cfg = {};
	struct smb_charger *chg = &chip->chg;

	dc_cfg.drv_data = chip;
	dc_cfg.of_node = chg->dev->of_node;
	chg->dc_psy = devm_power_supply_register(chg->dev,
						  &dc_psy_desc,
						  &dc_cfg);
	if (IS_ERR(chg->dc_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->dc_psy);
	}

	return 0;
}

/*************************
 * BATT PSY REGISTRATION *
 *************************/
static enum power_supply_property smb5_batt_props[] = {
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGER_TEMP,
	POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_QNOVO,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_QNOVO,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_SW_JEITA_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_DONE,
	POWER_SUPPLY_PROP_PARALLEL_DISABLE,
	POWER_SUPPLY_PROP_SET_SHIP_MODE,
	POWER_SUPPLY_PROP_DIE_HEALTH,
	POWER_SUPPLY_PROP_RERUN_AICL,
	POWER_SUPPLY_PROP_DP_DM,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_RECHARGE_SOC,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_FORCE_RECHARGE,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE,
};

#define DEBUG_ACCESSORY_TEMP_DECIDEGC	250
static int smb5_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct smb_charger *chg = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smblib_get_prop_batt_status(chg, val);
		//ASUS BSP : Show "+" on charging icon +++
		if (g_smb5_probe_complete)
			set_qc_stat(val);
		//ASUS BSP : Show "+" on charging icon ---
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = smblib_get_prop_batt_health(chg, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = smblib_get_prop_batt_present(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smblib_get_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = smblib_get_prop_batt_charge_type(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smblib_get_prop_batt_capacity(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = smblib_get_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		rc = smblib_get_prop_system_temp_level_max(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP:
		rc = smblib_get_prop_charger_temp(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
		val->intval = chg->charger_temp_max;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		rc = smblib_get_prop_input_current_limited(chg, val);
		break;
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		val->intval = chg->step_chg_enabled;
		break;
	case POWER_SUPPLY_PROP_SW_JEITA_ENABLED:
		val->intval = chg->sw_jeita_enabled;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = get_client_vote(chg->fv_votable,
					      QNOVO_VOTER);
		if (val->intval < 0)
			val->intval = get_client_vote(chg->fv_votable,
						      BATT_PROFILE_VOTER);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_QNOVO:
		val->intval = get_client_vote_locked(chg->fv_votable,
				QNOVO_VOTER);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = smblib_get_batt_current_now(chg, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_QNOVO:
		val->intval = get_client_vote_locked(chg->fcc_votable,
				QNOVO_VOTER);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_client_vote(chg->fcc_votable,
					      BATT_PROFILE_VOTER);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = get_effective_result(chg->fcc_votable);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		rc = smblib_get_prop_batt_iterm(chg, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chg->typec_mode == POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY)
			val->intval = DEBUG_ACCESSORY_TEMP_DECIDEGC;
		else
			rc = smblib_get_prop_from_bms(chg,
						POWER_SUPPLY_PROP_TEMP, val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		rc = smblib_get_prop_batt_charge_done(chg, val);
		break;
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
		val->intval = get_client_vote(chg->pl_disable_votable,
					      USER_VOTER);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as device is active */
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		rc = smblib_get_die_health(chg, val);
		break;
	case POWER_SUPPLY_PROP_DP_DM:
		val->intval = chg->pulse_cnt;
		break;
	case POWER_SUPPLY_PROP_RERUN_AICL:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CHARGE_COUNTER, val);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CYCLE_COUNT, val);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		val->intval = chg->auto_recharge_soc;
		break;
	case POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE:
		val->intval = 0;
		if (!chg->qnovo_disable_votable)
			chg->qnovo_disable_votable =
				find_votable("QNOVO_DISABLE");

		if (chg->qnovo_disable_votable)
			val->intval =
				!get_effective_result(
					chg->qnovo_disable_votable);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CHARGE_FULL, val);
		break;
	case POWER_SUPPLY_PROP_FORCE_RECHARGE:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_TIME_TO_FULL_NOW, val);
		break;
	case POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE:
		val->intval = chg->fcc_stepper_enable;
		break;
	default:
		pr_err("batt power supply prop %d not supported\n", psp);
		return -EINVAL;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int smb5_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct smb_charger *chg = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smblib_set_prop_batt_status(chg, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = smblib_set_prop_input_suspend(chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = smblib_set_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smblib_set_prop_batt_capacity(chg, val);
		break;
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
		vote(chg->pl_disable_votable, USER_VOTER, (bool)val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		chg->batt_profile_fv_uv = val->intval;
		vote(chg->fv_votable, BATT_PROFILE_VOTER, true, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_QNOVO:
		if (val->intval == -EINVAL) {
			vote(chg->fv_votable, BATT_PROFILE_VOTER, true,
					chg->batt_profile_fv_uv);
			vote(chg->fv_votable, QNOVO_VOTER, false, 0);
		} else {
			vote(chg->fv_votable, QNOVO_VOTER, true, val->intval);
			vote(chg->fv_votable, BATT_PROFILE_VOTER, false, 0);
		}
		break;
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
		chg->step_chg_enabled = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		chg->batt_profile_fcc_ua = val->intval;
		vote(chg->fcc_votable, BATT_PROFILE_VOTER, true, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_QNOVO:
		vote(chg->pl_disable_votable, PL_QNOVO_VOTER,
			val->intval != -EINVAL && val->intval < 2000000, 0);
		if (val->intval == -EINVAL) {
			vote(chg->fcc_votable, BATT_PROFILE_VOTER,
					true, chg->batt_profile_fcc_ua);
			vote(chg->fcc_votable, QNOVO_VOTER, false, 0);
		} else {
			vote(chg->fcc_votable, QNOVO_VOTER, true, val->intval);
			vote(chg->fcc_votable, BATT_PROFILE_VOTER, false, 0);
		}
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as the device is active */
		if (!val->intval)
			break;
		if (chg->pl.psy)
			power_supply_set_property(chg->pl.psy,
				POWER_SUPPLY_PROP_SET_SHIP_MODE, val);
		rc = smblib_set_prop_ship_mode(chg, val);
		break;
	case POWER_SUPPLY_PROP_RERUN_AICL:
		rc = smblib_run_aicl(chg, RERUN_AICL);
		break;
	case POWER_SUPPLY_PROP_DP_DM:
		if (!chg->flash_active)
			rc = smblib_dp_dm(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
		rc = smblib_set_prop_input_current_limited(chg, val);
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		chg->die_health = val->intval;
		power_supply_changed(chg->batt_psy);
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		rc = smblib_set_prop_rechg_soc_thresh(chg, val);
		break;
	case POWER_SUPPLY_PROP_FORCE_RECHARGE:
			/* toggle charging to force recharge */
			vote(chg->chg_disable_votable, FORCE_RECHARGE_VOTER,
					true, 0);
			/* charge disable delay */
			msleep(50);
			vote(chg->chg_disable_votable, FORCE_RECHARGE_VOTER,
					false, 0);
		break;
	case POWER_SUPPLY_PROP_FCC_STEPPER_ENABLE:
		chg->fcc_stepper_enable = val->intval;
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static int smb5_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
	case POWER_SUPPLY_PROP_DP_DM:
	case POWER_SUPPLY_PROP_RERUN_AICL:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
	case POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_DIE_HEALTH:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = smb5_batt_props,
	.num_properties = ARRAY_SIZE(smb5_batt_props),
	.get_property = smb5_batt_get_prop,
	.set_property = smb5_batt_set_prop,
	.property_is_writeable = smb5_batt_prop_is_writeable,
};

static int smb5_init_batt_psy(struct smb5 *chip)
{
	struct power_supply_config batt_cfg = {};
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;
	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);
	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}

	return rc;
}

/******************************
 * VBUS REGULATOR REGISTRATION *
 ******************************/

static struct regulator_ops smb5_vbus_reg_ops = {
	.enable = smblib_vbus_regulator_enable,
	.disable = smblib_vbus_regulator_disable,
	.is_enabled = smblib_vbus_regulator_is_enabled,
};

static int smb5_init_vbus_regulator(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct regulator_config cfg = {};
	int rc = 0;

	chg->vbus_vreg = devm_kzalloc(chg->dev, sizeof(*chg->vbus_vreg),
				      GFP_KERNEL);
	if (!chg->vbus_vreg)
		return -ENOMEM;

	cfg.dev = chg->dev;
	cfg.driver_data = chip;

	chg->vbus_vreg->rdesc.owner = THIS_MODULE;
	chg->vbus_vreg->rdesc.type = REGULATOR_VOLTAGE;
	chg->vbus_vreg->rdesc.ops = &smb5_vbus_reg_ops;
	chg->vbus_vreg->rdesc.of_match = "qcom,smb5-vbus";
	chg->vbus_vreg->rdesc.name = "qcom,smb5-vbus";

	chg->vbus_vreg->rdev = devm_regulator_register(chg->dev,
						&chg->vbus_vreg->rdesc, &cfg);
	if (IS_ERR(chg->vbus_vreg->rdev)) {
		rc = PTR_ERR(chg->vbus_vreg->rdev);
		chg->vbus_vreg->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't register VBUS regulator rc=%d\n", rc);
	}

	return rc;
}

/******************************
 * VCONN REGULATOR REGISTRATION *
 ******************************/

static struct regulator_ops smb5_vconn_reg_ops = {
	.enable = smblib_vconn_regulator_enable,
	.disable = smblib_vconn_regulator_disable,
	.is_enabled = smblib_vconn_regulator_is_enabled,
};

static int smb5_init_vconn_regulator(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct regulator_config cfg = {};
	int rc = 0;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	chg->vconn_vreg = devm_kzalloc(chg->dev, sizeof(*chg->vconn_vreg),
				      GFP_KERNEL);
	if (!chg->vconn_vreg)
		return -ENOMEM;

	cfg.dev = chg->dev;
	cfg.driver_data = chip;

	chg->vconn_vreg->rdesc.owner = THIS_MODULE;
	chg->vconn_vreg->rdesc.type = REGULATOR_VOLTAGE;
	chg->vconn_vreg->rdesc.ops = &smb5_vconn_reg_ops;
	chg->vconn_vreg->rdesc.of_match = "qcom,smb5-vconn";
	chg->vconn_vreg->rdesc.name = "qcom,smb5-vconn";

	chg->vconn_vreg->rdev = devm_regulator_register(chg->dev,
						&chg->vconn_vreg->rdesc, &cfg);
	if (IS_ERR(chg->vconn_vreg->rdev)) {
		rc = PTR_ERR(chg->vconn_vreg->rdev);
		chg->vconn_vreg->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			pr_err("Couldn't register VCONN regulator rc=%d\n", rc);
	}

	return rc;
}

/***************************
 * HARDWARE INITIALIZATION *
 ***************************/
static int smb5_configure_typec(struct smb_charger *chg)
{
	union power_supply_propval pval = {0, };
	int rc;
	u8 val = 0;

	rc = smblib_read(chg, LEGACY_CABLE_STATUS_REG, &val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't read Legacy status rc=%d\n", rc);
		return rc;
	}

	/*
	 * Across reboot, standard typeC cables get detected as legacy
	 * cables due to VBUS attachment prior to CC attach/detach. Reset
	 * the legacy detection logic by enabling/disabling the typeC mode.
	 */
	if (val & TYPEC_LEGACY_CABLE_STATUS_BIT) {
		pval.intval = POWER_SUPPLY_TYPEC_PR_NONE;
		rc = smblib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable TYPEC rc=%d\n", rc);
			return rc;
		}

		/* delay before enabling typeC */
		msleep(50);

		pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		rc = smblib_set_prop_typec_power_role(chg, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable TYPEC rc=%d\n", rc);
			return rc;
		}
	}

	smblib_apsd_enable(chg, true);

	rc = smblib_read(chg, TYPE_C_SNK_STATUS_REG, &val);
	if (rc < 0) {
		dev_err(chg->dev, "failed to read TYPE_C_SNK_STATUS_REG rc=%d\n",
				rc);

		return rc;
	}

	if (!(val & SNK_DAM_MASK)) {
		rc = smblib_masked_write(chg, TYPE_C_CFG_REG,
					BC1P2_START_ON_CC_BIT, 0);
		if (rc < 0) {
			dev_err(chg->dev, "failed to write TYPE_C_CFG_REG rc=%d\n",
					rc);

			return rc;
		}
	}

	/* Use simple write to clear interrupts */
	rc = smblib_write(chg, TYPE_C_INTERRUPT_EN_CFG_1_REG, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	val = chg->lpd_disabled ? 0 : TYPEC_WATER_DETECTION_INT_EN_BIT;
	/* Use simple write to enable only required interrupts */
	rc = smblib_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
				TYPEC_SRC_BATT_HPWR_INT_EN_BIT | val);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	/* enable try.snk and clear force sink for DRP mode */
	rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				EN_TRY_SNK_BIT | EN_SNK_ONLY_BIT,
				EN_TRY_SNK_BIT);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure TYPE_C_MODE_CFG_REG rc=%d\n", rc);
		return rc;
	}
	chg->typec_try_mode |= EN_TRY_SNK_BIT;

	/* For PD capable targets configure VCONN for software control */
	if (!chg->pd_not_supported) {
		rc = smblib_masked_write(chg, TYPE_C_VCONN_CONTROL_REG,
				 VCONN_EN_SRC_BIT | VCONN_EN_VALUE_BIT,
				 VCONN_EN_SRC_BIT);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't configure VCONN for SW control rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chg->chg_param.smb_version != PMI632_SUBTYPE) {
		/*
		 * Enable detection of unoriented debug
		 * accessory in source mode.
		 */
		rc = smblib_masked_write(chg, DEBUG_ACCESS_SRC_CFG_REG,
					 EN_UNORIENTED_DEBUG_ACCESS_SRC_BIT,
					 EN_UNORIENTED_DEBUG_ACCESS_SRC_BIT);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't configure TYPE_C_DEBUG_ACCESS_SRC_CFG_REG rc=%d\n",
					rc);
			return rc;
		}

		rc = smblib_masked_write(chg, USBIN_LOAD_CFG_REG,
				USBIN_IN_COLLAPSE_GF_SEL_MASK |
				USBIN_AICL_STEP_TIMING_SEL_MASK,
				0);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't set USBIN_LOAD_CFG_REG rc=%d\n", rc);
			return rc;
		}
	}

	/* Set CC threshold to 1.6 V in source mode */
	rc = smblib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG,
				SEL_SRC_UPPER_REF_BIT, SEL_SRC_UPPER_REF_BIT);
	if (rc < 0)
		dev_err(chg->dev,
			"Couldn't configure CC threshold voltage rc=%d\n", rc);

	return rc;
}

static int smb5_configure_micro_usb(struct smb_charger *chg)
{
	int rc;

	/* For micro USB connector, use extcon by default */
	chg->use_extcon = true;
	chg->pd_not_supported = true;

	rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
					MICRO_USB_STATE_CHANGE_INT_EN_BIT,
					MICRO_USB_STATE_CHANGE_INT_EN_BIT);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure Type-C interrupts rc=%d\n", rc);
		return rc;
	}

	if (chg->uusb_moisture_protection_enabled) {
		/* Enable moisture detection interrupt */
		rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
				TYPEC_WATER_DETECTION_INT_EN_BIT,
				TYPEC_WATER_DETECTION_INT_EN_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable moisture detection interrupt rc=%d\n",
				rc);
			return rc;
		}

		/* Enable uUSB factory mode */
		rc = smblib_masked_write(chg, TYPEC_U_USB_CFG_REG,
					EN_MICRO_USB_FACTORY_MODE_BIT,
					EN_MICRO_USB_FACTORY_MODE_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable uUSB factory mode c=%d\n",
				rc);
			return rc;
		}

		/* Disable periodic monitoring of CC_ID pin */
		rc = smblib_write(chg,
			((chg->chg_param.smb_version == PMI632_SUBTYPE) ?
				PMI632_TYPEC_U_USB_WATER_PROTECTION_CFG_REG :
				TYPEC_U_USB_WATER_PROTECTION_CFG_REG), 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable periodic monitoring of CC_ID rc=%d\n",
				rc);
			return rc;
		}
	}

	/* Enable HVDCP detection and authentication */
	if (!chg->hvdcp_disable)
		smblib_hvdcp_detect_enable(chg, true);

	return rc;
}

#define RAW_ITERM(iterm_ma, max_range)				\
		div_s64((int64_t)iterm_ma * ADC_CHG_ITERM_MASK, max_range)
static int smb5_configure_iterm_thresholds_adc(struct smb5 *chip)
{
	u8 *buf;
	int rc = 0;
	s16 raw_hi_thresh, raw_lo_thresh, max_limit_ma;
	struct smb_charger *chg = &chip->chg;

	if (chip->chg.chg_param.smb_version == PMI632_SUBTYPE)
		max_limit_ma = ITERM_LIMITS_PMI632_MA;
	else
		max_limit_ma = ITERM_LIMITS_PM8150B_MA;

	if (chip->dt.term_current_thresh_hi_ma < (-1 * max_limit_ma)
		|| chip->dt.term_current_thresh_hi_ma > max_limit_ma
		|| chip->dt.term_current_thresh_lo_ma < (-1 * max_limit_ma)
		|| chip->dt.term_current_thresh_lo_ma > max_limit_ma) {
		dev_err(chg->dev, "ITERM threshold out of range rc=%d\n", rc);
		return -EINVAL;
	}

	/*
	 * Conversion:
	 *	raw (A) = (term_current * ADC_CHG_ITERM_MASK) / max_limit_ma
	 * Note: raw needs to be converted to big-endian format.
	 */

	if (chip->dt.term_current_thresh_hi_ma) {
		raw_hi_thresh = RAW_ITERM(chip->dt.term_current_thresh_hi_ma,
					max_limit_ma);
		raw_hi_thresh = sign_extend32(raw_hi_thresh, 15);
		buf = (u8 *)&raw_hi_thresh;
		raw_hi_thresh = buf[1] | (buf[0] << 8);

		rc = smblib_batch_write(chg, CHGR_ADC_ITERM_UP_THD_MSB_REG,
				(u8 *)&raw_hi_thresh, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ITERM threshold HIGH rc=%d\n",
					rc);
			return rc;
		}
	}

	if (chip->dt.term_current_thresh_lo_ma) {
		raw_lo_thresh = RAW_ITERM(chip->dt.term_current_thresh_lo_ma,
					max_limit_ma);
		raw_lo_thresh = sign_extend32(raw_lo_thresh, 15);
		buf = (u8 *)&raw_lo_thresh;
		raw_lo_thresh = buf[1] | (buf[0] << 8);

		rc = smblib_batch_write(chg, CHGR_ADC_ITERM_LO_THD_MSB_REG,
				(u8 *)&raw_lo_thresh, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ITERM threshold LOW rc=%d\n",
					rc);
			return rc;
		}
	}

	return rc;
}

static int smb5_configure_iterm_thresholds(struct smb5 *chip)
{
	int rc = 0;
	struct smb_charger *chg = &chip->chg;

	switch (chip->dt.term_current_src) {
	case ITERM_SRC_ADC:
		if (chip->chg.chg_param.smb_version == PM8150B_SUBTYPE) {
			rc = smblib_masked_write(chg, CHGR_ADC_TERM_CFG_REG,
					TERM_BASED_ON_SYNC_CONV_OR_SAMPLE_CNT,
					TERM_BASED_ON_SAMPLE_CNT);
			if (rc < 0) {
				dev_err(chg->dev, "Couldn't configure ADC_ITERM_CFG rc=%d\n",
						rc);
				return rc;
			}
		}
		rc = smb5_configure_iterm_thresholds_adc(chip);
		break;
	default:
		break;
	}

	return rc;
}

static int smb5_configure_mitigation(struct smb_charger *chg)
{
	int rc;
	u8 chan = 0, src_cfg = 0;

	if (!chg->hw_die_temp_mitigation && !chg->hw_connector_mitigation &&
			!chg->hw_skin_temp_mitigation) {
		src_cfg = THERMREG_SW_ICL_ADJUST_BIT;
	} else {
		if (chg->hw_die_temp_mitigation) {
			chan = DIE_TEMP_CHANNEL_EN_BIT;
			src_cfg = THERMREG_DIE_ADC_SRC_EN_BIT
				| THERMREG_DIE_CMP_SRC_EN_BIT;
		}

		if (chg->hw_connector_mitigation) {
			chan |= CONN_THM_CHANNEL_EN_BIT;
			src_cfg |= THERMREG_CONNECTOR_ADC_SRC_EN_BIT;
		}

		if (chg->hw_skin_temp_mitigation) {
			chan |= MISC_THM_CHANNEL_EN_BIT;
			src_cfg |= THERMREG_SKIN_ADC_SRC_EN_BIT;
		}

		rc = smblib_masked_write(chg, BATIF_ADC_CHANNEL_EN_REG,
			CONN_THM_CHANNEL_EN_BIT | DIE_TEMP_CHANNEL_EN_BIT |
			MISC_THM_CHANNEL_EN_BIT, chan);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't enable ADC channel rc=%d\n",
				rc);
			return rc;
		}
	}

	rc = smblib_masked_write(chg, MISC_THERMREG_SRC_CFG_REG,
		THERMREG_SW_ICL_ADJUST_BIT | THERMREG_DIE_ADC_SRC_EN_BIT |
		THERMREG_DIE_CMP_SRC_EN_BIT | THERMREG_SKIN_ADC_SRC_EN_BIT |
		SKIN_ADC_CFG_BIT | THERMREG_CONNECTOR_ADC_SRC_EN_BIT, src_cfg);
	if (rc < 0) {
		dev_err(chg->dev,
				"Couldn't configure THERM_SRC reg rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int smb5_init_dc_peripheral(struct smb_charger *chg)
{
	int rc = 0;

	/* PMI632 does not have DC peripheral */
	if (chg->chg_param.smb_version == PMI632_SUBTYPE)
		return 0;

	/* Set DCIN ICL to 100 mA */
	rc = smblib_set_charge_param(chg, &chg->param.dc_icl, DCIN_ICL_MIN_UA);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set dc_icl rc=%d\n", rc);
		return rc;
	}

	/* Disable DC Input missing poller function */
	rc = smblib_masked_write(chg, DCIN_LOAD_CFG_REG,
					INPUT_MISS_POLL_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't disable DC Input missing poller rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int smb5_configure_recharging(struct smb5 *chip)
{
	int rc = 0;
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval pval;
	/* Configure VBATT-based or automatic recharging */

	rc = smblib_masked_write(chg, CHGR_CFG2_REG, RECHG_MASK,
				(chip->dt.auto_recharge_vbat_mv != -EINVAL) ?
				VBAT_BASED_RECHG_BIT : 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure VBAT-rechg CHG_CFG2_REG rc=%d\n",
			rc);
		return rc;
	}

	/* program the auto-recharge VBAT threshold */
	if (chip->dt.auto_recharge_vbat_mv != -EINVAL) {
		u32 temp = VBAT_TO_VRAW_ADC(chip->dt.auto_recharge_vbat_mv);

		temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
		rc = smblib_batch_write(chg,
			CHGR_ADC_RECHARGE_THRESHOLD_MSB_REG, (u8 *)&temp, 2);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure ADC_RECHARGE_THRESHOLD REG rc=%d\n",
				rc);
			return rc;
		}
		/* Program the sample count for VBAT based recharge to 3 */
		rc = smblib_masked_write(chg, CHGR_NO_SAMPLE_TERM_RCHG_CFG_REG,
					NO_OF_SAMPLE_FOR_RCHG,
					2 << NO_OF_SAMPLE_FOR_RCHG_SHIFT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHGR_NO_SAMPLE_FOR_TERM_RCHG_CFG rc=%d\n",
				rc);
			return rc;
		}
	}

	rc = smblib_masked_write(chg, CHGR_CFG2_REG, RECHG_MASK,
				(chip->dt.auto_recharge_soc != -EINVAL) ?
				SOC_BASED_RECHG_BIT : VBAT_BASED_RECHG_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure SOC-rechg CHG_CFG2_REG rc=%d\n",
			rc);
		return rc;
	}

	/* program the auto-recharge threshold */
	if (chip->dt.auto_recharge_soc != -EINVAL) {
		pval.intval = chip->dt.auto_recharge_soc;
		rc = smblib_set_prop_rechg_soc_thresh(chg, &pval);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHG_RCHG_SOC_REG rc=%d\n",
					rc);
			return rc;
		}

		/* Program the sample count for SOC based recharge to 1 */
		rc = smblib_masked_write(chg, CHGR_NO_SAMPLE_TERM_RCHG_CFG_REG,
						NO_OF_SAMPLE_FOR_RCHG, 0);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure CHGR_NO_SAMPLE_FOR_TERM_RCHG_CFG rc=%d\n",
				rc);
			return rc;
		}
	}

	return 0;
}

static int smb5_configure_float_charger(struct smb5 *chip)
{
	int rc = 0;
	u8 val = 0;
	struct smb_charger *chg = &chip->chg;

	/* configure float charger options */
	switch (chip->dt.float_option) {
	case FLOAT_SDP:
		val = FORCE_FLOAT_SDP_CFG_BIT;
		break;
	case DISABLE_CHARGING:
		val = FLOAT_DIS_CHGING_CFG_BIT;
		break;
	case SUSPEND_INPUT:
		val = SUSPEND_FLOAT_CFG_BIT;
		break;
	case FLOAT_DCP:
	default:
		val = 0;
		break;
	}

	chg->float_cfg = val;
	/* Update float charger setting and set DCD timeout 300ms */
	rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				FLOAT_OPTIONS_MASK | DCD_TIMEOUT_SEL_BIT, val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't change float charger setting rc=%d\n",
			rc);
		return rc;
	}

	return 0;
}

static int smb5_init_connector_type(struct smb_charger *chg)
{
	int rc, type = 0;
	u8 val = 0;

	/*
	 * PMI632 can have the connector type defined by a dedicated register
	 * PMI632_TYPEC_MICRO_USB_MODE_REG or by a common TYPEC_U_USB_CFG_REG.
	 */
	if (chg->chg_param.smb_version == PMI632_SUBTYPE) {
		rc = smblib_read(chg, PMI632_TYPEC_MICRO_USB_MODE_REG, &val);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't read USB mode rc=%d\n", rc);
			return rc;
		}
		type = !!(val & MICRO_USB_MODE_ONLY_BIT);
	}

	/*
	 * If PMI632_TYPEC_MICRO_USB_MODE_REG is not set and for all non-PMI632
	 * check the connector type using TYPEC_U_USB_CFG_REG.
	 */
	if (!type) {
		rc = smblib_read(chg, TYPEC_U_USB_CFG_REG, &val);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't read U_USB config rc=%d\n",
					rc);
			return rc;
		}

		type = !!(val & EN_MICRO_USB_MODE_BIT);
	}

	pr_debug("Connector type=%s\n", type ? "Micro USB" : "TypeC");

	if (type) {
		chg->connector_type = POWER_SUPPLY_CONNECTOR_MICRO_USB;
		rc = smb5_configure_micro_usb(chg);
	} else {
		chg->connector_type = POWER_SUPPLY_CONNECTOR_TYPEC;
		rc = smb5_configure_typec(chg);
	}
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure TypeC/micro-USB mode rc=%d\n", rc);
		return rc;
	}

	/*
	 * PMI632 based hw init:
	 * - Rerun APSD to ensure proper charger detection if device
	 *   boots with charger connected.
	 * - Initialize flash module for PMI632
	 */
	if (chg->chg_param.smb_version == PMI632_SUBTYPE) {
		schgm_flash_init(chg);
		smblib_rerun_apsd_if_required(chg);
	}

	return 0;

}

static int smb5_init_hw(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc;
	u8 val = 0, mask = 0, buf[2] = {0};

	if (chip->dt.no_battery)
		chg->fake_capacity = 50;

	if (chg->sdam_base) {
		rc = smblib_write(chg,
			chg->sdam_base + SDAM_QC_DET_STATUS_REG, 0);
		if (rc < 0)
			pr_err("Couldn't clear SDAM QC status rc=%d\n", rc);

		rc = smblib_batch_write(chg,
			chg->sdam_base + SDAM_QC_ADC_LSB_REG, buf, 2);
		if (rc < 0)
			pr_err("Couldn't clear SDAM ADC status rc=%d\n", rc);
	}

	if (chip->dt.batt_profile_fcc_ua < 0)
		smblib_get_charge_param(chg, &chg->param.fcc,
				&chg->batt_profile_fcc_ua);

	if (chip->dt.batt_profile_fv_uv < 0)
		smblib_get_charge_param(chg, &chg->param.fv,
				&chg->batt_profile_fv_uv);

	smblib_get_charge_param(chg, &chg->param.usb_icl,
				&chg->default_icl_ua);
	smblib_get_charge_param(chg, &chg->param.aicl_5v_threshold,
				&chg->default_aicl_5v_threshold_mv);
	chg->aicl_5v_threshold_mv = chg->default_aicl_5v_threshold_mv;
	smblib_get_charge_param(chg, &chg->param.aicl_cont_threshold,
				&chg->default_aicl_cont_threshold_mv);
	chg->aicl_cont_threshold_mv = chg->default_aicl_cont_threshold_mv;

	if (chg->charger_temp_max == -EINVAL) {
		rc = smblib_get_thermal_threshold(chg,
					DIE_REG_H_THRESHOLD_MSB_REG,
					&chg->charger_temp_max);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't get charger_temp_max rc=%d\n",
					rc);
			return rc;
		}
	}

	/*
	 * If SW thermal regulation WA is active then all the HW temperature
	 * comparators need to be disabled to prevent HW thermal regulation,
	 * apart from DIE_TEMP analog comparator for SHDN regulation.
	 */
	if (chg->wa_flags & SW_THERM_REGULATION_WA) {
		rc = smblib_write(chg, MISC_THERMREG_SRC_CFG_REG,
					THERMREG_SW_ICL_ADJUST_BIT
					| THERMREG_DIE_CMP_SRC_EN_BIT);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't disable HW thermal regulation rc=%d\n",
				rc);
			return rc;
		}
	} else {
		/* configure temperature mitigation */
		rc = smb5_configure_mitigation(chg);
		if (rc < 0) {
			dev_err(chg->dev, "Couldn't configure mitigation rc=%d\n",
					rc);
			return rc;
		}
	}

	/* Set HVDCP autonomous mode per DT option */
	smblib_hvdcp_hw_inov_enable(chg, chip->dt.hvdcp_autonomous);

	/* Enable HVDCP authentication algorithm for non-PD designs */
	if (chg->pd_not_supported)
		smblib_hvdcp_detect_enable(chg, true);

	/* Disable HVDCP and authentication algorithm if specified in DT */
	if (chg->hvdcp_disable)
		smblib_hvdcp_detect_enable(chg, false);

	rc = smb5_init_connector_type(chg);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure connector type rc=%d\n",
				rc);
		return rc;
	}

	/* Use ICL results from HW */
	rc = smblib_icl_override(chg, HW_AUTO_MODE);
	if (rc < 0) {
		pr_err("Couldn't disable ICL override rc=%d\n", rc);
		return rc;
	}

	/* set OTG current limit */
	rc = smblib_set_charge_param(chg, &chg->param.otg_cl, chg->otg_cl_ua);
	if (rc < 0) {
		pr_err("Couldn't set otg current limit rc=%d\n", rc);
		return rc;
	}

	/* vote 0mA on usb_icl for non battery platforms */
	vote(chg->usb_icl_votable,
		DEFAULT_VOTER, chip->dt.no_battery, 0);
	vote(chg->dc_suspend_votable,
		DEFAULT_VOTER, chip->dt.no_battery, 0);
	vote(chg->fcc_votable, HW_LIMIT_VOTER,
		chip->dt.batt_profile_fcc_ua > 0, chip->dt.batt_profile_fcc_ua);
	vote(chg->fv_votable, HW_LIMIT_VOTER,
		chip->dt.batt_profile_fv_uv > 0, chip->dt.batt_profile_fv_uv);
	vote(chg->fcc_votable,
		BATT_PROFILE_VOTER, chg->batt_profile_fcc_ua > 0,
		chg->batt_profile_fcc_ua);
	vote(chg->fv_votable,
		BATT_PROFILE_VOTER, chg->batt_profile_fv_uv > 0,
		chg->batt_profile_fv_uv);
#ifdef CONFIG_USBPD_PHY_QCOM
		/* enable switching charger */
		vote(chg->chg_disable_votable, DIRECT_CHARGE_VOTER, false, 0);
#endif

	/* Some h/w limit maximum supported ICL */
	vote(chg->usb_icl_votable, HW_LIMIT_VOTER,
			chg->hw_max_icl_ua > 0, chg->hw_max_icl_ua);

	/* Initialize DC peripheral configurations */
	rc = smb5_init_dc_peripheral(chg);
	if (rc < 0)
		return rc;

	/*
	 * AICL configuration: enable aicl and aicl rerun and based on DT
	 * configuration enable/disable ADB based AICL and Suspend on collapse.
	 */
	mask = USBIN_AICL_PERIODIC_RERUN_EN_BIT | USBIN_AICL_ADC_EN_BIT
			| USBIN_AICL_EN_BIT | SUSPEND_ON_COLLAPSE_USBIN_BIT;
	val = USBIN_AICL_PERIODIC_RERUN_EN_BIT | USBIN_AICL_EN_BIT;
	if (!chip->dt.disable_suspend_on_collapse)
		val |= SUSPEND_ON_COLLAPSE_USBIN_BIT;
	if (chip->dt.adc_based_aicl)
		val |= USBIN_AICL_ADC_EN_BIT;

	rc = smblib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG,
			mask, val);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't config AICL rc=%d\n", rc);
		return rc;
	}

	rc = smblib_write(chg, AICL_RERUN_TIME_CFG_REG,
				AICL_RERUN_TIME_3S_VAL);		// ASUS BSP : set from 12s to 3s +++
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure AICL rerun interval rc=%d\n", rc);
		return rc;
	}

	/* enable the charging path */
	rc = vote(chg->chg_disable_votable, DEFAULT_VOTER, false, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable charging rc=%d\n", rc);
		return rc;
	}

	/* configure VBUS for software control */
	rc = smblib_masked_write(chg, DCDC_OTG_CFG_REG, OTG_EN_SRC_CFG_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure VBUS for SW control rc=%d\n", rc);
		return rc;
	}

	val = (ilog2(chip->dt.wd_bark_time / 16) << BARK_WDOG_TIMEOUT_SHIFT)
			& BARK_WDOG_TIMEOUT_MASK;
	val |= (BITE_WDOG_TIMEOUT_8S | BITE_WDOG_DISABLE_CHARGING_CFG_BIT);
	val |= (chip->dt.wd_snarl_time_cfg << SNARL_WDOG_TIMEOUT_SHIFT)
			& SNARL_WDOG_TIMEOUT_MASK;

	rc = smblib_masked_write(chg, SNARL_BARK_BITE_WD_CFG_REG,
			BITE_WDOG_DISABLE_CHARGING_CFG_BIT |
			SNARL_WDOG_TIMEOUT_MASK | BARK_WDOG_TIMEOUT_MASK |
			BITE_WDOG_TIMEOUT_MASK,
			val);
	if (rc < 0) {
		pr_err("Couldn't configue WD config rc=%d\n", rc);
		return rc;
	}

	/* enable WD BARK and enable it on plugin */
	val = WDOG_TIMER_EN_ON_PLUGIN_BIT | BARK_WDOG_INT_EN_BIT;
	rc = smblib_masked_write(chg, WD_CFG_REG,
			WATCHDOG_TRIGGER_AFP_EN_BIT |
			WDOG_TIMER_EN_ON_PLUGIN_BIT |
			BARK_WDOG_INT_EN_BIT, val);
	if (rc < 0) {
		pr_err("Couldn't configue WD config rc=%d\n", rc);
		return rc;
	}

	/* set termination current threshold values */
	rc = smb5_configure_iterm_thresholds(chip);
	if (rc < 0) {
		pr_err("Couldn't configure ITERM thresholds rc=%d\n",
				rc);
		return rc;
	}

	rc = smb5_configure_float_charger(chip);
	if (rc < 0)
		return rc;

	switch (chip->dt.chg_inhibit_thr_mv) {
	case 50:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				INHIBIT_ANALOG_VFLT_MINUS_50MV);
		break;
	case 100:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				INHIBIT_ANALOG_VFLT_MINUS_100MV);
		break;
	case 200:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				INHIBIT_ANALOG_VFLT_MINUS_200MV);
		break;
	case 300:
		rc = smblib_masked_write(chg, CHARGE_INHIBIT_THRESHOLD_CFG_REG,
				CHARGE_INHIBIT_THRESHOLD_MASK,
				INHIBIT_ANALOG_VFLT_MINUS_300MV);
		break;
	case 0:
		rc = smblib_masked_write(chg, CHGR_CFG2_REG,
				CHARGER_INHIBIT_BIT, 0);
	default:
		break;
	}

	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure charge inhibit threshold rc=%d\n",
			rc);
		return rc;
	}

	rc = smblib_write(chg, CHGR_FAST_CHARGE_SAFETY_TIMER_CFG_REG,
					FAST_CHARGE_SAFETY_TIMER_768_MIN);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set CHGR_FAST_CHARGE_SAFETY_TIMER_CFG_REG rc=%d\n",
			rc);
		return rc;
	}

	rc = smb5_configure_recharging(chip);
	if (rc < 0)
		return rc;

	rc = smblib_disable_hw_jeita(chg, true);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set hw jeita rc=%d\n", rc);
		return rc;
	}

	rc = smblib_masked_write(chg, DCDC_ENG_SDCDC_CFG5_REG,
			ENG_SDCDC_BAT_HPWR_MASK, BOOST_MODE_THRESH_3P6_V);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure DCDC_ENG_SDCDC_CFG5 rc=%d\n",
				rc);
		return rc;
	}

	if (chg->connector_pull_up != -EINVAL) {
		rc = smb5_configure_internal_pull(chg, CONN_THERM,
				get_valid_pullup(chg->connector_pull_up));
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't configure CONN_THERM pull-up rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chg->smb_pull_up != -EINVAL) {
		rc = smb5_configure_internal_pull(chg, SMB_THERM,
				get_valid_pullup(chg->smb_pull_up));
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't configure SMB pull-up rc=%d\n",
				rc);
			return rc;
		}
	}

	return rc;
}

static int smb5_post_init(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval pval;
	int rc;

	/*
	 * In case the usb path is suspended, we would have missed disabling
	 * the icl change interrupt because the interrupt could have been
	 * not requested
	 */
	rerun_election(chg->usb_icl_votable);

	/* configure power role for dual-role */
	pval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
	rc = smblib_set_prop_typec_power_role(chg, &pval);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure DRP role rc=%d\n",
				rc);
		return rc;
	}

	rerun_election(chg->temp_change_irq_disable_votable);

	return 0;
}

/****************************
 * DETERMINE INITIAL STATUS *
 ****************************/

static int smb5_determine_initial_status(struct smb5 *chip)
{
	struct smb_irq_data irq_data = {chip, "determine-initial-status"};
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval val;
	int rc;

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		return rc;
	}
	chg->early_usb_attach = val.intval;

	if (chg->bms_psy)
		smblib_suspend_on_debug_battery(chg);

	usb_plugin_irq_handler(0, &irq_data);
	dc_plugin_irq_handler(0, &irq_data);
	typec_attach_detach_irq_handler(0, &irq_data);
	typec_state_change_irq_handler(0, &irq_data);
	usb_source_change_irq_handler(0, &irq_data);
	chg_state_change_irq_handler(0, &irq_data);
	icl_change_irq_handler(0, &irq_data);
	batt_temp_changed_irq_handler(0, &irq_data);
	wdog_bark_irq_handler(0, &irq_data);
	typec_or_rid_detection_change_irq_handler(0, &irq_data);
	wdog_snarl_irq_handler(0, &irq_data);

	return 0;
}

/**************************
 * INTERRUPT REGISTRATION *
 **************************/

static struct smb_irq_info smb5_irqs[] = {
	/* CHARGER IRQs */
	[CHGR_ERROR_IRQ] = {
		.name		= "chgr-error",
		.handler	= default_irq_handler,
	},
	[CHG_STATE_CHANGE_IRQ] = {
		.name		= "chg-state-change",
		.handler	= chg_state_change_irq_handler,
		.wake		= true,
	},
	[STEP_CHG_STATE_CHANGE_IRQ] = {
		.name		= "step-chg-state-change",
	},
	[STEP_CHG_SOC_UPDATE_FAIL_IRQ] = {
		.name		= "step-chg-soc-update-fail",
	},
	[STEP_CHG_SOC_UPDATE_REQ_IRQ] = {
		.name		= "step-chg-soc-update-req",
	},
	[FG_FVCAL_QUALIFIED_IRQ] = {
		.name		= "fg-fvcal-qualified",
	},
	[VPH_ALARM_IRQ] = {
		.name		= "vph-alarm",
	},
	[VPH_DROP_PRECHG_IRQ] = {
		.name		= "vph-drop-prechg",
	},
	/* DCDC IRQs */
	[OTG_FAIL_IRQ] = {
		.name		= "otg-fail",
		.handler	= default_irq_handler,
	},
	[OTG_OC_DISABLE_SW_IRQ] = {
		.name		= "otg-oc-disable-sw",
	},
	[OTG_OC_HICCUP_IRQ] = {
		.name		= "otg-oc-hiccup",
	},
	[BSM_ACTIVE_IRQ] = {
		.name		= "bsm-active",
	},
	[HIGH_DUTY_CYCLE_IRQ] = {
		.name		= "high-duty-cycle",
		.handler	= high_duty_cycle_irq_handler,
		.wake		= true,
	},
	[INPUT_CURRENT_LIMITING_IRQ] = {
		.name		= "input-current-limiting",
		.handler	= default_irq_handler,
	},
	[CONCURRENT_MODE_DISABLE_IRQ] = {
		.name		= "concurrent-mode-disable",
	},
	[SWITCHER_POWER_OK_IRQ] = {
		.name		= "switcher-power-ok",
		.handler	= switcher_power_ok_irq_handler,
	},
	/* BATTERY IRQs */
	[BAT_TEMP_IRQ] = {
		.name		= "bat-temp",
		.handler	= batt_temp_changed_irq_handler,
		.wake		= true,
	},
	[ALL_CHNL_CONV_DONE_IRQ] = {
		.name		= "all-chnl-conv-done",
	},
	[BAT_OV_IRQ] = {
		.name		= "bat-ov",
		.handler	= batt_psy_changed_irq_handler,
	},
	[BAT_LOW_IRQ] = {
		.name		= "bat-low",
		.handler	= batt_psy_changed_irq_handler,
	},
	[BAT_THERM_OR_ID_MISSING_IRQ] = {
		.name		= "bat-therm-or-id-missing",
		.handler	= batt_psy_changed_irq_handler,
	},
	[BAT_TERMINAL_MISSING_IRQ] = {
		.name		= "bat-terminal-missing",
		.handler	= batt_psy_changed_irq_handler,
	},
	[BUCK_OC_IRQ] = {
		.name		= "buck-oc",
	},
	[VPH_OV_IRQ] = {
		.name		= "vph-ov",
	},
	/* USB INPUT IRQs */
	[USBIN_COLLAPSE_IRQ] = {
		.name		= "usbin-collapse",
		.handler	= default_irq_handler,
	},
	[USBIN_VASHDN_IRQ] = {
		.name		= "usbin-vashdn",
		.handler	= default_irq_handler,
	},
	[USBIN_UV_IRQ] = {
		.name		= "usbin-uv",
		.handler	= usbin_uv_irq_handler,
		.wake		= true,
		.storm_data	= {true, 3000, 5},
	},
	[USBIN_OV_IRQ] = {
		.name		= "usbin-ov",
		.handler	= usbin_ov_irq_handler,
	},
	[USBIN_PLUGIN_IRQ] = {
		.name		= "usbin-plugin",
		.handler	= usb_plugin_irq_handler,
		.wake           = true,
	},
	[USBIN_REVI_CHANGE_IRQ] = {
		.name		= "usbin-revi-change",
	},
	[USBIN_SRC_CHANGE_IRQ] = {
		.name		= "usbin-src-change",
		.handler	= usb_source_change_irq_handler,
		.wake           = true,
	},
	[USBIN_ICL_CHANGE_IRQ] = {
		.name		= "usbin-icl-change",
		.handler	= icl_change_irq_handler,
		.wake           = true,
	},
	/* DC INPUT IRQs */
	[DCIN_VASHDN_IRQ] = {
		.name		= "dcin-vashdn",
	},
	[DCIN_UV_IRQ] = {
		.name		= "dcin-uv",
		.handler	= dcin_uv_irq_handler,
		.wake		= true,
	},
	[DCIN_OV_IRQ] = {
		.name		= "dcin-ov",
		.handler	= default_irq_handler,
	},
	[DCIN_PLUGIN_IRQ] = {
		.name		= "dcin-plugin",
		.handler	= dc_plugin_irq_handler,
		.wake           = true,
	},
	[DCIN_REVI_IRQ] = {
		.name		= "dcin-revi",
	},
	[DCIN_PON_IRQ] = {
		.name		= "dcin-pon",
		.handler	= default_irq_handler,
	},
	[DCIN_EN_IRQ] = {
		.name		= "dcin-en",
		.handler	= default_irq_handler,
	},
	/* TYPEC IRQs */
	[TYPEC_OR_RID_DETECTION_CHANGE_IRQ] = {
		.name		= "typec-or-rid-detect-change",
		.handler	= typec_or_rid_detection_change_irq_handler,
		.wake           = true,
	},
	[TYPEC_VPD_DETECT_IRQ] = {
		.name		= "typec-vpd-detect",
	},
	[TYPEC_CC_STATE_CHANGE_IRQ] = {
		.name		= "typec-cc-state-change",
		.handler	= typec_state_change_irq_handler,
		.wake           = true,
	},
	[TYPEC_VCONN_OC_IRQ] = {
		.name		= "typec-vconn-oc",
		.handler	= default_irq_handler,
	},
	[TYPEC_VBUS_CHANGE_IRQ] = {
		.name		= "typec-vbus-change",
	},
	[TYPEC_ATTACH_DETACH_IRQ] = {
		.name		= "typec-attach-detach",
		.handler	= typec_attach_detach_irq_handler,
		.wake		= true,
	},
	[TYPEC_LEGACY_CABLE_DETECT_IRQ] = {
		.name		= "typec-legacy-cable-detect",
		.handler	= default_irq_handler,
	},
	[TYPEC_TRY_SNK_SRC_DETECT_IRQ] = {
		.name		= "typec-try-snk-src-detect",
	},
	/* MISCELLANEOUS IRQs */
	[WDOG_SNARL_IRQ] = {
		.name		= "wdog-snarl",
		.handler	= wdog_snarl_irq_handler,
		.wake		= true,
	},
	[WDOG_BARK_IRQ] = {
		.name		= "wdog-bark",
		.handler	= wdog_bark_irq_handler,
		.wake		= true,
	},
	[AICL_FAIL_IRQ] = {
		.name		= "aicl-fail",
	},
	[AICL_DONE_IRQ] = {
		.name		= "aicl-done",
		.handler	= default_irq_handler,
	},
	[SMB_EN_IRQ] = {
		.name		= "smb-en",
		.handler	= smb_en_irq_handler,
	},
	[IMP_TRIGGER_IRQ] = {
		.name		= "imp-trigger",
	},
	/*
	 * triggered when DIE or SKIN or CONNECTOR temperature across
	 * either of the _REG_L, _REG_H, _RST, or _SHDN thresholds
	 */
	[TEMP_CHANGE_IRQ] = {
		.name		= "temp-change",
		.handler	= temp_change_irq_handler,
		.wake		= true,
	},
	[TEMP_CHANGE_SMB_IRQ] = {
		.name		= "temp-change-smb",
	},
	/* FLASH */
	[VREG_OK_IRQ] = {
		.name		= "vreg-ok",
	},
	[ILIM_S2_IRQ] = {
		.name		= "ilim2-s2",
		.handler	= schgm_flash_ilim2_irq_handler,
	},
	[ILIM_S1_IRQ] = {
		.name		= "ilim1-s1",
	},
	[VOUT_DOWN_IRQ] = {
		.name		= "vout-down",
	},
	[VOUT_UP_IRQ] = {
		.name		= "vout-up",
	},
	[FLASH_STATE_CHANGE_IRQ] = {
		.name		= "flash-state-change",
		.handler	= schgm_flash_state_change_irq_handler,
	},
	[TORCH_REQ_IRQ] = {
		.name		= "torch-req",
	},
	[FLASH_EN_IRQ] = {
		.name		= "flash-en",
	},
	/* SDAM */
	[SDAM_STS_IRQ] = {
		.name		= "sdam-sts",
		.handler	= sdam_sts_change_irq_handler,
	},
};

static int smb5_get_irq_index_byname(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb5_irqs); i++) {
		if (strcmp(smb5_irqs[i].name, irq_name) == 0)
			return i;
	}

	return -ENOENT;
}

static int smb5_request_interrupt(struct smb5 *chip,
				struct device_node *node, const char *irq_name)
{
	struct smb_charger *chg = &chip->chg;
	int rc, irq, irq_index;
	struct smb_irq_data *irq_data;

	irq = of_irq_get_byname(node, irq_name);
	if (irq < 0) {
		pr_err("Couldn't get irq %s byname\n", irq_name);
		return irq;
	}

	irq_index = smb5_get_irq_index_byname(irq_name);
	if (irq_index < 0) {
		pr_err("%s is not a defined irq\n", irq_name);
		return irq_index;
	}

	if (!smb5_irqs[irq_index].handler)
		return 0;

	irq_data = devm_kzalloc(chg->dev, sizeof(*irq_data), GFP_KERNEL);
	if (!irq_data)
		return -ENOMEM;

	irq_data->parent_data = chip;
	irq_data->name = irq_name;
	irq_data->storm_data = smb5_irqs[irq_index].storm_data;
	mutex_init(&irq_data->storm_data.storm_lock);

	smb5_irqs[irq_index].enabled = true;
	rc = devm_request_threaded_irq(chg->dev, irq, NULL,
					smb5_irqs[irq_index].handler,
					IRQF_ONESHOT, irq_name, irq_data);
	if (rc < 0) {
		pr_err("Couldn't request irq %d\n", irq);
		return rc;
	}

	smb5_irqs[irq_index].irq = irq;
	smb5_irqs[irq_index].irq_data = irq_data;
	if (smb5_irqs[irq_index].wake)
		enable_irq_wake(irq);

	return rc;
}

static int smb5_request_interrupts(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	struct device_node *node = chg->dev->of_node;
	struct device_node *child;
	int rc = 0;
	const char *name;
	struct property *prop;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names",
					    prop, name) {
			rc = smb5_request_interrupt(chip, child, name);
			if (rc < 0)
				return rc;
		}
	}

	vote(chg->limited_irq_disable_votable, CHARGER_TYPE_VOTER, true, 0);
	vote(chg->hdc_irq_disable_votable, CHARGER_TYPE_VOTER, true, 0);

	return rc;
}

static void smb5_free_interrupts(struct smb_charger *chg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb5_irqs); i++) {
		if (smb5_irqs[i].irq > 0) {
			if (smb5_irqs[i].wake)
				disable_irq_wake(smb5_irqs[i].irq);

			devm_free_irq(chg->dev, smb5_irqs[i].irq,
						smb5_irqs[i].irq_data);
		}
	}
}

static void smb5_disable_interrupts(struct smb_charger *chg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smb5_irqs); i++) {
		if (smb5_irqs[i].irq > 0)
			disable_irq(smb5_irqs[i].irq);
	}
}

#if defined(CONFIG_DEBUG_FS)

static int force_batt_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->batt_psy);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(force_batt_psy_update_ops, NULL,
			force_batt_psy_update_write, "0x%02llx\n");

static int force_usb_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->usb_psy);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(force_usb_psy_update_ops, NULL,
			force_usb_psy_update_write, "0x%02llx\n");

static int force_dc_psy_update_write(void *data, u64 val)
{
	struct smb_charger *chg = data;

	power_supply_changed(chg->dc_psy);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(force_dc_psy_update_ops, NULL,
			force_dc_psy_update_write, "0x%02llx\n");

static void smb5_create_debugfs(struct smb5 *chip)
{
	struct dentry *file;

	chip->dfs_root = debugfs_create_dir("charger", NULL);
	if (IS_ERR_OR_NULL(chip->dfs_root)) {
		pr_err("Couldn't create charger debugfs rc=%ld\n",
			(long)chip->dfs_root);
		return;
	}

	file = debugfs_create_file("force_batt_psy_update", 0600,
			    chip->dfs_root, chip, &force_batt_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_batt_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_file("force_usb_psy_update", 0600,
			    chip->dfs_root, chip, &force_usb_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_usb_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_file("force_dc_psy_update", 0600,
			    chip->dfs_root, chip, &force_dc_psy_update_ops);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create force_dc_psy_update file rc=%ld\n",
			(long)file);

	file = debugfs_create_u32("debug_mask", 0600, chip->dfs_root,
			&__debug_mask);
	if (IS_ERR_OR_NULL(file))
		pr_err("Couldn't create debug_mask file rc=%ld\n", (long)file);
}

#else

static void smb5_create_debugfs(struct smb5 *chip)
{}

#endif

static int smb5_show_charger_status(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	union power_supply_propval val;
	int usb_present, batt_present, batt_health, batt_charge_type;
	int rc;

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get usb present rc=%d\n", rc);
		return rc;
	}
	usb_present = val.intval;

	rc = smblib_get_prop_batt_present(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt present rc=%d\n", rc);
		return rc;
	}
	batt_present = val.intval;

	rc = smblib_get_prop_batt_health(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt health rc=%d\n", rc);
		val.intval = POWER_SUPPLY_HEALTH_UNKNOWN;
	}
	batt_health = val.intval;

	rc = smblib_get_prop_batt_charge_type(chg, &val);
	if (rc < 0) {
		pr_err("Couldn't get batt charge type rc=%d\n", rc);
		return rc;
	}
	batt_charge_type = val.intval;

	pr_info("SMB5 status - usb:present=%d type=%d batt:present = %d health = %d charge = %d\n",
		usb_present, chg->real_charger_type,
		batt_present, batt_health, batt_charge_type);
	return rc;
}

/*********************************
 * TYPEC CLASS REGISTRATION *
 **********************************/

static int smb5_init_typec_class(struct smb5 *chip)
{
	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	mutex_init(&chg->typec_lock);

	/* Register typec class for only non-PD TypeC and uUSB designs */
	if (!chg->pd_not_supported)
		return rc;

	chg->typec_caps.type = TYPEC_PORT_DRP;
	chg->typec_caps.data = TYPEC_PORT_DRD;
	chg->typec_partner_desc.usb_pd = false;
	chg->typec_partner_desc.accessory = TYPEC_ACCESSORY_NONE;
	chg->typec_caps.port_type_set = smblib_typec_port_type_set;
	chg->typec_caps.revision = 0x0130;

	chg->typec_port = typec_register_port(chg->dev, &chg->typec_caps);
	if (IS_ERR(chg->typec_port)) {
		rc = PTR_ERR(chg->typec_port);
		pr_err("failed to register typec_port rc=%d\n", rc);
		return rc;
	}

	return rc;
}

//ASUS BSP : Show "+" on charging icon +++
struct extcon_dev qc_stat;
#define SWITCH_THERMAL_ALER                 7
#define SWITCH_NXP_NOT_QUICK_CHARGING       6
#define SWITCH_NXP_QUICK_CHARGING           5
#define SWITCH_QC_NOT_QUICK_CHARGING        4
#define SWITCH_QC_QUICK_CHARGING            3
#define SWITCH_QC_NOT_QUICK_CHARGING_PLUS   2
#define SWITCH_QC_QUICK_CHARGING_PLUS       1
#define SWITCH_QC_OTHER	                    0
void set_qc_stat(union power_supply_propval *val)
{
	int stat;
	int set = SWITCH_QC_OTHER;
	int asus_status;

	stat = val->intval;
	asus_status = asus_get_batt_status();

	if (asus_status == NORMAL) {
		set = SWITCH_QC_OTHER;
		CHG_DBG("stat: %d, switch: %d\n", stat, set);
		asus_extcon_set_state_sync(smbchg_dev->quickchg_extcon, set);
		return;
	}

	switch (stat) {
	//"qc" stat happends in charger mode only, refer to smblib_get_prop_batt_status
	case POWER_SUPPLY_STATUS_CHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_QUICK_CHARGING:
	case POWER_SUPPLY_STATUS_QUICK_CHARGING_PLUS:
		if(cos_alert_once_flag) {
			set = SWITCH_THERMAL_ALER;
		}else {
			if (asus_status == NXP)
				set = SWITCH_NXP_QUICK_CHARGING;
			else if (asus_status == QC)
				set = SWITCH_QC_QUICK_CHARGING;
			else if (asus_status == QC_PLUS)
				set = SWITCH_QC_QUICK_CHARGING_PLUS;
		}
		asus_extcon_set_state_sync(smbchg_dev->quickchg_extcon, set);
		break;
	case POWER_SUPPLY_STATUS_THERMAL_ALERT:
	case POWER_SUPPLY_STATUS_THERMAL_ALERT_CABLE_OUT:
		set = SWITCH_THERMAL_ALER;
		asus_extcon_set_state_sync(smbchg_dev->quickchg_extcon, set);
		break;
	default:
		set = SWITCH_QC_OTHER;
		asus_extcon_set_state_sync(smbchg_dev->quickchg_extcon, set);
		break;
	}

	CHG_DBG("stat: %d, switch: %d\n", stat, set);
	return;
}
//ASUS BSP : Show "+" on charging icon +++

void asus_probe_pmic_settings(struct smb_charger *chg)
{
	int rc;

//A-1:0x1153
	rc = smblib_masked_write(chg, DCDC_OTG_CFG_REG, OTG_EN_SRC_CFG_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default DCDC_OTG_CFG_REG rc=%d\n", rc);
	}
//A-2:0x1070
	rc = smblib_write(chg, CHGR_FLOAT_VOLTAGE_CFG_REG, 0x4C);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default FLOAT_VOLTAGE_CFG_REG rc=%d\n", rc);
	}
/*A-3: set tcc & stcc in dtsi */
//A-3:0x1063
	rc = smblib_write(chg, TCCC_CHARGE_CURRENT_TERMINATION_CFG_REG, 0x06);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default TCCC_CHARGE_CURRENT_TERMINATION_CFG_REG rc=%d\n", rc);
	}
//A-4:0x1552
	rc = smblib_write(chg, TYPE_C_CURRSRC_CFG_REG, 0x00);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default TYPE_C_CURRSRC_CFG_REG rc=%d\n", rc);
	}
//A-5:0x1152
	/*rc = smblib_write(chg, DCDC_OTG_CURRENT_LIMIT_CFG_REG, 0x00);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default DCDC_OTG_CURRENT_LIMIT_CFG_REG rc=%d\n", rc);
	}*/
//A-12:0x1090
	rc = smblib_write(chg, JEITA_EN_CFG_REG, 0x10);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default JEITA_EN_CFG_REG rc=%d\n", rc);
	}
//A-13:0x155A
	rc = smblib_masked_write(chg, TYPE_C_LEGACY_CABLE_CFG_REG, LEGACY_CABLE_DET_WINDOW_MASK, 0x2);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default TYPE_C_LEGACY_CABLE_CFG_REG rc=%d\n", rc);
	}
	rc = smblib_masked_write(chg, TYPE_C_LEGACY_CABLE_CFG_REG, EN_LEGACY_CABLE_DETECTION_BIT, 0x0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default TYPE_C_LEGACY_CABLE_CFG_REG rc=%d\n", rc);
	}
/*
//A-14:0x1380
	//WA for air charing issue, this is from power team's request
	rc = smblib_masked_write(chg, USBIN_AICL_OPTIONS_CFG_REG, SUSPEND_ON_COLLAPSE_USBIN_BIT, 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set USBIN_AICL_OPTIONS_CFG_REG rc=%d\n", rc);
	}
//A-15:0x1365
	//Set the USBIN_IN_COLLAPSE from 5ms to 30us.
	//Fix the issue. The VBUS still keeps HIGH after unplug charger
	rc = smblib_masked_write(chg, USBIN_LOAD_CFG_REG, USBIN_IN_COLLAPSE_GF_SEL_MASK, 0x3);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set USBIN_LOAD_CFG_REG rc=%d\n", rc);
	}
*/
//A-XX:0x16B4~0x16B7
	// For INOV SKIN TEMP +++
	rc = smblib_write(smbchg_dev, SKIN_SHDN_THRESHOLD_MSB, 0x0B);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default SKIN_SHDN_THRESHOLD_MSB rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, SKIN_SHDN_THRESHOLD_LSB, 0x0F);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default SKIN_SHDN_THRESHOLD_LSB rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, SKIN_RST_THRESHOLD_MSB, 0x0F);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default SKIN_RST_THRESHOLD_MSB rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, SKIN_RST_THRESHOLD_LSB, 0x15);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default SKIN_RST_THRESHOLD_LSB rc=%d\n", rc);
	}
	
	rc = smblib_write(smbchg_dev, SKIN_H_THRESHOLD_MSB_REG, 0x1C);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default SKIN_H_THRESHOLD_MSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, SKIN_H_THRESHOLD_LSB_REG, 0x22);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default SKIN_H_THRESHOLD_LSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, SKIN_L_THRESHOLD_MSB_REG, 0x1D);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default SKIN_L_THRESHOLD_MSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, SKIN_L_THRESHOLD_LSB_REG, 0xE6);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default SKIN_L_THRESHOLD_LSB_REG rc=%d\n", rc);
	}
//A-YY:0x1370
	// For probe initila ICL set to 500mA +++
	rc = smblib_write(smbchg_dev, USBIN_CURRENT_LIMIT_CFG_REG, 0x0A);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default USBIN_CURRENT_LIMIT_CFG_REG rc=%d\n", rc);
	}
//A-ZZ:0x1550
	//For dual port design(VBUS will have drop between USB port transition)
	rc = smblib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG, EXIT_SNK_BASED_ON_CC_BIT, EXIT_SNK_BASED_ON_CC_BIT);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default TYPE_C_EXIT_STATE_CFG_REG rc=%d\n", rc);
	}
//A-YY:0x1670
	rc = smblib_write(chg, MISC_THERMREG_SRC_CFG_REG, 0x00);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default MISC_THERMREG_SRC_CFG_REG rc=%d\n", rc);
	}
	//[+++]Change to here from asus_insertion_initial_settings() in smb5-lib.c
	//Otherwise, the recharge voltage of battery safety will be covered
	//No.4-1:0x107E
	rc = smblib_write(chg, CHGR_ADC_RECHARGE_THRESHOLD_MSB_REG, 0x55);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default CHGR_ADC_RECHARGE_THRESHOLD_MSB_REG rc=%d\n", rc);
	}
	//No.4-2:0x107F
	rc = smblib_write(chg, CHGR_ADC_RECHARGE_THRESHOLD_LSB_REG, 0x83);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default CHGR_ADC_RECHARGE_THRESHOLD_LSB_REG rc=%d\n", rc);
	}
	//[---]Change to here from asus_insertion_initial_settings() in smb5-lib.c
	
//2020-01-19 add: aicl threshold = 4.5V (0x1384=0x05), DCP(ASUS_200K, ASUS_750K & others) = 2A
	rc = smblib_write(chg, USBIN_CONT_AICL_THRESHOLD_REG, 0x05);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set default CHGR_ADC_RECHARGE_THRESHOLD_LSB_REG rc=%d\n", rc);
	}

//jeita initial settings
	rc = smblib_write(chg, JEITA_FVCOMP_CFG_HOT_REG, 0x1C);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set JEITA_FVCOMP_CFG_HOT_REG to 4.08V rc=%d\n", rc);
	}

	rc = smblib_write(chg, JEITA_CCCOMP_CFG_HOT_REG, 0x00);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set JEITA_CCCOMP_CFG_HOT_REG to 2750mA rc=%d\n", rc);
	}

	rc = smblib_write(chg, JEITA_CCCOMP_CFG_COLD_REG, 0x21);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set JEITA_CCCOMP_CFG_COLD_REG to 1100mA rc=%d\n", rc);
	}

	rc = smblib_write(chg, JEITA_FVCOMP_CFG_COLD_REG, 0x00);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't set JEITA_FVCOMP_CFG_COLD_REG to 4.36V rc=%d\n", rc);
	}
}

//[+++]Need to check the status of pogo thermal if the device is booting
//The interrupt could be sent before the qpnp_smb2 probe finish
void asus_hid_is_connected(void)
{
	int ret;
	int station_cap;

	CHG_DBG("start\n");
	schedule_delayed_work(&smbchg_dev->asus_thermal_accy_work, 0);
	ret = hid_to_get_battery_cap(&station_cap);
	if (ret < 0) {
		CHG_DBG_E("Failed to get station capacity\n");
	} else if (station_cap >= 3 && station_cap <= 100 && ASUS_POGO_ID == STATION) {
		asus_extcon_set_state_sync(g_fgDev->st_bat_cap_extcon, station_cap);
		asus_extcon_set_state_sync(g_fgDev->st_present_extcon, 1);
	}   
}
EXPORT_SYMBOL(asus_hid_is_connected);
//[---]Need to check the status of pogo thermal if the device is booting

static irqreturn_t accy_thermal_alert_interrupt(int irq, void *dev_id)
{
	struct timespec mtNow, delta_time;
	long long delta_msec;

	//Use the delta time rather than trigger_level to filter the pulse for station
	mtNow = current_kernel_time();
	delta_time = timespec_sub(mtNow, g_last_accy_therm_int);
	g_last_accy_therm_int = current_kernel_time();
	delta_msec = (delta_time.tv_sec * NSEC_PER_SEC + delta_time.tv_nsec)/NSEC_PER_MSEC;
	CHG_DBG("[ACCY] delta = %lld msec.\n", delta_msec);
	if (delta_msec < 100)
		return IRQ_HANDLED;

	CHG_DBG("ACCY Thermal Alert interrupt\n");
	schedule_delayed_work(&smbchg_dev->asus_thermal_accy_work, 0);

	return IRQ_HANDLED;
}

void asus_probe_gpio_setting(struct platform_device *pdev, struct gpio_control *gpio_ctrl)
{
	struct pinctrl *chg_pc;
	struct pinctrl_state *chg_pcs;
	int accy_therm_alert_irq = 0;
	int rc = 0;

	chg_pc = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(chg_pc)) {
		CHG_DBG_E("failed to get charger pinctrl\n");
	}
	chg_pcs = pinctrl_lookup_state(chg_pc, "chg_gpio_default");
	if (IS_ERR_OR_NULL(chg_pcs)) {
		CHG_DBG_E("failed to get charger input pinctrl state from dtsi\n");
	}
	rc = pinctrl_select_state(chg_pc, chg_pcs);
	if (rc < 0) {
		CHG_DBG_E("failed to set charger input pinctrl state\n");
	}

	gpio_ctrl->USB2_MUX1_EN= of_get_named_gpio(pdev->dev.of_node, "USB2_MUX1_EN", 0);
	rc = gpio_request(gpio_ctrl->USB2_MUX1_EN, "USB2_MUX1_EN");
	if (rc)
		CHG_DBG_E("failed to request USB2_MUX1_EN\n");
	else
		CHG_DBG("Success to request USB2_MUX1_EN\n");

	gpio_ctrl->PMI_MUX_EN = of_get_named_gpio(pdev->dev.of_node, "PMI_MUX_EN", 0);
	rc = gpio_request(gpio_ctrl->PMI_MUX_EN, "PMI_MUX_EN");
	if (rc)
		CHG_DBG_E("failed to request PMI_MUX_EN\n");
	else
		CHG_DBG("Success to request PMI_MUX_EN\n");

	gpio_ctrl->POGO_OTG_EN = of_get_named_gpio(pdev->dev.of_node, "POGO_OTG_EN", 0);
	rc = gpio_request(gpio_ctrl->POGO_OTG_EN, "POGO_OTG_EN");
	if (rc)
		CHG_DBG_E("failed to request POGO_OTG_EN\n");
	else
		CHG_DBG("Success to request POGO_OTG_EN\n");

	gpio_ctrl->BTM_OTG_EN = of_get_named_gpio(pdev->dev.of_node, "BTM_OTG_EN", 0);
	rc = gpio_request(gpio_ctrl->BTM_OTG_EN, "BTM_OTG_EN");
	if (rc)
		CHG_DBG_E("failed to request BTM_OTG_EN\n");
	else
		CHG_DBG("Success to request BTM_OTG_EN\n");

	gpio_ctrl->POGO_OVP_ACOK = of_get_named_gpio(pdev->dev.of_node, "POGO_OVP_ACOK", 0);
	rc = gpio_request(gpio_ctrl->POGO_OVP_ACOK, "POGO_OVP_ACOK");
	if (rc)
		CHG_DBG_E("failed to request POGO_OVP_ACOK\n");
	else
		CHG_DBG("Success to request POGO_OVP_ACOK\n");

	gpio_ctrl->BTM_OVP_ACOK = of_get_named_gpio(pdev->dev.of_node, "BTM_OVP_ACOK", 0);
	rc = gpio_request(gpio_ctrl->BTM_OVP_ACOK, "BTM_OVP_ACOK");
	if (rc)
		CHG_DBG_E("failed to request BTM_OVP_ACOK\n");
	else
		CHG_DBG("Success to request BTM_OVP_ACOK\n");

	gpio_ctrl->PCA9468_EN = of_get_named_gpio(pdev->dev.of_node, "PCA9468_EN", 0);
	rc = gpio_request(gpio_ctrl->PCA9468_EN, "PCA9468_EN");
	if (rc)
		CHG_DBG_E("failed to request PCA9468_EN\n");
	else
		CHG_DBG("Success to request PCA9468_EN\n");

	gpio_ctrl->POGO_TEMP_INT = of_get_named_gpio(pdev->dev.of_node, "POGO_TEMP_INT", 0);
	rc = gpio_request(gpio_ctrl->POGO_TEMP_INT, "POGO_TEMP_INT");
	if (rc)
		CHG_DBG_E("failed to request POGO_TEMP_INT\n");
	else
		CHG_DBG("Success to request POGO_TEMP_INT\n");

	if (g_ASUS_hwID > HW_REV_EVB) {
		gpio_ctrl->ADC_SW_EN = of_get_named_gpio(pdev->dev.of_node, "ADC_SW_EN", 0);
		rc = gpio_request(gpio_ctrl->ADC_SW_EN, "ADC_SW_EN");
		if (rc)
			CHG_DBG_E("failed to request ADC_SW_EN\n");
		else
			CHG_DBG("Success to request ADC_SW_EN\n");

		gpio_ctrl->ADC_VH_EN_5 = of_get_named_gpio(pdev->dev.of_node, "ADC_VH_EN_5", 0);
		rc = gpio_request(gpio_ctrl->ADC_VH_EN_5, "ADC_VH_EN_5");
		if (rc)
			CHG_DBG_E("failed to request ADC_VH_EN_5\n");
		else
			CHG_DBG("Success to request ADC_VH_EN_5\n");
	}

	//ACCY thermal alert adc interrupt
	accy_therm_alert_irq = gpio_to_irq(gpio_ctrl->POGO_TEMP_INT);
	if (accy_therm_alert_irq < 0) {
		CHG_DBG_E("POGO_TEMP_INT to irq ERROR(%d).\n", accy_therm_alert_irq);
	}
	rc = request_threaded_irq(accy_therm_alert_irq, NULL, accy_thermal_alert_interrupt,
		IRQF_TRIGGER_FALLING  | IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND, "accy_therm_alert", NULL);
	if (rc < 0)
		CHG_DBG_E("Failed to request accy_thermal_alert_interrupt\n");
}

// POGO/BTM Thermal Alert Function +++

int read_side_conn_temp(int *conn_temp)
{
	int rc;
	
	if (smbchg_dev->iio.side_connector_temp_chan) {
		rc = iio_read_channel_processed(smbchg_dev->iio.side_connector_temp_chan,
				conn_temp);
		if (rc < 0) {
			CHG_DBG_E("Error in reading side_connector_temp channel, rc=%d\n", rc);

			//retry if read fail
			rc = iio_read_channel_processed(smbchg_dev->iio.side_connector_temp_chan,
				conn_temp);
			if (rc < 0) {
				CHG_DBG_E("Error in reading side_connector_temp channel retry, rc=%d\n", rc);
				return rc;
			}
		}
		*conn_temp = *conn_temp / 1000;
		CHG_DBG("conn_temp = %dmV\n", *conn_temp);
	}
	
	return rc;
}

int read_btm_conn_temp(int *conn_temp)
{
	int rc;
	
	if (smbchg_dev->iio.btm_connector_temp_chan) {
		rc = iio_read_channel_processed(smbchg_dev->iio.btm_connector_temp_chan,
				conn_temp);
		if (rc < 0) {
			CHG_DBG_E("Error in reading btm_connector_temp channel, rc=%d\n", rc);

			//retry if read fail
			rc = iio_read_channel_processed(smbchg_dev->iio.btm_connector_temp_chan,
				conn_temp);
			if (rc < 0) {
				CHG_DBG_E("Error in reading btm_connector_temp channel retry, rc=%d\n", rc);
				return rc;
			}
		}
		*conn_temp = *conn_temp / 1000;
		CHG_DBG("conn_temp = %dmV\n", *conn_temp);
	}
	
	return rc;
}

void asus_thermal_pogo_work(struct work_struct *work)
{
	int rc;
	int vbus_status, otg_status;
	int conn_temp;

	rc = read_side_conn_temp(&conn_temp);
	if (rc < 0)
		return;

	CHG_DBG("conn_temp %d\n", conn_temp);
	
	vbus_status = !gpio_get_value(global_gpio->POGO_OVP_ACOK);
	otg_status = gpio_get_value(global_gpio->POGO_OTG_EN);
		
	if (conn_temp < 440) {
		if (vbus_status) {
			smblib_set_usb_suspend(smbchg_dev, true);
			pmic_set_pca9468_charging(false);
			rc = smblib_write(smbchg_dev, DCDC_CMD_OTG_REG, 0);
			if (rc < 0)
				CHG_DBG_E("Failed to disable pmi_otg_en\n");
			rc = gpio_direction_output(global_gpio->POGO_OTG_EN, 0);
			if (rc)
				CHG_DBG_E("Failed to disable gpio BTM_OTG_EN\n");
			
			if (!usb_alert_side_flag)
				g_temp_side_state = 2;
		} else if (otg_status) {
			if (!usb_alert_side_flag)
				g_temp_side_state = 2;
				
			msleep(100);
			rc = smblib_write(smbchg_dev, DCDC_CMD_OTG_REG, 0);
			if (rc < 0)
				CHG_DBG_E("Failed to disable pmi_otg_en\n");
			rc = gpio_direction_output(global_gpio->POGO_OTG_EN, 0);
			if (rc)
				CHG_DBG_E("Failed to disable gpio POGO_OTG_EN\n");
		} else {
			if (!usb_alert_side_flag)
				g_temp_side_state = 1;
		}

		if (!usb_alert_side_flag) {
			if (g_temp_side_state == 2 || g_temp_btm_state == 2 || g_temp_station_state == 2 || g_temp_INBOX_DT_state == 2)
				g_temp_state = 2;
			else if (g_temp_side_state == 1 || g_temp_btm_state == 1 || g_temp_station_state == 1 || g_temp_INBOX_DT_state == 1)
				g_temp_state = 1;
			else
				g_temp_state = 0;
		
			asus_extcon_set_state_sync(smbchg_dev->thermal_extcon, g_temp_state);
			CHG_DBG("side thermal high, report_state = %d\n", g_temp_state);
			
			usb_alert_side_flag = 1;
			g_once_usb_thermal_side = 1; //for factory test
			if (g_Charger_mode)
				cos_alert_once_flag = 1;
		}
	} else if (conn_temp > 578){
		if (!vbus_status && !otg_status) {
			usb_alert_side_flag = 0;
			g_temp_side_state = 0;
			
			if (g_temp_side_state == 2 || g_temp_btm_state == 2 || g_temp_station_state == 2 || g_temp_INBOX_DT_state == 2)
				g_temp_state = 2;
			else if (g_temp_side_state == 1 || g_temp_btm_state == 1 || g_temp_station_state == 1 || g_temp_INBOX_DT_state == 1)
				g_temp_state = 1;
			else
				g_temp_state = 0;
			
			asus_extcon_set_state_sync(smbchg_dev->thermal_extcon, g_temp_state);
			CHG_DBG("side thermal low, report_state = %d\n", g_temp_state);
		}
	}
	
	schedule_delayed_work(&smbchg_dev->asus_thermal_pogo_work, msecs_to_jiffies(60000));
}

void asus_thermal_btm_work(struct work_struct *work)
{
	int rc;
	int vbus_status, otg_status;
	int conn_temp;

	rc = read_btm_conn_temp(&conn_temp);
	if (rc < 0)
		return;

	CHG_DBG("conn_temp %d\n", conn_temp);
	
	vbus_status = !gpio_get_value(global_gpio->BTM_OVP_ACOK);
	otg_status = gpio_get_value(global_gpio->BTM_OTG_EN);

	if (conn_temp < 440) {		
		if (vbus_status) {
			smblib_set_usb_suspend(smbchg_dev, true);
			pmic_set_pca9468_charging(false);
			rc = smblib_write(smbchg_dev, DCDC_CMD_OTG_REG, 0);
			if (rc < 0)
				CHG_DBG_E("Failed to disable pmi_otg_en\n");
			rc = gpio_direction_output(global_gpio->BTM_OTG_EN, 0);
			if (rc)
				CHG_DBG_E("Failed to disable gpio BTM_OTG_EN\n");
			
			if (!usb_alert_btm_flag)
				g_temp_btm_state = 2;
		} else if (otg_status) {
			if (!usb_alert_btm_flag)
				g_temp_btm_state = 2;
			
			msleep(100);
			rc = smblib_write(smbchg_dev, DCDC_CMD_OTG_REG, 0);
			if (rc < 0)
				CHG_DBG_E("Failed to disable pmi_otg_en\n");
			rc = gpio_direction_output(global_gpio->BTM_OTG_EN, 0);
			if (rc)
				CHG_DBG_E("Failed to disable gpio BTM_OTG_EN\n");
		} else {
			if (!usb_alert_btm_flag)
				g_temp_btm_state = 1;
		}
		
		if (!usb_alert_btm_flag) {
			if (g_temp_side_state == 2 || g_temp_btm_state == 2 || g_temp_station_state == 2 || g_temp_INBOX_DT_state == 2)
				g_temp_state = 2;
			else if (g_temp_side_state == 1 || g_temp_btm_state == 1 || g_temp_station_state == 1 || g_temp_INBOX_DT_state == 1)
				g_temp_state = 1;
			else
				g_temp_state = 0;

			asus_extcon_set_state_sync(smbchg_dev->thermal_extcon, g_temp_state);
			CHG_DBG("btm thermal high, report_state = %d\n", g_temp_state);
			
			usb_alert_btm_flag = 1;
			g_once_usb_thermal_btm = 1; //for factory test
			if (g_Charger_mode)
				cos_alert_once_flag = 1;
		}
	} else if (conn_temp > 578){
		if (!vbus_status && !otg_status) {
			usb_alert_btm_flag = 0;
			g_temp_btm_state = 0;
		
			if (g_temp_side_state == 2 || g_temp_btm_state == 2 || g_temp_station_state == 2 || g_temp_INBOX_DT_state == 2)
				g_temp_state = 2;
			else if (g_temp_side_state == 1 || g_temp_btm_state == 1 || g_temp_station_state == 1 || g_temp_INBOX_DT_state == 1)
				g_temp_state = 1;
			else
				g_temp_state = 0;
			
			asus_extcon_set_state_sync(smbchg_dev->thermal_extcon, g_temp_state);
			CHG_DBG("btm thermal low, report_state = %d\n", g_temp_state);
		}
	}
	
	schedule_delayed_work(&smbchg_dev->asus_thermal_btm_work, msecs_to_jiffies(60000));
}
// POGO/BTM Thermal Alert Function ---

//[+++] Add for station update when screen on

static int smb5_drm_notifier (struct notifier_block *nb,
					unsigned long val, void *data){

	struct drm_panel_notifier *evdata = data;
	int blank;

	if (!evdata)
		return 0;

	if (val != DRM_PANEL_EVENT_BLANK)
		return 0;

	if (evdata->data && val == DRM_PANEL_EVENT_BLANK) {
		blank = *(int *) (evdata->data);
		switch (blank) {
			case DRM_PANEL_BLANK_POWERDOWN:
				// panel is power down notify
				break;

			case DRM_PANEL_BLANK_UNBLANK:
				// panel is power on notify
				if (ASUS_POGO_ID == STATION)
					fg_station_attach_notifier(true);
				break;
			default:
				break;
		}
	}
	return NOTIFY_OK;
}

//[+++] Add for station update when screen on

// ASUS BSP Austin_T : Add attributes +++
static ssize_t boot_completed_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		boot_completed_flag = false;
		CHG_DBG("boot_completed_flag = 0\n");
	} else if (tmp == 1) {
		boot_completed_flag = true;
		CHG_DBG("boot_completed_flag = 1\n");
		schedule_delayed_work(&smbchg_dev->asus_thermal_btm_work, 0);
		schedule_delayed_work(&smbchg_dev->asus_thermal_pogo_work, msecs_to_jiffies(100));
		schedule_delayed_work(&g_fgDev->asus_battery_version_work, 0);

		if (ASUS_POGO_ID == STATION)
			fg_station_attach_notifier(true);
	}

	return len;
}

static ssize_t boot_completed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", boot_completed_flag);
}

static ssize_t charger_limit_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;
	int rc;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		charger_limit_enable_flag = 0;
		rc = smblib_masked_write(smbchg_dev, CHARGING_ENABLE_CMD_REG, CHARGING_ENABLE_CMD_BIT, CHARGING_ENABLE_CMD_BIT);
		if (PE_check_asus_vid() || rt_chg_check_asus_vid())
			pmic_set_pca9468_charging(true);
	} else if (tmp == 1) {
		charger_limit_enable_flag = 1;
		if (asus_get_prop_batt_capacity(smbchg_dev) >= charger_limit_value) {
			rc = smblib_masked_write(smbchg_dev, CHARGING_ENABLE_CMD_REG, CHARGING_ENABLE_CMD_BIT, 0);
			if (PE_check_asus_vid() || rt_chg_check_asus_vid())
				pmic_set_pca9468_charging(false);
		} else {
			rc = smblib_masked_write(smbchg_dev, CHARGING_ENABLE_CMD_REG, CHARGING_ENABLE_CMD_BIT, CHARGING_ENABLE_CMD_BIT);
			if (PE_check_asus_vid() || rt_chg_check_asus_vid())
				pmic_set_pca9468_charging(true);
		}
	}
	CHG_DBG("charger_limit_enable = %d", charger_limit_enable_flag);

	return len;
}

static ssize_t charger_limit_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", charger_limit_enable_flag);
}

static ssize_t charger_limit_percent_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp;

	tmp = simple_strtol(buf, NULL, 10);
	charger_limit_value = tmp;
	CHG_DBG("charger_limit_percent set to = %d", charger_limit_value);

	return len;
}

static ssize_t charger_limit_percent_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", charger_limit_value);
}

static ssize_t vbus_side_btm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool vbus_bottom;
	bool vbus_side;

	vbus_side = !gpio_get_value_cansleep(global_gpio->POGO_OVP_ACOK);
	vbus_bottom = !gpio_get_value_cansleep(global_gpio->BTM_OVP_ACOK);

	if (vbus_side)
		return sprintf(buf, "2\n");
	else if (vbus_bottom)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t CHG_TYPE_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (asus_adapter_detecting_flag) {
		return sprintf(buf, "OTHERS\n");
	} else {
		if (asus_CHG_TYPE == 750)
			return sprintf(buf, "DCP_ASUS_750K_2A\n");
		else if (asus_CHG_TYPE == 200)
			return sprintf(buf, "HVDCP_ASUS_200K_2A\n");
		else
			return sprintf(buf, "OTHERS\n");
	}
}

static ssize_t asus_usb_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;
	int rc;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		CHG_DBG("Set EnableCharging\n");
		asus_suspend_cmd_flag = 0;
		rc = smblib_set_usb_suspend(smbchg_dev, 0);
		if (PE_check_asus_vid() || rt_chg_check_asus_vid())
			pmic_set_pca9468_charging(true);
	} else if (tmp == 1) {
		CHG_DBG("Set DisableCharging\n");
		asus_suspend_cmd_flag = 1;
		rc = smblib_set_usb_suspend(smbchg_dev, 1);
		if (PE_check_asus_vid() || rt_chg_check_asus_vid())
			pmic_set_pca9468_charging(false);
	}

	power_supply_changed(smbchg_dev->batt_psy);

	return len;
}

static ssize_t asus_usb_suspend_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 reg;
	int ret;
	int suspend;

	ret = smblib_read(smbchg_dev, USBIN_CMD_IL_REG, &reg);
	suspend = reg & USBIN_SUSPEND_BIT;

	return sprintf(buf, "%d\n", suspend);
}

static ssize_t TypeC_Side_Detect2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int typec_side_detect, open, cc_pin;
	u8 reg;
	int ret = -1;

	ret = smblib_read(smbchg_dev, TYPE_C_MISC_STATUS_REG, &reg);
	open = reg & CC_ATTACHED_BIT;

	ret = smblib_read(smbchg_dev, TYPE_C_MISC_STATUS_REG, &reg);
	cc_pin = reg & CC_ORIENTATION_BIT;

	if (open == 0)
		typec_side_detect = 0;
	else if (cc_pin == 0)
		typec_side_detect = 1;
	else
		typec_side_detect = 2;

	return sprintf(buf, "%d\n", typec_side_detect);
}

static ssize_t thermal_gpio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status = 0;

	status = gpio_get_value(global_gpio->POGO_TEMP_INT);
	return sprintf(buf, "%d\n", status);
}

static ssize_t INOV_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;
	int rc;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		g_force_disable_inov = true;
		rc = smblib_write(smbchg_dev, MISC_THERMREG_SRC_CFG_REG, 0x00);
		if (rc < 0)
			dev_err(smbchg_dev->dev, "Couldn't set MISC_THERMREG_SRC_CFG_REG 0x0\n");
		CHG_DBG("Disable INOV function\n");
	} else if (tmp == 1) {
		g_force_disable_inov = false;
		rc = smblib_write(smbchg_dev, MISC_THERMREG_SRC_CFG_REG, 0x05);
		if (rc < 0)
			dev_err(smbchg_dev->dev, "Couldn't set MISC_THERMREG_SRC_CFG_REG 0x5\n");
		CHG_DBG("Enable INOV function\n");
    }

	return len;
}

static ssize_t INOV_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc;
	u8 reg;

    rc = smblib_read(smbchg_dev, MISC_THERMREG_SRC_CFG_REG, &reg);

	return sprintf(buf, "INOV_reg 0x1670 = 0x%x\n", reg);
}

static ssize_t disable_input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;
	int rc;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		CHG_DBG("Thermal Test over, can suspend input\n");
		no_input_suspend_flag = 0;
		rc = smblib_write(smbchg_dev, JEITA_EN_CFG_REG, 0x10);		
		if (rc < 0)
			CHG_DBG_E("Failed to set JEITA_EN_CFG_REG\n");
	} else if (tmp == 1) {
		CHG_DBG("Thermal Test, can not suspend input\n");
		no_input_suspend_flag = 1;
		rc = smblib_write(smbchg_dev, JEITA_EN_CFG_REG, 0x00);
		if (rc < 0)
			CHG_DBG_E("Failed to set JEITA_EN_CFG_REG\n");
	}

	rc = smblib_set_usb_suspend(smbchg_dev, 0);

	return len;
}

static ssize_t disable_input_suspend_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", no_input_suspend_flag);
}

static ssize_t usb_thermal_btm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int conn_temp;
	int rc;

	rc = read_btm_conn_temp(&conn_temp);
	if (rc < 0) {
		CHG_DBG("read_btm_conn_temp fail\n");
		return sprintf(buf, "FAIL\n");
	}
	
	if (conn_temp < 440 || conn_temp > 1550)
		return sprintf(buf, "FAIL\n");
	else
		return sprintf(buf, "PASS\n");
}

static ssize_t usb_thermal_side_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int conn_temp;
	int rc;

	rc = read_side_conn_temp(&conn_temp);
	if (rc < 0) {
		CHG_DBG("read_side_conn_temp fail\n");
		return sprintf(buf, "FAIL\n");
	}
	
	if (conn_temp < 100 || conn_temp > 1700)
		return sprintf(buf, "FAIL\n");
	else
		return sprintf(buf, "PASS\n");
}

static ssize_t usb_thermal_side_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int conn_temp;
	int rc;

	rc = read_side_conn_temp(&conn_temp);
	if (rc < 0) {
		CHG_DBG("read_side_conn_temp fail\n");
		return sprintf(buf, "FAIL\n");
	}

	return sprintf(buf, "%d\n", conn_temp);
}

static ssize_t once_usb_thermal_btm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (g_once_usb_thermal_btm)
		return sprintf(buf, "FAIL\n");
	else
		return sprintf(buf, "PASS\n");
}

static ssize_t once_usb_thermal_side_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (g_once_usb_thermal_side)
		return sprintf(buf, "FAIL\n");
	else
		return sprintf(buf, "PASS\n");
}

static ssize_t get_usb_mux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool usb2_mux1_stats, pmi_mux_stats;
	bool btm_otg_stats, pogo_otg_stats;
	usb2_mux1_stats = gpio_get_value_cansleep(global_gpio->USB2_MUX1_EN);
	pmi_mux_stats = gpio_get_value_cansleep(global_gpio->PMI_MUX_EN);

	btm_otg_stats = gpio_get_value_cansleep(global_gpio->BTM_OTG_EN);
	pogo_otg_stats = gpio_get_value_cansleep(global_gpio->POGO_OTG_EN);

	return sprintf(buf, "usb2_mux1_stats : %d\npmi_mux_stats : %d\nbtm_otg_stats : %d\npogo_otg_stats : %d\n",
					usb2_mux1_stats, pmi_mux_stats, btm_otg_stats, pogo_otg_stats);
}
//[+++]Open an interface to change the specified usb_mux value
static ssize_t force_usb_mux_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp, rc;
	int usb2_mux1_en = 0, pmi_mux_en = 0;

	tmp = simple_strtol(buf, NULL, 10);
	CHG_DBG("Set Force_USB_MUX : %d\n", tmp);
	g_force_usb_mux = tmp;
	if (g_force_usb_mux == 2) {
		usb2_mux1_en = 0;
		pmi_mux_en = 1;
		CHG_DBG("g_force_usb_mux is 2, force to use the BTM debug\n");
		rc = gpio_direction_output(global_gpio->USB2_MUX1_EN, usb2_mux1_en);
		if (rc)
			CHG_DBG_E("failed to control USB2_MUX1_EN\n");

		rc = gpio_direction_output(global_gpio->PMI_MUX_EN, pmi_mux_en);
		if (rc)
			CHG_DBG_E("failed to control PMI_MUX_EN\n");
	}
	else if (g_force_usb_mux == 1) {
		usb2_mux1_en = 1;
		pmi_mux_en = 0;
		CHG_DBG("g_force_usb_mux is 0\n");
		rc = gpio_direction_output(global_gpio->USB2_MUX1_EN, usb2_mux1_en);
		if (rc)
			CHG_DBG_E("failed to control USB2_MUX1_EN\n");

		rc = gpio_direction_output(global_gpio->PMI_MUX_EN, pmi_mux_en);
		if (rc)
			CHG_DBG_E("failed to control PMI_MUX_EN\n");
	}
	else if (g_force_usb_mux == 0) {
		usb2_mux1_en = 0;
		pmi_mux_en = 0;
		CHG_DBG("g_force_usb_mux is 0\n");
		rc = gpio_direction_output(global_gpio->USB2_MUX1_EN, usb2_mux1_en);
		if (rc)
			CHG_DBG_E("failed to control USB2_MUX1_EN\n");

		rc = gpio_direction_output(global_gpio->PMI_MUX_EN, pmi_mux_en);
		if (rc)
			CHG_DBG_E("failed to control PMI_MUX_EN\n");
	}

	return count;
}

static ssize_t force_usb_mux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	CHG_DBG("usb2_mux1_en : %d\n", gpio_get_value_cansleep(global_gpio->USB2_MUX1_EN));

	CHG_DBG("pmi_mux_en : %d\n", gpio_get_value_cansleep(global_gpio->PMI_MUX_EN));

	return sprintf(buf, "g_force_usb_mux : %d\n", g_force_usb_mux);
}
//[---]Open an interface to change the specified usb_mux value

static ssize_t get_gpio_highlow_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool pogo_ovp_stats, btm_ovp_stats;

	pogo_ovp_stats = gpio_get_value_cansleep(global_gpio->POGO_OVP_ACOK);
	btm_ovp_stats = gpio_get_value_cansleep(global_gpio->BTM_OVP_ACOK);

	return sprintf(buf, "pogo_ovp_stats : %d\nbtm_ovp_stats : %d\n", pogo_ovp_stats, btm_ovp_stats);
}

static ssize_t fake_er_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "fake_er_stage_flag : %d\n", fake_er_stage_flag);
}

static ssize_t fake_er_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		fake_er_stage_flag = false;
		CHG_DBG("set fake_er_stage_flag = 0\n");
	} else if (tmp == 1) {
		fake_er_stage_flag = true;
		CHG_DBG("set fake_er_stage_flag = 1\n");
	}

	return len;
}

static ssize_t ultra_bat_life_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;
	int rc;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		ultra_bat_life_flag = false;
		CHG_DBG("ultra_bat_life_flag = 0, cancel CHG_Limit\n");
	} else if (tmp == 1) {
		ultra_bat_life_flag = true;
		write_CHGLimit_value(0);
		CHG_DBG("ultra_bat_life_flag = 1, CHG_Limit = 60\n");
		if (asus_get_prop_batt_capacity(smbchg_dev) > 60 && asus_get_prop_usb_present(smbchg_dev)) {
			rc = smblib_set_usb_suspend(smbchg_dev, true);
			CHG_DBG("Capcity > 60, input suspend\n");
		}
	}

	return len;
}

static ssize_t ultra_bat_life_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ultra_bat_life_flag);
}

static ssize_t ultra_cos_spec_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp;

	tmp = simple_strtol(buf, NULL, 10);
	g_ultra_cos_spec_time = tmp;
	CHG_DBG("ultra_cos_spec_time set to = %d\n", g_ultra_cos_spec_time);

	return len;
}

static ssize_t ultra_cos_spec_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_ultra_cos_spec_time);
}

extern bool side_pps_flag;
extern bool btm_pps_flag;
extern void pca9468_smartchg_stop_charging(bool enable);
static ssize_t smartchg_stop_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;
	int rc;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		CHG_DBG("Smart charge enable charging\n");
		rc = smblib_masked_write(smbchg_dev, CHARGING_ENABLE_CMD_REG, CHARGING_ENABLE_CMD_BIT, CHARGING_ENABLE_CMD_BIT);
		if (rc < 0) {
			printk("[BAT][CHG] Couldn't write charging_enable rc = %d\n", rc);
			return rc;
		}
	} else if (tmp == 1) {
		CHG_DBG("Smart charge stop charging\n");
		smartchg_stop_flag = 1;
		rc = smblib_masked_write(smbchg_dev, CHARGING_ENABLE_CMD_REG, CHARGING_ENABLE_CMD_BIT, 0);
		if (rc < 0) {
			printk("[BAT][CHG] Couldn't write charging_enable rc = %d\n", rc);
			return rc;
		}
	}

	if (side_pps_flag || btm_pps_flag)
		pca9468_smartchg_stop_charging(tmp);

	/* Do suspend/unsuspend when resuming charging to work around possible fast charge issue. */
	if (tmp == 0) {
		CHG_DBG("Smart charge resume charging, set DisableCharging\n");
		asus_suspend_cmd_flag = 1;
		rc = smblib_set_usb_suspend(smbchg_dev, 1);
		if (PE_check_asus_vid() || rt_chg_check_asus_vid())
			pmic_set_pca9468_charging(false);

		CHG_DBG("Smart charge resume charging, set EnableCharging\n");
		asus_suspend_cmd_flag = 0;
		rc = smblib_set_usb_suspend(smbchg_dev, 0);
		if (PE_check_asus_vid() || rt_chg_check_asus_vid())
			pmic_set_pca9468_charging(true);

		smartchg_stop_flag = 0;

		power_supply_changed(smbchg_dev->batt_psy);
	}

	return len;
}

extern void pca9468_smartchg_slow_charging(bool enable);
static ssize_t smartchg_stop_charging_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", smartchg_stop_flag);
}

static ssize_t smartchg_slow_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		CHG_DBG("disable slow charging\n");
		smartchg_slow_flag = 0;
		jeita_rule();
	} else if (tmp == 1) {
		CHG_DBG("enable slow charging\n");
		smartchg_slow_flag = 1;
		jeita_rule();
	}

	pca9468_smartchg_slow_charging(tmp);
	return len;
}

static ssize_t smartchg_slow_charging_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", smartchg_slow_flag);
}

static ssize_t demo_app_property_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		demo_app_property_flag = false;
		CHG_DBG("demo_app_property_flag = 0\n");
	} else if (tmp == 1) {
		demo_app_property_flag = true;
		demo_recharge_delta = 2;
		CHG_DBG("demo_app_property_flag = 1\n");
    }

	return len;
}

static ssize_t demo_app_property_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", demo_app_property_flag);
}

static ssize_t cn_demo_app_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		cn_demo_app_flag = false;
		CHG_DBG("cn_demo_app_flag = 0\n");
	} else if (tmp == 1) {
		cn_demo_app_flag = true;
		demo_recharge_delta = 2;
		CHG_DBG("cn_demo_app_flag = 1\n");
    }

	return len;
}

static ssize_t cn_demo_app_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cn_demo_app_flag);
}

static ssize_t force_station_ultra_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		force_station_ultra_flag = false;
		CHG_DBG("force_station_ultra_flag = 0\n");
	} else if (tmp == 1) {
		force_station_ultra_flag = true;
		CHG_DBG("force_station_ultra_flag = 1\n");
    }

	return len;
}

static ssize_t force_station_ultra_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", force_station_ultra_flag);
}

//[+++] Add for the balance of charging and heat
static ssize_t game_type_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int tmp = 0;

	tmp = buf[0] - 48;

	if (tmp == 0) {
		game_type_flag = false;
		CHG_DBG("%s: game_type_flag = 0\n", __func__);
	} else if (tmp == 1) {
		game_type_flag = true;
		CHG_DBG("%s: game_type_flag = 1\n", __func__);
    }

	return len;
}

static ssize_t game_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", game_type_flag);
}
//[---] Add for the balance of charging and heat

static ssize_t adapter_id_vadc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_adapter_id_vadc);
}


static DEVICE_ATTR(boot_completed, 0664, boot_completed_show, boot_completed_store);
static DEVICE_ATTR(charger_limit_enable, 0664, charger_limit_enable_show, charger_limit_enable_store);
static DEVICE_ATTR(charger_limit_percent, 0664, charger_limit_percent_show, charger_limit_percent_store);
static DEVICE_ATTR(vbus_side_btm, 0664, vbus_side_btm_show, NULL);
static DEVICE_ATTR(CHG_TYPE, 0664, CHG_TYPE_show, NULL);
static DEVICE_ATTR(asus_usb_suspend, 0664, asus_usb_suspend_show, asus_usb_suspend_store);
static DEVICE_ATTR(TypeC_Side_Detect2, 0664, TypeC_Side_Detect2_show, NULL);
static DEVICE_ATTR(thermal_gpio, 0664, thermal_gpio_show, NULL);
static DEVICE_ATTR(INOV_enable, 0664, INOV_enable_show, INOV_enable_store);
static DEVICE_ATTR(disable_input_suspend, 0664, disable_input_suspend_show, disable_input_suspend_store);
static DEVICE_ATTR(usb_thermal_btm, 0664, usb_thermal_btm_show, NULL);
static DEVICE_ATTR(usb_thermal_side, 0664, usb_thermal_side_show, NULL);
static DEVICE_ATTR(usb_thermal_side_adc, 0664, usb_thermal_side_adc_show, NULL);
static DEVICE_ATTR(once_usb_thermal_btm, 0664, once_usb_thermal_btm_show, NULL);
static DEVICE_ATTR(once_usb_thermal_side, 0664, once_usb_thermal_side_show, NULL);
static DEVICE_ATTR(get_usb_mux, 0664, get_usb_mux_show, NULL);
static DEVICE_ATTR(force_usb_mux, 0664, force_usb_mux_show, force_usb_mux_store);
static DEVICE_ATTR(get_gpio_highlow, 0664, get_gpio_highlow_show, NULL);
static DEVICE_ATTR(fake_er, 0664, fake_er_show, fake_er_store);
static DEVICE_ATTR(ultra_bat_life, 0664, ultra_bat_life_show, ultra_bat_life_store);
static DEVICE_ATTR(ultra_cos_spec_time, 0664, ultra_cos_spec_time_show, ultra_cos_spec_time_store);
static DEVICE_ATTR(smartchg_stop_charging, 0664, smartchg_stop_charging_show, smartchg_stop_charging_store);
static DEVICE_ATTR(smartchg_slow_charging, 0664, smartchg_slow_charging_show, smartchg_slow_charging_store);
static DEVICE_ATTR(demo_app_property, 0664, demo_app_property_show, demo_app_property_store);
static DEVICE_ATTR(cn_demo_app, 0664, cn_demo_app_show, cn_demo_app_store);
static DEVICE_ATTR(force_station_ultra, 0664, force_station_ultra_show, force_station_ultra_store);
static DEVICE_ATTR(game_type, 0664, game_type_show, game_type_store);
static DEVICE_ATTR(adapter_id_vadc, 0664, adapter_id_vadc_show, NULL);

static struct attribute *asus_smblib_attrs[] = {
	&dev_attr_boot_completed.attr,
	&dev_attr_charger_limit_enable.attr,
	&dev_attr_charger_limit_percent.attr,
	&dev_attr_vbus_side_btm.attr,
	&dev_attr_CHG_TYPE.attr,
	&dev_attr_asus_usb_suspend.attr,
	&dev_attr_TypeC_Side_Detect2.attr,
	&dev_attr_thermal_gpio.attr,
	&dev_attr_INOV_enable.attr,
	&dev_attr_disable_input_suspend.attr,
	&dev_attr_usb_thermal_btm.attr,
	&dev_attr_usb_thermal_side.attr,
	&dev_attr_usb_thermal_side_adc.attr,
	&dev_attr_once_usb_thermal_btm.attr,
	&dev_attr_once_usb_thermal_side.attr,
	&dev_attr_get_usb_mux.attr,
	&dev_attr_force_usb_mux.attr,
	&dev_attr_get_gpio_highlow.attr,
	&dev_attr_fake_er.attr,
	&dev_attr_ultra_bat_life.attr,
	&dev_attr_ultra_cos_spec_time.attr,
	&dev_attr_smartchg_stop_charging.attr,
	&dev_attr_smartchg_slow_charging.attr,
	&dev_attr_demo_app_property.attr,
	&dev_attr_cn_demo_app.attr,
	&dev_attr_force_station_ultra.attr,
	&dev_attr_game_type.attr,
	&dev_attr_adapter_id_vadc.attr,
	NULL
};

static const struct attribute_group asus_smblib_attr_group = {
	.attrs = asus_smblib_attrs,
};
// ASUS BSP Austin_T : Add attributes ---

// ASUS BSP charger : BMMI Adb Interface +++
#define chargerIC_status_PROC_FILE	"driver/chargerIC_status"
static struct proc_dir_entry *chargerIC_status_proc_file;
static int chargerIC_status_proc_read(struct seq_file *buf, void *v)
{
	int ret = -1;
    u8 reg;
    ret = smblib_read(smbchg_dev, AICL_CMD_REG, &reg);
    if (ret) {
		ret = 0;
    } else {
    	ret = 1;
    }
	seq_printf(buf, "%d\n", ret);
	return 0;
}

static int chargerIC_status_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, chargerIC_status_proc_read, NULL);
}

static const struct file_operations chargerIC_status_fops = {
	.owner = THIS_MODULE,
    .open = chargerIC_status_proc_open,
    .read = seq_read,
    .release = single_release,
};

void static create_chargerIC_status_proc_file(void)
{
	chargerIC_status_proc_file = proc_create(chargerIC_status_PROC_FILE, 0644, NULL, &chargerIC_status_fops);

    if (chargerIC_status_proc_file) {
		CHG_DBG("sucessed!\n");
    } else {
	    CHG_DBG("failed!\n");
    }
}
// ASUS BSP charger : BMMI Adb Interface ---

//[+++]Reset the variables for station after removing station
void asus_reset_station_para (void)
{
	union power_supply_propval pval = {POWER_SUPPLY_TYPEC_PR_DUAL, };

	station_cable_flag = 0;
	last_charger_state = BAT_CHARGER_NULL;	//Reset station charger state/stage +++
	last_charger_statge = BAT_STAGE_NULL;

	// For station get 0% +++
	//set phone CC = DRP
	power_supply_set_property(smbchg_dev->usb_psy, POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &pval);
	CHG_DBG("Station plug out, set phone CC to DRP");
	// For station get 0% ---

	//[+++]This is station USB thermal alert case
	//Need to clear the flag of station USB thermal alert after removing Station
	station_cap_zero_flag = 0;
	g_temp_station_state = 0;
	g_temp_INBOX_DT_state = 0;
	usb_alert_keep_suspend_flag_ACCY = 0;
	usb_alert_flag_ACCY = 0;
	if (g_temp_side_state == 2 || g_temp_btm_state == 2)
		g_temp_state = 2;
	else if (g_temp_side_state == 1 || g_temp_btm_state == 1)
		g_temp_state = 1;
	else
		g_temp_state = 0;
	asus_extcon_set_state_sync(smbchg_dev->thermal_extcon, g_temp_state);
	CHG_DBG("g_temp_side_state = %d, g_temp_btm_state = %d, report_state = %d\n", g_temp_side_state, g_temp_btm_state, g_temp_state);
	//[---]This is station USB thermal alert case

	return;
}
//[---]Reset the variables for station after removing station

//ASUS BSP : Add for usb_mux update when there is ACCY inserted +++
static int pogo_detect_notifier (struct notifier_block *nb, unsigned long val, void *data)
{
	enum POGO_ID pogo_type;
	int rc;
	int aspd_result = 0;

	pogo_type = val;
	CHG_DBG("pogo_type : %s", pogo_id_str[pogo_type]);	
	//ASUSEvtlog("%s: pogo_type : %s", __func__, pogo_id_str[pogo_type]);
	ASUS_POGO_ID = pogo_type;

	switch (pogo_type) {
		case NO_INSERT:
			//[+++]For Dongle device plug-out, also need to update Action-1
			CHG_DBG("Try to run asus_write_mux_setting_1. \n");
			schedule_delayed_work(&smbchg_dev->asus_mux_setting_1_work, msecs_to_jiffies(0));
			//[---]For Dongle device plug-out, also need to update Action-1
		case STATION_1ST_UNLOCK:
		case STATION_UNLOCK:
			//WA for delaying to receive NO_INSERT change
			//[+++]Skip to set EN_TRY_SNK for dual port scenario(BTM : charging, SIDE : OTG)
			if ((ASUS_POGO_ID == NO_INSERT || ASUS_POGO_ID == STATION_1ST_UNLOCK|| ASUS_POGO_ID == STATION_UNLOCK)
				&& !gpio_get_value_cansleep(global_gpio->BTM_OVP_ACOK)) {
				CHG_DBG("skip to set EN_TRY_SNK\r\n");
				rc = smblib_masked_write(smbchg_dev, TYPE_C_MODE_CFG_REG,EN_TRY_SNK_BIT, 0);
				if (rc < 0)
					CHG_DBG_E("Couldn't set TYPE_C_MODE_CFG_REG rc=%d\n", rc);
			}
			//[---]Skip to set EN_TRY_SNK for dual port scenario(BTM : charging, SIDE : OTG)

			//[+++] Send EXTCON_USB when ACCY remove & BTM SDP/CDP attached
			aspd_result = asus_get_apsd_result_by_bit();
			if (ASUS_POGO_ID == NO_INSERT && !gpio_get_value_cansleep(global_gpio->BTM_OVP_ACOK) &&
				(aspd_result == 1 || aspd_result == 2)) {
				CHG_DBG("BTM SDP/CDP, set EXTCON_USB true\n");
				extcon_set_state_sync(smbchg_dev->extcon, EXTCON_USB, false);
				schedule_delayed_work(&smbchg_dev->asus_set_usb_extcon_work, msecs_to_jiffies(1500));
			}
			//[+++] Send EXTCON_USB when ACCY remove & BTM SDP/CDP attached
			fg_station_attach_notifier(false);
			asus_reset_station_para();
			break;
		case INBOX:
		case STATION:
		case DT:
			//[+++]For Dongle device plug-in, also need to update Action-1
			CHG_DBG("Try to run asus_write_mux_setting_1. \n");
			schedule_delayed_work(&smbchg_dev->asus_mux_setting_1_work, msecs_to_jiffies(0));
			//[---]For Dongle device plug-in, also need to update Action-1
			//[---]For reboot device on Station +++
			if (get_client_vote(smbchg_dev->usb_icl_votable, PD_VOTER) == 1210000) {
				CHG_DBG("Station is in PB mode\n");
				is_Station_PB = 1;
			}
			//[---]For reboot device on Station +++
			if (pogo_type == STATION)
				fg_station_attach_notifier(true);
			break;
		case PCIE:
		case OTHER:
		case ERROR_1:
		default:
			break;
	}
	return NOTIFY_DONE;
}
//ASUS BSP : Add for usb_mux update when there is ACCY inserted ---

extern void do_ec_porta_cc_connect(void);
//ASUS BSP : Add for ACCY has thermal alert +++

void asus_thermal_accy_work(struct work_struct *work) {
	u8 hid_interrupt_result, event;
	int rc=-1;
	bool trigger_level, vbus_status, otg_status;

	trigger_level = gpio_get_value_cansleep(global_gpio->POGO_TEMP_INT);
	CHG_DBG("POGO devie : %s, trigger_level : %d\n", pogo_id_str[ASUS_POGO_ID], trigger_level);
	switch (ASUS_POGO_ID) {
	case STATION_1ST_UNLOCK:
	case STATION_UNLOCK:
		rc = ec_i2c_check_interrupt(&hid_interrupt_result, &event);
		if (rc < 0) {
			CHG_DBG("failed to get hid_to_check_interrupt!\n");
			return;
		} else {
			if (hid_interrupt_result & BIT(5)){
				CHG_DBG("Port CC !\n");
				do_ec_porta_cc_connect();
			}
		}
		break;
	case STATION:
		rc = ec_i2c_check_interrupt(&hid_interrupt_result, &event);
		CHG_DBG("hid_interrupt_result : %d, event : %d\n", hid_interrupt_result, event);
		if (rc < 0) {
			CHG_DBG("failed to get hid_to_check_interrupt!\n");
			return;
		} else {
			CHG_DBG("g_ST_SDP_mode : %d\n", g_ST_SDP_mode);
			if ((hid_interrupt_result & BIT(0)) && g_ST_SDP_mode == 1) {
				CHG_DBG("set EXTCON_USB true\n");
				extcon_set_state_sync(smbchg_dev->extcon, EXTCON_USB, true);
			}
			//[+++]This is station USB thermal alert case
			if (hid_interrupt_result & BIT(1)) {
				if (g_Charger_mode && (event == 2 || event == 1))
					cos_alert_once_flag = 1;
				if (event == 2) {
					smblib_set_usb_suspend(smbchg_dev, true);
					g_temp_station_state = 2;
					usb_alert_flag_ACCY = 1;
					CHG_DBG("Notify the Station Discharging\n");
					asus_extcon_set_state_sync(g_fgDev->st_bat_stat_extcon, 3);
				} else if (event == 1) {
					g_temp_station_state = 1;
					usb_alert_flag_ACCY = 1;
				} else if (event == 0) {
					usb_alert_flag_ACCY = 0;
					if (!gpio_get_value(global_gpio->POGO_OVP_ACOK) && (g_temp_station_state == 2))
						usb_alert_keep_suspend_flag_ACCY = 1;
					g_temp_station_state = 0;
				}
				if (g_temp_side_state == 2 || g_temp_btm_state == 2 || g_temp_station_state == 2 || g_temp_INBOX_DT_state == 2)
					g_temp_state = 2;
				else if (g_temp_side_state == 1 || g_temp_btm_state == 1 || g_temp_station_state == 1 || g_temp_INBOX_DT_state == 1)
					g_temp_state = 1;
				else
					g_temp_state = 0;
				asus_extcon_set_state_sync(smbchg_dev->thermal_extcon, g_temp_state);

				CHG_DBG("g_temp_station_state = %d, report_state = %d\n", g_temp_station_state, g_temp_state);
			}
			//[---]This is station USB thermal alert case

			//[+++] LiJen implement power bank and balance mode
			if ((hid_interrupt_result & BIT(2)) && is_hall_sensor_detect == true) {
				union power_supply_propval pval = {POWER_SUPPLY_TYPEC_PR_DUAL, };

				//set phone CC = DRP
				power_supply_set_property(smbchg_dev->usb_psy, POWER_SUPPLY_PROP_TYPEC_POWER_ROLE , &pval);
				CHG_DBG("AC plugin, set phone CC to DRP");
			}
			//[---] LiJen implement power bank and balance mode

			//[+++++]Lotta_Lu  add for station latch 
			if (hid_interrupt_result & BIT(5)){
				CHG_DBG("Port CC !\n");
				do_ec_porta_cc_connect();
			}
			//[-----]Lotta_Lu  add for station latch 
			
			// For Station get 0% +++
			if (hid_interrupt_result & BIT(6)) {
				union power_supply_propval pval = {POWER_SUPPLY_TYPEC_PR_SINK, };
				station_cap_zero_flag = 1;
				fg_station_attach_notifier(false);	//ASUS BSP notifier fg driver +++
				//set phone CC = UFP
				power_supply_set_property(smbchg_dev->usb_psy, POWER_SUPPLY_PROP_TYPEC_POWER_ROLE , &pval);
				CHG_DBG("Get station 0% event");
			}
			// For Station get 0% ---
		}
		break;
	case INBOX:
	case DT:
		vbus_status = gpio_get_value(global_gpio->POGO_OVP_ACOK);
		otg_status = gpio_get_value(global_gpio->POGO_OTG_EN);
		//[+++]If GPIO101 is low, it could trigger a fake thermal interrupt
		//this is for DT remove AC case
		if (ASUS_POGO_ID == DT && !gpio_get_value(101)) {
			CHG_DBG("GPIO 101 is low. Don't handle this thermal alert for DT ACC");
			return;
		}
		//[---]If GPIO101 is low, it could trigger a fake thermal interrupt
		if (g_Charger_mode && !trigger_level)
			cos_alert_once_flag = 1;
		if (!vbus_status && !trigger_level) {
			smblib_set_usb_suspend(smbchg_dev, true);
			pmic_set_pca9468_charging(false);
			//[+++]For OTG disable
			rc = smblib_masked_write(smbchg_dev, DCDC_CMD_OTG_REG, OTG_EN_BIT, 0);
			if (rc < 0)
				CHG_DBG_E("Failed to disable pmi_otg_en\n");
			rc = gpio_direction_output(global_gpio->POGO_OTG_EN, 0);
			if (rc)
				CHG_DBG_E("Failed to disable gpio BTM_OTG_EN\n");
			//[---]For OTG disable
			g_temp_INBOX_DT_state = 2;
			usb_alert_flag_ACCY = 1;
		} else if (otg_status && !trigger_level) {
			rc = smblib_masked_write(smbchg_dev, DCDC_CMD_OTG_REG, OTG_EN_BIT, 0);
			if (rc < 0)
				CHG_DBG_E("Failed to disable pmi_otg_en\n");
			rc = gpio_direction_output(global_gpio->POGO_OTG_EN, 0);
			if (rc)
				CHG_DBG_E("Failed to disable gpio POGO_OTG_EN\n");
			g_temp_INBOX_DT_state = 2;
			usb_alert_flag_ACCY = 1;
		} else if (!trigger_level){
			g_temp_INBOX_DT_state = 1;
			usb_alert_flag_ACCY = 1;
		} else if (trigger_level) {
			usb_alert_flag_ACCY = 0;
			if (!gpio_get_value(global_gpio->POGO_OVP_ACOK) && (g_temp_INBOX_DT_state == 2))
				usb_alert_keep_suspend_flag_ACCY = 1;
			g_temp_INBOX_DT_state = 0;
		}
		if (g_temp_side_state == 2 || g_temp_btm_state == 2 || g_temp_station_state == 2 || g_temp_INBOX_DT_state == 2)
			g_temp_state = 2;
		else if (g_temp_side_state == 1 || g_temp_btm_state == 1 || g_temp_station_state == 1 || g_temp_INBOX_DT_state == 1)
			g_temp_state = 1;
		else
			g_temp_state = 0;

		asus_extcon_set_state_sync(smbchg_dev->thermal_extcon, g_temp_state);

		CHG_DBG("g_temp_INBOX_DT_state = %d, report_state = %d\n", g_temp_INBOX_DT_state, g_temp_state);
		break;
	default:
		CHG_DBG("This is not expected case\n");
		break;
	}
	CHG_DBG("usb_alert_flag_ACCY = %d, usb_alert_keep_suspend_flag_ACCY = %d\n", usb_alert_flag_ACCY, usb_alert_keep_suspend_flag_ACCY);   
}
//ASUS BSP : Add for ACCY has thermal alert ---

void asus_set_jeita_temp_thr(void)
{
	int rc;

	//A-ZZ:0x1094~0x109B
	// For JEDI TEMP +++
	CHG_DBG("Set JEITA temp register\n");
	rc = smblib_write(smbchg_dev, CHGR_JEITA_HOT_THRESHOLD_MSB_REG, 0x20);
	if (rc < 0) {
		dev_err(smbchg_dev->dev, "Couldn't set default CHGR_JEITA_HOT_THRESHOLD_MSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, CHGR_JEITA_HOT_THRESHOLD_LSB_REG, 0xB8);
	if (rc < 0) {
		dev_err(smbchg_dev->dev, "Couldn't set default CHGR_JEITA_HOT_THRESHOLD_LSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, CHGR_JEITA_COLD_THRESHOLD_MSB_REG, 0x4C);
	if (rc < 0) {
		dev_err(smbchg_dev->dev, "Couldn't set default CHGR_JEITA_COLD_THRESHOLD_MSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, CHGR_JEITA_COLD_THRESHOLD_LSB_REG, 0xCC);
	if (rc < 0) {
		dev_err(smbchg_dev->dev, "Couldn't set default CHGR_JEITA_COLD_THRESHOLD_LSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, CHGR_JEITA_THOT_THRESHOLD_MSB_REG, 0x18);
	if (rc < 0) {
		dev_err(smbchg_dev->dev, "Couldn't set default CHGR_JEITA_THOT_THRESHOLD_MSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, CHGR_JEITA_THOT_THRESHOLD_LSB_REG, 0x1D);
	if (rc < 0) {
		dev_err(smbchg_dev->dev, "Couldn't set default CHGR_JEITA_THOT_THRESHOLD_LSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, CHGR_JEITA_TCOLD_THRESHOLD_MSB_REG, 0x58);
	if (rc < 0) {
		dev_err(smbchg_dev->dev, "Couldn't set default CHGR_JEITA_TCOLD_THRESHOLD_MSB_REG rc=%d\n", rc);
	}
	rc = smblib_write(smbchg_dev, CHGR_JEITA_TCOLD_THRESHOLD_LSB_REG, 0xCD);
	if (rc < 0) {
		dev_err(smbchg_dev->dev, "Couldn't set default CHGR_JEITA_TCOLD_THRESHOLD_LSB_REG rc=%d\n", rc);
	}
}

void call_smb5_pmsp_extcon(int value)
{
	asus_extcon_set_state_sync(smbchg_dev->pmsp_extcon, value);
}
EXPORT_SYMBOL(call_smb5_pmsp_extcon);

static int smb5_probe(struct platform_device *pdev)
{
	struct smb5 *chip;
	struct smb_charger *chg;
	struct gpio_control *gpio_ctrl;	//ASUS BSP +++
	int rc = 0;

	CHG_DBG("+++\n");
	
	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	//ASUS BSP allocate GPIO control +++
	gpio_ctrl = devm_kzalloc(&pdev->dev, sizeof(*gpio_ctrl), GFP_KERNEL);
	if (!gpio_ctrl)
		return -ENOMEM;
	//ASUS BSP allocate GPIO control ---

	chg = &chip->chg;
	chg->dev = &pdev->dev;
	chg->debug_mask = &__debug_mask;
	chg->pd_disabled = 0;
	chg->weak_chg_icl_ua = 500000;
	chg->mode = PARALLEL_MASTER;
	chg->irq_info = smb5_irqs;
	chg->die_health = -EINVAL;
	chg->connector_health = -EINVAL;
	chg->otg_present = false;
	chg->main_fcc_max = -EINVAL;
	mutex_init(&chg->adc_lock);

	smbchg_dev = chg;			//ASUS BSP add globe device struct +++
	global_gpio = gpio_ctrl;		//ASUS BSP add gpio control struct +++

	chg->regmap = dev_get_regmap(chg->dev->parent, NULL);
	if (!chg->regmap) {
		CHG_DBG_E("parent regmap is missing\n");
		return -EINVAL;
	}

	rc = smb5_chg_config_init(chip);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			CHG_DBG_E("Couldn't setup chg_config rc=%d\n", rc);
		return rc;
	}

	rc = smb5_parse_dt(chip);
	if (rc < 0) {
		CHG_DBG_E("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	if (alarmtimer_get_rtcdev())
		alarm_init(&chg->lpd_recheck_timer, ALARM_REALTIME,
				smblib_lpd_recheck_timer);
	else
		return -EPROBE_DEFER;

	rc = smblib_init(chg);
	if (rc < 0) {
		CHG_DBG_E("Smblib_init failed rc=%d\n", rc);
		return rc;
	}

	/* set driver data before resources request it */
	platform_set_drvdata(pdev, chip);

	//ASUS BSP : Add asus_workque +++
	INIT_DELAYED_WORK(&chg->asus_thermal_btm_work, asus_thermal_btm_work);
	INIT_DELAYED_WORK(&chg->asus_thermal_pogo_work, asus_thermal_pogo_work);
	INIT_DELAYED_WORK(&chg->asus_thermal_accy_work, asus_thermal_accy_work);
	//ASUS BSP : Add asus_workque ---

	//ASUS BSP : Charger default PMIC settings +++
	asus_probe_pmic_settings(chg);

	//ASUS BSP : Request charger GPIO +++
	asus_probe_gpio_setting(pdev, gpio_ctrl);

	/* extcon registration */
	chg->extcon = devm_extcon_dev_allocate(chg->dev, smblib_extcon_cable);
	if (IS_ERR(chg->extcon)) {
		rc = PTR_ERR(chg->extcon);
		dev_err(chg->dev, "failed to allocate extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	rc = devm_extcon_dev_register(chg->dev, chg->extcon);
	if (rc < 0) {
		dev_err(chg->dev, "failed to register extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	/* Support reporting polarity and speed via properties */
	rc = extcon_set_property_capability(chg->extcon,
			EXTCON_USB, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(chg->extcon,
			EXTCON_USB, EXTCON_PROP_USB_SS);
	rc |= extcon_set_property_capability(chg->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(chg->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0) {
		dev_err(chg->dev,
			"failed to configure extcon capabilities\n");
		goto cleanup;
	}

/* ASUS BSP charger+++  asus extcon registration +++ */
	chg->thermal_extcon = extcon_dev_allocate(asus_extcon_cable);
	if (IS_ERR(chg->thermal_extcon)) {
		rc = PTR_ERR(chg->thermal_extcon);
		dev_err(chg->dev, "[BAT][CHG] failed to allocate ASUS thermal extcon device rc=%d\n",
				rc);
		goto cleanup;
	}
	chg->thermal_extcon->fnode_name = "usb_connector";

	rc = extcon_dev_register(chg->thermal_extcon);
	if (rc < 0) {
		dev_err(chg->dev, "[BAT][CHG] failed to register ASUS thermal extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	chg->quickchg_extcon = extcon_dev_allocate(asus_extcon_cable);
	if (IS_ERR(chg->quickchg_extcon)) {
		rc = PTR_ERR(chg->quickchg_extcon);
		dev_err(chg->dev, "[BAT][CHG] failed to allocate ASUS quickchg extcon device rc=%d\n",
				rc);
		goto cleanup;
	}
	chg->quickchg_extcon->fnode_name = "quick_charging";

	rc = extcon_dev_register(chg->quickchg_extcon);
	if (rc < 0) {
		dev_err(chg->dev, "[BAT][CHG] failed to register ASUS quickchg extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	chg->pmsp_extcon = extcon_dev_allocate(asus_extcon_cable);
	if (IS_ERR(chg->pmsp_extcon)) {
		rc = PTR_ERR(chg->pmsp_extcon);
		dev_err(chg->dev, "[BAT][CHG] failed to allocate ASUS pmsp extcon device rc=%d\n",
				rc);
		goto cleanup;
	}
	chg->pmsp_extcon->fnode_name = "PowerManagerServicePrinter";

	rc = extcon_dev_register(chg->pmsp_extcon);
	if (rc < 0) {
		dev_err(chg->dev, "[BAT][CHG] failed to register ASUS pmsp extcon device rc=%d\n",
				rc);
		goto cleanup;
	}

	chg->invalid_audiodongle_extcon = extcon_dev_allocate(asus_extcon_cable);
	if (IS_ERR(chg->invalid_audiodongle_extcon)) {
		rc = PTR_ERR(chg->invalid_audiodongle_extcon);
		dev_err(chg->dev, "[BAT][CHG] failed to allocate ASUS invalid audio dongle extcon device rc=%d\n",
				rc);
		goto cleanup;
	}
	chg->invalid_audiodongle_extcon->fnode_name = "invalid_dongle";

	rc = extcon_dev_register(chg->invalid_audiodongle_extcon);
	if (rc < 0) {
		dev_err(chg->dev, "[BAT][CHG] failed to register ASUS invalid audio dongle extcon device rc=%d\n",
				rc);
		goto cleanup;
	}
/* ASUS BSP charger+++  asus extcon registration --- */

	rc = smb5_init_hw(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto cleanup;
	}

	//ASUS BSP add for ftm variable +++
	if (g_ftm_mode)
		charger_limit_enable_flag = 1;

	// ASUS BSP add a file for SMMI adb interface +++
	create_chargerIC_status_proc_file();

	//ASUS BSP Austin_T : CHG_ATTRs +++
	rc = sysfs_create_group(&chg->dev->kobj, &asus_smblib_attr_group);
	if (rc)
		goto cleanup;
	//ASUS BSP Austin_T : CHG_ATTRs ---

	/*
	 * VBUS regulator enablement/disablement for host mode is handled
	 * by USB-PD driver only. For micro-USB and non-PD typeC designs,
	 * the VBUS regulator is enabled/disabled by the smb driver itself
	 * before sending extcon notifications.
	 * Hence, register vbus and vconn regulators for PD supported designs
	 * only.
	 */
	if (!chg->pd_not_supported) {
		rc = smb5_init_vbus_regulator(chip);
		if (rc < 0) {
			pr_err("Couldn't initialize vbus regulator rc=%d\n",
				rc);
			goto cleanup;
		}

		rc = smb5_init_vconn_regulator(chip);
		if (rc < 0) {
			pr_err("Couldn't initialize vconn regulator rc=%d\n",
				rc);
			goto cleanup;
		}
	}

	switch (chg->chg_param.smb_version) {
	case PM8150B_SUBTYPE:
	case PM6150_SUBTYPE:
	case PM7250B_SUBTYPE:
		rc = smb5_init_dc_psy(chip);
		if (rc < 0) {
			pr_err("Couldn't initialize dc psy rc=%d\n", rc);
			goto cleanup;
		}
		break;
	default:
		break;
	}

	rc = smb5_init_usb_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_init_usb_main_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb main psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_init_usb_port_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize usb pc_port psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_init_batt_psy(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_init_typec_class(chip);
	if (rc < 0) {
		pr_err("Couldn't initialize typec class rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_determine_initial_status(chip);
	if (rc < 0) {
		pr_err("Couldn't determine initial status rc=%d\n",
			rc);
		goto cleanup;
	}

	rc = smb5_request_interrupts(chip);
	if (rc < 0) {
		pr_err("Couldn't request interrupts rc=%d\n", rc);
		goto cleanup;
	}

	rc = smb5_post_init(chip);
	if (rc < 0) {
		pr_err("Failed in post init rc=%d\n", rc);
		goto free_irq;
	}

	smb5_create_debugfs(chip);

	rc = sysfs_create_groups(&chg->dev->kobj, smb5_groups);
	if (rc < 0) {
		pr_err("Couldn't create sysfs files rc=%d\n", rc);
		goto free_irq;
	}

	rc = smb5_show_charger_status(chip);
	if (rc < 0) {
		pr_err("Failed in getting charger status rc=%d\n", rc);
		goto free_irq;
	}

	device_init_wakeup(chg->dev, true);
	//ASUS BSP : Add for usb_mux update when there is ACCY inserted +++
	pogo_detect_nb.notifier_call = pogo_detect_notifier;
	ec_hid_event_register(&pogo_detect_nb);
	//ASUS BSP : Add for usb_mux update when there is ACCY inserted ---

	//[+++] Add for station update when screen on
	
	smb5_drm_nb.notifier_call = smb5_drm_notifier;
	if (drm_panel_notifier_register(active_panel_asus, &smb5_drm_nb) < 0)
		CHG_DBG_E("Failed to register fb notifier client\n");

	//[---] Add for station update when screen on

	//ASUS BSP : Reset JEITA Temp Register +++
	asus_set_jeita_temp_thr();

	if (g_Charger_mode) {
		schedule_delayed_work(&smbchg_dev->asus_thermal_btm_work, msecs_to_jiffies(20000));
		schedule_delayed_work(&smbchg_dev->asus_thermal_pogo_work, msecs_to_jiffies(20100));	
	}

	g_smb5_probe_complete = true;
	pr_info("QPNP SMB5 probed successfully\n");

	return rc;

free_irq:
	smb5_free_interrupts(chg);
cleanup:
	smblib_deinit(chg);
	platform_set_drvdata(pdev, NULL);

	//ASUS BSP Austin_T : CHG_ATTRs +++
		sysfs_remove_group(&chg->dev->kobj, &asus_smblib_attr_group);
	//ASUS BSP Austin_T : CHG_ATTRs ---

	return rc;
}

//ASUS BSP charger : Add suspend/resume function +++
void rerun_thermal_alert_detect_func(void)
{
/* todo porting
	int rc;
	u8 reg = 0;
	u8 pogo_therm_mask = 0xC0;
	u8 btm_therm_mask = 0x30;

	if (!asp1690e_ready) {
		CHG_DBG_E("ADC is not ready, bypass interrupt\n");
		return;
	}

	rc = asp1690e_read_reg(0x41, &reg);
	rc = asp1690e_read_reg(0x42, &reg);
	if (rc < 0) {		
		CHG_DBG_E("fail to read asp1690e ADC, rerun thermal alert detction\n");
		rerun_thermal_alert_flag = 1;
		return;
	}

	if (reg & pogo_therm_mask) {
		CHG_DBG("Side Thermal Alert interrupt\n");
		schedule_delayed_work(&smbchg_dev->asus_thermal_pogo_work, 0);
	} else if (reg & btm_therm_mask) {
		CHG_DBG("Bottom Thermal Alert interrupt\n");
		schedule_delayed_work(&smbchg_dev->asus_thermal_btm_work, 0);
	}
todo porting */
}

#define JEITA_MINIMUM_INTERVAL (30)
static int smb5_resume(struct device *dev)
{
	struct timespec mtNow;
	int nextJEITAinterval;

	if (rerun_thermal_alert_flag == 1) {
		CHG_DBG_E("There is i2c error in previous thermal alert detction\n");
		rerun_thermal_alert_flag = 0;
		rerun_thermal_alert_detect_func();
	}

	if (!asus_get_prop_usb_present(smbchg_dev)) {
		return 0;
	}

	if (!asus_flow_done_flag)
		return 0;

	vote(smbchg_dev->awake_votable, ASUS_CHG_VOTER, true, 0);
	mtNow = current_kernel_time();

	/*BSP Austin_Tseng: if next JEITA time less than 30s, do JEITA
			(next JEITA time = last JEITA time + 60s)*/
	nextJEITAinterval = 60 - (mtNow.tv_sec - last_jeita_time.tv_sec);
	CHG_DBG("nextJEITAinterval = %d\n", nextJEITAinterval);
	if (nextJEITAinterval <= JEITA_MINIMUM_INTERVAL) {
		cancel_delayed_work(&smbchg_dev->asus_min_monitor_work);
		cancel_delayed_work(&smbchg_dev->asus_cable_check_work);
		cancel_delayed_work(&smbchg_dev->asus_batt_RTC_work);
		schedule_delayed_work(&smbchg_dev->asus_min_monitor_work, 0);
	} else {
		smblib_asus_monitor_start(smbchg_dev, nextJEITAinterval * 1000);
		vote(smbchg_dev->awake_votable, ASUS_CHG_VOTER, false, 0);
	}
	return 0;
}

static int smb5_suspend(struct device *dev)
{
	return 0;
}
//ASUS BSP charger : Add suspend/resume function +++

static int smb5_remove(struct platform_device *pdev)
{
	struct smb5 *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;

	/* force enable APSD */
	smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
				BC1P2_SRC_DETECT_BIT, BC1P2_SRC_DETECT_BIT);

	smb5_free_interrupts(chg);
	smblib_deinit(chg);
	sysfs_remove_groups(&chg->dev->kobj, smb5_groups);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void smb5_shutdown(struct platform_device *pdev)
{
	struct smb5 *chip = platform_get_drvdata(pdev);
	struct smb_charger *chg = &chip->chg;

	/* disable all interrupts */
	smb5_disable_interrupts(chg);

	/* disable the SMB_EN configuration */
	smblib_masked_write(chg, MISC_SMB_EN_CMD_REG, EN_CP_CMD_BIT, 0);

	/* configure power role for UFP */
	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC)
		smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				TYPEC_POWER_ROLE_CMD_MASK, EN_SNK_ONLY_BIT);

	/* force enable and rerun APSD */
	smblib_apsd_enable(chg, true);
	smblib_hvdcp_exit_config(chg);
}

//ASUS BSP +++
static const struct dev_pm_ops smb5_pm_ops = {
	.suspend	= smb5_suspend,
	.resume		= smb5_resume,
};
//ASUS BSP ---

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,qpnp-smb5", },
	{ },
};

static struct platform_driver smb5_driver = {
	.driver		= {
		.name		= "qcom,qpnp-smb5",
		.of_match_table	= match_table,
		.pm		= &smb5_pm_ops,	//ASUS BSP +++
	},
	.probe		= smb5_probe,
	.remove		= smb5_remove,
	.shutdown	= smb5_shutdown,
};
module_platform_driver(smb5_driver);

MODULE_DESCRIPTION("QPNP SMB5 Charger Driver");
MODULE_LICENSE("GPL v2");

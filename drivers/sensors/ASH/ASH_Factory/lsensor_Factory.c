/* 
 * Copyright (C) 2015 ASUSTek Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

 /************************************/
/* Light Sensor Factory Module */
/***********************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/input/ASH.h>

/**************************/
/* Debug and Log System */
/************************/
#define MODULE_NAME			"ASH_Factory"
#define SENSOR_TYPE_NAME		"light"

#undef dbg
#ifdef ASH_FACTORY_DEBUG
	#define dbg(fmt, args...) printk(KERN_DEBUG "[%s][%s]"fmt,MODULE_NAME,SENSOR_TYPE_NAME,##args)
#else
	#define dbg(fmt, args...)
#endif
#define log(fmt, args...) printk(KERN_INFO "[%s][%s]"fmt,MODULE_NAME,SENSOR_TYPE_NAME,##args)
#define err(fmt, args...) printk(KERN_ERR "[%s][%s]"fmt,MODULE_NAME,SENSOR_TYPE_NAME,##args)

/*******************************************/
/* Light Sensor read/write Calibration*/
/******************************************/
int lsensor_factory_read_200lux(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Light Sensor read 200lux Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Light Sensor read 200lux Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Light Sensor read 200lux Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Light Sensor read 200lux Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Light Sensor read 200lux Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(lsensor_factory_read_200lux);

bool lsensor_factory_write_200lux(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Light Sensor write 200lux Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Light Sensor write 200lux Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Light Sensor write 200lux Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(lsensor_factory_write_200lux);

int lsensor_factory_read_1000lux(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Light Sensor read 300lux Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Light Sensor read 300lux Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Light Sensor read 300lux Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Light Sensor read 300lux Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Light Sensor read 300lux Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(lsensor_factory_read_1000lux);

bool lsensor_factory_write_1000lux(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Light Sensor write 1000lux Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Light Sensor write 1000lux Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Light Sensor write 1000lux Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(lsensor_factory_write_1000lux);

int lsensor_factory_read(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Light Sensor read Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Light Sensor read Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Light Sensor read Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Light Sensor read Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Light Sensor read Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(lsensor_factory_read);

bool lsensor_factory_write(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Light Sensor write Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Light Sensor write Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Light Sensor write Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(lsensor_factory_write);

/*For transition period from 100ms to 50ms +++*/
int lsensor_factory_read_50ms(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Light Sensor read 50MS Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Light Sensor read 50MS Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Light Sensor read 50MS Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Light Sensor read 50MS Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Light Sensor read 50MS Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(lsensor_factory_read_50ms);

bool lsensor_factory_write_50ms(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Light Sensor write 50MS Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Light Sensor write 50MS Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Light Sensor write 50MS Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(lsensor_factory_write_50ms);

int lsensor_factory_read_100ms(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Light Sensor read 100MS Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Light Sensor read 100MS Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Light Sensor read 100MS Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Light Sensor read 100MS Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Light Sensor read 100MS Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(lsensor_factory_read_100ms);

bool lsensor_factory_write_100ms(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Light Sensor write 100MS Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Light Sensor write 100MS Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Light Sensor write 100MS Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(lsensor_factory_write_100ms);

/*For transition period from 100ms to 50ms ---*/

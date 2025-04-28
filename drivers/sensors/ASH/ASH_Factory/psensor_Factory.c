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

 /*****************************************/
/* Proximity Sensor Factory Module */
/****************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/input/ASH.h>

/**************************/
/* Debug and Log System */
/************************/
#define MODULE_NAME			"ASH_Factory"
#define SENSOR_TYPE_NAME		"proximity"

#undef dbg
#ifdef ASH_FACTORY_DEBUG
	#define dbg(fmt, args...) printk(KERN_DEBUG "[%s][%s]"fmt,MODULE_NAME,SENSOR_TYPE_NAME,##args)
#else
	#define dbg(fmt, args...)
#endif
#define log(fmt, args...) printk(KERN_INFO "[%s][%s]"fmt,MODULE_NAME,SENSOR_TYPE_NAME,##args)
#define err(fmt, args...) printk(KERN_ERR "[%s][%s]"fmt,MODULE_NAME,SENSOR_TYPE_NAME,##args)

/***************************************/
/* Proximity read/write Calibration File*/
/**************************************/
int psensor_factory_read_high(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Proximity read High Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Proximity read High Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Proximity read High Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Proximity read High Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Proximity read High Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(psensor_factory_read_high);

bool psensor_factory_write_high(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Proximity write High Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Proximity Hi-Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Proximity write High Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(psensor_factory_write_high);

int psensor_factory_read_low(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Proximity read Low Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Proximity read Low Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Proximity read Low Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Proximity read Low Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Proximity read Low Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(psensor_factory_read_low);

bool psensor_factory_write_low(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Proximity write Low Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Proximity Lo-Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Proximity write Low Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(psensor_factory_write_low);


/********************************/
/* Proximity Inf calibration*/
/*******************************/
int psensor_factory_read_inf(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Proximity read INF Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Proximity read INF Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Proximity read INF Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Proximity read INF Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Proximity read INF Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(psensor_factory_read_inf);

bool psensor_factory_write_inf(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Proximity write INF Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Proximity INF-Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Proximity write INF Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(psensor_factory_write_inf);

/*For transition period from 3/5 to 2/4*/
int psensor_factory_read_2cm(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Proximity read 2CM Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Proximity read 2CM Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Proximity read 2CM Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Proximity read 2CM Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Proximity read 2CM Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(psensor_factory_read_2cm);

bool psensor_factory_write_2cm(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Proximity write 2CM Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Proximity 2CM Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Proximity write 2CM Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(psensor_factory_write_2cm);

int psensor_factory_read_4cm(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Proximity read 4CM Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Proximity read 4CM Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Proximity read 4CM Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Proximity read 4CM Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Proximity read 4CM Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(psensor_factory_read_4cm);

bool psensor_factory_write_4cm(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Proximity write 4CM Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Proximity 4CM Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Proximity write 4CM Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(psensor_factory_write_4cm);

int psensor_factory_read_3cm(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Proximity read 3CM Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Proximity read 3CM Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Proximity read 3CM Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Proximity read 3CM Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Proximity read 3CM Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(psensor_factory_read_3cm);

bool psensor_factory_write_3cm(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Proximity write 3CM Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Proximity 3CM Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Proximity write 3CM Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(psensor_factory_write_3cm);

int psensor_factory_read_5cm(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Proximity read 5CM Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Proximity read 5CM Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Proximity read 5CM Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Proximity read 5CM Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Proximity read 5CM Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(psensor_factory_read_5cm);

bool psensor_factory_write_5cm(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Proximity write 5CM Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Proximity 5CM Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Proximity write 5CM Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(psensor_factory_write_5cm);

int psensor_factory_read_1cm(const char *str, struct device *dev)
{
	const struct firmware *fw;
	int cal_val = 0;
	int ret = 0;
	char buf[8] = {0};
	size_t readlen = 0;

	if (!str || !dev) {
		pr_err("Proximity read 1CM Calibration: invalid arguments\n");
		return -EINVAL;
	}

	ret = request_firmware(&fw, str, dev);
	if (ret) {
		pr_err("Proximity read 1CM Calibration: failed to request firmware %s (%d)\n", str, ret);
		return ret;
	}

	readlen = min_t(size_t, fw->size, sizeof(buf) - 1);
	memcpy(buf, fw->data, readlen);
	buf[readlen] = '\0';

	ret = kstrtoint(buf, 10, &cal_val);
	if (ret) {
		pr_err("Proximity read 1CM Calibration: invalid content format in %s\n", str);
		cal_val = -EINVAL;
	} else if (cal_val < 0) {
		pr_err("Proximity read 1CM Calibration: invalid value (%d)\n", cal_val);
		cal_val = -EINVAL;
	} else {
		pr_info("Proximity read 1CM Calibration: %d\n", cal_val);
	}

	release_firmware(fw);

	return cal_val;
}
EXPORT_SYMBOL(psensor_factory_read_1cm);

bool psensor_factory_write_1cm(int calvalue, const char *str)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];	

	sprintf(buf, "%d", calvalue);
	
	fp = filp_open(str, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		err("Proximity write 1CM Calibration open (%s) fail\n", str);
		return false;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL) {
		pos_lsts = 0;
		vfs_write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		err("Proximity 1CM Calibration strlen: f_op=NULL or op->write=NULL\n");
		set_fs(old_fs);
		filp_close(fp, NULL);
		return false;
	}
	set_fs(old_fs);
	filp_close(fp, NULL);
	
	log("Proximity write 1CM Calibration : %s\n", buf);
	
	return true;
}
EXPORT_SYMBOL(psensor_factory_write_1cm);


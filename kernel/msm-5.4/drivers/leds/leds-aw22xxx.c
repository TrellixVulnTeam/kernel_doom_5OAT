/*
 * leds-aw22xxx.c   aw22xxx led module
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/leds.h>
#include <linux/leds-aw22xxx.h>
#include <linux/leds-aw22xxx-reg.h>
#include "leds-aw22xxx-lamp.h"
/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW22XXX_I2C_NAME "aw22xxx_led"

#define AW22XXX_DRIVER_VERSION "v1.1.3"

#define AW_I2C_RETRIES 2
#define AW_I2C_RETRY_DELAY 1
#define AW_READ_CHIPID_RETRIES 2
#define AW_READ_CHIPID_RETRY_DELAY 1

/******************************************************
 *
 * aw22xxx led parameter
 *
 ******************************************************/
#define AW22XXX_CFG_NAME_MAX        64
static char *aw22xxx_fw_name = "aw22127_fw.bin";
static char aw22xxx_cfg_name[][AW22XXX_CFG_NAME_MAX] = {
    {"aw22127_cfg_0.bin"},
    {"aw22127_cfg_1.bin"},
    {"aw22127_cfg_2.bin"},
    {"aw22127_cfg_3.bin"},
    {"aw22127_cfg_4.bin"},
    {"aw22127_cfg_5.bin"},
    {"aw22127_cfg_6.bin"},
    {"aw22127_cfg_7.bin"},
    {"aw22127_cfg_8.bin"},
    {"aw22127_cfg_9.bin"},
    {"aw22127_cfg_10.bin"},
    {"aw22127_cfg_11.bin"},
    {"aw22127_cfg_12.bin"},
    {"aw22127_cfg_13.bin"},
    {"aw22127_cfg_14.bin"},
    {"aw22127_cfg_15.bin"},
    {"aw22127_cfg_16.bin"},
    {"aw22127_cfg_17.bin"},
    {"aw22127_cfg_18.bin"},
    {"aw22127_cfg_19.bin"},
    {"aw22127_cfg_20.bin"},
};

static char aw22xxx_fan_cfg_name[][AW22XXX_CFG_NAME_MAX] = {
    {"aw22127_lamp2_0.bin"},
    {"aw22127_lamp2_1.bin"},
    {"aw22127_lamp2_2.bin"},
    {"aw22127_lamp2_3.bin"},
    {"aw22127_lamp2_4.bin"},
    {"aw22127_lamp2_5.bin"},
    {"aw22127_lamp2_6.bin"},
    {"aw22127_lamp2_7.bin"},
    {"aw22127_lamp2_8.bin"},
    {"aw22127_lamp2_9.bin"},
    {"aw22127_lamp2_10.bin"},
    {"aw22127_lamp2_11.bin"},
    {"aw22127_lamp2_12.bin"},
    {"aw22127_lamp2_13.bin"},
    {"aw22127_lamp2_14.bin"},
    {"aw22127_lamp2_15.bin"},
    {"aw22127_lamp2_16.bin"},
    {"aw22127_lamp2_17.bin"},
    {"aw22127_lamp2_18.bin"},
    {"aw22127_lamp2_19.bin"},
    {"aw22127_lamp2_20.bin"},
};
const char aw22xxx_chip_init_cfg[] = "chip_init.bin";

#define AW22XXX_IMAX_NAME_MAX       32
static char aw22xxx_imax_name[][AW22XXX_IMAX_NAME_MAX] = {
    {"AW22XXX_IMAX_2mA"},
    {"AW22XXX_IMAX_3mA"},
    {"AW22XXX_IMAX_4mA"},
    {"AW22XXX_IMAX_6mA"},
    {"AW22XXX_IMAX_9mA"},
    {"AW22XXX_IMAX_10mA"},
    {"AW22XXX_IMAX_15mA"},
    {"AW22XXX_IMAX_20mA"},
    {"AW22XXX_IMAX_30mA"},
    {"AW22XXX_IMAX_40mA"},
    {"AW22XXX_IMAX_45mA"},
    {"AW22XXX_IMAX_60mA"},
    {"AW22XXX_IMAX_75mA"},
};
static char aw22xxx_imax_code[] = {
    AW22XXX_IMAX_2mA,
    AW22XXX_IMAX_3mA,
    AW22XXX_IMAX_4mA,
    AW22XXX_IMAX_6mA,
    AW22XXX_IMAX_9mA,
    AW22XXX_IMAX_10mA,
    AW22XXX_IMAX_15mA,
    AW22XXX_IMAX_20mA,
    AW22XXX_IMAX_30mA,
    AW22XXX_IMAX_40mA,
    AW22XXX_IMAX_45mA,
    AW22XXX_IMAX_60mA,
    AW22XXX_IMAX_75mA,
};

struct aw22xxx_reg_data {
    int size;
    struct aw22xxx_reg_confg data[];
};
struct aw22xxx_reg_data *aw22xxx_data;
struct aw22xxx_reg_data *aw22xxx_data_fan;
static struct aw22xxx_reg_data *aw22xxx_data_fill = NULL;
static struct aw22xxx_reg_data *aw22xxx_data_warter = NULL;
static struct aw22xxx_reg_data *aw22xxx_data_breath = NULL;
static struct aw22xxx_reg_data *aw22xxx_data_cqa = NULL;
/******************************************************
 *
 * aw22xxx i2c write/read
 *
 ******************************************************/
static int aw22xxx_i2c_write(struct aw22xxx *aw22xxx,
        unsigned char reg_addr, unsigned char reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_write_byte_data(aw22xxx->i2c, reg_addr, reg_data);
        if(ret < 0) {
            pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

static int aw22xxx_i2c_read(struct aw22xxx *aw22xxx,
        unsigned char reg_addr, unsigned char *reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_read_byte_data(aw22xxx->i2c, reg_addr);
        if(ret < 0) {
            pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            *reg_data = ret;
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

static int aw22xxx_i2c_write_bits(struct aw22xxx *aw22xxx,
        unsigned char reg_addr, unsigned char mask, unsigned char reg_data)
{
    unsigned char reg_val;

    aw22xxx_i2c_read(aw22xxx, reg_addr, &reg_val);
    reg_val &= mask;
    reg_val |= reg_data;
    aw22xxx_i2c_write(aw22xxx, reg_addr, reg_val);

    return 0;
}

#ifdef AW22XXX_FLASH_I2C_WRITES
static int aw22xxx_i2c_writes(struct aw22xxx *aw22xxx,
        unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
    int ret = -1;
    unsigned char *data;

    data = kmalloc(len+1, GFP_KERNEL);
    if (data == NULL) {
        pr_err("%s: can not allocate memory\n", __func__);
        return  -ENOMEM;
    }

    data[0] = reg_addr;
    memcpy(&data[1], buf, len);

    ret = i2c_master_send(aw22xxx->i2c, data, len+1);
    if (ret < 0) {
        pr_err("%s: i2c master send error\n", __func__);
    }

    kfree(data);

    return ret;
}
#endif

/*****************************************************
 *
 * aw22xxx led cfg
 *
 *****************************************************/
static int aw22xxx_i2c_write_config(struct aw22xxx *aw22xxx,
        struct aw22xxx_reg_confg *config, unsigned int size)
{
    int i = 0;
    pr_debug("%d\n", size);
    for (i=0; i < size; i++) {
        pr_debug("aw22xxx: 0x%x, 0x%x\n", config[i].reg, config[i].data);
        aw22xxx_i2c_write(aw22xxx, config[i].reg, config[i].data);
    }
    return 0;
}

static int aw22xxx_effect_1_test(struct aw22xxx *aw22xxx)
{
    unsigned int size;
    size = sizeof(config_1)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, config_1, size);
    return 0;
}

static int aw22xxx_i2c_chip_init(struct aw22xxx *aw22xxx)
{
    unsigned int size;
    size = sizeof(chip_init)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, chip_init, size);
    //pr_info("%s\n", __func__);
    return 0;
}

static int aw22xxx_lamp1_on(struct aw22xxx *aw22xxx, unsigned int rgb, unsigned int brigtness)
{
    char rgb_l = rgb & 0xf; /* 0xf */
    char RGB = (rgb >> 4) & 0xf; /* 0xf0 */
    unsigned int size;
    //pr_info("aw22xxx: rgb_l = %d, RGB = %d\n", rgb_l, RGB);

    size = sizeof(lamp1_on_setting)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp1_on_setting, size);

    /* lamp1_on_setting_rgb */
    if (rgb_l == AW22XXX_RGB_R) {
        size = sizeof(lamp1_2_on_setting_r)/sizeof(struct aw22xxx_reg_confg);
        aw22xxx_i2c_write_config(aw22xxx, lamp1_2_on_setting_r, size);
    }else if (rgb_l == AW22XXX_RGB_G) {
        size = sizeof(lamp1_2_on_setting_g)/sizeof(struct aw22xxx_reg_confg);
        aw22xxx_i2c_write_config(aw22xxx, lamp1_2_on_setting_g, size);
    }else if (rgb_l == AW22XXX_RGB_B) {
        size = sizeof(lamp1_2_on_setting_b)/sizeof(struct aw22xxx_reg_confg);
        aw22xxx_i2c_write_config(aw22xxx, lamp1_2_on_setting_b, size);
    }

    size = sizeof(lamp1_2_on_config)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp1_2_on_config, size);

    aw22xxx_i2c_write(aw22xxx, 0x6, RGB-1);
    aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);

    size = sizeof(lamp1_on_setting_bn)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp1_on_setting_bn, size);

    return 0;
}

static int aw22xxx_lamp2_on(struct aw22xxx *aw22xxx, unsigned int rgb, unsigned int brigtness)
{
    char rgb_l = rgb & 0xf; /* 0xf */
    char RGB = (rgb >> 4) & 0xf; /* 0xf0 */
    unsigned int size;
    //pr_info("aw22xxx: fan rgb_l = %d, RGB = %d\n", rgb_l, RGB);

#if 0
    size = sizeof(lamp2_on_setting)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp2_on_setting, size);

    /* lamp1_on_setting_rgb */
    if (rgb_l == AW22XXX_RGB_R) {
        size = sizeof(lamp1_2_on_setting_r)/sizeof(struct aw22xxx_reg_confg);
        aw22xxx_i2c_write_config(aw22xxx, lamp1_2_on_setting_r, size);
    }else if (rgb_l == AW22XXX_RGB_G) {
        size = sizeof(lamp1_2_on_setting_g)/sizeof(struct aw22xxx_reg_confg);
        aw22xxx_i2c_write_config(aw22xxx, lamp1_2_on_setting_g, size);
    }else if (rgb_l == AW22XXX_RGB_B) {
        size = sizeof(lamp1_2_on_setting_b)/sizeof(struct aw22xxx_reg_confg);
        aw22xxx_i2c_write_config(aw22xxx, lamp1_2_on_setting_b, size);
    }

    size = sizeof(lamp1_2_on_config)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp1_2_on_config, size);

    aw22xxx_i2c_write(aw22xxx, 0x6, RGB-1);
    aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);

    size = sizeof(lamp2_on_setting_bn)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp2_on_setting_bn, size);

#else
    size = sizeof(lamp2_led_rgb[0])/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp2_led_rgb[(RGB-1)*3 + (rgb_l-1)], size);
#endif

    return 0;
}

#if 0
static int aw22xxx_lamp_breath_on(struct aw22xxx *aw22xxx, unsigned int rgb, unsigned int brigtness)
{
    char RGB_B = rgb & 0xff; /* 0xff */
    char RGB_R = (rgb >> 8) & 0xff; /* 0xff00 */
    char RGB_G = (rgb >> 16) & 0xff; /* 0xff0000 */
    unsigned int size;
    pr_info("aw22xxx: RGB_R = %d, RGB_G = %d,RGB_B = %d\n", RGB_R, RGB_G,RGB_B);

    size = sizeof(lamp1_on_setting)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp1_on_setting, size);

    /* lamp1_on_setting_rgb */
    aw22xxx_i2c_write(aw22xxx, 0x6, RGB_R);
    aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);
    aw22xxx_i2c_write(aw22xxx, 0x6, RGB_G);
    aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);
    aw22xxx_i2c_write(aw22xxx, 0x6, RGB_B);
    aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);

    size = sizeof(lamp1_on_setting_breath)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp1_on_setting_breath, size);

    return 0;
}
#endif

#if 0
static int aw22xxx_lamp_water_on(struct aw22xxx *aw22xxx, unsigned int rgb, unsigned int brigtness)
{
    char RGB_B = rgb & 0xff; /* 0xff */
    char RGB_R = (rgb >> 8) & 0xff; /* 0xff00 */
    char RGB_G = (rgb >> 16) & 0xff; /* 0xff0000 */
    //  unsigned int size;
    pr_info("aw22xxx: RGB_R = %d, RGB_G = %d,RGB_B = %d\n", RGB_R, RGB_G,RGB_B);
#if  0
    size = sizeof(lamp1_on_setting)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp1_on_setting, size);

    /* lamp1_on_setting_rgb */
    aw22xxx_i2c_write(aw22xxx, 0x6, RGB_R);
    aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);
    aw22xxx_i2c_write(aw22xxx, 0x6, RGB_G);
    aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);
    aw22xxx_i2c_write(aw22xxx, 0x6, RGB_B);
    aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);

    size = sizeof(lamp1_on_setting_breath)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp1_on_setting_breath, size);
#endif
    return 0;
}
#endif

#if 0
static int aw22xxx_lamp_fill_on(struct aw22xxx *aw22xxx, unsigned int rgb, unsigned int brigtness)
{
    char RGB_B = rgb & 0xff; /* 0xff */
    char RGB_R = (rgb >> 8) & 0xff; /* 0xff00 */
    char RGB_G = (rgb >> 16) & 0xff; /* 0xff0000 */
    unsigned int size;
    int i;
    pr_info("aw22xxx: RGB_R = %d, RGB_G = %d,RGB_B = %d\n", RGB_R, RGB_G,RGB_B);

    size = sizeof(lamp_on_setting_fill)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp_on_setting_fill, size);

    /* lamp1_on_setting_rgb */
    for(i=0;i<4;i++){
        aw22xxx_i2c_write(aw22xxx, 0x6, RGB_R);
        aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);
        aw22xxx_i2c_write(aw22xxx, 0x6, RGB_G);
        aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);
        aw22xxx_i2c_write(aw22xxx, 0x6, RGB_B);
        aw22xxx_i2c_write(aw22xxx, 0x4, 0x7);
    }
    size = sizeof(lamp_on_fill)/sizeof(struct aw22xxx_reg_confg);
    aw22xxx_i2c_write_config(aw22xxx, lamp_on_fill, size);

    return 0;
}
#endif


static int aw22xxx_reg_page_cfg(struct aw22xxx *aw22xxx, unsigned char page)
{
    aw22xxx_i2c_write(aw22xxx, REG_PAGE, page);
    return 0;
}

static int aw22xxx_sw_reset(struct aw22xxx *aw22xxx)
{
    aw22xxx_i2c_write(aw22xxx, REG_SRST, AW22XXX_SRSTW);
    usleep_range(10000, 15000);
    return 0;
}

static int aw22xxx_chip_enable(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_GCR,
                BIT_GCR_CHIPEN_MASK, BIT_GCR_CHIPEN_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_GCR,
                BIT_GCR_CHIPEN_MASK, BIT_GCR_CHIPEN_DISABLE);
    }
    usleep_range(10000, 15000);
    return 0;
}

/*
   static int aw22xxx_mcu_wakeup(struct aw22xxx *aw22xxx, bool flag)
   {
   if(flag) {
   aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
   BIT_MCUCTR_MCU_WAKEUP_MASK, BIT_MCUCTR_MCU_WAKEUP_ENABLE);
   } else {
   aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
   BIT_MCUCTR_MCU_WAKEUP_MASK, BIT_MCUCTR_MCU_WAKEUP_DISABLE);
   }
   return 0;
   }
 */

static int aw22xxx_mcu_reset(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_RESET_MASK, BIT_MCUCTR_MCU_RESET_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_RESET_MASK, BIT_MCUCTR_MCU_RESET_DISABLE);
    }
    return 0;
}

static int aw22xxx_mcu_enable(struct aw22xxx *aw22xxx, bool flag)
{
    if(flag) {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_WORK_MASK, BIT_MCUCTR_MCU_WORK_ENABLE);
    } else {
        aw22xxx_i2c_write_bits(aw22xxx, REG_MCUCTR,
                BIT_MCUCTR_MCU_WORK_MASK, BIT_MCUCTR_MCU_WORK_DISABLE);
    }
    return 0;
}

static int aw22xxx_led_task0_cfg(struct aw22xxx *aw22xxx, unsigned char task)
{
    aw22xxx_i2c_write(aw22xxx, REG_TASK0, task);
    return 0;
}

static int aw22xxx_led_task1_cfg(struct aw22xxx *aw22xxx, unsigned char task)
{
    aw22xxx_i2c_write(aw22xxx, REG_TASK1, task);
    return 0;
}

/*
   static int aw22xxx_get_pst(struct aw22xxx *aw22xxx, unsigned char *pst)
   {
   int ret = -1;
   ret = aw22xxx_i2c_read(aw22xxx, REG_PST, pst);
   if(ret < 0) {
   pr_err("%s: error=%d\n", __func__, ret);
   }
   return ret;
   }
 */

static int aw22xxx_imax_cfg(struct aw22xxx *aw22xxx, unsigned char imax)
{
    if(imax > 0x0f) {
        imax = 0x0f;
    }
    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
    aw22xxx_i2c_write(aw22xxx, REG_IMAX, imax);

    return 0;
}

/*
   static int aw22xxx_audio_enable(struct aw22xxx *aw22xxx, bool flag)
   {
   if(flag) {
   aw22xxx_i2c_write_bits(aw22xxx, REG_AUDCTR,
   BIT_AUDCTR_AUDEN_MASK, BIT_AUDCTR_AUDEN_ENABLE);
   } else {
   aw22xxx_i2c_write_bits(aw22xxx, REG_AUDCTR,
   BIT_AUDCTR_AUDEN_MASK, BIT_AUDCTR_AUDEN_DISABLE);
   }
   return 0;
   }

   static int aw22xxx_agc_enable(struct aw22xxx *aw22xxx, bool flag)
   {
   if(flag) {
   aw22xxx_i2c_write_bits(aw22xxx, REG_AUDCTR,
   BIT_AUDCTR_AGCEN_MASK, BIT_AUDCTR_AGCEN_ENABLE);
   } else {
   aw22xxx_i2c_write_bits(aw22xxx, REG_AUDCTR,
   BIT_AUDCTR_AGCEN_MASK, BIT_AUDCTR_AGCEN_DISABLE);
   }
   return 0;
   }

   static int aw22xxx_agc_igain_cfg(struct aw22xxx *aw22xxx, unsigned char igain)
   {
   if(igain > 0x3f) {
   igain = 0x3f;
   }
   aw22xxx_i2c_write(aw22xxx, REG_IGAIN, igain);

   return 0;
   }

   static int aw22xxx_get_agc_gain(struct aw22xxx *aw22xxx, unsigned char *gain)
   {
   int ret = -1;
   ret = aw22xxx_i2c_read(aw22xxx, REG_GAIN, gain);
   if(ret < 0) {
   pr_err("%s: error=%d\n", __func__, ret);
   }
   return ret;
   }
 */

static int aw22xxx_dbgctr_cfg(struct aw22xxx *aw22xxx, unsigned char cfg)
{
    if(cfg >= (AW22XXX_DBGCTR_MAX-1)) {
        cfg = AW22XXX_DBGCTR_NORMAL;
    }
    aw22xxx_i2c_write(aw22xxx, REG_DBGCTR, cfg);

    return 0;
}

static int aw22xxx_addr_cfg(struct aw22xxx *aw22xxx, unsigned int addr)
{
    aw22xxx_i2c_write(aw22xxx, REG_ADDR1, (unsigned char)((addr>>0)&0xff));
    aw22xxx_i2c_write(aw22xxx, REG_ADDR2, (unsigned char)((addr>>8)&0xff));

    return 0;
}

static int aw22xxx_data_cfg(struct aw22xxx *aw22xxx, unsigned int data)
{
    aw22xxx_i2c_write(aw22xxx, REG_DATA, data);

    return 0;
}
/*
   static int aw22xxx_get_fw_version(struct aw22xxx *aw22xxx)
   {
   unsigned char reg_val;
   unsigned char i;

   pr_info("%s: enter\n", __func__);

   aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
   aw22xxx_sw_reset(aw22xxx);
   aw22xxx_chip_enable(aw22xxx, true);
   aw22xxx_mcu_enable(aw22xxx, true);
   aw22xxx_led_task0_cfg(aw22xxx, 0xff);
   aw22xxx_mcu_reset(aw22xxx, false);
   msleep(2);
   aw22xxx_mcu_reset(aw22xxx, true);

   aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE2);
   aw22xxx->fw_version = 0;
   for(i=0; i<4; i++) {
   aw22xxx_i2c_read(aw22xxx, 0xe9+i, &reg_val);
   aw22xxx->fw_version |= (reg_val<<((3-i)*8));
   }
   pr_info("%s: flash fw version: 0x%04x\n", __func__, aw22xxx->fw_version);

   aw22xxx_i2c_read(aw22xxx, 0xd0, &reg_val);
   switch(reg_val) {
   case AW22118_CHIPID:
   aw22xxx->chipid= AW22118;
   pr_info("%s: flash chipid: aw22118\n", __func__);
   break;
   case AW22127_CHIPID:
   aw22xxx->chipid= AW22127;
   pr_info("%s: flash chipid: aw22127\n", __func__);
   break;
   default:
   pr_err("%s: unknown id=0x%02x\n", __func__, reg_val);
   }

   aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);

   return 0;
   }
 */

static int aw22xxx_led_display(struct aw22xxx *aw22xxx)
{
    aw22xxx_addr_cfg(aw22xxx, 0x00e1);
    aw22xxx_dbgctr_cfg(aw22xxx, AW22XXX_DBGCTR_SFR);
    aw22xxx_data_cfg(aw22xxx, 0x3d);
    aw22xxx_dbgctr_cfg(aw22xxx, AW22XXX_DBGCTR_NORMAL);
    return 0;
}

static int aw22xxx_led_off(struct aw22xxx *aw22xxx)
{
    aw22xxx_led_task0_cfg(aw22xxx, 0xff);
    aw22xxx_mcu_reset(aw22xxx, true);
    return 0;
}

static void aw22xxx_brightness_work(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx,
            brightness_work);

    //pr_info("%s: enter\n", __func__);

    aw22xxx_led_off(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, false);
    if(aw22xxx->cdev.brightness) {
        aw22xxx_chip_enable(aw22xxx, true);
        aw22xxx_mcu_enable(aw22xxx, true);

        aw22xxx_imax_cfg(aw22xxx, (unsigned char)aw22xxx->imax);
        aw22xxx_led_display(aw22xxx);

        aw22xxx_led_task0_cfg(aw22xxx, 0x82);
        aw22xxx_mcu_reset(aw22xxx, false);
    }
}

static void aw22xxx_set_brightness(struct led_classdev *cdev,
        enum led_brightness brightness)
{
    struct aw22xxx *aw22xxx = container_of(cdev, struct aw22xxx, cdev);

    aw22xxx->cdev.brightness = brightness;

    schedule_work(&aw22xxx->brightness_work);
}

static void aw22xxx_task_work(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx,
            task_work);

    //pr_info("%s: enter\n", __func__);

    aw22xxx_led_off(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, false);
    if(aw22xxx->task0) {
        aw22xxx_chip_enable(aw22xxx, true);
        aw22xxx_mcu_enable(aw22xxx, true);

        aw22xxx_imax_cfg(aw22xxx, (unsigned char)aw22xxx->imax);
        aw22xxx_led_display(aw22xxx);

        aw22xxx_led_task0_cfg(aw22xxx, aw22xxx->task0);
        aw22xxx_led_task1_cfg(aw22xxx, aw22xxx->task1);
        aw22xxx_mcu_reset(aw22xxx, false);
    }
}

static int aw22xxx_led_init(struct aw22xxx *aw22xxx)
{
    //pr_info("%s: enter\n", __func__);

    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_imax_cfg(aw22xxx, aw22xxx_imax_code[aw22xxx->imax]);
    aw22xxx_chip_enable(aw22xxx, false);

    //pr_info("%s: exit\n", __func__);

    return 0;
}

/*****************************************************
 *
 * firmware/cfg update
 *
 *****************************************************/
#if 0
static void aw22xxx_i2c_write_firmware( struct aw22xxx *aw22xxx, int fw_size,
        struct aw22xxx_reg_confg *fw_config)
{
    int i, size;
    struct aw22xxx_reg_confg *config;
    unsigned char page = 0;
    unsigned char reg_addr = 0;
    unsigned char reg_val = 0;

    size = fw_size;
    config = fw_config;

    for(i=0; i<size; i++) {
        if(config[i].reg  == 0xff) {
            page = config[i].data;
        }
        if(aw22xxx->cfg == 1) {
            aw22xxx_i2c_write(aw22xxx, config[i].reg, config[i].data);
            pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, config[i].reg, config[i].data);
        } else {
            if(page == AW22XXX_REG_PAGE1) {
                reg_addr = config[i].reg;
                if((reg_addr<0x2b) && (reg_addr>0x0f)) {
                    reg_addr -= 0x10;
                    reg_val = (unsigned char)(((aw22xxx->rgb[reg_addr/3])>>(8*(2-reg_addr%3)))&0xff);
                    aw22xxx_i2c_write(aw22xxx, config[i].reg, reg_val);
                    pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, config[i].reg, reg_val);
                } else {
                    aw22xxx_i2c_write(aw22xxx, config[i].reg, config[i].data);
                    pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, config[i].reg, config[i].data);
                }
            } else {
                aw22xxx_i2c_write(aw22xxx, config[i].reg, config[i].data);
                pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, config[i].reg, config[i].data);
            }
        }
        if(page == AW22XXX_REG_PAGE0) {
            reg_addr = config[i].reg;
            reg_val = config[i].data;
            /* gcr chip enable delay */
            if((reg_addr == REG_GCR) &&
                    ((reg_val&BIT_GCR_CHIPEN_ENABLE) == BIT_GCR_CHIPEN_ENABLE)) {
                msleep(2);
            }
        }
    }
}
#endif

static int aw22xxx_load_firmware(struct aw22xxx *aw22xxx, struct aw22xxx_reg_data **data)
{
    const struct firmware *cfg_file;
    int ret;

    if(aw22xxx->effect < (sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX)) {
        //pr_info("%s: cfg name=%s\n", __func__, aw22xxx_cfg_name[aw22xxx->effect]);
    } else {
        pr_err("%s: effect 0x%02x over max value \n", __func__, aw22xxx->effect);
        return -1;
    }

    if(aw22xxx->fw_flags != AW22XXX_FLAG_FW_OK) {
        pr_err("%s: fw update error: not compelte \n", __func__);
        return -2;
    }

    /* fw loaded */
    ret = request_firmware(&cfg_file,
            aw22xxx_cfg_name[aw22xxx->effect],
            aw22xxx->dev);
    if (ret < 0) {
        pr_err("%s: failed to read %s\n", __func__,
                aw22xxx_cfg_name[aw22xxx->effect]);
        return 0;
    }
    mutex_lock(&aw22xxx->cfg_lock);
    *data = vmalloc(cfg_file->size + sizeof(int));
    if (!(*data)) {
        release_firmware(cfg_file);
        pr_err("%s: error allocating memory\n", __func__);
        mutex_unlock(&aw22xxx->cfg_lock);
        return 0;
    }

    (*data)->size = (cfg_file->size) / 2;
    //pr_info("%s:  file [%s] size = %d\n", __func__,
    //        aw22xxx_cfg_name[aw22xxx->effect], cfg_file->size);
    memcpy((*data)->data, cfg_file->data, cfg_file->size);

    release_firmware(cfg_file);
    mutex_unlock(&aw22xxx->cfg_lock);
    return 0;
}

static int aw22xxx_write_firmware(struct aw22xxx *aw22xxx, struct aw22xxx_reg_data *data)
{
    unsigned int rgb;
    int i;
    char RGB_B, RGB_R, RGB_G;
    struct aw22xxx_reg_confg *p_data;

    if(aw22xxx->effect < (sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX)) {
        //pr_info("%s: enter\n", __func__);
    } else {
        pr_err("%s: effect 0x%02x over max value \n", __func__, aw22xxx->effect);
        return -1;
    }

    if (!data) {
        pr_err("%s: read file before\n", __func__);
        return -1;
    }

    if(aw22xxx->fw_flags != AW22XXX_FLAG_FW_OK) {
        pr_err("%s: fw update error: not compelte \n", __func__);
        return -2;
    }

    rgb = aw22xxx->aw_effect;
    RGB_R = (rgb >> 16) & 0xff; /* 0xff0000 */
    RGB_G = (rgb >> 8) & 0xff; /* 0xff00 */
    RGB_B = rgb & 0xff; /* 0xff */
    //pr_err("%s: aw_effect:0x%x, RGB_R:0x%x, RGB_G:0x%x, RGB_B:0x%x\n", __func__, aw22xxx->aw_effect, RGB_R, RGB_G, RGB_B);

    mutex_lock(&aw22xxx->cfg_lock);

    p_data = data->data;
    switch (aw22xxx->effect) {
        case 1: /* fill */
            for(i=127; i<406; i+=14) {
                p_data[i].data = RGB_R;
                p_data[i+2].data = RGB_G;
                p_data[i+4].data = RGB_B;
                if (i == 183) i+= 16;
                if (i == 255) i+= 18;
                if (i == 329) i+= 18;
            }
            break;
        case 2: /* quan shui */
            for(i=39; i<62; i+=6) {
                p_data[i].data = RGB_R;
                p_data[i+2].data = RGB_G;
                p_data[i+4].data = RGB_B;
            }

            rgb = aw22xxx->rgb[0];
            RGB_R = (rgb >> 16) & 0xff; /* 0xff0000 */
            RGB_G = (rgb >> 8) & 0xff; /* 0xff00 */
            RGB_B = rgb & 0xff; /* 0xff */
            for(i=63; i<86; i+=6) {
                p_data[i].data = RGB_R;
                p_data[i+2].data = RGB_G;
                p_data[i+4].data = RGB_B;
            }
            break;
        case 3: /* breath */
            //delay rise on fall off
            for(i=97; i<144; i+=14) {
                p_data[i].data = RGB_R;
                p_data[i+2].data = RGB_G;
                p_data[i+4].data = RGB_B;
            }

            rgb = aw22xxx->rgb[0];
            RGB_R = (rgb >> 16) & 0xff; /* 0xff0000 */
            RGB_G = (rgb >> 8) & 0xff; /* 0xff00 */
            RGB_B = rgb & 0xff; /* 0xff */
            for(i=157; i<204; i+=14) {
                p_data[i].data = RGB_R;
                p_data[i+2].data = RGB_G;
                p_data[i+4].data = RGB_B;
            }
            break;
        case 9: /* CQA all_on */
            p_data[53].data = RGB_R;
            p_data[55].data = RGB_G;
            p_data[57].data = RGB_B;
            break;
        default:
            break;
    }

    aw22xxx_i2c_write_config(aw22xxx, data->data, data->size);
    mutex_unlock(&aw22xxx->cfg_lock);
    return 0;
}

#if 0
static int aw22xxx_load_fan_firmware( struct aw22xxx *aw22xxx)
{
    const struct firmware *cfg_file;
    int ret;

    if(aw22xxx->fan_effect < (sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX)) {
        pr_info("%s: cfg name=%s\n", __func__, aw22xxx_cfg_name[aw22xxx->fan_effect]);
    } else {
        pr_err("%s: effect 0x%02x over max value \n", __func__, aw22xxx->fan_effect);
        return -1;
    }

    if(aw22xxx->fw_flags != AW22XXX_FLAG_FW_OK) {
        pr_err("%s: fw update error: not compelte \n", __func__);
        return -2;
    }
    /* fw loaded */
    ret = request_firmware(&cfg_file,
            aw22xxx_fan_cfg_name[aw22xxx->fan_effect],
            aw22xxx->dev);
    if (ret < 0) {
        pr_err("%s: failed to read %s\n", __func__,
                aw22xxx_fan_cfg_name[aw22xxx->fan_effect]);
        return 0;
    }
    mutex_lock(&aw22xxx->cfg_lock);
    if (aw22xxx_data_fan)
        vfree(aw22xxx_data_fan);
    aw22xxx_data_fan = vmalloc(cfg_file->size + sizeof(int));
    if (!aw22xxx_data_fan) {
        release_firmware(cfg_file);
        pr_err("%s: error allocating memory\n", __func__);
        mutex_unlock(&aw22xxx->cfg_lock);
        return 0;
    }

    aw22xxx_data_fan->size = cfg_file->size;
    pr_info("%s:  file [%s] size = %d\n", __func__,
            aw22xxx_fan_cfg_name[aw22xxx->fan_effect], cfg_file->size);
    memcpy(aw22xxx_data_fan->data, cfg_file->data, cfg_file->size);
    release_firmware(cfg_file);
    aw22xxx_i2c_write_firmware(aw22xxx, aw22xxx_data_fan->size, aw22xxx_data_fan->data);
    mutex_unlock(&aw22xxx->cfg_lock);
    return 0;
}
#endif

static void aw22xxx_cfg_loaded(const struct firmware *cont, void *context)
{
    struct aw22xxx *aw22xxx = context;
    int i = 0;
    unsigned char page = 0;
    unsigned char reg_addr = 0;
    unsigned char reg_val = 0;

    pr_info("%s: enter\n", __func__);

    if (!cont) {
        pr_err("%s: failed to read %s\n", __func__, aw22xxx_cfg_name[aw22xxx->effect]);
        release_firmware(cont);
        return;
    }

    pr_info("%s: loaded %s - size: %zu\n", __func__, aw22xxx_cfg_name[aw22xxx->effect],
            cont ? cont->size : 0);
    /*
       for(i=0; i<cont->size; i++) {
       pr_info("%s: addr:0x%04x, data:0x%02x\n", __func__, i, *(cont->data+i));
       }
     */
    for(i=0; i<cont->size; i+=2) {
        if(*(cont->data+i) == 0xff) {
            page = *(cont->data+i+1);
        }
        if(aw22xxx->cfg == 1) {
            aw22xxx_i2c_write(aw22xxx, *(cont->data+i), *(cont->data+i+1));
            pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), *(cont->data+i+1));
        } else {
            if(page == AW22XXX_REG_PAGE1) {
                reg_addr = *(cont->data+i);
                if((reg_addr<0x2b) && (reg_addr>0x0f)) {
                    reg_addr -= 0x10;
                    reg_val = (unsigned char)(((aw22xxx->rgb[reg_addr/3])>>(8*(2-reg_addr%3)))&0xff);
                    aw22xxx_i2c_write(aw22xxx, *(cont->data+i), reg_val);
                    pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), reg_val);
                } else {
                    aw22xxx_i2c_write(aw22xxx, *(cont->data+i), *(cont->data+i+1));
                    pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), *(cont->data+i+1));
                }
            } else {
                aw22xxx_i2c_write(aw22xxx, *(cont->data+i), *(cont->data+i+1));
                pr_debug("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data+i), *(cont->data+i+1));
            }
        }
        if(page == AW22XXX_REG_PAGE0) {
            reg_addr = *(cont->data+i);
            reg_val = *(cont->data+i+1);
            /* gcr chip enable delay */
            if((reg_addr == REG_GCR) &&
                    ((reg_val&BIT_GCR_CHIPEN_ENABLE) == BIT_GCR_CHIPEN_ENABLE)) {
                msleep(2);
            }
        }
    }

    release_firmware(cont);

    pr_info("%s: cfg update complete\n", __func__);
}

static int aw22xxx_cfg_update(struct aw22xxx *aw22xxx)
{
    pr_info("%s: enter\n", __func__);

    if(aw22xxx->effect < (sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX)) {
        pr_info("%s: cfg name=%s\n", __func__, aw22xxx_cfg_name[aw22xxx->effect]);
    } else {
        pr_err("%s: effect 0x%02x over max value \n", __func__, aw22xxx->effect);
        return -1;
    }

    if(aw22xxx->fw_flags != AW22XXX_FLAG_FW_OK) {
        pr_err("%s: fw update error: not compelte \n", __func__);
        return -2;
    }
    //mutex_lock(&aw22xxx->cfg_lock);

    request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
            aw22xxx_cfg_name[aw22xxx->effect], aw22xxx->dev, GFP_KERNEL,
            aw22xxx, aw22xxx_cfg_loaded);
    //mutex_unlock(&aw22xxx->cfg_lock);
    return 0;
}

static int aw22xxx_container_update(struct aw22xxx *aw22xxx,
        struct aw22xxx_container *aw22xxx_fw)
{
    unsigned int i;
    unsigned char reg_val;
    unsigned int tmp_bist;
#ifdef AW22XXX_FLASH_I2C_WRITES
    unsigned int tmp_len;
#endif

    /* chip enable*/
    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_mcu_enable(aw22xxx, true);

    /* flash cfg */
    aw22xxx_i2c_write(aw22xxx, 0x80, 0xec);
    aw22xxx_i2c_write(aw22xxx, 0x35, 0x29);
    //aw22xxx_i2c_write(aw22xxx, 0x37, 0xba);
    aw22xxx_i2c_write(aw22xxx, 0x38, aw22xxx_fw->key);

    /* flash erase*/
    aw22xxx_i2c_write(aw22xxx, 0x22, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x21, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x03);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x03);
    aw22xxx_i2c_write(aw22xxx, 0x23, 0x00);
    msleep(40);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x22, 0x40);
    aw22xxx_i2c_write(aw22xxx, 0x21, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x02);
    aw22xxx_i2c_write(aw22xxx, 0x23, 0x00);
    msleep(6);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x22, 0x42);
    aw22xxx_i2c_write(aw22xxx, 0x21, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x02);
    aw22xxx_i2c_write(aw22xxx, 0x23, 0x00);
    msleep(6);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x22, 0x44);
    aw22xxx_i2c_write(aw22xxx, 0x21, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x02);
    aw22xxx_i2c_write(aw22xxx, 0x23, 0x00);
    msleep(6);
    aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x00);

#ifdef AW22XXX_FLASH_I2C_WRITES
    /* flash writes */
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x03);
    for(i=0; i<aw22xxx_fw->len; i+=tmp_len) {
        aw22xxx_i2c_write(aw22xxx, 0x22, ((i>>8)&0xff));
        aw22xxx_i2c_write(aw22xxx, 0x21, ((i>>0)&0xff));
        aw22xxx_i2c_write(aw22xxx, 0x11, 0x01);
        aw22xxx_i2c_write(aw22xxx, 0x30, 0x04);
        if((aw22xxx_fw->len - i) < MAX_FLASH_WRITE_BYTE_SIZE) {
            tmp_len = aw22xxx_fw->len - i;
        } else {
            tmp_len = MAX_FLASH_WRITE_BYTE_SIZE;
        }
        aw22xxx_i2c_writes(aw22xxx, 0x23, &aw22xxx_fw->data[i], tmp_len);
        aw22xxx_i2c_write(aw22xxx, 0x11, 0x00);
        aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    }
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x00);
#else
    /* flash write */
    //pr_info("aw22xxx: singel write FW\n");
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x03);
    for(i=0; i<aw22xxx_fw->len; i++) {
        aw22xxx_i2c_write(aw22xxx, 0x22, ((i>>8)&0xff));
        aw22xxx_i2c_write(aw22xxx, 0x21, ((i>>0)&0xff));
        aw22xxx_i2c_write(aw22xxx, 0x30, 0x04);
        aw22xxx_i2c_write(aw22xxx, 0x23, aw22xxx_fw->data[i]);
        aw22xxx_i2c_write(aw22xxx, 0x30, 0x00);
    }
    aw22xxx_i2c_write(aw22xxx, 0x20, 0x00);
#endif

    /* bist check */
    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_mcu_enable(aw22xxx, true);
    aw22xxx_i2c_write(aw22xxx, 0x22, (((aw22xxx_fw->len-1)>>8)&0xff));
    aw22xxx_i2c_write(aw22xxx, 0x21, (((aw22xxx_fw->len-1)>>0)&0xff));
    aw22xxx_i2c_write(aw22xxx, 0x24, 0x07);
    msleep(5);
    aw22xxx_i2c_read(aw22xxx, 0x24, &reg_val);
    if(reg_val == 0x05) {
        aw22xxx_i2c_read(aw22xxx, 0x25, &reg_val);
        tmp_bist = reg_val;
        aw22xxx_i2c_read(aw22xxx, 0x26, &reg_val);
        tmp_bist |= (reg_val<<8);
        if(tmp_bist == aw22xxx_fw->bist) {
            //pr_info("%s: bist check pass, bist=0x%04x\n", __func__, aw22xxx_fw->bist);
        } else {
            pr_err("%s: bist check fail, bist=0x%04x\n", __func__, aw22xxx_fw->bist);
            pr_err("%s: fw update failed, please reset phone\n", __func__);
            return -1;
        }
    } else {
        pr_err("%s: bist check is running, reg0x24=0x%02x\n", __func__, reg_val);
    }
    aw22xxx_i2c_write(aw22xxx, 0x24, 0x00);

    return 0;
}

static void aw22xxx_fw_loaded(const struct firmware *cont, void *context)
{
    struct aw22xxx *aw22xxx = context;
    struct aw22xxx_container *aw22xxx_fw;
    int i = 0;
    int ret = -1;
    char tmp_buf[32] = {0};
    unsigned int shift = 0;
    unsigned short check_sum = 0;
    unsigned char reg_val = 0;
    unsigned int tmp_bist = 0;

    //pr_info("%s: enter\n", __func__);

    if (!cont) {
        pr_err("%s: failed to read %s\n", __func__, aw22xxx_fw_name);
        release_firmware(cont);
        return;
    }

    //pr_info("%s: loaded %s - size: %zu\n", __func__, aw22xxx_fw_name,
    //       cont ? cont->size : 0);
    /*
       for(i=0; i<cont->size; i++) {
       pr_info("%s: addr:0x%04x, data:0x%02x\n", __func__, i, *(cont->data+i));
       }
     */
    /* check sum */
    for(i=2; i<cont->size; i++) {
        check_sum += cont->data[i];
    }
    if(check_sum != (unsigned short)((cont->data[0]<<8)|(cont->data[1]))) {
        pr_err("%s: check sum err: check_sum=0x%04x\n", __func__, check_sum);
        release_firmware(cont);
        return;
    } else {
        //pr_info("%s: check sum pass : 0x%04x\n", __func__, check_sum);
    }

    /* get fw info */
    aw22xxx_fw = kzalloc(cont->size + 4*sizeof(unsigned int), GFP_KERNEL);
    if (!aw22xxx_fw) {
        release_firmware(cont);
        pr_err("%s: Error allocating memory\n", __func__);
        return;
    }
    shift += 2;

    //pr_info("%s: fw chip_id : 0x%02x\n", __func__, cont->data[0+shift]);
    shift += 1;

    memcpy(tmp_buf, &cont->data[0+shift], 16);
    //pr_info("%s: fw customer: %s\n", __func__, tmp_buf);
    shift += 16;

    memcpy(tmp_buf, &cont->data[0+shift], 8);
    //pr_info("%s: fw project: %s\n", __func__, tmp_buf);
    shift += 8;

    aw22xxx_fw->version = (cont->data[0+shift]<<24) | (cont->data[1+shift]<<16) |
        (cont->data[2+shift]<<8) | (cont->data[3+shift]<<0);
    //pr_info("%s: fw version : 0x%04x\n", __func__, aw22xxx_fw->version);
    shift += 4;

    // reserved
    shift += 3;

    aw22xxx_fw->bist = (cont->data[0+shift]<<8) | (cont->data[1+shift]<<0);
    //pr_info("%s: fw bist : 0x%04x\n", __func__, aw22xxx_fw->bist);
    shift += 2;

    aw22xxx_fw->key = cont->data[0+shift];
    //pr_info("%s: fw key : 0x%04x\n", __func__, aw22xxx_fw->key);
    shift += 1;

    // reserved
    shift += 1;

    aw22xxx_fw->len = (cont->data[0+shift]<<8) | (cont->data[1+shift]<<0);
    //pr_info("%s: fw len : 0x%04x\n", __func__, aw22xxx_fw->len);
    shift += 2;

    memcpy(aw22xxx_fw->data, &cont->data[shift], aw22xxx_fw->len);
    release_firmware(cont);

    /* check version */
    //aw22xxx_get_fw_version(aw22xxx);

    /* bist check */
    aw22xxx_sw_reset(aw22xxx);
    aw22xxx_chip_enable(aw22xxx, true);
    aw22xxx_mcu_enable(aw22xxx, true);
    aw22xxx_i2c_write(aw22xxx, 0x22, (((aw22xxx_fw->len-1)>>8)&0xff));
    aw22xxx_i2c_write(aw22xxx, 0x21, (((aw22xxx_fw->len-1)>>0)&0xff));
    aw22xxx_i2c_write(aw22xxx, 0x24, 0x07);
    msleep(5);
    aw22xxx_i2c_read(aw22xxx, 0x24, &reg_val);
    if(reg_val == 0x05) {
        aw22xxx_i2c_read(aw22xxx, 0x25, &reg_val);
        tmp_bist = reg_val;
        aw22xxx_i2c_read(aw22xxx, 0x26, &reg_val);
        tmp_bist |= (reg_val<<8);
        if(tmp_bist == aw22xxx_fw->bist) {
            //pr_info("%s: bist check pass, bist=0x%04x\n",
            //     __func__, aw22xxx_fw->bist);
            if(aw22xxx->fw_update == 0) {
                kfree(aw22xxx_fw);
                aw22xxx_i2c_write(aw22xxx, 0x24, 0x00);
                aw22xxx_led_init(aw22xxx);
                aw22xxx->fw_flags = AW22XXX_FLAG_FW_OK;
                return;
            } else {
                //pr_info("%s: fw version: 0x%04x, force update fw\n",
                //      __func__, aw22xxx_fw->version);
            }
        } else {
            //pr_info("%s: bist check fail, fw bist=0x%04x, flash bist=0x%04x\n",
            //      __func__, aw22xxx_fw->bist, tmp_bist);
            //pr_info("%s: find new fw: 0x%04x, need update\n",
            //      __func__, aw22xxx_fw->version);
        }
    } else {
        pr_err("%s: bist check is running, reg0x24=0x%02x\n", __func__, reg_val);
        //pr_info("%s: fw need update\n", __func__);
    }
    aw22xxx_i2c_write(aw22xxx, 0x24, 0x00);

    /* fw update */
    ret = aw22xxx_container_update(aw22xxx, aw22xxx_fw);
    if(ret) {
        aw22xxx->fw_flags = AW22XXX_FLAG_FW_FAIL;
    } else {
        aw22xxx->fw_flags = AW22XXX_FLAG_FW_OK;
    }
    kfree(aw22xxx_fw);

    aw22xxx->fw_update = 0;

    aw22xxx_led_init(aw22xxx);

    //pr_info("%s: exit\n", __func__);
}

static int aw22xxx_fw_update(struct aw22xxx *aw22xxx)
{
    //pr_info("%s: enter\n", __func__);

    aw22xxx->fw_flags = AW22XXX_FLAG_FW_UPDATE;

    return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
            aw22xxx_fw_name, aw22xxx->dev, GFP_KERNEL,
            aw22xxx, aw22xxx_fw_loaded);
}

#ifdef AWINIC_FW_UPDATE_DELAY
static enum hrtimer_restart aw22xxx_fw_timer_func(struct hrtimer *timer)
{
    struct aw22xxx *aw22xxx = container_of(timer, struct aw22xxx, fw_timer);

    //pr_info("%s: enter\n", __func__);

    schedule_work(&aw22xxx->fw_work);

    return HRTIMER_NORESTART;
}
#endif

static void aw22xxx_fw_work_routine(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx, fw_work);

    //pr_info("%s: enter\n", __func__);

    aw22xxx_fw_update(aw22xxx);

}

static void aw22xxx_cfg_work_routine(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx, cfg_work);

    //pr_info("%s: enter\n", __func__);

    aw22xxx_cfg_update(aw22xxx);

}

static int aw22xxx_chip_init(struct aw22xxx *aw22xxx)
{
    static bool init_once = true;

    msleep(2);
    if (!init_once)
        return 0;
    else
        init_once = false;

    mutex_lock(&aw22xxx->cfg_lock);
    aw22xxx_i2c_chip_init(aw22xxx);
    mutex_unlock(&aw22xxx->cfg_lock);
    return 0;
}

/**
   *    动态效果序号：
   *        0: 填充
   *        1: 泉水
   *        2: 呼吸
   *        3: 旋转
   *        4: 颜色轮播
   *        5: null
   *        6: null
   *        7: 音乐
   */
static int aw22xxx_aw_brightness_update(struct aw22xxx *aw22xxx)
{
    //unsigned char mode = (aw22xxx->aw_effect >> 8) & 0xff;
    unsigned char mode_select = (aw22xxx->aw_effect >> 28) & 0x0f;
    unsigned int size;
    char RGB, rgb_l;

    //pr_info("%s: enter, mode_select = 0x%x, aw22xxx->aw_effect:0x%x\n", __func__, mode_select, aw22xxx->aw_effect);
    aw22xxx_chip_init(aw22xxx);

    switch (mode_select) {
        case 0:
            if (aw22xxx->bn == 0) { /* LEDS off */
                mutex_lock(&aw22xxx->cfg_lock);
                size = sizeof(lamp1_all_off)/sizeof(struct aw22xxx_reg_confg);
                aw22xxx_i2c_write_config(aw22xxx, lamp1_all_off, size);
                mutex_unlock(&aw22xxx->cfg_lock);
            }else { /* CQA test */
                RGB = (aw22xxx->aw_effect >> 4) & 0xf; /* 0xf0 */
                if (RGB == 0x7) { //all on
                    rgb_l = (aw22xxx->aw_effect) & 0xf; /* 0xf */
                    if (rgb_l == AW22XXX_RGB_R) {
                        aw22xxx->aw_effect = 0xFF0000;
                    }else if (rgb_l == AW22XXX_RGB_G) {
                        aw22xxx->aw_effect = 0x00FF00;
                    }else if (rgb_l == AW22XXX_RGB_B) {
                        aw22xxx->aw_effect = 0x0000FF;
                    }
                    aw22xxx->effect = 9;
                    if (!aw22xxx_data_cqa)
                        aw22xxx_load_firmware(aw22xxx, &aw22xxx_data_cqa);
                    aw22xxx_write_firmware(aw22xxx, aw22xxx_data_cqa);
                } else {
                    mutex_lock(&aw22xxx->cfg_lock);
                    aw22xxx_lamp1_on(aw22xxx, aw22xxx->aw_effect, aw22xxx->bn);
                    mutex_unlock(&aw22xxx->cfg_lock);
                }
            }
            break;
        case 1: /* fill */
            //mutex_lock(&aw22xxx->cfg_lock);
            //aw22xxx_lamp_fill_on(aw22xxx, aw22xxx->aw_effect, aw22xxx->bn);
            //mutex_unlock(&aw22xxx->cfg_lock);
#if 1
            aw22xxx->effect = mode_select;
            if (!aw22xxx_data_fill)
                aw22xxx_load_firmware(aw22xxx, &aw22xxx_data_fill);
            aw22xxx_write_firmware(aw22xxx, aw22xxx_data_fill);
#else
            aw22xxx->effect = 9;
            if (!aw22xxx_data_cqa)
                aw22xxx_load_firmware(aw22xxx, &aw22xxx_data_cqa);
            aw22xxx_write_firmware(aw22xxx, aw22xxx_data_cqa);
#endif
            break;
        case 2: /* quan shui */
            aw22xxx->effect = mode_select;
            if (!aw22xxx_data_warter)
                aw22xxx_load_firmware(aw22xxx, &aw22xxx_data_warter);
            aw22xxx_write_firmware(aw22xxx, aw22xxx_data_warter);
            break;
        case 3: /* breath */
            aw22xxx->effect = mode_select;
            if (!aw22xxx_data_breath)
                aw22xxx_load_firmware(aw22xxx, &aw22xxx_data_breath);
            aw22xxx_write_firmware(aw22xxx, aw22xxx_data_breath);
            //mutex_lock(&aw22xxx->cfg_lock);
            //aw22xxx_lamp_breath_on(aw22xxx, aw22xxx->aw_effect, aw22xxx->bn);
            //mutex_unlock(&aw22xxx->cfg_lock);
            break;
        case 4: /* xuan zhuan */
            mutex_lock(&aw22xxx->cfg_lock);
            size = sizeof(lamp1_spin)/sizeof(struct aw22xxx_reg_confg);
            aw22xxx_i2c_write_config(aw22xxx, lamp1_spin, size);
            mutex_unlock(&aw22xxx->cfg_lock);
            break;
        case 5: /* lun bo */
            mutex_lock(&aw22xxx->cfg_lock);
            //size = sizeof(roration_init)/sizeof(struct aw22xxx_reg_confg);
            //aw22xxx_i2c_write_config(aw22xxx, roration_init, size);
            size = sizeof(lamp1_roration)/sizeof(struct aw22xxx_reg_confg);
            aw22xxx_i2c_write_config(aw22xxx, lamp1_roration, size);
            mutex_unlock(&aw22xxx->cfg_lock);
            break;
        case 8: /* music */
            mutex_lock(&aw22xxx->cfg_lock);
            size = sizeof(lamp1_music)/sizeof(struct aw22xxx_reg_confg);
            aw22xxx_i2c_write_config(aw22xxx, lamp1_music, size);
            mutex_unlock(&aw22xxx->cfg_lock);
            break;
        default:
            pr_info("%s: unsupport mode %d\n", __func__, mode_select);
            break;
    }

    return 0;
}

static void aw22xxx_aw_brightness_work_routine(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx, aw_brightness_work);

    //pr_info("%s: enter\n", __func__);
    aw22xxx_aw_brightness_update(aw22xxx);
}

static int aw22xxx_fan_brightness_update(struct aw22xxx *aw22xxx)
{
    //unsigned char mode = (aw22xxx->fan_effect >> 8) & 0xff;
    unsigned char mode_select = (aw22xxx->fan_effect >> 28) & 0x0f;
    unsigned int size, rgb;

    //pr_info("%s: enter, fan_effect = 0x%x, mode_select = 0x%x\n", __func__, aw22xxx->fan_effect, mode_select);
    aw22xxx_chip_init(aw22xxx);

    switch (mode_select) {
        case 0:
            if (aw22xxx->bn == 0) { /* LEDS off */
                mutex_lock(&aw22xxx->cfg_lock);
                size = sizeof(lamp2_all_off)/sizeof(struct aw22xxx_reg_confg);
                aw22xxx_i2c_write_config(aw22xxx, lamp2_all_off, size);
                mutex_unlock(&aw22xxx->cfg_lock);
            }else { /* CQA test */
                rgb = (aw22xxx->fan_effect >> 4) & 0xf; /* 0xf0 */
                mutex_lock(&aw22xxx->cfg_lock);
#if 1 //only for power analysis
                if (rgb == 0xa) {
                    rgb = (aw22xxx->fan_effect) & 0xf; /* 0xf */
                    aw22xxx_i2c_write(aw22xxx, 0x0b, rgb);
                } else {
#endif
                aw22xxx_lamp2_on(aw22xxx, aw22xxx->fan_effect, aw22xxx->bn);
                }
                mutex_unlock(&aw22xxx->cfg_lock);
            }
            break;
        case 3: /* breath */
            rgb = (aw22xxx->fan_effect) & 0x00ffffff;
            mutex_lock(&aw22xxx->cfg_lock);
            size = sizeof(lamp2_breath_red)/sizeof(struct aw22xxx_reg_confg);
            if (rgb == 0x00ff0000) {
                aw22xxx_i2c_write_config(aw22xxx, lamp2_breath_red, size);
            } else {
                aw22xxx_i2c_write_config(aw22xxx, lamp2_breath_blue, size);
            }
            mutex_unlock(&aw22xxx->cfg_lock);
            break;
        case 5: /* lun bo */
            mutex_lock(&aw22xxx->cfg_lock);
            //size = sizeof(roration_init)/sizeof(struct aw22xxx_reg_confg);
            //aw22xxx_i2c_write_config(aw22xxx, roration_init, size);
            size = sizeof(lamp2_roration)/sizeof(struct aw22xxx_reg_confg);
            aw22xxx_i2c_write_config(aw22xxx, lamp2_roration, size);
            mutex_unlock(&aw22xxx->cfg_lock);
            break;
        default:
            pr_info("%s: unsupport mode %d\n", __func__, mode_select);
            break;
    }
    return 0;
}

static void aw22xxx_fan_brightness_work_routine(struct work_struct *work)
{
    struct aw22xxx *aw22xxx = container_of(work, struct aw22xxx, fan_brightness_work);

    //pr_info("%s: enter\n", __func__);
    aw22xxx_fan_brightness_update(aw22xxx);
}

static int aw22xxx_fw_init(struct aw22xxx *aw22xxx)
{
#ifdef AWINIC_FW_UPDATE_DELAY
    int fw_timer_val = 10000;

    hrtimer_init(&aw22xxx->fw_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    aw22xxx->fw_timer.function = aw22xxx_fw_timer_func;
    INIT_WORK(&aw22xxx->fw_work, aw22xxx_fw_work_routine);
    INIT_WORK(&aw22xxx->cfg_work, aw22xxx_cfg_work_routine);
    INIT_WORK(&aw22xxx->aw_brightness_work, aw22xxx_aw_brightness_work_routine);
    INIT_WORK(&aw22xxx->fan_brightness_work, aw22xxx_fan_brightness_work_routine);
    hrtimer_start(&aw22xxx->fw_timer,
            ktime_set(fw_timer_val/1000, (fw_timer_val%1000)*1000000),
            HRTIMER_MODE_REL);
#else
    INIT_WORK(&aw22xxx->fw_work, aw22xxx_fw_work_routine);
    INIT_WORK(&aw22xxx->cfg_work, aw22xxx_cfg_work_routine);
    schedule_work(&aw22xxx->fw_work);
#endif
    return 0;
}


/******************************************************
 *
 * irq
 *
 ******************************************************/
static void aw22xxx_interrupt_clear(struct aw22xxx *aw22xxx)
{
    unsigned char reg_val;

    //pr_info("%s: enter\n", __func__);

    aw22xxx_i2c_read(aw22xxx, REG_INTST, &reg_val);
    //pr_info("%s: reg INTST=0x%x\n", __func__, reg_val);
}

static void aw22xxx_interrupt_setup(struct aw22xxx *aw22xxx)
{
    //pr_info("%s: enter\n", __func__);

    aw22xxx_interrupt_clear(aw22xxx);

    aw22xxx_i2c_write_bits(aw22xxx, REG_INTEN,
            BIT_INTEN_FUNCMPE_MASK, BIT_INTEN_FUNCMPE_ENABLE);
}

static irqreturn_t aw22xxx_irq(int irq, void *data)
{
    struct aw22xxx *aw22xxx = data;
    unsigned char reg_val;

    //pr_info("%s: enter\n", __func__);

    aw22xxx_i2c_read(aw22xxx, REG_INTST, &reg_val);
    //pr_info("%s: reg INTST=0x%x\n", __func__, reg_val);

    if(reg_val & BIT_INTST_FUNCMPE) {
        //pr_info("%s: functions compelte!\n", __func__);
        aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
        aw22xxx_mcu_reset(aw22xxx, true);
        aw22xxx_mcu_enable(aw22xxx, false);
        aw22xxx_chip_enable(aw22xxx, false);
        //pr_info("%s: enter standby mode!\n", __func__);
    }
    //pr_info("%s exit\n", __func__);

    return IRQ_HANDLED;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw22xxx_parse_dt(struct device *dev, struct aw22xxx *aw22xxx,
        struct device_node *np)
{
    aw22xxx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
    if (aw22xxx->reset_gpio < 0) {
        dev_err(dev, "%s: no reset gpio provided, will not HW reset device\n", __func__);
        return -1;
    } else {
        //dev_info(dev, "%s: reset gpio provided ok\n", __func__);
    }
    aw22xxx->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
    if (aw22xxx->irq_gpio < 0) {
        dev_err(dev, "%s: no irq gpio provided, will not suppport intterupt\n", __func__);
        return -1;
    } else {
        //dev_info(dev, "%s: irq gpio provided ok\n", __func__);
    }

    return 0;
}

static int aw22xxx_hw_reset(struct aw22xxx *aw22xxx)
{
    //pr_info("%s: enter\n", __func__);

    if (aw22xxx && gpio_is_valid(aw22xxx->reset_gpio)) {
        gpio_set_value_cansleep(aw22xxx->reset_gpio, 0);
        msleep(1);
        gpio_set_value_cansleep(aw22xxx->reset_gpio, 1);
        msleep(1);
    } else {
        dev_err(aw22xxx->dev, "%s:  failed\n", __func__);
    }
    return 0;
}

static int aw22xxx_hw_off(struct aw22xxx *aw22xxx)
{
    pr_info("%s: enter\n", __func__);

    if (aw22xxx && gpio_is_valid(aw22xxx->reset_gpio)) {
        gpio_set_value_cansleep(aw22xxx->reset_gpio, 0);
        msleep(1);
    } else {
        dev_err(aw22xxx->dev, "%s:  failed\n", __func__);
    }
    return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw22xxx_read_chipid(struct aw22xxx *aw22xxx)
{
    int ret = -1;
    unsigned char cnt = 0;
    unsigned char reg_val = 0;

    aw22xxx_reg_page_cfg(aw22xxx, AW22XXX_REG_PAGE0);
    aw22xxx_sw_reset(aw22xxx);

    while(cnt < AW_READ_CHIPID_RETRIES) {
        ret = aw22xxx_i2c_read(aw22xxx, REG_SRST, &reg_val);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                    "%s: failed to read register AW22XXX_REG_ID: %d\n",
                    __func__, ret);
            return -EIO;
        }
        switch (reg_val) {
            case AW22XXX_SRSTR:
                //pr_info("%s aw22xxx detected\n", __func__);
                //aw22xxx->flags |= AW22XXX_FLAG_SKIP_INTERRUPTS;
                aw22xxx_i2c_read(aw22xxx, REG_CHIPID, &reg_val);
                switch(reg_val) {
                    case AW22118_CHIPID:
                        aw22xxx->chipid= AW22118;
                        //pr_info("%s: chipid: aw22118\n", __func__);
                        break;
                    case AW22127_CHIPID:
                        aw22xxx->chipid= AW22127;
                        //pr_info("%s: chipid: aw22127\n", __func__);
                        break;
                    default:
                        pr_err("%s: unknown id=0x%02x\n", __func__, reg_val);
                        break;
                }
                return 0;
            default:
                pr_info("%s unsupported device revision (0x%x)\n",
                        __func__, reg_val );
                break;
        }
        cnt ++;

        msleep(AW_READ_CHIPID_RETRY_DELAY);
    }

    return -EINVAL;
}


/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw22xxx_reg_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[2] = {0, 0};

    if(2 == sscanf(buf, "%x %x", &databuf[0], &databuf[1])) {
        aw22xxx_i2c_write(aw22xxx, databuf[0], databuf[1]);
    }

    return count;
}

static ssize_t aw22xxx_reg_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
    ssize_t len = 0;
    unsigned int i = 0;
    unsigned char reg_val = 0;
    unsigned char reg_page = 0;
    aw22xxx_i2c_read(aw22xxx, REG_PAGE, &reg_page);
    for(i = 0; i < AW22XXX_REG_MAX; i ++) {
        if(!reg_page) {
            if(!(aw22xxx_reg_access[i]&REG_RD_ACCESS))
                continue;
        }
        aw22xxx_i2c_read(aw22xxx, i, &reg_val);
        len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x \n", i, reg_val);
    }
    return len;
}

static ssize_t aw22xxx_hwen_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        if(1 == databuf[0]) {
            aw22xxx_hw_reset(aw22xxx);
        } else {
            aw22xxx_hw_off(aw22xxx);
        }
    }

    return count;
}

static ssize_t aw22xxx_hwen_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "hwen=%d\n",
            gpio_get_value(aw22xxx->reset_gpio));

    return len;
}

static ssize_t aw22xxx_fw_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%x", &databuf[0])) {
        aw22xxx->fw_update = databuf[0];
        if(1 == databuf[0]) {
            schedule_work(&aw22xxx->fw_work);
        }
    }

    return count;
}

static ssize_t aw22xxx_fw_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    ssize_t len = 0;

    len += snprintf(buf+len, PAGE_SIZE-len, "firmware name = %s\n", aw22xxx_fw_name);

    return len;
}

static ssize_t aw22xxx_aw_brightness_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
    unsigned int databuf[3] = {0, 0, 0};

    flush_work(&aw22xxx->aw_brightness_work);

    if(sscanf(buf, "%x %x %x", &databuf[1], &databuf[0], &databuf[2])) {
        aw22xxx->bn = databuf[0];
        aw22xxx->aw_effect = databuf[1];
        aw22xxx->rgb[0] = databuf[2];
        //pr_info("%s: bn = %d, aw_effect = 0x%08x, rgb[0] = 0x%08x\n", __func__, aw22xxx->bn, aw22xxx->aw_effect, aw22xxx->rgb[0]);
        schedule_work(&aw22xxx->aw_brightness_work);
    }

    return count;
}

static ssize_t aw22xxx_aw_brightness_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    for(i=0; i<sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "cfg[%x] = %s\n", i, aw22xxx_cfg_name[i]);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current cfg = %s\n", aw22xxx_cfg_name[aw22xxx->effect]);

    return len;
}

static ssize_t aw22xxx_fan_brightness_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[2] = {0, 0};

    if(sscanf(buf, "%x %x", &databuf[1], &databuf[0])) {
        aw22xxx->bn = databuf[0];
        aw22xxx->fan_effect = databuf[1];
        //pr_info("%s: bn = %d, fan effect = %x\n",__func__, aw22xxx->bn, aw22xxx->fan_effect);
        schedule_work(&aw22xxx->fan_brightness_work);
    }

    return count;
}

static ssize_t aw22xxx_fan_brightness_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    for(i=0; i<sizeof(aw22xxx_fan_cfg_name)/AW22XXX_CFG_NAME_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "cfg[%x] = %s\n", i, aw22xxx_fan_cfg_name[i]);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current fan cfg = %s\n", aw22xxx_fan_cfg_name[aw22xxx->fan_effect]);

    return len;
}

static ssize_t aw22xxx_cfg_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    unsigned int databuf[1] = {0};

    if(1 == sscanf(buf, "%d", &databuf[0])) {
        aw22xxx->cfg = databuf[0];
        if(aw22xxx->cfg) {
            schedule_work(&aw22xxx->cfg_work);
        }
    }

    return count;
}

static ssize_t aw22xxx_cfg_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    for(i=0; i<sizeof(aw22xxx_cfg_name)/AW22XXX_CFG_NAME_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "cfg[%x] = %s\n", i, aw22xxx_cfg_name[i]);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current cfg = %s\n", aw22xxx_cfg_name[aw22xxx->effect]);

    return len;
}

static ssize_t aw22xxx_effect_store(struct device* dev, struct device_attribute *attr,
        const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%d",&databuf[0]);
    aw22xxx->effect = databuf[0];

    return len;
}

static ssize_t aw22xxx_effect_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);
    aw22xxx_effect_1_test(aw22xxx);
    len += snprintf(buf+len, PAGE_SIZE-len, "effect = 0x%02x\n", aw22xxx->effect);

    return len;
}

static ssize_t aw22xxx_imax_store(struct device* dev, struct device_attribute *attr,
        const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw22xxx->imax = databuf[0];
    aw22xxx_imax_cfg(aw22xxx, aw22xxx_imax_code[aw22xxx->imax]);

    return len;
}

static ssize_t aw22xxx_imax_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    for(i=0; i<sizeof(aw22xxx_imax_name)/AW22XXX_IMAX_NAME_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "imax[%x] = %s\n", i, aw22xxx_imax_name[i]);
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "current id = 0x%02x, imax = %s\n",
            aw22xxx->imax, aw22xxx_imax_name[aw22xxx->imax]);

    return len;
}

static ssize_t aw22xxx_rgb_store(struct device* dev, struct device_attribute *attr,
        const char* buf, size_t len)
{
    unsigned int databuf[2];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%x %x",&databuf[0], &databuf[1]);
    aw22xxx->rgb[databuf[0]] = databuf[1];

    return len;
}

static ssize_t aw22xxx_rgb_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    unsigned int i;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    for(i=0; i<AW22XXX_RGB_MAX; i++) {
        len += snprintf(buf+len, PAGE_SIZE-len, "rgb[%d] = 0x%06x\n", i, aw22xxx->rgb[i]);
    }
    return len;
}

static ssize_t aw22xxx_task0_store(struct device* dev, struct device_attribute *attr,
        const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw22xxx->task0 = databuf[0];
    schedule_work(&aw22xxx->task_work);

    return len;
}

static ssize_t aw22xxx_task0_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    len += snprintf(buf+len, PAGE_SIZE-len, "task0 = 0x%02x\n", aw22xxx->task0);

    return len;
}

static ssize_t aw22xxx_task1_store(struct device* dev, struct device_attribute *attr,
        const char* buf, size_t len)
{
    unsigned int databuf[1];
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    sscanf(buf,"%x",&databuf[0]);
    aw22xxx->task1 = databuf[0];

    return len;
}

static ssize_t aw22xxx_task1_show(struct device* dev,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

    len += snprintf(buf+len, PAGE_SIZE-len, "task1 = 0x%02x\n", aw22xxx->task1);

    return len;
}

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw22xxx_reg_show, aw22xxx_reg_store);
static DEVICE_ATTR(hwen, S_IWUSR | S_IRUGO, aw22xxx_hwen_show, aw22xxx_hwen_store);
static DEVICE_ATTR(fw, S_IWUSR | S_IRUGO, aw22xxx_fw_show, aw22xxx_fw_store);
static DEVICE_ATTR(cfg, S_IWUSR | S_IRUGO, aw22xxx_cfg_show, aw22xxx_cfg_store);
static DEVICE_ATTR(effect, S_IWUSR | S_IRUGO, aw22xxx_effect_show, aw22xxx_effect_store);
static DEVICE_ATTR(imax, S_IWUSR | S_IRUGO, aw22xxx_imax_show, aw22xxx_imax_store);
static DEVICE_ATTR(rgb, S_IWUSR | S_IRUGO, aw22xxx_rgb_show, aw22xxx_rgb_store);
static DEVICE_ATTR(task0, S_IWUSR | S_IRUGO, aw22xxx_task0_show, aw22xxx_task0_store);
static DEVICE_ATTR(task1, S_IWUSR | S_IRUGO, aw22xxx_task1_show, aw22xxx_task1_store);
static DEVICE_ATTR(aw_brightness, S_IWUSR | S_IRUGO, aw22xxx_aw_brightness_show, aw22xxx_aw_brightness_store);
static DEVICE_ATTR(fan_brightness, S_IWUSR | S_IRUGO, aw22xxx_fan_brightness_show, aw22xxx_fan_brightness_store);

static struct attribute *aw22xxx_attributes[] = {
    &dev_attr_reg.attr,
    &dev_attr_hwen.attr,
    &dev_attr_fw.attr,
    &dev_attr_cfg.attr,
    &dev_attr_effect.attr,
    &dev_attr_imax.attr,
    &dev_attr_rgb.attr,
    &dev_attr_task0.attr,
    &dev_attr_task1.attr,
    &dev_attr_aw_brightness.attr,
    &dev_attr_fan_brightness.attr,
    NULL
};

static struct attribute_group aw22xxx_attribute_group = {
    .attrs = aw22xxx_attributes
};


/******************************************************
 *
 * led class dev
 *
 ******************************************************/
static int aw22xxx_parse_led_cdev(struct aw22xxx *aw22xxx,
        struct device_node *np)
{
    struct device_node *temp;
    int ret = -1;

    //pr_info("%s: enter\n", __func__);

    for_each_child_of_node(np, temp) {
        ret = of_property_read_string(temp, "aw22xxx,name",
                &aw22xxx->cdev.name);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                    "Failure reading led name, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw22xxx,imax",
                &aw22xxx->imax);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                    "Failure reading imax, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw22xxx,brightness",
                &aw22xxx->cdev.brightness);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                    "Failure reading brightness, ret = %d\n", ret);
            goto free_pdata;
        }
        ret = of_property_read_u32(temp, "aw22xxx,max_brightness",
                &aw22xxx->cdev.max_brightness);
        if (ret < 0) {
            dev_err(aw22xxx->dev,
                    "Failure reading max brightness, ret = %d\n", ret);
            goto free_pdata;
        }
    }

    INIT_WORK(&aw22xxx->brightness_work, aw22xxx_brightness_work);
    INIT_WORK(&aw22xxx->task_work, aw22xxx_task_work);

    aw22xxx->cdev.brightness_set = aw22xxx_set_brightness;
    ret = led_classdev_register(aw22xxx->dev, &aw22xxx->cdev);
    if (ret) {
        dev_err(aw22xxx->dev,
                "unable to register led ret=%d\n", ret);
        goto free_pdata;
    }

    ret = sysfs_create_group(&aw22xxx->cdev.dev->kobj,
            &aw22xxx_attribute_group);
    if (ret) {
        dev_err(aw22xxx->dev, "led sysfs ret: %d\n", ret);
        goto free_class;
    }

    return 0;

free_class:
    led_classdev_unregister(&aw22xxx->cdev);
free_pdata:
    return ret;
}

#define RESET_ACTIVE    "pmx_aw22xxx_active"

static int aw22xxx_i2c_pinctrl_init(struct aw22xxx *aw22xxx)
{
    int retval;

    /* Get pinctrl if target uses pinctrl */
    aw22xxx->active_pinctrl = devm_pinctrl_get(&aw22xxx->i2c->dev);
    if (IS_ERR_OR_NULL(aw22xxx->active_pinctrl)) {
        retval = PTR_ERR(aw22xxx->active_pinctrl);
        pr_err("aw22xxx does not use pinctrl %d\n", retval);
        goto err_pinctrl_get;
    }

    aw22xxx->pinctrl_reset_state
        = pinctrl_lookup_state(aw22xxx->active_pinctrl, RESET_ACTIVE);
    if (IS_ERR_OR_NULL(aw22xxx->pinctrl_reset_state)) {
        retval = PTR_ERR(aw22xxx->pinctrl_reset_state);
        pr_err("Can not lookup %s pinstate %d\n",
                RESET_ACTIVE, retval);
        goto err_pinctrl_lookup;
    }

    if (aw22xxx->active_pinctrl) {
        /*
         * Pinctrl handle is optional. If pinctrl handle is found
         * let pins to be configured in active state. If not
         * found continue further without error.
         */
        retval = pinctrl_select_state(aw22xxx->active_pinctrl,
                aw22xxx->pinctrl_reset_state);
        if (retval < 0) {
            pr_err("%s: Failed to select %s pinstate %d\n",
                    __func__, RESET_ACTIVE, retval);
        }
    }

    return 0;

err_pinctrl_lookup:
    devm_pinctrl_put(aw22xxx->active_pinctrl);
err_pinctrl_get:
    aw22xxx->active_pinctrl = NULL;
    return retval;
}
/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw22xxx_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
    struct aw22xxx *aw22xxx;
    struct device_node *np = i2c->dev.of_node;
    int ret;
    int irq_flags;

    //pr_info("%s: enter\n", __func__);

    if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
        dev_err(&i2c->dev, "check_functionality failed\n");
        return -EIO;
    }

    aw22xxx = devm_kzalloc(&i2c->dev, sizeof(struct aw22xxx), GFP_KERNEL);
    if (aw22xxx == NULL)
        return -ENOMEM;

    aw22xxx->dev = &i2c->dev;
    aw22xxx->i2c = i2c;

    i2c_set_clientdata(i2c, aw22xxx);

    mutex_init(&aw22xxx->cfg_lock);

    /* aw22xxx rst & int */
    if (np) {
        ret = aw22xxx_parse_dt(&i2c->dev, aw22xxx, np);
        if (ret) {
            dev_err(&i2c->dev, "%s: failed to parse device tree node\n", __func__);
            goto err_parse_dt;
        }
    } else {
        aw22xxx->reset_gpio = -1;
        aw22xxx->irq_gpio = -1;
    }

    aw22xxx_i2c_pinctrl_init(aw22xxx);
    if (gpio_is_valid(aw22xxx->reset_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw22xxx->reset_gpio,
                GPIOF_OUT_INIT_LOW, "aw22xxx_rst");
        if (ret){
            dev_err(&i2c->dev, "%s: rst request failed\n", __func__);
            goto err_gpio_request;
        }
    }

    if (gpio_is_valid(aw22xxx->irq_gpio)) {
        ret = devm_gpio_request_one(&i2c->dev, aw22xxx->irq_gpio,
                GPIOF_DIR_IN, "aw22xxx_int");
        if (ret){
            dev_err(&i2c->dev, "%s: int request failed\n", __func__);
            goto err_gpio_request;
        }
    }

    /* hardware reset */
    aw22xxx_hw_reset(aw22xxx);

    /* aw22xxx chip id */
    ret = aw22xxx_read_chipid(aw22xxx);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s: aw22xxx_read_chipid failed ret=%d\n", __func__, ret);
        goto err_id;
    }

    /* aw22xxx irq */
    if (gpio_is_valid(aw22xxx->irq_gpio) &&
            !(aw22xxx->flags & AW22XXX_FLAG_SKIP_INTERRUPTS)) {
        /* register irq handler */
        aw22xxx_interrupt_setup(aw22xxx);
        irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
        ret = devm_request_threaded_irq(&i2c->dev,
                gpio_to_irq(aw22xxx->irq_gpio),
                NULL, aw22xxx_irq, irq_flags,
                "aw22xxx", aw22xxx);
        if (ret != 0) {
            dev_err(&i2c->dev, "%s: failed to request IRQ %d: %d\n",
                    __func__, gpio_to_irq(aw22xxx->irq_gpio), ret);
            goto err_irq;
        }
    } else {
        dev_info(&i2c->dev, "%s skipping IRQ registration\n", __func__);
        /* disable feature support if gpio was invalid */
        aw22xxx->flags |= AW22XXX_FLAG_SKIP_INTERRUPTS;
    }

    dev_set_drvdata(&i2c->dev, aw22xxx);

    aw22xxx_parse_led_cdev(aw22xxx, np);
    if (ret < 0) {
        dev_err(&i2c->dev, "%s error creating led class dev\n", __func__);
        goto err_sysfs;
    }

    aw22xxx_fw_init(aw22xxx);


    //pr_info("%s probe completed successfully!\n", __func__);

    return 0;

err_sysfs:
    devm_free_irq(&i2c->dev, gpio_to_irq(aw22xxx->irq_gpio), aw22xxx);
err_irq:
err_id:
    devm_gpio_free(&i2c->dev, aw22xxx->reset_gpio);
    devm_gpio_free(&i2c->dev, aw22xxx->irq_gpio);
err_gpio_request:
err_parse_dt:
    devm_kfree(&i2c->dev, aw22xxx);
    aw22xxx = NULL;
    return ret;
}

static int aw22xxx_i2c_remove(struct i2c_client *i2c)
{
    struct aw22xxx *aw22xxx = i2c_get_clientdata(i2c);

    //pr_info("%s: enter\n", __func__);
    sysfs_remove_group(&aw22xxx->cdev.dev->kobj,
            &aw22xxx_attribute_group);
    led_classdev_unregister(&aw22xxx->cdev);

    devm_free_irq(&i2c->dev, gpio_to_irq(aw22xxx->irq_gpio), aw22xxx);

    if (gpio_is_valid(aw22xxx->reset_gpio))
        devm_gpio_free(&i2c->dev, aw22xxx->reset_gpio);
    if (gpio_is_valid(aw22xxx->irq_gpio))
        devm_gpio_free(&i2c->dev, aw22xxx->irq_gpio);

    devm_kfree(&i2c->dev, aw22xxx);
    aw22xxx = NULL;

    return 0;
}

static const struct i2c_device_id aw22xxx_i2c_id[] = {
    { AW22XXX_I2C_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, aw22xxx_i2c_id);

static struct of_device_id aw22xxx_dt_match[] = {
    { .compatible = "awinic,aw22xxx_led" },
    { },
};

static struct i2c_driver aw22xxx_i2c_driver = {
    .driver = {
        .name = AW22XXX_I2C_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(aw22xxx_dt_match),
    },
    .probe = aw22xxx_i2c_probe,
    .remove = aw22xxx_i2c_remove,
    .id_table = aw22xxx_i2c_id,
};


static int __init aw22xxx_i2c_init(void)
{
    int ret = 0;

    //pr_info("aw22xxx driver version %s\n", AW22XXX_DRIVER_VERSION);

    ret = i2c_add_driver(&aw22xxx_i2c_driver);
    if(ret){
        pr_err("fail to add aw22xxx device into i2c\n");
        return ret;
    }

    return 0;
}
module_init(aw22xxx_i2c_init);


static void __exit aw22xxx_i2c_exit(void)
{
    i2c_del_driver(&aw22xxx_i2c_driver);
}
module_exit(aw22xxx_i2c_exit);


MODULE_DESCRIPTION("AW22XXX LED Driver");
MODULE_LICENSE("GPL v2");

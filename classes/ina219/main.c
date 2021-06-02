/*
 * HAKit - The Home Automation KIT
 * Copyright (C) 2021 Sylvain Giroudon
 *
 * INA219 current sensor on Raspberry Pi
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <mqueue.h>

#include "log.h"
#include "mod.h"
#include "sys.h"
#include "version.h"
#include "i2cdev.h"
#include "ina219.h"


#define CLASS_NAME "ina219"

#define DEFAULT_I2C_BUS 1

typedef struct {
	hk_obj_t *obj;
	char *hdr;
	i2cdev_t i2cdev;
        ina219_t chip;
	hk_pad_t *trig;
	hk_pad_t *current;
	hk_pad_t *voltage;
        int period;
	sys_tag_t period_tag;
} ctx_t;


static int ina219_read_u16(i2cdev_t *i2cdev, uint8_t addr, uint16_t *pvalue)
{
        uint8_t buf[2];

        if (i2cdev_read(i2cdev, INA219_COMMAND_BIT|addr, sizeof(buf), buf) < 0) {
                return -1;
        }

        log_debug(3, "%sina219_read(0x%02X) => 0x%02X%02X", i2cdev->hdr, addr, buf[0], buf[1]);

        if (pvalue != NULL) {
                *pvalue = (((uint16_t) buf[0]) << 8) + buf[1];
        }

        return 0;
}


static int ina219_write_u16(i2cdev_t *i2cdev, uint8_t addr, uint16_t value)
{
        uint8_t buf[2] = { (value >> 8) & 0xFF, value & 0xFF};

        log_debug(3, "%sina219_write(0x%02X) => 0x%02X%02X", i2cdev->hdr, addr, buf[0], buf[1]);

        if (i2cdev_write(i2cdev, INA219_COMMAND_BIT|addr, sizeof(buf), buf) < 0) {
                return -1;
        }

        return 0;
}


static inline int ina219_read_s16(i2cdev_t *i2cdev, uint8_t addr, int16_t *pvalue)
{
        return ina219_read_u16(i2cdev, addr, (uint16_t *) pvalue);
}


static int ina219_write_config(ctx_t *ctx)
{
        log_str("%sconfig = 0x%04X", ctx->hdr, ctx->chip.config);
        return ina219_write_u16(&ctx->i2cdev, INA219_CONFIG, ctx->chip.config);
}


static int ina219_write_calibration(ctx_t *ctx)
{
        log_str("%scalibration = 0x%04X", ctx->hdr, ctx->chip.cal_value);
        return ina219_write_u16(&ctx->i2cdev, INA219_CALIBRATION, ctx->chip.cal_value);
}


static int ina219_read_voltage(ctx_t *ctx)
{
        uint16_t value = 0;

        if (ina219_read_u16(&ctx->i2cdev, INA219_BUS_VOLTAGE, &value) < 0) {
                return -1;
        }

        return (value >> 3) * 4;
}


static int ina219_read_current(ctx_t *ctx)
{
        int16_t value = 0;

        if (ina219_read_s16(&ctx->i2cdev, INA219_CURRENT, &value) < 0) {
                return 0;
        }

        return ina219_get_current(&ctx->chip, value);
}


static int _new(hk_obj_t *obj)
{
	/* Alloc object context */
	ctx_t *ctx = malloc(sizeof(ctx_t));
	memset(ctx, 0, sizeof(ctx_t));
	ctx->obj = obj;
	obj->ctx = ctx;

        /* Set debug/error message header */
	int size = strlen(CLASS_NAME) + strlen(obj->name) + 8;
	ctx->hdr = malloc(size);
	snprintf(ctx->hdr, size, CLASS_NAME "(%s): ", obj->name);

        /* Init I2C bus interface */
        if (i2cdev_init(&ctx->i2cdev, ctx->hdr) < 0) {
		goto failed;
        }

	/* Get I2C bus number property */
	int bus = hk_prop_get_int(&obj->props, "bus");
	if (bus <= 0) {
		bus = DEFAULT_I2C_BUS;
	}

	/* Get I2C address property */
	int addr = hk_prop_get_int(&obj->props, "addr");
	if (addr < INA219_I2C_MIN_ADDR) {
		addr = INA219_I2C_MIN_ADDR;
	}
	if (addr > INA219_I2C_MAX_ADDR) {
		addr = INA219_I2C_MAX_ADDR;
	}
        log_str("%sI2C: bus=%d addr=0x%02X", ctx->hdr, bus, addr);

        /* Get trigger period property */
	char *scale = hk_prop_get(&obj->props, "scale");
        if (scale == NULL) {
                scale = "32V_2A";
        }

        if (strcmp(scale, "16V_5A") == 0) {
                ina219_set_calibration_16V_5A(&ctx->chip);
        }
        else if (strcmp(scale, "16V_400mA") == 0) {
                ina219_set_calibration_16V_400mA(&ctx->chip);
        }
        else if (strcmp(scale, "32V_1A") == 0) {
                ina219_set_calibration_32V_1A(&ctx->chip);
        }
        else {
                scale = "32V_2A";
                ina219_set_calibration_32V_2A(&ctx->chip);
        }
        log_str("%sscale = %s", ctx->hdr, scale);

	int res = hk_prop_get_int(&obj->props, "res");
        if (res >= 128) {
                res = 128;
                ina219_set_badc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_128S);
                ina219_set_sadc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_128S);
        }
        else if (res >= 64) {
                res = 64;
                ina219_set_badc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_64S);
                ina219_set_sadc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_64S);
        }
        else if (res >= 32) {
                res = 32;
                ina219_set_badc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_32S);
                ina219_set_sadc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_32S);
        }
        else if (res >= 16) {
                res = 16;
                ina219_set_badc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_16S);
                ina219_set_sadc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_16S);
        }
        else if (res >= 8) {
                res = 8;
                ina219_set_badc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_8S);
                ina219_set_sadc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_8S);
        }
        else if (res >= 4) {
                res = 4;
                ina219_set_badc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_4S);
                ina219_set_sadc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_4S);
        }
        else if (res >= 2) {
                res = 2;
                ina219_set_badc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_2S);
                ina219_set_sadc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_2S);
        }
        else {
                res = 1;
                ina219_set_badc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_1S);
                ina219_set_sadc_res(&ctx->chip, INA219_CONFIG_ADCRES_12BIT_1S);
        }
        log_str("%sADC resolution: %d samples", ctx->hdr, res);

        /* Get trigger period property */
	ctx->period = hk_prop_get_int(&obj->props, "period");

	/* Open I2C device */
	if (i2cdev_open(&ctx->i2cdev, bus, addr) < 0) {
		goto failed;
	}

        /* Reset the chip */
        if (ina219_write_u16(&ctx->i2cdev, INA219_CONFIG, INA219_CONFIG_RST) < 0) {
                goto failed;
        }

        /* Set config and calibration */
        ina219_write_calibration(ctx);
        ina219_write_config(ctx);

        /* Get config register */
        uint16_t config = 0;
        if (ina219_read_u16(&ctx->i2cdev, INA219_CONFIG, &config) < 0) {
                goto failed;
        }
        log_str("%sconfig = 0x%04X", ctx->hdr, ctx->chip.config);

	/* Create pads */
        ctx->trig = hk_pad_create(obj, HK_PAD_IN, "trig");
        ctx->current = hk_pad_create(obj, HK_PAD_OUT, "current");
        ctx->voltage = hk_pad_create(obj, HK_PAD_OUT, "voltage");

	return 0;

failed:
	i2cdev_close(&ctx->i2cdev);

	if (ctx->hdr != NULL) {
		free(ctx->hdr);
		ctx->hdr = NULL;
	}

	free(ctx);

	obj->ctx = NULL;
	return -1;
}


static int input_trig(ctx_t *ctx, bool refresh)
{
        /* Read value */
        int voltage = ina219_read_voltage(ctx);
        if (refresh || (voltage != ctx->voltage->state)) {
                ctx->voltage->state = voltage;
                hk_pad_update_int(ctx->voltage, voltage);
        }

        int current = ina219_read_current(ctx);
        if (refresh || (current != ctx->current->state)) {
                ctx->current->state = current;
                hk_pad_update_int(ctx->current, current);
        }

        return 1;
}


static int input_trig_periodic(ctx_t *ctx)
{
        return input_trig(ctx, false);
}


static int input_trig_async(ctx_t *ctx)
{
        return input_trig(ctx, true);
}


static void _start(hk_obj_t *obj)
{
	ctx_t *ctx = obj->ctx;
        if (ctx == NULL) {
                return;
        }

        input_trig_async(ctx);

        if (ctx->period > 0) {
                ctx->period_tag = sys_timeout(ctx->period, (sys_func_t) input_trig_periodic, ctx);
        }
}


static void _input(hk_pad_t *pad, char *value)
{
	ctx_t *ctx = pad->obj->ctx;
        if (ctx == NULL) {
                return;
        }

        int v = atoi(value);

        log_debug(2, "%s_input %s='%s'=%d", ctx->hdr, pad->name, value, v);

        if (pad == ctx->trig) {
                if (v != 0) {
                        input_trig_async(ctx);
                }
        }
}


hk_class_t _class = {
	.name = CLASS_NAME,
	.version = VERSION,
	.new = _new,
	.start = _start,
	.input = _input,
};

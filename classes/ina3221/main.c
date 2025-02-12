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

#include "log.h"
#include "mod.h"
#include "sys.h"
#include "version.h"
#include "i2cdev.h"
#include "ina3221.h"


#define CLASS_NAME "ina3221"

#define DEFAULT_I2C_BUS 1


typedef struct {
	hk_obj_t *obj;
	char *hdr;
	i2cdev_t i2cdev;
	hk_pad_t *trig;
	hk_pad_t *current[INA3221_NUM_CHANNELS];
	hk_pad_t *voltage[INA3221_NUM_CHANNELS];
        int period;
	sys_tag_t period_tag;
        float rshunt;
} ctx_t;


static int ina3221_read_u16(i2cdev_t *i2cdev, uint8_t addr, uint16_t *pvalue)
{
        uint8_t buf[2];

        if (i2cdev_read(i2cdev, INA3221_COMMAND_BIT|addr, sizeof(buf), buf) < 0) {
                return -1;
        }

        log_debug(3, "%sina3221_read(0x%02X) => 0x%02X%02X", i2cdev->hdr, addr, buf[0], buf[1]);

        if (pvalue != NULL) {
                *pvalue = (((uint16_t) buf[0]) << 8) + buf[1];
        }

        return 0;
}


static int ina3221_write_u16(i2cdev_t *i2cdev, uint8_t addr, uint16_t value)
{
        uint8_t buf[2] = { (value >> 8) & 0xFF, value & 0xFF};

        log_debug(3, "%sina3221_write(0x%02X) => 0x%02X%02X", i2cdev->hdr, addr, buf[0], buf[1]);

        if (i2cdev_write(i2cdev, INA3221_COMMAND_BIT|addr, sizeof(buf), buf) < 0) {
                return -1;
        }

        return 0;
}


static inline int ina3221_read_s16(i2cdev_t *i2cdev, uint8_t addr, int16_t *pvalue)
{
        return ina3221_read_u16(i2cdev, addr, (uint16_t *) pvalue);
}


static int ina3221_read_voltage(ctx_t *ctx, int ch)
{
        uint16_t value = 0;

        if (ina3221_read_u16(&ctx->i2cdev, INA3221_REG_BUS1+(ch*2), &value) < 0) {
                return -1;
        }

        int voltage = value;  //TODO

        return voltage;
}


static int ina3221_read_current(ctx_t *ctx, int ch)
{
        int16_t value = 0;

        if (ina3221_read_s16(&ctx->i2cdev, INA3221_REG_SHUNT1+(ch*2), &value) < 0) {
                return 0;
        }

        int current = value;  //TODO

        return current;
}


static int _new(hk_obj_t *obj)
{
        char *str;

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
        unsigned int addr = INA3221_I2C_MIN_ADDR;
        str =  hk_prop_get(&obj->props, "addr");
        if (str != NULL) {
                addr = strtoul(str, NULL, 0);
                if ((addr < INA3221_I2C_MIN_ADDR) || (addr > INA3221_I2C_MAX_ADDR)) {
                        log_str("%sERROR: Wrong I2C address 0x%02X", ctx->hdr, addr);
                        goto failed;
                }
        }
        log_str("%sI2C: bus=%d addr=0x%02X", ctx->hdr, bus, addr);

        /* Get trigger period property */
	ctx->period = hk_prop_get_int(&obj->props, "period");

        /* Get Rshunt property in ohms */
        ctx->rshunt = 0.1;
        str =  hk_prop_get(&obj->props, "rshunt");
        if (str != NULL) {
                ctx->rshunt = atof(str);
                if (ctx->rshunt <= 0) {
                        log_str("%sERROR: Illegal Rshunt value: %.03f", ctx->hdr, ctx->rshunt);
                        goto failed;
                }
        }
        log_str("%sRshunt = %.03f ohms", ctx->hdr, ctx->rshunt);

	/* Open I2C device */
	if (i2cdev_open(&ctx->i2cdev, bus, addr) < 0) {
		goto failed;
	}

        /* Check chip id */
        uint16_t manufacturer_id = 0;
        if (ina3221_read_u16(&ctx->i2cdev, INA3221_REG_MANUFACTURER_ID, &manufacturer_id) < 0) {
                goto failed;
        }

        uint16_t die_id = 0;
        ina3221_read_u16(&ctx->i2cdev, INA3221_REG_DIE_ID, &die_id);
        log_str("%sManufacturer ID = 0x%04X, Die ID = 0x%04X", ctx->hdr, manufacturer_id, die_id);

        if (manufacturer_id != INA3221_MANUFACTURER_ID) {
                log_str("%sERROR: Wrong manufacturer id", ctx->hdr);
                goto failed;
        }

        /* Reset the chip */
        if (ina3221_write_u16(&ctx->i2cdev, INA3221_REG_CONFIG, INA3221_CONFIG_RST) < 0) {
                goto failed;
        }

        /* Get config register */
        uint16_t config = 0;
        if (ina3221_read_u16(&ctx->i2cdev, INA3221_REG_CONFIG, &config) < 0) {
                goto failed;
        }
        log_str("%sconfig = 0x%04X", ctx->hdr, config);

	/* Create pads */
        ctx->trig = hk_pad_create(obj, HK_PAD_IN, "trig");
        int ch;
        for (ch = 0; ch < INA3221_NUM_CHANNELS; ch++) {
                char str[16];
                snprintf(str, sizeof(str), "current%d", ch+1);
                ctx->current[ch] = hk_pad_create(obj, HK_PAD_OUT, str);
                snprintf(str, sizeof(str), "voltage%d", ch+1);
                ctx->voltage[ch] = hk_pad_create(obj, HK_PAD_OUT, str);
        }

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
        int ch;

        for (ch = 0; ch < INA3221_NUM_CHANNELS; ch++) {
                /* Read value */
                if (hk_pad_is_connected(ctx->voltage[ch])) {
                        int voltage = ina3221_read_voltage(ctx, ch);
                        if (refresh || (voltage != ctx->voltage[ch]->state)) {
                                ctx->voltage[ch]->state = voltage;
                                hk_pad_update_int(ctx->voltage[ch], voltage);
                        }
                }

                if (hk_pad_is_connected(ctx->current[ch])) {
                        int current = ina3221_read_current(ctx, ch) / (200 * ctx->rshunt);
                        if (refresh || (current != ctx->current[ch]->state)) {
                                ctx->current[ch]->state = current;
                                hk_pad_update_int(ctx->current[ch], current);
                        }
                }
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
                if (ctx->period_tag != 0) {
                        sys_remove(ctx->period_tag);
                }
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

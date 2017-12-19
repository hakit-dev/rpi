/*
 * HAKit - The Home Automation KIT
 * Copyright (C) 2017 Sylvain Giroudon
 *
 * TCS34725 color sensor on Raspberry Pi
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include <stdlib.h>
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
#include "tcs34725.h"


#define CLASS_NAME "tcs34725"

#define DEFAULT_I2C_NUM 1

#define DEFAULT_GAIN TCS34725_GAIN_4X

typedef struct {
	hk_obj_t *obj;
	char *hdr;
	i2cdev_t i2cdev;
	hk_pad_t *trig;
	hk_pad_t *atime;
	hk_pad_t *gain;
	hk_pad_t *c;
	hk_pad_t *r;
	hk_pad_t *g;
	hk_pad_t *b;
} ctx_t;


static int tcs34725_check_id(i2cdev_t *i2cdev)
{
        int ret = 0;
        uint8_t chip_id = 0;

        if (i2cdev_read(i2cdev, TCS34725_COMMAND_BIT|TCS34725_ID, 1, &chip_id) == 1) {
                if (chip_id == 0x44) {
                        ret = 1;
                }
                else {
                        log_str("ERROR: %sWrong chip id %02X (0x44 expected)", i2cdev->hdr, chip_id);
                }
        }
        else {
                log_str("ERROR: %sUnable to read chip id", i2cdev->hdr);
        }

        return ret;
}


static int tcs34725_enable_aen(i2cdev_t *i2cdev)
{
        i2cdev_write(i2cdev, TCS34725_COMMAND_BIT|TCS34725_ENABLE, (TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN));
        return 0;
}


static int tcs34725_enable(i2cdev_t *i2cdev)
{
        /* Set Power-on enable flag */
        if (i2cdev_write(i2cdev, TCS34725_COMMAND_BIT|TCS34725_ENABLE, TCS34725_ENABLE_PON) < 0) {
                return -1;
        }

        /* Wait 10ms then set the ADC enable flag */
        sys_timeout(10, (sys_func_t) tcs34725_enable_aen, i2cdev);

        return 0;
}


static int tcs34725_disable(i2cdev_t *i2cdev)
{
        uint8_t reg = 0;

        if (i2cdev_read(i2cdev, TCS34725_COMMAND_BIT|TCS34725_ENABLE, 1, &reg) != 1) {
                return -1;
        }

        reg &= ~(TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);

        if (i2cdev_write(i2cdev, TCS34725_COMMAND_BIT|TCS34725_ENABLE, reg) < 0) {
                return -1;
        }

        return 0;
}


static int tcs34725_set_integration_time(i2cdev_t *i2cdev, uint8_t atime)
{
        if (i2cdev_write(i2cdev, TCS34725_COMMAND_BIT|TCS34725_ATIME, atime) < 0) {
                return -1;
        }

        return 0;
}


static int tcs34725_set_gain(i2cdev_t *i2cdev, uint8_t gain)
{
        if (i2cdev_write(i2cdev, TCS34725_COMMAND_BIT|TCS34725_CONTROL, gain) < 0) {
                return -1;
        }

        return 0;
}


static int tcs34725_get_raw_data(i2cdev_t *i2cdev, uint16_t crgb[4])
{
        uint8_t buf[8];
        int i;

        if (i2cdev_read(i2cdev, TCS34725_COMMAND_BIT|TCS34725_CDATAL, sizeof(buf), buf) < 0) {
                return -1;
        }

#if 0
        if (i2cdev_read_16le(i2cdev, TCS34725_COMMAND_BIT|TCS34725_RDATAL, &crgb[1]) < 0) {
                return -1;
        }

        if (i2cdev_read_16le(i2cdev, TCS34725_COMMAND_BIT|TCS34725_GDATAL, &crgb[2]) < 0) {
                return -1;
        }

        if (i2cdev_read_16le(i2cdev, TCS34725_COMMAND_BIT|TCS34725_BDATAL, &crgb[3]) < 0) {
                return -1;
        }
#endif

        for (i = 0; i < 4; i++) {
                crgb[i] = ((uint16_t) (buf[i*2+1] << 8)) + ((uint16_t) buf[i*2]);
        }

        log_debug(2, "%stcs34725_get_raw_data => c=%04X r=%04X g=%04X b=%04X", i2cdev->hdr, crgb[0], crgb[1], crgb[2], crgb[3]);

        return 0;
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

	/* Get I2C device num property */
	int num = hk_prop_get_int(&obj->props, "num");
	if (num <= 0) {
		num = DEFAULT_I2C_NUM;
	}

	/* Open I2C device */
	if (i2cdev_open(&ctx->i2cdev, num, TCS34725_ADDR) < 0) {
		goto failed;
	}

        /* Check chip id */
        if (!tcs34725_check_id(&ctx->i2cdev)) {
                goto failed;
        }

        /* Enable sensor */
        if (tcs34725_enable(&ctx->i2cdev) < 0) {
                goto failed;
        }

	/* Create pads */
        ctx->trig = hk_pad_create(obj, HK_PAD_IN, "trig");
        ctx->atime = hk_pad_create(obj, HK_PAD_IN, "atime");
        ctx->gain = hk_pad_create(obj, HK_PAD_IN, "gain");
        ctx->c = hk_pad_create(obj, HK_PAD_OUT, "c");
        ctx->r = hk_pad_create(obj, HK_PAD_OUT, "r");
        ctx->g = hk_pad_create(obj, HK_PAD_OUT, "g");
        ctx->b = hk_pad_create(obj, HK_PAD_OUT, "b");

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


static void input_trig(ctx_t *ctx, int v)
{
        if (v != 0) {
                /* Read value */
                uint16_t crgb[4];
                tcs34725_get_raw_data(&ctx->i2cdev, crgb);

                if (crgb[0] != ctx->c->state) {
                        ctx->c->state = crgb[0];
                        hk_pad_update_int(ctx->c, ctx->c->state);
                }
                if (crgb[1] != ctx->r->state) {
                        ctx->r->state = crgb[1];
                        hk_pad_update_int(ctx->r, ctx->r->state);
                }
                if (crgb[2] != ctx->g->state) {
                        ctx->g->state = crgb[2];
                        hk_pad_update_int(ctx->g, ctx->g->state);
                }
                if (crgb[3] != ctx->b->state) {
                        ctx->b->state = crgb[3];
                        hk_pad_update_int(ctx->b, ctx->b->state);
                }
        }
}


static void input_atime(ctx_t *ctx, int v)
{
        uint8_t atime;

        if (v >= 700) {
                atime = TCS34725_ATIME_700MS;
        }
        else if (v >= 154) {
                atime = TCS34725_ATIME_154MS;
        }
        else if (v >= 101) {
                atime = TCS34725_ATIME_101MS;
        }
        else if (v >= 50) {
                atime = TCS34725_ATIME_50MS;
        }
        else if (v >= 24) {
                atime = TCS34725_ATIME_24MS;
        }
        else {
                atime = TCS34725_ATIME_2_4MS;
        }

        tcs34725_set_integration_time(&ctx->i2cdev, atime);
}


static void input_gain(ctx_t *ctx, int v)
{
        uint8_t gain;

        if (v >= 60) {
                gain = TCS34725_GAIN_60X;
        }
        else if (v >= 16) {
                gain = TCS34725_GAIN_16X;
        }
        else if (v >= 4) {
                gain = TCS34725_GAIN_4X;
        }
        else {
                gain = TCS34725_GAIN_1X;
        }

        tcs34725_set_gain(&ctx->i2cdev, gain);
}


static void _input(hk_pad_t *pad, char *value)
{
	ctx_t *ctx = pad->obj->ctx;
        int v = atoi(value);

        log_debug(2, "%s_input %s='%s'=%d", ctx->hdr, pad->name, value, v);

        if (pad == ctx->trig) {
                input_trig(ctx, v);
        }
        else if (pad == ctx->atime) {
                input_atime(ctx, v);
        }
        else if (pad == ctx->gain) {
                input_gain(ctx, v);
        }
}


hk_class_t _class = {
	.name = CLASS_NAME,
	.version = VERSION,
	.new = _new,
	.input = _input,
};

/*
 * HAKit - The Home Automation KIT
 * Copyright (C) 2016 Sylvain Giroudon
 *
 * MCP3008 8-channel SPI ADC on Raspberry Pi
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
#include "spidev.h"


#define CLASS_NAME "mcp3008"

#define DEFAULT_ID "0.0"
#define DEFAULT_SPEED_HZ 1000000
#define DEFAULT_BITS_PER_WORD 8

#define NCHANS 8
#define MSG_MAXSIZE 16


typedef struct {
	hk_obj_t *obj;
	char *hdr;
	spidev_t spidev;
	pthread_t thr;
	mqd_t qin;
	mqd_t qout;
	sys_tag_t qout_tag;
	unsigned char cfg[NCHANS];
	hk_pad_t *trig[NCHANS];
	hk_pad_t *out[NCHANS];
        int period;
	sys_tag_t period_tag;
} ctx_t;


typedef struct {
	unsigned int chan;
	int value;
} msg_t;


static int read_value(ctx_t *ctx, unsigned char cfg)
{
	int value = -1;
	int size;

	unsigned char buf[3] = {
		0x01,                     // 1st byte transmitted -> start bit
		cfg,                      // 2nd byte transmitted -> (SGL/DIF = 1, D2=D1=D0=0)
		0x00,                     // 3rd byte transmitted....don't care
	};

	log_debug(2, "%sread_value diff=%u chan=%u", ctx->hdr, (cfg >> 7) & 1, (cfg >> 4) & 0x7);
	log_debug(2, "%sSPI write %02X %02X %02X", ctx->hdr, buf[0], buf[1], buf[2]);

	size = spidev_write_read(&ctx->spidev, buf, sizeof(buf));
	if (size == sizeof(buf)) {
		log_debug(2, "%sSPI read %02X %02X %02X", ctx->hdr, buf[0], buf[1], buf[2]);
		value = (((unsigned int) (buf[1] & 0x03)) << 8) | buf[2];
	}

	return value;
}


static void *qin_recv_loop(void *_ctx)
{
	ctx_t *ctx = _ctx;
	int ret = 0;

	while (ret == 0) {
		char mbuf[MSG_MAXSIZE];
		ssize_t msize;

		msize = mq_receive(ctx->qin, mbuf, sizeof(mbuf), NULL);
		if (msize < 0) {
			if ((errno != EAGAIN) && (errno != EINTR)) {
				log_str("PANIC: %sCannot read input queue: %s", ctx->hdr, strerror(errno));
			}
		}

		log_debug(2, "%sqin_recv_loop -> %d", ctx->hdr, msize);

		if (msize == sizeof(unsigned int)) {
			unsigned int *pchan = (unsigned int *) mbuf;
                        unsigned int chan = *pchan;

			if (chan < NCHANS) {
				msg_t msg = {
					.chan = chan,
					.value = read_value(ctx, ctx->cfg[chan]),
				};
				if (mq_send(ctx->qout, (char *) &msg, sizeof(msg), 0) < 0) {
					log_str("PANIC: %sCannot write output queue: %s", ctx->hdr, strerror(errno));
					ret = -1;
				}
			}
			else {
				ret = 1;
				log_debug(1, "%sLeaving input loop", ctx->hdr);
			}
		}
		else {
			log_str("PANIC: %sIllegal data received from input queue (%d bytes)", ctx->hdr, msize);
		}
	}

	return NULL;
}


static int qout_recv(ctx_t *ctx, int fd)
{
	char mbuf[MSG_MAXSIZE];
	ssize_t msize;

	msize = mq_receive(ctx->qout, mbuf, sizeof(mbuf), NULL);
	if (msize < 0) {
		if ((errno != EAGAIN) && (errno != EINTR)) {
			log_str("PANIC: %sCannot receive from output queue: %s", ctx->hdr, strerror(errno));
			return 0;
		}
	}

	log_debug(2, "%sqout_recv -> %d", ctx->hdr, msize);

	if (msize == sizeof(msg_t)) {
		msg_t *msg = (msg_t *) mbuf;
		if (msg->chan < NCHANS) {
                        hk_pad_t *out = ctx->out[msg->chan];
                        if (msg->value != out->state) {
                                out->state = msg->value;
                                hk_pad_update_int(out, msg->value);
                        }
		}
		else {
			log_str("PANIC: %sIllegal channel number received from output queue (%u)", ctx->hdr, msg->chan);
		}
	}
	else {
		log_str("PANIC: %sIllegal data received from output queue (%d bytes)", ctx->hdr, msize);
	}

	return 1;
}


static int create_queue(ctx_t *ctx, char *name, int flags)
{
	char qname[128];
	int mqd;

	snprintf(qname, sizeof(qname), "/hakit-%d-%s-%s", getpid(), ctx->obj->name, name);
	log_debug(2, "%screate_queue '%s'", ctx->hdr, qname);

	int qflags = O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL | flags;
	struct mq_attr qattr = {
		.mq_flags   = flags,
		.mq_maxmsg  = NCHANS,
		.mq_msgsize = MSG_MAXSIZE,
	};

	mqd = mq_open(qname, qflags, S_IRUSR | S_IWUSR, &qattr);
	if (mqd < 0) {
		log_str("PANIC: %sCannot create %sput queue: %s", ctx->hdr, name, strerror(errno));
		return -1;
	}

	/* Hide message queue from other processes */
	if (mq_unlink(qname) < 0) {
		log_str("PANIC: %sCannot unlink %sput queue: %s", ctx->hdr, name, strerror(errno));
		mq_close(mqd);
		return -1;
	}

	return mqd;
}


static int trigger(ctx_t *ctx, unsigned int chan)
{
	if (mq_send(ctx->qin, (char *) &chan, sizeof(chan), 0) < 0) {
		log_str("PANIC: %sCannot write input queue: %s", ctx->hdr, strerror(errno));
                return 0;
	}

	return 1;
}


static int trigger_all(ctx_t *ctx)
{
        int chan;

        for (chan = 0; chan < NCHANS; chan++) {
                if (ctx->trig[chan] != NULL) {
                        if (!trigger(ctx, chan)) {
                                return 0;
                        }
                }
        }

	return 1;
}


static int _new(hk_obj_t *obj)
{
	ctx_t *ctx;
	char *id;
	char *str;
	int size;

	/* Alloc object context */
	ctx = malloc(sizeof(ctx_t));
	memset(ctx, 0, sizeof(ctx_t));
	ctx->obj = obj;
	obj->ctx = ctx;
	spidev_init(&ctx->spidev, DEFAULT_SPEED_HZ, DEFAULT_BITS_PER_WORD);
	ctx->qin = -1;
	ctx->qout = -1;

	/* Get SPI device id */
	id = hk_prop_get(&obj->props, "id");
	if (id == NULL) {
		id = DEFAULT_ID;
	}

	/* Set debug/error message header */
	size = strlen(CLASS_NAME) + strlen(ctx->obj->name) + 8;
	ctx->hdr = malloc(size);
	snprintf(ctx->hdr, size, CLASS_NAME "(%s): ", ctx->obj->name);

        /* Get period property */
	ctx->period = hk_prop_get_int(&obj->props, "period");

	/* Get list of channels */
	str = hk_prop_get(&obj->props, "channels");
	if (str == NULL) {
		goto failed;
	}

	/* Create trigger and output pads for each channel */
	while (str != NULL) {
		char *end = strchr(str, ',');
		if (end != NULL) {
			*(end++) = '\0';
		}

                unsigned char diff = 0x80;
                if (*str == '*') {
                        str++;
                        diff = 0x00;
                }

		unsigned int chan = strtoul(str, NULL, 0);
		if (chan < NCHANS) {
			char buf[16];

                        ctx->cfg[chan] = (diff | (chan & 0x07)) << 4;

			snprintf(buf, sizeof(buf), "trig%u", chan);
			ctx->trig[chan] = hk_pad_create(obj, HK_PAD_IN, buf);
			ctx->trig[chan]->state = chan;

			snprintf(buf, sizeof(buf), "out%u", chan);
			ctx->out[chan] = hk_pad_create(obj, HK_PAD_IN, buf);
			ctx->out[chan]->state = 0x7FFFFFFF; // Unrealistic value to ensure value will be updated at first trigger
		}

		str = end;
	}

	/* Open SPI device name */
	if (spidev_open(&ctx->spidev, ctx->hdr, id) < 0) {
		goto failed;
	}

	/* Create input request queue */
	ctx->qin = create_queue(ctx, "in", 0);
	if (ctx->qin < 0) {
		goto failed;
	}

	/* Create output result queue */
	ctx->qout = create_queue(ctx, "out", O_NONBLOCK);
	if (ctx->qout < 0) {
		goto failed;
	}

	ctx->qout_tag = sys_io_watch(ctx->qout, (sys_io_func_t) qout_recv, ctx);

	/* Create read thread */
	if (pthread_create(&ctx->thr, NULL, qin_recv_loop, ctx)) {
		log_str("PANIC: %sFailed to create thread: %s", ctx->hdr, strerror(errno));
		goto failed;
	}

	return 0;

failed:
	if (ctx->qout_tag != 0) {
		sys_remove(ctx->qout_tag);
		ctx->qout_tag = 0;
	}

	if (ctx->qout >= 0) {
		mq_close(ctx->qout);
		ctx->qout = -1;
	}

	if (ctx->qin >= 0) {
		mq_close(ctx->qin);
		ctx->qin = -1;
	}

	spidev_close(&ctx->spidev);

	if (ctx->hdr != NULL) {
		free(ctx->hdr);
		ctx->hdr = NULL;
	}

	free(ctx);

	obj->ctx = NULL;

	return -1;
}


static void _start(hk_obj_t *obj)
{
	ctx_t *ctx = obj->ctx;

        trigger_all(ctx);

        if (ctx->period > 0) {
		ctx->period_tag = sys_timeout(ctx->period, (sys_func_t) trigger_all, ctx);
        }
}


static void _input(hk_pad_t *pad, char *value)
{
	ctx_t *ctx = pad->obj->ctx;

	/* Ignore falling edge */
	if (value[0] != '0') {
                trigger(ctx, pad->state);
	}
}


hk_class_t _class = {
	.name = CLASS_NAME,
	.version = VERSION,
	.new = _new,
	.start = _start,
	.input = _input,
};

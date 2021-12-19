/*
 * HAKit - The Home Automation KIT
 * Copyright (C) 2016 Sylvain Giroudon
 *
 * DS18B20 1-wire temperature sensor on Raspberry Pi
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
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <mqueue.h>

#include "log.h"
#include "mod.h"
#include "sys.h"
#include "prop.h"

#include "version.h"


#define CLASS_NAME "ds18b20"

#define SYS_W1_DIR "/sys/bus/w1/devices/"
#define MSG_MAXSIZE 16


typedef struct {
	hk_obj_t *obj;
	char *id;
	char *path;
	pthread_t thr;
	mqd_t qin;
	mqd_t qout;
	sys_tag_t qout_tag;
	hk_pad_t *trig;
	hk_pad_t *out;
        int period;
	sys_tag_t period_tag;
} ctx_t;


static char *find_id(hk_obj_t *obj, char *id)
{
	DIR *d;
	struct dirent *ent;
	int count = 0;
	char *dir = NULL;

	/* Id specified: check it actually exists */
	d = opendir(SYS_W1_DIR);
	if (d == NULL) {
		log_str("ERROR: " CLASS_NAME "(%s): Directory " SYS_W1_DIR " not found", obj->name);
		return NULL;
	}

	while ((ent = readdir(d)) != NULL) {
		char *name = ent->d_name;

		if (isdigit(name[0])) {
			int found = 0;

			if (dir == NULL) {
				if (id != NULL) {
					/* Id specified: check it matches an existing devices */
					if (strcmp(id, name) == 0) {
						dir = strdup(name);
						found = 1;
					}
				}
				else {
					/* No id specified: take the first one */
					dir = strdup(name);
					found = 1;
				}
			}

			if (count == 0) {
				log_debug(1, CLASS_NAME "(%s): 1-wire device list:", obj->name);
			}
			log_debug(1, CLASS_NAME "(%s):   %s%s", obj->name, found ? "* ":"  ", name);
			count++;
		}
	}

	closedir(d);

	/* Show error message if device directory not found */
	if (dir == NULL) {
		if (id != NULL) {
			log_str("ERROR: " CLASS_NAME "(%s): 1-wire device %s not found", obj->name, id);
		}
		else {
			log_str("ERROR: " CLASS_NAME "(%s): No 1-wire device found", obj->name);
		}
		return NULL;
	}

	return dir;
}


static int read_value(ctx_t *ctx)
{
	FILE *f;
	int crc_ok = 0;
	int value = -1;

	f = fopen(ctx->path, "r");
	if (f == NULL) {
		log_str("ERROR: " CLASS_NAME "(%s): Cannot open %s: %s", ctx->obj->name, ctx->path, strerror(errno));
		return -1;
	}

	while (!feof(f)) {
		char buf[128];

		if (fgets(buf, sizeof(buf), f) != NULL) {
			char *s = buf;
			while (*s >= ' ') {
				s++;
			}
			*s = '\0';

			/* Parse data and extract value */
			log_debug(3, CLASS_NAME "(%s): %s", ctx->obj->name, buf);

			/* Check CRC is ok */
			s = strstr(buf, "crc=");
			if (s != NULL) {
				if (strstr(s, "YES") != NULL) {
					crc_ok = 1;
				}
			}
			else if (crc_ok) {
				s = strstr(buf, "t=");
				if (s != NULL) {
					value = atoi(s+2);
					break;
				}
			}
		}
	}

	fclose(f);

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
				log_str("PANIC: " CLASS_NAME "(%s): Cannot read input queue: %s", ctx->obj->name, strerror(errno));
			}
		}

		log_debug(2, CLASS_NAME "(%s): qin_recv_loop -> %d", ctx->obj->name, msize);

		if (msize > 0) {
			if (mbuf[0] == 0) {
				int value = read_value(ctx);
				if (mq_send(ctx->qout, (char *) &value, sizeof(value), 0) < 0) {
					log_str("PANIC: " CLASS_NAME "(%s): Cannot write output queue: %s", ctx->obj->name, strerror(errno));
					ret = -1;
				}
			}
			else {
				ret = 1;
				log_debug(1, CLASS_NAME "(%s): Leaving input loop", ctx->obj->name);
			}
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
			log_str("PANIC: " CLASS_NAME "(%s): Cannot receive from output queue: %s", ctx->obj->name, strerror(errno));
			return 0;
		}
	}

	log_debug(2, CLASS_NAME "(%s): qout_recv -> %d", ctx->obj->name, msize);

	if (msize == sizeof(int)) {
		int *pvalue = (int *) mbuf;
		int value100 = *pvalue / 100;

                if (value100 != ctx->out->state) {
                        char str[20];

                        ctx->out->state = value100;

                        if (value100 >= 0) {
                                snprintf(str, sizeof(str), "%d.%d", value100/10, value100%10);
                        }
                        else {
                                value100 = (-value100);
                                snprintf(str, sizeof(str), "-%d.%d", value100/10, value100%10);
                        }
                        hk_pad_update_str(ctx->out, str);
                }
	}
	else {
		log_str("PANIC: " CLASS_NAME "(%s): Illegal data received from output queue (%d bytes)", ctx->obj->name, msize);
	}

	return 1;
}


static int create_queue(ctx_t *ctx, char *name, int flags)
{
	char qname[128];
	int mqd;

	snprintf(qname, sizeof(qname), "/hakit-%d-%s-%s", getpid(), ctx->obj->name, name);
	log_debug(2, CLASS_NAME "(%s): create_queue '%s'", ctx->obj->name, qname);

	int qflags = O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL | flags;
	struct mq_attr qattr = {
		.mq_flags   = flags,
		.mq_maxmsg  = 4,
		.mq_msgsize = MSG_MAXSIZE,
	};

	mqd = mq_open(qname, qflags, S_IRUSR | S_IWUSR, &qattr);
	if (mqd < 0) {
		log_str("PANIC: " CLASS_NAME "(%s): Cannot create %sput queue: %s", ctx->obj->name, name, strerror(errno));
		return -1;
	}

	/* Hide message queue from other processes */
	if (mq_unlink(qname) < 0) {
		log_str("PANIC: " CLASS_NAME "(%s): Cannot unlink %sput queue: %s", ctx->obj->name, name, strerror(errno));
		mq_close(mqd);
		return -1;
	}

	return mqd;
}


static int trigger(ctx_t *ctx)
{
	char req = 0;
	if (mq_send(ctx->qin, &req, sizeof(req), 0) < 0) {
		log_str("PANIC: " CLASS_NAME "(%s): Cannot write input queue: %s", ctx->obj->name, strerror(errno));
                return 0;
	}

	return 1;
}


static int _new(hk_obj_t *obj)
{
	ctx_t *ctx;
	int size;

	/* Load device drivers */
	if (access(SYS_W1_DIR, R_OK) != 0) {
                log_str("ERROR: " CLASS_NAME "(%s): 1-wire bus not available", obj->name);
                return -1;
	}

	ctx = malloc(sizeof(ctx_t));
	memset(ctx, 0, sizeof(ctx_t));
	ctx->obj = obj;
	obj->ctx = ctx;
	ctx->qin = -1;
	ctx->qout = -1;

	/* Get sensor id */
	ctx->id = find_id(obj, hk_prop_get(&obj->props, "id"));
	if (ctx->id == NULL) {
		goto failed;
	}

        /* Get period property */
	ctx->period = hk_prop_get_int(&obj->props, "period");

	/* Setup sensor data path */
	size = strlen(SYS_W1_DIR) + strlen(ctx->id) + 16;
	ctx->path = malloc(size);
	snprintf(ctx->path, size, SYS_W1_DIR "/%s/w1_slave", ctx->id);

	/* Create trigger input pad */
	ctx->trig = hk_pad_create(obj, HK_PAD_IN, "trig");

	/* Create output pad */
	ctx->out = hk_pad_create(obj, HK_PAD_IN, "out");
        ctx->out->state = 0x7FFFFFFF;  // Unrealistic value to ensure value will be updated at first trigger

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
		log_str("PANIC: " CLASS_NAME "(%s): Failed to create thread: %s", obj->name, strerror(errno));
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

	if (ctx->path != NULL) {
		free(ctx->path);
		ctx->path = NULL;
	}

	if (ctx->id != NULL) {
		free(ctx->id);
		ctx->id = NULL;
	}

	free(ctx);

	obj->ctx = NULL;

	return -1;
}


static void _start(hk_obj_t *obj)
{
	ctx_t *ctx = obj->ctx;

        trigger(ctx);

        if (ctx->period > 0) {
		ctx->period_tag = sys_timeout(ctx->period, (sys_func_t) trigger, ctx);
        }
}


static void _input(hk_pad_t *pad, char *value)
{
	ctx_t *ctx = pad->obj->ctx;

	/* Ignore falling edge */
	if (value[0] != '0') {
                trigger(ctx);
	}
}


hk_class_t _class = {
	.name = CLASS_NAME,
	.version = VERSION,
	.new = _new,
	.start = _start,
	.input = _input,
};

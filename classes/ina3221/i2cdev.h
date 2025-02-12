/*
 * HAKit - The Home Automation KIT
 * Copyright (C) 2017 Sylvain Giroudon
 *
 * Linux i2cdev i/o primitives
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef __HAKIT_I2CDEV_H__
#define __HAKIT_I2CDEV_H__

#include <stdint.h>

typedef struct {
	char *hdr;
	int fd;
} i2cdev_t;

extern int i2cdev_init(i2cdev_t *i2cdev, char *hdr);
extern int i2cdev_open(i2cdev_t *i2cdev, int num, unsigned char addr);
extern void i2cdev_close(i2cdev_t *i2cdev);

extern int i2cdev_read(i2cdev_t *i2cdev, uint8_t command, uint8_t size, uint8_t *data);
extern int i2cdev_write(i2cdev_t *i2cdev, uint8_t command, uint8_t size, uint8_t *data);

#endif /* __HAKIT_I2CDEV_H__ */

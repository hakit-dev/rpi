/*
 * HAKit - The Home Automation KIT
 * Copyright (C) 2016 Sylvain Giroudon
 *
 * Linux spidev i/o primitives
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef __HAKIT_SPIDEV_H__
#define __HAKIT_SPIDEV_H__

typedef struct {
	char *hdr;
	int fd;
	unsigned int speed_hz;
	unsigned char bits_per_word;
} spidev_t;

extern void spidev_init(spidev_t *spidev, unsigned int speed_hz, unsigned char bits_per_word);
extern int spidev_open(spidev_t *spidev, char *hdr, char *id);
extern void spidev_close(spidev_t *spidev);

extern int spidev_write_read(spidev_t *spidev, unsigned char *buf, int size);

#endif /* __HAKIT_SPIDEV_H__ */

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

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "log.h"
#include "spidev.h"


void spidev_init(spidev_t *spidev, unsigned int speed_hz, unsigned char bits_per_word)
{
	spidev->hdr = NULL;
	spidev->fd = -1;
	spidev->speed_hz = speed_hz;
	spidev->bits_per_word = bits_per_word;
}


int spidev_open(spidev_t *spidev, char *hdr, char *id)
{
	char devname[strlen(id) + 16];
	unsigned char mode = SPI_MODE_0;
	int ret;

	spidev->hdr = strdup(hdr);
	spidev->fd = -1;

	snprintf(devname, sizeof(devname), "/dev/spidev%s", id);
	log_debug(1, "%sOpening SPI device %s", hdr, devname);

	spidev->fd = open(devname, O_RDWR);
	if (spidev->fd < 0) {
		log_str("PANIC: %sCannot open %s: %s", hdr, devname, strerror(errno));
		return -1;
	}

	log_debug(2, "%sspidev_open => fd=%d", hdr, spidev->fd);

	ret = ioctl(spidev->fd, SPI_IOC_WR_MODE, &mode);
	if (ret < 0){
		log_str("PANIC: %sCannot setup %s write mode: %s", hdr, devname, strerror(errno));
		goto failed;
	}

	ret = ioctl (spidev->fd, SPI_IOC_RD_MODE, &mode);
	if (ret < 0){
		log_str("PANIC: %sCannot setup %s read mode: %s", hdr, devname, strerror(errno));
		goto failed;
	}

	ret = ioctl (spidev->fd, SPI_IOC_WR_BITS_PER_WORD, &spidev->bits_per_word);
	if (ret < 0){
		log_str("PANIC: %sCannot setup %s write bits per word: %s", hdr, devname, strerror(errno));
		goto failed;
	}

	ret = ioctl (spidev->fd, SPI_IOC_RD_BITS_PER_WORD, &spidev->bits_per_word);
	if (ret < 0){
		log_str("PANIC: %sCannot setup %s read bits per word: %s", hdr, devname, strerror(errno));
		goto failed;
	}

	ret = ioctl (spidev->fd, SPI_IOC_WR_MAX_SPEED_HZ, &spidev->speed_hz);
	if (ret < 0){
		log_str("PANIC: %sCannot setup %s write speed: %s", hdr, devname, strerror(errno));
		goto failed;
	}

	ret = ioctl (spidev->fd, SPI_IOC_RD_MAX_SPEED_HZ, &spidev->speed_hz);
	if (ret < 0){
		log_str("PANIC: %sCannot setup %s read speed: %s", hdr, devname, strerror(errno));
		goto failed;
	}

	return spidev->fd;

failed:
	spidev_close(spidev);

	return -1;
}


void spidev_close(spidev_t *spidev)
{
	if (spidev->fd >= 0) {
		close(spidev->fd);
		spidev->fd = -1;
	}

	if (spidev->hdr != NULL) {
		free(spidev->hdr);
		spidev->hdr = NULL;
	}
}


int spidev_write_read(spidev_t *spidev, unsigned char *buf, int size)
{
	struct spi_ioc_transfer spi = {
                .tx_buf = (unsigned long) buf,
                .rx_buf = (unsigned long) buf,
                .len = size,
                .delay_usecs = 0,
                .speed_hz = spidev->speed_hz,
                .bits_per_word = spidev->bits_per_word,
                .cs_change = 0,
        };
	int ret;

	log_debug(2, "%sspidev_write_read fd=%d size=%d", spidev->hdr, spidev->fd, size);

	ret = ioctl(spidev->fd, SPI_IOC_MESSAGE(1), &spi);
	if (ret < 0) {
		log_str("ERROR: %sSPI transfer error: %s", spidev->hdr, strerror(errno));
	}

	return ret;
}

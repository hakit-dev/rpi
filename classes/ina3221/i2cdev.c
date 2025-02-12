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

// Enable I2C in file /boot/config.txt:
//   dtparam=i2c_arm=on 
//   dtparam=i2c1=on
//
// Load I2C drivers:
//   # modprobe i2c-dev
//   # modprobe i2c-bcm2708
//
// Install I2C tools:
//   # apt-get install i2c-tools
//
// Probe I2C devices:
//   # i2cdetect -y 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#ifndef I2C_FUNC_I2C

#include <linux/i2c.h>

//
// Compatibility primitives for old i2c-dev library
//

static inline __s32 i2c_smbus_access(int file, char read_write, __u8 command, 
                                     int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;
	return ioctl(file,I2C_SMBUS,&args);
}

static inline __s32 i2c_smbus_read_byte_data(int file, __u8 command)
{
	union i2c_smbus_data data;
	if (i2c_smbus_access(file,I2C_SMBUS_READ,command,
	                     I2C_SMBUS_BYTE_DATA,&data))
		return -1;
	else
		return 0x0FF & data.byte;
}

static inline __s32 i2c_smbus_write_byte_data(int file, __u8 command, 
                                              __u8 value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
	                        I2C_SMBUS_BYTE_DATA, &data);
}

static inline __s32 i2c_smbus_read_i2c_block_data(int file, __u8 command,
                                                  __u8 length, __u8 *values)
{
	union i2c_smbus_data data;
	int i;

	if (length > 32)
		length = 32;
	data.block[0] = length;
	if (i2c_smbus_access(file,I2C_SMBUS_READ,command,
	                     length == 32 ? I2C_SMBUS_I2C_BLOCK_BROKEN :
	                      I2C_SMBUS_I2C_BLOCK_DATA,&data))
		return -1;
	else {
		for (i = 1; i <= data.block[0]; i++)
			values[i-1] = data.block[i];
		return data.block[0];
	}
}

static inline __s32 i2c_smbus_write_i2c_block_data(int file, __u8 command,
                                                   __u8 length, __u8 *values)
{
	union i2c_smbus_data data;
	int i;

	if (length > 32)
		length = 32;
	data.block[0] = length;
        for (i = 1; i <= length; i++)
                data.block[i] = values[i-1];
	if (i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
	                     length == 32 ? I2C_SMBUS_I2C_BLOCK_BROKEN :
	                      I2C_SMBUS_I2C_BLOCK_DATA,&data))
		return -1;
	else {
		return data.block[0];
	}
}

#endif

#include "log.h"
#include "i2cdev.h"

#define SYS_I2C_CLASS "/sys/class/i2c-dev/"

int i2cdev_init(i2cdev_t *i2cdev, char *hdr)
{
	/* Load device drivers */
	if (access(SYS_I2C_CLASS, R_OK) != 0) {
		int status = system("modprobe i2c-dev");
		if (status != 0) {
			log_str("ERROR: %sFailed to init i2c device driver: %s", hdr, strerror(errno));
			return -1;
		}
	}

	i2cdev->hdr = strdup(hdr);
	i2cdev->fd = -1;

        return 0;
}


int i2cdev_open(i2cdev_t *i2cdev, int num, unsigned char addr)
{
	char devname[16];

	snprintf(devname, sizeof(devname), "/dev/i2c-%d", num);
	log_debug(1, "%sOpening I2C device %s", i2cdev->hdr, devname);

	i2cdev->fd = open(devname, O_RDWR);
	if (i2cdev->fd < 0) {
		log_str("ERROR: %sCannot open %s: %s", i2cdev->hdr, devname, strerror(errno));
                goto failed;
	}

	log_debug(3, "%si2cdev_open => fd=%d", i2cdev->hdr, i2cdev->fd);

	// Select device address
	if (ioctl(i2cdev->fd, I2C_SLAVE, addr) < 0) {
		log_str("ERROR: %sCould not select I2C address 0x%02X on %s: %s\n", i2cdev->hdr, devname, addr, devname, strerror(errno));
                goto failed;
	}

	return i2cdev->fd;

failed:
        i2cdev_close(i2cdev);

        return -1;
}


void i2cdev_close(i2cdev_t *i2cdev)
{
	if (i2cdev->fd >= 0) {
		close(i2cdev->fd);
		i2cdev->fd = -1;
	}
}


int i2cdev_read(i2cdev_t *i2cdev, uint8_t command, uint8_t size, uint8_t *data)
{
	int ret = i2c_smbus_read_i2c_block_data(i2cdev->fd, command, size, data);
	if (ret < 0) {
                log_str("ERROR: %sFailed to read data at 0x%02X: %s", i2cdev->hdr, command, strerror(errno));
	}

	return ret;
}


int i2cdev_write(i2cdev_t *i2cdev, uint8_t command, uint8_t size, uint8_t *data)
{
	int ret = i2c_smbus_write_i2c_block_data(i2cdev->fd, command, size, data);
	if (ret < 0) {
		log_str("ERROR: %sFailed to write data at 0x%02X: %s", i2cdev->hdr, command, strerror(errno));
	}

        return ret;
}

/*
 * HAKit - The Home Automation KIT
 * Copyright (C) 2025 Sylvain Giroudon
 *
 * INA3221 calibration and measurement settings
 * Ported from AdaFruit Python library:
 * https://github.com/adafruit/Adafruit_CircuitPython_INA3221/blob/main/adafruit_ina3221.py
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef __INA3221_H__
#define __INA3221_H__

#include <stdint.h>

#define INA3221_I2C_MIN_ADDR 0x40
#define INA3221_I2C_MAX_ADDR 0x43
#define INA3221_COMMAND_BIT  0x80

#define INA3221_NUM_CHANNELS  3

#define INA3221_REG_CONFIG      0x00
#define   INA3221_CONFIG_RST         0x8000
#define   INA3221_CONFIG_CH_EN(ch)   (0x4000 >> (ch-1))
#define   INA3221_CONFIG_CH_ALL      0x7000
#define   INA3221_CONFIG_AVG_1       (0 << 9)
#define   INA3221_CONFIG_AVG_4       (1 << 9)
#define   INA3221_CONFIG_AVG_16      (2 << 9)
#define   INA3221_CONFIG_AVG_64      (3 << 9)
#define   INA3221_CONFIG_AVG_128     (4 << 9)
#define   INA3221_CONFIG_AVG_256     (5 << 9)
#define   INA3221_CONFIG_AVG_512     (6 << 9)
#define   INA3221_CONFIG_AVG_1024    (7 << 9)
#define   INA3221_CONFIG_VBUS_140    (0 << 6)
#define   INA3221_CONFIG_VBUS_204    (1 << 6)
#define   INA3221_CONFIG_VBUS_332    (2 << 6)
#define   INA3221_CONFIG_VBUS_588    (3 << 6)
#define   INA3221_CONFIG_VBUS_1100   (4 << 6)
#define   INA3221_CONFIG_VBUS_2116   (5 << 6)
#define   INA3221_CONFIG_VBUS_4156   (6 << 6)
#define   INA3221_CONFIG_VBUS_8244   (7 << 6)
#define   INA3221_CONFIG_VSH_140     (0 << 3)
#define   INA3221_CONFIG_VSH_204     (1 << 3)
#define   INA3221_CONFIG_VSH_332     (2 << 3)
#define   INA3221_CONFIG_VSH_588     (3 << 3)
#define   INA3221_CONFIG_VSH_1100    (4 << 3)
#define   INA3221_CONFIG_VSH_2116    (5 << 3)
#define   INA3221_CONFIG_VSH_4156    (6 << 3)
#define   INA3221_CONFIG_VSH_8244    (7 << 3)
#define   INA3221_CONFIG_MODE_POWERDOWN  0
#define   INA3221_CONFIG_MODE_SHUNT      1
#define   INA3221_CONFIG_MODE_BUS        2
#define   INA3221_CONFIG_MODE_CONTINUOUS 4

#define INA3221_REG_SHUNT1            0x01
#define INA3221_REG_BUS1              0x02
#define INA3221_REG_SHUNT2            0x03
#define INA3221_REG_BUS2              0x04
#define INA3221_REG_SHUNT3            0x05
#define INA3221_REG_BUS3              0x06
#define INA3221_REG_CRIT1             0x07
#define INA3221_REG_WARN1             0x08
#define INA3221_REG_CRIT2             0x09
#define INA3221_REG_WARN2             0x0A
#define INA3221_REG_CRIT3             0x0B
#define INA3221_REG_WARN3             0x0C
#define INA3221_REG_SHUNT_SUM         0x0D
#define INA3221_REG_CRIT_SUM          0x0E

#define INA3221_REG_MANUFACTURER_ID   0xFE
#define   INA3221_MANUFACTURER_ID     0x5449
#define INA3221_REG_DIE_ID            0xFF
#define   INA3221_DIE_ID              0x3210


typedef struct {
        float current_lsb;
        float power_lsb;
        uint16_t cal_value;
        uint16_t config;
} ina219_t;


extern void ina219_set_calibration_32V_2A(ina219_t *chip);
extern void ina219_set_calibration_32V_1A(ina219_t *chip);
extern void ina219_set_calibration_16V_400mA(ina219_t *chip);
extern void ina219_set_calibration_16V_5A(ina219_t *chip);

extern void ina219_set_badc_res(ina219_t *chip, uint16_t res);
extern void ina219_set_sadc_res(ina219_t *chip, uint16_t res);


static inline float ina219_get_current(ina219_t *chip, int16_t raw_current)
{
        return raw_current * chip->current_lsb;
}

static inline float ina219_get_power(ina219_t *chip, int16_t raw_power)
{
        return raw_power * chip->power_lsb;
}

#endif /* __INA3221_H__ */

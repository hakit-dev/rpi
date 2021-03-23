/*
 * HAKit - The Home Automation KIT
 * Copyright (C) 2021 Sylvain Giroudon
 *
 * INA219 calibration and measurement settings
 * Ported from AdaFruit Python library:
 * https://circuitpython.readthedocs.io/projects/ina219/en/latest/_modules/adafruit_ina219.html
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef __INA219_H__
#define __INA219_H__

#include <stdint.h>

#define INA219_I2C_MIN_ADDR 0x40
#define INA219_I2C_MAX_ADDR 0x4F
#define INA219_COMMAND_BIT  0x80

#define INA219_CONFIG      0x00
#define INA219_CONFIG_RST         0x8000

#define INA219_CONFIG_RANGE_16V   0x0000
#define INA219_CONFIG_RANGE_32V   0x2000

#define INA219_CONFIG_GAIN_DIV1_40MV   0x0000
#define INA219_CONFIG_GAIN_DIV2_80MV   0x0800
#define INA219_CONFIG_GAIN_DIV4_160MV  0x1000
#define INA219_CONFIG_GAIN_DIV8_320MV  0x1800

#define INA219_CONFIG_BADC(__value__) ((__value__) << 7)
#define INA219_CONFIG_SADC(__value__) ((__value__) << 3)
#define INA219_CONFIG_ADCRES_9BIT_1S   0x0  //  9bit,   1 sample,     84us
#define INA219_CONFIG_ADCRES_10BIT_1S  0x1  // 10bit,   1 sample,    148us
#define INA219_CONFIG_ADCRES_11BIT_1S  0x2  // 11 bit,  1 sample,    276us
#define INA219_CONFIG_ADCRES_12BIT_1S  0x3  // 12 bit,  1 sample,    532us
#define INA219_CONFIG_ADCRES_12BIT_2S  0x9  // 12 bit,  2 samples,  1.06ms
#define INA219_CONFIG_ADCRES_12BIT_4S  0xA  // 12 bit,  4 samples,  2.13ms
#define INA219_CONFIG_ADCRES_12BIT_8S  0xB  // 12bit,   8 samples,  4.26ms
#define INA219_CONFIG_ADCRES_12BIT_16S 0xC  // 12bit,  16 samples,  8.51ms
#define INA219_CONFIG_ADCRES_12BIT_32S 0xD  // 12bit,  32 samples, 17.02ms
#define INA219_CONFIG_ADCRES_12BIT_64S 0xE  // 12bit,  64 samples, 34.05ms
#define INA219_CONFIG_ADCRES_12BIT_128S 0xF // 12bit, 128 samples, 68.10ms
#define INA219_CONFIG_ADCRES_MASK 0xF

#define INA219_CONFIG_MODE_POWERDOWN            0x0000
#define INA219_CONFIG_MODE_SVOLT_TRIGGERED      0x0001
#define INA219_CONFIG_MODE_BVOLT_TRIGGERED      0x0002
#define INA219_CONFIG_MODE_SANDBVOLT_TRIGGERED  0x0003
#define INA219_CONFIG_MODE_ADCOFF               0x0004
#define INA219_CONFIG_MODE_SVOLT_CONTINUOUS     0x0005
#define INA219_CONFIG_MODE_BVOLT_CONTINUOUS     0x0006
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x0007

#define INA219_SHUNT_VOLTAGE 0x01
#define INA219_BUS_VOLTAGE   0x02
#define INA219_POWER         0x03
#define INA219_CURRENT       0x04
#define INA219_CALIBRATION   0x05

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

#endif /* __INA219_H__ */

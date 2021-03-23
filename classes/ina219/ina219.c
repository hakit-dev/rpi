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

#include "ina219.h"


void ina219_set_calibration_32V_2A(ina219_t *chip)
{
        /*
        Configures to INA219 to be able to measure up to 32V and 2A of current. Counter
        overflow occurs at 3.2A.

        ..note :: These calculations assume a 0.1 shunt ohm resistor is present

        By default we use a pretty huge range for the input voltage,
        which probably isn't the most appropriate choice for system
        that don't use a lot of power.  But all of the calculations
        are shown below if you want to change the settings.  You will
        also need to change any relevant register settings, such as
        setting the VBUS_MAX to 16V instead of 32V, etc.

        VBUS_MAX = 32V             (Assumes 32V, can also be set to 16V)
        VSHUNT_MAX = 0.32          (Assumes Gain 8, 320mV, can also be 0.16, 0.08, 0.04)
        RSHUNT = 0.1               (Resistor value in ohms)
        */

        // 1. Determine max possible current
        // MaxPossible_I = VSHUNT_MAX / RSHUNT
        // MaxPossible_I = 3.2A

        // 2. Determine max expected current
        // MaxExpected_I = 2.0A

        // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
        // MinimumLSB = MaxExpected_I/32767
        // MinimumLSB = 0.000061              (61uA per bit)
        // MaximumLSB = MaxExpected_I/4096
        // MaximumLSB = 0,000488              (488uA per bit)

        // 4. Choose an LSB between the min and max values
        //    (Preferrably a roundish number close to MinLSB)
        // CurrentLSB = 0.0001 (100uA per bit)
        chip->current_lsb = 0.1;  // Current LSB = 100uA per bit

        // 5. Compute the calibration register
        // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
        // Cal = 4096 (0x1000)
        chip->cal_value = 4096;

        // 6. Calculate the power LSB
        // PowerLSB = 20 * CurrentLSB
        // PowerLSB = 0.002 (2mW per bit)
        chip->power_lsb = 0.002;  // Power LSB = 2mW per bit

        // 7. Compute the maximum current and shunt voltage values before overflow
        //
        // Max_Current = Current_LSB * 32767
        // Max_Current = 3.2767A before overflow
        //
        // If Max_Current > Max_Possible_I then
        //    Max_Current_Before_Overflow = MaxPossible_I
        // Else
        //    Max_Current_Before_Overflow = Max_Current
        // End If
        //
        // Max_ShuntVoltage = Max_Current_Before_Overflow * RSHUNT
        // Max_ShuntVoltage = 0.32V
        //
        // If Max_ShuntVoltage >= VSHUNT_MAX
        //    Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
        // Else
        //    Max_ShuntVoltage_Before_Overflow = Max_ShuntVoltage
        // End If

        // 8. Compute the Maximum Power
        // MaximumPower = Max_Current_Before_Overflow * VBUS_MAX
        // MaximumPower = 3.2 * 32V
        // MaximumPower = 102.4W

        // Set Config register to take into account the settings above
        chip->config = INA219_CONFIG_RANGE_32V |
                INA219_CONFIG_GAIN_DIV8_320MV |
                INA219_CONFIG_BADC(INA219_CONFIG_ADCRES_12BIT_1S) |
                INA219_CONFIG_SADC(INA219_CONFIG_ADCRES_12BIT_1S) |
                INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
}


void ina219_set_calibration_32V_1A(ina219_t *chip)
{
        /* 
           Configures to INA219 to be able to measure up to 32V and 1A of current. Counter overflow
           occurs at 1.3A.

           .. note:: These calculations assume a 0.1 ohm shunt resistor is present

           By default we use a pretty huge range for the input voltage,
           which probably isn't the most appropriate choice for system
           that don't use a lot of power.  But all of the calculations
           are shown below if you want to change the settings.  You will
           also need to change any relevant register settings, such as
           setting the VBUS_MAX to 16V instead of 32V, etc.

           VBUS_MAX = 32V        (Assumes 32V, can also be set to 16V)
           VSHUNT_MAX = 0.32    (Assumes Gain 8, 320mV, can also be 0.16, 0.08, 0.04)
           RSHUNT = 0.1            (Resistor value in ohms)
        */

        // 1. Determine max possible current
        // MaxPossible_I = VSHUNT_MAX / RSHUNT
        // MaxPossible_I = 3.2A

        // 2. Determine max expected current
        // MaxExpected_I = 1.0A

        // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
        // MinimumLSB = MaxExpected_I/32767
        // MinimumLSB = 0.0000305             (30.5uA per bit)
        // MaximumLSB = MaxExpected_I/4096
        // MaximumLSB = 0.000244              (244uA per bit)

        // 4. Choose an LSB between the min and max values
        //    (Preferrably a roundish number close to MinLSB)
        // CurrentLSB = 0.0000400 (40uA per bit)
        chip->current_lsb = 0.04; // In milliamps

        // 5. Compute the calibration register
        // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
        // Cal = 10240 (0x2800)
        chip->cal_value = 10240;

        // 6. Calculate the power LSB
        // PowerLSB = 20 * CurrentLSB
        // PowerLSB = 0.0008 (800uW per bit)
        chip->power_lsb = 0.0008;

        // 7. Compute the maximum current and shunt voltage values before overflow
        //
        // Max_Current = Current_LSB * 32767
        // Max_Current = 1.31068A before overflow
        //
        // If Max_Current > Max_Possible_I then
        //    Max_Current_Before_Overflow = MaxPossible_I
        // Else
        //    Max_Current_Before_Overflow = Max_Current
        // End If
        //
        // ... In this case, we're good though since Max_Current is less than MaxPossible_I
        //
        // Max_ShuntVoltage = Max_Current_Before_Overflow * RSHUNT
        // Max_ShuntVoltage = 0.131068V
        //
        // If Max_ShuntVoltage >= VSHUNT_MAX
        //    Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
        // Else
        //    Max_ShuntVoltage_Before_Overflow = Max_ShuntVoltage
        // End If

        // 8. Compute the Maximum Power
        // MaximumPower = Max_Current_Before_Overflow * VBUS_MAX
        // MaximumPower = 1.31068 * 32V
        // MaximumPower = 41.94176W

        // Set Config register to take into account the settings above
        chip->config = INA219_CONFIG_RANGE_32V |
                INA219_CONFIG_GAIN_DIV8_320MV |
                INA219_CONFIG_BADC(INA219_CONFIG_ADCRES_12BIT_1S) |
                INA219_CONFIG_SADC(INA219_CONFIG_ADCRES_12BIT_1S) |
                INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
}


void ina219_set_calibration_16V_400mA(ina219_t *chip)
{
        /*
          Configures to INA219 to be able to measure up to 16V and 400mA of current. Counter
          overflow occurs at 1.6A.

          .. note:: These calculations assume a 0.1 ohm shunt resistor is present"""

          Calibration which uses the highest precision for
          current measurement (0.1mA), at the expense of
          only supporting 16V at 400mA max.

          VBUS_MAX = 16V
          VSHUNT_MAX = 0.04          (Assumes Gain 1, 40mV)
          RSHUNT = 0.1               (Resistor value in ohms)
        */

        // 1. Determine max possible current
        // MaxPossible_I = VSHUNT_MAX / RSHUNT
        // MaxPossible_I = 0.4A

        // 2. Determine max expected current
        // MaxExpected_I = 0.4A

        // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
        // MinimumLSB = MaxExpected_I/32767
        // MinimumLSB = 0.0000122              (12uA per bit)
        // MaximumLSB = MaxExpected_I/4096
        // MaximumLSB = 0.0000977              (98uA per bit)

        // 4. Choose an LSB between the min and max values
        //    (Preferrably a roundish number close to MinLSB)
        // CurrentLSB = 0.00005 (50uA per bit)
        chip->current_lsb = 0.05;  // in milliamps

        // 5. Compute the calibration register
        // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
        // Cal = 8192 (0x2000)
        chip->cal_value = 8192;

        // 6. Calculate the power LSB
        // PowerLSB = 20 * CurrentLSB
        // PowerLSB = 0.001 (1mW per bit)
        chip->power_lsb = 0.001;

        // 7. Compute the maximum current and shunt voltage values before overflow
        //
        // Max_Current = Current_LSB * 32767
        // Max_Current = 1.63835A before overflow
        //
        // If Max_Current > Max_Possible_I then
        //    Max_Current_Before_Overflow = MaxPossible_I
        // Else
        //    Max_Current_Before_Overflow = Max_Current
        // End If
        //
        // Max_Current_Before_Overflow = MaxPossible_I
        // Max_Current_Before_Overflow = 0.4
        //
        // Max_ShuntVoltage = Max_Current_Before_Overflow * RSHUNT
        // Max_ShuntVoltage = 0.04V
        //
        // If Max_ShuntVoltage >= VSHUNT_MAX
        //    Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
        // Else
        //    Max_ShuntVoltage_Before_Overflow = Max_ShuntVoltage
        // End If
        //
        // Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
        // Max_ShuntVoltage_Before_Overflow = 0.04V

        // 8. Compute the Maximum Power
        // MaximumPower = Max_Current_Before_Overflow * VBUS_MAX
        // MaximumPower = 0.4 * 16V
        // MaximumPower = 6.4W

        // Set Config register to take into account the settings above
        chip->config = INA219_CONFIG_RANGE_16V |
                INA219_CONFIG_GAIN_DIV1_40MV |
                INA219_CONFIG_BADC(INA219_CONFIG_ADCRES_12BIT_1S) |
                INA219_CONFIG_SADC(INA219_CONFIG_ADCRES_12BIT_1S) |
                INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
}


void ina219_set_calibration_16V_5A(ina219_t *chip)
{
        /*
          Configures to INA219 to be able to measure up to 16V and 5000mA of current. Counter
          overflow occurs at 8.0A.

          .. note:: These calculations assume a 0.02 ohm shunt resistor is present"""

          Calibration which uses the highest precision for
          current measurement (0.1mA), at the expense of
          only supporting 16V at 5000mA max.

          VBUS_MAX = 16V
          VSHUNT_MAX = 0.16          (Assumes Gain 3, 160mV)
          RSHUNT = 0.02              (Resistor value in ohms)
        */

        // 1. Determine max possible current
        // MaxPossible_I = VSHUNT_MAX / RSHUNT
        // MaxPossible_I = 8.0A

        // 2. Determine max expected current
        // MaxExpected_I = 5.0A

        // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
        // MinimumLSB = MaxExpected_I/32767
        // MinimumLSB = 0.0001529              (uA per bit)
        // MaximumLSB = MaxExpected_I/4096
        // MaximumLSB = 0.0012207              (uA per bit)

        // 4. Choose an LSB between the min and max values
        //    (Preferrably a roundish number close to MinLSB)
        // CurrentLSB = 0.00016 (uA per bit)
        chip->current_lsb = 0.1524;  // in milliamps

        // 5. Compute the calibration register
        // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
        // Cal = 13434 (0x347a)
        chip->cal_value = 13434;

        // 6. Calculate the power LSB
        // PowerLSB = 20 * CurrentLSB
        // PowerLSB = 0.003 (3.048mW per bit)
        chip->power_lsb = 0.003048;

        // 7. Compute the maximum current and shunt voltage values before overflow

        // 8. Compute the Maximum Power

        // Set Config register to take into account the settings above
        chip->config = INA219_CONFIG_RANGE_16V |
                INA219_CONFIG_GAIN_DIV4_160MV |
                INA219_CONFIG_BADC(INA219_CONFIG_ADCRES_12BIT_1S) |
                INA219_CONFIG_SADC(INA219_CONFIG_ADCRES_12BIT_1S) |
                INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
}


void ina219_set_badc_res(ina219_t *chip, uint16_t res)
{
        chip->config = (chip->config & ~INA219_CONFIG_BADC(INA219_CONFIG_ADCRES_MASK)) | INA219_CONFIG_BADC(res);
}


void ina219_set_sadc_res(ina219_t *chip, uint16_t res)
{
        chip->config = (chip->config & ~INA219_CONFIG_SADC(INA219_CONFIG_ADCRES_MASK)) | INA219_CONFIG_SADC(res);
}

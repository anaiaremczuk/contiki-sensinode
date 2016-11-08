/*
 * Copyright (c) 2011, George Oikonomou - <oikonomou@users.sourceforge.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *         BMP 280 sensor module for TI SmartRF05EB devices.
 *
 * \author
 *         George Oikonomou - <oikonomou@users.sourceforge.net>
 */
#include "sfr-bits.h"
#include "cc253x.h"
#include "bmp280-sensor.h"
#include "dev/hal_i2c.h"
#include "dev/bmp280.h"
#include "debug.h"

#if BMP_SENSOR_ON

struct bmp280_calib_param_t bmp280cal;

/*!
 *	@brief Reads actual pressure from uncompensated pressure
 *	and returns the value in Pascal(Pa)
 *	@note Output value of "96386" equals 96386 Pa =
 *	963.86 hPa = 963.86 millibar
 *
 *
 *
 *
 *  @param  v_uncomp_pressure_s32: value of uncompensated pressure
 *
 *
 *
 *  @return Returns the Actual pressure out put as s32
 *
 */

u32 bmp280_compensate_pressure_int32(s32 v_uncomp_pressure_s32)
{
	s32 v_x1_u32r = BMP280_INIT_VALUE;
	s32 v_x2_u32r = BMP280_INIT_VALUE;
	u32 v_pressure_u32 = BMP280_INIT_VALUE;
	/* calculate x1*/
	v_x1_u32r = (((s32)bmp280cal.t_fine)
			>> BMP280_SHIFT_BIT_POSITION_BY_01_BIT) - (s32)64000;
	/* calculate x2*/
	v_x2_u32r = (((v_x1_u32r >> BMP280_SHIFT_BIT_POSITION_BY_02_BITS)
			* (v_x1_u32r >> BMP280_SHIFT_BIT_POSITION_BY_02_BITS))
			>> BMP280_SHIFT_BIT_POSITION_BY_11_BITS)
			* ((s32)bmp280cal.dig_P6);
	v_x2_u32r = v_x2_u32r + ((v_x1_u32r *
			((s32)bmp280cal.dig_P5))
			<< BMP280_SHIFT_BIT_POSITION_BY_01_BIT);
	v_x2_u32r = (v_x2_u32r >> BMP280_SHIFT_BIT_POSITION_BY_02_BITS)
			+ (((s32)bmp280cal.dig_P4)
			<< BMP280_SHIFT_BIT_POSITION_BY_16_BITS);
	/* calculate x1*/
	v_x1_u32r = (((bmp280cal.dig_P3
			* (((v_x1_u32r
			>> BMP280_SHIFT_BIT_POSITION_BY_02_BITS) * (v_x1_u32r
			>> BMP280_SHIFT_BIT_POSITION_BY_02_BITS))
			>> BMP280_SHIFT_BIT_POSITION_BY_13_BITS))
			>> BMP280_SHIFT_BIT_POSITION_BY_03_BITS)
			+ ((((s32)bmp280cal.dig_P2)
			* v_x1_u32r)
			>> BMP280_SHIFT_BIT_POSITION_BY_01_BIT))
			>> BMP280_SHIFT_BIT_POSITION_BY_18_BITS;
	v_x1_u32r = ((((32768 + v_x1_u32r))
			* ((s32)bmp280cal.dig_P1))
			>> BMP280_SHIFT_BIT_POSITION_BY_15_BITS);
	/* calculate pressure*/
	v_pressure_u32 = (((u32)(((s32)1048576) - v_uncomp_pressure_s32)
			- (v_x2_u32r >> BMP280_SHIFT_BIT_POSITION_BY_12_BITS)))
			* 3125;
	/* check overflow*/
	if (v_pressure_u32 < 0x80000000)
		/* Avoid exception caused by division by zero */
		if (v_x1_u32r != BMP280_INIT_VALUE)
			v_pressure_u32 = (v_pressure_u32
					<< BMP280_SHIFT_BIT_POSITION_BY_01_BIT)
					/ ((u32)v_x1_u32r);
		else
			return BMP280_INVALID_DATA;
	else
	/* Avoid exception caused by division by zero */
	if (v_x1_u32r != BMP280_INIT_VALUE)
		v_pressure_u32 = (v_pressure_u32 / (u32)v_x1_u32r) * 2;
	else
		return BMP280_INVALID_DATA;
	/* calculate x1*/
	v_x1_u32r = (((s32)bmp280cal.dig_P9) * ((s32)(
			((v_pressure_u32
			>> BMP280_SHIFT_BIT_POSITION_BY_03_BITS)
			* (v_pressure_u32
			>> BMP280_SHIFT_BIT_POSITION_BY_03_BITS))
			>> BMP280_SHIFT_BIT_POSITION_BY_13_BITS)))
			>> BMP280_SHIFT_BIT_POSITION_BY_12_BITS;
	/* calculate x2*/
	v_x2_u32r = (((s32)(v_pressure_u32 >>
			BMP280_SHIFT_BIT_POSITION_BY_02_BITS))
			* ((s32)bmp280cal.dig_P8))
			>> BMP280_SHIFT_BIT_POSITION_BY_13_BITS;
	/* calculate true pressure*/
	v_pressure_u32 = (u32)((s32)v_pressure_u32 + ((v_x1_u32r + v_x2_u32r
			+ bmp280cal.dig_P7)
			>> BMP280_SHIFT_BIT_POSITION_BY_04_BITS));

	return v_pressure_u32;
}

/*!
 *	@brief Reads actual temperature
 *	from uncompensated temperature
 *	@note Returns the value in 0.01 degree Centigrade
 *	@note Output value of "5123" equals 51.23 DegC.
 *
 *
 *
 *  @param v_uncomp_temperature_s32 : value of uncompensated temperature
 *
 *
 *
 *  @return Actual temperature output as s32
 *
 */

s32 bmp280_compensate_temperature_int32(s32 v_uncomp_temperature_s32)
{
	s32 v_x1_u32r = BMP280_INIT_VALUE;
	s32 v_x2_u32r = BMP280_INIT_VALUE;
	s32 temperature = BMP280_INIT_VALUE;
	/* calculate true temperature*/
	/*calculate x1*/
	v_x1_u32r = ((((v_uncomp_temperature_s32
			>> BMP280_SHIFT_BIT_POSITION_BY_03_BITS)
			- ((s32)bmp280cal.dig_T1
			<< BMP280_SHIFT_BIT_POSITION_BY_01_BIT)))
			* ((s32)bmp280cal.dig_T2))
			>> BMP280_SHIFT_BIT_POSITION_BY_11_BITS;
	/*calculate x2*/
	v_x2_u32r = (((((v_uncomp_temperature_s32
			>> BMP280_SHIFT_BIT_POSITION_BY_04_BITS)
			- ((s32)bmp280cal.dig_T1))
			* ((v_uncomp_temperature_s32
			>> BMP280_SHIFT_BIT_POSITION_BY_04_BITS)
			- ((s32)bmp280cal.dig_T1)))
			>> BMP280_SHIFT_BIT_POSITION_BY_12_BITS)
			* ((s32)bmp280cal.dig_T3))
			>> BMP280_SHIFT_BIT_POSITION_BY_14_BITS;
	/*calculate t_fine*/
	bmp280cal.t_fine = v_x1_u32r + v_x2_u32r;
	/*calculate temperature*/
	temperature = (bmp280cal.t_fine * 5 + 128)
			>> BMP280_SHIFT_BIT_POSITION_BY_08_BITS;

	return temperature;
}
/*---------------------------------------------------------------------------*/

static int
value(int type)
{
  int16_t reading;
  uint8_t buf[3];
  static int32_t lastReadPressure = 0xFFFFFFFF;

  switch(type)
  {
	  case BMP_SENSOR_TYPE_TEMP:
	  {
		  HalI2CReceive(BMP280_I2C_ADDRESS2,BMP280_TEMPERATURE_MSB_REG,buf,3);
		  return (int16_t)bmp280_compensate_temperature_int32(((int32_t)buf[0])<<12 | ((int32_t)buf[1])<<4 | ((int32_t)buf[0])>>4);
		  break;
	  }
	  case BMP_SENSOR_TYPE_PRESS_hPa:
	  {
		  HalI2CReceive(BMP280_I2C_ADDRESS2,BMP280_PRESSURE_MSB_REG,buf,3);
		  lastReadPressure = (bmp280_compensate_pressure_int32(((int32_t)buf[0])<<12 | ((int32_t)buf[1])<<4 | ((int32_t)buf[0])>>4));
		  return (int)(lastReadPressure/100);
		  break;
	  }
	  case BMP_SENSOR_TYPE_LAST_READ_PRESS_Pa_MSB:
	  {
		  return (int)(lastReadPressure>>16);
		  break;
	  }
	  case BMP_SENSOR_TYPE_LAST_READ_PRESS_Pa_LSB:
	  {
		  return (int)(lastReadPressure&0xFFFF);
		  break;
	  }
  default:
  {

	  /* If the sensor is not present or disabled in conf, return -1 */
	  return -1;

  }
  }

  return (uint16_t)reading;
}
/*---------------------------------------------------------------------------*/
static int
status(int type)
{
  return 1;
}
/*---------------------------------------------------------------------------*/

u8 a_data_u8[BMP280_CALIB_DATA_SIZE] = {BMP280_INIT_VALUE,
		BMP280_INIT_VALUE, BMP280_INIT_VALUE, BMP280_INIT_VALUE,
		BMP280_INIT_VALUE, BMP280_INIT_VALUE, BMP280_INIT_VALUE,
		BMP280_INIT_VALUE, BMP280_INIT_VALUE, BMP280_INIT_VALUE,
		BMP280_INIT_VALUE, BMP280_INIT_VALUE, BMP280_INIT_VALUE,
		BMP280_INIT_VALUE, BMP280_INIT_VALUE, BMP280_INIT_VALUE,
		BMP280_INIT_VALUE, BMP280_INIT_VALUE, BMP280_INIT_VALUE,
		BMP280_INIT_VALUE, BMP280_INIT_VALUE, BMP280_INIT_VALUE,
		BMP280_INIT_VALUE, BMP280_INIT_VALUE};

void bmp280_get_calib_param(void)
{


		HalI2CReceive(BMP280_I2C_ADDRESS2,BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG,a_data_u8,BMP280_PRESSURE_TEMPERATURE_CALIB_DATA_LENGTH);
		/* read calibration values*/
		bmp280cal.dig_T1 = (u16)((((u16)((u8)a_data_u8[
					BMP280_TEMPERATURE_CALIB_DIG_T1_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_TEMPERATURE_CALIB_DIG_T1_LSB]);
		bmp280cal.dig_T2 = (s16)((((s16)((s8)a_data_u8[
					BMP280_TEMPERATURE_CALIB_DIG_T2_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_TEMPERATURE_CALIB_DIG_T2_LSB]);
		bmp280cal.dig_T3 = (s16)((((s16)((s8)a_data_u8[
					BMP280_TEMPERATURE_CALIB_DIG_T3_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_TEMPERATURE_CALIB_DIG_T3_LSB]);
		bmp280cal.dig_P1 = (u16)((((u16)((u8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P1_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P1_LSB]);
		bmp280cal.dig_P2 = (s16)((((s16)((s8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P2_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P2_LSB]);
		bmp280cal.dig_P3 = (s16)((((s16)((s8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P3_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P3_LSB]);
		bmp280cal.dig_P4 = (s16)((((s16)((s8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P4_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P4_LSB]);
		bmp280cal.dig_P5 = (s16)((((s16)((s8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P5_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P5_LSB]);
		bmp280cal.dig_P6 = (s16)((((s16)((s8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P6_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P6_LSB]);
		bmp280cal.dig_P7 = (s16)((((s16)((s8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P7_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P7_LSB]);
		bmp280cal.dig_P8 = (s16)((((s16)((s8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P8_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P8_LSB]);
		bmp280cal.dig_P9 = (s16)((((s16)((s8)a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P9_MSB]))
					<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
					| a_data_u8[
					BMP280_PRESSURE_CALIB_DIG_P9_LSB]);
}

static int
configure(int type, int value)
{
	uint8_t bmp280ID;
	uint8_t bmp280TempPressConfig[]= {BMP280_CTRL_MEAS_REG,0xB7};

	switch(type)
	{
		case SENSORS_HW_INIT:
		{
			HalI2CInit();
			// Configura o pino P1.7 e seta o valor para 0, de modo que o endereço
			// do BMP280 seja 0x76
			P1SEL &= ~SDO_MASK;
			P1DIR |= SDO_MASK;
			P1_7 |= SDO_MASK;
		    HalI2CReceive(BMP280_I2C_ADDRESS2,BMP280_CHIP_ID_REG,&bmp280ID, 1);
		    if(bmp280ID!=BMP280_CHIP_ID3)
		    	return 0;
		    HalI2CSend(BMP280_I2C_ADDRESS2,bmp280TempPressConfig,2);
		    bmp280_get_calib_param();
			break;
		}
	}
	return 1;
}
/*---------------------------------------------------------------------------*/
SENSORS_SENSOR(bmp_sensor, BMP_SENSOR, value, configure, status);
#endif /* ADC_SENSOR_ON */

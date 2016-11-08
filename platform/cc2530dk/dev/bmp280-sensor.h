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
 *         Header file for BMP280 sensor on the PPSEAI11 kit.
 *
 *         Sensors will be off by default, unless turned on explicitly
 *         in contiki-conf.h
 *
 * \author
 *         George Oikonomou - <oikonomou@users.sourceforge.net>
 */

#ifndef __BMP_SENSOR_H__
#define __BMP_SENSOR_H__

#include "cc253x.h"
#include "contiki-conf.h"
#include "lib/sensors.h"

/* BMP Sensor Types */
#define BMP_SENSOR "BMP"

#ifdef BMP_SENSOR_CONF_ON
#define BMP_SENSOR_ON BMP_SENSOR_CONF_ON
#endif /* BMP_SENSOR_CONF_ON */


#if BMP_SENSOR_ON
extern const struct sensors_sensor bmp_sensor;
#define   BMP_SENSOR_ACTIVATE() bmp_sensor.configure(SENSORS_ACTIVE, 1)
#else
#define   BMP_SENSOR_ACTIVATE()
#endif /* BMP_SENSOR_ON */

#define BMP_SENSOR_TYPE_TEMP    0
#define BMP_SENSOR_TYPE_PRESS_hPa   1
#define BMP_SENSOR_TYPE_LAST_READ_PRESS_Pa_MSB 2
#define BMP_SENSOR_TYPE_LAST_READ_PRESS_Pa_LSB 3

#define SDO_MASK  0x80

#endif /* __BMP_SENSOR_H__ */

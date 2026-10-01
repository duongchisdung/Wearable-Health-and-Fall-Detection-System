/*
 * app_sps30.h
 *
 *  Created on: Aug 26, 2026
 *      Author: duong
 */

#ifndef APP_SPS30_H_
#define APP_SPS30_H_

#include "sl_status.h"
#include "sparkfun_sps30_i2c.h"

sl_status_t app_sps30_init(void);
sl_status_t app_sps30_read(sps30_measurement_t *measure);

#endif /* APP_SPS30_H_ */

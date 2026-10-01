/*
 * app_scd41.h
 *
 *  Created on: Aug 26, 2026
 *      Author: duong
 */

#ifndef APP_SCD41_H
#define APP_SCD41_H

#include "sl_status.h"
#include <stdint.h>

sl_status_t app_scd41_init(void);
sl_status_t app_scd41_read_co2(uint16_t *co2_ppm);

#endif // APP_SCD41_H

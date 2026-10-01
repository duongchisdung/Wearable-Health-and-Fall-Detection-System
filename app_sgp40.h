/*
 * app_sgp40.h
 *
 *  Created on: Aug 14, 2026
 *      Author: duong
 */

#ifndef APP_SGP40_H
#define APP_SGP40_H

#include "sl_status.h"
#include <stdint.h>

sl_status_t app_sgp40_init(void);
sl_status_t app_sgp40_read( int32_t *voc_index);

#endif // APP_SGP40_H

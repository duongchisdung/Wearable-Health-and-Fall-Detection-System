/*
 * app_rht.h
 *
 *  Created on: Aug 17, 2026
 *      Author: duong
 */

#ifndef APP_RHT_H
#define APP_RHT_H

#include "sl_status.h"
#include <stdint.h>

sl_status_t app_rht_init(void);
sl_status_t app_rht_read(uint32_t *humidity, int32_t *temperature);

#endif // APP_RHT_H

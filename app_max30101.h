/*
 * app_max30101.h
 *
 *  Created on: Aug 17, 2026
 *      Author: duong
 */

#ifndef APP_MAX30101_H_
#define APP_MAX30101_H_

#include "sl_status.h"
#include <stdint.h>

sl_status_t app_max30101_init(void);
sl_status_t app_max30101_read (
    uint16_t *heart_rate,
    uint8_t  *confidence,
    uint16_t *oxygen,
    uint8_t  *status);





#endif /* APP_MAX30101_H_ */

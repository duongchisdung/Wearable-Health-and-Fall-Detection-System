/*
 * app_buzzer.h
 *
 *  Created on: Aug 24, 2026
 *      Author: duong
 */

#ifndef APP_BUZZER_H
#define APP_BUZZER_H

#include "sl_status.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

sl_status_t buzzer_init(void);
void buzzer_set_volume(uint8_t volume);
void buzzer_control(bool on);
void buzzer_beep(void);
void buzzer_beep_health_alert(void);
void buzzer_beep_env_alert(void);

#ifdef __cplusplus
}
#endif

#endif // APP_BUZZER_H

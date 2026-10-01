/*
 * oled_app.h
 *
 *  Created on: Aug 14, 2026
 *      Author: duong
 */










/*#ifndef OLED_APP_H
#define OLED_APP_H

#include "sl_status.h"
#include "sparkfun_sps30_i2c.h"
#include <stdint.h>

typedef enum {
    OLED_PAGE_MAIN = 0,
    OLED_PAGE_AIR,
    OLED_PAGE_COUNT
} oled_page_t;

extern volatile oled_page_t oled_current_page;

sl_status_t app_oled_init(void);
void app_oled_update_air_quality(int32_t voc_index,
                                 uint32_t humidity,
                                 int32_t temperature,
                                 uint16_t heart_rate,
                                 uint8_t confidence,
                                 uint16_t oxygen,
                                 uint8_t finger_status,
                                 double body_temp,
                                 uint16_t co2_ppm,
                                 const sps30_measurement_t *pm_data);
void app_oled_show_error(const char *msg);
void app_oled_show_fall_alert(void);


#endif // */



#ifndef OLED_APP_H
#define OLED_APP_H

#include "sl_status.h"
#include <stdint.h>
#include <stdbool.h>

// ── Trang OLED ──
typedef enum {
  OLED_PAGE_MAIN = 0,
  OLED_PAGE_AIR,
} oled_page_t;

extern volatile oled_page_t oled_current_page;

// ── API ──
sl_status_t app_oled_init(void);

// SPS30 đã bỏ → tham số cuối là const void * (luôn truyền NULL)
void app_oled_update_air_quality(int32_t voc_index,
                                 uint32_t humidity,
                                 int32_t temperature,
                                 uint16_t heart_rate,
                                 uint8_t confidence,
                                 uint16_t oxygen,
                                 uint8_t finger_status,
                                 double body_temp,
                                 uint16_t co2_ppm,
                                 float pm2_5);  // ← đổi từ sps30_measurement_t*

void app_oled_show_error(const char *msg);
void app_oled_show_fall_alert(void);

#endif // OLED_APP_H

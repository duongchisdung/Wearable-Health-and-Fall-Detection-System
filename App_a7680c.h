/*
 * app_a7680c.h
 *
 *  Created on: Sep 21, 2026
 *      Author: duong
 */

#ifndef APP_A7680C_H
#define APP_A7680C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sl_status.h"

// ── coord_t — Lưu tọa độ dạng integer, tránh lỗi printf("%.6f") trên EFR32 ──
typedef struct {
  int32_t  integer;
  uint32_t decimal;
  bool     negative;
} coord_t;

// ── Khởi tạo module A7680C ──
sl_status_t App_a7680c_init(void);

// ── Gửi SMS thông thường ──
sl_status_t App_a7680c_send_sms(const char *phone_number,
                                 const char *message);

// ── Lấy vị trí Cell-ID qua AT+CLBS=1,1 ──
// loc_buf: buffer nhận kết quả (dạng link Google Maps hoặc thông báo lỗi)
// buf_size: kích thước buffer
sl_status_t App_a7680c_get_cell_location(char *loc_buf, size_t buf_size);


sl_status_t App_a7680c_mqtt_test(void);
sl_status_t App_a7680c_mqtt_publish_health(
    float temperature,
    float humidity,
    int voc,
    int heart_rate,
    int spo2,
    float body_temperature,
    int co2,
    float pm2_5,
    int fall);


#endif // APP_A7680C_H








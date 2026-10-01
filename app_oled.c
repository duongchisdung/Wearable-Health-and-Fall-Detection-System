/*
 * app_oled.c
 *
 *  Created on: Aug 14, 2026
 *      Author: duong
 */


/***************************************************************************//**
 * @file oled_app.c
 * @brief OLED display application
 ******************************************************************************/
/*#include "oled_app.h"
#include "micro_oled_ssd1306.h"
#include "glib.h"
#include "sl_i2cspm_instances.h"
#include "sl_sleeptimer.h"
//#include "sparkfun_sps30_i2c.h"
#include "app_log.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// ── PRIVATE VARIABLES ──
static glib_context_t glib_context;
static bool oled_is_connected = false;

volatile oled_page_t oled_current_page = OLED_PAGE_MAIN;

// ── INIT ──

sl_status_t app_oled_init(void)
{
  oled_is_connected = false;

  if (ssd1306_init(sl_i2cspm_qwiic) != SL_STATUS_OK) {
    app_log("[OLED] Init Failed or not connected!\r\n");
    return SL_STATUS_FAIL;
  }

  // FIX: Khởi tạo glib trước
  glib_init(&glib_context);

  // FIX: Set màu TRƯỚC khi set font và vẽ
  glib_context.bg_color   = GLIB_BLACK;
  glib_context.text_color = GLIB_WHITE;

  // FIX: Set font SAU khi set màu
  glib_set_font(&glib_context, NULL);

  oled_is_connected = true;

  // Hiển thị màn hình loading
  glib_clear(&glib_context);
  glib_draw_string(&glib_context, "LOADING...", 2, 20);
  glib_update_display();

  sl_sleeptimer_delay_millisecond(500);

  glib_clear(&glib_context);
  glib_update_display();

  app_log("[OLED] Init Successfully!\r\n");
  return SL_STATUS_OK;
}

// ── CẬP NHẬT MÀN HÌNH CHÍNH ──

void app_oled_update_air_quality(int32_t voc_index,
                                 uint32_t humidity,
                                 int32_t temperature,
                                 uint16_t heart_rate,
                                 uint8_t confidence,
                                 uint16_t oxygen,
                                 uint8_t finger_status,
                                 double body_temp,
                                 uint16_t co2_ppm,
                                 const sps30_measurement_t *pm_data)
{
  char str[20];
  (void)confidence; // Không dùng, tránh warning

  if (!oled_is_connected) {
    return;
  }

  glib_clear(&glib_context);

  if (oled_current_page == OLED_PAGE_MAIN) {
    // ── MÀN HÌNH 1: SỨC KHỎE ──

    // Tiêu đề
    glib_draw_string(&glib_context, "== HEALTH ==", 0, 0);

    // Dòng 1 (Y=10): Nhịp tim
    if (finger_status == 3) {
      snprintf(str, sizeof(str), "HR: %3d bpm", heart_rate);
    } else {
      snprintf(str, sizeof(str), "HR:  ---");
    }
    glib_draw_string(&glib_context, str, 0, 10);

    // Dòng 2 (Y=20): SpO2
    if (finger_status == 3) {
      snprintf(str, sizeof(str), "SpO2: %2d%%", oxygen);
    } else {
      snprintf(str, sizeof(str), "SpO2: ---");
    }
    glib_draw_string(&glib_context, str, 0, 20);

    // Dòng 3 (Y=30): Thân nhiệt
    // FIX: Dùng integer math thay vì float để tránh lỗi format
    if ((body_temp >= 28.0) && (body_temp <= 45.0)) {
      int32_t bx10 = (int32_t)(body_temp * 10);
      snprintf(str, sizeof(str), "Tmp:%2d.%dC",
               (int)(bx10 / 10),
               (int)(bx10 % 10));
    } else {
      snprintf(str, sizeof(str), "Tmp: ---");
    }
    glib_draw_string(&glib_context, str, 0, 30);

    // Dòng 4 (Y=40): Gợi ý chuyển trang
    // FIX: Bỏ tham số thừa, dùng trực tiếp string literal
    glib_draw_string(&glib_context, ">BTN1:Env", 0, 40);

  } else {
    // ── MÀN HÌNH 2: MÔI TRƯỜNG ──

    // Tiêu đề
    glib_draw_string(&glib_context, "=== ENV ===", 0, 0);

    // Dòng 1 (Y=10): CO2
    // "CO2:9999ppm" = 11 chars -> OK với str[20]
    snprintf(str, sizeof(str), "CO2:%4uppm", co2_ppm);
    glib_draw_string(&glib_context, str, 0, 10);

    // Dòng 2 (Y=20): VOC Index
    if (voc_index >= 0) {
      snprintf(str, sizeof(str), "VOC:%4d", (int)voc_index);
    } else {
      snprintf(str, sizeof(str), "VOC: ---");
    }
    glib_draw_string(&glib_context, str, 0, 20);

    // Dòng 3 (Y=30): Bụi mịn PM2.5
    if (pm_data != NULL) {
      // FIX: Dùng integer math thay vì %.1f để tránh lỗi printf float
      int32_t pm_x10 = (int32_t)(pm_data->mc_2p5 * 10);
      snprintf(str, sizeof(str), "PM2.5:%d.%d",
               (int)(pm_x10 / 10),
               (int)(pm_x10 % 10));
    } else {
      snprintf(str, sizeof(str), "PM2.5: ---");
    }
    glib_draw_string(&glib_context, str, 0, 30);

    // Dòng 4 (Y=40): Nhiệt độ + Độ ẩm
    // FIX: humidity đơn vị milli-percent -> /1000 = %
    //      temperature đơn vị milli-Celsius -> /1000 = C
    snprintf(str, sizeof(str), "T:%dC H:%u%%",
             (int)(temperature / 1000),
             (unsigned)(humidity / 1000));
    glib_draw_string(&glib_context, str, 0, 40);

    // Dòng 5 (Y=50): Gợi ý chuyển trang
    // FIX: Bỏ tham số thừa
    glib_draw_string(&glib_context, ">BTN1:Hlth", 0, 50);
  }

  glib_update_display();
}

// ── HIỂN THỊ LỖI ──

void app_oled_show_error(const char *msg)
{
  if (!oled_is_connected) {
    return;
  }
  glib_clear(&glib_context);
  glib_draw_string(&glib_context, "!! ERROR !!", 0, 0);
  glib_draw_string(&glib_context, msg, 0, 14);
  glib_update_display();
}

// ── HIỂN THỊ CẢNH BÁO NGÃ ──

void app_oled_show_fall_alert(void)
{
  if (!oled_is_connected) {
    return;
  }
  glib_clear(&glib_context);
  glib_draw_string(&glib_context, "!! FALL !!", 2, 0);
  glib_draw_string(&glib_context, "ALERT!", 14, 12);
  glib_draw_string(&glib_context, "Are you OK?", 0, 26);
  glib_draw_string(&glib_context, "BTN0:Cancel", 0, 40);
  glib_update_display();
}*/


#include "oled_app.h"
#include "micro_oled_ssd1306.h"
#include "glib.h"
#include "sl_i2cspm_instances.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// ── PRIVATE VARIABLES ──
static glib_context_t glib_context;
static bool oled_is_connected = false;

volatile oled_page_t oled_current_page = OLED_PAGE_MAIN;

// ── INIT ──
sl_status_t app_oled_init(void)
{
  oled_is_connected = false;

  if (ssd1306_init(sl_i2cspm_qwiic) != SL_STATUS_OK) {
    app_log("[OLED] Init Failed or not connected!\r\n");
    return SL_STATUS_FAIL;
  }

  glib_init(&glib_context);

  glib_context.bg_color   = GLIB_BLACK;
  glib_context.text_color = GLIB_WHITE;

  glib_set_font(&glib_context, NULL);

  oled_is_connected = true;

  glib_clear(&glib_context);
  glib_draw_string(&glib_context, "LOADING...", 2, 20);
  glib_update_display();

  sl_sleeptimer_delay_millisecond(500);

  glib_clear(&glib_context);
  glib_update_display();

  app_log("[OLED] Init Successfully!\r\n");
  return SL_STATUS_OK;
}

// ── CẬP NHẬT MÀN HÌNH CHÍNH ──
void app_oled_update_air_quality(int32_t voc_index,
                                 uint32_t humidity,
                                 int32_t temperature,
                                 uint16_t heart_rate,
                                 uint8_t confidence,
                                 uint16_t oxygen,
                                 uint8_t finger_status,
                                 double body_temp,
                                 uint16_t co2_ppm,
                                 float pm2_5)  // NULL vì đã bỏ SPS30
{
  char str[20];
  (void)confidence;


  if (!oled_is_connected) {
    return;
  }

  glib_clear(&glib_context);

  if (oled_current_page == OLED_PAGE_MAIN) {
    // ── MÀN HÌNH 1: SỨC KHỎE ──

    glib_draw_string(&glib_context, "== HEALTH ==", 0, 0);

    // Dòng 1 (Y=10): Nhịp tim
    if (finger_status == 3) {
      snprintf(str, sizeof(str), "HR: %3d bpm", heart_rate);
    } else {
      snprintf(str, sizeof(str), "HR:  ---");
    }
    glib_draw_string(&glib_context, str, 0, 10);

    // Dòng 2 (Y=20): SpO2
    if (finger_status == 3) {
      snprintf(str, sizeof(str), "SpO2: %2d%%", oxygen);
    } else {
      snprintf(str, sizeof(str), "SpO2: ---");
    }
    glib_draw_string(&glib_context, str, 0, 20);

    // Dòng 3 (Y=30): Thân nhiệt
    if ((body_temp >= 31.0) && (body_temp <= 45.0)) {
      int32_t bx10 = (int32_t)(body_temp * 10);
      snprintf(str, sizeof(str), "Tmp:%2d.%dC",
               (int)(bx10 / 10),
               (int)(bx10 % 10));
    } else {
      snprintf(str, sizeof(str), "Tmp: ---");
    }
    glib_draw_string(&glib_context, str, 0, 30);

    // Dòng 4 (Y=40): Gợi ý chuyển trang
    glib_draw_string(&glib_context, ">BTN1:Env", 0, 40);

  } else {
    // ── MÀN HÌNH 2: MÔI TRƯỜNG ──

    glib_draw_string(&glib_context, "=== ENV ===", 0, 0);

    // Dòng 1 (Y=10): CO2 — rút ngắn để không tràn 64px
    snprintf(str, sizeof(str), "CO2:%4u ppm", co2_ppm);
    glib_draw_string(&glib_context, str, 0, 10);

    // Dòng 2 (Y=20): VOC Index
    if (voc_index >= 0) {
      snprintf(str, sizeof(str), "VOC:%4d", (int)voc_index);
    } else {
      snprintf(str, sizeof(str), "VOC: ---");
    }
    glib_draw_string(&glib_context, str, 0, 20);

// Dòng 3 (Y=30): PM2.5 — thay cho Temp/Humi rieng, gop chung 1 dong gon
    if (pm2_5 >= 0.0f) {
      int32_t pmx10 = (int32_t)(pm2_5 * 10.0f);
      snprintf(str, sizeof(str), "PM2.5:%d.%d",
               (int)(pmx10 / 10),
               (int)(pmx10 % 10));
    } else {
      snprintf(str, sizeof(str), "PM2.5: ---");
    }
    glib_draw_string(&glib_context, str, 0, 30);

    // Dòng 4 (Y=40): Nhiệt độ + Độ ẩm gộp chung 1 dòng để lấy chỗ cho PM2.5
    snprintf(str, sizeof(str), "T:%dC H:%u%%",
             (int)(temperature / 1000),
             (unsigned)(humidity / 1000));
    glib_draw_string(&glib_context, str, 0, 40);
  }

  glib_update_display();
}

// ── HIỂN THỊ LỖI ──
void app_oled_show_error(const char *msg)
{
  if (!oled_is_connected) {
    return;
  }
  glib_clear(&glib_context);
  glib_draw_string(&glib_context, "!! ERROR !!", 0, 0);
  glib_draw_string(&glib_context, msg, 0, 14);
  glib_update_display();
}

// ── HIỂN THỊ CẢNH BÁO NGÃ ──
void app_oled_show_fall_alert(void)
{
  if (!oled_is_connected) {
    return;
  }
  glib_clear(&glib_context);
  glib_draw_string(&glib_context, "!! FALL !!", 2, 0);
  glib_draw_string(&glib_context, "ALERT!", 14, 12);
  glib_draw_string(&glib_context, "Are you OK?", 0, 26);
  glib_draw_string(&glib_context, "BTN0:Cancel", 0, 40);
  glib_update_display();
}

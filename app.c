/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/








/*#include "oled_app.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "app_sgp40.h"
#include "app_rht.h"
#include "app_max30101.h"
#include "app_mlx90632.h"
#include "app_fall_detection.h"
#include "sl_simple_button_instances.h"
#include "app_buzzer.h"
#include "app_scd41.h"
#include "app_sps30.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"
#include "em_i2c.h"
#include "em_gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "sl_iostream_init_eusart_instances.h"

// ── NGƯỠNG SỨC KHỎE ──
#define HEALTH_SPO2_MIN_THRESHOLD       95
#define HEALTH_HR_MIN_THRESHOLD         50
#define HEALTH_HR_MAX_THRESHOLD         110
#define HEALTH_BODY_TEMP_MAX_THRESHOLD  37.8

// ── NGƯỠNG MÔI TRƯỜNG ──
#define ENV_CO2_MAX_THRESHOLD           1500
#define ENV_PM25_MAX_THRESHOLD          50.0f
#define ENV_VOC_MAX_THRESHOLD           250

// ── THỜI GIAN ──
#define READING_INTERVAL_MSEC           1000
#define FALL_AUTO_RECOVERY_TIME_MS      5000
#define SENSOR_WARMUP_CYCLES            6

static sl_sleeptimer_timer_handle_t app_timer_handle;
static sl_sleeptimer_timer_handle_t fall_recovery_timer_handle;

static volatile bool app_timer_expire      = false;
static volatile bool is_fall_detected      = false;
static volatile bool auto_recovery_allowed = false;

static uint16_t            latest_co2_ppm      = 0;
static sps30_measurement_t latest_sps30_measure = { 0 };
static bool                sps30_has_valid_data = false;

// Chuyển ra file scope để dễ quản lý
static bool health_alert_triggered = false;
static bool env_alert_triggered    = false;

// ── CALLBACKS ──

static void fall_recovery_timer_cb(sl_sleeptimer_timer_handle_t *handle,
                                   void *data)
{
  (void) handle;
  (void) data;
  auto_recovery_allowed = false;
  app_log("--> Qua 5s: Khoa tu dong huy, bat buoc reset bang BTN0!\n");
}

static void app_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;
  app_timer_expire = true;
}

// ── XỬ LÝ NÚT BẤM ──

void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    if (&sl_button_btn0 == handle) {
      if (is_fall_detected) {
        app_log("--> Nhan BTN0: Reset canh bao nga.\n");
        sl_sleeptimer_stop_timer(&fall_recovery_timer_handle);
        is_fall_detected      = false;
        auto_recovery_allowed = false;
        app_fall_detection_reset();
        buzzer_control(false);
        app_timer_expire = true;
      }
    } else if (&sl_button_btn1 == handle) {
      oled_current_page = (oled_page_t)
          ((oled_current_page + 1) % OLED_PAGE_COUNT);
      app_log("--> Nhan BTN1: Chuyen trang OLED -> %d\n", oled_current_page);
      app_timer_expire = true;
    }
  }
}

// ── KHỞI TẠO ──

void app_init(void)
{
#if defined(SL_CATALOG_IOSTREAM_EUSART_PRESENT)
  sl_iostream_set_default(sl_iostream_vcom_handle);
  app_log_iostream_set(sl_iostream_vcom_handle);
#endif

  app_log("=== Project Starting ===\n");
  sl_sleeptimer_delay_millisecond(500);

  if (app_oled_init() != SL_STATUS_OK) {
    app_log("[WARN] OLED init failed!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // SGP40 — chờ power-on, retry tối đa 3 lần
  sl_sleeptimer_delay_millisecond(2000);
  bool sgp40_ok = false;
  for (int retry = 0; retry < 3; retry++) {
    if (app_sgp40_init() == SL_STATUS_OK) {
      sgp40_ok = true;
      break;
    }
    app_log("[SGP40] Init retry %d/3...\n", retry + 1);
    sl_sleeptimer_delay_millisecond(1000);
  }
  if (!sgp40_ok) {
    app_log("[WARN] SGP40 init failed after 3 retries!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_rht_init() != SL_STATUS_OK) {
    app_log("[WARN] RHT init failed!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_max30101_init() != SL_STATUS_OK) {
    app_log("[WARN] MAX30101 init failed!\n");
  } else {
    app_log("MAX30101 init OK. Waiting 2s warm-up...\n");
    sl_sleeptimer_delay_millisecond(2000);
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_mlx90632_init() != SL_STATUS_OK) {
    app_log("[WARN] MLX90632 init failed!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_fall_detection_init() != SL_STATUS_OK) {
    app_log("[WARN] Fall detection init failed!\n");
  }

  if (buzzer_init() != SL_STATUS_OK) {
    app_log("[WARN] Buzzer init failed!\n");
  } else {
    app_log("Buzzer init OK.\n");
    buzzer_beep();
  }

  sl_sleeptimer_delay_millisecond(1000);

  // SCD41 — periodic measurement, chờ 5s sau khi start
  if (app_scd41_init() != SL_STATUS_OK) {
    app_log("[WARN] SCD41 init failed or not connected!\n");
  } else {
    app_log("SCD41 init OK.\n");
    sl_sleeptimer_delay_millisecond(5000);  // Chờ lần đo đầu tiên
  }

  if (app_sps30_init() != SL_STATUS_OK) {
    app_log("[WARN] SPS30 init failed or not connected!\n");
  } else {
    app_log("SPS30 init OK.\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  sl_sleeptimer_start_periodic_timer_ms(&app_timer_handle,
                                        READING_INTERVAL_MSEC,
                                        app_timer_cb,
                                        (void *) NULL,
                                        0,
                                        0);

  app_log("=== Init complete. Waiting for sensors to warm up (%d s)... ===\n",
          SENSOR_WARMUP_CYCLES);
}

// ── VÒNG LẶP CHÍNH ──

void app_process_action(void)
{
  static uint8_t warmup_counter = 0;

  // ── 1. XỬ LÝ PHÁT HIỆN NGÃ ──
  if (!is_fall_detected) {
    if (app_fall_detection_check()) {
      is_fall_detected      = true;
      auto_recovery_allowed = true;

      app_log("\r\n=============================================================\r\n");
      app_log(">>> [CANH BAO] PHAT HIEN NGA! Kich hoat Buzzer!\r\n");
      app_log(">>> Cho 5s de kiem tra tu phuc hoi hoac bam BTN0...\r\n");
      app_log("=============================================================\r\n\r\n");

      sl_sleeptimer_start_timer_ms(&fall_recovery_timer_handle,
                                   FALL_AUTO_RECOVERY_TIME_MS,
                                   fall_recovery_timer_cb,
                                   NULL,
                                   0,
                                   0);

      app_oled_show_fall_alert();
      buzzer_control(true);
    }
  } else {
    if (auto_recovery_allowed) {
      if (app_fall_detection_check_recovery()) {
        app_log("--> Nguoi dung da dung day trong 5s: Tu dong huy canh bao!\n");
        sl_sleeptimer_stop_timer(&fall_recovery_timer_handle);
        is_fall_detected      = false;
        auto_recovery_allowed = false;
        app_fall_detection_reset();
        buzzer_control(false);
        app_timer_expire = true;
      }
    }
  }

  if (is_fall_detected) {
    return;
  }

  // ── 2. KIỂM TRA TIMER ──
  if (!app_timer_expire) {
    return;
  }
  app_timer_expire = false;

  // ── 3. WARM-UP ──
  if (warmup_counter < SENSOR_WARMUP_CYCLES) {
    warmup_counter++;
    app_log("[WARMUP] Cho sensor on dinh... (%d/%d)\n",
            warmup_counter, SENSOR_WARMUP_CYCLES);
    return;
  }

  // ── 4. ĐỌC DỮ LIỆU CẢM BIẾN ──
  // Thứ tự: sensor nhanh trước, SCD41 cuối cùng
  int32_t  voc_index     = -1;
  uint32_t humidity      = 0;
  int32_t  temperature   = 0;
  uint16_t heart_rate    = 0;
  uint8_t  confidence    = 0;
  uint16_t oxygen        = 0;
  uint8_t  finger_status = 0;
  double   ambient_temp  = 0.0;
  double   body_temp     = 0.0;

  if (app_sgp40_read(&voc_index) != SL_STATUS_OK) {
    voc_index = -1;
    app_log("[WARN] SGP40 read failed.\n");
  }

  if (app_rht_read(&humidity, &temperature) != SL_STATUS_OK) {
    humidity    = 0;
    temperature = 0;
    app_log("[WARN] RHT read failed.\n");
  }

  if (app_max30101_read(&heart_rate, &confidence, &oxygen,
                        &finger_status) != SL_STATUS_OK) {
    heart_rate    = 0;
    oxygen        = 0;
    finger_status = 0;
    app_log("[WARN] MAX30101 read failed.\n");
  }

  double obj_temp = 0.0;
  if (app_mlx90632_read(&ambient_temp, &obj_temp) == SL_STATUS_OK) {
    if ((obj_temp - ambient_temp >= 1.5)
        && (obj_temp >= 28.0)
        && (obj_temp <= 42.0)) {
      body_temp = obj_temp;
    } else {
      body_temp = 0.0;
    }
  } else {
    body_temp = 0.0;
    app_log("[WARN] MLX90632 read failed.\n");
  }

  sps30_measurement_t temp_measure = { 0 };
  if (app_sps30_read(&temp_measure) == SL_STATUS_OK) {
    if (temp_measure.mc_2p5 >= 0.0f) {
      latest_sps30_measure = temp_measure;
      sps30_has_valid_data = true;
    }
  }

  // SCD41 đọc cuối cùng — tránh block bus các sensor khác
  uint16_t    new_co2 = 0;
  sl_status_t co2_sc  = app_scd41_read_co2(&new_co2);
  if (co2_sc == SL_STATUS_OK) {
    if (new_co2 >= 400 && new_co2 <= 10000) {
      latest_co2_ppm = new_co2;
      app_log("SCD41 CO2: %u ppm\n", latest_co2_ppm);
    } else {
      app_log("[WARN] SCD41 CO2 out of range: %u ppm (ignored)\n", new_co2);
    }
  } else if (co2_sc == SL_STATUS_NOT_READY) {
    app_log("[SCD41] Not ready, keeping last: %u ppm\n", latest_co2_ppm);
  }

  // ── 5. KIỂM TRA NGƯỠNG VÀ PHÁT CẢNH BÁO ──
  if (finger_status == 3) {
    bool is_health_abnormal =
        (oxygen < HEALTH_SPO2_MIN_THRESHOLD)
        || (heart_rate < HEALTH_HR_MIN_THRESHOLD)
        || (heart_rate > HEALTH_HR_MAX_THRESHOLD)
        || (body_temp > HEALTH_BODY_TEMP_MAX_THRESHOLD);

    if (is_health_abnormal) {
      if (!health_alert_triggered) {
        health_alert_triggered = true;
        app_log("\n[CANH BAO] Chi so suc khoe vuot nguong!\n");
        app_log("  SpO2=%u%%, HR=%u bpm, Temp=%.1f C\n",
                oxygen, heart_rate, body_temp);
        oled_current_page = OLED_PAGE_MAIN;
        buzzer_beep_health_alert();
      }
    } else {
      health_alert_triggered = false;
    }
  } else {
    health_alert_triggered = false;
  }

  bool is_env_abnormal = false;

  if (latest_co2_ppm > ENV_CO2_MAX_THRESHOLD) {
    is_env_abnormal = true;
    app_log("[ENV] CO2 cao: %u ppm (nguong: %u)\n",
            latest_co2_ppm, ENV_CO2_MAX_THRESHOLD);
  }

  if (voc_index > ENV_VOC_MAX_THRESHOLD) {
    is_env_abnormal = true;
    app_log("[ENV] VOC cao: %ld (nguong: %d)\n",
            voc_index, ENV_VOC_MAX_THRESHOLD);
  }

  if (sps30_has_valid_data
      && (latest_sps30_measure.mc_2p5 > ENV_PM25_MAX_THRESHOLD)) {
    is_env_abnormal = true;
    app_log("[ENV] PM2.5 cao: %.2f ug/m3 (nguong: %.1f)\n",
            latest_sps30_measure.mc_2p5, ENV_PM25_MAX_THRESHOLD);
  }

  if (is_env_abnormal) {
    if (!env_alert_triggered) {
      env_alert_triggered = true;
      app_log("\n[CANH BAO] Khong khi vuot nguong an toan!\n");
      oled_current_page = OLED_PAGE_AIR;
      buzzer_beep_env_alert();
    }
  } else {
    env_alert_triggered = false;
  }

  // ── 6. CẬP NHẬT OLED ──
  app_oled_update_air_quality(voc_index,
                              humidity,
                              temperature,
                              heart_rate,
                              confidence,
                              oxygen,
                              finger_status,
                              body_temp,
                              latest_co2_ppm,
                              sps30_has_valid_data
                                ? &latest_sps30_measure
                                : NULL);
}*/



/*#include "oled_app.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "app_rht.h"
#include "app_sgp40.h"
#include "app_max30101.h"
#include "app_mlx90632.h"
#include "app_scd41.h"
#include "app_sps30.h"
#include "sl_simple_button_instances.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"
#include "em_gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "sl_iostream_init_eusart_instances.h"

#define READING_INTERVAL_MSEC    1000
#define SENSOR_WARMUP_CYCLES     3

static sl_sleeptimer_timer_handle_t app_timer_handle;
static volatile bool app_timer_expire = false;

// ── Lưu giá trị mới nhất để truyền vào OLED ──
static int32_t             latest_temperature  = 0;
static uint32_t            latest_humidity     = 0;
static int32_t             latest_voc_index    = -1;
static uint16_t            latest_heart_rate   = 0;
static uint8_t             latest_confidence   = 0;
static uint16_t            latest_oxygen       = 0;
static uint8_t             latest_finger       = 0;
static double              latest_body_temp    = 0.0;
static uint16_t            latest_co2_ppm      = 0;
static sps30_measurement_t latest_pm           = { 0 };
static bool                latest_pm_valid     = false;

static void app_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;
  app_timer_expire = true;
}

// ── Button: nhấn BTN0 để chuyển trang OLED ──
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    if (&sl_button_btn0 == handle) {
      if (oled_current_page == OLED_PAGE_MAIN) {
        oled_current_page = OLED_PAGE_AIR;
      } else {
        oled_current_page = OLED_PAGE_MAIN;
      }
    }
  }
}

void app_init(void)
{
#if defined(SL_CATALOG_IOSTREAM_EUSART_PRESENT)
  sl_iostream_set_default(sl_iostream_vcom_handle);
  app_log_iostream_set(sl_iostream_vcom_handle);
#endif

  app_log("=== [FINAL] All Sensors + OLED ===\n");
  sl_sleeptimer_delay_millisecond(500);

  // OLED
  if (app_oled_init() != SL_STATUS_OK) {
    app_log("[OLED] Init FAILED!\n");
  } else {
    app_log("[OLED] Init OK\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // RHT
  if (app_rht_init() != SL_STATUS_OK) {
    app_log("[RHT] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // SGP40
  sl_sleeptimer_delay_millisecond(2000);
  if (app_sgp40_init() == SL_STATUS_OK) {
    app_log("[SGP40] Init OK!\n");
  } else {
    app_log("[SGP40] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // MAX30101
  if (app_max30101_init() == SL_STATUS_OK) {
    app_log("[MAX30101] Init OK!\n");
    sl_sleeptimer_delay_millisecond(2000);
  } else {
    app_log("[MAX30101] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // MLX90632
  if (app_mlx90632_init() == SL_STATUS_OK) {
    app_log("[MLX90632] Init OK!\n");
  } else {
    app_log("[MLX90632] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // SCD41
  sl_sleeptimer_delay_millisecond(5000);
  if (app_scd41_init() == SL_STATUS_OK) {
    app_log("[SCD41] Init OK!\n");
  } else {
    app_log("[SCD41] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // SPS30
  if (app_sps30_init() == SL_STATUS_OK) {
    app_log("[SPS30] Init OK!\n");
  } else {
    app_log("[SPS30] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  sl_sleeptimer_start_periodic_timer_ms(&app_timer_handle,
                                        READING_INTERVAL_MSEC,
                                        app_timer_cb,
                                        (void *) NULL,
                                        0,
                                        0);
  app_log("=== Init done ===\n");
}

void app_process_action(void)
{
  static uint8_t warmup_counter = 0;

  if (!app_timer_expire) return;
  app_timer_expire = false;

  if (warmup_counter < SENSOR_WARMUP_CYCLES) {
    warmup_counter++;
    app_log("[WARMUP] %d/%d\n", warmup_counter, SENSOR_WARMUP_CYCLES);
    return;
  }

  // ── RHT ──
  if (app_rht_read(&latest_humidity, &latest_temperature) == SL_STATUS_OK) {
    app_log("[RHT] OK | Temp: %ld.%03ld C | Hum: %lu.%03lu %%\n",
            latest_temperature / 1000,
            (uint32_t)(latest_temperature % 1000),
            latest_humidity    / 1000,
            latest_humidity    % 1000);
  } else {
    app_log("[RHT] WARN: read failed!\n");
  }

  // ── SGP40 ──
  if (app_sgp40_read(&latest_voc_index) == SL_STATUS_OK) {
    app_log("[SGP40] OK | VOC Index: %ld\n", latest_voc_index);
  } else {
    app_log("[SGP40] WARN: read failed!\n");
  }

  // ── MAX30101 ──
  sl_status_t max_sc = app_max30101_read(&latest_heart_rate,
                                          &latest_confidence,
                                          &latest_oxygen,
                                          &latest_finger);
  if (max_sc == SL_STATUS_OK) {
    app_log("[MAX30101] OK | HR=%u bpm | SpO2=%u%% | "
            "Conf=%u%% | Finger=%u\n",
            latest_heart_rate, latest_oxygen,
            latest_confidence, latest_finger);
  } else if (max_sc == SL_STATUS_NOT_READY) {
    app_log("[MAX30101] Not ready\n");
  } else {
    app_log("[MAX30101] WARN: read failed! (0x%lx)\n", (uint32_t)max_sc);
  }

  // ── MLX90632 ──
  double ambient_temp = 0.0;
  if (app_mlx90632_read(&ambient_temp, &latest_body_temp) == SL_STATUS_OK) {
    app_log("[MLX90632] OK | Ambient: %.2f C | Object: %.2f C\n",
            ambient_temp, latest_body_temp);
  } else {
    app_log("[MLX90632] WARN: read failed!\n");
  }

  // ── SPS30 ──
  sps30_measurement_t temp_pm = { 0 };
  if (app_sps30_read(&temp_pm) == SL_STATUS_OK) {
    latest_pm       = temp_pm;
    latest_pm_valid = true;
    app_log("[SPS30] OK | PM1.0=%.2f PM2.5=%.2f "
            "PM4.0=%.2f PM10=%.2f ug/m3\n",
            temp_pm.mc_1p0, temp_pm.mc_2p5,
            temp_pm.mc_4p0, temp_pm.mc_10p0);
  } else {
    app_log("[SPS30] WARN: read failed!\n");
  }

  // ── SCD41 ──
  uint16_t    new_co2 = 0;
  sl_status_t co2_sc  = app_scd41_read_co2(&new_co2);
  if (co2_sc == SL_STATUS_OK) {
    latest_co2_ppm = new_co2;
    app_log("[SCD41] OK | CO2: %u ppm\n", latest_co2_ppm);
  } else if (co2_sc == SL_STATUS_NOT_READY) {
    app_log("[SCD41] Not ready, last: %u ppm\n", latest_co2_ppm);
  } else {
    app_log("[SCD41] WARN: read failed! (0x%lx)\n", (uint32_t)co2_sc);
  }

  // ── CẬP NHẬT OLED ── ← ĐÂY LÀ DÒNG BỊ THIẾU
  app_oled_update_air_quality(
      latest_voc_index,
      latest_humidity,
      latest_temperature,
      latest_heart_rate,
      latest_confidence,
      latest_oxygen,
      latest_finger,
      latest_body_temp,
      latest_co2_ppm,
      latest_pm_valid ? &latest_pm : NULL
  );

  app_log("----------------------------------------\n");
}*/



/*#include "oled_app.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "app_rht.h"
#include "app_sgp40.h"
#include "app_max30101.h"
#include "app_mlx90632.h"
#include "app_scd41.h"
#include "app_sps30.h"
#include "app_fall_detection.h"
#include "sl_simple_button_instances.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"
#include "em_gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "sl_iostream_init_eusart_instances.h"

#define READING_INTERVAL_MSEC    1000
#define FALL_CHECK_INTERVAL_MSEC 10      // 100Hz = mỗi 10ms check 1 lần
#define SENSOR_WARMUP_CYCLES     3

// ── Timer handles ──
static sl_sleeptimer_timer_handle_t app_timer_handle;
static sl_sleeptimer_timer_handle_t fall_timer_handle;

// ── Flags ──
static volatile bool app_timer_expire  = false;
static volatile bool fall_timer_expire = false;

// ── Trạng thái ngã ──
typedef enum {
  APP_FALL_STATE_NORMAL,      // Bình thường
  APP_FALL_STATE_ALERT,       // Đang hiện cảnh báo ngã
} app_fall_ui_state_t;

static volatile app_fall_ui_state_t fall_ui_state = APP_FALL_STATE_NORMAL;

// ── Lưu giá trị sensor mới nhất ──
static int32_t             latest_temperature = 0;
static uint32_t            latest_humidity    = 0;
static int32_t             latest_voc_index   = -1;
static uint16_t            latest_heart_rate  = 0;
static uint8_t             latest_confidence  = 0;
static uint16_t            latest_oxygen      = 0;
static uint8_t             latest_finger      = 0;
static double              latest_body_temp   = 0.0;
static uint16_t            latest_co2_ppm     = 0;
static sps30_measurement_t latest_pm          = { 0 };
static bool                latest_pm_valid    = false;

// ── Timer callbacks ──
static void app_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;
  app_timer_expire = true;
}

static void fall_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;
  fall_timer_expire = true;
}

// ── Button handler ──
// BTN0 → Xác nhận/hủy cảnh báo ngã (khi đang alert)
// BTN1 → Chuyển trang OLED (khi bình thường)
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {

    if (handle == &sl_button_btn0) {
      // BTN0: Hủy cảnh báo ngã
      if (fall_ui_state == APP_FALL_STATE_ALERT) {
        fall_ui_state = APP_FALL_STATE_NORMAL;
        app_fall_detection_reset();
        app_log("[FALL] Canh bao da duoc huy boi nguoi dung.\n");
      }
    }

    if (handle == &sl_button_btn1) {
      // BTN1: Chuyển trang OLED (chỉ khi không có cảnh báo ngã)
      if (fall_ui_state == APP_FALL_STATE_NORMAL) {
        if (oled_current_page == OLED_PAGE_MAIN) {
          oled_current_page = OLED_PAGE_AIR;
          app_log("[OLED] Chuyen sang trang ENV\n");
        } else {
          oled_current_page = OLED_PAGE_MAIN;
          app_log("[OLED] Chuyen sang trang HEALTH\n");
        }
      }
    }
  }
}

// ── Init ──
void app_init(void)
{
#if defined(SL_CATALOG_IOSTREAM_EUSART_PRESENT)
  sl_iostream_set_default(sl_iostream_vcom_handle);
  app_log_iostream_set(sl_iostream_vcom_handle);
#endif

  app_log("=== [FINAL] All Sensors + OLED + Fall Detection ===\n");
  sl_sleeptimer_delay_millisecond(500);

  // OLED
  if (app_oled_init() != SL_STATUS_OK) {
    app_log("[OLED] Init FAILED!\n");
  } else {
    app_log("[OLED] Init OK\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // RHT
  if (app_rht_init() != SL_STATUS_OK) {
    app_log("[RHT] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // SGP40
  sl_sleeptimer_delay_millisecond(2000);
  if (app_sgp40_init() == SL_STATUS_OK) {
    app_log("[SGP40] Init OK!\n");
  } else {
    app_log("[SGP40] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // MAX30101
  if (app_max30101_init() == SL_STATUS_OK) {
    app_log("[MAX30101] Init OK!\n");
    sl_sleeptimer_delay_millisecond(2000);
  } else {
    app_log("[MAX30101] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // MLX90632
  if (app_mlx90632_init() == SL_STATUS_OK) {
    app_log("[MLX90632] Init OK!\n");
  } else {
    app_log("[MLX90632] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // SCD41
  sl_sleeptimer_delay_millisecond(5000);
  if (app_scd41_init() == SL_STATUS_OK) {
    app_log("[SCD41] Init OK!\n");
  } else {
    app_log("[SCD41] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // SPS30
  if (app_sps30_init() == SL_STATUS_OK) {
    app_log("[SPS30] Init OK!\n");
  } else {
    app_log("[SPS30] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ICM40627 Fall Detection
  if (app_fall_detection_init() == SL_STATUS_OK) {
    app_log("[FALL] Init OK!\n");
  } else {
    app_log("[FALL] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // Timer đọc sensor 1 giây/lần
  sl_sleeptimer_start_periodic_timer_ms(&app_timer_handle,
                                        READING_INTERVAL_MSEC,
                                        app_timer_cb,
                                        (void *) NULL,
                                        0,
                                        0);

  // Timer phát hiện ngã 10ms/lần (100Hz)
  sl_sleeptimer_start_periodic_timer_ms(&fall_timer_handle,
                                        FALL_CHECK_INTERVAL_MSEC,
                                        fall_timer_cb,
                                        (void *) NULL,
                                        0,
                                        0);

  app_log("=== Init done ===\n");
}

// ── Main loop ──
void app_process_action(void)
{
  static uint8_t warmup_counter = 0;

  // ── FALL DETECTION — chạy 100Hz, độc lập với sensor timer ──
  if (fall_timer_expire) {
    fall_timer_expire = false;

    if (fall_ui_state == APP_FALL_STATE_NORMAL) {
      // Đang bình thường — kiểm tra ngã
      if (app_fall_detection_check()) {
        fall_ui_state = APP_FALL_STATE_ALERT;
        app_log("[FALL] !!! PHAT HIEN NGA — Hien canh bao OLED !!!\n");
        app_oled_show_fall_alert();
      }
    } else {
      // Đang alert — kiểm tra người dùng đứng dậy chưa
      if (app_fall_detection_check_recovery()) {
        fall_ui_state = APP_FALL_STATE_NORMAL;
        app_log("[FALL] Nguoi dung da dung day — huy canh bao.\n");
      }
    }
  }

  // ── SENSOR READING — 1Hz ──
  if (!app_timer_expire) return;
  app_timer_expire = false;

  if (warmup_counter < SENSOR_WARMUP_CYCLES) {
    warmup_counter++;
    app_log("[WARMUP] %d/%d\n", warmup_counter, SENSOR_WARMUP_CYCLES);
    return;
  }

  // ── RHT ──
  if (app_rht_read(&latest_humidity, &latest_temperature) == SL_STATUS_OK) {
    app_log("[RHT] OK | Temp: %ld.%03ld C | Hum: %lu.%03lu %%\n",
            latest_temperature / 1000,
            (uint32_t)(latest_temperature % 1000),
            latest_humidity    / 1000,
            latest_humidity    % 1000);
  } else {
    app_log("[RHT] WARN: read failed!\n");
  }

  // ── SGP40 ──
  if (app_sgp40_read(&latest_voc_index) == SL_STATUS_OK) {
    app_log("[SGP40] OK | VOC Index: %ld\n", latest_voc_index);
  } else {
    app_log("[SGP40] WARN: read failed!\n");
  }

  // ── MAX30101 ──
  sl_status_t max_sc = app_max30101_read(&latest_heart_rate,
                                          &latest_confidence,
                                          &latest_oxygen,
                                          &latest_finger);
  if (max_sc == SL_STATUS_OK) {
    app_log("[MAX30101] OK | HR=%u bpm | SpO2=%u%% | "
            "Conf=%u%% | Finger=%u\n",
            latest_heart_rate, latest_oxygen,
            latest_confidence, latest_finger);
  } else if (max_sc == SL_STATUS_NOT_READY) {
    app_log("[MAX30101] Not ready\n");
  } else {
    app_log("[MAX30101] WARN: read failed! (0x%lx)\n", (uint32_t)max_sc);
  }

  // ── MLX90632 ──
  double ambient_temp = 0.0;
  if (app_mlx90632_read(&ambient_temp, &latest_body_temp) == SL_STATUS_OK) {
    app_log("[MLX90632] OK | Ambient: %.2f C | Object: %.2f C\n",
            ambient_temp, latest_body_temp);
  } else {
    app_log("[MLX90632] WARN: read failed!\n");
  }

  // ── SPS30 ──
  sps30_measurement_t temp_pm = { 0 };
  if (app_sps30_read(&temp_pm) == SL_STATUS_OK) {
    latest_pm       = temp_pm;
    latest_pm_valid = true;
    app_log("[SPS30] OK | PM1.0=%.2f PM2.5=%.2f "
            "PM4.0=%.2f PM10=%.2f ug/m3\n",
            temp_pm.mc_1p0, temp_pm.mc_2p5,
            temp_pm.mc_4p0, temp_pm.mc_10p0);
  } else {
    app_log("[SPS30] WARN: read failed!\n");
  }

  // ── SCD41 ──
  uint16_t    new_co2 = 0;
  sl_status_t co2_sc  = app_scd41_read_co2(&new_co2);
  if (co2_sc == SL_STATUS_OK) {
    latest_co2_ppm = new_co2;
    app_log("[SCD41] OK | CO2: %u ppm\n", latest_co2_ppm);
  } else if (co2_sc == SL_STATUS_NOT_READY) {
    app_log("[SCD41] Not ready, last: %u ppm\n", latest_co2_ppm);
  } else {
    app_log("[SCD41] WARN: read failed! (0x%lx)\n", (uint32_t)co2_sc);
  }

  // ── CẬP NHẬT OLED ──
  // Chỉ cập nhật data bình thường khi KHÔNG có cảnh báo ngã
  // (Khi có ngã, OLED đang hiện fall alert — không ghi đè)
  if (fall_ui_state == APP_FALL_STATE_NORMAL) {
    app_oled_update_air_quality(
        latest_voc_index,
        latest_humidity,
        latest_temperature,
        latest_heart_rate,
        latest_confidence,
        latest_oxygen,
        latest_finger,
        latest_body_temp,
        latest_co2_ppm,
        latest_pm_valid ? &latest_pm : NULL
    );
  }

  app_log("----------------------------------------\n");
}*/



/*#include "oled_app.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "app_rht.h"
#include "app_sgp40.h"
#include "app_max30101.h"
#include "app_mlx90632.h"
#include "app_scd41.h"
#include "app_sps30.h"
#include "app_fall_detection.h"
#include "sl_simple_button_instances.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"
#include "em_gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "sl_iostream_init_eusart_instances.h"

#define READING_INTERVAL_MSEC     1000
#define FALL_CHECK_INTERVAL_MSEC  10
#define SENSOR_WARMUP_CYCLES      3
#define FALL_RECOVERY_WINDOW_SEC  5

// ── Timer handles ──
static sl_sleeptimer_timer_handle_t app_timer_handle;
static sl_sleeptimer_timer_handle_t fall_timer_handle;

// ── Flags ──
static volatile bool app_timer_expire   = false;
static volatile bool fall_timer_expire  = false;
static volatile bool fall_recovery_flag = false;

// ── Trạng thái ngã ──
typedef enum {
  APP_FALL_STATE_NORMAL,
  APP_FALL_STATE_ALERT_COUNTING,
  APP_FALL_STATE_ALERT_LOCKED,
} app_fall_ui_state_t;

static volatile app_fall_ui_state_t fall_ui_state      = APP_FALL_STATE_NORMAL;
static          uint32_t            fall_alert_start_ms = 0;

// ── Lưu giá trị sensor mới nhất ──
static int32_t             latest_temperature = 0;
static uint32_t            latest_humidity    = 0;
static int32_t             latest_voc_index   = -1;
static uint16_t            latest_heart_rate  = 0;
static uint8_t             latest_confidence  = 0;
static uint16_t            latest_oxygen      = 0;
static uint8_t             latest_finger      = 0;
static double              latest_body_temp   = 0.0;
static uint16_t            latest_co2_ppm     = 0;
static sps30_measurement_t latest_pm          = { 0 };
static bool                latest_pm_valid    = false;

// ── MAX30101 fail counter — phải là static toàn cục, không để trong hàm ──
static uint8_t max30101_fail_count = 0;

// ── Timer callbacks ──
static void app_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;
  app_timer_expire = true;
}

static void fall_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;
  fall_timer_expire = true;
}

// ── Helper: lấy thời gian hiện tại (ms) ──
static uint32_t get_tick_ms(void)
{
  return sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
}

// ── Helper: cập nhật OLED với data hiện tại ──
static void oled_refresh_normal(void)
{
  app_oled_update_air_quality(
      latest_voc_index,
      latest_humidity,
      latest_temperature,
      latest_heart_rate,
      latest_confidence,
      latest_oxygen,
      latest_finger,
      latest_body_temp,
      latest_co2_ppm,
      latest_pm_valid ? &latest_pm : NULL
  );
}

// ── Helper: kích hoạt cảnh báo ngã ──
static void fall_trigger_alert(void)
{
  fall_ui_state       = APP_FALL_STATE_ALERT_COUNTING;
  fall_alert_start_ms = get_tick_ms();
  fall_recovery_flag  = false;

  app_log("========================================\n");
  app_log("[FALL] !!! PHAT HIEN NGA !!!\n");
  app_log("[FALL] Cho %d giay de tu dung day...\n",
          FALL_RECOVERY_WINDOW_SEC);
  app_log("[FALL] Log sensor TAM DUNG.\n");
  app_log("========================================\n");

  app_oled_show_fall_alert();
}

// ── Button handler ──
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {

    // BTN0: Hủy cảnh báo ngã
    if (handle == &sl_button_btn0) {
      if (fall_ui_state == APP_FALL_STATE_ALERT_COUNTING
          || fall_ui_state == APP_FALL_STATE_ALERT_LOCKED) {
        uint32_t elapsed = get_tick_ms() - fall_alert_start_ms;
        app_log("========================================\n");
        app_log("[FALL] BTN0 nhan sau %lu ms.\n", elapsed);
        app_log("[FALL] Nguoi dung huy canh bao thu cong.\n");
        app_log("[FALL] [SMS] Gui tin nhan canh bao nga!\n");
        app_log("[FALL] Log sensor TIEP TUC.\n");
        app_log("========================================\n");

        fall_ui_state = APP_FALL_STATE_NORMAL;
        app_fall_detection_reset();
        oled_refresh_normal();
      }
    }

    // BTN1: Chuyển trang OLED (chỉ khi bình thường)
    if (handle == &sl_button_btn1) {
      if (fall_ui_state == APP_FALL_STATE_NORMAL) {
        if (oled_current_page == OLED_PAGE_MAIN) {
          oled_current_page = OLED_PAGE_AIR;
          app_log("[OLED] Chuyen sang trang ENV\n");
        } else {
          oled_current_page = OLED_PAGE_MAIN;
          app_log("[OLED] Chuyen sang trang HEALTH\n");
        }
        oled_refresh_normal();
      }
    }
  }
}

// ── Init ──
void app_init(void)
{
#if defined(SL_CATALOG_IOSTREAM_EUSART_PRESENT)
  sl_iostream_set_default(sl_iostream_vcom_handle);
  app_log_iostream_set(sl_iostream_vcom_handle);
#endif

  app_log("=== [FINAL] All Sensors + OLED + Fall Detection ===\n");
  sl_sleeptimer_delay_millisecond(500);

  if (app_oled_init() != SL_STATUS_OK) {
    app_log("[OLED] Init FAILED!\n");
  } else {
    app_log("[OLED] Init OK\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_rht_init() != SL_STATUS_OK) {
    app_log("[RHT] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  sl_sleeptimer_delay_millisecond(2000);
  if (app_sgp40_init() == SL_STATUS_OK) {
    app_log("[SGP40] Init OK!\n");
  } else {
    app_log("[SGP40] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_max30101_init() == SL_STATUS_OK) {
    app_log("[MAX30101] Init OK!\n");
    sl_sleeptimer_delay_millisecond(2000);
  } else {
    app_log("[MAX30101] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_mlx90632_init() == SL_STATUS_OK) {
    app_log("[MLX90632] Init OK!\n");
  } else {
    app_log("[MLX90632] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  sl_sleeptimer_delay_millisecond(5000);
  if (app_scd41_init() == SL_STATUS_OK) {
    app_log("[SCD41] Init OK!\n");
  } else {
    app_log("[SCD41] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_sps30_init() == SL_STATUS_OK) {
    app_log("[SPS30] Init OK!\n");
  } else {
    app_log("[SPS30] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  if (app_fall_detection_init() == SL_STATUS_OK) {
    app_log("[FALL] Init OK!\n");
  } else {
    app_log("[FALL] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  sl_sleeptimer_start_periodic_timer_ms(&app_timer_handle,
                                        READING_INTERVAL_MSEC,
                                        app_timer_cb,
                                        (void *) NULL, 0, 0);

  sl_sleeptimer_start_periodic_timer_ms(&fall_timer_handle,
                                        FALL_CHECK_INTERVAL_MSEC,
                                        fall_timer_cb,
                                        (void *) NULL, 0, 0);

  app_log("=== Init done ===\n");
}

// ── Main loop ──
void app_process_action(void)
{
  static uint8_t warmup_counter = 0;

  // ── FALL DETECTION — 100Hz ──
  if (fall_timer_expire) {
    fall_timer_expire = false;

    switch (fall_ui_state) {

      case APP_FALL_STATE_NORMAL:
        if (app_fall_detection_check()) {
          fall_trigger_alert();
        }
        break;

      case APP_FALL_STATE_ALERT_COUNTING: {
        uint32_t elapsed_ms  = get_tick_ms() - fall_alert_start_ms;
        uint32_t elapsed_sec = elapsed_ms / 1000;

        if (elapsed_sec < FALL_RECOVERY_WINDOW_SEC) {
          if (app_fall_detection_check_recovery()) {
            app_log("========================================\n");
            app_log("[FALL] Nguoi dung TU DUNG DAY sau %lu ms!\n",
                    elapsed_ms);
            app_log("[FALL] OLED se chuyen ve binh thuong.\n");
            app_log("[FALL] Log sensor TIEP TUC.\n");
            app_log("========================================\n");

            fall_ui_state      = APP_FALL_STATE_NORMAL;
            fall_recovery_flag = true;
            app_fall_detection_reset();
          }
        } else {
          fall_ui_state = APP_FALL_STATE_ALERT_LOCKED;
          app_log("========================================\n");
          app_log("[FALL] Het %d giay — KHONG dung day!\n",
                  FALL_RECOVERY_WINDOW_SEC);
          app_log("[FALL] GIU NGUYEN man hinh canh bao.\n");
          app_log("[FALL] [SMS] Gui tin nhan canh bao nga!\n");
          app_log("[FALL] Nhan BTN0 de huy canh bao.\n");
          app_log("========================================\n");
        }
        break;
      }

      case APP_FALL_STATE_ALERT_LOCKED:
        break;

      default:
        fall_ui_state = APP_FALL_STATE_NORMAL;
        break;
    }
  }

  // ── Xử lý recovery flag ở main loop ──
  if (fall_recovery_flag) {
    fall_recovery_flag = false;
    app_log("[FALL] OLED chuyen ve man hinh binh thuong ngay.\n");
    oled_refresh_normal();
    return;
  }

  // ── SENSOR READING — 1Hz ──
  if (!app_timer_expire) return;
  app_timer_expire = false;

  if (fall_ui_state == APP_FALL_STATE_ALERT_COUNTING) {
    uint32_t elapsed_sec = (get_tick_ms() - fall_alert_start_ms) / 1000;
    uint32_t remain_sec  = (elapsed_sec < (uint32_t)FALL_RECOVERY_WINDOW_SEC)
                           ? ((uint32_t)FALL_RECOVERY_WINDOW_SEC - elapsed_sec)
                           : 0;
    app_log("[FALL] Dang cho dung day — con %lu giay...\n", remain_sec);
    return;
  }

  if (fall_ui_state == APP_FALL_STATE_ALERT_LOCKED) {
    app_log("[FALL] Man hinh canh bao nga — nhan BTN0 de huy.\n");
    return;
  }

  if (warmup_counter < SENSOR_WARMUP_CYCLES) {
    warmup_counter++;
    app_log("[WARMUP] %d/%d\n", warmup_counter, SENSOR_WARMUP_CYCLES);
    return;
  }

  // ── RHT — FIX: Đọc trước, log sau, đủ 4 tham số ──
  if (app_rht_read(&latest_humidity, &latest_temperature) == SL_STATUS_OK) {
    app_log("[RHT] OK | Temp: %ld.%03ld C | Hum: %lu.%03lu %%\n",
            latest_temperature / 1000,
            (uint32_t)(latest_temperature % 1000),
            latest_humidity    / 1000,
            latest_humidity    % 1000);
  } else {
    app_log("[RHT] WARN: read failed!\n");
  }

  // ── SGP40 ──
  if (app_sgp40_read(&latest_voc_index) == SL_STATUS_OK) {
    app_log("[SGP40] OK | VOC Index: %ld\n", latest_voc_index);
  } else {
    app_log("[SGP40] WARN: read failed!\n");
  }

  // ── MAX30101 — FIX: Lọc Conf >= 60% + SpO2 > 0 + Finger == 3 ──
  sl_status_t max_sc = app_max30101_read(&latest_heart_rate,
                                          &latest_confidence,
                                          &latest_oxygen,
                                          &latest_finger);
  if (max_sc == SL_STATUS_OK) {
    max30101_fail_count = 0;

    if (latest_finger == 3
        && latest_confidence >= 60
        && latest_oxygen > 0) {
      // Dữ liệu ổn định — hiển thị
      app_log("[MAX30101] OK | HR=%u bpm | SpO2=%u%% | "
              "Conf=%u%% | Finger=%u\n",
              latest_heart_rate, latest_oxygen,
              latest_confidence, latest_finger);
    } else {
      // Dữ liệu chưa ổn định
      app_log("[MAX30101] Data chua on dinh | "
              "HR=%u SpO2=%u Conf=%u Finger=%u\n",
              latest_heart_rate, latest_oxygen,
              latest_confidence, latest_finger);
      // Chỉ reset khi không có ngón tay
      if (latest_finger == 0) {
        latest_heart_rate = 0;
        latest_oxygen     = 0;
      }
    }
  } else if (max_sc == SL_STATUS_NOT_READY) {
    app_log("[MAX30101] Not ready\n");
  } else {
    max30101_fail_count++;
    app_log("[MAX30101] WARN: read failed! (0x%lx) [%u/3]\n",
            (uint32_t)max_sc, max30101_fail_count);

    if (max30101_fail_count >= 3) {
      max30101_fail_count = 0;
      app_log("[MAX30101] Thu re-init...\n");
      if (app_max30101_init() == SL_STATUS_OK) {
        app_log("[MAX30101] Re-init OK!\n");
      } else {
        app_log("[MAX30101] Re-init FAILED!\n");
      }
      latest_heart_rate = 0;
      latest_oxygen     = 0;
      latest_confidence = 0;
      latest_finger     = 0;
    }
  }

  // ── MLX90632 — FIX: Ngưỡng 2°C ──
  double ambient_temp = 0.0;
  if (app_mlx90632_read(&ambient_temp, &latest_body_temp) == SL_STATUS_OK) {
    app_log("[MLX90632] OK | Ambient: %.2f C | Object: %.2f C\n",
            ambient_temp, latest_body_temp);

    if ((latest_body_temp - ambient_temp) < 2.0) {
      latest_body_temp = 0.0;
      app_log("[MLX90632] Chua co tay — body temp reset ve 0\n");
    }
  } else {
    app_log("[MLX90632] WARN: read failed!\n");
  }

  // ── SPS30 ──
  sps30_measurement_t temp_pm = { 0 };
  if (app_sps30_read(&temp_pm) == SL_STATUS_OK) {
    latest_pm       = temp_pm;
    latest_pm_valid = true;
    app_log("[SPS30] OK | PM1.0=%.2f PM2.5=%.2f "
            "PM4.0=%.2f PM10=%.2f ug/m3\n",
            temp_pm.mc_1p0, temp_pm.mc_2p5,
            temp_pm.mc_4p0, temp_pm.mc_10p0);
  } else {
    app_log("[SPS30] WARN: read failed!\n");
  }

  // ── SCD41 ──
  uint16_t    new_co2 = 0;
  sl_status_t co2_sc  = app_scd41_read_co2(&new_co2);
  if (co2_sc == SL_STATUS_OK) {
    latest_co2_ppm = new_co2;
    app_log("[SCD41] OK | CO2: %u ppm\n", latest_co2_ppm);
  } else if (co2_sc == SL_STATUS_NOT_READY) {
    app_log("[SCD41] Not ready, last: %u ppm\n", latest_co2_ppm);
  } else {
    app_log("[SCD41] WARN: read failed! (0x%lx)\n", (uint32_t)co2_sc);
  }

  // ── CẬP NHẬT OLED ──
  oled_refresh_normal();

  app_log("----------------------------------------\n");
}*/



#include "App_a7680c.h"
#include "oled_app.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "app_rht.h"
#include "app_sgp40.h"
#include "app_max30101.h"
#include "app_mlx90632.h"
#include "app_scd41.h"
#include "app_sps30.h"
#include "app_fall_detection.h"
#include "sl_simple_button_instances.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"
#include "em_gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "sl_iostream_init_eusart_instances.h"
#include "app_buzzer.h"

#define READING_INTERVAL_MSEC     1000
#define MQTT_PUBLISH_INTERVAL_MSEC 5000
#define FALL_CHECK_INTERVAL_MSEC  10
#define SENSOR_WARMUP_CYCLES      3
#define FALL_RECOVERY_WINDOW_SEC  5
#define EMERGENCY_PHONE           "+8466752645"

// ── Nguong canh bao (VUI LONG DIEU CHINH THEO THUC TE) ──
#define CO2_THRESHOLD_PPM       1000
#define VOC_THRESHOLD_INDEX     300
#define SPO2_THRESHOLD_PERCENT  90
#define BODY_TEMP_THRESHOLD_C   38.0
#define PM2_5_THRESHOLD_UGM3    40.0f


// ── Chế độ SMS ──
// 1 = log only (khong gui that)
// 0 = gui SMS THAT qua A7680C
#define SMS_LOG_ONLY              0

// ── Timer handles ──
static sl_sleeptimer_timer_handle_t app_timer_handle;
static sl_sleeptimer_timer_handle_t fall_timer_handle;

// ── Flags ──
static volatile bool app_timer_expire   = false;
static volatile bool fall_timer_expire  = false;
static volatile bool fall_recovery_flag = false;

// ── Flag chống gửi SMS 2 lần khi LOCKED ──
static bool fall_sms_sent = false;

// ── Flag chống lặp log khi da LOCKED ──
static bool fall_locked_log_shown = false;

// ── Trạng thái ngã ──
typedef enum {
  APP_FALL_STATE_NORMAL,
  APP_FALL_STATE_ALERT_COUNTING,
  APP_FALL_STATE_ALERT_LOCKED,
} app_fall_ui_state_t;

static volatile app_fall_ui_state_t fall_ui_state      = APP_FALL_STATE_NORMAL;
static          uint32_t            fall_alert_start_ms = 0;

// ── Lưu giá trị sensor mới nhất ──
static int32_t  latest_temperature = 0;
static uint32_t latest_humidity    = 0;
static int32_t  latest_voc_index   = -1;
static uint16_t latest_heart_rate  = 0;
static uint8_t  latest_confidence  = 0;
static uint16_t latest_oxygen      = 0;
static uint8_t  latest_finger      = 0;
static double   latest_body_temp   = 0.0;
static uint16_t latest_co2_ppm     = 0;
static float latest_pm2_5 = 0.0f;
static uint32_t mqtt_publish_counter = 0;

// ── A7680C status ──
static bool a7680c_is_ready = false;

// ── Timer callbacks ──
static void app_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;
  app_timer_expire = true;
}

static void fall_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void) handle;
  (void) data;
  fall_timer_expire = true;
}

// ── Helper: lấy thời gian hiện tại (ms) ──
static uint32_t get_tick_ms(void)
{
  return sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
}

// ── Helper: kiem tra va canh bao chat luong khong khi ──
static void check_env_alert(void)
{
  bool alert = false;

  if (latest_co2_ppm >= CO2_THRESHOLD_PPM) {
    app_log("[ENV] CANH BAO: CO2 cao (%u ppm)\n", latest_co2_ppm);
    alert = true;
  }
  if (latest_voc_index >= VOC_THRESHOLD_INDEX) {
    app_log("[ENV] CANH BAO: VOC cao (%ld)\n", latest_voc_index);
    alert = true;
  }
  if (latest_pm2_5 >= PM2_5_THRESHOLD_UGM3) {
      app_log("[ENV] CANH BAO: PM2.5 cao (%0.2f ug/m3)\n", latest_pm2_5);
      alert = true;
    }

  if (alert) {
    buzzer_beep_env_alert();   // Keu 4 tieng ngan
  }
}

// ── Helper: kiem tra va canh bao suc khoe ──
static void check_health_alert(void)
{
  bool alert = false;

  if (latest_oxygen > 0 && latest_oxygen < SPO2_THRESHOLD_PERCENT) {
    app_log("[HEALTH] CANH BAO: SpO2 thap (%u%%)\n", latest_oxygen);
    alert = true;
  }
  if (latest_body_temp >= BODY_TEMP_THRESHOLD_C) {
    app_log("[HEALTH] CANH BAO: Sot cao (%.2f C)\n", latest_body_temp);
    alert = true;
  }

  if (alert) {
    buzzer_beep_health_alert();   // Keu 3 tieng ngan
  }
}

// ── Helper: cập nhật OLED ──
static void oled_refresh_normal(void)
{
  app_oled_update_air_quality(
      latest_voc_index,
      latest_humidity,
      latest_temperature,
      latest_heart_rate,
      latest_confidence,
      latest_oxygen,
      latest_finger,
      latest_body_temp,
      latest_co2_ppm,
      latest_pm2_5
  );
}

// ── Helper: gửi SMS ──
static void a7680c_send_sms_log(const char *phone, const char *msg)
{
#if SMS_LOG_ONLY
  app_log("========================================\n");
  app_log("[SMS-LOG] >>> GIA LAP GUI SMS <<<\n");
  app_log("[SMS-LOG] To : %s\n", phone);
  app_log("[SMS-LOG] Msg: %s\n", msg);
  app_log("[SMS-LOG] (SMS_LOG_ONLY=1 — chi log, khong gui that)\n");
  app_log("========================================\n");
#else
  if (a7680c_is_ready) {
    app_log("[SMS] Dang gui SMS that den: %s\n", phone);
    app_log("[SMS] Noi dung: %s\n", msg);
    sl_status_t ret = App_a7680c_send_sms(phone, msg);
    if (ret == SL_STATUS_OK) {
      app_log("[SMS] GUI THANH CONG!\n");
    } else {
      app_log("[SMS] GUI THAT BAI! (0x%lx)\n", (uint32_t)ret);
      app_log("[SMS] Kiem tra log A7680C o tren de biet nguyen nhan.\n");
    }
  } else {
    app_log("[A7680C] WARN: Module chua san sang, bo qua SMS!\n");
  }
#endif
}

// ── Helper: tạo và gửi SMS cảnh báo ngã kèm vị trí Cell-ID ──
// CHI GOM 2 PHAN: "Phat Hien Nga!" + vi tri
static void fall_send_alert_sms(void)
{
  char loc_buf[100] = { 0 };
  char sms_buf[160] = { 0 };

  // Lay vi tri Cell-ID truoc khi gui SMS
  app_log("[FALL] Dang lay vi tri Cell-ID...\n");
  sl_status_t loc_ret = App_a7680c_get_cell_location(loc_buf,
                                                      sizeof(loc_buf));
  if (loc_ret != SL_STATUS_OK) {
    snprintf(loc_buf, sizeof(loc_buf), "Khong xac dinh");
    app_log("[FALL] Khong lay duoc vi tri Cell-ID\n");
  } else {
    app_log("[FALL] Vi tri: %s\n", loc_buf);
  }

  // Chi gom 2 phan: canh bao + vi tri
  snprintf(sms_buf, sizeof(sms_buf),
           "Phat Hien Nga! %s",
           loc_buf);

  app_log("[FALL] Do dai SMS: %d ky tu\n", (int)strlen(sms_buf));

  a7680c_send_sms_log(EMERGENCY_PHONE, sms_buf);
}

// ── Helper: kích hoạt cảnh báo ngã ──
// CHI hien thi OLED — CHUA gui SMS, cho 5s truoc
static void fall_trigger_alert(void)
{
  fall_ui_state       = APP_FALL_STATE_ALERT_COUNTING;
  fall_alert_start_ms = get_tick_ms();
  fall_recovery_flag  = false;
  fall_sms_sent       = false;
  fall_locked_log_shown  = false;   // ← thêm dòng này

  app_log("========================================\n");
  app_log("[FALL] !!! PHAT HIEN NGA !!!\n");
  app_log("[FALL] Cho %d giay de tu dung day...\n",
          FALL_RECOVERY_WINDOW_SEC);
  app_log("[FALL] Log sensor TAM DUNG.\n");
  app_log("========================================\n");


  // Bat buzzer keu LIEN TUC ngay khi phat hien nga
    buzzer_control(true);

  // Chi hien thi OLED — CHUA gui SMS
  app_oled_show_fall_alert();
}

// ── Button handler ──
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {

    // BTN0: Hủy cảnh báo ngã — CHI chuyen OLED ve binh thuong, KHONG gui SMS
    if (handle == &sl_button_btn0) {
      if (fall_ui_state == APP_FALL_STATE_ALERT_COUNTING
          || fall_ui_state == APP_FALL_STATE_ALERT_LOCKED) {

        uint32_t elapsed = get_tick_ms() - fall_alert_start_ms;
        app_log("========================================\n");
        app_log("[FALL] BTN0 nhan sau %lu ms.\n", elapsed);
        app_log("[FALL] Nguoi dung huy canh bao thu cong.\n");
        app_log("[FALL] OLED chuyen ve man hinh binh thuong.\n");
        app_log("[FALL] Log sensor TIEP TUC.\n");
        app_log("========================================\n");

        buzzer_control(false);
        fall_ui_state = APP_FALL_STATE_NORMAL;
        fall_sms_sent = false;
        fall_locked_log_shown  = false;   // ← thêm dòng này
        app_fall_detection_reset();
        oled_refresh_normal();
      }
    }

    // BTN1: Chuyển trang OLED (chỉ khi bình thường)
    if (handle == &sl_button_btn1) {
      if (fall_ui_state == APP_FALL_STATE_NORMAL) {
        if (oled_current_page == OLED_PAGE_MAIN) {
          oled_current_page = OLED_PAGE_AIR;
          app_log("[OLED] Chuyen sang trang ENV\n");
        } else {
          oled_current_page = OLED_PAGE_MAIN;
          app_log("[OLED] Chuyen sang trang HEALTH\n");
        }
        oled_refresh_normal();
      }
    }
  }
}

// ── Init ──
void app_init(void)
{
#if defined(SL_CATALOG_IOSTREAM_EUSART_PRESENT)
  sl_iostream_set_default(sl_iostream_vcom_handle);
  app_log_iostream_set(sl_iostream_vcom_handle);
#endif

  app_log("=== [FINAL] All Sensors + OLED + Fall Detection ===\n");
#if SMS_LOG_ONLY
  app_log("[A7680C] Che do: SMS LOG ONLY.\n");
#else
  app_log("[A7680C] Che do: GUI SMS THAT.\n");
#endif
  sl_sleeptimer_delay_millisecond(500);

  // ── OLED ──
  if (app_oled_init() != SL_STATUS_OK) {
    app_log("[OLED] Init FAILED!\n");
  } else {
    app_log("[OLED] Init OK\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── RHT ──
  if (app_rht_init() != SL_STATUS_OK) {
    app_log("[RHT] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── SGP40 ──
  sl_sleeptimer_delay_millisecond(2000);
  if (app_sgp40_init() == SL_STATUS_OK) {
    app_log("[SGP40] Init OK!\n");
  } else {
    app_log("[SGP40] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── MAX30101 ──
  if (app_max30101_init() == SL_STATUS_OK) {
    app_log("[MAX30101] Init OK!\n");
    sl_sleeptimer_delay_millisecond(2000);
  } else {
    app_log("[MAX30101] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── MLX90632 ──
  if (app_mlx90632_init() == SL_STATUS_OK) {
    app_log("[MLX90632] Init OK!\n");
  } else {
    app_log("[MLX90632] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── SCD41 ──
  sl_sleeptimer_delay_millisecond(5000);
  if (app_scd41_init() == SL_STATUS_OK) {
    app_log("[SCD41] Init OK!\n");
  } else {
    app_log("[SCD41] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── SPS30 (bui min PM2.5) ──
  if (app_sps30_init() == SL_STATUS_OK) {
    app_log("[SPS30] Init OK!\n");
  } else {
    app_log("[SPS30] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── Fall Detection ──
  if (app_fall_detection_init() == SL_STATUS_OK) {
    app_log("[FALL] Init OK!\n");
  } else {
    app_log("[FALL] Init FAILED!\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── A7680C — CHI init, KHONG gui SMS test ──
  app_log("[A7680C] Dang init module...\n");
  sl_sleeptimer_delay_millisecond(2000);
  if (App_a7680c_init() == SL_STATUS_OK) {
    a7680c_is_ready = true;
    app_log("[A7680C] Init OK! Module va mang san sang.\n");


// =====================================================
  // MQTT TEST
  // A7680C -> HiveMQ -> IoT MQTT Panel
  // =====================================================

  app_log("[MQTT] Bat dau test MQTT...\n");
  if (App_a7680c_mqtt_test() == SL_STATUS_OK) {

         app_log("[MQTT] TEST THANH CONG!\n");

     } else {

         app_log("[MQTT] TEST THAT BAI!\n");
     }
  } else {
    a7680c_is_ready = false;
    app_log("[A7680C] Init FAILED! Xem log o tren de biet nguyen nhan.\n");
    app_log("[A7680C] He thong van chay, SMS se bi bo qua.\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── Buzzer ──
  if (buzzer_init() != SL_STATUS_OK) {
    app_log("[BUZZER] Init FAILED!\n");
  } else {
    app_log("[BUZZER] Init OK\n");
  }
  sl_sleeptimer_delay_millisecond(100);

  // ── Timers ──
  sl_sleeptimer_start_periodic_timer_ms(&app_timer_handle,
                                        READING_INTERVAL_MSEC,
                                        app_timer_cb,
                                        (void *) NULL, 0, 0);

  sl_sleeptimer_start_periodic_timer_ms(&fall_timer_handle,
                                        FALL_CHECK_INTERVAL_MSEC,
                                        fall_timer_cb,
                                        (void *) NULL, 0, 0);

  app_log("=== Init done ===\n");
}

// ── Main loop ──
void app_process_action(void)
{
  static uint8_t warmup_counter = 0;

  // ── FALL DETECTION — 100Hz ──
  if (fall_timer_expire) {
    fall_timer_expire = false;

    switch (fall_ui_state) {

      case APP_FALL_STATE_NORMAL:
        if (app_fall_detection_check()) {
          fall_trigger_alert();
        }
        break;

      case APP_FALL_STATE_ALERT_COUNTING: {
        uint32_t elapsed_ms  = get_tick_ms() - fall_alert_start_ms;
        uint32_t elapsed_sec = elapsed_ms / 1000;

        if (elapsed_sec < FALL_RECOVERY_WINDOW_SEC) {
          // Con trong 5 giay — kiem tra tu dung day
          if (app_fall_detection_check_recovery()) {
            app_log("========================================\n");
            app_log("[FALL] Nguoi dung TU DUNG DAY sau %lu ms!\n", elapsed_ms);
            app_log("[FALL] OLED se chuyen ve binh thuong.\n");
            app_log("[FALL] Log sensor TIEP TUC.\n");
            app_log("========================================\n");


            buzzer_control(false);
            fall_ui_state      = APP_FALL_STATE_NORMAL;
            fall_recovery_flag = true;
            fall_sms_sent      = false;
            app_fall_detection_reset();
            // KHONG gui SMS — nguoi dung tu dung day
          }
        } else {
          // ── Het 5 giay → LOCKED + GUI SMS CANH BAO + VI TRI ──
          fall_ui_state = APP_FALL_STATE_ALERT_LOCKED;

          if (!fall_sms_sent) {
            fall_sms_sent = true;

            app_log("========================================\n");
            app_log("[FALL] Het %d giay — KHONG dung day!\n",
                    FALL_RECOVERY_WINDOW_SEC);
            app_log("[FALL] GUI SMS CANH BAO NGA + VI TRI!\n");
            app_log("========================================\n");



            // ── GỬI MQTT NGAY LẬP TỨC ──

             if (a7680c_is_ready) {

                 sl_status_t mqtt_ret = App_a7680c_mqtt_publish_health(
                     (float)latest_temperature / 1000.0f,
                     (float)latest_humidity / 1000.0f,
                     (int)latest_voc_index,
                     (int)latest_heart_rate,
                     (int)latest_oxygen,
                     (float)latest_body_temp,
                     (int)latest_co2_ppm,
                     latest_pm2_5,
                     1
                 );

                 if (mqtt_ret == SL_STATUS_OK) {
                     app_log("[MQTT] FALL ALERT PUBLISHED!\n");
                 } else {
                     app_log("[MQTT] FALL ALERT FAILED! (0x%lx)\n",
                             (uint32_t)mqtt_ret);
                 }
             }


            // Gui SMS canh bao nga kem vi tri Cell-ID
            fall_send_alert_sms();

            app_log("[FALL] GIU NGUYEN man hinh canh bao.\n");
            app_log("[FALL] Nhan BTN0 de huy canh bao.\n");
            app_log("========================================\n");
          }
        }
        break;
      }

      case APP_FALL_STATE_ALERT_LOCKED:
        // Giu nguyen man hinh canh bao, cho BTN0
        break;

      default:
        fall_ui_state = APP_FALL_STATE_NORMAL;
        break;
    }
  }

  // ── Xử lý recovery flag ──
  if (fall_recovery_flag) {
    fall_recovery_flag = false;
    app_log("[FALL] OLED chuyen ve man hinh binh thuong ngay.\n");
    oled_refresh_normal();
    return;
  }

  // ── SENSOR READING — 1Hz ──
  if (!app_timer_expire) return;
  app_timer_expire = false;

  if (fall_ui_state == APP_FALL_STATE_ALERT_COUNTING) {
    uint32_t elapsed_sec = (get_tick_ms() - fall_alert_start_ms) / 1000;
    uint32_t remain_sec  = (elapsed_sec < (uint32_t)FALL_RECOVERY_WINDOW_SEC)
                           ? ((uint32_t)FALL_RECOVERY_WINDOW_SEC - elapsed_sec)
                           : 0;
    app_log("[FALL] Dang cho dung day — con %lu giay...\n", remain_sec);
    return;
  }

  if (fall_ui_state == APP_FALL_STATE_ALERT_LOCKED) {
    if (!fall_locked_log_shown) {
      fall_locked_log_shown = true;
      app_log("[FALL] Man hinh canh bao nga — nhan BTN0 de huy.\n");
    }
    return;
  }


  if (warmup_counter < SENSOR_WARMUP_CYCLES) {
    warmup_counter++;
    app_log("[WARMUP] %d/%d\n", warmup_counter, SENSOR_WARMUP_CYCLES);
    return;
  }

  // ── RHT ──
  if (app_rht_read(&latest_humidity, &latest_temperature) == SL_STATUS_OK) {
    app_log("[RHT] OK | Temp: %ld.%03ld C | Hum: %lu.%03lu %%\n",
            latest_temperature / 1000,
            (uint32_t)(latest_temperature % 1000),
            latest_humidity    / 1000,
            latest_humidity    % 1000);
  } else {
    app_log("[RHT] WARN: read failed!\n");
  }

  // ── SGP40 ──
  if (app_sgp40_read(&latest_voc_index) == SL_STATUS_OK) {
    app_log("[SGP40] OK | VOC Index: %ld\n", latest_voc_index);
  } else {
    app_log("[SGP40] WARN: read failed!\n");
  }

  // ── MAX30101 ──
  sl_status_t max_sc = app_max30101_read(&latest_heart_rate,
                                          &latest_confidence,
                                          &latest_oxygen,
                                          &latest_finger);
  if (max_sc == SL_STATUS_OK) {
    app_log("[MAX30101] OK | HR=%u bpm | SpO2=%u%% | "
            "Conf=%u%% | Finger=%u\n",
            latest_heart_rate, latest_oxygen,
            latest_confidence, latest_finger);
  } else if (max_sc == SL_STATUS_NOT_READY) {
    app_log("[MAX30101] Not ready\n");
  } else {
    app_log("[MAX30101] WARN: read failed! (0x%lx)\n", (uint32_t)max_sc);
  }

  // ── MLX90632 ──
  double ambient_temp = 0.0;
  if (app_mlx90632_read(&ambient_temp, &latest_body_temp) == SL_STATUS_OK) {
    app_log("[MLX90632] OK | Ambient: %.2f C | Object: %.2f C\n",
            ambient_temp, latest_body_temp);
  } else {
    app_log("[MLX90632] WARN: read failed!\n");
  }

  // ── SCD41 ──
  uint16_t    new_co2 = 0;
  sl_status_t co2_sc  = app_scd41_read_co2(&new_co2);
  if (co2_sc == SL_STATUS_OK) {
    latest_co2_ppm = new_co2;
    app_log("[SCD41] OK | CO2: %u ppm\n", latest_co2_ppm);
  } else if (co2_sc == SL_STATUS_NOT_READY) {
    app_log("[SCD41] Not ready, last: %u ppm\n", latest_co2_ppm);
  } else {
    app_log("[SCD41] WARN: read failed! (0x%lx)\n", (uint32_t)co2_sc);
  }


  // ── SPS30 ──
  sps30_measurement_t sps30_data;
  sl_status_t sps30_sc = app_sps30_read(&sps30_data);
  if (sps30_sc == SL_STATUS_OK) {
    latest_pm2_5 = sps30_data.mc_2p5;
    app_log("[SPS30] OK | PM2.5: %0.2f ug/m3\n", latest_pm2_5);
  } else if (sps30_sc == SL_STATUS_NOT_READY) {
    app_log("[SPS30] Not ready, last: %0.2f ug/m3\n", latest_pm2_5);
  } else {
    app_log("[SPS30] WARN: read failed! (0x%lx)\n", (uint32_t)sps30_sc);
  }

  // ── KIỂM TRA CẢNH BÁO MÔI TRƯỜNG / SỨC KHỎE ──

  check_env_alert();
  check_health_alert();

  // ── CẬP NHẬT OLED ──

  oled_refresh_normal();

  // ── MQTT: GỬI DỮ LIỆU MỖI 5 GIÂY ──

  if (a7680c_is_ready) {

      mqtt_publish_counter += READING_INTERVAL_MSEC;

      if (mqtt_publish_counter >= MQTT_PUBLISH_INTERVAL_MSEC) {

          mqtt_publish_counter = 0;

          app_log("========================================\n");
          app_log("[MQTT] Gui du lieu suc khoe + moi truong...\n");

          sl_status_t mqtt_ret = App_a7680c_mqtt_publish_health(
              (float)latest_temperature / 1000.0f,
              (float)latest_humidity / 1000.0f,
              (int)latest_voc_index,
              (int)latest_heart_rate,
              (int)latest_oxygen,
              (float)latest_body_temp,
              (int)latest_co2_ppm,
              latest_pm2_5,
              0
          );

          if (mqtt_ret == SL_STATUS_OK) {
              app_log("[MQTT] Gui JSON THANH CONG!\n");
          } else {
              app_log("[MQTT] Gui JSON THAT BAI! (0x%lx)\n",
                      (uint32_t)mqtt_ret);
          }

          app_log("========================================\n");
      }
  }

  app_log("----------------------------------------\n");
}

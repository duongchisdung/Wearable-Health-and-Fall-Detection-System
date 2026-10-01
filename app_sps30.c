/*
 * sps_30.c
 *
 *  Created on: Aug 26, 2026
 *      Author: duong
 */


#include "app_sps30.h"
#include "sparkfun_sps30_i2c.h"
#include "sl_i2cspm_instances.h"
#include "sl_sleeptimer.h"
#include "app_log.h"


#define SPS30_MAX_PROBE_RETRIES     5
#define SPS30_PROBE_RETRY_DELAY_MS  1000

sl_status_t app_sps30_init(void)
{
  // 1. Khoi tao bus I2C cho SPS30
  sl_status_t stt = sps30_init(sl_i2cspm_sensor);
  if (stt != SL_STATUS_OK) {
    app_log("[SPS30] ERROR: sps30_init (I2C open) failed! (0x%lx)\r\n",
            (uint32_t)stt);
    return stt;
  }
  app_log("[SPS30] I2C init OK.\r\n");

  // 2. Kiem tra cam bien co phan hoi tren I2C hay khong
  sl_status_t probe_stt = SL_STATUS_FAIL;
  for (uint8_t retries = SPS30_MAX_PROBE_RETRIES; retries > 0; retries--) {
    probe_stt = sps30_probe();
    if (probe_stt == SL_STATUS_OK) {
      app_log("[SPS30] Probed successfully!\r\n");
      break;
    }
    app_log("[SPS30] Probe attempt failed! (0x%lx), retries left = %u\r\n",
            (uint32_t)probe_stt, (unsigned)(retries - 1));
    if (retries == 1) {
      app_log("[SPS30] NOT FOUND! (Kiem tra Pin 4 phai noi GND va nguon 5V)\r\n");
      app_log("[SPS30] Last probe error code: 0x%lx\r\n",
              (uint32_t)probe_stt);
      return SL_STATUS_NOT_FOUND;
    }
    sl_sleeptimer_delay_millisecond(SPS30_PROBE_RETRY_DELAY_MS);
  }

  // 3. Doc phien ban Firmware
  uint8_t fw_major = 0, fw_minor = 0;
  sl_status_t fw_stt = sps30_read_firmware_version(&fw_major, &fw_minor);
  if (fw_stt == SL_STATUS_OK) {
    app_log("[SPS30] FW Version: %u.%u\r\n", fw_major, fw_minor);
  } else {
    app_log("[SPS30] read_firmware_version failed! (0x%lx)\r\n",
            (uint32_t)fw_stt);
  }

  // 4. Doc Serial Number
  uint8_t serial[SPS30_MAX_SERIAL_LEN];
  sl_status_t serial_stt = sps30_get_serial((char *)serial);
  if (serial_stt == SL_STATUS_OK) {
    app_log("[SPS30] Serial: %s\r\n", serial);
  } else {
    app_log("[SPS30] get_serial failed! (0x%lx)\r\n", (uint32_t)serial_stt);
  }

  // 5. Cau hinh tu dong ve sinh buong do dinh ky (moi ngay 1 lan)
  sl_status_t clean_stt = sps30_set_fan_auto_cleaning_interval_days(1);
  if (clean_stt != SL_STATUS_OK) {
    app_log("[SPS30] set_fan_auto_cleaning_interval_days failed! (0x%lx)\r\n",
            (uint32_t)clean_stt);
  }

  // 6. Bat dau do
  sl_status_t stop_stt = sps30_stop_measurement();
  if (stop_stt != SL_STATUS_OK) {
    app_log("[SPS30] stop_measurement warning: (0x%lx)\r\n",
            (uint32_t)stop_stt);
  }
  sl_sleeptimer_delay_millisecond(50);

  stt = sps30_start_measurement();
  if (stt != SL_STATUS_OK) {
    app_log("[SPS30] start_measurement FAILED! (0x%lx)\r\n", (uint32_t)stt);
    return stt;
  }

  app_log("[SPS30] Measurement started successfully!\r\n");
  return SL_STATUS_OK;
}

sl_status_t app_sps30_read(sps30_measurement_t *measure)
{
  if (measure == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Kiem tra co data ready truoc khi doc du lieu moi
  uint16_t data_ready = 0;
  sl_status_t stt = sps30_read_data_ready(&data_ready);
  if (stt != SL_STATUS_OK) {
    app_log("[SPS30] read_data_ready failed! (0x%lx)\r\n", (uint32_t)stt);
    return stt;
  }
  if (data_ready == 0) {
    return SL_STATUS_NOT_READY; // Chua co mau do moi, khong doc
  }

  stt = sps30_read_measurement(measure);
  if (stt != SL_STATUS_OK) {
    app_log("[SPS30] read_measurement failed! (0x%lx)\r\n", (uint32_t)stt);
  }

  return stt;
}

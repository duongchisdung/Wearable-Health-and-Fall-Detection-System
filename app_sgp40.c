/*
 * app_sgp40.c
 *
 *  Created on: Aug 14, 2026
 *      Author: duong
 */

#include "app_sgp40.h"
#include "sparkfun_sgp40.h"
#include "sl_i2cspm_instances.h"
#include "app_log.h"

sl_status_t app_sgp40_init(void)
{
  sl_status_t ret;
  uint16_t results;

  // Bước 1: Khởi tạo I2C cho SGP40
  ret = sparkfun_sgp40_init(sl_i2cspm_qwiic);
  if (ret != SL_STATUS_OK) {
    app_log("[SGP40] ERROR: sparkfun_sgp40_init failed! (0x%lx)\n",
            (uint32_t)ret);
    return SL_STATUS_FAIL;
  }
  app_log("[SGP40] I2C init OK.\n");

  // Bước 2: Chạy self-test nội bộ (kiểm tra hotplate + MOX material)
  ret = sparkfun_sgp40_measure_test(&results);
  if (ret != SL_STATUS_OK) {
    // Loi o tang giao tiep I2C (khong doc duoc du lieu tu cam bien)
    app_log("[SGP40] ERROR: measure_test I2C communication failed! (0x%lx)\n",
            (uint32_t)ret);
    app_log("[SGP40] -> Kiem tra day SDA/SCL, nguon, dia chi I2C.\n");
    return ret;
  }

  app_log("[SGP40] measure_test raw result = 0x%04X "
          "(expect 0xD400 = PASSED, 0x4B00 = FAILED)\n", results);

  if (results != SPARKFUN_SGP40_TEST_PASSED) {
    // Giao tiep I2C thanh cong nhung ket qua self-test khong dat
    app_log("[SGP40] ERROR: Self-test result FAILED (0x%04X)!\n", results);
    app_log("[SGP40] -> Co the do hotplate/MOX material loi, "
            "hoac nguon cap khong on dinh trong luc heater hoat dong.\n");
    return SL_STATUS_NOT_READY;
  }
  app_log("[SGP40] Self-test PASSED.\n");

  // Bước 3: Tắt heater sau self-test
  ret = sparkfun_sgp40_heater_off();
  if (ret != SL_STATUS_OK) {
    app_log("[SGP40] ERROR: heater_off failed! (0x%lx)\n", (uint32_t)ret);
    return ret;
  }
  app_log("[SGP40] Heater off OK.\n");

  // Bước 4: Khởi tạo thuật toán VOC
  sparkfun_sgp40_voc_algorithm_init();
  app_log("[SGP40] VOC algorithm init OK.\n");

  app_log("[SGP40] Init hoan tat thanh cong!\n");
  return SL_STATUS_OK;
}

sl_status_t app_sgp40_read(int32_t *voc_index)
{
  sl_status_t ret = sparkfun_sgp40_get_voc_index(voc_index, 50, 25);

  if (ret != SL_STATUS_OK) {
    app_log("[SGP40] ERROR: get_voc_index failed! (0x%lx)\n", (uint32_t)ret);
  }

  return ret;
}

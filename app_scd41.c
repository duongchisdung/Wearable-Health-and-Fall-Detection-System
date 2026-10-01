/*
 * app_scd41.c
 *
 *  Created on: Aug 26, 2026
 *      Author: duong
 */


#include "app_scd41.h"
#include "mikroe_scd41.h"
#include "sl_i2cspm_instances.h"
#include "sl_sleeptimer.h"
#include "app_log.h"

#define SCD41_MEASUREMENT_INTERVAL_MS   5000

static uint32_t scd41_last_tick   = 0;
static bool     scd41_first_read  = true;
static bool     scd41_running     = false;
static uint16_t scd41_last_co2    = 0;

sl_status_t app_scd41_init(void)
{
  sl_status_t stt;

  stt = mikroe_scd41_init(sl_i2cspm_qwiic);
  if (stt != SL_STATUS_OK) {
    app_log("SCD41 init failed: 0x%lx\r\n", (uint32_t)stt);
    return stt;
  }

  mikroe_scd41_send_cmd(MIKROE_SCD41_STOP_PERIODIC_MEASUREMENT);
  sl_sleeptimer_delay_millisecond(500);

  mikroe_scd41_send_cmd(MIKROE_SCD41_PERFORM_FACTORY_RESET);
  sl_sleeptimer_delay_millisecond(2000);

  uint16_t ser_num[3] = { 0 };
  stt = mikroe_scd41_get_serial_number(ser_num);
  if (stt == SL_STATUS_OK) {
    app_log("SCD41 Serial Number: %.4d-%.4d-%.4d\r\n",
            ser_num[0], ser_num[1], ser_num[2]);
  } else {
    app_log("SCD41 get serial failed: 0x%lx\r\n", (uint32_t)stt);
    return stt;
  }

  mikroe_scd41_send_cmd(MIKROE_SCD41_START_PERIODIC_MEASUREMENT);

  scd41_last_tick  = sl_sleeptimer_get_tick_count();
  scd41_first_read = true;
  scd41_running    = true;
  scd41_last_co2   = 0;

  app_log("SCD41 periodic measurement started!\r\n");
  return SL_STATUS_OK;
}

sl_status_t app_scd41_read_co2(uint16_t *co2_ppm)
{
  if (co2_ppm == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (!scd41_running) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  uint32_t now        = sl_sleeptimer_get_tick_count();
  uint32_t elapsed_ms = sl_sleeptimer_tick_to_ms(now - scd41_last_tick);
  if (elapsed_ms < SCD41_MEASUREMENT_INTERVAL_MS) {
    *co2_ppm = scd41_last_co2;
    return SL_STATUS_NOT_READY;
  }
  scd41_last_tick = sl_sleeptimer_get_tick_count();

  if (scd41_first_read) {
    scd41_first_read = false;
    app_log("[SCD41] First interval after reset, skipping.\r\n");
    *co2_ppm = scd41_last_co2;
    return SL_STATUS_NOT_READY;
  }

  mikroe_scd41_measuremen_data_t scd41_data = { 0 };
  sl_status_t stt = mikroe_scd41_read_measurement(&scd41_data);
  if (stt != SL_STATUS_OK) {
    app_log("[WARN] SCD41 read_measurement failed: 0x%lx\r\n",
            (uint32_t)stt);
    *co2_ppm = scd41_last_co2;
    return SL_STATUS_TRANSMIT;
  }

  app_log("SCD41 Raw: CO2=%u ppm | T=%.2f C | RH=%.2f %%\r\n",
          scd41_data.co2_concent,
          scd41_data.temperature,
          scd41_data.r_humidity);

  // Validate nhiet do
  if (scd41_data.temperature < 10.0f || scd41_data.temperature > 60.0f) {
    app_log("[WARN] SCD41 T invalid (%.2f C), rejected.\r\n",
            scd41_data.temperature);
    *co2_ppm = scd41_last_co2;
    return SL_STATUS_NOT_READY;
  }

  // Validate do am
  if (scd41_data.r_humidity < 1.0f || scd41_data.r_humidity > 100.0f) {
    app_log("[WARN] SCD41 RH invalid (%.2f %%), rejected.\r\n",
            scd41_data.r_humidity);
    *co2_ppm = scd41_last_co2;
    return SL_STATUS_NOT_READY;
  }

  // Validate CO2
  if (scd41_data.co2_concent == 0) {
    app_log("[WARN] SCD41 CO2 = 0, not ready.\r\n");
    *co2_ppm = scd41_last_co2;
    return SL_STATUS_NOT_READY;
  }

  scd41_last_co2 = scd41_data.co2_concent;
  *co2_ppm = scd41_last_co2;
  return SL_STATUS_OK;
}

/*
 * app_max30101.c
 *
 *  Created on: Aug 17, 2026
 *      Author: duong
 */


#include "app_max30101.h"
#include "app_log.h"
#include "sparkfun_max30101_max32664.h"
#include "sl_i2cspm_instances.h"
#include "sl_sleeptimer.h"
#include <stdint.h>
#include <stdbool.h>

sl_status_t app_max30101_init(void)
{
  sl_status_t ret;

  ret = bio_hub_init(sl_i2cspm_qwiic, 0);
  app_log("bio_hub_init ret=0x%lx\n", ret);
  if (ret != SL_STATUS_OK) {
    app_log("MAX30101 init failed!\n");
    return SL_STATUS_FAIL;
  }

  ret = bio_hub_config_bpm(BIO_HUB_ALGO_MODE_ONE);
  app_log("bio_hub_config_bpm ret=0x%lx\n", ret);
  if (ret != SL_STATUS_OK) {
    app_log("MAX30101 config failed!\n");
    return SL_STATUS_FAIL;
  }
  sl_sleeptimer_delay_millisecond(2000);

  ret = bio_hub_set_pulse_width(MAX30101_PULSE_WIDTH_411);
  app_log("set_pulse_width ret=0x%lx\n", ret);
  if (ret != SL_STATUS_OK) {
    app_log("Set pulse width failed!\n");
    return SL_STATUS_FAIL;
  }

  ret = bio_hub_set_sample_rate(MAX30101_SAMPLE_RATE_400);
  app_log("set_sample_rate ret=0x%lx\n", ret);
  if (ret != SL_STATUS_OK) {
    app_log("Set sample rate failed!\n");
    return SL_STATUS_FAIL;
  }

  app_log("Load buffer....\n");
  sl_sleeptimer_delay_millisecond(4000);
  app_log("MAX30101 init OK!\n");
  return SL_STATUS_OK;
}

sl_status_t app_max30101_read(uint16_t *heart_rate,
                              uint8_t  *confidence,
                              uint16_t *oxygen,
                              uint8_t  *status)
{
  sl_status_t ret;
  bio_hub_data_t body;

  ret = bio_hub_read_bpm(&body);
  if (ret != SL_STATUS_OK) {
    *heart_rate = 0;
    *confidence = 0;
    *oxygen     = 0;
    *status     = 0;
    return ret;
  }

  *heart_rate = body.heart_rate;
  *confidence = body.confidence;
  *oxygen     = body.oxygen;
  *status     = body.status;
  return SL_STATUS_OK;
}

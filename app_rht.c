/*
 * app_rht.c
 *
 *  Created on: Aug 17, 2026
 *      Author: duong
 */

#include "app_rht.h"
#include "sl_si70xx.h"
#include "sl_i2cspm_instances.h"
#include "sl_board_control.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "em_gpio.h"

#define RHT_SENSOR_ENABLE_PORT   gpioPortC
#define RHT_SENSOR_ENABLE_PIN    9

#define RHT_SCL_PORT   gpioPortC
#define RHT_SCL_PIN    4
#define RHT_SDA_PORT   gpioPortC
#define RHT_SDA_PIN    5

static void rht_bus_recovery(void)
{
  GPIO_PinModeSet(RHT_SCL_PORT, RHT_SCL_PIN, gpioModeWiredAndPullUp, 1);
  GPIO_PinModeSet(RHT_SDA_PORT, RHT_SDA_PIN, gpioModeWiredAndPullUp, 1);

  for (int i = 0; i < 9; i++) {
    GPIO_PinOutClear(RHT_SCL_PORT, RHT_SCL_PIN);
    sl_sleeptimer_delay_millisecond(1);
    GPIO_PinOutSet(RHT_SCL_PORT, RHT_SCL_PIN);
    sl_sleeptimer_delay_millisecond(1);
  }

  // STOP condition
  GPIO_PinOutClear(RHT_SDA_PORT, RHT_SDA_PIN);
  sl_sleeptimer_delay_millisecond(1);
  GPIO_PinOutSet(RHT_SCL_PORT, RHT_SCL_PIN);
  sl_sleeptimer_delay_millisecond(1);
  GPIO_PinOutSet(RHT_SDA_PORT, RHT_SDA_PIN);
  sl_sleeptimer_delay_millisecond(10);
}

sl_status_t app_rht_init(void)
{
  // Bước 1: Set PC09 HIGH thủ công
  GPIO_PinModeSet(RHT_SENSOR_ENABLE_PORT,
                  RHT_SENSOR_ENABLE_PIN,
                  gpioModePushPull,
                  1);

  // Bước 2: Gọi board control để đồng bộ
  sl_board_enable_sensor(SL_BOARD_SENSOR_RHT);

  // Bước 3: Chờ Si7021 power-on tối thiểu 80ms
  sl_sleeptimer_delay_millisecond(80);

  // Bước 4: Kiểm tra PC09
  uint32_t pc09 = GPIO_PinInGet(RHT_SENSOR_ENABLE_PORT,
                                RHT_SENSOR_ENABLE_PIN);
  app_log("[RHT] PC09 state = %lu (phai = 1)\n", pc09);
  if (pc09 == 0) {
    app_log("[RHT] ERROR: PC09 van LOW! Si7021 khong co nguon!\n");
    return SL_STATUS_FAIL;
  }

  // Bước 5: Bus recovery
  rht_bus_recovery();
  sl_sleeptimer_delay_millisecond(20);

  // Bước 6: Kiểm tra sensor có present không
  uint8_t device_id = 0;
  bool present = sl_si70xx_present(sl_i2cspm_qwiic, SI7021_ADDR, &device_id);
  app_log("[RHT] sl_si70xx_present = %d | device_id = 0x%02X"
          " (Si7021 phai = 0x15)\n",
          (int)present, device_id);

  if (!present) {
    app_log("[RHT] ERROR: Si7021 khong phan hoi tren bus!\n");
    return SL_STATUS_FAIL;
  }

  // Bước 7: Init
  sl_status_t ret = sl_si70xx_init(sl_i2cspm_qwiic, SI7021_ADDR);
  if (ret != SL_STATUS_OK) {
    app_log("Si7021 init failed! (0x%lx)\n", (uint32_t)ret);
    return SL_STATUS_FAIL;
  }

  app_log("Si7021 init OK!\n");
  return SL_STATUS_OK;
}

sl_status_t app_rht_read(uint32_t *humidity, int32_t *temperature)
{
  if (humidity == NULL || temperature == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sl_status_t status = sl_si70xx_measure_rh_and_temp(
      sl_i2cspm_qwiic,
      SI7021_ADDR,
      humidity,
      temperature);

  // Log chi tiết error code để debug
  if (status != SL_STATUS_OK) {
    app_log("[RHT] measure_rh_and_temp failed: 0x%lx\n", (uint32_t)status);
    // 0x02 = SL_STATUS_TRANSMIT
    // 0x04 = SL_STATUS_TIMEOUT
    // 0x10 = SL_STATUS_FAIL

    // Bus recovery rồi thử lại 1 lần
    app_log("[RHT] Attempting bus recovery...\n");
    rht_bus_recovery();
    sl_sleeptimer_delay_millisecond(20);

    status = sl_si70xx_measure_rh_and_temp(
        sl_i2cspm_qwiic,
        SI7021_ADDR,
        humidity,
        temperature);

    app_log("[RHT] Retry status: 0x%lx\n", (uint32_t)status);
  }

  if (status == SL_STATUS_OK) {
    app_log("[RHT] OK | Temp: %ld.%03ld C | Hum: %lu.%03lu %%\n",
            *temperature / 1000,
            (uint32_t)(*temperature % 1000),
            *humidity / 1000,
            *humidity % 1000);
  }

  return status;
}

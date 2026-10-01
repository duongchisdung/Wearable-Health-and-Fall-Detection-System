/*
 * app_fall_detection.c
 *
 *  Created on: Aug 17, 2026
 *      Author: duong
 */


#include "app_fall_detection.h"
#include "sl_icm40627.h"
#include "app_log.h"
#include <math.h>

// ── Ngưỡng phát hiện ngã ──
#define FALL_FREE_FALL_THRESHOLD_G    0.8f
#define FALL_IMPACT_THRESHOLD_G       1.6f
#define FALL_TIMEOUT_SAMPLES          40    // 400ms @ 100Hz

// ── Ngưỡng phát hiện đứng dậy ──
#define RECOVERY_FREE_FALL_THRESHOLD_G  0.8f
#define RECOVERY_TILT_THRESHOLD_G       1.3f
#define RECOVERY_TILT_MIN_G             0.85f
#define RECOVERY_DELTA_THRESHOLD_G      0.3f

typedef enum {
  FALL_STATE_NORMAL,
  FALL_STATE_FREE_FALL,
  FALL_STATE_IMPACT
} fall_state_t;

static fall_state_t current_state         = FALL_STATE_NORMAL;
static uint32_t     state_timeout_counter = 0;
static float        prev_total_accel      = 1.0f;

// ── Init ──
sl_status_t app_fall_detection_init(void)
{
  sl_status_t status = sl_icm40627_init();
  if (status != SL_STATUS_OK) {
    app_log("ICM40627 init failed: 0x%04lx\n", status);
    return status;
  }

  sl_icm40627_enable_sensor(true, true, false);
  sl_icm40627_set_sample_rate(100.0f);
  sl_icm40627_accel_set_full_scale(sl_accelFS_4g);

  app_fall_detection_reset();
  app_log("ICM40627 Fall Detection Init OK!\n");
  return SL_STATUS_OK;
}

// ── Reset trạng thái ──
void app_fall_detection_reset(void)
{
  current_state         = FALL_STATE_NORMAL;
  state_timeout_counter = 0;
  prev_total_accel      = 1.0f;
}

// ── Phát hiện ngã ──
// Lưu ý: sl_icm40627_accel_read_data() trong driver sisdk-2025.6
// luôn trả về SL_STATUS_FAIL dù đọc data thành công (bug trong driver)
// → Bỏ qua return value, kiểm tra data hợp lệ bằng sanity check
bool app_fall_detection_check(void)
{
  float avec[3] = { 0.0f, 0.0f, 0.0f };
  bool  fall_detected = false;

  // Bỏ qua return value vì bug trong driver
  sl_icm40627_accel_read_data(avec);

  float total_accel = sqrtf(avec[0] * avec[0]
                          + avec[1] * avec[1]
                          + avec[2] * avec[2]);

  // Sanity check — data không hợp lệ thì bỏ qua
  if (total_accel < 0.01f || total_accel > 20.0f) {
    return false;
  }

  switch (current_state) {

    case FALL_STATE_NORMAL:
      if (total_accel < FALL_FREE_FALL_THRESHOLD_G) {
        current_state         = FALL_STATE_FREE_FALL;
        state_timeout_counter = 0;
        app_log("[FALL] Free-fall detected! accel=%.2f g\n",
                (double)total_accel);
      }
      break;

    case FALL_STATE_FREE_FALL:
      state_timeout_counter++;
      if (total_accel > FALL_IMPACT_THRESHOLD_G) {
        current_state = FALL_STATE_IMPACT;
        fall_detected = true;
        app_log("[FALL] !!! PHAT HIEN NGA! accel=%.2f g !!!\n",
                (double)total_accel);
      } else if (state_timeout_counter > FALL_TIMEOUT_SAMPLES) {
        app_log("[FALL] Timeout, no impact. Reset.\n");
        current_state         = FALL_STATE_NORMAL;
        state_timeout_counter = 0;
      }
      break;

    case FALL_STATE_IMPACT:
      // Reset ve NORMAL sau khi phat hien va cham
      current_state         = FALL_STATE_NORMAL;
      state_timeout_counter = 0;
      break;

    default:
      current_state         = FALL_STATE_NORMAL;
      state_timeout_counter = 0;
      break;
  }

  prev_total_accel = total_accel;
  return fall_detected;
}

// ── Phát hiện đứng dậy ──
// Điều kiện 1: Free-fall lần 2 (< 0.8g) — người dùng thả/nhấc thiết bị
// Điều kiện 2: Thay đổi góc nghiêng đột ngột (delta > 0.3g) — người dùng đứng dậy
bool app_fall_detection_check_recovery(void)
{
  float avec[3] = { 0.0f, 0.0f, 0.0f };

  // Bỏ qua return value vì bug trong driver
  sl_icm40627_accel_read_data(avec);

  float total_accel = sqrtf(avec[0] * avec[0]
                          + avec[1] * avec[1]
                          + avec[2] * avec[2]);

  // Sanity check
  if (total_accel < 0.01f || total_accel > 20.0f) {
    prev_total_accel = total_accel;
    return false;
  }

  float delta = fabsf(total_accel - prev_total_accel);

  // Điều kiện 1: Free-fall lần 2
  if (total_accel < RECOVERY_FREE_FALL_THRESHOLD_G) {
    app_log("[FALL] Recovery: Tha lan 2! accel=%.2f g\n",
            (double)total_accel);
    prev_total_accel = total_accel;
    return true;
  }

  // Điều kiện 2: Nghiêng đột ngột từ trạng thái tĩnh
  if (total_accel   > RECOVERY_TILT_THRESHOLD_G
      && prev_total_accel > RECOVERY_TILT_MIN_G
      && delta            > RECOVERY_DELTA_THRESHOLD_G) {
    app_log("[FALL] Recovery: Nghieng dot ngot! "
            "accel=%.2f g | prev=%.2f g | delta=%.2f g\n",
            (double)total_accel,
            (double)prev_total_accel,
            (double)delta);
    prev_total_accel = total_accel;
    return true;
  }

  prev_total_accel = total_accel;
  return false;
}

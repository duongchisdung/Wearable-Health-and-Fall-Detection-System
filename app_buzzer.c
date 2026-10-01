/*
 * app_buzzer.c
 *
 *  Created on: Aug 24, 2026
 *      Author: duong
 */


#include "mikroe_cmt_8540s_smt.h"
#include "sl_pwm_instances.h"
#include "sl_sleeptimer.h"
#include "app_buzzer.h"
#include "app_log.h"

#define BUZZER_FREQ_HZ          4000
#define BUZZER_DUTY_CYCLE_ON    0.01f   // Giam tu 0.05f xuong 0.01f de tieng be hon
#define BUZZER_DUTY_CYCLE_OFF   0.0f

// Muc am luong (level) cho ham play_sound — toi da 1000, giam xuong de nghe nho hon
#define BUZZER_LEVEL_TEST        100

sl_status_t buzzer_init(void)
{
  sl_status_t sc = mikroe_cmt_8540s_smt_init(&sl_pwm_mikroe);
  if (sc != SL_STATUS_OK) {
    app_log("Buzzer init error: 0x%04lx\r\n", sc);
    return SL_STATUS_FAIL;
  }

  // Khởi tạo mức ban đầu ở trạng thái tắt
  mikroe_cmt_8540s_smt_set_duty_cycle(BUZZER_DUTY_CYCLE_OFF);
  mikroe_cmt_8540s_smt_pwm_stop();

  return SL_STATUS_OK;
}

void buzzer_control(bool on)
{
  if (on) {
    // Đặt duty cycle thấp và bật xung PWM chạy liên tục — tiếng nhỏ hơn để test
    mikroe_cmt_8540s_smt_set_duty_cycle(BUZZER_DUTY_CYCLE_ON);
    mikroe_cmt_8540s_smt_pwm_start();
  } else {
    // Tắt hoàn toàn xung PWM
    mikroe_cmt_8540s_smt_set_duty_cycle(BUZZER_DUTY_CYCLE_OFF);
    mikroe_cmt_8540s_smt_pwm_stop();
  }
}

void buzzer_beep(void)
{
  // Phát âm bíp ở tần số cộng hưởng 4000Hz trong 200ms, level giam cho tieng be
  mikroe_cmt_8540s_smt_play_sound(BUZZER_FREQ_HZ, BUZZER_LEVEL_TEST, 200);
}

void buzzer_beep_health_alert(void)
{
  // Kêu 3 tiếng tít tít tít (mỗi tiếng 150ms, nghỉ 100ms), level giam
  for (int i = 0; i < 3; i++) {
    mikroe_cmt_8540s_smt_play_sound(4000, BUZZER_LEVEL_TEST, 150);
    sl_sleeptimer_delay_millisecond(100);
  }
}

void buzzer_beep_env_alert(void)
{
  // Kêu 4 tiếng tít tít tít tít (mỗi tiếng 120ms, nghỉ 80ms), level giam
  for (int i = 0; i < 4; i++) {
    mikroe_cmt_8540s_smt_play_sound(4000, BUZZER_LEVEL_TEST, 120);
    sl_sleeptimer_delay_millisecond(80);
  }
}

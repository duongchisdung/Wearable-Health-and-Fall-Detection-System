/*
 * app_fall_detection.h
 *
 *  Created on: Aug 17, 2026
 *      Author: duong
 */

#ifndef APP_FALL_DETECTION_H
#define APP_FALL_DETECTION_H

#include "sl_status.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * Khởi tạo thuật toán / cảm biến phát hiện ngã.
 ******************************************************************************/
sl_status_t app_fall_detection_init(void);

/***************************************************************************//**
 * Kiểm tra xem có sự kiện ngã xảy ra hay không.
 ******************************************************************************/
bool app_fall_detection_check(void);

/***************************************************************************//**
 * Kiểm tra xem người dùng đã tự đứng dậy / hồi phục sau khi ngã hay chưa.
 ******************************************************************************/
bool app_fall_detection_check_recovery(void);

/***************************************************************************//**
 * Reset trạng thái phát hiện ngã về ban đầu.
 ******************************************************************************/
void app_fall_detection_reset(void);

#ifdef __cplusplus
}
#endif

#endif // APP_FALL_DETECTION_H

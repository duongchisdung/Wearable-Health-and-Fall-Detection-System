/*
 * app_mlx90632.h
 *
 *  Created on: Aug 17, 2026
 *      Author: duong
 */

#ifndef APP_MLX90632_H_
#define APP_MLX90632_H_

#include "sl_status.h"

sl_status_t app_mlx90632_init(void);
sl_status_t app_mlx90632_read(double *ambient_temp, double *object_temp);

#endif // APP_MLX90632_H_

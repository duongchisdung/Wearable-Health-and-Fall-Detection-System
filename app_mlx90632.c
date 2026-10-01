/*
 * app_mlx90632.c
 *
 *  Created on: Aug 17, 2026
 *      Author: duong
 */


#include "app_mlx90632.h"
#include "mikroe_mlx90632.h"
#include "sl_i2cspm_instances.h"
#include "app_log.h"

sl_status_t app_mlx90632_init(void)
{
  sl_status_t status;



  status = mikroe_mlx90632_init(sl_i2cspm_sensor);
  if (status != SL_STATUS_OK) {
    app_log("MLX90632 init failed!\n");
    return status;
  }

  mikroe_mlx90632_default_config();
  app_log("MLX90632 init OK!\n");

  return SL_STATUS_OK;
}

sl_status_t app_mlx90632_read(double *ambient_temp, double *object_temp)
{
  if (mikroe_mlx90632_present() != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }

  *ambient_temp = (double)mikroe_mlx90632_get_ambient_temperature();
  *object_temp = (double)mikroe_mlx90632_get_object_temperature();

  return SL_STATUS_OK;
}

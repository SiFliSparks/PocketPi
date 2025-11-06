#ifndef INPUT_H
#define INPUT_H

#include "rtthread.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "stdio.h"
#include "string.h"
#include "drivers/i2c.h"

#define INPUT_I2C_BUS_NAME       "i2c1"
#define INPUT_AW9523_DEV_ADDR    0x5B

rt_err_t input_init();
rt_err_t input_deinit();

uint16_t input_get_state();

#endif
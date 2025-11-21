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

#define INPUT_KEY_A        0
#define INPUT_KEY_X        1
#define INPUT_KEY_B        2
#define INPUT_KEY_Y        3
#define INPUT_KEY_START    4
#define INPUT_KEY_SELECT   5
#define INPUT_KEY_MENU     6
#define INPUT_KEY_DOWN     7
#define INPUT_KEY_R        11
#define INPUT_KEY_LEFT     12
#define INPUT_KEY_UP       13
#define INPUT_KEY_RIGHT    14
#define INPUT_KEY_L        15

rt_err_t input_init();
rt_err_t input_deinit();

uint16_t input_get_state();
int input_get_key_state(int key_index);

#endif
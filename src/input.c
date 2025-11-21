#include "input.h"

static struct rt_i2c_bus_device *i2c_bus = NULL;
static uint16_t state_cache = 0xFFFF;
static uint32_t last_updete_time = 0;

rt_err_t input_init()
{
    HAL_PIN_Set(PAD_PA11, I2C1_SCL, PIN_PULLUP, 1); // i2c io select
    HAL_PIN_Set(PAD_PA10, I2C1_SDA, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA09, GPIO_A0 + 9, PIN_PULLUP, 1);
    i2c_bus = rt_i2c_bus_device_find(INPUT_I2C_BUS_NAME);
    if(i2c_bus == RT_NULL)
    {
        rt_kprintf("input: i2c bus find failed!\n");
        return -RT_ERROR;
    }
    rt_kprintf("input: i2c bus found.\n");
    rt_device_open((rt_device_t)i2c_bus, RT_DEVICE_FLAG_RDWR);
    struct rt_i2c_configuration configuration =
    {
        .mode = 0,
        .addr = 0,
        .timeout = 500, //Waiting for timeout period (ms)
        .max_hz = 400000, //I2C rate (hz)
    };
    // config I2C parameter
    rt_i2c_configure(i2c_bus, &configuration);
    if(i2c_bus != RT_NULL)
    {
        rt_uint16_t dev_addr = 0x5B; /* TODO: 根据硬件调整地址 */
        uint8_t buf[2];

        /* 配置 P0 IO 模式寄存器 0x04 = 0x3F */
        buf[0] = 0x04; buf[1] = 0xFF;
        rt_i2c_master_send(i2c_bus, dev_addr, RT_I2C_WR , buf, sizeof(buf));
        // rt_i2c_mem_write(i2c_bus, dev_addr, 0x04, 8, &buf[1], 1);

        /* 配置 P1 IO 模式寄存器 0x05 = 0xFF */
        buf[0] = 0x05; buf[1] = 0xFF;
        rt_i2c_master_send(i2c_bus, dev_addr, RT_I2C_WR , buf, sizeof(buf));

        /* 配置全局寄存器 0x11 = 0x03 */
        buf[0] = 0x11; buf[1] = 0x03;
        rt_i2c_master_send(i2c_bus, dev_addr, RT_I2C_WR , buf, sizeof(buf));

        /* 配置 LED 模式寄存器 0x13 = 0x1F */
        buf[0] = 0x13; buf[1] = 0xF8;
        rt_i2c_master_send(i2c_bus, dev_addr, RT_I2C_WR , buf, sizeof(buf));
        
        buf[0] = 0x20; buf[1] = 0xFF;
        rt_i2c_master_send(i2c_bus, dev_addr, RT_I2C_WR , buf, sizeof(buf));
        buf[0] = 0x21; buf[1] = 0xFF;
        rt_i2c_master_send(i2c_bus, dev_addr, RT_I2C_WR , buf, sizeof(buf));
        buf[0] = 0x22; buf[1] = 0xFF;
        rt_i2c_master_send(i2c_bus, dev_addr, RT_I2C_WR , buf, sizeof(buf));
    }

    return RT_EOK;
}

rt_err_t input_deinit()
{
    if (i2c_bus != RT_NULL)
    {
        rt_device_close((rt_device_t)i2c_bus);
        i2c_bus = RT_NULL;
    }
    return RT_EOK;
}

uint16_t input_get_state()
{
    if (i2c_bus == RT_NULL)
    {
        rt_kprintf("input: i2c bus not initialized!\n");
        return 0;
    }
    if(rt_tick_get() - last_updete_time < 10)
    {
        return state_cache;
    }

    uint16_t port_data = 0;
    uint8_t buf[2];
    buf[0] = 0x00; buf[1] = 0;
    rt_i2c_master_send(i2c_bus, INPUT_AW9523_DEV_ADDR, RT_I2C_WR , buf, 1);
    rt_i2c_master_recv(i2c_bus, INPUT_AW9523_DEV_ADDR, RT_I2C_RD , buf + 1, 1);
    port_data |= buf[1];
    buf[0] = 0x01; buf[1] = 0;
    rt_i2c_master_send(i2c_bus, INPUT_AW9523_DEV_ADDR, RT_I2C_WR , buf, 1);
    rt_i2c_master_recv(i2c_bus, INPUT_AW9523_DEV_ADDR, RT_I2C_RD , buf + 1, 1);
    port_data |= (buf[1] << 8);
    state_cache = port_data;
    last_updete_time = rt_tick_get();
    return port_data;
}

int input_get_key_state(int key_index)
{
    uint16_t state = input_get_state();
    if(state & (1 << key_index))
    {
        return 0; // not pressed
    }
    else
    {
        return 1; // pressed
    }
}
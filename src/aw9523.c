/*
 * aw9523.c
 *
 * AW9523 I/O 扩展芯片驱动框架实现（存根）。
 * - 提供基本的 create/destroy 实现。
 * - 其他 API 仅返回 AW9523_NOT_IMPLEMENTED，后续填充具体硬件访问逻辑。
 */

#include "aw9523.h"
#include <stdlib.h>
#include <string.h>

/* 创建实例：分配内存并复制配置 */
aw9523_t *aw9523_create(const aw9523_config_t *cfg)
{
    if (!cfg) return NULL;
    aw9523_t *dev = (aw9523_t *)malloc(sizeof(aw9523_t));
    if (!dev) return NULL;
    memset(dev, 0, sizeof(*dev));
    dev->cfg = *cfg;
    dev->priv = NULL; /* 实现可在 init 中分配 */
    return dev;
}

/* 初始化：存根，未实现具体 I2C/硬件逻辑 */
aw9523_result_t aw9523_init(aw9523_t *dev)
{
    if (!dev) return AW9523_ERR;
    /* TODO: 初始化 I2C、校验设备、配置默认寄存器 */
    return AW9523_NOT_IMPLEMENTED;
}

/* 解除初始化 */
aw9523_result_t aw9523_deinit(aw9523_t *dev)
{
    if (!dev) return AW9523_ERR;
    /* TODO: 释放 priv、注销中断等 */
    return AW9523_NOT_IMPLEMENTED;
}

/* 读寄存器（存根） */
aw9523_result_t aw9523_read_reg(aw9523_t *dev, uint8_t reg, uint8_t *out)
{
    (void)dev; (void)reg; (void)out;
    return AW9523_NOT_IMPLEMENTED;
}

/* 写寄存器（存根） */
aw9523_result_t aw9523_write_reg(aw9523_t *dev, uint8_t reg, uint8_t val)
{
    (void)dev; (void)reg; (void)val;
    return AW9523_NOT_IMPLEMENTED;
}

/* 设置 GPIO（存根） */
aw9523_result_t aw9523_set_gpio(aw9523_t *dev, uint8_t pin, uint8_t value)
{
    (void)dev; (void)pin; (void)value;
    return AW9523_NOT_IMPLEMENTED;
}

/* 获取 GPIO（存根） */
aw9523_result_t aw9523_get_gpio(aw9523_t *dev, uint8_t pin, uint8_t *out_value)
{
    (void)dev; (void)pin; (void)out_value;
    return AW9523_NOT_IMPLEMENTED;
}

/* 销毁实例 */
void aw9523_destroy(aw9523_t *dev)
{
    if (!dev) return;
    /* 如果 priv 在 init 中被分配，应在 deinit 中释放 */
    free(dev);
}

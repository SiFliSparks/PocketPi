/*
 * aw9523.h
 *
 * 简要：AW9523 I/O 扩展芯片驱动框架（头文件）
 * 本文件只包含类型定义和函数原型，具体实现留空/为存根。
 */
#ifndef AW9523_H
#define AW9523_H

#include <stdint.h>
#include <stddef.h>

#include "aw9523_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 返回码：简单约定 */
typedef enum {
    AW9523_OK = 0,
    AW9523_ERR = -1,
    AW9523_NOT_IMPLEMENTED = -2,
} aw9523_result_t;

/** 配置结构（可按需扩展） */
typedef struct {
    uint8_t i2c_addr;   /* I2C 地址 */
    uint32_t i2c_bus;   /* 总线编号或总线句柄编号（项目内约定）*/
    uint8_t irq_pin;    /* 可选：中断引脚编号 */
} aw9523_config_t;

/** 设备句柄 */
typedef struct aw9523_device {
    aw9523_config_t cfg;
    void *priv; /* 私有实现指针（驱动实现可使用） */
} aw9523_t;

/*
 * Contract / 约定：
 * - 输入/输出与错误模式：函数返回 aw9523_result_t，0 表示成功，负值表示错误或未实现。
 * - 设备句柄通过 aw9523_create / aw9523_destroy 管理。
 */

/** 创建驱动实例（仅分配并拷贝配置） */
aw9523_t *aw9523_create(const aw9523_config_t *cfg);

/** 初始化设备（配置 I2C、复位、默认寄存器等） */
aw9523_result_t aw9523_init(aw9523_t *dev);

/** 解除初始化 / 释放硬件资源 */
aw9523_result_t aw9523_deinit(aw9523_t *dev);

/** 读取寄存器（单字节） */
aw9523_result_t aw9523_read_reg(aw9523_t *dev, uint8_t reg, uint8_t *out);

/** 写寄存器（单字节） */
aw9523_result_t aw9523_write_reg(aw9523_t *dev, uint8_t reg, uint8_t val);

/** 设置 GPIO 引脚电平（0/1） */
aw9523_result_t aw9523_set_gpio(aw9523_t *dev, uint8_t pin, uint8_t value);

/** 获取 GPIO 引脚电平（0/1） */
aw9523_result_t aw9523_get_gpio(aw9523_t *dev, uint8_t pin, uint8_t *out_value);

/** 销毁实例并释放内存 */
void aw9523_destroy(aw9523_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* AW9523_H */

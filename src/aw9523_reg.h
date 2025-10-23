/*
 * aw9523_reg.h
 *
 * AW9523 寄存器定义 (寄存器地址与说明)
 *
 * 说明：本文件包含 AW9523 常用寄存器地址宏及简短注释，供驱动实现使用。
 */

#ifndef AW9523_REG_H
#define AW9523_REG_H

/* Device-level notes:
 * - AW9523 是一款 I2C 可编程 I/O/LED 驱动芯片，常用于 GPIO 扩展与 LED 电流/亮度控制。
 * - 寄存器包括端口输入/输出、方向、中断、上拉/下拉、LED 模式与 DIM（亮度）寄存器。
 */

/* --- 基本端口寄存器 --- */
#define AW9523_REG_INPUT_PORT0        0x00  /* 输入端口组0（低字节） */
#define AW9523_REG_INPUT_PORT1        0x01  /* 输入端口组1（高字节） */

#define AW9523_REG_OUTPUT_PORT0       0x02  /* 输出端口组0 */
#define AW9523_REG_OUTPUT_PORT1       0x03  /* 输出端口组1 */

#define AW9523_REG_CONFIG_PORT0       0x04  /* 端口0 输入/输出 配置 */
#define AW9523_REG_CONFIG_PORT1       0x05  /* 端口1 输入/输出 配置 */

/* 中断使能寄存器 */
#define AW9523_REG_INT_EN_PORT0       0x06
#define AW9523_REG_INT_EN_PORT1       0x07

/* 设备 ID / 全局控制 */
#define AW9523_REG_ID                 0x10  /* 设备 ID（只读） */
#define AW9523_REG_GLOBAL_CTRL        0x11  /* 全局控制寄存器 */

/* LED 模式寄存器 */
#define AW9523_REG_LED_MODE_P0        0x12  /* 控制 P0_7..P0_0 的工作模式 */
#define AW9523_REG_LED_MODE_P1        0x13  /* 控制 P1_7..P1_0 的工作模式 */

/* 0x20-0x2F: DIM（电流/亮度）寄存器区间，用于 P1_0..P0_7 的电流控制 !这里写的有问题 */
#define AW9523_REG_DIM_P1_0           0x20
#define AW9523_REG_DIM_P1_1           0x21
#define AW9523_REG_DIM_P1_2           0x22
#define AW9523_REG_DIM_P1_3           0x23
#define AW9523_REG_DIM_P1_4           0x24
#define AW9523_REG_DIM_P1_5           0x25
#define AW9523_REG_DIM_P1_6           0x26
#define AW9523_REG_DIM_P1_7           0x27
#define AW9523_REG_DIM_P0_0           0x28
#define AW9523_REG_DIM_P0_1           0x29
#define AW9523_REG_DIM_P0_2           0x2A
#define AW9523_REG_DIM_P0_3           0x2B
#define AW9523_REG_DIM_P0_4           0x2C
#define AW9523_REG_DIM_P0_5           0x2D
#define AW9523_REG_DIM_P0_6           0x2E
#define AW9523_REG_DIM_P0_7           0x2F

/* 0x7F: 软件复位控制*/
#define AW9523_REG_SW_RSTN            0x7F

/* End of file */

#endif /* AW9523_REG_H */

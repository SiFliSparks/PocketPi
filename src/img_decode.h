#ifndef IMG_DECODE_H
#define IMG_DECODE_H

#include <stdint.h>
#include "tjpgd.h"

typedef struct 
{
    uint32_t width;
    uint32_t height;
    uint8_t *data;
} img_decode_result_t;

// RGB888 格式解码
int img_decode_from_file(const char* file_path, img_decode_result_t* result);
// RGB565 格式解码
int img_decode_from_file_rgb565(const char* file_path, img_decode_result_t* result);
// 获取 JPEG 图像尺寸
int img_decode_get_jpeg_dimensions(const char* file_path, uint32_t* width, uint32_t* height);
// 调整图像大小
int img_decode_resize_image(img_decode_result_t* result, uint32_t new_width, uint32_t new_height);
// 调整图像大小（RGB565 格式） 
int img_decode_resize_image_rgb565(img_decode_result_t* result, uint32_t new_width, uint32_t new_height);
// 释放解码结果
void img_free_decode_result(img_decode_result_t* result);

#endif // IMG_DECODE_H
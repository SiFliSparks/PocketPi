#include "img_decode.h"
#include "tjpgd.h"
#include <stdio.h>
#include <string.h>
#include "rtthread.h"

#if RT_USING_DFS
    #include "dfs_file.h"
    #include "dfs_posix.h"
#endif

// 外部 PSRAM 内存分配函数
extern void *opus_heap_malloc(uint32_t size);
extern void opus_heap_free(void *p);

// 解码器上下文
typedef struct {
    FILE* fp;
    uint8_t *fbuf;
    uint32_t wfbuf;
    uint32_t format; // 0: RGB888, 1: RGB565
} img_decode_context_t;

/**
 * @brief JPEG 输入函数（从文件读取）
 */
static size_t img_decode_in_func(JDEC* jd, unsigned char* buff, size_t nbyte)
{
    img_decode_context_t *ctx = (img_decode_context_t*)jd->device;
    
    if (buff) {
        // 从输入流读取数据
        return (uint32_t)fread(buff, 1, nbyte, ctx->fp);
    } else {
        // 跳过 nbyte 字节
        return fseek(ctx->fp, nbyte, SEEK_CUR) ? 0 : nbyte;
    }
}

/**
 * @brief JPEG 输出函数（RGB888 格式）
 */
static int img_decode_out_func_rgb888(JDEC* jd, void* bitmap, JRECT* rect)
{
    img_decode_context_t *ctx = (img_decode_context_t*)jd->device;
    uint8_t *src, *dst;
    uint32_t y, bws, bwd;
    
    // 输出进度（可选）
    // if (rect->left == 0) {
    //     rt_kprintf("\rDecoding: %lu%%\n", (rect->top << jd->scale) * 100UL / jd->height);
    // }
    
    // 拷贝解码的 RGB888 矩形到帧缓冲区
    src = (uint8_t*)bitmap;
    dst = ctx->fbuf + 3 * (rect->top * ctx->wfbuf + rect->left);
    bws = 3 * (rect->right - rect->left + 1);  // 源矩形宽度[字节]
    bwd = 3 * ctx->wfbuf;                       // 帧缓冲区宽度[字节]
    
    for (y = rect->top; y <= rect->bottom; y++) {
        memcpy(dst, src, bws);
        src += bws;
        dst += bwd;
    }
    
    return 1; // 继续解码
}

/**
 * @brief JPEG 输出函数（RGB565 格式）
 */
static int img_decode_out_func_rgb565(JDEC* jd, void* bitmap, JRECT* rect)
{
    img_decode_context_t *ctx = (img_decode_context_t*)jd->device;
    uint8_t *src;
    uint16_t *dst;
    uint32_t x, y, width;
    
    // 输出进度（可选）
    if (rect->left == 0) {
        rt_kprintf("\rDecoding: %lu%%", (rect->top << jd->scale) * 100UL / jd->height);
    }
    
    // 将 RGB888 转换为 RGB565
    src = (uint8_t*)bitmap;
    width = rect->right - rect->left + 1;
    
    for (y = rect->top; y <= rect->bottom; y++) {
        dst = (uint16_t*)(ctx->fbuf) + y * ctx->wfbuf + rect->left;
        for (x = 0; x < width; x++) {
            uint8_t r = src[x * 3 + 0];
            uint8_t g = src[x * 3 + 1];
            uint8_t b = src[x * 3 + 2];
            // RGB888 -> RGB565: RRRRRGGGGGGBBBBB
            dst[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
        src += width * 3;
    }
    
    return 1; // 继续解码
}

/**
 * @brief 从文件解码 JPEG 图像到 PSRAM (RGB888 格式)
 * @param file_path 图像文件路径
 * @param result 解码结果输出
 * @return 0 成功，-1 失败
 */
int img_decode_from_file(const char* file_path, img_decode_result_t* result)
{
    if (!file_path || !result) {
        rt_kprintf("Invalid parameters for img_decode_from_file\n");
        return -1;
    }
    
    // 打开文件
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        rt_kprintf("Failed to open image file: %s\n", file_path);
        return -1;
    }
    
    // 分配 JPEG 解码工作区（从普通堆分配，临时使用）
    void *work = malloc(3100 * 2);
    if (!work) {
        rt_kprintf("Failed to allocate JPEG work buffer\n");
        fclose(fp);
        return -1;
    }
    
    // 准备解码上下文
    img_decode_context_t ctx;
    ctx.fp = fp;
    ctx.fbuf = NULL;
    ctx.format = 0; // RGB888
    
    // 准备 JPEG 解码器
    JDEC jdec;
    JRESULT jres = jd_prepare(&jdec, img_decode_in_func, work, 3100 * 2, &ctx);
    
    if (jres != JDR_OK) {
        rt_kprintf("JPEG prepare failed: %d\n", jres);
        free(work);
        fclose(fp);
        return -1;
    }
    
    rt_kprintf("Image size: %d x %d\n", jdec.width, jdec.height);
    
    // 从 PSRAM 分配图像缓冲区 (RGB888: 3 bytes per pixel)
    ctx.fbuf = (uint8_t*)opus_heap_malloc(jdec.width * jdec.height * 3);
    if (!ctx.fbuf) {
        rt_kprintf("Failed to allocate image buffer in PSRAM\n");
        free(work);
        fclose(fp);
        return -1;
    }
    
    ctx.wfbuf = jdec.width;
    
    // 开始解码
    jres = jd_decomp(&jdec, img_decode_out_func_rgb888, 0);
    
    if (jres == JDR_OK) {
        rt_kprintf("\nImage decoded successfully: %s\n", file_path);
        result->width = jdec.width;
        result->height = jdec.height;
        result->data = ctx.fbuf;
    } else {
        rt_kprintf("\nImage decode failed: %d\n", jres);
        opus_heap_free(ctx.fbuf);
        free(work);
        fclose(fp);
        return -1;
    }
    
    // 清理临时资源
    free(work);
    fclose(fp);
    
    return 0;
}

/**
 * @brief 从文件解码 JPEG 图像到 PSRAM (RGB565 格式)
 * @param file_path 图像文件路径
 * @param result 解码结果输出
 * @return 0 成功，-1 失败
 */
int img_decode_from_file_rgb565(const char* file_path, img_decode_result_t* result)
{
    if (!file_path || !result) {
        rt_kprintf("Invalid parameters for img_decode_from_file_rgb565\n");
        return -1;
    }
    
    // 打开文件
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        rt_kprintf("Failed to open image file: %s\n", file_path);
        return -1;
    }
    
    // 分配 JPEG 解码工作区
    void *work = malloc(3100 * 2);
    if (!work) {
        rt_kprintf("Failed to allocate JPEG work buffer\n");
        fclose(fp);
        return -1;
    }
    
    // 准备解码上下文
    img_decode_context_t ctx;
    ctx.fp = fp;
    ctx.fbuf = NULL;
    ctx.format = 1; // RGB565
    
    // 准备 JPEG 解码器
    JDEC jdec;
    JRESULT jres = jd_prepare(&jdec, img_decode_in_func, work, 3100 * 2, &ctx);
    
    if (jres != JDR_OK) {
        rt_kprintf("JPEG prepare failed: %d\n", jres);
        free(work);
        fclose(fp);
        return -1;
    }
    
    rt_kprintf("Image size: %d x %d\n", jdec.width, jdec.height);
    
    // 从 PSRAM 分配图像缓冲区 (RGB565: 2 bytes per pixel)
    ctx.fbuf = (uint8_t*)opus_heap_malloc(jdec.width * jdec.height * 2);
    if (!ctx.fbuf) {
        rt_kprintf("Failed to allocate image buffer in PSRAM\n");
        free(work);
        fclose(fp);
        return -1;
    }
    
    ctx.wfbuf = jdec.width;
    
    // 开始解码
    jres = jd_decomp(&jdec, img_decode_out_func_rgb565, 0);
    
    if (jres == JDR_OK) {
        rt_kprintf("\nImage decoded successfully: %s\n", file_path);
        result->width = jdec.width;
        result->height = jdec.height;
        result->data = ctx.fbuf;
    } else {
        rt_kprintf("\nImage decode failed: %d\n", jres);
        opus_heap_free(ctx.fbuf);
        free(work);
        fclose(fp);
        return -1;
    }
    
    // 清理临时资源
    free(work);
    fclose(fp);
    
    return 0;
}

/**
 * @brief 获取 JPEG 图像尺寸（不解码图像数据）
 * @param file_path 图像文件路径
 * @param width 输出图像宽度
 * @param height 输出图像高度
 * @return 0 成功，-1 失败
 */
int img_decode_get_jpeg_dimensions(const char* file_path, uint32_t* width, uint32_t* height)
{
    if (!file_path || !width || !height) {
        rt_kprintf("Invalid parameters for img_decode_get_jpeg_dimensions\n");
        return -1;
    }
    
    // 打开文件
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        rt_kprintf("Failed to open image file: %s\n", file_path);
        return -1;
    }
    
    // 分配 JPEG 解码工作区（临时使用）
    void *work = malloc(3100 * 2);
    if (!work) {
        rt_kprintf("Failed to allocate JPEG work buffer\n");
        fclose(fp);
        return -1;
    }
    
    // 准备解码上下文（只用于读取文件）
    img_decode_context_t ctx;
    ctx.fp = fp;
    ctx.fbuf = NULL;
    ctx.format = 0;
    
    // 准备 JPEG 解码器（只解析头部信息）
    JDEC jdec;
    JRESULT jres = jd_prepare(&jdec, img_decode_in_func, work, 3100 * 2, &ctx);
    
    if (jres == JDR_OK) {
        // 成功获取尺寸
        *width = jdec.width;
        *height = jdec.height;
        rt_kprintf("JPEG dimensions: %u x %u\n", *width, *height);
    } else {
        rt_kprintf("Failed to get JPEG dimensions: %d\n", jres);
        free(work);
        fclose(fp);
        return -1;
    }
    
    // 清理临时资源
    free(work);
    fclose(fp);
    
    return 0;
}

/**
 * @brief 调整图像大小（RGB888 格式，使用双线性插值 + 中心裁剪）
 * @param result 原始图像数据（会被替换为新图像）
 * @param new_width 新宽度
 * @param new_height 新高度
 * @return 0 成功，-1 失败
 */
int img_decode_resize_image(img_decode_result_t* result, uint32_t new_width, uint32_t new_height)
{
    if (!result || !result->data || result->width == 0 || result->height == 0) {
        rt_kprintf("Invalid image result for resize\n");
        return -1;
    }

    if (new_width == 0 || new_height == 0) {
        rt_kprintf("Invalid target dimensions\n");
        return -1;
    }

    // 如果尺寸相同，无需处理
    if (result->width == new_width && result->height == new_height) {
        return 0;
    }

    uint32_t src_width = result->width;
    uint32_t src_height = result->height;
    uint8_t *src_data = result->data;

    // 计算缩放比例（保持宽高比，选择较大的比例以填充目标区域）
    float scale_x = (float)new_width / (float)src_width;
    float scale_y = (float)new_height / (float)src_height;
    float scale = (scale_x > scale_y) ? scale_x : scale_y;

    // 计算缩放后的尺寸
    uint32_t scaled_width = (uint32_t)(src_width * scale);
    uint32_t scaled_height = (uint32_t)(src_height * scale);

    // 计算裁剪偏移（中心裁剪）
    int32_t offset_x = (scaled_width - new_width) / 2;
    int32_t offset_y = (scaled_height - new_height) / 2;

    rt_kprintf("Resize: %ux%u -> %ux%u (scale=%.2f, scaled=%ux%u, offset=%d,%d)\n",
               src_width, src_height, new_width, new_height, scale, 
               scaled_width, scaled_height, offset_x, offset_y);

    // 分配新图像缓冲区
    uint8_t *new_data = (uint8_t*)opus_heap_malloc(new_width * new_height * 3);
    if (!new_data) {
        rt_kprintf("Failed to allocate memory for resized image\n");
        return -1;
    }

    // 执行缩放和裁剪（双线性插值）
    for (uint32_t y = 0; y < new_height; y++) {
        for (uint32_t x = 0; x < new_width; x++) {
            // 计算在缩放图像中的位置
            float src_x = ((float)(x + offset_x) / scale);
            float src_y = ((float)(y + offset_y) / scale);

            // 边界检查
            if (src_x < 0) src_x = 0;
            if (src_y < 0) src_y = 0;
            if (src_x >= src_width - 1) src_x = src_width - 1.001f;
            if (src_y >= src_height - 1) src_y = src_height - 1.001f;

            // 双线性插值
            uint32_t x0 = (uint32_t)src_x;
            uint32_t y0 = (uint32_t)src_y;
            uint32_t x1 = x0 + 1;
            uint32_t y1 = y0 + 1;

            float fx = src_x - x0;
            float fy = src_y - y0;

            // 获取四个相邻像素
            uint8_t *p00 = src_data + (y0 * src_width + x0) * 3;
            uint8_t *p10 = src_data + (y0 * src_width + x1) * 3;
            uint8_t *p01 = src_data + (y1 * src_width + x0) * 3;
            uint8_t *p11 = src_data + (y1 * src_width + x1) * 3;

            // 对 RGB 三个通道分别插值
            uint8_t *dst = new_data + (y * new_width + x) * 3;
            for (int c = 0; c < 3; c++) {
                float val = (1 - fx) * (1 - fy) * p00[c] +
                           fx * (1 - fy) * p10[c] +
                           (1 - fx) * fy * p01[c] +
                           fx * fy * p11[c];
                dst[c] = (uint8_t)(val + 0.5f);
            }
        }
    }

    // 释放旧数据并更新结构体
    opus_heap_free(src_data);
    result->data = new_data;
    result->width = new_width;
    result->height = new_height;

    rt_kprintf("Image resized successfully\n");
    return 0;
}

/**
 * @brief 调整图像大小（RGB565 格式，使用双线性插值 + 中心裁剪）
 * @param result 原始图像数据（会被替换为新图像）
 * @param new_width 新宽度
 * @param new_height 新高度
 * @return 0 成功，-1 失败
 */
int img_decode_resize_image_rgb565(img_decode_result_t* result, uint32_t new_width, uint32_t new_height)
{
    if (!result || !result->data || result->width == 0 || result->height == 0) {
        rt_kprintf("Invalid image result for resize\n");
        return -1;
    }

    if (new_width == 0 || new_height == 0) {
        rt_kprintf("Invalid target dimensions\n");
        return -1;
    }

    // 如果尺寸相同，无需处理
    if (result->width == new_width && result->height == new_height) {
        return 0;
    }

    uint32_t src_width = result->width;
    uint32_t src_height = result->height;
    uint16_t *src_data = (uint16_t*)result->data;

    // 计算缩放比例（保持宽高比，选择较大的比例以填充目标区域）
    float scale_x = (float)new_width / (float)src_width;
    float scale_y = (float)new_height / (float)src_height;
    float scale = (scale_x > scale_y) ? scale_x : scale_y;

    // 计算缩放后的尺寸
    uint32_t scaled_width = (uint32_t)(src_width * scale);
    uint32_t scaled_height = (uint32_t)(src_height * scale);

    // 计算裁剪偏移（中心裁剪）
    int32_t offset_x = (scaled_width - new_width) / 2;
    int32_t offset_y = (scaled_height - new_height) / 2;

    rt_kprintf("Resize RGB565: %ux%u -> %ux%u (scale=%.2f, scaled=%ux%u, offset=%d,%d)\n",
               src_width, src_height, new_width, new_height, scale, 
               scaled_width, scaled_height, offset_x, offset_y);

    // 分配新图像缓冲区
    uint16_t *new_data = (uint16_t*)opus_heap_malloc(new_width * new_height * 2);
    if (!new_data) {
        rt_kprintf("Failed to allocate memory for resized image\n");
        return -1;
    }

    // 执行缩放和裁剪（双线性插值）
    for (uint32_t y = 0; y < new_height; y++) {
        for (uint32_t x = 0; x < new_width; x++) {
            // 计算在缩放图像中的位置
            float src_x = ((float)(x + offset_x) / scale);
            float src_y = ((float)(y + offset_y) / scale);

            // 边界检查
            if (src_x < 0) src_x = 0;
            if (src_y < 0) src_y = 0;
            if (src_x >= src_width - 1) src_x = src_width - 1.001f;
            if (src_y >= src_height - 1) src_y = src_height - 1.001f;

            // 双线性插值
            uint32_t x0 = (uint32_t)src_x;
            uint32_t y0 = (uint32_t)src_y;
            uint32_t x1 = x0 + 1;
            uint32_t y1 = y0 + 1;

            float fx = src_x - x0;
            float fy = src_y - y0;

            // 获取四个相邻像素（RGB565）
            uint16_t p00 = src_data[y0 * src_width + x0];
            uint16_t p10 = src_data[y0 * src_width + x1];
            uint16_t p01 = src_data[y1 * src_width + x0];
            uint16_t p11 = src_data[y1 * src_width + x1];

            // 分离 RGB 分量并插值
            // RGB565: RRRRRGGGGGGBBBBB
            uint8_t r00 = (p00 >> 11) & 0x1F;
            uint8_t g00 = (p00 >> 5) & 0x3F;
            uint8_t b00 = p00 & 0x1F;

            uint8_t r10 = (p10 >> 11) & 0x1F;
            uint8_t g10 = (p10 >> 5) & 0x3F;
            uint8_t b10 = p10 & 0x1F;

            uint8_t r01 = (p01 >> 11) & 0x1F;
            uint8_t g01 = (p01 >> 5) & 0x3F;
            uint8_t b01 = p01 & 0x1F;

            uint8_t r11 = (p11 >> 11) & 0x1F;
            uint8_t g11 = (p11 >> 5) & 0x3F;
            uint8_t b11 = p11 & 0x1F;

            // 插值计算
            float r = (1 - fx) * (1 - fy) * r00 + fx * (1 - fy) * r10 +
                     (1 - fx) * fy * r01 + fx * fy * r11;
            float g = (1 - fx) * (1 - fy) * g00 + fx * (1 - fy) * g10 +
                     (1 - fx) * fy * g01 + fx * fy * g11;
            float b = (1 - fx) * (1 - fy) * b00 + fx * (1 - fy) * b10 +
                     (1 - fx) * fy * b01 + fx * fy * b11;

            // 合成 RGB565
            uint8_t r_val = (uint8_t)(r + 0.5f) & 0x1F;
            uint8_t g_val = (uint8_t)(g + 0.5f) & 0x3F;
            uint8_t b_val = (uint8_t)(b + 0.5f) & 0x1F;

            new_data[y * new_width + x] = (r_val << 11) | (g_val << 5) | b_val;
        }
    }

    // 释放旧数据并更新结构体
    opus_heap_free(src_data);
    result->data = (uint8_t*)new_data;
    result->width = new_width;
    result->height = new_height;

    rt_kprintf("Image resized successfully (RGB565)\n");
    return 0;
}

/**
 * @brief 释放解码结果（从 PSRAM 释放）
 * @param result 解码结果
 */
void img_free_decode_result(img_decode_result_t* result)
{
    if (result && result->data) {
        opus_heap_free(result->data);
        result->data = NULL;
        result->width = 0;
        result->height = 0;
    }
}

#include "rtthread.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "stdio.h"
#include "string.h"
#include "mem_section.h"
#include "littlevgl2rtt.h"

#if RT_USING_DFS
    #include "dfs_file.h"
    #include "dfs_posix.h"
#endif
#include "drv_flash.h"
#include <drivers/audio.h>
#include "input.h"
#include "audio.h"
#include "tjpgd.h"

typedef struct {
    FILE* fp;
    uint8_t *fbuf;
    uint32_t wfbuf;
} IODEV;
uint8_t *logo_data =NULL;
lv_img_dsc_t logo_dsc;
lv_img_dsc_t bg_dsc;
lv_obj_t * bg_img;

size_t in_func (JDEC* jd, unsigned char* buff, size_t nbyte)
{
    IODEV *dev = (IODEV*)jd->device;   /* Device identifier for the session (5th argument of jd_prepare function) */
 
 
    if (buff) {
        /* 从输入流读取一字节 */
        return (uint32_t)fread(buff, 1, nbyte, dev->fp);
    } else {
        /* 从输入流移除一字节 */
        return fseek(dev->fp, nbyte, SEEK_CUR) ? 0 : nbyte;
    }
}

int out_func (JDEC* jd, void* bitmap, JRECT* rect)
{
    IODEV *dev = (IODEV*)jd->device;
    uint8_t *src, *dst;
    uint32_t y, bws, bwd;
 
 
    /* 输出进度 */
    if (rect->left == 0) {
        printf("\r%lu%%", (rect->top << jd->scale) * 100UL / jd->height);
    }
 
    /* 拷贝解码的RGB矩形范围到帧缓冲区(假设RGB888配置) */
    src = (uint8_t*)bitmap;
    dst = dev->fbuf + 3 * (rect->top * dev->wfbuf + rect->left);  /* 目标矩形的左上 */
    bws = 3 * (rect->right - rect->left + 1);     /* 源矩形的宽度[字节] */
    bwd = 3 * dev->wfbuf;                         /* 帧缓冲区宽度[字节] */
    for (y = rect->top; y <= rect->bottom; y++) {
        memcpy(dst, src, bws);   /* 拷贝一行 */
        src += bws; dst += bwd;  /* 定位下一行 */
    }
 
    return 1;    /* 继续解码 */
}

LV_FONT_DECLARE(nmdws_16);

const char *framebuffer = (const char *)PSRAM_BASE+0x400000;
rt_device_t lcd_device;
const char supported_extensions[][4] = {
    "nes",
    "NES"
};

#ifndef FS_REGION_START_ADDR
    #error "Need to define file system start address!"
#endif

#define FS_ROOT "root"

/**
 * @brief Mount fs.
 */
int mnt_init(void)
{
    register_mtd_device(FS_REGION_START_ADDR, FS_REGION_SIZE, FS_ROOT);
    if (dfs_mount(FS_ROOT, "/", "elm", 0, 0) == 0) // fs exist
    {
        rt_kprintf("mount fs on flash to root success\n");
    }
    else
    {
        // auto mkfs, remove it if you want to mkfs manual
        rt_kprintf("mount fs on flash to root fail\n");
        if (dfs_mkfs("elm", FS_ROOT) == 0)//Format file system
        {
            rt_kprintf("make elm fs on flash sucess, mount again\n");
            if (dfs_mount(FS_ROOT, "/", "elm", 0, 0) == 0)
                rt_kprintf("mount fs on flash success\n");
            else
                rt_kprintf("mount to fs on flash fail\n");
        }
        else
            rt_kprintf("dfs_mkfs elm flash fail\n");
    }
    return RT_EOK;
}
INIT_ENV_EXPORT(mnt_init);

static bool sdcard_init(void)
{
    // TF card must be inserted before initialization can proceed.
    rt_pin_mode(27, PIN_MODE_INPUT); // PA27
    while (rt_pin_read(27) == PIN_HIGH)
    {
        rt_kprintf("Please insert TF card.\n");
        rt_thread_mdelay(1000);
    }

    rt_kprintf("TF card detected.\n");

    rt_device_t msd = rt_device_find("sd0");
    if (msd == NULL)
    {
        rt_kprintf("Error: the flash device name (sd0) is not found.\n");
        return false;
    }

    if (dfs_mount("sd0", "/sdcard", "elm", 0, 0) != 0) // fs exist
    {
        rt_kprintf("mount fs on tf card to root fail\n");
        return false;
    }

    rt_kprintf("mount fs on tf card to root success\n");
    return true;
}

static uint8_t opus_heap_pool[4096 * 1024] L2_RET_BSS_SECT(opus_heap_pool);
static struct rt_memheap opus_memheap;
int opus_heap_init(void)
{
    rt_memheap_init(&opus_memheap, "opus_memheap", (void *)opus_heap_pool,
                    sizeof(opus_heap_pool));
    return 0;
}

void *opus_heap_malloc(uint32_t size)
{
    return rt_memheap_alloc(&opus_memheap, size);
}

void opus_heap_free(void *p)
{
    rt_memheap_free(p);
}

extern int redrawNesFlag;
extern lv_img_dsc_t nes_img_dsc; // LVGL 图像描述符
extern lv_obj_t * nes_img_obj; // LVGL 图像对象
int nofrendo_main(int argc, char *argv[]);

lv_group_t * g;
lv_indev_t * indev;

void keyboard_read(lv_indev_t * indev, lv_indev_data_t * data) {
    if(input_get_key_state(INPUT_KEY_A))
    {
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
    if(input_get_key_state(INPUT_KEY_DOWN))
    {
        data->key = LV_KEY_NEXT;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }    
    if(input_get_key_state(INPUT_KEY_UP))
    {
        data->key = LV_KEY_PREV;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
        if(input_get_key_state(INPUT_KEY_R))
    {
        data->key = LV_KEY_NEXT;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }    
    if(input_get_key_state(INPUT_KEY_L))
    {
        data->key = LV_KEY_PREV;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
}

void key_deinit();
rt_thread_t nes_thread = RT_NULL;
lv_obj_t * cont;
extern uint16_t *lcdfb;
void emu_thread_entry(void *parameter)
{
    char* filename = parameter;//"mario.nes";

    char* args[1] = { filename };

    rt_thread_mdelay(100); // wait for lvgl ready

    nofrendo_main(1,args);

    rt_kprintf("Emu thread exit.\n");
    nes_thread = RT_NULL;
    key_deinit();
}

static void btn_event_cb(lv_event_t * e)
{
    rt_kprintf("Button clicked\n");
    if(nes_thread != RT_NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    const char *file_name = NULL;
    if (code == LV_EVENT_RELEASED) {
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *label = lv_obj_get_child(btn, 0); // 获取第一个子对象（标签）

        if (label && lv_obj_check_type(label, &lv_label_class)) {
            file_name = lv_label_get_text(label);
            printf("Clicked file: %s\n", file_name);
        }
    }

    nes_thread = rt_thread_create("nes_launcher",
        (void(*)(void*))emu_thread_entry, (void*)file_name,
        16384, 21, 100);
    if (nes_thread != NULL) {
        rt_thread_startup(nes_thread);
    }
    lv_obj_delete_delayed(cont,500);
}

/* 当按钮获得焦点时，滚动容器使该按钮可见（结合 scroll snap center 可居中） */
static void btn_focused_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    /* 将按钮滚动到可见区域，配合 lv_obj_set_scroll_snap_x 设置可以使其居中 */
    lv_obj_scroll_to_view(btn, LV_ANIM_ON);
    // lv_obj_t * parent = lv_obj_get_parent(btn);
    // int x = 0;
    // for(int i=0;i<lv_obj_get_child_cnt(parent);)
    // {
    //     lv_obj_t * child = lv_obj_get_child(parent, i);
    //     if(child == btn) break;
    //     x = i * 120 + 120;
    //     i++;
    // }
    // lv_obj_scroll_to_x(parent, x, LV_ANIM_ON);
}

static void scroll_event_cb(lv_event_t * e)
{
    lv_obj_t * cont = lv_event_get_target(e);
    int scroll_y = lv_obj_get_scroll_y(cont);
    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    uint32_t i;
    for(i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        int child_y = lv_obj_get_y(child);
        int rel_y = child_y - scroll_y;
        int diff_to_center = rel_y - (lv_obj_get_height(cont) / 2 - lv_obj_get_height(child) / 2);
        diff_to_center = diff_to_center > 0 ? diff_to_center : -diff_to_center;
        // rt_kprintf("diff to center: %d\n", diff_to_center);
        float ratio = 1.0f;
        if(diff_to_center < 50)
        {
            ratio = 1.0f;
        }
        else if(diff_to_center < 60)
        {
            ratio = 1.0f - ((float)(diff_to_center - 50) / 10.0f) * 0.1f;
        }
        else
        {
            ratio = 0.9f;
        }
        lv_obj_set_style_transform_pivot_x(child, lv_obj_get_width(child) / 2, 0);
        lv_obj_set_style_transform_pivot_y(child, lv_obj_get_height(child) / 2, 0);
        lv_obj_set_style_transform_zoom(child, (int)(256 * ratio), 0);
    }
}

int change_extension(const char *filename, char *new_filename, int new_size, const char *new_ext)
{
    const char *dot = strrchr(filename, '.');
    int base_len;
    if (!dot || dot == filename)
        base_len = strlen(filename);
    else
        base_len = dot - filename;
    if (base_len + 1 + strlen(new_ext) + 1 > new_size)
        return -1;
    strncpy(new_filename, filename, base_len);
    new_filename[base_len] = '.';
    strcpy(new_filename + base_len + 1, new_ext);
    return 0;
}

int get_extension(const char *filename, char *ext, int ext_size)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return -1;
    strncpy(ext, dot + 1, ext_size - 1);
    ext[ext_size - 1] = '\0'; // Ensure null-termination
    return 0;
}

int extension_filter(const char *ext)
{
    for(size_t i=0;i<sizeof(supported_extensions)/sizeof(supported_extensions[0]);i++)
    {
        if(strcmp(ext, supported_extensions[i])==0) return 1;
    }
    return 0;
}

void list_init(void)
{
    cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, 320, 240);
    // lv_obj_set_pos(cont, 240-1, 0);
    // lv_obj_set_style_transform_pivot_x(cont, 120, 0);
    // lv_obj_set_style_transform_pivot_y(cont, 160, 0);
    // lv_obj_set_style_transform_angle(cont, 900, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_50, 0);
    // lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(cont, scroll_event_cb, LV_EVENT_SCROLL, NULL);
    // lv_obj_set_style_radius(cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(cont, true, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    /* 水平方向启用 snap 到中心，这样在调用 scroll_to_view 时会把子对象居中显示 */
    // lv_obj_set_scroll_snap_x(cont, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

    int cnt = 0;
    DIR *dirp;
    struct dirent *d; dirp = opendir("/");
    if (dirp == RT_NULL)
    {
        rt_kprintf("open directory error!\n");
    }
    while ((d = readdir(dirp)) != RT_NULL)
    {
        char ext[16];
        if(get_extension(d->d_name, ext, sizeof(ext)) == 0)
        {
            if(!extension_filter(ext)) continue;
        }
        else
        {
            continue;
        }
        lv_obj_t * btn = lv_imagebutton_create(cont);
        // lv_obj_set_pos(btn, cnt * 120 + 90, -100);
        // lv_obj_set_pos(btn, 0,0);
        cnt++;
        char img_path[256];
        sprintf(img_path, "/%s", d->d_name);
        change_extension(img_path, img_path, sizeof(img_path), "jpg");
        if(access(img_path, 0) == 0)
        {
            static char lvg_img_path[260];
            sprintf(lvg_img_path, "A:%s", img_path+1);
            rt_kprintf("Image path: %s\n", lvg_img_path);
            rt_kprintf("%c\n", lvg_img_path[0]);
            lv_imagebutton_set_src(btn, LV_IMGBTN_STATE_RELEASED, NULL, lvg_img_path, NULL);
        }
        else
        {
            lv_imagebutton_set_src(btn, LV_IMGBTN_STATE_RELEASED, NULL, &logo_dsc, NULL);
        }
        lv_group_add_obj(g, btn);
        // lv_obj_set_width(btn, lv_pct(60));
        lv_obj_set_width(btn, 120);
        lv_obj_set_height(btn, 90);
        lv_obj_set_align(btn, LV_ALIGN_CENTER);
        // lv_obj_set_style_transform_pivot_x(btn, lv_obj_get_width(btn) / 2, 0);
        // lv_obj_set_style_transform_pivot_y(btn, lv_obj_get_height(btn) / 2, 0);
        // lv_obj_set_style_transform_angle(btn, 900, 0);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text_fmt(label, "%s", d->d_name);
    lv_obj_set_style_text_font(label, &nmdws_16, 0);
    /* Make the label narrower than the button and enable circular scrolling
     * so long filenames will scroll (marquee). Adjust percent if needed. */
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_RELEASED, NULL);
    /* 当按钮被键盘/方向键选中（获得焦点）时，让容器滚动并居中该按钮 */
    lv_obj_add_event_cb(btn, btn_focused_cb, LV_EVENT_FOCUSED, NULL);
    }
    closedir(dirp);

    /*Update the buttons position manually for first*/
    lv_obj_send_event(cont, LV_EVENT_SCROLL, NULL);

    /*Be sure the fist button is in the middle*/
    lv_obj_scroll_to_view(lv_obj_get_child(cont, 0), LV_ANIM_OFF);
}

void delete_nes_thread(void)
{
    if(nes_thread != RT_NULL) rt_thread_delete(nes_thread);
}
MSH_CMD_EXPORT(delete_nes_thread, delete nes thread);

// 临时使用
static struct rt_i2c_bus_device *i2c_bus = NULL;
uint16_t get_aw9523_input()
{
    
    // HAL_PIN_Set(PAD_PA11, I2C1_SCL, PIN_PULLUP, 1); // i2c io select
    // HAL_PIN_Set(PAD_PA10, I2C1_SDA, PIN_PULLUP, 1);
    // HAL_PIN_Set(PAD_PA09, GPIO_A0 + 9, PIN_PULLUP, 1);
    // uint16_t port_data = 0;
    // uint8_t buf[2];
    // buf[0] = 0x00; buf[1] = 0;
    // rt_i2c_master_send(i2c_bus, 0x5B, RT_I2C_WR , buf, 1);
    // rt_i2c_master_recv(i2c_bus, 0x5B, RT_I2C_RD , buf + 1, 1);
    // port_data |= buf[1];
    // buf[0] = 0x01; buf[1] = 0;
    // rt_i2c_master_send(i2c_bus, 0x5B, RT_I2C_WR , buf, 1);
    // rt_i2c_master_recv(i2c_bus, 0x5B, RT_I2C_RD , buf + 1, 1);
    // port_data |= (buf[1] << 8);
    // return port_data;
    // rt_kprintf("port data: 0x%04X\n", port_data);
    // rt_thread_mdelay(10);
}


/**
 * @brief  Main program
 * @param  None
 * @retval 0 if success, otherwise failure number
 */
int main(void)
{
    /* Output a message on console using printf function */
    rt_kprintf("lchspi-extension.\n");

    opus_heap_init();

    rt_err_t ret = RT_EOK;
    rt_err_t err;
    rt_uint32_t ms;
    /* init littlevGL */
    ret = littlevgl2rtt_init("lcd");
    HAL_PIN_Set(PAD_PA00 + 41, LCDC1_8080_DIO5, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + 37, LCDC1_8080_DIO2, PIN_PULLDOWN, 1);
    if (ret != RT_EOK)
    {
        return ret;
    }
    // lv_display_set_rotation(NULL, LV_DISPLAY_ROTATION_270);

    // TODO: 替换成分离到 audio.c 的接口
    audio_init();

    void * work;
    JDEC jdec;
    JRESULT jres;
    IODEV devid;

    devid.fp = fopen("NES_logo.jpg", "rb");
    if(devid.fp == NULL)
    {
        rt_kprintf("Open image file failed!\n");
    }
    else
    {
        work = malloc(3100*2); // 分配解码工作区
        jres = jd_prepare(&jdec, in_func, work, 3100*2, &devid);
        if (jres == JDR_OK)
        {
            rt_kprintf("Image prepared: %d x %d\n", jdec.width, jdec.height);
            devid.fbuf = opus_heap_malloc(jdec.width * jdec.height * 3); // RGB888
            devid.wfbuf = jdec.width;
            jres = jd_decomp(&jdec, out_func, 0); // 开始解码
            if (jres == JDR_OK)
            {
                rt_kprintf("Image decoded successfully.\n");
            }
            else
            {
                rt_kprintf("Image decode failed: %d\n", jres);
            }
        }
        else
        {
            rt_kprintf("Image prepare failed: %d\n", jres);
        }
        // for(int i=0;i<jdec.width * jdec.height;i++)
        // {
        //     uint8_t r = devid.fbuf[i*3];
        //     uint8_t g = devid.fbuf[i*3+1];
        //     uint8_t b = devid.fbuf[i*3+2];
        //     uint16_t rgb565 = (r>>3) | (g>>2)<<5 | (b>>3) <<11;
        //     ((uint16_t *)(devid.fbuf))[i] = rgb565;
        // }
        logo_dsc.header.always_zero = 0;
        logo_dsc.header.w = jdec.width;
        logo_dsc.header.h = jdec.height;
        logo_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
        logo_dsc.data_size = jdec.width * jdec.height * 3;
        logo_dsc.data = devid.fbuf;
        logo_data = devid.fbuf;
        fclose(devid.fp);
        free(work);
    }

    devid.fp = fopen("bg.jpg", "rb");
    if(devid.fp == NULL)
    {
        rt_kprintf("Open image file failed!\n");
    }
    else
    {
        work = malloc(3100*2); // 分配解码工作区
        jres = jd_prepare(&jdec, in_func, work, 3100*2, &devid);
        if (jres == JDR_OK)
        {
            rt_kprintf("Image prepared: %d x %d\n", jdec.width, jdec.height);
            devid.fbuf = opus_heap_malloc(jdec.width * jdec.height * 3); // RGB888
            devid.wfbuf = jdec.width;
            jres = jd_decomp(&jdec, out_func, 0); // 开始解码
            if (jres == JDR_OK)
            {
                rt_kprintf("Image decoded successfully.\n");
            }
            else
            {
                rt_kprintf("Image decode failed: %d\n", jres);
            }
        }
        else
        {
            rt_kprintf("Image prepare failed: %d\n", jres);
        }
        // for(int i=0;i<jdec.width * jdec.height;i++)
        // {
        //     uint8_t r = devid.fbuf[i*3];
        //     uint8_t g = devid.fbuf[i*3+1];
        //     uint8_t b = devid.fbuf[i*3+2];
        //     uint16_t rgb565 = (r>>3) | (g>>2)<<5 | (b>>3) <<11;
        //     ((uint16_t *)(devid.fbuf))[i] = rgb565;
        // }
        bg_dsc.header.always_zero = 0;
        bg_dsc.header.w = jdec.width;
        bg_dsc.header.h = jdec.height;
        bg_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
        bg_dsc.data_size = jdec.width * jdec.height * 3;
        bg_dsc.data = devid.fbuf;
        fclose(devid.fp);
        free(work);
    }

    input_init();
    g = lv_group_create();
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keyboard_read);
    lv_indev_set_group(indev, g);
    bg_img = lv_img_create(lv_scr_act());
    lv_img_set_src(bg_img, &bg_dsc);
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);
    // lv_img_set_angle(bg_img, 900);
    float ratio_x = (float)320 / (float)bg_dsc.header.w;
    float ratio_y = (float)240 / (float)bg_dsc.header.h;
    float ratio = ratio_x > ratio_y ? ratio_x : ratio_y;
    lv_img_set_zoom(bg_img, (int)(256 * ratio));
    list_init(); 
    /* Infinite loop */
    // mkdir("/sdcard",0x0777);
    // sdcard_init();   
    // nes_thread = rt_thread_create("nes_launcher",
    // (void(*)(void*))emu_thread_entry, (void*)"/sdcard/7100324.nes",
    // 16384, 21, 100);
    // if (nes_thread != NULL) {
    //     rt_thread_startup(nes_thread);
    // }

    while (1)
    {
        if(nes_thread == RT_NULL)
        {
            if(nes_img_obj != NULL)
            {
                lv_obj_delete(nes_img_obj);
                nes_img_obj = NULL;
                free(lcdfb);
                lcdfb = NULL;
                list_init();
                lv_obj_clear_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
            }
            ms = lv_task_handler();
            rt_thread_mdelay(ms);
        }
        else
        {
            rt_thread_mdelay(10);
        }
    }
    return 0;
}


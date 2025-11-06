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
    uint16_t state = input_get_state();
    // rt_kprintf("key state: 0x%04X\n", state);
    if(!(state&0x80))
    {
        data->key = LV_KEY_NEXT;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
    // else{
    //     data->key = LV_KEY_NEXT;
    //     data->state = LV_INDEV_STATE_RELEASED;
    //     return;
    // }
    if(!(state&0x01))
    {
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
    // else{
    //     data->key = LV_KEY_ENTER;
    //     data->state = LV_INDEV_STATE_RELEASED;
    //     return;
    // }
//   if(key_pressed()) {
//      data->key = my_last_key();            /* Get the last pressed or released key */
//      data->state = LV_INDEV_STATE_PRESSED;
//   } else {
//      data->state = LV_INDEV_STATE_RELEASED;
//   }
}

void key_deinit();
rt_thread_t nes_thread = RT_NULL;
lv_obj_t * cont;
extern uint16_t *lcdfb;
void emu_thread_entry(void *parameter)
{
    char* filename = parameter;//"mario.nes";

    char* args[1] = { filename };

    rt_thread_mdelay(2000); // wait for lvgl ready

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
    lv_obj_delete_delayed(cont,1000);
}

static void scroll_event_cb(lv_event_t * e)
{
    lv_obj_t * cont = lv_event_get_target(e);

    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);
    int32_t cont_y_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;

    int32_t r = lv_obj_get_height(cont) * 7 / 10;
    uint32_t i;
    uint32_t child_cnt = lv_obj_get_child_count(cont);
    for(i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);

        int32_t child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;

        int32_t diff_y = child_y_center - cont_y_center;
        diff_y = LV_ABS(diff_y);

        /*Get the x of diff_y on a circle.*/
        int32_t x;
        /*If diff_y is out of the circle use the last point of the circle (the radius)*/
        if(diff_y >= r) {
            x = r;
        }
        else {
            /*Use Pythagoras theorem to get x from radius and y*/
            uint32_t x_sqr = r * r - diff_y * diff_y;
            lv_sqrt_res_t res;
            lv_sqrt(x_sqr, &res, 0x8000);   /*Use lvgl's built in sqrt root function*/
            x = r - res.i;
        }

        /*Translate the item by the calculated X coordinate*/
        lv_obj_set_style_translate_x(child, x, 0);

        /*Use some opacity with larger translations*/
        lv_opa_t opa = lv_map(x, 0, r, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_obj_set_style_opa(child, LV_OPA_COVER - opa, 0);
    }
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
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(cont, scroll_event_cb, LV_EVENT_SCROLL, NULL);
    // lv_obj_set_style_radius(cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(cont, true, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

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
        lv_obj_t * btn = lv_button_create(cont);
        lv_group_add_obj(g, btn);
        lv_obj_set_width(btn, lv_pct(60));

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text_fmt(label, "%s", d->d_name);
    lv_obj_set_style_text_font(label, &nmdws_16, 0);
    /* Make the label narrower than the button and enable circular scrolling
     * so long filenames will scroll (marquee). Adjust percent if needed. */
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_RELEASED, NULL);
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

    // TODO: 替换成分离到 audio.c 的接口

    input_init();
    g = lv_group_create();
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keyboard_read);
    lv_indev_set_group(indev, g);
    list_init();    
    // nes_thread = rt_thread_create("nes_launcher",
    // (void(*)(void*))emu_thread_entry, (void*)"7100324.nes",
    // 16384, 21, 100);
    // if (nes_thread != NULL) {
    //     rt_thread_startup(nes_thread);
    // }
    /* Infinite loop */

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


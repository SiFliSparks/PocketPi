// TODO: 拆分这个文件的功能到多个文件
// TODO: 整理头文件
#include <math.h>
#include <string.h>
#include <noftypes.h>
#include <bitmap.h>
#include <nofconfig.h>
#include <event.h>
#include <gui.h>
#include "log.h"
#include <nes.h>
#include <nes_pal.h>
#include <nesinput.h>
#include "nesstate.h"
#include <osd.h>
#include <stdlib.h>
#include <stdint.h>

#include "rtthread.h"
#include "bf0_hal.h"
#include "drv_io.h"

#include "input.h"
#include "audio.h"

#include "drv_epic.h"
#include "string.h"

// TODO: 删除无用函数

void key_scan();
void key_init()
{
    //make compiler happy
}

void key_deinit()
{
    //make compiler happy
}

int get_key_state(int key_index)
{
   //make compiler happy
}

void key_scan()
{
    //make compiler happy
}

int get_key_press_event(int key_index)
{
    //make compiler happy
}

int get_key_release_event(int key_index)
{
    //make compiler happy
}

void key_set(int argc, char **argv)
{
    //make compiler happy
}
MSH_CMD_EXPORT(key_set, set key_state value e.g: key_debug 0x0000);

void *opus_heap_malloc(uint32_t size);

void opus_heap_free(void *p);

#include "rtthread.h"
#include <drivers/audio.h>
#include "littlevgl2rtt.h"

lv_img_dsc_t nes_img_dsc; // LVGL 图像描述符
lv_obj_t * nes_img_obj; // LVGL 图像对象

#define DEFAULT_SAMPLERATE 32000
#define DEFAULT_FRAGSIZE 512

#define DEFAULT_FRAME_WIDTH 256
#define DEFAULT_FRAME_HEIGHT 240//NES_VISIBLE_HEIGHT

int showOverlay = 0;
static void SaveState();
static void LoadState();

rt_timer_t timer = NULL;
int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize)
{
    printf("Timer install, freq=%d\n", frequency);

    // 目前模拟器不依赖该定时器回调作为参考时间。
    
    return 0;
}

/*
** Audio
*/
void (*audio_callback)(void *buffer, int length) = NULL;
static short audio_frame[4096];
int audio_write_p = 0;
extern rt_event_t g_tx_ev;
extern rt_device_t audprc_dev;
extern rt_device_t audcodec_dev;
uint32_t audio_shift_bits = 4;


#define RING_BUFFER_LENGTH 1536

void do_audio_frame()
{
    // int nsamples = DEFAULT_SAMPLERATE / NES_REFRESH_RATE + 1;
    // int free = audio_ring_get_free();
    // if(nsamples > free) nsamples = free;
    // audio_callback(audio_frame, nsamples);
    // for(int i=0;i<nsamples;i++)
    // {
    //     audio_frame[i] = audio_frame[i] >> audio_shift_bits;
    // }
    // audio_ring_buffer_put(audio_frame, nsamples);
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
    //Indicates we should call playfunc() to get more data.
    audio_callback = playfunc;
}

void osd_stopsound(void)
{
    audio_callback = NULL;
}

static int osd_init_sound(void)
{
    return 0;
}

void osd_getsoundinfo(sndinfo_t *info)
{
    info->sample_rate = DEFAULT_SAMPLERATE;
    info->bps = 16;
}

/*
** Video
*/

static int init(int width, int height);
static void shutdown(void);
static int set_mode(int width, int height);
static void set_palette(rgb_t *pal);
static void clear(uint8 color);
static bitmap_t *lock_write(void);
static void free_write(int num_dirties, rect_t *dirty_rects);
static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects);
static char fb[1]; //dummy

// QueueHandle_t vidQueue;

viddriver_t sdlDriver =
    {
        "Simple DirectMedia Layer", /* name */
        init,                       /* init */
        shutdown,                   /* shutdown */
        set_mode,                   /* set_mode */
        set_palette,                /* set_palette */
        clear,                      /* clear */
        lock_write,                 /* lock_write */
        free_write,                 /* free_write */
        custom_blit,                /* custom_blit */
        false                       /* invalidate flag */
};

bitmap_t *myBitmap;

void osd_getvideoinfo(vidinfo_t *info)
{
    info->default_width = DEFAULT_FRAME_WIDTH;
    info->default_height = DEFAULT_FRAME_HEIGHT;
    info->driver = &sdlDriver;
}

/* flip between full screen and windowed */
void osd_togglefullscreen(int code)
{
}

/* initialise video */
static int init(int width, int height)
{
    return 0;
}

static void shutdown(void)
{
}

/* set a video mode */
static int set_mode(int width, int height)
{
    return 0;
}

uint16_t myPalette[256];
uint32_t PaletteARGB[256];

/* copy nes palette over to hardware */
static void set_palette(rgb_t *pal)
{
    uint16 c;

    int i;

    for (i = 0; i < 256; i++)
    {
        c = (pal[i].b >> 3) + ((pal[i].g >> 2) << 5) + ((pal[i].r >> 3) << 11);
        myPalette[i] = (c >> 8) | ((c & 0xff) << 8);
        // myPalette[i]=c;
    }
    for(i=0;i<256;i++)
    {
        PaletteARGB[i] = (0xff << 24) | (pal[i].r <<16) | (pal[i].g <<8) | (pal[i].b);
        // lv_color32_t col;
        // col.blue = pal[i].b;
        // col.green = pal[i].g;
        // col.red = pal[i].r;
        // col.alpha = 0xff;
        // lv_image_buf_set_palette(&nes_img_dsc, i, col);
    }
}

/* clear all frames to a particular color */
static void clear(uint8 color)
{
    //   SDL_FillRect(mySurface, 0, color);
}

/* acquire the directbuffer for writing */
static bitmap_t *lock_write(void)
{
    //   SDL_LockSurface(mySurface);
    myBitmap = bmp_createhw((uint8 *)fb, DEFAULT_FRAME_WIDTH, DEFAULT_FRAME_HEIGHT, DEFAULT_FRAME_WIDTH * 2);
    return myBitmap;
}

/* release the resource */
static void free_write(int num_dirties, rect_t *dirty_rects)
{
    bmp_destroy(&myBitmap);
}

static rt_err_t lcd_flush_done(rt_device_t dev, void *buffer)
{
    // redrawNesFlag = 0;
}

int is_paused = 0;
uint16_t *lcdfb;
extern lv_obj_t * bg_img;
int redrawNesFlag = 0; // 0:not redraw, 1:need redraw, 2:redrawing
EPIC_LayerConfigTypeDef input_layers[1];
EPIC_LayerConfigTypeDef output_layers[1];
rt_device_t disp = RT_NULL;
static uint8_t tmp[256*224];
static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects)
{
    // printf("look up start: %d\n",(int)rt_tick_get());
    // while(redrawNesFlag) rt_thread_mdelay(1);
    // for(int y=0;y<224;y++)
    // {
    //     for(int x=0;x<256;x++)
    //     {
    //         uint16_t pixel = myPalette[bmp->line[0][y*256+x]];
    //         pixel = (pixel >> 8) | ((pixel & 0xff) << 8);
    //         ((uint16_t *)lcdfb)[y*256+x] = (pixel);
    //         // ((uint16_t *)lcdfb)[224*(256-x-1)+y] = (pixel); //rotate -90
    //     }
    // }
    // printf("look up end: %d\n",(int)rt_tick_get());
    // printf("flush start: %d\n",(int)rt_tick_get());
    
    int32_t t = rt_tick_get();
    memcpy(tmp, bmp->line[0], 256*224);
    HAL_EPIC_LayerConfigInit(&input_layers[0]);
        input_layers[0].data = (uint8_t *)tmp;
        input_layers[0].color_mode = EPIC_COLOR_L8;
        input_layers[0].width = 256;  // 270
        input_layers[0].height = 224; // 270
        input_layers[0].x_offset = -16;
        input_layers[0].y_offset = 16;
        input_layers[0].total_width = 256;
        input_layers[0].color_en = false;
        input_layers[0].alpha = 255;
        input_layers[0].ax_mode = ALPHA_BLEND_RGBCOLOR;
        input_layers[0].lookup_table = (uint8_t *)PaletteARGB;
        input_layers[0].lookup_table_size = 256;
        input_layers[0].transform_cfg.angle = 900;   // 设置旋转角度
        input_layers[0].transform_cfg.pivot_x = 128; // 设置旋转中心X坐标
        input_layers[0].transform_cfg.pivot_y = 112; // 设置旋转中心Y坐标

    HAL_EPIC_LayerConfigInit(&output_layers[0]);
        output_layers[0].data = (uint8_t *)lcdfb;
        output_layers[0].color_mode = EPIC_COLOR_RGB565;
        output_layers[0].width = 224;  // 270
        output_layers[0].height = 256; // 270
        output_layers[0].x_offset = 0;
        output_layers[0].y_offset = 0;
        output_layers[0].total_width = 224;
        output_layers[0].color_en = false;
        output_layers[0].alpha = 255;
        output_layers[0].ax_mode = ALPHA_BLEND_RGBCOLOR;
        output_layers[0].lookup_table = NULL;
        output_layers[0].lookup_table_size = 0;
    
    drv_epic_blend(input_layers, 1, output_layers, NULL);
    // drv_epic_wait_done();
    // rt_kprintf("NES frame blit time: %d ms\n", (int)(rt_tick_get() - t));

    lv_img_cache_invalidate_src(&nes_img_dsc);
    lv_obj_invalidate(nes_img_obj); // 标记该对象需要重绘
    // printf("flush end: %d\n",(int)rt_tick_get());

    // lv_obj_add_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
    // uint32_t ms = lv_task_handler();
    // rt_thread_mdelay(ms);

    if((disp!=RT_NULL) && (!is_paused))
    {
        // rt_graphix_ops(disp)->set_window(0, 0, 256, 224);
    rt_device_set_tx_complete(disp, lcd_flush_done);
        rt_graphix_ops(disp)->draw_rect_async((const char *)lcdfb, (240-224)/2, (320-256)/2, 224-1+(240-224)/2, 256-1+(320-256)/2);
    }
    if(is_paused)
    {
        lv_task_handler();
    }
}

static void SaveState()
{
    printf("Saving state.\n");

    // save_sram();

    printf("Saving state OK.\n");
}

static void PowerDown()
{

}

/*
** Input
*/

static void osd_initinput()
{
}

// input_gamepad_state previous_state;
static bool ignoreMenuButton;
static unsigned short powerFrameCount;

/**
 * @brief temporary function to trigger quit event
 */
void trigger_quit()
{
    event_t evh = event_get(event_quit);
    if (evh)
    {
        evh(INP_STATE_MAKE);
        evh(INP_STATE_BREAK);
    }
}
MSH_CMD_EXPORT(trigger_quit, trigger quit event);

void trigger_event(int event_id)
{
    event_t evh = event_get(event_id);
    if (evh)
    {
        evh(INP_STATE_MAKE);
        evh(INP_STATE_BREAK);
    }
}

void tev(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage: tev <event_id>\n");
        rt_kprintf("Example: tev 1\n");
        return;
    }

    /* 将字符串参数转换为整型数 */
    char *endptr;
    long parsed_value = strtol(argv[1], &endptr, 10); /* 10表示10进制 */

    /* 检查转换是否成功 */
    if (*endptr != '\0')
    {
        rt_kprintf("Error: Invalid event ID. Please input a number.\n");
        return;
    }

    /* 检查值是否在合理的范围内 (根据你的变量类型调整，这里假设event_id是int) */
    if (parsed_value < 0 || parsed_value > 60) // 假设事件ID在0到60之间
    {
        rt_kprintf("Error: Event ID out of range. Please input between 0 and 60.\n");
        return;
    }

    trigger_event((int)parsed_value);
}
MSH_CMD_EXPORT(tev, trigger event by id e.g: tev 1);

void nes_save_state()
{
    state_save();
}
MSH_CMD_EXPORT(nes_save_state, save nes state);
static lv_obj_t * pause_menu = NULL;
extern lv_group_t * g;
extern lv_indev_t * indev;


// 销毁暂停菜单
static void destroy_pause_menu(void)
{
    if (pause_menu) {
        // 从组中移除按钮
        uint32_t child_cnt = lv_obj_get_child_cnt(pause_menu);
        for (uint32_t i = 0; i < child_cnt; i++) {
            lv_obj_t * child = lv_obj_get_child(pause_menu, i);
            if (lv_obj_check_type(child, &lv_btn_class)) {
                lv_group_remove_obj(child);
            }
        }
        
        lv_obj_del(pause_menu);
        pause_menu = NULL;
    }
}

// 暂停菜单按钮事件回调
static void pause_menu_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t * btn = lv_event_get_target(e);
        int action = (int)(intptr_t)lv_obj_get_user_data(btn);
        
        switch(action) {
            case 0: // 存档
                rt_kprintf("Saving state...\n");
                state_save();
                rt_kprintf("State saved\n");
                break;
                
            case 1: // 读档
                rt_kprintf("Loading state...\n");
                state_load();
                rt_kprintf("State loaded\n");
                break;
                
            case 2: // 退出
                rt_kprintf("Exiting game...\n");
                // 退出前先销毁菜单
                destroy_pause_menu();
                is_paused = 0;
                trigger_quit();
                break;
        }
        
        // 恢复游戏（非退出情况）
        if (action != 2) {
            trigger_event(event_togglepause);
            is_paused = 0;
            destroy_pause_menu();
        }
    }
}

// 创建暂停菜单
static void create_pause_menu(void)
{
    if (pause_menu) return; // 已存在
    
    // 创建半透明背景容器
    pause_menu = lv_obj_create(lv_scr_act());
    lv_obj_set_size(pause_menu, 320, 240);
    lv_obj_center(pause_menu);
    lv_obj_set_style_bg_color(pause_menu, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pause_menu, LV_OPA_70, 0);
    lv_obj_set_style_border_width(pause_menu, 0, 0);
    lv_obj_set_style_pad_all(pause_menu, 20, 0);
    lv_obj_set_flex_flow(pause_menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(pause_menu, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 标题
    lv_obj_t * title = lv_label_create(pause_menu);
    lv_label_set_text(title, "PAUSED");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_bottom(title, 20, 0);
    
    const char * btn_texts[] = {"Save State", "Load State", "Exit Game"};
    
    // 创建三个按钮
    for (int i = 0; i < 3; i++) {
        lv_obj_t * btn = lv_btn_create(pause_menu);
        lv_obj_set_width(btn, 200);
        lv_obj_set_height(btn, 50);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1976D2), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x42A5F5), LV_STATE_FOCUSED);
        
        // 添加到输入组
        lv_group_add_obj(g, btn);
        
        // 存储按钮功能ID
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, btn_texts[i]);
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        
        lv_obj_add_event_cb(btn, pause_menu_event_cb, LV_EVENT_CLICKED, NULL);
        
        // 设置间距
        if (i < 2) {
            lv_obj_set_style_pad_bottom(btn, 10, 0);
        }
    }
}

extern uint16_t get_aw9523_input();
extern void trigger_quit();

int selectedSlot = 0;
int lastMenuButtonState = 0;
int lastLButtonState = 0;
int lastRButtonState = 0;
static int ConvertGamepadInput()
{
    int result = 0;

    /* 使用 input_get_key_state 替代 aw_input 位检查。
       说明：文件中已有 INPUT_KEY_A/X/B 的定义与使用，其他按键常量按常见命名假设如下：
       INPUT_KEY_Y, INPUT_KEY_START, INPUT_KEY_SELECT, INPUT_KEY_MENU,
       INPUT_KEY_DOWN, INPUT_KEY_R, INPUT_KEY_LEFT, INPUT_KEY_UP, INPUT_KEY_RIGHT, INPUT_KEY_L
       如果项目中常量命名不同，请告知或我可以搜索并做进一步替换。
    */

    // if (input_get_key_state(INPUT_KEY_MENU))
    // {
    //     /* menu 触发退出（原先使用 aw_input bit6） */
    //     // trigger_quit();
    // }

    int menuButtonState = input_get_key_state(INPUT_KEY_MENU);
    if (menuButtonState && !lastMenuButtonState)
    {
        rt_thread_mdelay(100);
        // Menu button pressed
        trigger_event(event_togglepause);
        is_paused = !is_paused;
        
        if (is_paused) {
            // 进入暂停，创建菜单
            create_pause_menu();
        } else {
            // 退出暂停，销毁菜单
            destroy_pause_menu();
        }
    }
    lastMenuButtonState = menuButtonState;

    int lButtonState = input_get_key_state(INPUT_KEY_L);
    if (lButtonState && !lastLButtonState)
    {
        if(audio_shift_bits < 8) audio_shift_bits++;
    }
    lastLButtonState = lButtonState;
    int rButtonState = input_get_key_state(INPUT_KEY_R);
    if (rButtonState && !lastRButtonState)
    {
        if(audio_shift_bits > 0) audio_shift_bits--;
    }
    lastRButtonState = rButtonState;

    if (input_get_key_state(INPUT_KEY_A)) // a
    {
        result |= (1 << 13);
    }

    if (input_get_key_state(INPUT_KEY_X)) // x
    {
        // this button is unused
    }

    if (input_get_key_state(INPUT_KEY_B)) // b
    {
        result |= (1 << 14);
    }

    if (input_get_key_state(INPUT_KEY_Y)) // y
    {
        // this button is unused
    }

    if (input_get_key_state(INPUT_KEY_START)) // start
    {
        result |= (1 << 3);
    }

    if (input_get_key_state(INPUT_KEY_SELECT)) // select
    {
        result |= (1 << 0);
    }

    if (input_get_key_state(INPUT_KEY_MENU)) // menu（保留但标注为未使用）
    {
        // this button is unused
    }

    if (input_get_key_state(INPUT_KEY_DOWN)) // down
    {
        result |= (1 << 6);
    }

    // no 8-10

    if (input_get_key_state(INPUT_KEY_R)) // r
    {
        // this button is unused
        // if(audio_shift_bits > 0) audio_shift_bits--;
    }

    if (input_get_key_state(INPUT_KEY_LEFT)) // left
    {
        result |= (1 << 7);
    }

    if (input_get_key_state(INPUT_KEY_UP)) // up
    {
        result |= (1 << 4);
    }

    if (input_get_key_state(INPUT_KEY_RIGHT)) // right
    {
        result |= (1 << 5);
    }

    if (input_get_key_state(INPUT_KEY_L)) // l
    {
        // this button is unused
        // if(audio_shift_bits < 8) audio_shift_bits++;
    }
    return result;
}

static int oldb = 0x0000;
void osd_getinput(void)
{
    const int ev[16] = {
        event_joypad1_select,
        0,
        0,
        event_joypad1_start,
        event_joypad1_up,
        event_joypad1_right,
        event_joypad1_down,
        event_joypad1_left,
        0,
        0,
        0,
        0,
        event_soft_reset,
        event_joypad1_a,
        event_joypad1_b,
        event_hard_reset};
    int b = ConvertGamepadInput();
    int chg = b ^ oldb;
    int x;
    oldb = b;
    event_t evh;
    for (x = 0; x < 16; x++)
    {
        if (chg & 1)
        {
            evh = event_get(ev[x]);
            if (evh)
                evh((b & 1) ? INP_STATE_MAKE : INP_STATE_BREAK);
        }
        chg >>= 1;
        b >>= 1;
    }
}

static void osd_freeinput(void)
{
}

void osd_getmouse(int *x, int *y, int *button)
{
}

/*
** Shutdown
*/

/* this is at the bottom, to eliminate warnings */
void osd_shutdown()
{
    osd_stopsound();
    osd_freeinput();
}

static int logprint(const char *string)
{
    return printf("%s", string);
}

/*
** Startup
*/
// Boot state overrides
bool forceConsoleReset = false;

extern void log_chain_logfunc(int (*logfunc)(const char *string));
int osd_init()
{
    log_chain_logfunc(logprint);
    disp = rt_device_find("lcd");

    // lcdfb = malloc(sizeof(uint16_t) * 256 * 224);
    lcdfb = opus_heap_malloc(sizeof(uint16_t) * 256 * 224);
    if(lcdfb == NULL)
    {
        printf("Failed to allocate memory for lcdfb\n");
        return -1;
    }
    key_init();
    nes_img_dsc.header.always_zero = 0;
    nes_img_dsc.header.w = 224;
    nes_img_dsc.header.h = 256;
    nes_img_dsc.data_size = 224 * 256 * 2;
    nes_img_dsc.header.cf = LV_COLOR_FORMAT_NATIVE; 
    nes_img_dsc.data = (const uint8_t *)lcdfb;

    nes_img_obj = lv_img_create(lv_scr_act()); 
    lv_img_set_zoom(nes_img_obj, 256); // 
    lv_img_set_src(nes_img_obj, &nes_img_dsc); // 设置图像数据源
    lv_obj_align(nes_img_obj, LV_ALIGN_CENTER, 0, 0); // 居中显示，你可以调整位置和大小
    lv_img_set_pivot(nes_img_obj, 224/2, 256/2); // 设置旋转中心为图像中心
    lv_img_set_angle(nes_img_obj, 2700); 

    return 0;
}

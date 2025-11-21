// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

// TODO: 改为使用aw9523的驱动

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
static void (*audio_callback)(void *buffer, int length) = NULL;
// QueueHandle_t queue;
static short audio_frame[4096];
int audio_write_p = 0;
extern rt_event_t g_tx_ev;
extern rt_device_t audprc_dev;
extern rt_device_t audcodec_dev;
uint32_t audio_shift_bits = 4;


#define RING_BUFFER_LENGTH 1536

void do_audio_frame()
{
    int nsamples = DEFAULT_SAMPLERATE / NES_REFRESH_RATE + 1;
    int free = audio_ring_get_free();
    if(nsamples > free) nsamples = free;
    audio_callback(audio_frame, nsamples);
    for(int i=0;i<nsamples;i++)
    {
        audio_frame[i] = audio_frame[i] >> audio_shift_bits;
    }
    audio_ring_buffer_put(audio_frame, nsamples);
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
    //Indicates we should call playfunc() to get more data.
    audio_callback = playfunc;
}

static void osd_stopsound(void)
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

// uint16_t lcdfb[256 * 224];
uint16_t *lcdfb;
extern rt_device_t lcd_device;
extern const char *framebuffer;
extern lv_obj_t * bg_img;
int redrawNesFlag = 0; // 0:not redraw, 1:need redraw, 2:redrawing
static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects)
{
    // printf("look up start: %d\n",(int)rt_tick_get());
    // while(redrawNesFlag) rt_thread_mdelay(1);
    for(int y=0;y<224;y++)
    {
        for(int x=0;x<256;x++)
        {
            uint16_t pixel = myPalette[bmp->line[0][y*256+x]];
            pixel = (pixel >> 8) | ((pixel & 0xff) << 8);
            ((uint16_t *)lcdfb)[y*256+x] = (pixel);
            // ((uint16_t *)lcdfb)[224*(256-x-1)+y] = (pixel); //rotate -90
        }
    }
    // printf("look up end: %d\n",(int)rt_tick_get());
    // printf("flush start: %d\n",(int)rt_tick_get());
    
    lv_img_cache_invalidate_src(&nes_img_dsc);
    lv_obj_invalidate(nes_img_obj); // 标记该对象需要重绘
    // printf("flush end: %d\n",(int)rt_tick_get());
    // GPIO_TypeDef *gpio = hwp_gpio1;
    // GPIO_InitTypeDef GPIO_InitStruct;

    // set sensor pin to output mode
    // HAL_RCC_EnableModule(RCC_MOD_GPIO1);
    // GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    // GPIO_InitStruct.Pin = 2;
    // GPIO_InitStruct.Pull = GPIO_PULLUP;
    // HAL_GPIO_Init(gpio, &GPIO_InitStruct);
    // HAL_PIN_Set(PAD_PA00 + 2, GPIO_A0 + 2, PIN_PULLUP, 1);
    // while(HAL_GPIO_ReadPin(hwp_gpio1,2)==0) ;

    lv_obj_add_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
    uint32_t ms = lv_task_handler();
    // rt_thread_mdelay(ms);
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

extern uint16_t get_aw9523_input();
int selectedSlot = 0;
static int ConvertGamepadInput()
{
    int result = 0;

    /* 使用 input_get_key_state 替代 aw_input 位检查。
       说明：文件中已有 INPUT_KEY_A/X/B 的定义与使用，其他按键常量按常见命名假设如下：
       INPUT_KEY_Y, INPUT_KEY_START, INPUT_KEY_SELECT, INPUT_KEY_MENU,
       INPUT_KEY_DOWN, INPUT_KEY_R, INPUT_KEY_LEFT, INPUT_KEY_UP, INPUT_KEY_RIGHT, INPUT_KEY_L
       如果项目中常量命名不同，请告知或我可以搜索并做进一步替换。
    */

    if (input_get_key_state(INPUT_KEY_MENU))
    {
        /* menu 触发退出（原先使用 aw_input bit6） */
        trigger_quit();
    }

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

    lcdfb = malloc(sizeof(uint16_t) * 256 * 224);
    if(lcdfb == NULL)
    {
        printf("Failed to allocate memory for lcdfb\n");
        return -1;
    }
    key_init();
    nes_img_dsc.header.always_zero = 0;
    nes_img_dsc.header.w = 256;
    nes_img_dsc.header.h = 224;
    nes_img_dsc.data_size = 224 * 256 * 2;
    nes_img_dsc.header.cf = LV_COLOR_FORMAT_NATIVE; 
    nes_img_dsc.data = (const uint8_t *)lcdfb;

    nes_img_obj = lv_img_create(lv_scr_act()); 
    lv_img_set_zoom(nes_img_obj, 256); // 
    lv_img_set_src(nes_img_obj, &nes_img_dsc); // 设置图像数据源
    lv_obj_align(nes_img_obj, LV_ALIGN_CENTER, 0, 0); // 居中显示，你可以调整位置和大小
    lv_img_set_pivot(nes_img_obj, 256/2, 224/2); // 设置旋转中心为图像中心
    // lv_img_set_angle(nes_img_obj, 900); 

    return 0;
}

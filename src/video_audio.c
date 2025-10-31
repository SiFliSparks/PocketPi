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

#define hwp_gpio hwp_gpio1
#define RCC_MOD_GPIO RCC_MOD_GPIO1

int key_pin_def[]={
    30,24,25,20,10,11,27,28,29
};

#define KEY_NUM (sizeof(key_pin_def) / sizeof(key_pin_def[0]))

uint8_t key_buffer[KEY_NUM] = {0};
uint32_t key_state = 0;
uint32_t key_release_event = 0;
uint32_t key_press_event = 0;

rt_timer_t key_timer = NULL;
void key_scan();
void key_init()
{
    HAL_RCC_EnableModule(RCC_MOD_GPIO); // GPIO clock enable

    GPIO_InitTypeDef GPIO_InitStruct;

    for(int i=0;i<KEY_NUM;i++)
    {
        GPIO_InitStruct.Pin = key_pin_def[i];
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(hwp_gpio, &GPIO_InitStruct);
        HAL_PIN_Set(PAD_PA00 + key_pin_def[i], GPIO_A0 + key_pin_def[i], PIN_PULLUP, 1);
    }

    key_timer = rt_timer_create(
        "key_scan",                          // 定时器名称
        (void (*)(void*))key_scan,          // 超时回调函数（需强制转换函数类型）
        RT_NULL,                        // 回调函数参数
        RT_TICK_PER_SECOND / 200, // 超时时间（tick数）
        RT_TIMER_FLAG_PERIODIC          // 周期定时模式
    );

    if (key_timer != RT_NULL)
    {
        rt_timer_start(key_timer); // 启动定时器
    }
}

void key_deinit()
{
    if (key_timer != RT_NULL)
    {
        rt_timer_stop(key_timer);
        rt_timer_delete(key_timer);
        key_timer = RT_NULL;
    }
}

int get_key_state(int key_index)
{
    if (key_index < 0 || key_index >= KEY_NUM)
        return 0;
    return (key_state >> key_index) & 0x01;
}

void key_scan()
{
    for(int i=0;i<KEY_NUM;i++)
    {
        key_buffer[i]<<=1;
        if(HAL_GPIO_ReadPin(hwp_gpio,key_pin_def[i])==GPIO_PIN_RESET)
        {
            key_buffer[i]|=0x01;
        }
        if(key_buffer[i]==0xff)
        {
            if(get_key_state(i)==0)
            {
                key_press_event|=1<<i;
                key_release_event&=~(1<<i);
            }
            key_state|=1<<i;
        }
        if(key_buffer[i]==0x00)
        {
            if(get_key_state(i)==1)
            {
                key_release_event|=1<<i;
                key_press_event&=~(1<<i);
            }
            key_state&=~(1<<i);
        }
    }
}

int get_key_press_event(int key_index)
{
    if (key_index < 0 || key_index >= KEY_NUM)
        return 0;
    int ret = (key_press_event >> key_index) & 0x01;
    key_press_event &= ~(1 << key_index);
    return ret;
}

int get_key_release_event(int key_index)
{
    if (key_index < 0 || key_index >= KEY_NUM)
        return 0;
    int ret = (key_release_event >> key_index) & 0x01;
    key_release_event &= ~(1 << key_index);
    return ret;
}

void key_set(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage: key_set <hex_value>\n");
        rt_kprintf("Example: key_set 0x00A1\n");
        return;
    }

    /* 将字符串参数转换为整型数 */
    char *endptr;
    long parsed_value = strtol(argv[1], &endptr, 16); /* 16表示16进制 */

    /* 检查转换是否成功 */
    if (*endptr != '\0')
    {
        // rt_kprintf("Error: Invalid hex value. Please input like '0x0000'.\n");
        return;
    }

    /* 检查值是否在合理的范围内 (根据你的变量类型调整，这里假设key_state是int) */
    if (parsed_value < 0 || parsed_value > 0xFFFF)
    {
        // rt_kprintf("Error: Value out of range. Please input between 0x0000 and 0xFFFF.\n");
        return;
    }

    key_state = (int)parsed_value;
    // rt_kprintf("key_state set to 0x%04X\n", key_state); /* %04X表示输出4位十六进制，不足补零 */
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
    
    // // 创建RT-Thread定时器（周期模式）
    // timer = rt_timer_create(
    //     "nes",                          // 定时器名称
    //     (void (*)(void*))func,          // 超时回调函数（需强制转换函数类型）
    //     RT_NULL,                        // 回调函数参数
    //     RT_TICK_PER_SECOND / frequency, // 超时时间（tick数）
    //     RT_TIMER_FLAG_PERIODIC          // 周期定时模式
    // );
    
    // // 启动定时器
    // if (timer != RT_NULL) {
    //     rt_timer_start(timer);
    // } else {
    //     printf("Failed to create timer!\n");
    //     return -1;
    // }
    
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
uint32_t audio_shift_bits = 5;


#define RING_BUFFER_LENGTH 1536
typedef struct audio_ring_buffer_s
{
    int16_t samples[RING_BUFFER_LENGTH];
    int write_p, read_p;
} audio_ring_buffer_t;
audio_ring_buffer_t audio_ring_buffer = {
    .write_p = 0,
    .read_p = 0
};

int audio_ring_get_buffered()
{
    int ret = audio_ring_buffer.write_p - audio_ring_buffer.read_p;
    if (ret < 0)
        ret += RING_BUFFER_LENGTH;
    return ret;
}

int audio_ring_get_free()
{
    return RING_BUFFER_LENGTH - audio_ring_get_buffered() - 1;
}

void audio_ring_buffer_put(int16_t *samples, uint32_t shift_bits, int length)
{
    for (int i = 0; i < length; i++)
    {
        audio_ring_buffer.samples[audio_ring_buffer.write_p++] = samples[i] >> shift_bits;
        if (audio_ring_buffer.write_p == RING_BUFFER_LENGTH)
            audio_ring_buffer.write_p = 0;
    }
}

void audio_ring_buffer_get(int16_t *samples, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (audio_ring_buffer.read_p == audio_ring_buffer.write_p)
        {
            samples[i] = 0;
        }
        else
        {
            samples[i] = audio_ring_buffer.samples[audio_ring_buffer.read_p++];
            if (audio_ring_buffer.read_p == RING_BUFFER_LENGTH)
                audio_ring_buffer.read_p = 0;
        }
    }
}

void do_audio_frame()
{
    // printf("audio frame start: %d\n",(int)rt_tick_get());
    // printf("audio buffer now level: %d\n", audio_ring_get_buffered());
    int nsamples = DEFAULT_SAMPLERATE / NES_REFRESH_RATE + 1;
    int free = audio_ring_get_free();
    if(nsamples > free)
        nsamples = free;
    audio_callback(audio_frame, nsamples);
    audio_ring_buffer_put(audio_frame, audio_shift_bits, nsamples);
    // printf("audio buffer level after put: %d\n", audio_ring_get_buffered());
    // printf("audio frame end: %d\n",(int)rt_tick_get());
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

extern uint16_t get_aw9523_input();
int selectedSlot = 0;
static int ConvertGamepadInput()
{
    int result = 0;

    uint16_t aw_input = ~get_aw9523_input();

    if(aw_input & (1 << 0)) // a
    {
        result |= (1 << 13);
    }

    if(aw_input & (1 << 1)) // x
    {
        // this button is unused
    }

    if(aw_input & (1 << 2)) // b
    {
        result |= (1 << 14);
    }

    if(aw_input & (1 << 3)) // y
    {
        // this button is unused
    }

    if(aw_input & (1 << 4)) // start
    {
        result |= (1 << 3);
    }

    if(aw_input & (1 << 5)) // select
    {
        result |= (1 << 0);
    }

    if(aw_input & (1 << 6)) // menu
    {
        // this button is unused
    }
    
    if(aw_input & (1 << 7)) // down
    {
        result |= (1 << 6);
    }
    
    // no 8-10

    if(aw_input & (1 << 11)) // r
    {
        // this button is unused
    }

    if(aw_input & (1 << 12)) // left
    {
        result |= (1 << 7);
    }
    
    if(aw_input & (1 << 13)) // up
    {
        result |= (1 << 4);
    }

    if(aw_input & (1 << 14)) // right
    {
        result |= (1 << 5);
    }

    if(aw_input & (1 << 15)) // l
    {
        // this button is unused
    }

    // if(get_key_state(0) && get_key_press_event(5))
    // {
    //     if(audio_shift_bits > 1) audio_shift_bits--;
    // }
    // if(get_key_state(0) && get_key_press_event(8))
    // {
    //     if(audio_shift_bits < 16) audio_shift_bits++;
    // }

    // if(get_key_state(0) && get_key_press_event(7))
    // {
    //     selectedSlot--;
    //     if(selectedSlot < 0) selectedSlot = 9;
    //     rt_kprintf("State slot: %d\n",selectedSlot);
    //     state_setslot(selectedSlot);
    // }

    // if(get_key_state(0) && get_key_press_event(6))
    // {
    //     selectedSlot++;
    //     if(selectedSlot > 9) selectedSlot = 0;
    //     rt_kprintf("State slot: %d\n",selectedSlot);
    //     state_setslot(selectedSlot);
    // }

    // if(get_key_state(0)&& get_key_press_event(4))
    // {
    //     state_save();
    // }

    // if(get_key_state(0)&& get_key_press_event(3))
    // {
    //     state_load();
    // }

    // if(get_key_state(0)&& get_key_press_event(2))
    // {
    //     trigger_quit();
    // }

    // if(get_key_state(0))
    // {
    //     result |= (1 << 0); // select
    // }

    // // if(get_key_state(1))
    // // {
    // //     result |= (1 << 3); // menu
    // // }

    // if(get_key_state(2))
    // {
    //     result |= (1 << 3); // start
    // }

    // if(get_key_state(3))
    // {
    //     result |= (1 << 14); // b
    // }

    // if(get_key_state(4))
    // {
    //     result |= (1 << 13); // a
    // }

    // if(get_key_state(5))
    // {
    //     result |= (1 << 4); // up
    // }

    // if(get_key_state(6))
    // {
    //     result |= (1 << 5); // right
    // }

    // if(get_key_state(7))
    // {
    //     result |= (1 << 7); // left
    // }

    // if(get_key_state(8))
    // {
    //     result |= (1 << 6); // down
    // }
    
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

    return 0;
}

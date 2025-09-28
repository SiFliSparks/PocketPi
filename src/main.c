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


const char *framebuffer = (const char *)PSRAM_BASE+0x400000;
rt_device_t lcd_device;
rt_event_t g_tx_ev;
rt_device_t audprc_dev;
rt_device_t audcodec_dev;
const char supported_extensions[][4] = {
    "nes",
    "NES"
};

static rt_err_t speaker_tx_done(rt_device_t dev, void *buffer)
{
    //此函数在发送一帧完成的dma中断里，表示发送一次完成
    rt_event_send(g_tx_ev, 1);
    return RT_EOK;
}
#define DMA_BUF_SIZE    (2048*2)

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

lv_obj_t * cont;
void emu_thread_entry(void *parameter)
{
    char* filename = parameter;//"mario.nes";

    char* args[1] = { filename };

    nofrendo_main(1,args);
}

rt_thread_t nes_thread = RT_NULL;
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
        (void(*)(void*))emu_thread_entry, file_name,
        8192, 21, 100);
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
    lv_obj_set_size(cont, 390, 450);
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
        lv_obj_set_width(btn, lv_pct(60));

        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text_fmt(label, "%s", d->d_name);
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
    if (ret != RT_EOK)
    {
        return ret;
    }

    // 初始化音频
    g_tx_ev = rt_event_create("audio_tx_evt", RT_IPC_FLAG_FIFO);
    //1. 打开设备
    // int err;
    audprc_dev = rt_device_find("audprc");
    audcodec_dev = rt_device_find("audcodec");
    RT_ASSERT(audprc_dev && audcodec_dev);

    err = rt_device_open(audprc_dev, RT_DEVICE_FLAG_RDWR);
    RT_ASSERT(RT_EOK == err);
    err = rt_device_open(audcodec_dev, RT_DEVICE_FLAG_WRONLY);
    RT_ASSERT(RT_EOK == err);

    //2. 设置发送完成的回到函数
    rt_device_set_tx_complete(audprc_dev, speaker_tx_done);
    
    //3. 设置一次DMA buffer的大小，底层驱动里会使用2个这样的DMA buffer做ping/pong buffer
    rt_device_control(audprc_dev, AUDIO_CTL_SET_TX_DMA_SIZE, (void *)DMA_BUF_SIZE);

    //4. 音频输出到CODEC, 如果到I2S，可以设置AUDPRC_TX_TO_I2S
    rt_device_control(audcodec_dev, AUDIO_CTL_SETOUTPUT, (void *)AUDPRC_TX_TO_CODEC);
    rt_device_control(audprc_dev,   AUDIO_CTL_SETOUTPUT, (void *)AUDPRC_TX_TO_CODEC);
    
    //5. 设置codec参数
    struct rt_audio_caps caps;

    caps.main_type = AUDIO_TYPE_OUTPUT;
    caps.sub_type = (1 << HAL_AUDCODEC_DAC_CH0);
#if BSP_ENABLE_DAC2
    caps.sub_type |= (1 << HAL_AUDCODEC_DAC_CH1);
#endif
    caps.udata.config.channels   = 1; //最后的输出为一个声道
    caps.udata.config.samplerate = 32000; //采样率, 8000/11025/12000/16000/22050/24000/32000/44100/48000
    caps.udata.config.samplefmt = 16; //位深度8 16 24 or 32
    rt_device_control(audcodec_dev, AUDIO_CTL_CONFIGURE, &caps);
    
    struct rt_audio_sr_convert cfg;
    cfg.channel = 1; //源数据的通道个数，如果是2，则数据传入的格式位LRLRLR....的interleave格式
    cfg.source_sr = 32000; //源数据的采样率
    cfg.dest_sr = 32000;   //播放时的采样率  
    rt_device_control(audprc_dev, AUDIO_CTL_OUTPUTSRC, (void *)(&cfg));

    //数据选择，一路源数据就这样配置就行了，多路源数据的处理参考《音频通路mix&mux功能说明.docx》
    caps.main_type = AUDIO_TYPE_SELECTOR;
    caps.sub_type = 0xFF;
    caps.udata.value = 0x5050;
    rt_device_control(audprc_dev, AUDIO_CTL_CONFIGURE, &caps);
    caps.main_type = AUDIO_TYPE_MIXER;
    caps.sub_type = 0xFF;
    caps.udata.value   = 0x5050;
    rt_device_control(audprc_dev, AUDIO_CTL_CONFIGURE, &caps);

    //源数据格式说明
    caps.main_type = AUDIO_TYPE_OUTPUT;
    caps.sub_type = HAL_AUDPRC_TX_CH0;
    caps.udata.config.channels   = 1;
    caps.udata.config.samplerate = 32000;
    caps.udata.config.samplefmt = 16;
    rt_device_control(audprc_dev, AUDIO_CTL_CONFIGURE, &caps);

    //从EQ配置表中获得音量并设置到codec，vol_level为0 ~ 15
    // uint8_t vol_level = 1;
    // int volumex2 = eq_get_music_volumex2(vol_level);
    // if (caps.udata.config.samplerate == 16000 || caps.udata.config.samplerate == 8000)
    //    volumex2 = eq_get_tel_volumex2(vol_level);
    // rt_device_control(audcodec_dev, AUDIO_CTL_SETVOLUME, (void *)vol_level);

    //开始播放
    int stream = AUDIO_STREAM_REPLAY | ((1 << HAL_AUDPRC_TX_CH0) << 8);
    rt_device_control(audprc_dev, AUDIO_CTL_START, (void *)&stream);
    stream = AUDIO_STREAM_REPLAY | ((1 << HAL_AUDCODEC_DAC_CH0) << 8);
    rt_device_control(audcodec_dev, AUDIO_CTL_START, &stream);
    rt_event_send(g_tx_ev, 1);
    // 初始化音频完成

    list_init();
    /* Infinite loop */
    while (1)
    {
        if(nes_thread == RT_NULL)
        {
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


#include "audio.h"

rt_event_t g_tx_ev;
rt_device_t audprc_dev;
rt_device_t audcodec_dev;
audio_ring_buffer_t audio_ring_buffer = {
    .write_p = 0,
    .read_p = 0
};
extern int audio_shift_bits;
static rt_thread_t audio_thread = RT_NULL;
extern void (*audio_callback)(void *buffer, int length);

static rt_err_t speaker_tx_done(rt_device_t dev, void *buffer)
{
    //此函数在发送一帧完成的dma中断里，表示发送一次完成
    rt_event_send(g_tx_ev, 1);
    return RT_EOK;
}

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

void audio_ring_buffer_put(int16_t *samples, int length)
{
    for (int i = 0; i < length; i++)
    {
        audio_ring_buffer.samples[audio_ring_buffer.write_p++] = samples[i];
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

void audio_set_volume(int vol)
{
    
}

// static void audio_thread_entry(void *parameter)
// {
//     int16_t audio_temp_buffer[DMA_BUF_SIZE];
//     while (1)
//     {
//         audio_ring_buffer_get(audio_temp_buffer, DMA_BUF_SIZE/2);
//         uint32_t evt = 0;
//         rt_event_recv(g_tx_ev, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &evt);
//         rt_device_write(audprc_dev, 0, audio_temp_buffer, DMA_BUF_SIZE);
//     }
// }

static void audio_thread_entry(void *parameter)
{
    int16_t audio_temp_buffer[DMA_BUF_SIZE];
    while (1)
    {
        if(audio_callback)
        {
            audio_callback(audio_temp_buffer, DMA_BUF_SIZE/2);
            for(int i=0;i<DMA_BUF_SIZE/2;i++)
            {
                audio_temp_buffer[i] = audio_temp_buffer[i] >> audio_shift_bits;
            }
        }
        else
        {
            memset(audio_temp_buffer, 0, sizeof(audio_temp_buffer));
        }
        uint32_t evt = 0;
        rt_event_recv(g_tx_ev, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &evt);
        rt_device_write(audprc_dev, 0, audio_temp_buffer, DMA_BUF_SIZE);
    }
}

rt_err_t audio_init()
{
    // 初始化音频
    g_tx_ev = rt_event_create("audio_tx_evt", RT_IPC_FLAG_FIFO);
    //1. 打开设备
    // int err;
    audprc_dev = rt_device_find("audprc");
    audcodec_dev = rt_device_find("audcodec");
    RT_ASSERT(audprc_dev && audcodec_dev);

    rt_err_t err;
    err = rt_device_open(audprc_dev, RT_DEVICE_FLAG_RDWR);
    RT_ASSERT(RT_EOK == err);
    err = rt_device_open(audcodec_dev, RT_DEVICE_FLAG_WRONLY);
    RT_ASSERT(RT_EOK == err);

    //2. 设置发送完成的回调函数
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
    caps.udata.config.channels   = AUDIO_CHANNEL_NUM; //最后的输出为一个声道
    caps.udata.config.samplerate = AUDIO_SAMPLE_RATE; //采样率, 8000/11025/12000/16000/22050/24000/32000/44100/48000
    caps.udata.config.samplefmt = AUDIO_SAMPLE_BITS; //位深度8 16 24 or 32
    rt_device_control(audcodec_dev, AUDIO_CTL_CONFIGURE, &caps);
    
    struct rt_audio_sr_convert cfg;
    cfg.channel = AUDIO_CHANNEL_NUM; //源数据的通道个数，如果是2，则数据传入的格式位LRLRLR....的interleave格式
    cfg.source_sr = AUDIO_SAMPLE_RATE; //源数据的采样率
    cfg.dest_sr = AUDIO_SAMPLE_RATE;   //播放时的采样率  
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
    caps.udata.config.channels   = AUDIO_CHANNEL_NUM;
    caps.udata.config.samplerate = AUDIO_SAMPLE_RATE;
    caps.udata.config.samplefmt = AUDIO_SAMPLE_BITS;
    rt_device_control(audprc_dev, AUDIO_CTL_CONFIGURE, &caps);

    //从EQ配置表中获得音量并设置到codec，vol_level为0 ~ 15
    int vol_level = 0;
    // int volumex2 = eq_get_music_volumex2(vol_level);
    // if (caps.udata.config.samplerate == 16000 || caps.udata.config.samplerate == 8000)
    //    volumex2 = eq_get_tel_volumex2(vol_level);
    rt_device_control(audcodec_dev, AUDIO_CTL_SETVOLUME, (void *)vol_level);

    //开始播放
    int stream = AUDIO_STREAM_REPLAY | ((1 << HAL_AUDPRC_TX_CH0) << 8);
    rt_device_control(audprc_dev, AUDIO_CTL_START, (void *)&stream);
    stream = AUDIO_STREAM_REPLAY | ((1 << HAL_AUDCODEC_DAC_CH0) << 8);
    rt_device_control(audcodec_dev, AUDIO_CTL_START, &stream);
    rt_device_control(audcodec_dev, AUDIO_CTL_MUTE, (void *)0); //取消静音
    rt_event_send(g_tx_ev, 1);
    if (audio_thread == RT_NULL)
    {
        audio_thread = rt_thread_create("audio_thread",
            audio_thread_entry, RT_NULL,
            4096, 2, 10);
        if (audio_thread != RT_NULL) rt_thread_startup(audio_thread);
        else
            return -RT_ERROR;
    }
    return RT_EOK;
}
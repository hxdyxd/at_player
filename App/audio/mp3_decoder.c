
#include <string.h>

#include "kfifo.h"
#include "app_debug.h"
#include "cmsis_os.h"
#include "pwm_transfer.h"


//mad
#include "mad.h"
#include "os_malloc.h"

#include <httpcu.h>
#include <mp3_decoder.h>

int use_pwm = 1;

static volatile uint8_t player_mad_task_run_sflag = 0;
static volatile uint8_t player_mad_task_stop_sflag = 1;
static volatile uint8_t player_mad_async_stop_sflag = 1;
static char gs_http_mp3_run = 0;

#define READBUFSZ (2106)

//fifo
#define MP3_FIFO_SIZE    (512)
static struct __kfifo mp3_fifo;
static HTTPCU_T http_mp3;
/* Semaphore to signal incoming packets */
static osSemaphoreId smp3_semaphore = NULL;
static osThreadId player_mad_task_handle = NULL;
static uint32_t play_start_time = 0;


//Called by the NXP modifications of libmad. Sets the needed output sample rate.
static int oldRate = 0;
void set_dac_sample_rate(int rate, int chls)
{
    if (rate == oldRate) return;
    oldRate = rate;
    APP_DEBUG("MAD: Rate %d, channels %d \r\n", rate, chls);
    play_start_time = TIME_COUNT();
//    if(use_i2s)
//        i2s_device_set_rate(rate, chls);
    if(use_pwm) 
        pwm_device_set_rate(rate, chls);
}


//static inline signed int scale(mad_fixed_t sample)
//{
//    /* round */
//    sample += (1L << (MAD_F_FRACBITS - 16));

//    /* clip */
//    if (sample >= MAD_F_ONE)
//        sample = MAD_F_ONE - 1;
//    else if (sample < -MAD_F_ONE)
//        sample = -MAD_F_ONE;

//    /* quantize */
//    return sample >> (MAD_F_FRACBITS + 1 - 16);
//}


static inline signed short scale8(mad_fixed_t sample)
{
    /* round */
    sample += (1L << (MAD_F_FRACBITS - 8));

    /* clip */
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    /* quantize */
    return sample >> (MAD_F_FRACBITS + 1 - 8);
}


enum mad_flow output(void *data, struct mad_header const *header, struct mad_pcm *pcm)
{
    unsigned int nchannels, nsamples;
    mad_fixed_t const *left_ch, *right_ch;

    /* pcm->samplerate contains the sampling frequency */

    nchannels = pcm->channels;
    nsamples  = pcm->length;
    left_ch   = pcm->samples[0];
    right_ch  = pcm->samples[1];
    (void)right_ch;
    (void)nchannels;

    //APP_DEBUG("nchannels:%d  nsamples:%d\r\n", nchannels, nsamples);
    set_dac_sample_rate(pcm->samplerate, pcm->channels);
    
//    if(use_i2s) {
//        while (nsamples--) {
//            signed int sample[2];
//            
//            /* output sample(s) in 16-bit signed little-endian PCM */
//            
//            sample[0] = scale(*left_ch++);
//            if (nchannels == 2) {
//                sample[1] = scale(*right_ch++);
//            } else {
//                sample[1] = sample[0];
//            }
//            //printf("%d ", sample[0]);
//            
//            int w_len_count = 0;
//            while(w_len_count < 2) {
//                int w_len = __kfifo_in(&i2s_fifo, sample + w_len_count, 2 - w_len_count);
//                if(w_len == 0) {
//                    osDelay(1);
//                }
//                
//                w_len_count += w_len;
//            }
//        }
//    
//    }
    
    if(use_pwm) {
        left_ch   = pcm->samples[0];
        while (nsamples--) {
            signed short sample;
            unsigned short sample16;
            
            /* output sample(s) in 16-bit signed little-endian PCM */
            
            sample = scale8(*left_ch++);
            //printf("%d ", sample[0]);
            

            //sample = (((sample) + 32768) >> 8) & 0xff;
            sample16 = (sample + 128) & 0xff;
            
            while(1) {
                int w_len = __kfifo_in(&pwm_fifo, &sample16, 1);
                if(w_len == 1) {
                    break;
                }
                osDelay(5);
            }
        }
    
    }

    return MAD_FLOW_CONTINUE;
}


//Routine to print out an error
static enum mad_flow error(void *data, struct mad_stream *stream, struct mad_frame *frame)
{
    APP_ERROR("MAD: Dec err 0x%04x (%s) \r\n", stream->error, mad_stream_errorstr(stream));
    return MAD_FLOW_CONTINUE;
}


__weak int mp3_running_callback(void)
{
    return 0;
}


enum mad_flow input(void *user_data, struct mad_stream *stream)
{
    int rem;//, fifoLen;
    //Shift remaining contents of buf to the front
    rem = stream->bufend - stream->next_frame;
    memmove(user_data, stream->next_frame, rem);
    
    while (rem < READBUFSZ) {

//        int ret;
//        if((ret = f_read(&mad_fp, readBuf + rem, READBUFSZ - rem, &bw)) != FR_OK) {
//            APP_ERROR("f_read: %d \r\n", ret);
//            return MAD_FLOW_STOP;
//        }
//        if(bw == 0) {
//            return MAD_FLOW_STOP;
//        }
        
        //网络超时？播放器未运行？
        if( (!gs_http_mp3_run) || (!player_mad_task_run_sflag)) {
            return MAD_FLOW_STOP;
        }
        
        int rx_len;
        if( (rx_len = __kfifo_out(&mp3_fifo, (char *)user_data + rem, READBUFSZ - rem)) == 0) {
            //缓存不够了，释放信号量，通知串口线程继续填充数据
            osSemaphoreRelease(smp3_semaphore);
            osDelay(1);
        }
        
        rem += rx_len;
        //printf("rem: %d \n", stream->bufend - stream->next_frame);
    }
    //Okay, let MAD decode the buffer.
    mad_stream_buffer(stream, user_data, READBUFSZ);
    return MAD_FLOW_CONTINUE;
}


enum mad_flow header_func(void *user_data, struct mad_header const *header)
{
    PRINTF("samplerate: %d channel mode: %d\n", header->samplerate, header->mode);
    return MAD_FLOW_CONTINUE;
}


void player_mad_task_proc(void const *par)
{
//    int ret;
    struct mad_decoder *decoder = NULL;
    unsigned char *readBuf = NULL;
    player_mad_task_stop_sflag = 0;
    player_mad_task_run_sflag = 1;
    oldRate = 0;
    
//    if((ret = f_open(&mad_fp, par, FA_READ)) != FR_OK) {
//        APP_ERROR("f_open: %d \r\n", ret);
//        goto exit;
//    }
    
    if(__kfifo_alloc(&mp3_fifo, MP3_FIFO_SIZE, 1, 0) < 0) {
        APP_ERROR("MP3 kfifo alloc error \r\n");
        goto exit0;
    }
    
    decoder = malloc(sizeof(struct mad_decoder));
    if(decoder == NULL) {
        APP_ERROR("mem alloc failed \r\n");
        goto exit1;
    }
    
    readBuf = malloc(READBUFSZ);
    if(readBuf == NULL) {
        APP_ERROR("mem alloc failed \r\n");
        goto exit2;
    }
    
    mad_decoder_init(decoder, readBuf, input, 0,
                 /*filter*/ 0, output, error, /* message */ 0);
    mad_decoder_run(decoder, MAD_DECODER_MODE_SYNC);
    mad_decoder_finish(decoder);
    
    free(readBuf);
exit2:
    free(decoder);
exit1:
//    f_close(&mad_fp);
    __kfifo_free(&mp3_fifo);
exit0:
    APP_DEBUG("MAD: Wait I2S transfer finally \r\n");
    
//    if(use_i2s) {
//        while(i2s_fifo.in - i2s_fifo.out > 0) {
//            vTaskDelay(100);
//        }
//    }
    if(use_pwm) {
        while(pwm_fifo.in - pwm_fifo.out > 0) {
            vTaskDelay(100);
        }
    }
    
    APP_WARN("");
    PRINTF(RED_FONT, "MAD: player mad task exit! ");
    PRINTF("\r\n");
    player_mad_task_run_sflag = 0;
    player_mad_task_stop_sflag = 1;
    vTaskDelete(NULL);
}

int mp3_fifo_in(void *buf, int len)
{
    if(mp3_fifo.data == NULL)
        return 0;
    return  __kfifo_in(&mp3_fifo, buf, len);
}

/**
  *  @note: callback from uart_wifi_task
  **/
void httpcu_data_callback(void *httpc, void *buf, int len)
{
    uint8_t *pbuf = buf;
    if( !gs_http_mp3_run) {
        return;
    }
    int w_len_count = 0;
    while(w_len_count < len && player_mad_task_run_sflag) {
        int w_len = mp3_fifo_in(pbuf + w_len_count, len - w_len_count);
        if(w_len == 0) {
             if (osSemaphoreWait(smp3_semaphore, portMAX_DELAY) == osOK) {
                 
             }
        }
        
        w_len_count += w_len;
    }
}


void mad_network_player_start(const void *config)
{
    MP3_DECODER_T *conf = (MP3_DECODER_T *)config;
    unsigned char *httpcu_pbuf = NULL;
    int ret = -1;
    osSemaphoreDef(SEM2);
    smp3_semaphore = osSemaphoreCreate(osSemaphore(SEM2) , 1 );
    
    gs_http_mp3_run = 1;
    
    osThreadDef(play_decoder, player_mad_task_proc, osPriorityAboveNormal, 0, 2304);
    player_mad_task_handle = osThreadCreate(osThread(play_decoder), NULL);
    if(player_mad_task_handle == NULL) {
        APP_ERROR("task create failed \r\n");
        goto exit;
    }
    player_mad_task_run_sflag = 1;
    player_mad_task_stop_sflag = 0;
    
    
    httpcu_pbuf = malloc(HTTPCU_BUF_MAX_LEN);
    if(httpcu_pbuf == NULL) {
        APP_ERROR("mem alloc failed \r\n");
        goto exit;
    }
    
    httpcu_init(&http_mp3, 1, conf->host, conf->port, conf->uri);
    httpcu_set_callback(&http_mp3, httpcu_data_callback);
    httpcu_get(&http_mp3, httpcu_pbuf, HTTPCU_BUF_MAX_LEN);
    gs_http_mp3_run = 0;
    
    while(!player_mad_task_stop_sflag) {
        osDelay(500);
        APP_DEBUG("wait decoder task exit ...\r\n");
    }
    
    ret = 0;
    free(httpcu_pbuf);
exit:
    APP_DEBUG("mp3 play end \r\n");
    //return ret;
    player_mad_async_stop_sflag = 1;
    vTaskDelete(NULL);
}


int mad_network_player_start_async(MP3_DECODER_T *config)
{
    if(!config) {
        return -1;
    }
    player_mad_async_stop_sflag = 0;
    osThreadDef(play_network, mad_network_player_start, osPriorityAboveNormal, 0, 256);
    osThreadId player_async_task_handle = osThreadCreate(osThread(play_network), config);
    if(player_mad_task_handle == NULL) {
        APP_ERROR("task create failed \r\n");
        return -1;
    }
    return 0;
}

unsigned int mad_network_player_status(void)
{
    return !player_mad_async_stop_sflag;
}


int mad_network_player_pause(void)
{
    gs_http_mp3_run = 0;
    return 0;
}

uint32_t mad_network_player_get_play_time(void)
{
    return (TIME_COUNT() - play_start_time);
}


/*****************************END OF FILE***************************/

/* 2019 04 10 */
/* By hxdyxd */

#include <stdlib.h>
#include <string.h>
#include <app_debug.h>
#include <user_main.h>

#include <cmsis_os.h>
#include <os_malloc.h>

#include <uart_receiver.h>
#include <pwm_transfer.h>
#include <cJSON.h>

#include <esp8266.h>
#include <esp8266_receiver.h>
#include <httpcu.h>
#include <mp3_decoder.h>
#include <bds_stt.h>

#include <tim.h>

osThreadId main_task_proc_handle;



uint8_t sendbuf[2048*3];
char lrcbuf[256];
HTTPCU_T httpcu_test;


char *getline(char *buf, int count, int *olen)
{
    char *last = buf;
    char *next = last;
    while(count--) {
        next = strstr(last, "\n");
        if(next == NULL) {
            return NULL;
        }
        last = next + 1;
    }
    next = strstr(last, "\n");
    if(next == NULL) {
        return NULL;
    }
    *olen = next - last;
    return last;
}


void uart_cmd_callback(char ch)
{
    uint8_t *listbuf = NULL;
    switch(ch) {
    case 'l':
    {
        listbuf = os_malloc(512);
        if(listbuf) {
            osThreadList(listbuf);
            PRINTF("-----------------------------\r\n");
            PRINTF("%s", listbuf);
            PRINTF("-----------------------------\r\n");
            vTaskGetRunTimeStats((char *)listbuf);
            PRINTF("%s\r\n", listbuf);
            PRINTF("------------%08x-------------\r\n", __HAL_TIM_GET_COUNTER(&htim2) );
            os_free(listbuf);
        }
        break;
    }
    case 'm':
    {
        size_t sz = xPortGetMinimumEverFreeHeapSize();
        PRINTF("MinimumEverFreeHeapSize = %d\r\n", sz);
        break;
    }
    case 'b':
    {
        bds_stt_test();
        break;
    }
    default:
        putchar(ch);
    }
}


void main_task_proc(void const *p)
{
    
    while(esp8266_init_asnyc() < 0) {
        osDelay(1000);
    }
    
    while(1) {
        osDelay(500);
        if(!get_wifi_status() ) {
            APP_WARN("Waiting for connection establishment\r\n");
            continue;
        }
        
        printf("---------------------start---------------------\r\n");
        esp8266_get_status();
        
        httpcu_init(&httpcu_test, 0, "ip.sococos.com", 8080, "/playerdayhot.php");
        int glen = httpcu_get(&httpcu_test, sendbuf, sizeof(sendbuf));
        if(glen > 0 && glen != sizeof(sendbuf)) {
            sendbuf[glen] = '\0';
            PRINTF("%s\r\n", sendbuf);
            
            cJSON *ocJson = cJSON_Parse( (char *)sendbuf);
            if(ocJson == NULL) {
                APP_ERROR("JSON Parse failed \r\n");
                continue;
            }
                    
            for(int i = 0;i<cJSON_GetArraySize(ocJson);i++) {
                cJSON *ocJson_msg = cJSON_GetArrayItem(ocJson, i);
                cJSON *ocJson_host = cJSON_GetObjectItem(ocJson_msg, "songHostName");
                cJSON *ocJson_path = cJSON_GetObjectItem(ocJson_msg, "songPath");
                cJSON *ocJson_lrchost = cJSON_GetObjectItem(ocJson_msg, "lrcHostName");
                cJSON *ocJson_lrcpath = cJSON_GetObjectItem(ocJson_msg, "lrcPath");
                
                PRINTF(GREEN_FONT, "----------------Audio Player Message----------------");
                PRINTF("\r\n");
                APP_DEBUG("Host: %s\r\n", ocJson_host->valuestring );
                APP_DEBUG("Path: %s\r\n", ocJson_path->valuestring );
                APP_DEBUG("artistName: %s\r\n", cJSON_GetObjectItem(ocJson_msg, "artistName")->valuestring );
                APP_DEBUG("songName: %s\r\n", cJSON_GetObjectItem(ocJson_msg, "songName")->valuestring );
                if(ocJson_lrchost && ocJson_lrcpath) {
                     APP_DEBUG("lrcHost: %s\r\n", ocJson_lrchost->valuestring );
                    APP_DEBUG("lrcPath: %s\r\n", ocJson_lrcpath->valuestring );
                }
                PRINTF(GREEN_FONT, "----------------Audio Player Message----------------");
                PRINTF("\r\n");
                
                MP3_DECODER_T mp3_conf;
                mp3_conf.host = ocJson_host->valuestring;
                mp3_conf.port = 80;
                mp3_conf.uri = ocJson_path->valuestring;
                
                if(mad_network_player_start_async(&mp3_conf) < 0) {
                    continue;
                }
                
                char loadlrc = 0;
                if(ocJson_lrchost && ocJson_lrcpath) {
                    httpcu_init(&httpcu_test, 0, ocJson_lrchost->valuestring, 80, ocJson_lrcpath->valuestring);
                    int jlen = httpcu_get(&httpcu_test, sendbuf, sizeof(sendbuf));
                    if(jlen > 0 && jlen != sizeof(sendbuf)) {
                        sendbuf[jlen] = '\0';
                        //PRINTF("%s", sendbuf);
                        loadlrc = 1;
                    }
                }
                
                APP_DEBUG("playing ...\r\n");
                int k = 0;
                while( mad_network_player_status() ) {
                    osDelay(300);
                    if(loadlrc) {
                        int rlen = 0;
                        char *line = getline( (char *)sendbuf, k++, &rlen);
                        if(line == NULL || rlen >= sizeof(lrcbuf) - 1) {
                            continue;
                        }
                        memcpy(lrcbuf, line, rlen);
                        lrcbuf[rlen] = '\0';
                        
                        int min, sec, msec = 0;
                        int cnt = sscanf(lrcbuf, "[%02d:%02d.%02d]", &min, &sec, &msec);
                        
                        if(cnt == 3) {
                            char *lrcstr = lrcbuf + 10;
                            msec = (min*60 + sec)*1000 + msec*10;
                            
                            if(mad_network_player_get_play_time() >= msec) {
                                PRINTF("%d %s\r\n", msec, lrcstr);
                            } else {
                                --k;
                            }
                        }
                    }
                }
                
            }
            
            cJSON_Delete(ocJson);
        }
                
        APP_DEBUG("mp3 play end \r\n");
        printf("----------------------end----------------------\r\n");
    }
    //vTaskDelete(NULL);
}



void user_os_setup(void)
{
    debug_init();
    printf("\r\n\r\n[OS STM32F401CC] Build , %s %s \r\n", __DATE__, __TIME__);
    
    os_malloc_init();
    
    uart_receiver_init();
    pwm_transfer_init();
    
    //__HAL_TIM_SET_COUNTER(&htim2, 0);
    HAL_TIM_Base_Start(&htim2);
    
    osThreadDef(main_task, main_task_proc, osPriorityNormal, 0, 512);
    main_task_proc_handle = osThreadCreate(osThread(main_task), NULL);
}


void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
    APP_ERROR("%p %s\r\n", xTask, pcTaskName);
    while(1);
}

void configureTimerForRunTimeStats(void)
{
}

unsigned long getRunTimeCounterValue(void)
{
    return __HAL_TIM_GET_COUNTER(&htim2);
}


/* nonos */
void user_setup(void)
{
}

void user_loop(void)
{
}


/*****************************END OF FILE***************************/

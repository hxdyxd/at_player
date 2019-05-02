/* 2019 04 10 */
/* By hxdyxd */

#include <stdlib.h>
#include <string.h>
#include <app_debug.h>
#include <user_main.h>

#include <cmsis_os.h>

#include <uart_receiver.h>
#include <pwm_transfer.h>
#include <cJSON.h>

#include <esp8266.h>
#include <esp8266_receiver.h>
#include <httpcu.h>
#include <mp3_decoder.h>


osThreadId main_task_proc_handle;



uint8_t sendbuf[2048];
HTTPCU_T httpcu_test;

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
                
                mad_netword_player_start(ocJson_host->valuestring, 80, ocJson_path->valuestring);
            }
            
            cJSON_Delete(ocJson);
        }
                
//        osThreadList(sendbuf);
//        printf("%s", sendbuf);
        APP_DEBUG("mp3 play end \r\n");
        printf("----------------------end----------------------\r\n");
    }
    //vTaskDelete(NULL);
}



void user_os_setup(void)
{
    debug_init();
    printf("\r\n\r\n[OS STM32F401CC] Build , %s %s \r\n", __DATE__, __TIME__);
    
    uart_receiver_init();
    pwm_transfer_init();
    
    osThreadDef(main_task, main_task_proc, osPriorityNormal, 0, 512);
    main_task_proc_handle = osThreadCreate(osThread(main_task), NULL);
}


void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
    APP_ERROR("%p %s\r\n", xTask, pcTaskName);
    while(1);
}



/* nonos */
void user_setup(void)
{
}

void user_loop(void)
{
}


/*****************************END OF FILE***************************/

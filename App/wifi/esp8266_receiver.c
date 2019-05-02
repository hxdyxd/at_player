/* 2019 05 02 */
/* By hxdyxd */

#include <stdint.h>
#include <string.h>
#include <httpc.h>
#include <httpcu.h>
#include <user_main.h>
#include <app_debug.h>

static char g_wifi_status = 0;

void esp8266_user_data_callback(int id, uint8_t *buf, int len)
{
    switch(id) {
    case 0:
        //httpc_process(&http_mp3, buf, len);
        //break;
    case 1:
    {
        HTTPC_T *httpc = http_get_handle(id);
        if(httpc) {
            httpc_process(httpc, buf, len);
        }
        break;
    }
    default:
        ;
    }

    LED_REV(LED_BASE);
}


void esp8266_urc_callback(void *buf, int len)
{
    APP_DEBUG("(%d) %s \r\n", len, (char *)buf);
    if(strcmp(buf, "ready") == 0) {
        
    } else if(strcmp(buf,  "WIFI CONNECTED") == 0) {
        g_wifi_status = 1;
    } else if(strcmp(buf,  "WIFI DISCONNECT") == 0) {
        g_wifi_status = 0;
    }
}

int get_wifi_status(void)
{
    return g_wifi_status;
}


/*****************************END OF FILE***************************/

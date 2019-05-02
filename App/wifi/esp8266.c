/* 2019 04 20 */
/* By hxdyxd */

#include <stdlib.h>
#include <string.h>
#include "esp8266.h"
#include "cmsis_os.h"
#include "usart.h"
#include "app_debug.h"
#include "uart_fifo_rx.h"
#include "user_main.h"

#define ESP8266_DEBUG  APP_DEBUG
#define ESP8266_WARN  APP_WARN
#define ESP8266_ERROR  APP_ERROR


#define esp8266_printf(...) do {\
    PRINTF("(w)#");\
    PRINTF(__VA_ARGS__);\
    int len = snprintf( (void *)at_buf, AT_BUFFER_SIZE, __VA_ARGS__);\
    HAL_UART_Transmit(&huart6, (void *)at_buf, len, 2*len); }while(0)


#define ENSEE  do{\
}while(0)

#define L(...)  (char *[]){__VA_ARGS__}, (sizeof((char *[]){__VA_ARGS__})/sizeof(char *))

#define AT_BUFFER_SIZE   (512)


typedef struct {
    char *at_word;
    signed char flag;
    int jumpto;
    char *rx_buf;
}AT_PARA_T;


AT_PARA_T g_at_list[] = {
    {
        .at_word = "OK",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "ERROR",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "FAIL",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "DNS Fail",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "ALREADY CONNECTED",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "SEND OK",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "SEND FAIL",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "busy s...",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "busy p...",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = ">",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    {
        .at_word = "no ip",
        .flag = FLAG_DETECTED,
        .jumpto = S_NOT_DETECT,
    },
    
    //urc
    {
        .at_word = "+IPD",
        .flag = FLAG_ALWAYS,
        .jumpto = S_IPD_DETECTED,
        .rx_buf = NULL,
    },
    {
        .at_word = "WIFI CONNECTED",
        .flag = FLAG_ALWAYS,
        .jumpto = S_URC_INFO,
    },
    {
        .at_word = "WIFI DISCONNECT",
        .flag = FLAG_ALWAYS,
        .jumpto = S_URC_INFO,
    },
    {
        .at_word = "ready",
        .flag = FLAG_ALWAYS,
        .jumpto = S_URC_INFO,
    },
    
    //user data
    {
        .at_word = "STATUS:",
        .flag = FLAG_DETECTED,
        .jumpto = S_GET_INFO,
        .rx_buf = NULL,
    },
    {
        .at_word = "+CWJAP:",
        .flag = FLAG_DETECTED,
        .jumpto = S_GET_INFO,
        .rx_buf = NULL,
    },
    {
        .at_word = "+CIPDOMAIN:",
        .flag = FLAG_DETECTED,
        .jumpto = S_GET_INFO,
        .rx_buf = NULL,
    },
    {
        .at_word = "+CIPSNTPTIME:",
        .flag = FLAG_DETECTED,
        .jumpto = S_GET_INFO,
        .rx_buf = NULL,
    },
};
int g_at_list_num = sizeof(g_at_list)/sizeof(AT_PARA_T);


osMutexId esp8266_mutex;
uint8_t at_buf[AT_BUFFER_SIZE];
int at_failed = 0;
osSemaphoreId sReady_xSemaphore = NULL;

static void usart6_set_baudrate(int baudrate)
{
    huart6.Init.BaudRate = baudrate;
    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        Error_Handler();
    }
}


static int reverse_memcmp(const void *end1, const void *end2, size_t n)
{
    const unsigned char *a = end1, *b = end2;
    for (; n; --n)
        if (*--a != *--b) return *a - *b;
    return 0;
}

static int php_strpos(void *buf, int buflen, void *subbuf, int subbuflen, int start)
{
    int i;
    if(buflen < subbuflen || subbuf == NULL)
        return -1;
    for(i = start ; i < buflen - subbuflen; i++) {
        if(memcmp( (uint8_t *)buf + i, subbuf, subbuflen) == 0) {
            return i;
        }
    }
    return -1;
}



static int esp8266_try_async(char *cmd, char *res[], char len, int try_c, int timeout)
{
    static signed char at_list_item_id[10];
    int try_count = try_c;
    int ok = -1;
    if(len <= 0) {
        return -1;
    }
    
    while(ok < 0) {
        if(try_count == 0) {
            APP_WARN("esp8266_try timeout : %s\r\n", cmd?cmd:"empty\0");
            at_failed ++;
            break;
        }
        try_count--;
        
        
        //set allow detect flag
        for(int i=0; i<len; i++) {
            at_list_item_id[i] = -1;
            for(int j=0; j<g_at_list_num; j++) {
                //AT COMMAND FIND
                if( !strcmp(res[i], g_at_list[j].at_word) ) {
                    g_at_list[j].jumpto = S_NOT_DETECT;
                    g_at_list[j].flag = FLAG_CLEAR;
                    at_list_item_id[i] = j;
                    //printf("allow detect %s\r\n", res[i]);
                    break;
                }
            }
            if(at_list_item_id[i] < 0) {
                PRINTF("can't find %s \r\n", res[i]);
                return -1;
            }
        }
        
        if(cmd) {
            esp8266_printf("%s", cmd);
        }
        

        uint32_t lst_timer = TIME_COUNT();
        while(TIME_COUNT() - lst_timer < timeout && ok < 0) {
            for(int i=0; i<len; i++) {
                if(at_list_item_id[i] < 0)
                    return -1;
                if(g_at_list[at_list_item_id[i]].flag == FLAG_DETECTED) {
                    PRINTF("detected %s\r\n", g_at_list[at_list_item_id[i]].at_word);
                    //recovery
                    for(int j=0; j<len; j++) {
                        if(g_at_list[at_list_item_id[j]].flag == FLAG_CLEAR) {
                            g_at_list[at_list_item_id[j]].flag = FLAG_DETECTED;
                        }
                    }
                    return i;
                }
            }
            osDelay(2);
        }
        
        if(ok < 0) {
            osDelay(200);
        }
    }
    
    //recovery
    for(int j=0; j<len; j++) {
        if(g_at_list[at_list_item_id[j]].flag == FLAG_CLEAR) {
            g_at_list[at_list_item_id[j]].flag = FLAG_DETECTED;
        }
    }
    
    return ok;
}

static int esp8266_get_message_async(char *cmd, char *res[], char len, char *rx_buf, int timeout)
{
    static signed char at_list_item_id[10];
    int ok = -1;
    if(rx_buf == NULL || len <= 0) {
        return -1;
    }
    
    //set allow detect flag
    for(int i=0; i<len; i++) {
        at_list_item_id[i] = -1;
        for(int j=0; j<g_at_list_num; j++) {
            //AT COMMAND FIND
            if( !strcmp(res[i], g_at_list[j].at_word) ) {
                g_at_list[j].rx_buf = rx_buf;
                g_at_list[j].flag = FLAG_CLEAR;
                at_list_item_id[i] = j;
                //printf("allow detect %s\r\n", res[i]);
                break;
            }
        }
        if(at_list_item_id[i] < 0) {
            PRINTF("can't find %s \r\n", res[i]);
            return -1;
        }
    }
    
    if(cmd) {
        esp8266_printf("%s", cmd);
    }
    
    //detect res
    uint32_t lst_timer = TIME_COUNT();
    while(TIME_COUNT() - lst_timer < timeout && ok < 0) {
        for(int i=0; i<len; i++) {
            if(at_list_item_id[i] < 0)
                return -1;
            if(g_at_list[at_list_item_id[i]].flag == FLAG_DETECTED) {
                PRINTF("detected %s\r\n", g_at_list[at_list_item_id[i]].at_word);
                //recovery
                for(int j=0; j<len; j++) {
                    if(g_at_list[at_list_item_id[j]].flag == FLAG_CLEAR) {
                        g_at_list[at_list_item_id[j]].flag = FLAG_DETECTED;
                    }
                }
                return i;
            }
        }
        osDelay(5);
    }
    
    //recovery
    for(int j=0; j<len; j++) {
        if(g_at_list[at_list_item_id[j]].flag == FLAG_CLEAR) {
            g_at_list[at_list_item_id[j]].flag = FLAG_DETECTED;
        }
    }
    
    return ok;
}




int esp8266_init_asnyc(void)
{
    HAL_GPIO_WritePin(WIFI_RTS_GPIO_Port, WIFI_RTS_Pin, GPIO_PIN_RESET);
    osDelay(500);
    
    if(sReady_xSemaphore == NULL) {
        osSemaphoreDef(SEMr);
        sReady_xSemaphore = osSemaphoreCreate(osSemaphore(SEMr) , 1 );
    }
    
    LED_ON(ESP8266_BASE);
    osDelay(1000);
    
    LED_OFF(ESP8266_BASE);
    osSemaphoreWait( sReady_xSemaphore, TIME_WAITING_FOR_INPUT);
    osDelay(2000);
    
    at_failed = 0;
    APP_WARN("g_at_list_num = %d \r\n", g_at_list_num);
    
    if(esp8266_mutex == NULL) {
        osMutexDef(esp8266_mutex);
        esp8266_mutex = osMutexCreate(osMutex(esp8266_mutex));
        APP_DEBUG("create mutex for esp8266 success\r\n");
    }
    
    usart6_set_baudrate(115200);
    
    while(esp8266_try_async("AT\r\n", L("OK", "busy p..."), 3, 500) == 1) {
        APP_WARN("wait busy p \r\n");
        osDelay(500);
    }
    if(at_failed) {
        return -1;
    }
    
    snprintf((char *)at_buf, AT_BUFFER_SIZE, "AT+UART_CUR=%d,%d,%d,%d,%d\r\n", BANDRATE_MAX, 8, 1 /* 3: 2bit stop bits*/, 0, 2); //CTS ENABLE
    esp8266_try_async( (char *)at_buf, L("OK"), 1, 500);
    usart6_set_baudrate(BANDRATE_MAX);
    
    esp8266_try_async("AT\r\n", L("OK"), 3, 200);
    /* show module version */
    esp8266_try_async("AT+GMR\r\n", L("OK"), 1, 1000);
    /* disable echo */
    esp8266_try_async("ATE0\r\n",  L("OK"), 1, 200);
    //Set to Wi-Fi Station mode
    esp8266_try_async("AT+CWMODE_CUR=1\r\n", L("OK"), 1, 500);
    esp8266_try_async("AT\r\n", L("OK"), 3, 200);
    
    //Set hostname
    esp8266_set_hostname(WIFI_HOSTNAME);
    
    //enable dhcp
    esp8266_try_async("AT+CWDHCP_CUR=1,1\r\n", L("OK"), 1, 1000);
    
    //connect to ap
    esp8266_connect_ap(WIFI_SSID, WIFI_PASSWORD);
    
    while(esp8266_try_async("AT\r\n", L("OK", "busy p..."), 3, 200) == 1) {
        APP_WARN("wait busy p \r\n");
        osDelay(500);
    }
    
    esp8266_try_async("AT+CWJAP_CUR?\r\n", L("OK"), 1, 500);
    
    esp8266_try_async("AT+CIPSTATUS\r\n", L("OK"), 1, 500);
    esp8266_try_async("AT+CIPSTA_CUR?\r\n", L("OK"), 1, 500);
    esp8266_try_async("AT+CIPDNS_CUR?\r\n", L("OK"), 1, 500);
    
    esp8266_set_mux(1);
    
    esp8266_try_async("AT+CIPSSLSIZE=4096\r\n", L("OK"), 1, 1000);
    
    esp8266_enable_sntp(1, 8);
    
    char *ip = esp8266_dns_resolve("ss.sococos.com");
    if(ip)
        PRINTF("%s \r\n", ip);

    char *ip2 = esp8266_dns_resolve("ip.sococos.com");
    if(ip2)
        PRINTF("%s \r\n", ip2);
    
    //?
    //PRINTF("%d \r\n", esp8266_get_status());
    int stat = esp8266_get_status();
    PRINTF("%d \r\n", stat);
    
    return 0;
}




int esp8266_get_status(void)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    int status = esp8266_get_message_async("AT+CIPSTATUS\r\n", L("STATUS:"), (char *)at_buf, 1000);
    if(status == 0) {
        int sta = atoi( (char *)at_buf);
        osMutexRelease(esp8266_mutex);
        return sta;
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}


int esp8266_set_mux(int enable)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    snprintf( (char *)at_buf, AT_BUFFER_SIZE, "AT+CIPMUX=%d\r\n", enable);
    if(esp8266_try_async( (char *)at_buf, L("OK", "ERROR"), 1, 5000) == 0) {
        osMutexRelease(esp8266_mutex);
        return 0;
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}

int esp8266_set_hostname(char *name)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    snprintf( (char *)at_buf, AT_BUFFER_SIZE, "AT+CWHOSTNAME=\"%s\"\r\n", name);
    if(esp8266_try_async( (char *)at_buf, (char *[]){"OK", "ERROR"}, 2, 1, 4000) == 0) {
        osMutexRelease(esp8266_mutex);
        return 0;
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}

int esp8266_connect_ap(char *ssid, char *passwd)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    snprintf( (char *)at_buf, AT_BUFFER_SIZE, "AT+CWJAP_CUR=\"%s\",\"%s\"\r\n", ssid, passwd);
    int status = esp8266_get_message_async( (char *)at_buf, L("OK", "FAIL", "+CWJAP:"), (char *)at_buf, 20000);
    if(status  == 0) {
        osMutexRelease(esp8266_mutex);
        return 0;
    } else if(status == 2) {
        APP_DEBUG("error code %s \r\n", at_buf + 1);
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}



/*
 * 1:Passive mode
 * 0:Active mode
 */
int esp8266_set_passive_recv(int enable)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    snprintf( (char *)at_buf, AT_BUFFER_SIZE, "AT+CIPRECVMODE=%d\r\n", enable);
    if(esp8266_try_async( (char *)at_buf, L("OK", "ERROR"), 1, 5000) == 0) {
        osMutexRelease(esp8266_mutex);
        return 0;
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}


/* sntp */
int esp8266_enable_sntp(int enable, int timezone)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    snprintf( (char *)at_buf, AT_BUFFER_SIZE,
        "AT+CIPSNTPCFG=%d,%d,\"cn.ntp.org.cn\",\"ntp.sjtu.edu.cn\",\"us.pool.ntp.org\"\r\n", enable, timezone);
    if(esp8266_try_async( (char *)at_buf, L("OK", "ERROR"), 1, 5000) == 0) {
        osMutexRelease(esp8266_mutex);
        return 0;
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}

int esp8266_sntp_get(void *buf, int len)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    int status = esp8266_get_message_async("AT+CIPSNTPTIME?\r\n", L("+CIPSNTPTIME:"), (char *)at_buf, 5000);
    if(status == 0) {
        strncpy(buf, (char *)at_buf, len);
        osMutexRelease(esp8266_mutex);
        return 0;
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}



/* smartconfig */

void esp8266_start_smartconfig(void)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    //2：AirKiss
    esp8266_try_async("AT+CWSTARTSMART=2\r\n", L("OK"),  1, 200);
    osMutexRelease(esp8266_mutex);
}

void esp8266_stop_smartconfig(void)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    //Stop AirKiss
    esp8266_try_async("AT+CWSTOPSMART\r\n",L("OK"), 1, 200);
    osMutexRelease(esp8266_mutex);
}


/* dns resolve */

char *esp8266_dns_resolve(const char *hostname)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
//    int stat = esp8266_get_status();
//    if(stat < 0 || stat == 5) {
//        return NULL;
//    }
    
    
    static char ip[17];
    snprintf( (char *)at_buf, AT_BUFFER_SIZE, "AT+CIPDOMAIN=\"%s\"\r\n", hostname);
    int status = esp8266_get_message_async( (char *)at_buf, L("+CIPDOMAIN:", "DNS Fail", "ERROR"), (char *)at_buf, 20000);
    if(status == 0) {
        strncpy(ip, (char *)at_buf, 17);
        osMutexRelease(esp8266_mutex);
        return ip;
    }
    
    while(esp8266_try_async("AT\r\n", L("OK", "busy p..."), 1, 50) == 1) {
        ESP8266_WARN("wait dns resolve \r\n");
        osDelay(500);
    }
    osMutexRelease(esp8266_mutex);
    return NULL;
}


int esp8266_connect(int id, char *type, char *host, unsigned short port)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    int sta;
    int count = 0;
    while(esp8266_try_async("AT\r\n", L("OK", "busy p..."), 1, 500) == 1) {
        ESP8266_WARN("wait busy p \r\n");
        if(++count > 120) {
            esp8266_init_asnyc();
            count = 0;
        }
        osDelay(500);
    }
    snprintf( (char *)at_buf, AT_BUFFER_SIZE, "AT+CIPSTART=%d,\"%s\",\"%s\",%d\r\n", id, type, host, port);
    if( (sta = esp8266_try_async( (char *)at_buf, L("OK", "ALREADY CONNECTED", "ERROR", "no ip"), 1, 5000)) == 0) {
        osMutexRelease(esp8266_mutex);
        return 0;
    }
    if(sta == 1) {
        ESP8266_WARN("ALREADY CONNECTED \r\n");
        if(esp8266_close(id)) {
            osMutexRelease(esp8266_mutex);
            return esp8266_connect(id, type, host, port);
        }
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}

int esp8266_close(int id)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    snprintf( (char *)at_buf, AT_BUFFER_SIZE, "AT+CIPCLOSE=%d\r\n", id);
    if(esp8266_try_async( (char *)at_buf, L("OK", "ERROR"), 1, 3000) == 0) {
        osMutexRelease(esp8266_mutex);
        return 0;
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}


int esp8266_tcp_send(int id, void *buf, int length)
{
    if(length > 2048) {
        //too long
        return -1;
    }
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    snprintf( (char *)at_buf, AT_BUFFER_SIZE, " AT+CIPSEND=%d,%d\r\n", id, length);
    if(esp8266_try_async( (char *)at_buf, L(">", "ERROR"), 1, 5000) == 0) {
        HAL_UART_Transmit_DMA(&huart6, (void *)buf, length);
        if(esp8266_try_async(NULL, L("SEND OK", "SEND FAIL"), 1, 5000) == 0) {
            osMutexRelease(esp8266_mutex);
            return 0;
        }
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}


int esp8266_tcp_clen(void)
{
    osMutexWait(esp8266_mutex, TIME_WAITING_FOR_INPUT);
    
    if(esp8266_try_async( "AT+CIPRECVLEN?\r\n", L("OK", "ERROR"), 1, 5000) == 0) {
        osMutexRelease(esp8266_mutex);
        return 0;
    }
    osMutexRelease(esp8266_mutex);
    return -1;
}


static int mod_reverse_memcmp(const void *buf, unsigned int ptr, const void *end1, size_t n, const unsigned int mask)
{
    const unsigned char *a = buf, *b = end1;
    for (; n; --n) {
        ptr--;
        ptr &= mask;
        if (a[ptr] != *--b) return a[ptr] - *b;
    }
    return 0;
}


__weak void esp8266_urc_callback(void *buf, int len)
{
    /* NOTE: This function Should not be modified, when the callback is needed,
           the esp8266_urc_callback could be implemented in the user file
    */
}

__weak void esp8266_user_data_callback(int id, uint8_t *buf, int len)
{
    /* NOTE: This function Should not be modified, when the callback is needed,
           the esp8266_user_data_callback could be implemented in the user file
    */
}

static void esp8266_urc_process(void *buf, int len)
{
    if(strcpy(buf, "ready") == 0) {
        osSemaphoreRelease(sReady_xSemaphore);
    }
    esp8266_urc_callback(buf, len);
}


#define MAX_FRAME_SIZE       (4096)
#define MAX_HEADER_SYNC_SIZE   (64)
static uint8_t zero[1024];
static unsigned char u_data_buffer[512];


int esp8266_process(void)
{
    static    int wifi_rx_sta = S_NOT_DETECT;
    static    int frame_id = 0;
    static    int frame_length = 0;
    static    int frame_rx_counter = 0;
    static    unsigned char detecter_fifo_ptr = 0;
    static    unsigned char detecter_at_item_id = 0;
    
    
    int rx_len;
    if( (rx_len = uart_fifo_out(1, zero, sizeof(zero))) == 0) {
        return -1;
    }
    //printf("fifo read %d \r\n", rx_len);
    
    for(int i=0; i<rx_len; i++) {
        switch(wifi_rx_sta) {
        case S_NOT_DETECT:
            u_data_buffer[detecter_fifo_ptr] = zero[i];
            detecter_fifo_ptr++;
            detecter_fifo_ptr &= 0xff;  // mod 256
            
            //PRINTF("[%c]", zero[i]);
            //find rx flag
            for(int k=0 ;k<g_at_list_num; k++) {
                //if detected, skip it
                if(g_at_list[k].flag == FLAG_DETECTED)
                    continue;
                int at_len = strlen(g_at_list[k].at_word);
                //printf("%s\r\n", g_at_list[k].at_word);
                if(0 == mod_reverse_memcmp( u_data_buffer, detecter_fifo_ptr, g_at_list[k].at_word + at_len, at_len, 0xff)) {
                    //printf("%s detected in %d\r\n", g_at_list[k].at_word, detecter_fifo_ptr);
                    //no info, sign detected
                    if(g_at_list[k].jumpto == S_NOT_DETECT && g_at_list[k].flag == FLAG_CLEAR) {
                        g_at_list[k].flag = FLAG_DETECTED;
                    } else {
                        frame_rx_counter = 0;
                        detecter_at_item_id = k;
                        wifi_rx_sta = g_at_list[k].jumpto;
                        //printf("jump to %d \r\n", wifi_rx_sta);
                    }
                    break;
                }
            }
            
            break;
        case S_IPD_DETECTED:
            u_data_buffer[frame_rx_counter] = zero[i];
            frame_rx_counter++;
            
            if(zero[i] == ':') {
                //find , result is j
                u_data_buffer[frame_rx_counter] = 0;
                int len = sscanf( (char *)u_data_buffer, ",%d,%d:", &frame_id, &frame_length);
                if(len == 2 && frame_length < MAX_FRAME_SIZE)  {
                    //detected length
                    //printf("[%d],%d\r\n", frame_id, frame_length);
                    wifi_rx_sta = S_LEN_DETECTED;
                    frame_rx_counter = 0;
                } else {
                    //detecter error
                    APP_WARN("detecter error\r\n");
                    wifi_rx_sta = S_NOT_DETECT;
                    
                    APP_WARN("%.*s \r\n", frame_rx_counter, u_data_buffer);
                    //debug only, Unexpectedly, data may be lost
                    while(1);
                }
            } else if(frame_rx_counter >= MAX_HEADER_SYNC_SIZE) {
                 //detecter error
                 APP_WARN("detecter sync error\r\n");
                 wifi_rx_sta = S_NOT_DETECT;
                
                APP_WARN("%.*s \r\n", frame_rx_counter, u_data_buffer);
                //debug only, Unexpectedly, data may be lost
                while(1);
            }
            break;
        case S_LEN_DETECTED:
        {
            //APP_WARN("S_LEN_DETECTED: %d \r\n", frame_length);
            //Copy all data
            int copy_size = min(rx_len - i, frame_length - frame_rx_counter);
            //Call user function
            esp8266_user_data_callback(frame_id, zero + i, copy_size); ///addd
            
            //PRINTF("<%c,%02x>", zero[i], zero[i]);
            i += copy_size - 1;
            frame_rx_counter += copy_size;
            if(frame_rx_counter >= frame_length || frame_rx_counter >= MAX_FRAME_SIZE ) {
                //frame receive complete
                //PRINTF("<%d,%c,%c>", copy_size, zero[i], zero[i+1]);
                //PRINTF("%d %d \r\n", frame_rx_counter, frame_length);
                wifi_rx_sta = S_NOT_DETECT;
            }
            break;
        }
        case S_GET_INFO:
            g_at_list[detecter_at_item_id].rx_buf[frame_rx_counter] = zero[i];
            frame_rx_counter++;
            
            //printf("%c", zero[i]);
            if(zero[i] == '\r') {
                frame_rx_counter--;
                g_at_list[detecter_at_item_id].rx_buf[frame_rx_counter] = 0;
                APP_DEBUG("%s \r\n", g_at_list[detecter_at_item_id].rx_buf);
                
                g_at_list[detecter_at_item_id].flag = FLAG_DETECTED;
                wifi_rx_sta = S_NOT_DETECT;
            }
            break;
        case S_URC_INFO:
            
            //printf("%c", zero[i]);
            if(zero[i] == '\r') {
                esp8266_urc_process(
                    g_at_list[detecter_at_item_id].at_word, 
                    strlen(g_at_list[detecter_at_item_id].at_word)
                );
            }
            wifi_rx_sta = S_NOT_DETECT;
            break;
        default:
            APP_ERROR("failed \r\n");
            while(1);
        }
        
        
    }
    return 0;
}

/*****************************END OF FILE***************************/

#include <bds_stt.h>
#include <httpcu.h>
#include <os_malloc.h>
#include <app_debug.h>
#include <string.h>

#define BDS_STT_BUFFER_SIZE   (1024)
#define BDS_BUFFER_HALF(p)  ((p)+BDS_STT_BUFFER_SIZE/2)

#define BDS_STT_TOKEN_SIZE  (100)
#define BDS_STT_CUID    "HXDYXD_IOT"

HTTPCU_T httpcu_bds;
char gc_token[BDS_STT_TOKEN_SIZE] = "\0";

short audio_test[1000];
int audio_test_counter = 0;

int httpcu_bds_stt_write_callback(void *httpc, void **buf)
{
    if(audio_test_counter < 2000) {
        *buf = (char *)audio_test + audio_test_counter;
        audio_test_counter += 500;
        return 500;
    }
    return 0;
}


int bds_stt_test(void)
{
    
    char *http_buffer = os_malloc(BDS_STT_BUFFER_SIZE);
    if(http_buffer == NULL) {
        return -1;
    }
    
    if(!(*gc_token)) {
        /* http get request initial */
        snprintf(BDS_BUFFER_HALF(http_buffer), BDS_STT_BUFFER_SIZE/2, "/bds/get_token.php?cuid=%s", BDS_STT_CUID);
        httpcu_init(&httpcu_bds, 2, "ip.sococos.com", 8080, BDS_BUFFER_HALF(http_buffer));
        
        /* http process ... */
        int glen = httpcu_get(&httpcu_bds, http_buffer, BDS_STT_BUFFER_SIZE);
        if(glen > 0 && glen != BDS_STT_BUFFER_SIZE) {
            http_buffer[glen] = 0;
            strcpy(gc_token, http_buffer);
            APP_DEBUG("%s\r\n", gc_token);
        }
    }
        
    /* http post request initial */
    snprintf(BDS_BUFFER_HALF(http_buffer), BDS_STT_BUFFER_SIZE/2, "/server_api?cuid=%s&token=%s", BDS_STT_CUID, gc_token);
    httpcu_init(&httpcu_bds, 2, "vop.baidu.com", 80, BDS_BUFFER_HALF(http_buffer));
    httpcu_set_method(&httpcu_bds, HTTP_METHOD_POST);
    httpcu_set_header(&httpcu_bds, "Content-Type: audio/pcm;rate=16000\r\n");
    httpcu_set_write_callback(&httpcu_bds, httpcu_bds_stt_write_callback);
    audio_test_counter = 0;
    
    /* http process ... */
    int glen = httpcu_get(&httpcu_bds, http_buffer, BDS_STT_BUFFER_SIZE);
    if(glen > 0 && glen != BDS_STT_BUFFER_SIZE) {
        http_buffer[glen] = 0;
        APP_DEBUG("%s\r\n", http_buffer);
    }
        
    
    os_free(http_buffer);
    return 0;
}



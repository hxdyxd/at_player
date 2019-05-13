/* 2019 05 01 */
/* By hxdyxd */

#include <string.h>
#include <app_debug.h>
#include <stdlib.h>
#include "httpc.h"

#define S_HTTP_NOT_DETECT        0
#define S_HTTP_HEADER_DETECTED   1
#define S_HTTP_BODY_DETECTED     2
#define S_HTTP_CHUNKED_DETECT    3
#define S_HTTP_CHUNKED_DETECTED  4

typedef struct {
    char *header_key;
}HTTP_HEADER_T;

static const HTTP_HEADER_T g_header_list[] = {
    {
        .header_key = "\n\r\n",
    },
    {
        .header_key = "HTTP/1.0",
    },
    {
        .header_key = "HTTP/1.1",
    },
    {
        .header_key = "Server:",
    },
    {
        .header_key = "Content-Type:",
    },
    {
        .header_key = "Transfer-Encoding:",
    },
    {
        .header_key = "Connection:",
    },
    {
        .header_key = "Content-Encoding:",
    },
    {
        .header_key = "Content-Length:",
    },
};

static const int g_header_list_num = sizeof(g_header_list)/sizeof(HTTP_HEADER_T);


static void httpc_header_process(HTTPC_T *http_h, const char *header_key, const char *header_val, const int len)
{
    PRINTF("H[%s] V{%s}\r\n", header_key, header_val);
    if(strcmp(header_key, "Content-Length:") == 0) {
        http_h->content_length = atoi(header_val);
    } else if(strcmp(header_key, "HTTP/1.1") == 0 || strcmp(header_key, "HTTP/1.0") == 0) {
        if(strcmp(header_val, "200 OK") != 0) {
            //error
            if(http_h->httpc_body_callback) {
                http_h->httpc_body_callback(http_h, "", 0);
            }
        }
        http_h->content_length = 0;
        http_h->chunked_length = 0;
        http_h->http_header_detected = 1;
        PRINTF("http header start\r\n");
    } else if(strcmp(header_key, "Transfer-Encoding:") == 0) {
        http_h->content_length = -1;
        PRINTF("USE Transfer-Encoding\r\n");
    }
}

static void httpc_chunked_process(HTTPC_T *http_h, const char *c_val, const int len)
{
    PRINTF("%s \r\n", c_val);
    if(sscanf(c_val, "%x", &http_h->chunked_length) != 1) {
        PRINTF("chunked length error\r\n");
        while(1);
    } else {
        http_h->content_length += http_h->chunked_length;
        PRINTF("Chunk length = %d\r\n", http_h->chunked_length);
    }
}



__weak void httpc_body_callback(void *httpc, void *buf, int len)
{
    /* NOTE: This function Should not be modified, when the callback is needed,
           the esp8266_user_data_callback could be implemented in the user file
    */
}


int httpc_init(HTTPC_T *http_h, HTTPC_CALLBACK_T func_cb)
{
    http_h->http_header_detected = 0;
    http_h->content_length = 0;
    http_h->chunked_length = 0;
    
    http_h->httpc_sta = S_HTTP_NOT_DETECT;
    http_h->frame_rx_counter = 0;
    http_h->detecter_fifo_ptr = 0;
    http_h->detecter_at_item_id = 0;
    http_h->httpc_body_callback = httpc_body_callback;
    http_h->lock = HTTPC_UNLOCK;
    http_h->u_data_buffer = HTTPC_MALLOC(U_DATA_MAX_LENGTH);
    if(http_h->u_data_buffer == NULL) {
        return -1;
    }
    if(func_cb) {
        http_h->httpc_body_callback = func_cb;
    }
    return 0;
}

int httpc_close(HTTPC_T *http_h)
{
    if(http_h->lock == HTTPC_UNLOCK) {
        http_h->http_header_detected = 0;
        http_h->content_length = 0;
        http_h->chunked_length = 0;
        
        http_h->httpc_sta = S_HTTP_NOT_DETECT;
        http_h->frame_rx_counter = 0;
        http_h->detecter_fifo_ptr = 0;
        http_h->detecter_at_item_id = 0;
        http_h->lock = HTTPC_UNLOCK;
        http_h->httpc_body_callback = httpc_body_callback;
        HTTPC_FREE(http_h->u_data_buffer);
    } else {
        return -1;
    }

    return 0;
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


static void httpc_detect_header(HTTPC_T *http_h, const char ch)
{
    http_h->u_data_buffer[http_h->detecter_fifo_ptr] = ch;
    http_h->detecter_fifo_ptr++;
    http_h->detecter_fifo_ptr &= (U_DATA_MAX_LENGTH-1);  // mod 256
    
    //find header
    for(int k=0 ;k<g_header_list_num; k++) {

        int header_key_len = strlen(g_header_list[k].header_key);
        if(0 == mod_reverse_memcmp( http_h->u_data_buffer, http_h->detecter_fifo_ptr,
            g_header_list[k].header_key + header_key_len, header_key_len, U_DATA_MAX_LENGTH-1)) {
            if(k != 0) {
                http_h->frame_rx_counter = 0;
                http_h->detecter_at_item_id = k;
                http_h->httpc_sta = S_HTTP_HEADER_DETECTED;
                return;
            }

            if(k == 0 && http_h->http_header_detected) {
                http_h->http_header_detected = 0;
                if(http_h->content_length > 0) {
                    http_h->frame_rx_counter = 0;
                    http_h->httpc_sta = S_HTTP_BODY_DETECTED;
                } else if(http_h->content_length < 0) {
                    http_h->frame_rx_counter = 0;
                    http_h->httpc_sta = S_HTTP_CHUNKED_DETECT;
                } else {
                    PRINTF("unsupport http header \r\n");
                    //while(1);
                }
                return;
            }
            
            //printf("jump to %d \r\n", httpc_sta);
        }
    }
    return;
}


static void httpc_detected_header(HTTPC_T *http_h, const char ch)
{
    //skip space
    if(http_h->frame_rx_counter == 0 && ch == ' ') {
        return;
    }
    http_h->u_data_buffer[http_h->frame_rx_counter] = ch;
    http_h->frame_rx_counter++;
    
    if(ch == '\r') {
        http_h->frame_rx_counter--;
        http_h->u_data_buffer[http_h->frame_rx_counter] = 0;
        httpc_header_process(
            http_h, 
            g_header_list[http_h->detecter_at_item_id].header_key, 
            (char *)http_h->u_data_buffer, 
            http_h->frame_rx_counter
        );
        http_h->httpc_sta = S_HTTP_NOT_DETECT;
        return;
    } else if(http_h->frame_rx_counter >= MAX_HEADER_VALUE_LEN) {
        PRINTF("unsupported response header!\r\n");
        http_h->httpc_sta = S_HTTP_NOT_DETECT;
        return;
    }
    return;
}


static void httpc_detect_chunked(HTTPC_T *http_h, const char ch)
{
    //we need chunk length
    if(http_h->frame_rx_counter == 0 && (ch == '\r' || ch == '\n')) {
        PRINTF("skip crlf \r\n");
        return;
    }
    http_h->u_data_buffer[http_h->frame_rx_counter] = ch;
    http_h->frame_rx_counter++;
    
    if(ch == '\n' && http_h->frame_rx_counter > 2) {
        http_h->frame_rx_counter--;
        http_h->frame_rx_counter--;
        http_h->u_data_buffer[http_h->frame_rx_counter] = 0;
        httpc_chunked_process(http_h, (char *)http_h->u_data_buffer, http_h->frame_rx_counter);
        http_h->frame_rx_counter = 0;
        if(http_h->chunked_length == 0) {
            //chunked receive complete
            PRINTF("[chunked] transfer end %d\r\n", http_h->content_length);
            if(http_h->httpc_body_callback) {
                http_h->httpc_body_callback(http_h, "", 0);
            }
            http_h->httpc_sta = S_HTTP_NOT_DETECT;
            return;
        } else {
            http_h->httpc_sta = S_HTTP_CHUNKED_DETECTED;
            return;
        }
    } else if(http_h->frame_rx_counter >= MAX_CHUNKED_LENGTH_LEN) {
        PRINTF("unsupported chunked length!\r\n");
        http_h->httpc_sta = S_HTTP_NOT_DETECT;
        return;
    }
    return;
}


static int httpc_detected_body(HTTPC_T *http_h, void *pbuf, const int len)
{
    //Copy all data
    int copy_size = min(len, http_h->content_length - http_h->frame_rx_counter);
    //Call user function
    if(http_h->httpc_body_callback) {
        http_h->httpc_body_callback(http_h, pbuf, copy_size);
    }
    
    http_h->frame_rx_counter += copy_size;
    if(http_h->frame_rx_counter >= http_h->content_length ) {
        //frame receive complete
        PRINTF("[httpc] transfer end %d\r\n", http_h->content_length);
        if(http_h->httpc_body_callback) {
            http_h->httpc_body_callback(http_h, "", 0);
        }
        http_h->httpc_sta = S_HTTP_NOT_DETECT;
    }
    return copy_size;
}


static int httpc_detected_chunked(HTTPC_T *http_h, void *pbuf, const int len)
{
    //Copy all data
    int copy_size = min(len, http_h->chunked_length - http_h->frame_rx_counter);
    //Call user function
    if(http_h->httpc_body_callback) {
        http_h->httpc_body_callback(http_h, pbuf, copy_size);
    }
    
    http_h->frame_rx_counter += copy_size;
    if(http_h->frame_rx_counter >= http_h->chunked_length ) {
        //chunk receive complete
        PRINTF("%d %d \r\n", http_h->frame_rx_counter, http_h->chunked_length);
        http_h->frame_rx_counter = 0;
        http_h->chunked_length = -1;
        http_h->httpc_sta = S_HTTP_CHUNKED_DETECT;
    }
    return copy_size;
}



void httpc_process(HTTPC_T *http_h, const void *buf, const int len)
{
    uint8_t *pbuf = (uint8_t *)buf;
    
    http_h->lock = HTTPC_LOCK;
    if(http_h->u_data_buffer == NULL) {
        PRINTF("http_h must be initialized\r\n");
        http_h->lock = HTTPC_UNLOCK;
        return;
    }
    
    for(int i=0; i<len; i++) {
        switch(http_h->httpc_sta) {
        case S_HTTP_NOT_DETECT:
            httpc_detect_header(http_h, pbuf[i]);
            break;
        case S_HTTP_HEADER_DETECTED:
            httpc_detected_header(http_h, pbuf[i]);
            break;
        case S_HTTP_BODY_DETECTED:
            i += httpc_detected_body(http_h, pbuf + i, len - i);
            i--;
            break;
        case S_HTTP_CHUNKED_DETECT:
            httpc_detect_chunked(http_h, pbuf[i]);
            break;
        case S_HTTP_CHUNKED_DETECTED:
            i += httpc_detected_chunked(http_h, pbuf + i, len - i);
            i--;
            break;
        default:
            break;
        }
    }
    http_h->lock = HTTPC_UNLOCK;
}


/*****************************END OF FILE***************************/

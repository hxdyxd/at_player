/* 2019 05 01 */
/* By hxdyxd */

#ifndef _HTTPC_H
#define _HTTPC_H


#include "os_malloc.h"

typedef void (* HTTPC_CALLBACK_T)(void *httpc, void *buf, int len);


typedef struct {
    int http_header_detected;
    int content_length;
    int chunked_length;
    int httpc_sta;
    int frame_rx_counter;
    unsigned char detecter_fifo_ptr;
    unsigned char detecter_at_item_id;
    HTTPC_CALLBACK_T httpc_body_callback;
    unsigned char lock;
    unsigned char *u_data_buffer;
}HTTPC_T;

#define HTTPC_LOCK    1
#define HTTPC_UNLOCK  0


#define HTTPC_MALLOC  os_malloc
#define HTTPC_FREE    os_free


#define U_DATA_MAX_LENGTH   (256)
#define MAX_HEADER_VALUE_LEN   (128)
#define MAX_CHUNKED_LENGTH_LEN (16)

#define min(a, b)  (((a)<(b))?(a):(b))


int httpc_init(HTTPC_T *http_h, HTTPC_CALLBACK_T func_cb);
int httpc_close(HTTPC_T *http_h);
void httpc_process(HTTPC_T *http_h, const void *buf, const int len);


void httpc_body_callback(void *httpc, void *buf, int len);


#endif
/*****************************END OF FILE***************************/

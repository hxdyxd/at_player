/* 2019 05 02 */
/* By hxdyxd */

#ifndef _HTTPCU_H
#define _HTTPCU_H


#include <stdint.h>
#include <httpc.h>
#include <cmsis_os.h>

typedef void (* HTTPCU_CALLBACK_T)(void *httpc, void *buf, int len);
typedef int (* HTTPCU_WRITE_CALLBACK_T)(void *httpc, void **buf);

typedef struct {
    //httpcu_get
    uint8_t *pbuf;
    int pbuf_max_len;
    //httpc callback
    int pbuf_recv_len;
    //httpcu_init
    int socket_fd;
    char *host;
    uint16_t port;
    char *uri;
    //httpcu_set_method
    char *method;
    //httpcu_set_header
    char *req_header;
    //httpcu callback
    HTTPCU_WRITE_CALLBACK_T httpcu_body_write_callback;
    HTTPCU_CALLBACK_T httpcu_data_callback;
    //Semaphore
    osSemaphoreId http_complate_semaphore;
    HTTPC_T http_g;
    uint32_t last_tick;
}HTTPCU_T;


#define MAX_HTTP_HANDLE   (4)
#define MAX_HTTP_TIMEOUT  (8000*2)
#define USER_AGENT    "HXDYXD_IOT"


#define HTTP_METHOD_GET   "GET"
#define HTTP_METHOD_POST  "POST"

#define container_of(ptr, type, member) ((void *)((char *)(ptr) - offsetof(type, member)))



HTTPC_T *http_get_handle(int socket_fd);

int httpcu_init(HTTPCU_T *httpcu, int socket_fd,  char *host, uint16_t port, char *uri);
void httpcu_set_method(HTTPCU_T *httpcu, char *method);
void httpcu_set_header(HTTPCU_T *httpcu, char *req_header);
int httpcu_set_callback(HTTPCU_T *httpcu, HTTPCU_CALLBACK_T fn);
int httpcu_get(HTTPCU_T *httpcu, void *pbuf, int mlen);

//POST Only
int httpcu_set_write_callback(HTTPCU_T *httpcu, HTTPCU_WRITE_CALLBACK_T fn);


#endif
/*****************************END OF FILE***************************/

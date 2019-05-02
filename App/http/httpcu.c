/* 2019 05 02 */
/* By hxdyxd */

#include <stddef.h>
#include <string.h>
#include <app_debug.h>
#include <esp8266.h>
#include <httpcu.h>

static HTTPC_T *g_httpc_handle_list[MAX_HTTP_HANDLE] = {
    NULL,
    NULL,
    NULL,
    NULL,
};


void httpc_get_callback(void *httpc, void *buf, int len)
{
    HTTPCU_T * const httpcu =  container_of(httpc, HTTPCU_T, http_g);
    
    if(len == 0) {
        osSemaphoreRelease(httpcu->http_complate_semaphore);
    } else {
        if(httpcu->httpcu_data_callback == NULL) {
            if(httpcu->pbuf_recv_len + len > httpcu->pbuf_max_len) {
                len = httpcu->pbuf_max_len - httpcu->pbuf_recv_len;
                PRINTF("[httpcu] Overflow pbuf, max recv length: %d\r\n", httpcu->pbuf_recv_len);
            }
            
            memcpy(httpcu->pbuf + httpcu->pbuf_recv_len, buf, len);
        } else {
            httpcu->httpcu_data_callback(httpcu, buf, len);
        }
        httpcu->pbuf_recv_len += len;
        //PRINTF("%.*s", len, (char *)buf);
    }
}


HTTPC_T *http_get_handle(int socket_fd)
{
    return g_httpc_handle_list[socket_fd];
}


int httpcu_init(HTTPCU_T *httpcu, int socket_fd,  char *host, uint16_t port, char *uri)
{
    if(g_httpc_handle_list[httpcu->socket_fd] || httpcu->host) {
        return -1;
    }
    httpcu->socket_fd = socket_fd;
    httpcu->host = host;
    httpcu->port = port;
    httpcu->uri = uri;
    httpcu->httpcu_data_callback = NULL;
    return 0;
}


int httpcu_set_callback(HTTPCU_T *httpcu, HTTPCU_CALLBACK_T fn)
{
    httpcu->httpcu_data_callback = fn;
    return 0;
}


int httpcu_get(HTTPCU_T *httpcu, void *pbuf, int mlen)
{
    int ret = 0;
    int slen = 0;
    
    httpcu->pbuf_recv_len = 0;
    httpcu->pbuf = pbuf;
    httpcu->pbuf_max_len = mlen;
    
     /* create a binary semaphore used for informing ethernetif of frame reception */
    osSemaphoreDef(SEM);
    httpcu->http_complate_semaphore = osSemaphoreCreate(osSemaphore(SEM) , 1 );
    
    
    if(esp8266_connect(httpcu->socket_fd, "TCP", httpcu->host, httpcu->port) < 0) {
        APP_WARN("tcp0 connect failed \r\n");
        ret = -1;
        goto exit;
    }
    
    
    if(httpc_init(&httpcu->http_g, httpc_get_callback) < 0) {
        APP_WARN("httpc init failed \r\n");
        ret = -1;
        goto exit1;
    }
    
    g_httpc_handle_list[httpcu->socket_fd] = &httpcu->http_g;
    
    slen = snprintf( (char *)pbuf, mlen, "GET %s HTTP/1.1\r\n", httpcu->uri);
    if(esp8266_tcp_send(httpcu->socket_fd, pbuf, slen ) < 0) {
        APP_WARN("tcp0 send failed \r\n");
        ret = -1;
        goto exit2;
    }
    
    
    slen = snprintf( (char *)pbuf, mlen, "Host: %s:%d\r\n\r\n", httpcu->host, httpcu->port);
    if(esp8266_tcp_send(httpcu->socket_fd, pbuf, slen ) < 0) {
        APP_WARN("tcp0 send failed \r\n");
        ret = -1;
        goto exit2;
    }
    
    osSemaphoreWait( httpcu->http_complate_semaphore, TIME_WAITING_FOR_INPUT);
    
    while (osSemaphoreWait( httpcu->http_complate_semaphore, TIME_WAITING_FOR_INPUT) != osOK) {}
    APP_DEBUG("http get ok: %d\r\n", httpcu->pbuf_recv_len);
    
exit2:
    g_httpc_handle_list[httpcu->socket_fd] = NULL;
    httpcu->httpcu_data_callback = NULL;
    httpcu->host = NULL;
    httpc_close(&httpcu->http_g);
exit1:
    esp8266_close(httpcu->socket_fd);
exit:
    osSemaphoreDelete(httpcu->http_complate_semaphore);
    if(ret < 0) {
        return ret;
    } else {
        return httpcu->pbuf_recv_len;
    }
}

/*****************************END OF FILE***************************/

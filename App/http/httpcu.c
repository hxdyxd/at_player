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

/**
  *  @note: callback from uart_wifi_task
  **/
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
        httpcu->last_tick = TIME_COUNT();
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
    httpcu->method = HTTP_METHOD_GET;
    httpcu->req_header = NULL;
    httpcu->httpcu_body_write_callback = NULL;
    httpcu->httpcu_data_callback = NULL;
    return 0;
}


void httpcu_set_method(HTTPCU_T *httpcu, char *method)
{
    httpcu->method = method;
}

void httpcu_set_header(HTTPCU_T *httpcu, char *req_header)
{
    httpcu->req_header = req_header;
}


//POST Only
int httpcu_set_write_callback(HTTPCU_T *httpcu, HTTPCU_WRITE_CALLBACK_T fn)
{
    httpcu->httpcu_body_write_callback = fn;
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
    httpcu->last_tick = 0;
    
    slen = snprintf( (char *)pbuf, mlen, "%s %s HTTP/1.1\r\n", httpcu->method, httpcu->uri);
    if(esp8266_tcp_send(httpcu->socket_fd, pbuf, slen ) < 0) {
        APP_WARN("tcp0 send failed \r\n");
        ret = -1;
        goto exit2;
    }
    
    slen = snprintf( (char *)pbuf, mlen, "Host: %s:%d\r\n", httpcu->host, httpcu->port);
    if(esp8266_tcp_send(httpcu->socket_fd, pbuf, slen ) < 0) {
        APP_WARN("tcp0 send failed \r\n");
        ret = -1;
        goto exit2;
    }
    
    if( strcmp(httpcu->method, HTTP_METHOD_POST) == 0) {
        /* POST Only */
        slen = snprintf( (char *)pbuf, mlen, "Transfer-Encoding: chunked\r\n");
        if(esp8266_tcp_send(httpcu->socket_fd, pbuf, slen ) < 0) {
            APP_WARN("tcp0 send failed \r\n");
            ret = -1;
            goto exit2;
        }
    }
    
    if(httpcu->req_header) {
        if(esp8266_tcp_send(httpcu->socket_fd, httpcu->req_header, strlen(httpcu->req_header) ) < 0) {
            APP_WARN("tcp0 send failed \r\n");
            ret = -1;
            goto exit2;
        }
    }
    
    slen = snprintf( (char *)pbuf, mlen, "User-Agent: %s\r\n\r\n", USER_AGENT);
    if(esp8266_tcp_send(httpcu->socket_fd, pbuf, slen ) < 0) {
        APP_WARN("tcp0 send failed \r\n");
        ret = -1;
        goto exit2;
    }
    
    if( strcmp(httpcu->method, HTTP_METHOD_POST) == 0 && httpcu->httpcu_body_write_callback) {
        /* POST Only */
        char *wpbuf = NULL;
        int wlen = 0;
        while((wlen = httpcu->httpcu_body_write_callback(httpcu, (void **)&wpbuf)) > 0) {
            slen = snprintf( (char *)pbuf, mlen, "%x\r\n", wlen);
            if(esp8266_tcp_send(httpcu->socket_fd, pbuf, slen ) < 0) {
                APP_WARN("tcp0 send failed \r\n");
                ret = -1;
                goto exit2;
            }
            PRINTF("%s", pbuf);
            
            if(esp8266_tcp_send(httpcu->socket_fd, wpbuf, wlen) < 0) {
                APP_WARN("tcp0 send failed \r\n");
                ret = -1;
                goto exit2;
            }
            
            if(esp8266_tcp_send(httpcu->socket_fd, "\r\n", 2 ) < 0) {
                APP_WARN("tcp0 send failed \r\n");
                ret = -1;
                goto exit2;
            }
        }
    }
    if( strcmp(httpcu->method, HTTP_METHOD_POST) == 0) {
        /* POST Only */
        if(esp8266_tcp_send(httpcu->socket_fd, "0\r\n\r\n", 5 ) < 0) {
            APP_WARN("tcp0 send failed \r\n");
            ret = -1;
            goto exit2;
        }
    }
    
    
    osSemaphoreWait( httpcu->http_complate_semaphore, TIME_WAITING_FOR_INPUT);
    
    while (osSemaphoreWait( httpcu->http_complate_semaphore, MAX_HTTP_TIMEOUT) != osOK) {
        if(TIME_COUNT() - httpcu->last_tick > MAX_HTTP_TIMEOUT) {
            APP_WARN("http get timeout\r\n");
            ret = -2;
            break;
        }
    }
    APP_DEBUG("http get ok: %d\r\n", httpcu->pbuf_recv_len);
    
exit2:
    g_httpc_handle_list[httpcu->socket_fd] = NULL;
    httpcu->httpcu_body_write_callback = NULL;
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

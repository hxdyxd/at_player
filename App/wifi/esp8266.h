/* 2019 04 20 */
/* By hxdyxd */

#ifndef _ESP8266_H
#define _ESP8266_H


//#define BANDRATE_MAX    (115200*16)
//#define BANDRATE_MAX    (115200*8)
#define BANDRATE_MAX    (2000000)
#define WIFI_HOSTNAME   "HXDYXD_IOT"
#define WIFI_SSID       "101"
#define WIFI_PASSWORD   "198612888"


#define  ESP8266_STATUS_GOTIP         2
#define  ESP8266_STATUS_CON           3
#define  ESP8266_STATUS_LOSE_CON      4
#define  ESP8266_STATUS_NOT_ROUTER    5

#define S_NOT_DETECT    0
#define S_IPD_DETECTED  1

#define S_LEN_DETECTED  3

#define S_GET_INFO     10
#define S_URC_INFO     12


#define FLAG_ALWAYS     -1
#define FLAG_CLEAR       0
#define FLAG_DETECTED    1

/* The time to block waiting for input. */
#define TIME_WAITING_FOR_INPUT ( portMAX_DELAY )


#define min(a, b)  (((a)<(b))?(a):(b))
typedef void (*USER_DATA_CB_T)(int, unsigned char *, int);


int mod_reverse_memcmp(const void *buf, unsigned int ptr, const void *end1, size_t n, unsigned int mask);
int reverse_memcmp(const void *end1, const void *end2, size_t n);
int php_strpos(void *buf, int buflen, void *subbuf, int subbuflen, int start);


int esp8266_init(void);
int esp8266_get_status(void);
int esp8266_set_mux(int enable);
int esp8266_set_hostname(char *name);
int esp8266_connect_ap(char *ssid, char *passwd);
int esp8266_set_passive_recv(int enable);

int esp8266_enable_sntp(int enable, int timezone);
int esp8266_sntp_get(void *buf, int len);

void esp8266_start_smartconfig(void);
void esp8266_stop_smartconfig(void);
char *esp8266_dns_resolve(const char *hostname);

int esp8266_connect(int id, char *type, char *host, unsigned short port);
int esp8266_close(int id);
int esp8266_tcp_send(int id, void *buf, int length);
int esp8266_tcp_clen(void);

#define esp8266_tcp_connect(id,host,port) esp8266_connect(id,"TCP",host,port)
int esp8266_process(void);

int esp8266_init_asnyc(void);

#endif

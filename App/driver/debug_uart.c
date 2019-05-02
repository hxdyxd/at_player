/* 2019 04 10 */
/* By hxdyxd */

#include <stdio.h>
#include "usart.h"
#include "cmsis_os.h"
//#include "usbd_cdc_if.h"


int fputc(int ch, FILE *f)
{
    uint8_t c = ch;
    HAL_UART_Transmit(&huart1, &c, 1, 200);
    //CDC_Transmit_FS(&c, 1);
    return ch;
}

int fgetc(FILE *f)
{
    return 1;
}


osMutexId debug_mutex;

int debug_init(void)
{
    osMutexDef(debug_mutex_c);
    debug_mutex = osMutexCreate(osMutex(debug_mutex_c));
    printf("create mutex for debug success\r\n");
    return 0;
}

void put_hex(uint8_t *p, uint8_t len, uint8_t lf)
{
    int i;
    for(i=0;i<len;i++) {
        printf("%02x", *p );
        p++;
    }
    if(lf) {
        printf("\r\n");
    }
}


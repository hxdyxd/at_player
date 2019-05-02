/* hxdyxd 2019 04 20 */

#ifndef _OS_MALLOC_H
#define _OS_MALLOC_H

#include <stdio.h>
#include "FreeRTOS.h"
#define malloc(s)   pvPortMalloc(s), printf("malloc size: %d %d\r\n", s, xPortGetFreeHeapSize())
#define calloc(a,b)   pvPortMalloc(a*b), printf("calloc size: %d \r\n", a*b)
#define free(p)     vPortFree(p), printf("free: %p \r\n", p);

#define os_malloc(s)   pvPortMalloc(s), printf("malloc size: %d %d\r\n", s, xPortGetFreeHeapSize())
#define os_calloc(a,b)   pvPortMalloc(a*b), printf("calloc size: %d \r\n", a*b)
#define os_free(p)     vPortFree(p), printf("free: %p \r\n", p);


#endif

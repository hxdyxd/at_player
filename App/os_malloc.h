/* hxdyxd 2019 04 20 */

#ifndef _OS_MALLOC_H
#define _OS_MALLOC_H

#include <stdio.h>
#include "FreeRTOS.h"
#define malloc(s)   os_malloc(s)
#define calloc(a,b)   os_calloc(a,b)
#define free(p)     os_free(p)



void os_malloc_init(void);
void *os_malloc(size_t sz);
void *os_calloc(size_t nmemb, size_t size);
void os_free(void *ptr);

#endif

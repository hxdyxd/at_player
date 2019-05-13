#include <os_malloc.h>
#include <cmsis_os.h>
#include <app_debug.h>
#include <string.h>

static osMutexId os_malloc_mutex;
static int32_t gs_malloc_counter = 0;


void os_malloc_init(void)
{
    gs_malloc_counter = 0;
    osMutexDef(debug_mutex_c);
    os_malloc_mutex = osMutexCreate(osMutex(debug_mutex_c));
}

void *os_calloc(size_t nmemb, size_t size)
{
    size_t len = nmemb*size;
    void *p = os_malloc(len);
    memset(p, 0, len);
    return p;
}


void *os_malloc(size_t sz)
{
    osMutexWait(os_malloc_mutex, portMAX_DELAY);
    void *p = pvPortMalloc(sz);
    int free_sz = xPortGetFreeHeapSize();
    gs_malloc_counter++;
    PRINTF("[%d]malloc %p size: %d %d\r\n", gs_malloc_counter, p, sz, free_sz);
    osMutexRelease(os_malloc_mutex);
    return p;
}


void os_free(void *ptr)
{
    osMutexWait(os_malloc_mutex, portMAX_DELAY);
    vPortFree(ptr);
    gs_malloc_counter--;
    PRINTF("[%d]free: %p \r\n", gs_malloc_counter, ptr);
    osMutexRelease(os_malloc_mutex);
}


/* 2019 05 02 */
/* By hxdyxd */

#include <uart_receiver.h>
#include <usart.h>
#include <uart_fifo_rx.h>
#include <esp8266.h>



/* Semaphore to signal incoming packets */
static osSemaphoreId s_xSemaphore1 = NULL;
static osSemaphoreId s_xSemaphore2 = NULL;


void uart_update_callback(int id)
{
    if(id == 1) {
        //释放信号量，通知串口线程处理数据
        osSemaphoreRelease(s_xSemaphore2);
    }
    if(id == 0) {
        osSemaphoreRelease(s_xSemaphore1);
    }
}

__weak void uart_cmd_callback(char ch)
{
     putchar(ch);
}


void uart_fifo_task_proc1(void const *p)
{
    /* create a binary semaphore used for informing ethernetif of frame reception */
    osSemaphoreDef(SEM);
    s_xSemaphore1 = osSemaphoreCreate(osSemaphore(SEM) , 1 );
    
    
    while(1) {
        if (osSemaphoreWait( s_xSemaphore1, TIME_WAITING_FOR_INPUT) == osOK) {
            int ch;
            if(uart_fifo_out(0, &ch, 1)) {
                uart_cmd_callback(ch);
            }
        }
    }
    //vTaskDelete(NULL);
}


void uart_fifo_task_proc2(void const *p)
{
    /* create a binary semaphore used for informing ethernetif of frame reception */
    osSemaphoreDef(SEM);
    s_xSemaphore2 = osSemaphoreCreate(osSemaphore(SEM) , 1 );
    
    
    while(1) {
        if (osSemaphoreWait( s_xSemaphore2, TIME_WAITING_FOR_INPUT) == osOK) {
            while(esp8266_process() >= 0) 
            {
            }
        }
    }
    //vTaskDelete(NULL);
}

osThreadId uart_fifo_task_proc_handle1;
osThreadId uart_fifo_task_proc_handle2;

int uart_receiver_init(void)
{
    uart_fifo_rx_init();
    APP_DEBUG("uart fifo rx init success \r\n");
    
    //UART receiving task
    osThreadDef(uart_cmd_task, uart_fifo_task_proc1, osPriorityLow, 0, 256);
    uart_fifo_task_proc_handle1 = osThreadCreate(osThread(uart_cmd_task), NULL);
    
    //UART receiving task
    osThreadDef(uart_wifi_task, uart_fifo_task_proc2, osPriorityRealtime, 0, 256);
    uart_fifo_task_proc_handle2 = osThreadCreate(osThread(uart_wifi_task), NULL);
    
    if(uart_fifo_task_proc_handle1 && uart_fifo_task_proc_handle2) {
        return 0;
    } else {
        return -1;
    }
}



/*****************************END OF FILE***************************/

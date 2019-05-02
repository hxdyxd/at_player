/* 2019 05 02 */
/* By hxdyxd */

#include <uart_receiver.h>
#include <usart.h>
#include <uart_fifo_rx.h>
#include <esp8266.h>



/* Semaphore to signal incoming packets */
osSemaphoreId s_xSemaphore = NULL;


void uart_update_callback(int id)
{
    if(id == 1) {
        //释放信号量，通知串口线程处理数据
        osSemaphoreRelease(s_xSemaphore);
    }
}


void uart_fifo_task_proc(void const *p)
{
    /* create a binary semaphore used for informing ethernetif of frame reception */
    osSemaphoreDef(SEM);
    s_xSemaphore = osSemaphoreCreate(osSemaphore(SEM) , 1 );
    
    
    uart_fifo_rx_init();
    APP_DEBUG("uart fifo rx init success \r\n");
    
    while(1) {
        if (osSemaphoreWait( s_xSemaphore, TIME_WAITING_FOR_INPUT) == osOK) {
            while(esp8266_process() >= 0) 
            {
            }
        }
    }
    //vTaskDelete(NULL);
}

osThreadId uart_fifo_task_proc_handle;

int uart_receiver_init(void)
{
    
    //UART receiving task
    osThreadDef(uart_fifo_task, uart_fifo_task_proc, osPriorityRealtime, 0, 512);
    uart_fifo_task_proc_handle = osThreadCreate(osThread(uart_fifo_task), NULL);
    if(uart_fifo_task_proc_handle) {
        return 0;
    } else {
        return -1;
    }
}



/*****************************END OF FILE***************************/

/* 2019 04 20 */
/* By hxdyxd */

#include "uart_fifo_rx.h"
#include "usart.h"
#include "dma.h"
#include "kfifo.h"
#include "user_main.h"

#define FIFO_RX_SIZE  (2048)

#define UART_RX_NUM   (2)


extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart6_rx;


typedef struct {
    //user define
    UART_HandleTypeDef *uart;
    DMA_HandleTypeDef *dma;
    //inner
    struct __kfifo fifo;
}uart_fifo_rx_t;


uart_fifo_rx_t huartx[UART_RX_NUM] = {
    {
        .uart = &huart1,
        .dma = &hdma_usart1_rx,
    },
    {
        .uart = &huart6,
        .dma = &hdma_usart6_rx,
    },
};

void UART_DMAReceiveCplt_t(DMA_HandleTypeDef *hdma);


HAL_StatusTypeDef uart_receive_dma_idle(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size)
{  
    uint32_t *tmp;

    /* Check that a Rx process is not already ongoing */
    if(huart->RxState == HAL_UART_STATE_READY) 
    {
        if((pData == NULL ) || (Size == 0)) 
        {
          return HAL_ERROR;
        }

        /* Process Locked */
        __HAL_LOCK(huart);

        huart->pRxBuffPtr = pData;
        huart->RxXferSize = Size;

        huart->ErrorCode = HAL_UART_ERROR_NONE;
        huart->RxState = HAL_UART_STATE_BUSY_RX;
            
        /* Set the UART DMA transfer complete callback */
        huart->hdmarx->XferCpltCallback = UART_DMAReceiveCplt_t;

        /* Set the UART DMA Half transfer complete callback */
        huart->hdmarx->XferHalfCpltCallback = UART_DMAReceiveCplt_t;

        /* Set the DMA error callback */
        huart->hdmarx->XferErrorCallback = NULL;

        /* Set the DMA abort callback */
        huart->hdmarx->XferAbortCallback = NULL;

        /* Enable the DMA Stream */
        tmp = (uint32_t*)&pData;
        HAL_DMA_Start_IT(huart->hdmarx, (uint32_t)&huart->Instance->DR, *(uint32_t*)tmp, Size);

        /* Clear the Overrun flag just before enabling the DMA Rx request: can be mandatory for the second transfer */
        __HAL_UART_CLEAR_OREFLAG(huart);

        /* Process Unlocked */
        __HAL_UNLOCK(huart);
        

        /* Enable the UARTIdle Interrupt */
       __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);

        /* Enable the DMA transfer for the receiver request by setting the DMAR bit 
        in the UART CR3 register */
        SET_BIT(huart->Instance->CR3, USART_CR3_DMAR);

        return HAL_OK;
    }
    else
    {
        return HAL_BUSY; 
    }
}


#define UNUSED_SIZE(fifo)  (((fifo)->mask + 1) - ((fifo)->in - (fifo)->out))


unsigned int base_in = 0;
unsigned int transfer_in;
unsigned int last_counter[UART_RX_NUM] = {0};



__weak void uart_update_callback(int id)
{
    UNUSED(id);
    /* NOTE: This function Should not be modified, when the callback is needed,
           the uart_update_callback could be implemented in the user file
    */
}


void uart_fifo_rx_handler(UART_HandleTypeDef *huart)
{
    if( __HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) ) {
        
        for(int i=0; i<UART_RX_NUM; i++) {
            if(huartx[i].uart == huart) {
               // __HAL_DMA_DISABLE(huartx[i].dma);
                transfer_in = FIFO_RX_SIZE - __HAL_DMA_GET_COUNTER(huartx[i].dma);
               // __HAL_DMA_ENABLE(huartx[i].dma);
                unsigned int transfer_size = (transfer_in - last_counter[i]) & (FIFO_RX_SIZE-1);
                //more ?
                if( FIFO_RX_SIZE - uart_fifo_size(i) >= transfer_size ) {
                    huartx[i].fifo.in = base_in + transfer_in;
                    last_counter[i] = transfer_in;
                } else {
                    APP_ERROR("ffifo unused: %d\r\n", FIFO_RX_SIZE - uart_fifo_size(i));
#if UART_FIFO_RX_HOLD_BUG
                    while(1);
#endif
                }
                //printf("[IDLE] transfer_in : %d \r\n", transfer_in);
                if(uart_fifo_size(i) >= FIFO_RX_SIZE>>1) {
                    //dma fifo数据已经过半，继续传输可能产生溢出，暂时禁止对端传输
                    uart_fifo_rx_receive_RTS(1, 0);
                }
                uart_update_callback(i);
            }
        }
        
         __HAL_UART_CLEAR_IDLEFLAG(huart);
    }
}


void UART_DMAReceiveCplt_t(DMA_HandleTypeDef *hdma)
{
    for(int i=0; i<UART_RX_NUM; i++) {
        if(huartx[i].dma == hdma) {
            // __HAL_DMA_DISABLE(huartx[i].dma);
            transfer_in = FIFO_RX_SIZE - __HAL_DMA_GET_COUNTER(huartx[i].dma);
            if(transfer_in == 0) {
                base_in += FIFO_RX_SIZE;
            }
            
           // __HAL_DMA_ENABLE(huartx[i].dma);
            unsigned int transfer_size = (transfer_in - last_counter[i]) & (FIFO_RX_SIZE-1);
            //more ?
            
            if(  FIFO_RX_SIZE - uart_fifo_size(i) >= transfer_size ) {
                huartx[i].fifo.in = base_in + transfer_in;
                last_counter[i] = transfer_in;
            } else {
                APP_ERROR("ffifo unused: %d\r\n", FIFO_RX_SIZE - uart_fifo_size(i));
#if UART_FIFO_RX_HOLD_BUG
                while(1);
#endif
            }
            //printf("[CB] transfer_in : %d \r\n", transfer_in);
            if(uart_fifo_size(i) >= FIFO_RX_SIZE>>1) {
                //dma fifo数据已经过半，继续传输可能产生溢出，暂时禁止对端传输
                uart_fifo_rx_receive_RTS(1, 0);
            }
            uart_update_callback(i);
        }
    }
}


void uart_fifo_rx_receive_RTS(int id, int enable)
{
    /* Process Locked */
    //__HAL_LOCK(huartx[id].uart);
    if(id == 1) {
        if(enable) {
            /* Enable the UART DMA Rx request */
            //SET_BIT(huartx[id].uart->Instance->CR3, USART_CR3_DMAR);
            HAL_GPIO_WritePin(ESP8266_RTS_BASE, GPIO_PIN_RESET);
        } else {
             /* Disable the UART DMA Rx request */
            //CLEAR_BIT(huartx[id].uart->Instance->CR3, USART_CR3_DMAR);
            HAL_GPIO_WritePin(ESP8266_RTS_BASE, GPIO_PIN_SET);
        }
    }
    /* Process Unlocked */
    //__HAL_UNLOCK(huartx[id].uart);
}


int uart_fifo_rx_init(void)
{
    int i;
    for(i=0; i<UART_RX_NUM; i++) {
        if(__kfifo_alloc(&huartx[i].fifo, FIFO_RX_SIZE, 1, 0) < 0) {
            printf("uart kfifo alloc error \r\n");
            return -1;
        }
        
        //Start DMA Receive
        if(uart_receive_dma_idle(huartx[i].uart, huartx[i].fifo.data, FIFO_RX_SIZE) != HAL_OK) {
            printf("uart receive dma start error \r\n");
            return -1;
        }
        printf("uart_receive_dma_idle \r\n");
    }
    return 0;
}


int uart_fifo_out(int id, void *buf, int len)
{
    int out_len = __kfifo_out(&huartx[id].fifo, buf, len);
    //恢复传输
    if(uart_fifo_size(id) < FIFO_RX_SIZE>>1) {
        uart_fifo_rx_receive_RTS(1, 1);
    }
    return out_len;
}

int uart_fifo_out_peek(int id, void *buf, int len)
{
    return __kfifo_out_peek(&huartx[id].fifo, buf, len);
}

int uart_fifo_size(int id)
{
    return huartx[id].fifo.in - huartx[id].fifo.out;
}



#include "pwm_transfer.h"
#include "kfifo.h"
#include "tim.h"
#include "dma.h"
#include <math.h>
#include "app_debug.h"
#include "cmsis_os.h"
#include <string.h>


#define AUDIO_QUEYE_SIZE  (1024)

extern DMA_HandleTypeDef hdma_tim3_ch4_up;

struct __kfifo pwm_fifo;
uint16_t pwm_dma_isr_buf[AUDIO_QUEYE_SIZE];

void TIM_DMADelayPulseCplt_t(DMA_HandleTypeDef *hdma)
{
    pwm_transfer_stop();
    
    unsigned int size = __kfifo_out(&pwm_fifo, &pwm_dma_isr_buf, AUDIO_QUEYE_SIZE); //element number
    if(size == 0) {
        memset(pwm_dma_isr_buf, 0, sizeof(pwm_dma_isr_buf));
        pwm_transfer_transmit_dma( pwm_dma_isr_buf, AUDIO_QUEYE_SIZE);
    } else {
        pwm_transfer_transmit_dma( pwm_dma_isr_buf, size );
    }
}

void TIM_DMAError_t(DMA_HandleTypeDef *hdma)
{
    /* Change the htim state */
    htim3.State = HAL_TIM_STATE_READY;
    
    printf("e%d \n", hdma->ErrorCode);
}


void pwm_device_set_rate(int rate, int chls)
{
    htim3.Init.Period = (100000000/rate)-1;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
    {
        Error_Handler();
    }
    
    memset(pwm_dma_isr_buf, 0, sizeof(pwm_dma_isr_buf) );
    while(pwm_transfer_transmit_dma(pwm_dma_isr_buf, AUDIO_QUEYE_SIZE) < 0) {
        osDelay(1000);
    }
    APP_DEBUG("pwm start \r\n");
}



void pwm_transfer_stop(void)
{
    __HAL_TIM_DISABLE_DMA(&htim3, TIM_DMA_UPDATE);
    
    /* Disable the Capture compare channel */
    TIM_CCxChannelCmd(htim3.Instance, TIM_CHANNEL_4, TIM_CCx_DISABLE);

    if(IS_TIM_ADVANCED_INSTANCE(htim3.Instance) != RESET)  
    {
        /* Disable the Main Output */
        __HAL_TIM_MOE_DISABLE(&htim3);
    }

    /* Disable the Peripheral */
    __HAL_TIM_DISABLE(&htim3);

    /* Change the htim state */
    htim3.State = HAL_TIM_STATE_READY;
}


void pwm_transfer_init(void)
{
    htim3.Init.Period = (100000000/8000)-1;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
    {
        Error_Handler();
    }
    
    
    htim4.Init.Period = (256)-1;
    if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
    {
        Error_Handler();
    }
    
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
    
     //byte number
    if(__kfifo_alloc(&pwm_fifo, AUDIO_QUEYE_SIZE, 2, 0) < 0) {
        APP_ERROR("fifo error \r\n");
    }
    
    pwm_transfer_stop();
}


int pwm_transfer_transmit_dma(void *buffer, int length)
{
    if(((HAL_TIM_StateTypeDef)htim3.State == HAL_TIM_STATE_BUSY)) {
        return -1;
    }
    
    htim3.State = HAL_TIM_STATE_BUSY;
      /* Set the DMA Period elapsed callback */
    hdma_tim3_ch4_up.XferCpltCallback = TIM_DMADelayPulseCplt_t;

    /* Set the DMA error callback */
    hdma_tim3_ch4_up.XferErrorCallback = TIM_DMAError_t;

    /* Enable the DMA Stream */
    HAL_DMA_Start_IT(&hdma_tim3_ch4_up, (uint32_t)buffer, (uint32_t)(&htim4.Instance->CCR3), length);

    /* Enable the TIM Capture/Compare 1 DMA request */
    __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_UPDATE);
    
      /* Enable the Capture compare channel */
    TIM_CCxChannelCmd(htim3.Instance, TIM_CHANNEL_4, TIM_CCx_ENABLE);

    if(IS_TIM_ADVANCED_INSTANCE(htim3.Instance) != RESET)  
    {
        /* Enable the main output */
        __HAL_TIM_MOE_ENABLE(&htim3);
    }

    /* Enable the Peripheral */
    __HAL_TIM_ENABLE(&htim3); 
    return 0;
}


/*****************************END OF FILE***************************/

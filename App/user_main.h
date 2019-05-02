/* 2019 04 10 */
/* By hxdyxd */

#ifndef _USER_MAIN_H_
#define _USER_MAIN_H_

#include "gpio.h"

#define STIMER_FM_CTL_PROC_ID            (0)
#define STIMER_SHOW_PROC_ID              (1)
#define STIMER_OLED_AUTO_OFF_PROC_ID     (2)

#define SCREEN_BUFFER_MAX_SIZE          (20)


/* LEDS */
#define LED_OFF(id)  HAL_GPIO_WritePin(id, GPIO_PIN_SET)
#define LED_ON(id)   HAL_GPIO_WritePin(id, GPIO_PIN_RESET)
#define LED_REV(id)  HAL_GPIO_TogglePin(id)

#define LED_BASE       LD2_GPIO_Port, LD2_Pin
#define ESP8266_BASE   WIFI_RST_GPIO_Port, WIFI_RST_Pin
#define ESP8266_RTS_BASE   WIFI_RTS_GPIO_Port, WIFI_RTS_Pin

void user_os_setup(void);
void user_setup(void);
void user_loop(void);


#endif
/*****************************END OF FILE***************************/

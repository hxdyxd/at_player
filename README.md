
# 介绍

这个工程主要用于测试串口WIFI模块（ESP8266）在高波特率加硬件流控下的表现，MCU使用了STM32F411RE，100Mhz主频，
运行FreeRTOS，工程使用STM32CubeMX生成。

效果为，HTTP GET MP3音频文件并透过PWM调制，实时播放，输出信号经低通滤波后可送speaker播放。

代码分为UART接收处理、ESP8266串口解析、PWM传输三部分，各部分间通过无锁fifo共享数据。

## UART接收处理：
使用了UART DMA循环模式接收，在DMA传输完成、过半以及UART IDLE中断中释放信号量，触发ESP8266串口解析数据。
## ESP8266串口解析：
使用单独线程运行，从串口的DMA 中读出数据，在esp8266_process中针对AT响应和数据接收做统一处理。
## PWM传输：
使用TIM3UP事件定时触发DMA传输，改变TIM4 CCR寄存器，实时改变占空比；TIM3溢出频率同音频采样率，TIM4溢出频率约392khz(100000/255)。

FFMPEG 生成PCM文件  
ffmpeg -i $(INPUT) -ar 11025 -f u8 -ac 1 out.pcm  

#ifndef _PWM_TRANSFER_H
#define _PWM_TRANSFER_H

extern struct __kfifo pwm_fifo;


void pwm_device_set_rate(int rate, int chls);
void pwm_transfer_init(void);
void pwm_transfer_stop(void);

int pwm_transfer_transmit_dma(void *buffer, int length);

#endif

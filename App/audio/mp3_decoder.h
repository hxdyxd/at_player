#ifndef _MP3_DECODER_H
#define _MP3_DECODER_H

#include <stdint.h>

#define HTTPCU_BUF_MAX_LEN  1024

int mad_netword_player_start(char *host, uint16_t port, char *uri);
int mad_netword_player_pause(void);

#endif
/*****************************END OF FILE***************************/

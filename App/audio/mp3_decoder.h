#ifndef _MP3_DECODER_H
#define _MP3_DECODER_H

#include <stdint.h>

typedef struct {
    char *host;
    uint16_t port;
    char *uri;
}MP3_DECODER_T;



#define HTTPCU_BUF_MAX_LEN  1024

void mad_netword_player_start(const void *config);
int mad_netword_player_start_async(MP3_DECODER_T *config);
int mad_netword_player_pause(void);
uint32_t mad_netword_player_get_play_time(void);
unsigned int mad_network_player_status(void);

#endif
/*****************************END OF FILE***************************/

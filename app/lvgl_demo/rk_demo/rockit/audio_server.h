#ifndef __AUDIO_SERVER_H__
#define __AUDIO_SERVER_H__

enum
{
    STATE_IDLE,
    STATE_RUNNING,
    STATE_EXIT,
};

int run_audio_server(void);
int exit_audio_server(void);
int run_audio_client(char *ip);
int exit_audio_client(void);
int audio_client_state(void);

#endif


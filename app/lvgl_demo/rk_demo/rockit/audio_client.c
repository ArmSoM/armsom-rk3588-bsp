#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_in.h"
#include "audio_out.h"
#include "audio_server.h"

#include "rk_defines.h"
#include "rk_debug.h"
#include "rk_mpi_sys.h"

static int fd;
static int state = STATE_IDLE;

static int ai_write(void *arg, char *buf, int len)
{
    int num = write(fd, buf, len);

    return num;
}

static void *do_cap(void *arg)
{
    ai_init();
    state = STATE_RUNNING;
    while (state == STATE_RUNNING)
    {
        if (ai_fetch(ai_write, NULL) == -1)
            break;
    }
    ai_deinit();

    close(fd);
    fd = -1;
    state = STATE_IDLE;
}

int audio_client_state(void)
{
    return state;
}

int exit_audio_client(void)
{
    state = STATE_EXIT;
}

int run_audio_client(char *ip)
{
    struct sockaddr_in serveraddr;
    pthread_t tid;
    int ret;

    if (state == STATE_RUNNING)
        return 0;

    state = STATE_RUNNING;
    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
    {
        printf("socket error\n");
        return fd;
    }

    printf("coonnect to [%s]\n", ip);
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serveraddr.sin_addr.s_addr);
    serveraddr.sin_port = htons(9999);
    ret = connect(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));

    if (ret < 0)
    {
        printf("connect error\n");
        close(fd);
        fd = -1;
        return ret;
    }

    ret = pthread_create(&tid, NULL, do_cap, NULL);
    if (ret < 0)
    {
        printf("pthread error\n");
        close(fd);
        fd = -1;
        state = STATE_IDLE;
    }

    return ret;
}


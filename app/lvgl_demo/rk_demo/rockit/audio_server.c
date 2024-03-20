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

static int ao_read(void *arg, char *buf, int len)
{
    int cfd = *(int *)arg;
    int num = read(cfd, buf, len);

    return num;
}

static int audio_server_init(void)
{
    struct sockaddr_in saddr;
    int ret;

    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
        return fd;

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(9999);
    ret = bind(fd, (struct sockaddr *)&saddr, sizeof(saddr));

    if (ret < 0)
        return ret;

    return fd;
}

static void *audio_server(void *arg)
{
    char clientIP[16];
    unsigned short clientPort;
    struct sockaddr_in clientaddr;
    int len;
    int cfd;
    int ret;

    state = STATE_RUNNING;
    while (state == STATE_RUNNING)
    {
        printf("start listening...\n");
        ret = listen(fd, 8);
        if (ret == -1)
        {
            printf("listen error\n");
            break;
        }

        len = sizeof(clientaddr);
        cfd = accept(fd, (struct sockaddr *)&clientaddr, &len);

        if (cfd == -1)
        {
            printf("accept error\n");
            break;
        }

        inet_ntop(AF_INET, &clientaddr.sin_addr.s_addr, clientIP, sizeof(clientIP));
        clientPort = ntohs(clientaddr.sin_port);
        printf("client ip is %s, port is %d\n", clientIP, clientPort);

        run_audio_client(clientIP);

        ao_init();
        while (1)
        {
            if (ao_push(ao_read, &cfd) == -1)
            {
                printf("ao push failed\n");
                break;
            }
        }
        ao_deinit();

        close(cfd);

        exit_audio_client();
    }
    close(fd);
    fd = -1;
    state = STATE_IDLE;
    printf("audio server exit\n");
}

int exit_audio_server(void)
{
    if (state == STATE_RUNNING)
        state = STATE_EXIT;

    return 0;
}

int run_audio_server(void)
{
    pthread_t tid;
    int ret;

    ret = audio_server_init();
    if (ret < 0)
    {
        printf("audio server init failed\n");
        return ret;
    }

    ret = pthread_create(&tid, NULL, audio_server, NULL);
    if (ret < 0)
    {
        close(fd);
        fd = -1;
        state = STATE_IDLE;
        printf("audio server start failed\n");
    }

    return ret;
}


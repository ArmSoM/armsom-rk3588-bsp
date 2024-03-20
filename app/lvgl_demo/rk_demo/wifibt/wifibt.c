#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bt.h"
#include "Rk_wifi.h"
#include "wifibt.h"

#define wifibt_log(fmt, ...)    printf("[WIFIBT] "fmt, ##__VA_ARGS__)

static int fd[2];
static sem_t sem;

int wifibt_send_wait(void *buf, int len)
{
    struct wifibt_cmdarg *cmdarg = (struct wifibt_cmdarg *)buf;

    cmdarg->wait = true;
    int ret = write(fd[1], buf, len);
    if (ret <= 0)
        return ret;
    sem_wait(&sem);

    return ret;
}

int wifibt_send(void *buf, int len)
{
    struct wifibt_cmdarg *cmdarg = (struct wifibt_cmdarg *)buf;

    cmdarg->wait = false;

    return write(fd[1], buf, len);
}

static void *wifibt_server(void *arg)
{
    struct wifibt_cmdarg *cmdarg;
    intptr_t intval;
    char buf[1024];
    char **key;
    int len;

    if (RK_wifi_enable(1) < 0)
        printf("RK_wifi_enable 1 fail!\n");

    bt_ble_init();

    while (1)
    {
        int num = read(fd[0], buf, sizeof(buf));
        cmdarg = (struct wifibt_cmdarg *)buf;
        switch (cmdarg->cmd)
        {
        case WIFI_ENABLE:
            wifibt_log("WIFI_ENABLE\n");
            RK_wifi_enable(1);
            break;
        case WIFI_DISABLE:
            wifibt_log("WIFI_DISABLE\n");
            RK_wifi_enable(0);
            break;
        case WIFI_SCAN:
            wifibt_log("WIFI_SCAN\n");
            RK_wifi_scan();
            break;
        case WIFI_CONNECT:
            wifibt_log("WIFI_CONNECT\n");
            key = cmdarg->val;
            RK_wifi_connect(key[0], key[1]);
            break;
        case WIFI_DISCONNECT:
            wifibt_log("WIFI_DISCONNECT\n");
            RK_wifi_disconnect_network();
            break;
        case BT_ENABLE:
            wifibt_log("BT_ENABLE\n");
            bt_ble_init();
            break;
        case BT_DISABLE:
            wifibt_log("BT_DISABLE\n");
            bt_ble_deinit();
            break;
        case BT_SINK_ENABLE:
            wifibt_log("BT_SINK_ENABLE\n");
            bt_sink_enable();
            break;
        case BT_SINK_DISABLE:
            wifibt_log("BT_SINK_DISABLE\n");
            rk_bt_sink_pause();
            rk_bt_sink_disconnect();
            bt_sink_disable();
            break;
        case BT_SINK_PLAY:
            wifibt_log("BT_SINK_PLAY\n");
            rk_bt_sink_play();
            break;
        case BT_SINK_PAUSE:
            wifibt_log("BT_SINK_PAUSE\n");
            rk_bt_sink_pause();
            break;
        case BT_SINK_PREV:
            wifibt_log("BT_SINK_PREV\n");
            rk_bt_sink_prev();
            break;
        case BT_SINK_NEXT:
            wifibt_log("BT_SINK_NEXT\n");
            rk_bt_sink_next();
            break;
        case BT_SINK_VOL_UP:
            wifibt_log("BT_SINK_VOL_UP\n");
            rk_bt_sink_volume_up();
            break;
        case BT_SINK_VOL_DOWN:
            wifibt_log("BT_SINK_VOL_DOWN\n");
            rk_bt_sink_volume_down();
            break;
        case BT_SINK_VOL:
            intval = (intptr_t)cmdarg->val;
            wifibt_log("BT_SINK_VOL %d\n", intval);
            rk_bt_sink_set_volume(intval);
            break;
        case BT_SINK_MUTE:
            wifibt_log("BT_SINK_MUTE\n");
            rk_bt_sink_set_volume(0);
            break;
        case BT_INFO:
            bt_sink_info((struct bt_info *)cmdarg->val);
            break;
        case BT_SINK_TRACK_CLEAR:
            bt_sink_track_clear();
            break;
        case BT_SINK_POS_CLEAR:
            bt_sink_pos_clear();
            break;
        default:
            wifibt_log("Unknow cmd %d\n", cmdarg->cmd);
            break;
        }
        if (cmdarg->wait)
            sem_post(&sem);
    }
}

int run_wifibt_server(void)
{
    pthread_t tid;
    int ret;

    sem_init(&sem, 0, 1);

    ret = pipe(fd);
    if (ret != 0)
    {
        wifibt_log("wifibt server init failed\n");
        return ret;
    }

    ret = pthread_create(&tid, NULL, wifibt_server, NULL);
    if (ret < 0)
    {
        close(fd[0]);
        close(fd[1]);
        wifibt_log("wifibt server start failed\n");
    }

    return ret;
}


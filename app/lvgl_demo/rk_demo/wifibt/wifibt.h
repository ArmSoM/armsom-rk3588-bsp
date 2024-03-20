#ifndef __WIFIBT_H__
#define __WIFIBT_H__

#include <RkBtBase.h>
#include <RkBtSink.h>
#include <RkBle.h>

enum
{
    WIFI_ENABLE,
    WIFI_DISABLE,
    WIFI_SCAN,
    WIFI_CONNECT,
    WIFI_DISCONNECT,
    BT_ENABLE,
    BT_DISABLE,
    BT_SINK_ENABLE,
    BT_SINK_DISABLE,
    BT_SINK_PLAY,
    BT_SINK_PAUSE,
    BT_SINK_PREV,
    BT_SINK_NEXT,
    BT_SINK_VOL_UP,
    BT_SINK_VOL_DOWN,
    BT_SINK_VOL,
    BT_SINK_MUTE,
    BT_SINK_TRACK_CLEAR,
    BT_SINK_POS_CLEAR,
    BT_INFO,
};

struct wifibt_cmdarg
{
    int cmd;
    bool wait;
    void *val;
};

struct bt_info
{
    RK_BT_STATE bt_state;
    RK_BT_SINK_STATE sink_state;
    RK_BT_BOND_STATE bond_state;
    bool sink_started;
    bool track_changed;
    bool pos_changed;
    char title[256];
    char artist[256];
    int pos;
    int time;
    int vol;
};

int wifibt_send(void *buf, int len);
int wifibt_send_wait(void *buf, int len);
int run_wifibt_server(void);

#endif


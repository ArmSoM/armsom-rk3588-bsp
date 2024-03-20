#ifndef __BT_H__
#define __BT_H__

#include <RkBtSink.h>

#include "wifibt.h"

void bt_sink_info(struct bt_info *info);
int bt_ble_init(void);
int bt_ble_deinit(void);
int bt_sink_enable(void);
int bt_sink_disable(void);
void bt_sink_track_clear(void);
void bt_sink_pos_clear(void);

#endif


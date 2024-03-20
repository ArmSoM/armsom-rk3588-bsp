#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "main.h"
#include "cJSON.h"
#include "Rk_wifi.h"
#include "ui_resource.h"

static int init_done = 0;

static lv_obj_t *bg;
static lv_obj_t *item_scan;
static lv_obj_t *item_scan_icon;

static lv_obj_t *part_switch;
static lv_obj_t *part_saved;
static lv_obj_t *part_scaned;

static lv_obj_t *wifi_label;
static lv_obj_t *wifi_switch;

static lv_obj_t *item_label_saved;
static lv_obj_t *item_label_scaned;

static lv_obj_t *item_list_saved;
static lv_obj_t *item_list_scaned;

static lv_obj_t *kb;

static lv_timer_t *timer;

static lv_anim_t icon_anim;

static lv_style_t style_txt;
static lv_style_t style_list;

static cJSON *aps = NULL;

static volatile bool rkwifi_gonff = false;
static RK_WIFI_RUNNING_State_e wifi_state = 0;
static int wifi_result = 0;

static void lv_saved_wifi_list(lv_obj_t *parent);
static void connect_wifi(lv_event_t *e);

int wifi_connected(void)
{
    return wifi_state == RK_WIFI_State_CONNECTED ||
           wifi_state == RK_WIFI_State_DHCP_OK;
}

static void read_saved_wifi(int check)
{
    RK_WIFI_SAVED_INFO_s *wsi;
    int ap_cnt = 0;

    RK_wifi_getSavedInfo(&wsi, &ap_cnt);
    if (ap_cnt <= 0)
    {
        printf("not found saved ap!\n");
        return;
    }

    if (!check)
        return;

    lv_obj_clean(item_list_saved);
    for (int i = 1; i < ap_cnt; i++)
    {
        lv_obj_t *btn;
        char *ssid, *bssid;
        ssid = wsi[i].ssid;
        bssid = wsi[i].bssid;
        printf("id: %d, name: %s, bssid: %s, state: %s\n",
               wsi[i].id,
               wsi[i].ssid,
               wsi[i].bssid,
               wsi[i].state);
        btn = lv_list_add_btn(item_list_saved, NULL,
                              (ssid && (strlen(ssid) > 0)) ? ssid : bssid);
        lv_obj_add_event_cb(btn, connect_wifi, LV_EVENT_CLICKED, btn);
        if (strcmp(wsi[i].state, "[CURRENT]") == 0)
        {
            lv_obj_t *label;
            label = lv_list_add_text(item_list_saved, "已连接");
            lv_obj_remove_style_all(label);
            lv_obj_add_style(label, &style_txt, LV_PART_MAIN);
            lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(label, LV_OBJ_FLAG_IGNORE_LAYOUT);
            lv_obj_align_to(label, btn, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }

    if (wsi != NULL)
        free(wsi);
}

static void printf_connect_info(RK_WIFI_INFO_Connection_s *info)
{
    if (!info)
        return;

    printf("	id: %d\n", info->id);
    printf("	bssid: %s\n", info->bssid);
    printf("	ssid: %s\n", info->ssid);
    printf("	freq: %d\n", info->freq);
    printf("	mode: %s\n", info->mode);
    printf("	wpa_state: %s\n", info->wpa_state);
    printf("	ip_address: %s\n", info->ip_address);
    printf("	mac_address: %s\n", info->mac_address);
}

static int rk_wifi_state_callback(RK_WIFI_RUNNING_State_e state, RK_WIFI_INFO_Connection_s *info)
{
    printf("%s state: %d\n", __func__, state);

    if (state != RK_WIFI_State_SCAN_RESULTS)
        wifi_state = state;

    switch (state)
    {
    case RK_WIFI_State_IDLE:
        break;
    case RK_WIFI_State_CONNECTING:
        break;
    case RK_WIFI_State_CONNECTFAILED:
        printf("RK_WIFI_State_CONNECTFAILED\n");
        break;
    case RK_WIFI_State_CONNECTFAILED_WRONG_KEY:
        printf("RK_WIFI_State_CONNECTFAILED_WRONG_KEY\n");
        break;
    case RK_WIFI_State_CONNECTED:
        printf("RK_WIFI_State_CONNECTED\n");
        //printf_connect_info(info);
        //RK_wifi_get_connected_ap_rssi();
        break;
    case RK_WIFI_State_DISCONNECTED:
        printf("RK_WIFI_State_DISCONNECTED\n");
        break;
    case RK_WIFI_State_OPEN:
        rkwifi_gonff = true;
        printf("RK_WIFI_State_OPEN\n");
        break;
    case RK_WIFI_State_OFF:
        rkwifi_gonff = false;
        printf("RK_WIFI_State_OFF\n");
        break;
    case RK_WIFI_State_SCAN_RESULTS:
        char *scan_r;
        printf("RK_WIFI_State_SCAN_RESULTS\n");
        scan_r = RK_wifi_scan_r();
        if (aps)
            cJSON_Delete(aps);
        aps = cJSON_Parse(scan_r);
        wifi_result = 1;
        free(scan_r);
        break;
    case RK_WIFI_State_DHCP_OK:
        printf("RK_WIFI_State_DHCP_OK\n");
        break;
    }

    return 0;
}

static void style_init(void)
{
    lv_style_init(&style_txt);
    lv_style_set_text_font(&style_txt, ttf_main_s.font);
    lv_style_set_text_color(&style_txt, lv_color_make(0xff, 0x23, 0x23));

    lv_style_init(&style_list);
    lv_style_set_text_font(&style_list, ttf_main_m.font);
    lv_style_set_text_color(&style_list, lv_color_black());
}

static void event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *ibox = lv_obj_get_parent(obj);
    const char *ssid;
    const char *psk;

    if (strcmp(lv_inputbox_get_active_btn_text(ibox), "确认") == 0)
    {
        ssid = lv_list_get_btn_text(item_list_scaned, lv_event_get_user_data(e));
        psk = lv_textarea_get_text(lv_inputbox_get_text_area(ibox));
        printf("connect %s, %s\n", ssid, psk);

        if (RK_wifi_connect((char *)ssid, (char *)psk) < 0)
            printf("RK_wifi_connect1 fail!\n");

        read_saved_wifi(init_done);
    }
    lv_msgbox_close(ibox);
    lv_obj_del(kb);
}

static void connect_wifi(lv_event_t *e)
{
    char title[128];
    static const char *btns[] = {"确认", "取消", ""};

    lv_obj_t *btn = lv_event_get_user_data(e);
    printf("try connect %s\n", lv_list_get_btn_text(item_list_scaned, btn));
    snprintf(title, sizeof(title), "连接到%s", lv_list_get_btn_text(item_list_scaned, btn));

    lv_obj_t *ibox = lv_inputbox_create(NULL, title, "请输入密码", btns, false);
    lv_obj_add_event_cb(ibox, event_cb, LV_EVENT_VALUE_CHANGED, btn);
    lv_obj_add_style(ibox, &style_txt, LV_PART_MAIN);
    lv_obj_set_size(ibox, lv_pct(80), lv_pct(20));
    lv_obj_align(ibox, LV_ALIGN_TOP_MID, 0, lv_pct(15));

    kb = lv_keyboard_create(lv_layer_sys());
    lv_obj_set_size(kb, lv_pct(100), lv_pct(30));
    lv_obj_set_align(kb, LV_ALIGN_BOTTOM_MID);
    lv_keyboard_set_textarea(kb, lv_inputbox_get_text_area(ibox));
}

static void icon_anim_end(lv_anim_t *anim)
{
    if (!init_done)
        return;

    lv_obj_clear_flag(item_scan, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(item_scan_icon, LV_OBJ_FLAG_HIDDEN);

    if (aps)
    {
        cJSON *sub;
        lv_obj_clean(item_list_scaned);
        cJSON_ArrayForEach(sub, aps)
        {
            lv_obj_t *btn;
            char *ssid;
            char *bssid;

            ssid = cJSON_GetStringValue(
                       cJSON_GetObjectItem(sub, "ssid"));
            bssid = cJSON_GetStringValue(
                        cJSON_GetObjectItem(sub, "bssid"));

            btn = lv_list_add_btn(item_list_scaned, NULL,
                                  (ssid && (strlen(ssid) > 0)) ? ssid : bssid);
            lv_obj_add_event_cb(btn, connect_wifi, LV_EVENT_CLICKED, btn);
        }
    }
}

static void icon_anim_cb(void *var, int32_t v)
{
    lv_img_set_angle(var, v);
}

static void scan_btn_cb(lv_event_t *e)
{
    lv_obj_add_flag(item_scan, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(item_scan_icon, LV_OBJ_FLAG_HIDDEN);
    lv_anim_start(&icon_anim);
    RK_wifi_scan();
}

static void label_drawed_cb(lv_event_t *e)
{
    if (!item_scan)
        return;

    lv_obj_align_to(item_scan, lv_event_get_target(e),
                    LV_ALIGN_OUT_TOP_RIGHT, 0, 0);
}

static void wifi_update(lv_timer_t *timer)
{
    static RK_WIFI_RUNNING_State_e last_state = 0;

    if (last_state != wifi_state)
    {
        last_state = wifi_state;
        switch (last_state)
        {
        case RK_WIFI_State_DHCP_OK:
        case RK_WIFI_State_CONNECTED:
            read_saved_wifi(init_done);
            break;
        default:
            break;
        }
    }
    if (wifi_result)
    {
        lv_anim_del(&icon_anim, NULL);
        wifi_result = 0;
    }
}

lv_obj_t *menu_wifi_init(lv_obj_t *parent)
{
    RK_wifi_register_callback(rk_wifi_state_callback);

    style_init();

    bg = lv_img_create(parent);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(bg, LV_FLEX_FLOW_COLUMN);
    lv_obj_center(bg);

    part_switch = lv_obj_create(bg);
    //lv_obj_remove_style_all(part_switch);
    lv_obj_set_width(part_switch, lv_pct(100));
    lv_obj_set_height(part_switch, LV_SIZE_CONTENT);
    lv_obj_add_style(part_switch, &style_txt, LV_PART_MAIN);

    wifi_label = lv_label_create(part_switch);
    lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(wifi_label, "WIFI");
    lv_obj_add_style(wifi_label, &style_txt, LV_PART_MAIN);
    wifi_switch = lv_switch_create(part_switch);
    lv_obj_align(wifi_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_state(wifi_switch, LV_STATE_CHECKED | LV_STATE_DISABLED);

    item_label_saved = lv_label_create(bg);
    lv_label_set_text(item_label_saved, "已保存网络");
    lv_obj_add_style(item_label_saved, &style_txt, LV_PART_MAIN);

    part_saved = lv_obj_create(bg);
    lv_obj_remove_style_all(part_saved);
    lv_obj_set_width(part_saved, lv_pct(100));
    lv_obj_set_height(part_saved, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(part_saved, LV_FLEX_FLOW_COLUMN);

    item_label_scaned = lv_label_create(bg);
    lv_label_set_text(item_label_scaned, "可用网络");
    lv_obj_add_style(item_label_scaned, &style_txt, LV_PART_MAIN);

    part_scaned = lv_obj_create(bg);
    lv_obj_remove_style_all(part_scaned);
    lv_obj_set_width(part_scaned, lv_pct(100));
    lv_obj_set_height(part_scaned, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(part_scaned, LV_FLEX_FLOW_COLUMN);

    lv_obj_add_event_cb(part_scaned, label_drawed_cb, LV_EVENT_DRAW_POST_END, NULL);

    item_list_saved = lv_list_create(part_saved);
    lv_obj_set_size(item_list_saved, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(item_list_saved, &style_list, LV_PART_MAIN);
    lv_list_add_btn(item_list_saved, NULL, "无");

    item_list_scaned = lv_list_create(part_scaned);
    lv_obj_set_size(item_list_scaned, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(item_list_scaned, &style_list, LV_PART_MAIN);
    lv_list_add_btn(item_list_scaned, NULL, "无");

    read_saved_wifi(1);

    item_scan = lv_label_create(bg);
    lv_label_set_text(item_scan, "刷新");
    lv_obj_add_style(item_scan, &style_txt, LV_PART_MAIN);
    lv_obj_align_to(item_scan, part_scaned, LV_ALIGN_OUT_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(item_scan, scan_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(item_scan, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_IGNORE_LAYOUT);

    item_scan_icon = lv_img_create(bg);
    lv_img_set_src(item_scan_icon, WIFI_SCANNING);
    lv_img_set_angle(item_scan_icon, 0);
    lv_obj_align_to(item_scan_icon, item_scan, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(item_scan_icon, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_IGNORE_LAYOUT);

    lv_anim_init(&icon_anim);
    lv_anim_set_var(&icon_anim, item_scan_icon);
    lv_anim_set_values(&icon_anim, 0, 3600);
    lv_anim_set_time(&icon_anim, 1000);
    lv_anim_set_exec_cb(&icon_anim, icon_anim_cb);
    lv_anim_set_path_cb(&icon_anim, lv_anim_path_linear);
    lv_anim_set_deleted_cb(&icon_anim, icon_anim_end);
    lv_anim_set_repeat_count(&icon_anim, 3/*LV_ANIM_REPEAT_INFINITE*/);
    scan_btn_cb(NULL);

    timer = lv_timer_create(wifi_update, 1000, NULL);
    lv_timer_enable(timer);

    init_done = 1;

    return bg;
}

void menu_wifi_deinit(void)
{
    init_done = 0;
    lv_timer_del(timer);
    lv_anim_set_deleted_cb(&icon_anim, NULL);
    lv_anim_set_exec_cb(&icon_anim, NULL);
    lv_anim_del(&icon_anim, NULL);
    lv_obj_del(bg);
    bg = NULL;
}


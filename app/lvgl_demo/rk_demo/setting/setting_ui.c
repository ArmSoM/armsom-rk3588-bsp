#include <lvgl/lvgl.h>

#include "about_ui.h"
#include "bt_ui.h"
#include "display_ui.h"
#include "home_ui.h"
#include "language_and_date_ui.h"
#include "main.h"
#include "volume_ui.h"
#include "ui_resource.h"
#include "wallpaper_ui.h"
#include "wifi_ui.h"

enum
{
    SUBMENU_MIN = 0,
    SUBMENU_WIFI = SUBMENU_MIN,
    SUBMENU_BT,
    SUBMENU_DISPLAY,
    SUBMENU_VOLUME,
    SUBMENU_WALLPAPER,
    SUBMENU_LANGUAGE_DATE,
    SUBMENU_ABOUT,
    SUBMENU_MAX,
    SUBMENU_DEFAULT = SUBMENU_WIFI,
};

struct submenu_s
{
    char *name;
    void (*init)(void);
    void (*deinit)(void);
    lv_obj_t *menu;
};

static lv_obj_t *ui_screen;
static lv_obj_t *btn_return;
static lv_obj_t *label_menu;
static lv_obj_t *area_menu;
static lv_obj_t *area_content = NULL;

static lv_obj_t *wifi_saved_area;
static lv_obj_t *wifi_scaned;
static lv_obj_t *wifi_saved_area;
static lv_obj_t *wifi_scaned;
static lv_obj_t *wifi_switch;

static lv_style_t *style_cont;

static lv_obj_t *sub_menu[SUBMENU_MAX];
static int cur_menu = SUBMENU_DEFAULT;
static struct submenu_s submenu_desc[SUBMENU_MAX];

#define SUBMENU_COMMON_DEFINE(enum_t, name) \
static void submenu_##name(void)  \
{   \
    if (!submenu_desc[enum_t].menu)\
        submenu_desc[enum_t].menu = menu_##name##_init(area_content);\
}   \
static void submenu_##name##_destroy(void)  \
{   \
    if (submenu_desc[enum_t].menu)\
        menu_##name##_deinit();\
}

SUBMENU_COMMON_DEFINE(SUBMENU_WIFI, wifi)
SUBMENU_COMMON_DEFINE(SUBMENU_BT, bt)
SUBMENU_COMMON_DEFINE(SUBMENU_DISPLAY, display)
SUBMENU_COMMON_DEFINE(SUBMENU_VOLUME, volume)
SUBMENU_COMMON_DEFINE(SUBMENU_WALLPAPER, wallpaper)
SUBMENU_COMMON_DEFINE(SUBMENU_LANGUAGE_DATE, language)
SUBMENU_COMMON_DEFINE(SUBMENU_ABOUT, about)

static struct submenu_s submenu_desc[SUBMENU_MAX] =
{
    {"WIFI",        submenu_wifi,    submenu_wifi_destroy, NULL},
    {"蓝牙",        submenu_bt,      submenu_bt_destroy,   NULL},
    {"显示和亮度",  submenu_display, submenu_display_destroy, NULL},
    {"音量",        submenu_volume,  submenu_volume_destroy, NULL},
    {"锁屏和壁纸",  submenu_wallpaper,  submenu_wallpaper_destroy, NULL},
    {"语言和日期",  submenu_language,  submenu_language_destroy, NULL},
    {"关于",        submenu_about,  submenu_about_destroy, NULL}
};

static void style_init(void)
{
    if (style_cont)
        return;

    style_cont = malloc(sizeof(style_cont));
    lv_style_init(style_cont);
    lv_style_set_text_font(style_cont, ttf_main_m.font);
    lv_style_set_text_color(style_cont, lv_color_black());
    lv_style_set_radius(style_cont, 10);
    lv_style_set_pad_left(style_cont, 10);
    lv_style_set_pad_right(style_cont, 10);
    lv_style_set_pad_top(style_cont, 10);
    lv_style_set_pad_bottom(style_cont, 10);
}

static void menu_switch_cb(lv_event_t *e)
{
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);

    if (idx < SUBMENU_MIN || idx >= SUBMENU_MAX)
        return;

    if ((cur_menu >= SUBMENU_MIN)
            && (cur_menu < SUBMENU_MAX)
            && submenu_desc[cur_menu].menu)
    {
        lv_obj_add_flag(submenu_desc[cur_menu].menu,
                        LV_OBJ_FLAG_HIDDEN);
    }

    if (submenu_desc[idx].init)
        submenu_desc[idx].init();

    cur_menu = idx;
    if (submenu_desc[cur_menu].menu)
    {
        lv_obj_clear_flag(submenu_desc[cur_menu].menu,
                          LV_OBJ_FLAG_HIDDEN);
    }
}

static void btn_drawed_cb(lv_event_t *e)
{
    switch (e->code)
    {
    case LV_EVENT_CLICKED:
        home_ui_init();
        for (int i = SUBMENU_MIN; i < SUBMENU_MAX; i++)
        {
            if (submenu_desc[i].deinit)
                submenu_desc[i].deinit();
        }
        lv_obj_del(ui_screen);
        ui_screen = NULL;
        area_content = NULL;
        label_menu = NULL;
        break;
    case LV_EVENT_DRAW_POST_END:
        if (!label_menu)
            return;
        lv_obj_align_to(label_menu, btn_return,
                        LV_ALIGN_OUT_RIGHT_MID,
                        5, 0);
        break;
    default:
        break;
    }
}

void setting_ui_init(void)
{
    style_init();

    if (ui_screen)
        goto load;

    ui_screen = lv_obj_create(NULL);

    lv_obj_clear_flag(ui_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_img_opa(ui_screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    btn_return = lv_img_create(ui_screen);
    lv_obj_set_pos(btn_return, 10, 10);
    lv_img_set_src(btn_return, IMG_RETURN_BTN);
    lv_obj_add_flag(btn_return, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_return, btn_drawed_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_return, btn_drawed_cb, LV_EVENT_DRAW_POST_END, NULL);

    label_menu = lv_label_create(ui_screen);
    lv_label_set_text(label_menu, "系统设置");
    lv_obj_add_style(label_menu, &style_txt_m, LV_PART_MAIN);
    lv_obj_align_to(label_menu, btn_return,
                    LV_ALIGN_OUT_RIGHT_MID,
                    5, 0);

    area_menu = lv_list_create(ui_screen);
    lv_obj_set_size(area_menu, lv_pct(30), LV_SIZE_CONTENT);
    lv_obj_set_pos(area_menu, lv_pct(5), lv_pct(5));
    lv_obj_add_style(area_menu, style_cont, LV_PART_MAIN);

    area_content = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(area_content);
    lv_obj_set_size(area_content, lv_pct(55), lv_pct(95));
    lv_obj_set_pos(area_content, lv_pct(40), lv_pct(5));
    lv_obj_set_flex_flow(area_content, LV_FLEX_FLOW_COLUMN);

    for (intptr_t i = SUBMENU_MIN; i < SUBMENU_MAX; i++)
    {
        sub_menu[i] = lv_list_add_btn(area_menu, NULL, submenu_desc[i].name);
        lv_obj_add_event_cb(sub_menu[i], menu_switch_cb, LV_EVENT_CLICKED, (void *)i);
        submenu_desc[i].menu = NULL;
    }

    submenu_desc[SUBMENU_DEFAULT].init();
    cur_menu = SUBMENU_DEFAULT;
load:
    lv_disp_load_scr(ui_screen);
}


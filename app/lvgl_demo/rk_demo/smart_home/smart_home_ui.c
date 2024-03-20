#include <lvgl/lvgl.h>

#include "control_ui.h"
#include "home_ui.h"
#include "info_ui.h"
#include "main.h"
#include "music_ui.h"
#include "ui_resource.h"

enum
{
    SUBMENU_MIN = 0,
    SUBMENU_INFO = SUBMENU_MIN,
    SUBMENU_CONTROL,
    SUBMENU_MUSIC,
    SUBMENU_MAX,
    SUBMENU_DEFAULT = SUBMENU_INFO,
};

struct submenu_s
{
    char *name;
    void (*init)(lv_obj_t *parent);
    void (*deinit)(void);
    lv_obj_t *menu;
};

static lv_obj_t *ui_screen;
static lv_obj_t *bg_pic;
static lv_obj_t *btn_return;
static lv_obj_t *label_menu;
static lv_obj_t *area_submenu;

static lv_img_dsc_t *bg_snapshot;

static lv_style_t *style_cont;

static lv_obj_t *sub_menu[SUBMENU_MAX];
static struct submenu_s submenu_desc[SUBMENU_MAX];

#define SUBMENU_COMMON_DEFINE(enum_t, name) \
static void submenu_##name(lv_obj_t * parent)  \
{   \
    if (!submenu_desc[enum_t].menu)\
        submenu_desc[enum_t].menu = menu_##name##_init(parent);\
}   \
static void submenu_##name##_destroy(void)  \
{   \
    if (submenu_desc[enum_t].menu)\
        menu_##name##_deinit();\
}

SUBMENU_COMMON_DEFINE(SUBMENU_INFO, info)
SUBMENU_COMMON_DEFINE(SUBMENU_CONTROL, control)
SUBMENU_COMMON_DEFINE(SUBMENU_MUSIC, music)

static struct submenu_s submenu_desc[SUBMENU_MAX] =
{
    {"首页",   submenu_info,    submenu_info_destroy,    NULL},
    {"控制",   submenu_control, submenu_control_destroy, NULL},
    {"播放器", submenu_music,   submenu_music_destroy,   NULL}
};

static void bg_pic_snapshot_blur(void)
{
    lv_draw_rect_dsc_t dsc;

    bg_snapshot = lv_snapshot_take(bg_pic, LV_IMG_CF_TRUE_COLOR);

    lv_obj_t *canvas = lv_canvas_create(NULL);
    lv_area_t area;
    lv_canvas_set_buffer(canvas, (void *)bg_snapshot->data,
                         bg_snapshot->header.w,
                         bg_snapshot->header.h,
                         bg_snapshot->header.cf);
    area.x1 = 0;
    area.y1 = 0;
    area.x2 = bg_snapshot->header.w - 1;
    area.y2 = bg_snapshot->header.h - 1;
    lv_canvas_blur_ver(canvas, &area, 100);
    lv_canvas_blur_hor(canvas, &area, 100);
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = 70;
    dsc.bg_color = lv_color_black();
    lv_canvas_draw_rect(canvas, 0, 0,
                        bg_snapshot->header.w,
                        bg_snapshot->header.h, &dsc);
    lv_obj_del(canvas);
}

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
            submenu_desc[i].menu = NULL;
        }
        lv_obj_del(ui_screen);
        ui_screen = NULL;
        label_menu = NULL;
        lv_snapshot_free(bg_snapshot);
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

lv_img_dsc_t *smart_home_ui_bg_blur(void)
{
    return bg_snapshot;
}

void smart_home_ui_init(void)
{
    lv_obj_t *obj;

    style_init();

    if (ui_screen)
        goto load;

    ui_screen = lv_obj_create(NULL);

    lv_obj_clear_flag(ui_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_img_opa(ui_screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    bg_pic = lv_img_create(ui_screen);
    lv_obj_set_pos(bg_pic, 0, 0);
    lv_img_set_src(bg_pic, BG_PIC_0);
    bg_pic_snapshot_blur();

    btn_return = lv_img_create(ui_screen);
    lv_obj_set_pos(btn_return, 10, 10);
    lv_img_set_src(btn_return, IMG_RETURN_BTN);
    lv_obj_add_flag(btn_return, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_return, btn_drawed_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_return, btn_drawed_cb, LV_EVENT_DRAW_POST_END, NULL);

    label_menu = lv_label_create(ui_screen);
    lv_label_set_text(label_menu, "智能家居");
    lv_obj_set_style_text_color(label_menu, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_style(label_menu, &style_txt_m, LV_PART_MAIN);
    lv_obj_align_to(label_menu, btn_return,
                    LV_ALIGN_OUT_RIGHT_MID,
                    5, 0);

    area_submenu = lv_tileview_create(ui_screen);
    lv_obj_remove_style_all(area_submenu);
    lv_obj_set_size(area_submenu, lv_pct(100), lv_pct(90));
    lv_obj_set_pos(area_submenu, 0, lv_pct(10));
    for (int i = SUBMENU_MIN; i < SUBMENU_MAX; i++)
    {
        obj = lv_tileview_add_tile(area_submenu, i, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
        submenu_desc[i].init(obj);
    }

load:
    lv_disp_load_scr(ui_screen);
}


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "main.h"
#include "ui_resource.h"

static lv_obj_t *bg;

static lv_obj_t *area_screen_timeout;
static lv_obj_t *area_locked_screen;
static lv_obj_t *area_wallpaper;

static char *wallpapers[4] =
{
    WALLPAPER_0,
    WALLPAPER_1,
    WALLPAPER_2,
    WALLPAPER_3,
};

lv_obj_t *menu_wallpaper_init(lv_obj_t *parent)
{
    lv_obj_t *obj;
    lv_obj_t *area_depart;

    bg = lv_obj_create(parent);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bg, LV_FLEX_FLOW_COLUMN);
    lv_obj_center(bg);

    area_screen_timeout = lv_obj_create(bg);
    lv_obj_set_size(area_screen_timeout, lv_pct(100), LV_SIZE_CONTENT);
    obj = lv_label_create(area_screen_timeout);
    lv_label_set_text(obj, "锁屏时间");
    lv_obj_add_style(obj, &style_txt_m, LV_PART_MAIN);
    obj = lv_dropdown_create(area_screen_timeout);
    lv_obj_add_style(obj, &style_txt_s, LV_PART_MAIN);
    lv_dropdown_set_options(obj, "60s\n120s\n5mins\nNever");
    lv_dropdown_set_selected(obj, 3);
    lv_obj_align(obj, LV_ALIGN_RIGHT_MID, 0, 0);

    area_locked_screen = lv_obj_create(bg);
    lv_obj_set_size(area_locked_screen, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(area_locked_screen, LV_FLEX_FLOW_COLUMN);

    obj = lv_label_create(area_locked_screen);
    lv_label_set_text(obj, "锁屏");
    lv_obj_add_style(obj, &style_txt_m, LV_PART_MAIN);
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, 0);

    area_depart = lv_obj_create(area_locked_screen);
    lv_obj_remove_style_all(area_depart);
    lv_obj_set_size(area_depart, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(area_depart, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(area_depart, 10, 0);
    for (int i = 0; i < 4; i++)
    {
        obj = lv_img_create(area_depart);
        lv_obj_set_size(obj, 160, 120);
        lv_img_set_src(obj, wallpapers[i]);
    }

    area_wallpaper = lv_obj_create(bg);
    lv_obj_set_size(area_wallpaper, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(area_wallpaper, LV_FLEX_FLOW_COLUMN);

    obj = lv_label_create(area_wallpaper);
    lv_label_set_text(obj, "壁纸");
    lv_obj_add_style(obj, &style_txt_m, LV_PART_MAIN);
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, 0);

    area_depart = lv_obj_create(area_wallpaper);
    lv_obj_remove_style_all(area_depart);
    lv_obj_set_size(area_depart, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(area_depart, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(area_depart, 10, 0);
    for (int i = 0; i < 4; i++)
    {
        obj = lv_img_create(area_depart);
        lv_obj_set_size(obj, 160, 120);
        lv_img_set_src(obj, wallpapers[i]);
    }

    return bg;
}

void menu_wallpaper_deinit(void)
{
    lv_obj_del(bg);
    bg = NULL;
}


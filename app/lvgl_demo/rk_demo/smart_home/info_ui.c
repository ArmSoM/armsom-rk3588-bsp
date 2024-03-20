#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "main.h"
#include "smart_home_ui.h"
#include "ui_resource.h"

static lv_obj_t *bg;

static lv_img_dsc_t *bg_snapshot;

static lv_obj_t *area_date;
static lv_obj_t *area_weather;
static lv_obj_t *area_scene;

static lv_img_dsc_t *bg_img_date;
static lv_img_dsc_t *bg_img_weather;
static lv_img_dsc_t *bg_img_scene;

lv_obj_t *menu_info_init(lv_obj_t *parent)
{
    lv_obj_t *obj;
    lv_obj_t *label;
    int x, y;
    int ofs;

    bg = lv_obj_create(parent);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, lv_pct(100), lv_pct(90));
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(bg);
    lv_obj_refr_size(bg);
    lv_obj_refr_pos(bg);

    bg_snapshot = smart_home_ui_bg_blur();
    bg_img_date = malloc(sizeof(*bg_img_date));
    bg_img_weather = malloc(sizeof(*bg_img_weather));
    bg_img_scene = malloc(sizeof(*bg_img_scene));

    memcpy(bg_img_date, bg_snapshot, sizeof(*bg_img_date));
    memcpy(bg_img_weather, bg_snapshot, sizeof(*bg_img_weather));
    memcpy(bg_img_scene, bg_snapshot, sizeof(*bg_img_scene));

    area_date = lv_img_create(bg);
    lv_obj_set_size(area_date, 300, 300);
    lv_obj_align(area_date, LV_ALIGN_CENTER, -160, -160);
    lv_obj_refr_size(area_date);
    lv_obj_refr_pos(area_date);
    x = lv_obj_get_x(area_date) + lv_obj_get_x(bg);
    y = lv_obj_get_y(area_date) + lv_obj_get_y(bg);
    ofs = (y * bg_img_date->header.w + x)
          * lv_img_cf_get_px_size(bg_img_date->header.cf) / 8;
    bg_img_date->data = bg_snapshot->data + ofs;
    lv_img_set_src(area_date, bg_img_date);

    obj = lv_label_create(area_date);
    lv_label_set_text(obj, "10:32");
    lv_obj_add_style(obj, &style_txt_l, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, -50);
    label = lv_label_create(area_date);
    lv_label_set_text(label, "7月13日 周四");
    lv_obj_add_style(label, &style_txt_m, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(label, obj, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    area_weather = lv_img_create(bg);
    lv_obj_set_size(area_weather, 300, 300);
    lv_obj_align(area_weather, LV_ALIGN_CENTER, 160, -160);
    lv_obj_refr_size(area_weather);
    lv_obj_refr_pos(area_weather);
    x = lv_obj_get_x(area_weather) + lv_obj_get_x(bg);
    y = lv_obj_get_y(area_weather) + lv_obj_get_y(bg);
    ofs = (y * bg_img_weather->header.w + x)
          * lv_img_cf_get_px_size(bg_img_weather->header.cf) / 8;
    bg_img_weather->data = bg_snapshot->data + ofs;
    lv_img_set_src(area_weather, bg_img_weather);

    obj = lv_label_create(area_weather);
    lv_label_set_text(obj, "32℃");
    lv_obj_add_style(obj, &style_txt_l, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, -50);
    label = lv_label_create(area_weather);
    lv_label_set_text(label, "福州   晴");
    lv_obj_add_style(label, &style_txt_m, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(label, obj, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    area_scene = lv_img_create(bg);
    lv_obj_set_size(area_scene, 620, 300);
    lv_obj_clear_flag(area_scene, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(area_scene, LV_ALIGN_CENTER, 0, 160);
    lv_obj_refr_size(area_scene);
    lv_obj_refr_pos(area_scene);
    x = lv_obj_get_x(area_scene) + lv_obj_get_x(bg);
    y = lv_obj_get_y(area_scene) + lv_obj_get_y(bg);
    ofs = (y * bg_img_scene->header.w + x)
          * lv_img_cf_get_px_size(bg_img_scene->header.cf) / 8;
    bg_img_scene->data = bg_snapshot->data + ofs;
    lv_img_set_src(area_scene, bg_img_scene);

    obj = lv_btn_create(area_scene);
    lv_obj_align(obj, LV_ALIGN_CENTER, -150, 0);
    lv_obj_set_size(obj, 150, 150);
    lv_obj_set_style_radius(obj, 75, LV_PART_MAIN);
    obj = lv_label_create(obj);
    lv_label_set_text(obj, "回家");
    lv_obj_add_style(obj, &style_txt_s, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(obj);

    obj = lv_btn_create(area_scene);
    lv_obj_align(obj, LV_ALIGN_CENTER, 150, 0);
    lv_obj_set_size(obj, 150, 150);
    lv_obj_set_style_radius(obj, 75, LV_PART_MAIN);
    obj = lv_label_create(obj);
    lv_label_set_text(obj, "外出");
    lv_obj_add_style(obj, &style_txt_s, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(obj);

    return bg;
}

void menu_info_deinit(void)
{
    lv_obj_del(bg);
    bg = NULL;

    free(bg_img_date);
    free(bg_img_weather);
    free(bg_img_scene);
}


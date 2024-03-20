#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "main.h"
#include "ui_resource.h"
#include "smart_home_ui.h"

static lv_obj_t *bg;

static lv_img_dsc_t *bg_snapshot;

static lv_img_dsc_t *bg_img_aircond_0;
static lv_img_dsc_t *bg_img_aircond_1;
static lv_img_dsc_t *bg_img_light;

static lv_obj_t *area_aircond_0;
static lv_obj_t *area_aircond_1;
static lv_obj_t *area_light;

static lv_obj_t *light_menu(lv_obj_t *parent,
                            lv_img_dsc_t *img)
{
    char *light_name[] =
    {
        "卧室1",
        "卧室2",
        "客厅1",
        "客厅2",
        "客卧1",
        "客卧2",
        "餐厅",
    };
    int light_state[] =
    {
        LV_STATE_CHECKED,
        LV_STATE_DEFAULT,
        LV_STATE_CHECKED,
        LV_STATE_CHECKED,
        LV_STATE_DEFAULT,
        LV_STATE_DEFAULT,
        LV_STATE_DEFAULT
    };
    lv_obj_t *obj;
    lv_obj_t *cont;
    lv_obj_t *depart;
    lv_obj_t *light;
    int x, y;
    int ofs;

    light = lv_img_create(parent);
    lv_obj_set_size(light, lv_pct(100), 150);
    lv_obj_align(light, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_refr_size(light);
    lv_obj_refr_pos(light);
    x = lv_obj_get_x(light) + lv_obj_get_x(bg);
    y = lv_obj_get_y(light) + lv_obj_get_y(bg);
    ofs = (y * img->header.w + x)
          * lv_img_cf_get_px_size(img->header.cf) / 8;
    img->data = bg_snapshot->data + ofs;
    lv_img_set_src(light, img);
    lv_obj_clear_flag(light, LV_OBJ_FLAG_SCROLLABLE);

    obj = lv_label_create(light);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(30));
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(obj, "灯光");
    lv_obj_add_style(obj, &style_txt_m, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);

    depart = lv_obj_create(light);
    lv_obj_remove_style_all(depart);
    lv_obj_set_size(depart, lv_pct(100), lv_pct(50));
    lv_obj_align(depart, LV_ALIGN_BOTTOM_LEFT, 20, -10);
    lv_obj_set_flex_flow(depart, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(depart, 10, 0);

    for (int i = 0; i < sizeof(light_name) / sizeof(light_name[0]); i++)
    {
        cont = lv_obj_create(depart);
        lv_obj_remove_style_all(cont);
        lv_obj_set_size(cont, LV_SIZE_CONTENT, lv_pct(100));

        obj = lv_label_create(cont);
        lv_label_set_text(obj, light_name[i]);
        lv_obj_add_style(obj, &style_txt_m, LV_PART_MAIN);
        lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
        lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, 0);

        obj = lv_switch_create(cont);
        lv_obj_add_state(obj, light_state[i]);
        lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    return light;
}

static lv_obj_t *aircond_menu(lv_obj_t *parent,
                              char *name,
                              lv_img_dsc_t *img)
{
    char *btn_name[] =
    {
        "开启/关闭",
        "制冷模式",
        "制热模式",
        "风力：3"
    };
    lv_obj_t *area_aircond;
    lv_obj_t *area_temp;
    lv_obj_t *area_btn;
    lv_obj_t *area_set;
    lv_obj_t *obj;
    lv_obj_t *temp;
    int x, y;
    int ofs;

    area_aircond = lv_img_create(parent);
    lv_obj_set_size(area_aircond, lv_pct(100), 500);
    lv_obj_refr_size(area_aircond);
    lv_obj_refr_pos(area_aircond);
    x = lv_obj_get_x(area_aircond) + lv_obj_get_x(bg);
    y = lv_obj_get_y(area_aircond) + lv_obj_get_y(bg);
    ofs = (y * img->header.w + x)
          * lv_img_cf_get_px_size(img->header.cf) / 8;
    img->data = bg_snapshot->data + ofs;
    lv_img_set_src(area_aircond, img);
    lv_obj_clear_flag(area_aircond, LV_OBJ_FLAG_SCROLLABLE);

    obj = lv_label_create(area_aircond);
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(obj, name);
    lv_obj_add_style(obj, &style_txt_m, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);

    area_temp = lv_obj_create(area_aircond);
    lv_obj_remove_style_all(area_temp);
    lv_obj_set_size(area_temp, lv_pct(100), lv_pct(50));
    lv_obj_align(area_temp, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(area_temp, LV_OBJ_FLAG_SCROLLABLE);

    area_btn = lv_obj_create(area_aircond);
    lv_obj_remove_style_all(area_btn);
    lv_obj_set_size(area_btn, lv_pct(50), lv_pct(50));
    lv_obj_align(area_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_clear_flag(area_btn, LV_OBJ_FLAG_SCROLLABLE);

    area_set = lv_obj_create(area_aircond);
    lv_obj_remove_style_all(area_set);
    lv_obj_set_size(area_set, lv_pct(50), lv_pct(50));
    lv_obj_align(area_set, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_clear_flag(area_set, LV_OBJ_FLAG_SCROLLABLE);

    temp = lv_label_create(area_temp);
    lv_obj_align(temp, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(temp, "24℃");
    lv_obj_add_style(temp, &style_txt_l, LV_PART_MAIN);
    lv_obj_set_style_text_color(temp, lv_color_white(), LV_PART_MAIN);
    lv_obj_refr_size(temp);
    lv_obj_refr_pos(temp);

    obj = lv_label_create(area_temp);
    lv_label_set_text(obj, "当前室温");
    lv_obj_add_style(obj, &style_txt_s, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_refr_size(obj);
    lv_obj_refr_pos(obj);
    lv_obj_align_to(obj, temp, LV_ALIGN_OUT_LEFT_TOP, 0, 0);

    for (int i = 0; i < 4; i++)
    {
        obj = lv_btn_create(area_btn);
        lv_obj_align(obj, LV_ALIGN_CENTER, 60 * (i % 2 ? 1 : -1), 60 * (i < 2 ? -1 : 1));
        lv_obj_set_size(obj, 100, 100);
        lv_obj_set_style_radius(obj, 50, LV_PART_MAIN);
        obj = lv_label_create(obj);
        lv_label_set_text(obj, btn_name[i]);
        lv_obj_add_style(obj, &style_txt_s, LV_PART_MAIN);
        lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
        lv_obj_center(obj);
    }

    temp = lv_roller_create(area_set);
    lv_roller_set_options(temp, "16\n17\n18\n19\n20\n21\n22\n23\n"
                          "24\n25\n26\n27\n28\n29\n30\n31\n32",
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(temp, 3);
    lv_roller_set_selected(temp, 8, LV_ANIM_OFF);
    lv_obj_align(temp, LV_ALIGN_CENTER, 0, 0);

    obj = lv_label_create(area_set);
    lv_label_set_text(obj, "设置温度");
    lv_obj_add_style(obj, &style_txt_s, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(obj, temp, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    return area_aircond;
}

lv_obj_t *menu_control_init(lv_obj_t *parent)
{
    bg = lv_obj_create(parent);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, lv_pct(90), lv_pct(90));
    lv_obj_center(bg);
    lv_obj_set_flex_flow(bg, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(bg, 30, 0);
    lv_obj_refr_size(bg);
    lv_obj_refr_pos(bg);

    bg_snapshot = smart_home_ui_bg_blur();
    bg_img_aircond_0 = malloc(sizeof(*bg_img_aircond_0));
    bg_img_aircond_1 = malloc(sizeof(*bg_img_aircond_1));
    bg_img_light = malloc(sizeof(*bg_img_light));
    memcpy(bg_img_aircond_0, bg_snapshot, sizeof(*bg_img_aircond_0));
    memcpy(bg_img_aircond_1, bg_snapshot, sizeof(*bg_img_aircond_1));
    memcpy(bg_img_light, bg_snapshot, sizeof(*bg_img_light));

    area_light = light_menu(bg, bg_img_light);

    area_aircond_0 = aircond_menu(bg, "客厅空调", bg_img_aircond_0);

    area_aircond_1 = aircond_menu(bg, "卧室空调", bg_img_aircond_1);

    return bg;
}

void menu_control_deinit(void)
{
    lv_obj_del(bg);
    bg = NULL;

    free(bg_img_aircond_0);
    free(bg_img_aircond_1);
    free(bg_img_light);
}


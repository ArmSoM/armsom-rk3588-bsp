#include <stdio.h>
#include "main.h"
#include "icebox_ui.h"
#include "furniture_control_ui.h"

///////////////////// VARIABLES ////////////////////
static lv_obj_t *ui_icebox_screen = NULL;
static lv_obj_t *ui_icebox_box = NULL;
static lv_obj_t *ui_icebox_text_box = NULL;
static lv_obj_t *text_box1 = NULL;
static lv_obj_t *text_box2 = NULL;
static lv_obj_t *ui_icebox_png_box = NULL;
static lv_obj_t *bg_pic = NULL;
static lv_obj_t *text_bg;
static lv_obj_t *ui_icebox_tem;
static lv_obj_t *ui_Label1;
static lv_obj_t *ui_Label3;
static lv_obj_t *ui_rice;
static lv_obj_t *ui_return;
static lv_img_dsc_t *bg_snapshot;

///////////////////// TEST LVGL SETTINGS ////////////////////

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////

void icebox_page_jump_furniture_control_callback(lv_event_t *event)
{
    printf("icebox_page_jump_furniture_control_callback is into \n");
    furniture_control_ui_init();
    lv_obj_del(ui_icebox_screen);
    free(bg_snapshot);
    ui_icebox_screen = NULL;
}

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

///////////////////// SCREENS ////////////////////
void ui_icebox_screen_init(void)
{
    int x, y;
    int ofs;
    ui_icebox_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_icebox_screen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    bg_pic = lv_img_create(ui_icebox_screen);
    lv_obj_set_pos(bg_pic, 0, 0);
    lv_img_set_src(bg_pic, BG_PIC_0);
    bg_pic_snapshot_blur();

    ui_icebox_box = lv_obj_create(ui_icebox_screen);
    lv_obj_remove_style_all(ui_icebox_box);
    lv_obj_set_width(ui_icebox_box, lv_pct(100));
    lv_obj_set_height(ui_icebox_box, lv_pct(33));
    lv_obj_align(ui_icebox_box, LV_ALIGN_TOP_LEFT, 0, lv_pct(33));
    lv_obj_refr_pos(ui_icebox_box);
    //lv_obj_set_flex_flow(ui_icebox_box, LV_FLEX_FLOW_ROW);//行

    text_bg = lv_img_create(ui_icebox_box);
    lv_obj_set_width(text_bg, lv_pct(100));
    lv_obj_set_height(text_bg, lv_pct(100));
    lv_obj_refr_pos(text_bg);
    x = lv_obj_get_x(text_bg) + lv_obj_get_x(ui_icebox_box);
    y = lv_obj_get_y(text_bg) + lv_obj_get_y(ui_icebox_box);
    ofs = (y * bg_snapshot->header.w + x) * lv_img_cf_get_px_size(bg_snapshot->header.cf) / 8;
    bg_snapshot->data = bg_snapshot->data + ofs;
    lv_img_set_src(text_bg, bg_snapshot);
    lv_obj_clear_flag(text_bg, LV_OBJ_FLAG_SCROLLABLE);

    ui_icebox_text_box = lv_obj_create(text_bg);
    lv_obj_remove_style_all(ui_icebox_text_box);
    lv_obj_set_width(ui_icebox_text_box, lv_pct(66));
    lv_obj_set_height(ui_icebox_text_box, lv_pct(80));
    lv_obj_clear_flag(ui_icebox_text_box, LV_OBJ_FLAG_SCROLLABLE);

    text_box1 = lv_obj_create(ui_icebox_text_box);
    lv_obj_remove_style_all(text_box1);
    lv_obj_set_width(text_box1, lv_pct(100));
    lv_obj_set_height(text_box1, lv_pct(30));
    lv_obj_align(text_box1, LV_ALIGN_TOP_LEFT, 0, 0);

    ui_Label1 = lv_label_create(text_box1);
    lv_obj_set_width(ui_Label1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label1, LV_SIZE_CONTENT);    /// 1
    lv_obj_add_style(ui_Label1, &style_txt_m, LV_PART_MAIN);
    lv_obj_set_align(ui_Label1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label1, "每日食谱-炒饭");

    text_box2 = lv_obj_create(ui_icebox_text_box);
    lv_obj_remove_style_all(text_box2);
    lv_obj_set_width(text_box2, lv_pct(100));
    lv_obj_set_height(text_box2, lv_pct(66));
    lv_obj_align(text_box2, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    ui_Label3 = lv_label_create(text_box2);
    lv_obj_set_width(ui_Label3, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label3, LV_SIZE_CONTENT);    /// 1
    lv_obj_add_style(ui_Label3, &style_txt_m, LV_PART_MAIN);
    lv_obj_set_align(ui_Label3, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label3,
                      "将玉米粒、青豆、胡萝卜入锅一起炒匀，\n加适量的盐调味、滴几滴蚝油炒匀。");

    ui_icebox_png_box = lv_img_create(text_bg);
    lv_obj_set_width(ui_icebox_png_box, lv_pct(30));
    lv_obj_set_height(ui_icebox_png_box, lv_pct(100));
    lv_obj_align(ui_icebox_png_box, LV_ALIGN_TOP_RIGHT, 0, 0);

    ui_rice = lv_img_create(ui_icebox_png_box);
    lv_img_set_src(ui_rice, IMG_RICE);
    lv_obj_set_width(ui_rice, LV_SIZE_CONTENT);   /// 150
    lv_obj_set_height(ui_rice, LV_SIZE_CONTENT);    /// 115
    lv_obj_set_align(ui_rice, LV_ALIGN_CENTER);

    ui_return = lv_img_create(ui_icebox_screen);
    lv_img_set_src(ui_return, IMG_RETURN_BTN);
    lv_obj_set_width(ui_return, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_return, LV_SIZE_CONTENT);    /// 32
    lv_obj_align(ui_return, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_flag(ui_return, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_return, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    if (ui_return != NULL)
    {
        lv_obj_add_event_cb(ui_return, icebox_page_jump_furniture_control_callback, LV_EVENT_CLICKED, NULL);
    }

}

void icebox_ui_init(void)
{
    if (!ui_icebox_screen)
        ui_icebox_screen_init();
    lv_disp_load_scr(ui_icebox_screen);
}

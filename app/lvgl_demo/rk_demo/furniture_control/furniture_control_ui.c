#include <stdio.h>
#include <lvgl/lvgl.h>

#include "furniture_control_ui.h"
#include "home_ui.h"
#include "icebox_ui.h"
#include "main.h"
#include "coffee_machine.h"
#include "player_ui.h"

///////////////////// VARIABLES ////////////////////
extern uint32_t LV_EVENT_GET_COMP_CHILD;

static lv_obj_t *furniture_control_ui_Screen1 = NULL;
static lv_obj_t *furniture_control_ui_box = NULL;
static lv_obj_t *furniture_control_ui_icebox_box = NULL;
static lv_obj_t *furniture_control_ui_player_box = NULL;
static lv_obj_t *furniture_control_ui_coffee_box = NULL;
static lv_obj_t *furniture_control_ui_return;
static lv_obj_t *furniture_control_ui_Label2;
static lv_obj_t *furniture_control_ui_icebox;
static lv_obj_t *furniture_control_ui_player;
static lv_obj_t *furniture_control_ui_coffee_machine;
static lv_obj_t *furniture_control_ui_Label1;
static lv_obj_t *furniture_control_ui_Label4;
static lv_obj_t *furniture_control_ui_Label3;
static lv_obj_t *bg_pic;
static lv_obj_t *v_bg;
static lv_img_dsc_t *bg_snapshot;
///////////////////// TEST LVGL SETTINGS ////////////////////

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
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
    dsc.bg_opa = 20;
    dsc.bg_color = lv_color_black();
    lv_canvas_draw_rect(canvas, 0, 0,
                        bg_snapshot->header.w,
                        bg_snapshot->header.h, &dsc);
    lv_obj_del(canvas);
}

void furniture_control_page_jump_home_callback(lv_event_t *event)
{
    printf("page_jump_return_home_callback is into \n");
    home_ui_init();
    lv_obj_del(furniture_control_ui_Screen1);
    free(bg_snapshot);
    bg_snapshot = NULL;
    furniture_control_ui_Screen1 = NULL;
}

void furniture_control_page_jump_icebox_callback(lv_event_t *event)
{
    printf("furniture_control_page_jump_icebox_callback is into \n");
    icebox_ui_init();
    lv_obj_del(furniture_control_ui_Screen1);
    free(bg_snapshot);
    bg_snapshot = NULL;
    furniture_control_ui_Screen1 = NULL;
}

void furniture_control_page_jump_coffee_machine_callback(lv_event_t *event)
{
    printf("furniture_control_page_jump_coffee_machine_callback is into \n");
    coffee_machine_ui_init();
    lv_obj_del(furniture_control_ui_Screen1);
    free(bg_snapshot);
    bg_snapshot = NULL;
    furniture_control_ui_Screen1 = NULL;
}

void furniture_control_page_jump_player_callback(lv_event_t *event)
{
    printf("furniture_control_page_jump_player_callback is into \n");
    player_ui_init();
    lv_obj_del(furniture_control_ui_Screen1);
    free(bg_snapshot);
    bg_snapshot = NULL;
    furniture_control_ui_Screen1 = NULL;
}


///////////////////// SCREENS ////////////////////

void ui_furniture_control_screen_init(void)
{
    int x, y;
    int ofs;
    furniture_control_ui_Screen1 = lv_obj_create(NULL);
    lv_obj_clear_flag(furniture_control_ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    bg_pic = lv_img_create(furniture_control_ui_Screen1);
    lv_obj_set_pos(bg_pic, 0, 0);
    lv_img_set_src(bg_pic, BG_PIC_0);
    bg_pic_snapshot_blur();

    furniture_control_ui_return = lv_img_create(furniture_control_ui_Screen1);
    lv_img_set_src(furniture_control_ui_return, IMG_RETURN_BTN);
    lv_obj_set_width(furniture_control_ui_return, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(furniture_control_ui_return, LV_SIZE_CONTENT);    /// 32
    lv_obj_align(furniture_control_ui_return, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_flag(furniture_control_ui_return, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(furniture_control_ui_return, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_flag(furniture_control_ui_return, LV_OBJ_FLAG_CLICKABLE);
    if (furniture_control_ui_return != NULL)
    {
        lv_obj_add_event_cb(furniture_control_ui_return, furniture_control_page_jump_home_callback, LV_EVENT_CLICKED, NULL);
    }

    furniture_control_ui_Label2 = lv_label_create(furniture_control_ui_Screen1);
    lv_obj_set_width(furniture_control_ui_Label2, 249);
    lv_obj_set_height(furniture_control_ui_Label2, 26);
    lv_obj_align(furniture_control_ui_Label2, LV_ALIGN_TOP_LEFT, 100, 20);
    lv_obj_add_style(furniture_control_ui_Label2, &style_txt_m, LV_PART_MAIN);
    lv_label_set_text(furniture_control_ui_Label2, "家电显控");

    furniture_control_ui_box = lv_obj_create(furniture_control_ui_Screen1);
    lv_obj_remove_style_all(furniture_control_ui_box);
    lv_obj_set_width(furniture_control_ui_box, lv_pct(100));
    lv_obj_set_height(furniture_control_ui_box, lv_pct(33));
    lv_obj_align(furniture_control_ui_box, LV_ALIGN_TOP_LEFT, 0, lv_pct(33));
    lv_obj_set_flex_flow(furniture_control_ui_box, LV_FLEX_FLOW_ROW);//行

    v_bg = lv_img_create(furniture_control_ui_box);
    lv_obj_set_width(v_bg, lv_pct(100));
    lv_obj_set_height(v_bg, lv_pct(100));
    lv_obj_refr_pos(v_bg);
    x = lv_obj_get_x(v_bg) + lv_obj_get_x(furniture_control_ui_box);
    y = lv_obj_get_y(v_bg) + lv_obj_get_y(furniture_control_ui_box);
    ofs = (y * bg_snapshot->header.w + x) * lv_img_cf_get_px_size(bg_snapshot->header.cf) / 8;
    bg_snapshot->data = bg_snapshot->data + ofs;
    lv_img_set_src(v_bg, bg_snapshot);
    lv_obj_set_flex_flow(v_bg, LV_FLEX_FLOW_ROW);//行
    lv_obj_clear_flag(v_bg, LV_OBJ_FLAG_SCROLLABLE);

    furniture_control_ui_icebox_box = lv_obj_create(v_bg);
    lv_obj_remove_style_all(furniture_control_ui_icebox_box);
    lv_obj_set_width(furniture_control_ui_icebox_box, lv_pct(33));
    lv_obj_set_height(furniture_control_ui_icebox_box, lv_pct(100));
    lv_obj_align(furniture_control_ui_icebox_box, LV_ALIGN_TOP_LEFT, 0, 0);

    furniture_control_ui_icebox = lv_img_create(furniture_control_ui_icebox_box);
    lv_img_set_src(furniture_control_ui_icebox, IMG_ICEBOX);
    lv_obj_set_width(furniture_control_ui_icebox, LV_SIZE_CONTENT);   /// 64
    lv_obj_set_height(furniture_control_ui_icebox, LV_SIZE_CONTENT);    /// 64
    lv_obj_set_align(furniture_control_ui_icebox, LV_ALIGN_CENTER);
    lv_obj_add_flag(furniture_control_ui_icebox, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(furniture_control_ui_icebox, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_flag(furniture_control_ui_icebox, LV_OBJ_FLAG_CLICKABLE);
    if (furniture_control_ui_icebox != NULL)
    {
        lv_obj_add_event_cb(furniture_control_ui_icebox, furniture_control_page_jump_icebox_callback, LV_EVENT_CLICKED, NULL);
    }

    furniture_control_ui_Label1 = lv_label_create(furniture_control_ui_icebox_box);
    lv_obj_set_width(furniture_control_ui_Label1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(furniture_control_ui_Label1, LV_SIZE_CONTENT);    /// 1
    lv_obj_align(furniture_control_ui_Label1, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_style(furniture_control_ui_Label1, &style_txt_m, LV_PART_MAIN);
    lv_label_set_text(furniture_control_ui_Label1, "每日菜谱");


    furniture_control_ui_player_box = lv_obj_create(v_bg);
    lv_obj_remove_style_all(furniture_control_ui_player_box);
    lv_obj_set_width(furniture_control_ui_player_box, lv_pct(33));
    lv_obj_set_height(furniture_control_ui_player_box, lv_pct(100));
    lv_obj_align(furniture_control_ui_player_box, LV_ALIGN_TOP_LEFT, 0, 0);

    furniture_control_ui_player = lv_img_create(furniture_control_ui_player_box);
    lv_img_set_src(furniture_control_ui_player, IMG_PLAYER);
    lv_obj_set_width(furniture_control_ui_player, LV_SIZE_CONTENT);   /// 64
    lv_obj_set_height(furniture_control_ui_player, LV_SIZE_CONTENT);    /// 64
    lv_obj_set_align(furniture_control_ui_player, LV_ALIGN_CENTER);
    lv_obj_add_flag(furniture_control_ui_player, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(furniture_control_ui_player, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_flag(furniture_control_ui_player, LV_OBJ_FLAG_CLICKABLE);
    if (furniture_control_ui_player != NULL)
    {
        lv_obj_add_event_cb(furniture_control_ui_player, furniture_control_page_jump_player_callback, LV_EVENT_CLICKED, NULL);
    }

    furniture_control_ui_Label4 = lv_label_create(furniture_control_ui_player_box);
    lv_obj_set_width(furniture_control_ui_Label4, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(furniture_control_ui_Label4, LV_SIZE_CONTENT);    /// 1
    lv_obj_align(furniture_control_ui_Label4, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_style(furniture_control_ui_Label4, &style_txt_m, LV_PART_MAIN);
    lv_label_set_text(furniture_control_ui_Label4, "宣传视频");

    furniture_control_ui_coffee_box = lv_img_create(v_bg);
    lv_obj_remove_style_all(furniture_control_ui_coffee_box);
    lv_obj_set_width(furniture_control_ui_coffee_box, lv_pct(33));
    lv_obj_set_height(furniture_control_ui_coffee_box, lv_pct(100));
    lv_obj_align(furniture_control_ui_coffee_box, LV_ALIGN_TOP_LEFT, 0, 0);

    furniture_control_ui_coffee_machine = lv_img_create(furniture_control_ui_coffee_box);
    lv_img_set_src(furniture_control_ui_coffee_machine, IMG_COFFEE);
    lv_obj_set_width(furniture_control_ui_coffee_machine, LV_SIZE_CONTENT);   /// 64
    lv_obj_set_height(furniture_control_ui_coffee_machine, LV_SIZE_CONTENT);    /// 64
    lv_obj_set_align(furniture_control_ui_coffee_machine, LV_ALIGN_CENTER);
    lv_obj_add_flag(furniture_control_ui_coffee_machine, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(furniture_control_ui_coffee_machine, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_flag(furniture_control_ui_coffee_machine, LV_OBJ_FLAG_CLICKABLE);
    if (furniture_control_ui_coffee_machine != NULL)
    {
        lv_obj_add_event_cb(furniture_control_ui_coffee_machine, furniture_control_page_jump_coffee_machine_callback, LV_EVENT_CLICKED, NULL);
    }

    furniture_control_ui_Label3 = lv_label_create(furniture_control_ui_coffee_box);
    lv_obj_set_width(furniture_control_ui_Label3, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(furniture_control_ui_Label3, LV_SIZE_CONTENT);    /// 1
    lv_obj_align(furniture_control_ui_Label3, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_style(furniture_control_ui_Label3, &style_txt_m, LV_PART_MAIN);
    lv_label_set_text(furniture_control_ui_Label3, "咖啡机");
}

void furniture_control_ui_init(void)
{
    if (!furniture_control_ui_Screen1)
        ui_furniture_control_screen_init();
    lv_disp_load_scr(furniture_control_ui_Screen1);
}


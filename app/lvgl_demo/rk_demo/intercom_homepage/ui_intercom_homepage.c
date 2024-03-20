#include "home_ui.h"
#include <time.h>
#include "ui_intercom_homepage.h"

static lv_obj_t *ui_back;

lv_obj_t *ui_Screen_intercom_homepage;
lv_obj_t *ui_intercom_call_Label_0;
static lv_obj_t *bg_pic;

extern lv_obj_t *ui_img_circular;
extern lv_font_t Security_alarm;
extern lv_font_t Video_monitor;
extern lv_obj_t *ui_Screen_intercom_call;
extern lv_style_t style_txt_s;
extern lv_style_t style_txt_m;

static void icon_cb(lv_event_t *e);

static struct lv_button_parameter button[] =
{
    {NULL, NULL, 45, 300, "安防报警", -25, -5},
    {NULL, NULL, 175, 300, "视频监控", -25, -5},
    {NULL, NULL, 305, 300, "对讲呼叫", -25, -5},
    {NULL, NULL, 435, 300, "信息", 0, -5},
    {NULL, NULL, 565, 300, "家电控制", -25, -5},
    {NULL, NULL, 45, 550, "留影留言", -25, -5},
    {NULL, NULL, 175, 550, "电梯召唤", -25, -5},
    {NULL, NULL, 305, 550, "呼叫管理员", -30, -5},
    {NULL, NULL, 435, 550, "图片管理", -25, -5},
    {NULL, NULL, 565, 550, "家人留言", -25, -5},
};


void intercom_call_ui_init();
void monitor_ui_init();

static void icon_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    intptr_t type = (intptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED)
    {
        switch (type)
        {
        case 1:
            monitor_ui_init();
            lv_obj_del(ui_Screen_intercom_homepage);
            ui_Screen_intercom_homepage = NULL;
            break;
        case 2:
            intercom_call_ui_init();
            lv_obj_del(ui_Screen_intercom_homepage);
            ui_Screen_intercom_homepage = NULL;
            break;

        default:
            break;
        }
    }
}


static void back_icon_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED)
    {
        home_ui_init();
        lv_obj_del(ui_Screen_intercom_homepage);
        ui_Screen_intercom_homepage = NULL;
    }
}


void function_keys(lv_obj_t *parent, lv_obj_t *referent)
{
    for (intptr_t i = 0; i < 10; i ++)
    {
        button[i].ui_circle = lv_img_create(parent);
        lv_img_set_src(button[i].ui_circle, IMG_INTERCOM_ROUND);
        lv_obj_set_width(button[i].ui_circle, LV_SIZE_CONTENT);
        lv_obj_set_height(button[i].ui_circle, LV_SIZE_CONTENT);
        lv_obj_add_flag(button[i].ui_circle, LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_clear_flag(button[i].ui_circle, LV_OBJ_FLAG_SCROLLABLE);
        lv_img_set_zoom(button[i].ui_circle, 500);
        lv_obj_align_to(button[i].ui_circle, referent, LV_ALIGN_OUT_BOTTOM_LEFT, button[i].x, button[i].y);
        lv_obj_add_flag(button[i].ui_circle, LV_OBJ_FLAG_CLICKABLE);

        button[i].ui_circle_label = lv_label_create(parent);
        lv_obj_set_size(button[i].ui_circle_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align_to(button[i].ui_circle_label, button[i].ui_circle, LV_ALIGN_CENTER, button[i].x_po_verify, button[i].y_po_verify);
        lv_label_set_text(button[i].ui_circle_label, button[i].txt);
        lv_obj_add_event_cb(button[i].ui_circle, icon_cb, LV_EVENT_ALL, (void *)i);
    }

}




void ui_intercom_home_page_screen_init()
{
    ui_Screen_intercom_homepage = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen_intercom_homepage, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_img_opa(ui_Screen_intercom_homepage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_Screen_intercom_homepage, &style_txt_s, LV_PART_MAIN);


    bg_pic = lv_img_create(ui_Screen_intercom_homepage);
    lv_obj_set_pos(bg_pic, 0, 0);
    lv_img_set_src(bg_pic, BG_PIC_0);

    //back img
    ui_back = lv_img_create(ui_Screen_intercom_homepage);
    lv_img_set_src(ui_back, IMG_RETURN_BTN);
    lv_obj_set_width(ui_back, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_back, LV_SIZE_CONTENT);    /// 32
    lv_obj_align(ui_back, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_flag(ui_back, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_back, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_event_cb(ui_back, back_icon_cb, LV_EVENT_ALL, 0);


    ui_intercom_call_Label_0 = lv_label_create(ui_Screen_intercom_homepage);
    lv_label_set_text(ui_intercom_call_Label_0, "楼宇对讲");
    lv_obj_set_style_text_color(ui_intercom_call_Label_0, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_style(ui_intercom_call_Label_0, &style_txt_m, LV_PART_MAIN);
    lv_obj_align_to(ui_intercom_call_Label_0, ui_back,
                    LV_ALIGN_OUT_RIGHT_MID,
                    5, 0);


    function_keys(ui_Screen_intercom_homepage, ui_back);
}

void intercom_homepage_ui_init()
{
    if (!ui_Screen_intercom_homepage)
        ui_intercom_home_page_screen_init();
    lv_disp_load_scr(ui_Screen_intercom_homepage);
}

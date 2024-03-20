#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "home_ui.h"
#include "ui_intercom_homepage.h"
#include "audio_server.h"
#include "local_ip.h"


lv_obj_t *ui_Screen_test;
lv_obj_t *ui_Screen_intercom_call;

static lv_obj_t *ui_back;
static lv_obj_t *bg_pic;
static lv_obj_t *ui_rectangle;
static lv_obj_t *ui_circle[15];
static lv_obj_t *ui_intercom_call_Label_0;
static lv_obj_t *ui_intercom_call_Label_1;
static lv_obj_t *ui_call_break;
static lv_obj_t *ui_ip_label;

static lv_timer_t *timer;

extern lv_obj_t *ui_Screen_intercom_homepage;
extern lv_style_t style_txt_s;
extern lv_style_t style_txt_m;

static void icon_cb(lv_event_t *e);

/*Maximum number of digits*/
#define MAX_DIGITS 15
char digits[MAX_DIGITS + 1] = "";

static struct lv_button_parameter button[] =
{
    {NULL, NULL, 5, 50, "1", 15, -5},
    {NULL, NULL, 145, 50, "2", 15, -5},
    {NULL, NULL, 285, 50, "3", 15, -5},
    {NULL, NULL, 425, 50, "栋号", 0, -5},

    {NULL, NULL, 5, 190, "4", 15, -5},
    {NULL, NULL, 145, 190, "5", 15, -5},
    {NULL, NULL, 285, 190, "6", 15, -5},
    {NULL, NULL, 425, 190, "单元", -5, -5},

    {NULL, NULL, 5, 330, "7", 15, -5},
    {NULL, NULL, 145, 330, "8", 15, -5},
    {NULL, NULL, 285, 330, "9", 15, -5},
    {NULL, NULL, 425, 330, ".", 15, -5},

    {NULL, NULL, 5, 470, "呼叫", 0, -5},
    {NULL, NULL, 145, 470, "0", 15, -5},
    {NULL, NULL, 285, 470, "删除", 0, -5},
    {NULL, NULL, 425, 470, "管理中心", -25, -5},
};


static const char *type_messages[] =
{
    "1", "2", "3", "Buil", "4", "5", "6", "Unit", "7", "8", "9", ".", "call", "0", "Delete", "Manage"
};

static void icon_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    intptr_t type = (intptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED && type >= 0 && type < sizeof(type_messages) / sizeof(type_messages[0]))
    {
        if (type != 3 && type != 7 && type != 12 && type != 15)
        {
            /*Add and Remove Character Operations*/
            if (type != 14)
            {
                if (digits[strlen(digits) - 1] == '.' && type == 11)
                {
                    printf("illegal format\n");
                }
                else
                {
                    if (strlen(digits) < MAX_DIGITS)
                    {
                        strcat(digits, type_messages[type]);
                        lv_label_set_text(ui_intercom_call_Label_1, digits);
                    }
                }
            }
            else
            {
                int length = strlen(digits);
                if (length > 1)
                {
                    digits[length - 1] = '\0';
                    lv_label_set_text(ui_intercom_call_Label_1, digits);
                }
            }
        }
        if (type == 12)
        {
            if (audio_client_state() != STATE_RUNNING)
                run_audio_client(lv_label_get_text(ui_intercom_call_Label_1));
            else
                exit_audio_client();
        }
    }
}

static void back_icon_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED)
    {
        intercom_homepage_ui_init();
        lv_obj_del(ui_Screen_intercom_call);
        ui_Screen_intercom_call = NULL;
        lv_timer_del(timer);
        exit_audio_client();
        exit_audio_server();
    }
}



void intercom_call_button(lv_obj_t *parent, lv_obj_t *referent)
{
    for (intptr_t i = 0; i < 16; i ++)
    {
        button[i].ui_circle = lv_img_create(parent);
        lv_img_set_src(button[i].ui_circle, IMG_INTERCOM_ROUND);
        lv_obj_set_width(button[i].ui_circle, LV_SIZE_CONTENT);
        lv_obj_set_height(button[i].ui_circle, LV_SIZE_CONTENT);
        lv_obj_add_flag(button[i].ui_circle, LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_clear_flag(button[i].ui_circle, LV_OBJ_FLAG_SCROLLABLE);
        lv_img_set_zoom(button[i].ui_circle, 500);
        lv_obj_align_to(button[i].ui_circle, referent, LV_ALIGN_OUT_BOTTOM_LEFT, button[i].x, button[i].y);  // 将 lv_obj_align_to 替换为 lv_obj_align
        button[i].ui_circle_label = lv_label_create(parent);
        lv_obj_set_size(button[i].ui_circle_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align_to(button[i].ui_circle_label, button[i].ui_circle, LV_ALIGN_CENTER, button[i].x_po_verify, button[i].y_po_verify);
        lv_label_set_text(button[i].ui_circle_label, button[i].txt);
        lv_obj_add_flag(button[i].ui_circle, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(button[i].ui_circle, icon_cb, LV_EVENT_ALL, (void *)i);

    }
}

static void state_update(lv_timer_t *timer)
{
    static int state = STATE_IDLE;

    if (audio_client_state() != state)
    {
        state = audio_client_state();
        if (state == STATE_RUNNING)
        {
            lv_label_set_text(button[12].ui_circle_label, "挂断");
            lv_obj_set_style_text_color(button[12].ui_circle_label,
                                        lv_color_make(0xff, 0x0, 0x0),
                                        LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text(button[12].ui_circle_label, button[12].txt);
            lv_obj_set_style_text_color(button[12].ui_circle_label,
                                        lv_color_black(),
                                        LV_PART_MAIN);
        }
    }
}

void ui_intercom_call_screen_init()
{
    char *ip;

    ui_Screen_intercom_call = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen_intercom_call, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_img_opa(ui_Screen_intercom_call, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_Screen_intercom_call, &style_txt_s, LV_PART_MAIN);

    bg_pic = lv_img_create(ui_Screen_intercom_call);
    lv_obj_set_pos(bg_pic, 0, 0);
    lv_img_set_src(bg_pic, BG_PIC_0);


    //back img
    ui_back = lv_img_create(ui_Screen_intercom_call);
    lv_img_set_src(ui_back, IMG_RETURN_BTN);
    lv_obj_set_width(ui_back, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_back, LV_SIZE_CONTENT);    /// 32
    lv_obj_align(ui_back, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_flag(ui_back, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_back, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_event_cb(ui_back, back_icon_cb, LV_EVENT_ALL, 0);


    ui_intercom_call_Label_0 = lv_label_create(ui_Screen_intercom_call);
    lv_label_set_text(ui_intercom_call_Label_0, "对讲呼叫");
    lv_obj_set_style_text_color(ui_intercom_call_Label_0, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_style(ui_intercom_call_Label_0, &style_txt_m, LV_PART_MAIN);
    lv_obj_align_to(ui_intercom_call_Label_0, ui_back,
                    LV_ALIGN_OUT_RIGHT_MID,
                    5, 0);

    ui_rectangle = lv_img_create(ui_Screen_intercom_call);
    lv_img_set_src(ui_rectangle, IMG_INTERCOM_RECTANGLE1);
    lv_obj_set_width(ui_rectangle, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_rectangle, LV_SIZE_CONTENT);
    lv_obj_align(ui_rectangle, LV_ALIGN_CENTER, 0, -250);
    lv_obj_add_flag(ui_rectangle, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_rectangle, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_intercom_call_Label_1 = lv_label_create(ui_rectangle);
    lv_obj_set_size(ui_intercom_call_Label_1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_intercom_call_Label_1, ui_rectangle, LV_ALIGN_LEFT_MID, 20, 0);
    memset(digits, 0, sizeof(digits));
    lv_label_set_text(ui_intercom_call_Label_1, digits);
    lv_obj_refr_size(ui_intercom_call_Label_1);
    lv_obj_refr_pos(ui_intercom_call_Label_1);

    ui_ip_label = lv_label_create(ui_Screen_intercom_call);
    lv_obj_set_size(ui_ip_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    ip = get_local_ip();
    //lv_obj_set_style_bg_opa(ui_ip_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_text(ui_ip_label, ip ? ip : "未联网");
    if (ip)
        free(ip);
    lv_obj_refr_size(ui_rectangle);
    lv_obj_refr_pos(ui_rectangle);
    lv_obj_refr_size(ui_ip_label);
    lv_obj_refr_pos(ui_ip_label);
    lv_obj_align_to(ui_ip_label, ui_rectangle, LV_ALIGN_OUT_TOP_LEFT, 0, -10);

    intercom_call_button(ui_Screen_intercom_call, ui_rectangle);

    timer = lv_timer_create(state_update, 100, NULL);
    lv_timer_enable(timer);
}

void intercom_call_ui_init()
{
    if (!ui_Screen_intercom_call)
        ui_intercom_call_screen_init();
    lv_disp_load_scr(ui_Screen_intercom_call);
    run_audio_server();
}

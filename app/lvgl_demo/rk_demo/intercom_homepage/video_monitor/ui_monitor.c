#include "home_ui.h"
#include "ui_intercom_homepage.h"
#include "main.h"
#include <time.h>

#include "rkadk_common.h"
#include "rkadk_media_comm.h"
#include "rkadk_log.h"
#include "rkadk_param.h"
#include "rkadk_player.h"

static RKADK_PLAYER_CFG_S stPlayCfg;
static RKADK_MW_PTR pPlayer = NULL;

lv_obj_t *ui_Screen_monitor;
lv_obj_t *rtsp_mode;
lv_obj_t *obj;
lv_obj_t *ui_monitor_Label_0;
lv_obj_t *ui_monitor_Label_1;
lv_obj_t *ui_rtsp_label;

lv_obj_t *rtsp_button;
lv_obj_t *ui_circular_0;
lv_obj_t *rtsp_connect;
lv_obj_t *ui_circular_1;
lv_obj_t *ui_circular_2;
lv_obj_t *ui_circular_3;
lv_obj_t *ui_circular_mid;


static lv_obj_t *ui_back;
static lv_obj_t *ui_pause;
static lv_obj_t *ui_contuine;
static lv_obj_t *ui_webcam;
static lv_obj_t *ui_forward;
static lv_obj_t *ui_backward;
static lv_obj_t *kb;

static lv_obj_t *player_box = NULL;
static lv_obj_t *icon_box = NULL;

extern lv_style_t style_txt_s;
extern lv_style_t style_txt_m;

static lv_style_t style_txt;
static lv_style_t style_list;

int network_enable;
int video_stop;
int video_pause;
int thread_start;
pthread_t video_thread;
RKADK_BOOL bVideoEnable = true;
RKADK_BOOL bAudioEnable = false;

char rtsp_address[128];


static void rkadk_deinit(void)
{
    RKADK_PLAYER_Stop(pPlayer);
    RKADK_PLAYER_Destroy(pPlayer);
    pPlayer = NULL;
    RKADK_MPI_SYS_Exit();
}

static void back_icon_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED)
    {
        intercom_homepage_ui_init();
        if (network_enable == 1)
        {
            video_stop = 1;
            thread_start = 0;
            pthread_join(video_thread, NULL);
        }
        lv_obj_del(ui_Screen_monitor);
        ui_Screen_monitor = NULL;
        rkadk_deinit();
    }
}

int is_network_enable(void)
{
    int ret = system("ping 114.114.114.114 -c 1 -W 1 > /dev/null");
    return !ret;
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

static void param_init(RKADK_PLAYER_FRAME_INFO_S *pstFrmInfo)
{
    RKADK_CHECK_POINTER_N(pstFrmInfo);

    memset(pstFrmInfo, 0, sizeof(RKADK_PLAYER_FRAME_INFO_S));
    pstFrmInfo->u32DispWidth = 720;
    pstFrmInfo->u32DispHeight = 512; //1280*0.4=512
    pstFrmInfo->u32ImgWidth = pstFrmInfo->u32DispWidth;
    pstFrmInfo->u32ImgHeight = pstFrmInfo->u32DispHeight;
    pstFrmInfo->u32VoFormat = VO_FORMAT_NV12;
    pstFrmInfo->u32EnIntfType = DISPLAY_TYPE_LCD;
    pstFrmInfo->u32VoLay = 1;
    pstFrmInfo->enIntfSync = RKADK_VO_OUTPUT_DEFAULT;
    pstFrmInfo->enVoSpliceMode = SPLICE_MODE_BYPASS;
    pstFrmInfo->u32BorderColor = 0x0000FA;
    pstFrmInfo->bMirror = RKADK_FALSE;
    pstFrmInfo->bFlip = RKADK_FALSE;
    pstFrmInfo->u32Rotation = 1;
    pstFrmInfo->stSyncInfo.bIdv = RKADK_TRUE;
    pstFrmInfo->stSyncInfo.bIhs = RKADK_TRUE;
    pstFrmInfo->stSyncInfo.bIvs = RKADK_TRUE;
    pstFrmInfo->stSyncInfo.bSynm = RKADK_TRUE;
    pstFrmInfo->stSyncInfo.bIop = RKADK_TRUE;
    pstFrmInfo->stSyncInfo.u16FrameRate = 30;
    pstFrmInfo->stSyncInfo.u16PixClock = 65000;
    pstFrmInfo->stSyncInfo.u16Hact = 1200;
    pstFrmInfo->stSyncInfo.u16Hbb = 24;
    pstFrmInfo->stSyncInfo.u16Hfb = 240;
    pstFrmInfo->stSyncInfo.u16Hpw = 136;
    pstFrmInfo->stSyncInfo.u16Hmid = 0;
    pstFrmInfo->stSyncInfo.u16Vact = 1200;
    pstFrmInfo->stSyncInfo.u16Vbb = 200;
    pstFrmInfo->stSyncInfo.u16Vfb = 194;
    pstFrmInfo->stSyncInfo.u16Vpw = 6;

    return;
}

static RKADK_VOID PlayerEventFnTest(RKADK_MW_PTR pPlayer,
                                    RKADK_PLAYER_EVENT_E enEvent,
                                    RKADK_VOID *pData) {
  switch (enEvent) {
  case RKADK_PLAYER_EVENT_STATE_CHANGED:
    printf("+++++ RKADK_PLAYER_EVENT_STATE_CHANGED +++++\n");
    break;
  case RKADK_PLAYER_EVENT_EOF:
    printf("+++++ RKADK_PLAYER_EVENT_EOF +++++\n");
    break;
  case RKADK_PLAYER_EVENT_SOF:
    printf("+++++ RKADK_PLAYER_EVENT_SOF +++++\n");
    break;
  case RKADK_PLAYER_EVENT_SEEK_END:
    printf("+++++ RKADK_PLAYER_EVENT_SEEK_END +++++\n");
    break;
  case RKADK_PLAYER_EVENT_ERROR:
    printf("+++++ RKADK_PLAYER_EVENT_ERROR +++++\n");
    break;
  case RKADK_PLAYER_EVENT_PREPARED:
    printf("+++++ RKADK_PLAYER_EVENT_PREPARED +++++\n");
    break;
  case RKADK_PLAYER_EVENT_PLAY:
    printf("+++++ RKADK_PLAYER_EVENT_PLAY +++++\n");
    break;
  case RKADK_PLAYER_EVENT_PAUSED:
    printf("+++++ RKADK_PLAYER_EVENT_PAUSED +++++\n");
    break;
  case RKADK_PLAYER_EVENT_STOPPED:
    printf("+++++ RKADK_PLAYER_EVENT_STOPPED +++++\n");
    break;
  default:
    printf("+++++ Unknown event(%d) +++++\n", enEvent);
    break;
  }
}

static void rkadk_init(void)
{
    setenv("rt_vo_disable_vop", "0", 1);
    RKADK_MPI_SYS_Init();
    RKADK_PARAM_Init(NULL, NULL);
    memset(&stPlayCfg, 0, sizeof(RKADK_PLAYER_CFG_S));
    param_init(&stPlayCfg.stFrmInfo);
    stPlayCfg.bEnableAudio = false;
    stPlayCfg.bEnableVideo = false;
    if (bAudioEnable)
        stPlayCfg.bEnableAudio = true;
    if (bVideoEnable)
        stPlayCfg.bEnableVideo = true;
    stPlayCfg.stFrmInfo.u32FrmInfoX = 0;
    stPlayCfg.stFrmInfo.u32FrmInfoY = 128;
    stPlayCfg.stRtspCfg.u32IoTimeout = 3 * 1000 * 1000;

    stPlayCfg.pfnPlayerCallback = PlayerEventFnTest;

    stPlayCfg.stRtspCfg.transport = "udp";

    if (RKADK_PLAYER_Create(&pPlayer, &stPlayCfg))
    {
        printf("rkadk: RKADK_PLAYER_Create failed\n");
        return;
    }
}


void* rtsp_play()
{
    //rtsp_address,eg:rtsp://192.168.1.101:8554/
    printf("rtsp_address = %s\n", rtsp_address);
    if (pPlayer != NULL)
    {
        printf("video_name_callback: stop and deinit pPlayer\n");
        RKADK_PLAYER_Stop(pPlayer);
        rkadk_deinit();
    }
    if (pPlayer == NULL)
    {
        printf("video_name_callback: rkadk_init pPlayer\n");
        rkadk_init();
    }
    int ret = RKADK_PLAYER_SetDataSource(pPlayer, rtsp_address);
    if (ret)
    {
        printf("rkadk: SetDataSource failed, ret = %d\n", ret);
        return -1;
    }
    printf("RKADK_PLAYER_SetDataSource\n");
    ret = RKADK_PLAYER_Prepare(pPlayer);
    if (ret)
    {
        printf("rkadk: Prepare failed, ret = %d\n", ret);
        return -1;
    }
    printf("RKADK_PLAYER_Prepare\n");
    ret = RKADK_PLAYER_Play(pPlayer);
    while (!video_stop){
        usleep(1000 * 100);
    }
}


void rtsp_play_start_callback(lv_event_t *event){
    printf("rtsp_play_stop_callback into\n");
    int ret = RKADK_PLAYER_Play(pPlayer);
    if (ret)
    {
        printf("rkadk: Pause failed, ret = %d\n", ret);
    }
}

void rtsp_play_stop_callback(lv_event_t *event)
{
    printf("rtsp_play_stop_callback into\n");
    if (!video_pause){
        RKADK_PLAYER_Pause(pPlayer);
        lv_obj_add_flag(ui_pause, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_contuine, LV_OBJ_FLAG_HIDDEN);
    } else {
        RKADK_PLAYER_Play(pPlayer);
        lv_obj_add_flag(ui_contuine, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pause, LV_OBJ_FLAG_HIDDEN);
    }
    video_pause = !video_pause;
}

static void event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *ibox = lv_obj_get_parent(obj);

    const char *psk;

    if (strcmp(lv_inputbox_get_active_btn_text(ibox), "确认") == 0)
    {
        psk = lv_textarea_get_text(lv_inputbox_get_text_area(ibox));
        strcpy(rtsp_address, psk);
        network_enable = is_network_enable();
        if (network_enable){
            if (thread_start)
            {
                video_stop = 1;
                pthread_join(video_thread, NULL);
            }
            video_stop = 0;
            thread_start = 1;
            pthread_create(&video_thread, NULL, rtsp_play, NULL);
        } else {
            printf("network disable\n");
        }
    }
    lv_msgbox_close(ibox);
    lv_obj_del(kb);
}


static void connect_rtsp()
{
    char title[128];

    static const char *btns[] = {"确认", "取消", ""};

    lv_obj_t *ibox = lv_inputbox_create(NULL, title, "请输入rtsp地址", btns, false);
    lv_obj_add_event_cb(ibox, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_style(ibox, &style_txt, LV_PART_MAIN);
    lv_obj_set_size(ibox, lv_pct(80), lv_pct(20));
    lv_obj_align(ibox, LV_ALIGN_TOP_MID, 0, lv_pct(15));

    kb = lv_keyboard_create(lv_layer_sys());
    lv_obj_set_size(kb, lv_pct(100), lv_pct(30));
    lv_obj_set_align(kb, LV_ALIGN_BOTTOM_MID);
    lv_textarea_set_password_mode(lv_inputbox_get_text_area(ibox), false);
    lv_keyboard_set_textarea(kb, lv_inputbox_get_text_area(ibox));
}

static void rtsp_address_get(lv_event_t *e)
{
    video_pause = 0;
    connect_rtsp();
}

static void obj_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    char tmp_buf[32];
    if (code == LV_EVENT_VALUE_CHANGED){
        lv_dropdown_get_selected_str(obj, tmp_buf, sizeof(tmp_buf));
        if (strcmp(tmp_buf, "视频")==0){
            bVideoEnable = true;
            bAudioEnable = false;
        }
        if (strcmp(tmp_buf, "音视频")==0){
            bVideoEnable = true;
            bAudioEnable = true;
        }
        if (strcmp(tmp_buf, "音频")==0){
            bVideoEnable = false;
            bAudioEnable = true;
        }
    }
}


void ui_monitor_screen_init()
{
    video_pause = 0;
    network_enable = 0;
    thread_start = 0;
    style_init();
    ui_Screen_monitor = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen_monitor, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_opa(ui_Screen_monitor, LV_OPA_TRANSP, 0);

    icon_box = lv_obj_create(ui_Screen_monitor);
    lv_obj_set_width(icon_box, lv_pct(100));
    lv_obj_set_height(icon_box, lv_pct(10));
    lv_obj_align(icon_box, LV_ALIGN_TOP_LEFT, 0, 0);

    ui_back = lv_img_create(icon_box);
    lv_img_set_src(ui_back, IMG_RETURN_BTN);
    lv_obj_set_width(ui_back, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_back, LV_SIZE_CONTENT);    /// 32
    lv_obj_align(ui_back, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_flag(ui_back, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_back, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_event_cb(ui_back, back_icon_cb, LV_EVENT_ALL, 0);

    /*text:Video Monitor*/
    ui_monitor_Label_0 = lv_label_create(icon_box);
    lv_label_set_text(ui_monitor_Label_0, "视频监控");
    lv_obj_set_style_text_color(ui_monitor_Label_0, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_style(ui_monitor_Label_0, &style_txt_m, LV_PART_MAIN);
    lv_obj_align_to(ui_monitor_Label_0, ui_back,
                    LV_ALIGN_OUT_RIGHT_MID,
                    5, 0);

    rtsp_button = lv_obj_create(icon_box);
    lv_obj_remove_style_all(rtsp_button);
    lv_obj_set_width(rtsp_button, lv_pct(20));
    lv_obj_set_height(rtsp_button, lv_pct(80));
    lv_obj_align(rtsp_button, LV_ALIGN_RIGHT_MID, 0, 0);
    ui_rtsp_label = lv_label_create(rtsp_button);
    lv_label_set_text(ui_rtsp_label, "rstp地址");
    lv_obj_add_style(ui_rtsp_label, &style_txt_m, LV_PART_MAIN);
    lv_obj_align_to(ui_rtsp_label, rtsp_button,
                    LV_ALIGN_CENTER,
                    0, 0);
    lv_obj_add_flag(rtsp_button, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_add_event_cb(rtsp_button, rtsp_address_get, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(rtsp_button, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_IGNORE_LAYOUT);

    player_box = lv_obj_create(ui_Screen_monitor);
    lv_obj_set_width(player_box, lv_pct(100));
    lv_obj_set_height(player_box, lv_pct(50));
    lv_obj_align(player_box, LV_ALIGN_TOP_LEFT, 0, lv_pct(50));

    rtsp_mode = lv_obj_create(player_box);
    lv_obj_remove_style_all(rtsp_mode);
    lv_obj_set_width(rtsp_mode, lv_pct(30));
    lv_obj_set_height(rtsp_mode, lv_pct(20));
    obj = lv_label_create(rtsp_mode);
    lv_label_set_text(obj, "模式选择");
    lv_obj_add_style(obj, &style_txt_m, LV_PART_MAIN);
    lv_obj_align(rtsp_mode, LV_ALIGN_RIGHT_MID, 0, -70);
    obj = lv_dropdown_create(rtsp_mode);
    lv_obj_add_style(obj, &style_txt_s, LV_PART_MAIN);
    lv_obj_add_style(lv_dropdown_get_list(obj), &style_txt_s, LV_PART_MAIN);
    lv_dropdown_set_options(obj, "视频\n""音视频\n""音频");
    lv_obj_align(obj, LV_ALIGN_RIGHT_MID, -40, 20);
    lv_obj_add_event_cb(obj, obj_event_handler, LV_EVENT_ALL, NULL);

    ui_circular_0 = lv_img_create(player_box);
    lv_img_set_src(ui_circular_0, IMG_INTERCOM_ROUND);
    lv_obj_set_size(ui_circular_0, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(ui_circular_0, 30, 160);
    ui_backward = lv_img_create(player_box);
    lv_img_set_src(ui_backward, IMG_INTERCOM_BACKWARD);
    lv_obj_set_size(ui_backward, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_backward, ui_circular_0, LV_ALIGN_CENTER, 0, -3);

    //circular_right
    ui_circular_1 = lv_img_create(player_box);
    lv_img_set_src(ui_circular_1, IMG_INTERCOM_ROUND);
    lv_obj_set_size(ui_circular_1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_circular_1, ui_circular_0, LV_ALIGN_OUT_RIGHT_MID, 170, 0);
    ui_forward = lv_img_create(player_box);
    lv_img_set_src(ui_forward, IMG_INTERCOM_ARROWUP);
    lv_obj_set_size(ui_forward, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_forward, ui_circular_1, LV_ALIGN_CENTER, 0, -3);

    ui_circular_mid = lv_img_create(player_box);
    lv_img_set_src(ui_circular_mid, IMG_INTERCOM_ROUND);
    lv_img_set_zoom(ui_circular_mid, 500);
    lv_obj_set_size(ui_circular_mid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_circular_mid, ui_circular_0, LV_ALIGN_OUT_RIGHT_MID, 50, 0);
    ui_monitor_Label_1 = lv_label_create(player_box);
    lv_obj_set_size(ui_monitor_Label_1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_monitor_Label_1, ui_circular_mid, LV_ALIGN_CENTER, -3, -5);
    lv_label_set_text(ui_monitor_Label_1, "主机1");
    lv_obj_add_style(ui_monitor_Label_1, &style_txt_s, LV_PART_MAIN);

    //webcam
    ui_circular_2 = lv_img_create(player_box);
    lv_img_set_src(ui_circular_2, IMG_INTERCOM_ROUND);
    lv_obj_set_size(ui_circular_2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_circular_2, ui_circular_mid, LV_ALIGN_OUT_TOP_MID, 0, -30);
    ui_webcam = lv_img_create(player_box);
    lv_img_set_src(ui_webcam, IMG_INTERCOM_WEBCAM);
    lv_obj_set_size(ui_webcam, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_webcam, ui_circular_2, LV_ALIGN_CENTER, 0, -3);


    //pause
    ui_circular_3 = lv_img_create(player_box);
    lv_img_set_src(ui_circular_3, IMG_INTERCOM_ROUND);
    lv_obj_set_size(ui_circular_3, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_circular_3, ui_circular_mid, LV_ALIGN_OUT_BOTTOM_MID, 0, 30);
    lv_obj_add_flag(ui_circular_3, LV_OBJ_FLAG_CLICKABLE);

    ui_pause = lv_img_create(player_box);
    lv_img_set_src(ui_pause, IMG_INTERCOM_PAUSE);
    lv_obj_set_size(ui_pause, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_pause, ui_circular_3, LV_ALIGN_CENTER, 0, -3);
    lv_obj_add_flag(ui_circular_3, LV_OBJ_FLAG_CLICKABLE);

    ui_contuine = lv_img_create(player_box);
    lv_img_set_src(ui_contuine, IMG_INTERCOM_CONTUINE);
    lv_obj_set_size(ui_contuine, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align_to(ui_contuine, ui_circular_3, LV_ALIGN_CENTER, 0, -3);
    lv_obj_add_flag(ui_pause, LV_OBJ_FLAG_HIDDEN);

    if (ui_circular_3 != NULL)
    {
        lv_obj_add_event_cb(ui_circular_3, rtsp_play_stop_callback, LV_EVENT_CLICKED, NULL);
    }

}

void monitor_ui_init()
{
    if (!ui_Screen_monitor)
        ui_monitor_screen_init();
    lv_disp_load_scr(ui_Screen_monitor);
}
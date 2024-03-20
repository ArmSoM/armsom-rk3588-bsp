#ifndef __MAIN_H__
#define __MAIN_H__

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "lv_port_file.h"
#include "lv_port_indev.h"

extern lv_ft_info_t ttf_main_s;
extern lv_ft_info_t ttf_main_m;
extern lv_ft_info_t ttf_main_l;

extern lv_style_t style_txt_s;
extern lv_style_t style_txt_m;
extern lv_style_t style_txt_l;

int app_disp_rotation(void);

#endif


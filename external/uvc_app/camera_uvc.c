/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 * author: Zhihua Wang, hogan.wang@rock-chips.com
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL), available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "uvc_control.h"
#include "uvc_video.h"
#include <camera_engine_rkisp/interface/rkisp_api.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_VIDEO_ID 20

struct camera_param {
    int width;
    int height;
};

static struct camera_param g_param;
static pthread_t g_th;
static bool g_run;

int get_video_id(char *name)
{
    FILE *fp = NULL;
    char buf[1024];
    int i;
    char cmd[128];
    bool exist = false;

    for (i = 0; i < MAX_VIDEO_ID; i++) {
        snprintf(cmd, sizeof(cmd), "/sys/class/video4linux/video%d/name", i);
        if (access(cmd, F_OK))
            continue;
        snprintf(cmd, sizeof(cmd), "cat /sys/class/video4linux/video%d/name", i);
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) {
                if (strstr(buf, name))
                    exist = true;
            }
            pclose(fp);
        }
        if (exist)
            break;
    }
    return (i == MAX_VIDEO_ID ? -1 : i);
}

int isp_uvc(int width, int height)
{
    const struct rkisp_api_ctx *ctx;
    const struct rkisp_api_buf *buf;
    char name[32];
    int extra_cnt = 0;

    if (!g_run)
        return -1;

    printf("%s enter\n", __func__);

    int id = get_video_id("rkisp1_mainpath");
    if (id < 0) {
        printf("%s: get video id fail!\n", __func__);
        return -1;
    }

    snprintf(name, sizeof(name), "/dev/video%d", id);
    printf("%s: %s\n", __func__, name);
    ctx = rkisp_open_device(name, 0);
    if (ctx == NULL) {
        printf("%s: ctx is NULL\n", __func__);
        return -1;
    }

    rkisp_set_fmt(ctx, width, height, V4L2_PIX_FMT_NV12);

    if (rkisp_start_capture(ctx))
        return -1;

    do {
        buf = rkisp_get_frame(ctx, 0);
        if (!buf) {
            printf("%s: rkisp_get_frame NULL\n", __func__);
            break;
        }
        extra_cnt++;
        uvc_read_camera_buffer(buf->buf, buf->fd, buf->size,
                               &extra_cnt, sizeof(extra_cnt));
        rkisp_put_frame(ctx, buf);
    } while (g_run);

    rkisp_stop_capture(ctx);
    rkisp_close_device(ctx);

    printf("%s exit\n", __func__);

    return 0;
}

void *isp_uvc_thread(void *arg)
{
    struct camera_param *param = (struct camera_param *)arg;
    while (!isp_uvc(param->width, param->height))
        usleep(100000);
    pthread_exit(NULL);
}

int open_isp_uvc(int width, int height)
{
    g_run = true;
    g_param.width = width;
    g_param.height = height;
    return pthread_create(&g_th, NULL, isp_uvc_thread, &g_param);
}

void close_isp_uvc(void)
{
    g_run = false;
    pthread_join(g_th, NULL);
}

int cif_uvc(int width, int height)
{
    const struct rkisp_api_ctx *ctx;
    const struct rkisp_api_buf *buf;
    char name[32];
    int extra_cnt = 0;

    if (!g_run)
        return -1;

    int id = get_video_id("stream_cif_dvp");
    if (id < 0) {
        printf("%s: get video id fail!\n", __func__);
        return -1;
    }

    snprintf(name, sizeof(name), "/dev/video%d", id);
    printf("%s: %s\n", __func__, name);
    ctx = rkisp_open_device(name, 0);
    if (ctx == NULL) {
        printf("%s: ctx is NULL\n", __func__);
        return -1;
    }

    rkisp_set_sensor_fmt(ctx, width, height, MEDIA_BUS_FMT_YUYV8_2X8);
    rkisp_set_fmt(ctx, width, height, V4L2_PIX_FMT_NV12);

    if (rkisp_start_capture(ctx))
        return -1;

    do {
        buf = rkisp_get_frame(ctx, 0);
        if (!buf) {
            printf("%s: rkisp_get_frame NULL\n", __func__);
            break;
        }
        extra_cnt++;
        uvc_read_camera_buffer(buf->buf, buf->fd, buf->size,
                               &extra_cnt, sizeof(extra_cnt));
        rkisp_put_frame(ctx, buf);
    } while (g_run);

    rkisp_stop_capture(ctx);
    rkisp_close_device(ctx);

    return 0;
}

void *cif_uvc_thread(void *arg)
{
    struct camera_param *param = (struct camera_param *)arg;
    while (!cif_uvc(param->width, param->height))
        usleep(100000);
    pthread_exit(NULL);
}

int open_cif_uvc(int width, int height)
{
    g_run = true;
    g_param.width = width;
    g_param.height = height;
    return pthread_create(&g_th, NULL, cif_uvc_thread, &g_param);
}

void close_cif_uvc(void)
{
    g_run = false;
    pthread_join(g_th, NULL);
}

void usage(const char *name)
{
    printf("Usage: %s options\n"
           "-i --isp   Use isp camera.\n"
           "-c --cif   Use cif camera.\n"
           , name);
    printf("e.g. %s -i\n", name);
    printf("e.g. %s -c\n", name);
    exit(0);
}

int main(int argc, char* argv[])
{
    uint32_t flags = 0;

    bool g_isp_en = false;
    bool g_cif_en = false;

    int next_option;
    const char* const short_options = "ic";
    const struct option long_options[] = {
        {"isp", 0, NULL, 'i'},
        {"cif", 0, NULL, 'c'},
    };

    do {
        next_option = getopt_long(argc, argv, short_options, long_options, NULL);
        switch (next_option) {
        case 'i':
            g_isp_en = true;
            break;
        case 'c':
            g_cif_en = true;
            break;
        case -1:
            break;
        default:
            usage(argv[0]);
            break;
        }
    } while (next_option != -1);

    if (!g_isp_en && !g_cif_en)
        usage(argv[0]);

    if (g_isp_en) {
        register_uvc_open_camera(open_isp_uvc);
        register_uvc_close_camera(close_isp_uvc);
    } else if (g_cif_en) {
        register_uvc_open_camera(open_cif_uvc);
        register_uvc_close_camera(close_cif_uvc);
    }

    flags = UVC_CONTROL_LOOP_ONCE;
    uvc_control_run(flags);

    while (1)
        sleep(5);

    uvc_control_join(flags);

    return 0;
}

/*
 * Copyright (C) 2019 Rockchip Electronics Co., Ltd.
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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include "uvc_control.h"
#include "uvc_encode.h"
#include "uvc_video.h"
#include "uevent.h"

#define SYS_ISP_NAME "isp"
#define SYS_CIF_NAME "cif"
#define UVC_STREAMING_INTF_PATH "/sys/kernel/config/usb_gadget/rockchip/functions/uvc.gs6/streaming/bInterfaceNumber"

struct uvc_ctrl {
    int id;
    int width;
    int height;
    int fps;
};

static struct uvc_ctrl uvc_ctrl[2];
struct uvc_encode uvc_enc;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int uvc_streaming_intf = -1;

static pthread_t run_id = 0;
static bool run_flag = true;
static pthread_mutex_t run_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t run_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t video_added = PTHREAD_COND_INITIALIZER;

static uvc_open_camera_callback uvc_open_camera_cb = NULL;
static uvc_close_camera_callback uvc_close_camera_cb = NULL;

void register_uvc_open_camera(uvc_open_camera_callback cb)
{
    uvc_open_camera_cb = cb;
}

void register_uvc_close_camera(uvc_close_camera_callback cb)
{
    uvc_close_camera_cb = cb;
}

static bool is_uvc_video(void *buf)
{
    if (strstr(buf, "usb") || strstr(buf, "gadget"))
        return true;
    else
        return false;
}

static void query_uvc_streaming_intf(void)
{
    int fd;

    fd = open(UVC_STREAMING_INTF_PATH, O_RDONLY);
    if (fd >= 0) {
        char intf[32] = {0};
        read(fd, intf, sizeof(intf) - 1);
        uvc_streaming_intf = atoi(intf);
        printf("uvc_streaming_intf = %d\n", uvc_streaming_intf);
        close(fd);
    } else {
        printf("open %s failed!\n", UVC_STREAMING_INTF_PATH);
    }
}

int get_uvc_streaming_intf(void)
{
    return uvc_streaming_intf;
}

int check_uvc_video_id(void)
{
    FILE *fp = NULL;
    char buf[1024];
    int i;
    char cmd[128];

    memset(&uvc_ctrl, 0, sizeof(uvc_ctrl));
    uvc_ctrl[0].id = -1;
    uvc_ctrl[1].id = -1;
    for (i = 0; i < 30; i++) {
        snprintf(cmd, sizeof(cmd), "/sys/class/video4linux/video%d/name", i);
        if (access(cmd, F_OK))
            continue;
        snprintf(cmd, sizeof(cmd), "cat /sys/class/video4linux/video%d/name", i);
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) {
                if (is_uvc_video(buf)) {
                    if (uvc_ctrl[0].id < 0)
                        uvc_ctrl[0].id = i;
                    else if (uvc_ctrl[1].id < 0)
                        uvc_ctrl[1].id = i;
                }
            }
            pclose(fp);
        }
    }
    if (uvc_ctrl[0].id < 0 && uvc_ctrl[1].id < 0) {
        printf("Please configure uvc...\n");
        return -1;
    }
    query_uvc_streaming_intf();
    return 0;
}

void add_uvc_video()
{
    if (uvc_ctrl[0].id >= 0)
        uvc_video_id_add(uvc_ctrl[0].id);
    if (uvc_ctrl[1].id >= 0)
        uvc_video_id_add(uvc_ctrl[1].id);
}

void uvc_control_init(int width, int height, int fcc)
{
    pthread_mutex_lock(&lock);
    memset(&uvc_enc, 0, sizeof(uvc_enc));
    if (uvc_encode_init(&uvc_enc, width, height, fcc)) {
        printf("%s fail!\n", __func__);
        abort();
    }
    pthread_mutex_unlock(&lock);
    if (uvc_open_camera_cb)
        uvc_open_camera_cb(width, height);
}

void uvc_control_exit()
{
    if (uvc_close_camera_cb)
        uvc_close_camera_cb();
    pthread_mutex_lock(&lock);
    uvc_encode_exit(&uvc_enc);
    memset(&uvc_enc, 0, sizeof(uvc_enc));
    pthread_mutex_unlock(&lock);
}

void uvc_read_camera_buffer(void *cam_buf, int cam_fd, size_t cam_size,
                            void* extra_data, size_t extra_size)
{
    pthread_mutex_lock(&lock);
    if (cam_size <= uvc_enc.width * uvc_enc.height * 2) {
        uvc_enc.video_id = uvc_video_id_get(0);
        uvc_enc.extra_data = extra_data;
        uvc_enc.extra_size = extra_size;
        uvc_encode_process(&uvc_enc, cam_buf, cam_fd, cam_size);
    } else if (uvc_enc.width > 0 && uvc_enc.height > 0) {
        printf("%s: cam_size = %u, uvc_enc.width = %d, uvc_enc.height = %d\n",
               __func__, cam_size, uvc_enc.width, uvc_enc.height);
    }
    pthread_mutex_unlock(&lock);
}

static void uvc_control_wait(void)
{
    pthread_mutex_lock(&run_mutex);
    if (run_flag)
        pthread_cond_wait(&run_cond, &run_mutex);
    pthread_mutex_unlock(&run_mutex);
}

void uvc_control_signal(void)
{
    pthread_mutex_lock(&run_mutex);
    pthread_cond_signal(&run_cond);
    pthread_mutex_unlock(&run_mutex);
}

void uvc_added_signal(void)
{
    pthread_mutex_lock(&run_mutex);
    pthread_cond_signal(&video_added);
    pthread_mutex_unlock(&run_mutex);
}

static void uvc_added_wait(void)
{
    pthread_mutex_lock(&run_mutex);
    if (run_flag)
        pthread_cond_wait(&video_added, &run_mutex);
    pthread_mutex_unlock(&run_mutex);
}

static void *uvc_control_thread(void *arg)
{
    uint32_t flag = *(uint32_t *)arg;

    while (run_flag) {
        if (!check_uvc_video_id()) {
            add_uvc_video();
            /* Ensure main was waiting for this signal */
            usleep(500);
            uvc_added_signal();
            if (flag & UVC_CONTROL_LOOP_ONCE)
                break;
            uvc_control_wait();
            uvc_video_id_exit_all();
        } else {
            uvc_control_wait();
        }
    }
    pthread_exit(NULL);
}

int uvc_control_run(uint32_t flags)
{
    if (flags & UVC_CONTROL_CHECK_STRAIGHT) {
        if (!check_uvc_video_id())
            add_uvc_video();
        else
            return -1;
    } else {
        uevent_monitor_run(flags);
        if (pthread_create(&run_id, NULL, uvc_control_thread, &flags)) {
            printf("%s: pthread_create failed!\n", __func__);
            return -1;
        }
        uvc_added_wait();
    }

    return 0;
}

void uvc_control_join(uint32_t flags)
{
    if (flags & UVC_CONTROL_CHECK_STRAIGHT) {
        uvc_video_id_exit_all();
    } else {
        run_flag = false;
        uvc_control_signal();
        pthread_join(run_id, NULL);
        if (flags & UVC_CONTROL_LOOP_ONCE);
            uvc_video_id_exit_all();
    }
}

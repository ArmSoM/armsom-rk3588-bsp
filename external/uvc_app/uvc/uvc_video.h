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

#ifndef __UVC_VIDEO_H__
#define __UVC_VIDEO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <linux/videodev2.h>

#define UVC_BUFFER_NUM 3
#define YUYV_AS_RAW 0

struct uvc_device;

struct uvc_user {
    unsigned int width;
    unsigned int height;
    bool run;
    unsigned int fcc;
};

struct uvc_video {
    int id;
    bool uvc_process;
    pthread_t uvc_pid;
    struct video_uvc* uvc;
    pthread_mutex_t buffer_mutex;
    pthread_mutex_t user_mutex;
    struct uvc_user uvc_user;
    struct uvc_buffer* buffer_s;
};

int uvc_gadget_pthread_create(int *id);

int uvc_video_id_check(int id);
int uvc_video_id_add(int id);
void uvc_video_id_remove(int id);
void uvc_video_id_exit_all();
int uvc_video_id_get(unsigned int seq);

void uvc_video_set_uvc_process(int id, bool state);
bool uvc_video_get_uvc_process(int id);

int uvc_buffer_init(int id);
void uvc_buffer_deinit(int id);
bool uvc_buffer_write_enable(int id);
void uvc_buffer_write(unsigned short stamp,
                      void* extra_data,
                      size_t extra_size,
                      void* data,
                      size_t size,
                      unsigned int fcc,
                      int id);
void uvc_set_user_resolution(int width, int height, int id);
void uvc_get_user_resolution(int* width, int* height, int id);
bool uvc_get_user_run_state(int id);
void uvc_set_user_run_state(bool state, int id);
void uvc_set_user_fcc(unsigned int fcc, int id);
unsigned int uvc_get_user_fcc(int id);
void uvc_memset_uvc_user(int id);
pthread_t* uvc_video_get_uvc_pid(int id);
void uvc_user_fill_buffer(struct uvc_device *dev, struct v4l2_buffer *buf, int id);

#ifdef __cplusplus
}
#endif

#endif

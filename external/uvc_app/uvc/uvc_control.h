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

#ifndef __UVC_CONTROL_H__
#define __UVC_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef USE_RK_MODULE
#define ISP_SEQ 1
#define ISP_FMT HAL_FRMAE_FMT_NV12
#define CIF_SEQ 0
#define CIF_FMT HAL_FRMAE_FMT_SBGGR10
#else
#define ISP_SEQ 0
#define ISP_FMT HAL_FRMAE_FMT_SBGGR8
#define CIF_SEQ 1
#define CIF_FMT HAL_FRMAE_FMT_NV12
#endif

#define UVC_CONTROL_LOOP_ONCE		(1 << 0)
#define UVC_CONTROL_CHECK_STRAIGHT	(1 << 1)

typedef int (*uvc_open_camera_callback)(int width, int height);
void register_uvc_open_camera(uvc_open_camera_callback cb);
typedef void (*uvc_close_camera_callback)(void);
void register_uvc_close_camera(uvc_close_camera_callback cb);

void add_uvc_video();
int check_uvc_video_id(void);
void uvc_control_init(int width, int height, int fcc);
void uvc_control_exit();
void uvc_read_camera_buffer(void *cam_buf, int cam_fd, size_t cam_size,
                            void* extra_data, size_t extra_size);
int get_uvc_streaming_intf(void);
void uvc_control_signal(void);
int uvc_control_run(uint32_t flags);
void uvc_control_join(uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif

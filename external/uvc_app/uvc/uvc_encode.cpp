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

#include "uvc_encode.h"
#include "uvc_video.h"
#include <stdio.h>
#include <stdlib.h>

int uvc_encode_init(struct uvc_encode *e, int width, int height,int fcc)
{
    printf("%s: width = %d, height = %d, fcc = %d\n", __func__, width, height,fcc);
    memset(e, 0, sizeof(*e));
    e->video_id = -1;
    e->width = -1;
    e->height = -1;
    e->width = width;
    e->height = height;
    e->fcc = fcc;
    mpi_enc_cmd_config(&e->mpi_cmd, width, height, fcc);
    //mpi_enc_cmd_config_mjpg(&e->mpi_cmd, width, height);
    if(fcc == V4L2_PIX_FMT_YUYV)
        return 0;
    if (mpi_enc_test_init(&e->mpi_cmd, &e->mpi_data) != MPP_OK)
        return -1;
    if (fcc == V4L2_PIX_FMT_H264) {
        e->h264_extra_data = calloc(10240, 1);
        if (!e->h264_extra_data)
            return -1;
        e->h264_extra_size = 10240;
        if (mpi_enc_get_h264_extra(e->mpi_data, e->h264_extra_data, &e->h264_extra_size)) {
            free(e->h264_extra_data);
            return -1;
        }
    }

    return 0;
}

void uvc_encode_exit(struct uvc_encode *e)
{
    if(e->fcc != V4L2_PIX_FMT_YUYV)
        mpi_enc_test_deinit(&e->mpi_data);
    e->video_id = -1;
    e->width = -1;
    e->height = -1;
    if (e->fcc == V4L2_PIX_FMT_H264 && e->h264_extra_data) {
        free(e->h264_extra_data);
        e->h264_extra_data = NULL;
    }
}

bool uvc_encode_process(struct uvc_encode *e, void *virt, int fd, size_t size)
{
    int ret = 0;
    unsigned int fcc;
    int width, height;
    int jpeg_quant;
    void* hnd = NULL;

    if (!uvc_get_user_run_state(e->video_id) || !uvc_buffer_write_enable(e->video_id))
        return false;

    uvc_get_user_resolution(&width, &height, e->video_id);
    fcc = uvc_get_user_fcc(e->video_id);
    switch (fcc) {
    case V4L2_PIX_FMT_YUYV:
        if (virt)
            uvc_buffer_write(0, NULL, 0, virt, width * height * 2, fcc, e->video_id);
        break;
    case V4L2_PIX_FMT_MJPEG:
        if (fd >= 0 && mpi_enc_test_run(&e->mpi_data, fd, size) == MPP_OK) {
            uvc_buffer_write(0, e->extra_data, e->extra_size,
                             e->mpi_data->enc_data, e->mpi_data->enc_len, fcc, e->video_id);
        }
        break;
    case V4L2_PIX_FMT_H264:
        e->extra_data = e->h264_extra_data;
        e->extra_size = e->h264_extra_size;
        if (fd >= 0 && mpi_enc_test_run(&e->mpi_data, fd, size) == MPP_OK) {
            uvc_buffer_write(0, e->extra_data, e->extra_size,
                             e->mpi_data->enc_data, e->mpi_data->enc_len, fcc, e->video_id);
        }
        break;
    default:
        printf("%s: not support fcc: %u\n", __func__, fcc);
        break;
    }

    return true;
}

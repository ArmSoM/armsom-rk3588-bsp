/*
 * Copyright 2015 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __MPI_ENC_H__
#define __MPI_ENC_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#include "vld.h"
#endif

#define MODULE_TAG "mpi_enc"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rockchip/rk_mpi.h>

//#include "mpp_env.h"
//#include "mpp_mem.h"
//#include "printf.h"
//#include "mpp_time.h"
#include "mpp_common.h"

//#include "utils.h"

#define MAX_FILE_NAME_LENGTH        256

typedef struct {
    char            file_input[MAX_FILE_NAME_LENGTH];
    char            file_output[MAX_FILE_NAME_LENGTH];
    MppCodingType   type;
    RK_U32          width;
    RK_U32          height;
    MppFrameFormat  format;
    RK_U32          debug;
    RK_U32          num_frames;

    RK_U32          have_input;
    RK_U32          have_output;
} MpiEncTestCmd;

typedef struct {
    // global flow control flag
    RK_U32 frm_eos;
    RK_U32 pkt_eos;
    RK_U32 frame_count;
    RK_U64 stream_size;

    // src and dst
    FILE *fp_input;
    FILE *fp_output;

    // base flow context
    MppCtx ctx;
    MppApi *mpi;
    MppEncPrepCfg prep_cfg;
    MppEncRcCfg rc_cfg;
    MppEncCodecCfg codec_cfg;

    // input / output
    MppBuffer frm_buf;
    MppEncSeiMode sei_mode;

    // paramter for resource malloc
    RK_U32 width;
    RK_U32 height;
    RK_U32 hor_stride;
    RK_U32 ver_stride;
    MppFrameFormat fmt;
    MppCodingType type;
    RK_U32 num_frames;

    // resources
    size_t frame_size;
    /* NOTE: packet buffer may overflow */
    size_t packet_size;

    // rate control runtime parameter
    RK_S32 gop;
    RK_S32 fps;
    RK_S32 bps;
    MppPacket packet;
    void *enc_data;
    size_t enc_len;
} MpiEncTestData;

MPP_RET mpi_enc_test_init(MpiEncTestCmd *cmd, MpiEncTestData **data);
MPP_RET mpi_enc_test_run(MpiEncTestData **data, int fd, size_t size);
MPP_RET mpi_enc_test_deinit(MpiEncTestData **data);
void mpi_enc_cmd_config(MpiEncTestCmd *cmd, int width, int height,int fcc);
void mpi_enc_cmd_config_mjpg(MpiEncTestCmd *cmd, int width, int height);
void mpi_enc_cmd_config_h264(MpiEncTestCmd *cmd, int width, int height);
void mpi_enc_set_format(MppFrameFormat format);
int mpi_enc_get_h264_extra(MpiEncTestData *p, void *buffer, size_t *size);

#ifdef __cplusplus
}
#endif

#endif

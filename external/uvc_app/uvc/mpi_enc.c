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

#include "mpi_enc.h"
#include <linux/videodev2.h>

#if 0
static OptionInfo mpi_enc_cmd[] = {
    {"i",               "input_file",           "input bitstream file"},
    {"o",               "output_file",          "output bitstream file, "},
    {"w",               "width",                "the width of input picture"},
    {"h",               "height",               "the height of input picture"},
    {"f",               "format",               "the format of input picture"},
    {"t",               "type",                 "output stream coding type"},
    {"n",               "max frame number",     "max encoding frame number"},
    {"d",               "debug",                "debug flag"},
};
#endif
static MppFrameFormat g_format = MPP_FMT_YUV420SP;

static MPP_RET test_ctx_init(MpiEncTestData **data, MpiEncTestCmd *cmd)
{
    MpiEncTestData *p = NULL;
    MPP_RET ret = MPP_OK;

    if (!data || !cmd) {
        printf("invalid input data %p cmd %p\n", data, cmd);
        return MPP_ERR_NULL_PTR;
    }

    p = calloc(sizeof(MpiEncTestData), 1);
    if (!p) {
        printf("create MpiEncTestData failed\n");
        ret = MPP_ERR_MALLOC;
        goto RET;
    }

    // get paramter from cmd
    p->width        = cmd->width;
    p->height       = cmd->height;
    p->hor_stride   = cmd->width;//MPP_ALIGN(cmd->width, 16);
    p->ver_stride   = cmd->height;//MPP_ALIGN(cmd->height, 16);
    p->fmt          = cmd->format;
    p->type         = cmd->type;
    if (cmd->type == MPP_VIDEO_CodingMJPEG)
        cmd->num_frames = 1;
    p->num_frames   = cmd->num_frames;

    if (cmd->have_input) {
        p->fp_input = fopen(cmd->file_input, "rb");
        if (NULL == p->fp_input) {
            printf("failed to open input file %s\n", cmd->file_input);
            printf("create default yuv image for test\n");
        }
    }

    if (cmd->have_output) {
        p->fp_output = fopen(cmd->file_output, "w+b");
        if (NULL == p->fp_output) {
            printf("failed to open output file %s\n", cmd->file_output);
            ret = MPP_ERR_OPEN_FILE;
        }
    }

    // update resource parameter
    if (p->fmt <= MPP_FMT_YUV420SP_VU)
        p->frame_size = MPP_ALIGN(cmd->width, 16) * MPP_ALIGN(cmd->height, 16) * 2;
    else if (p->fmt <= MPP_FMT_YUV422_UYVY) {
        // NOTE: yuyv and uyvy need to double stride
        p->hor_stride *= 2;
        p->frame_size = p->hor_stride * p->ver_stride;
    } else
        p->frame_size = p->hor_stride * p->ver_stride * 4;
    p->packet_size  = p->frame_size;//p->width * p->height;

RET:
    *data = p;
    return ret;
}

static MPP_RET test_ctx_deinit(MpiEncTestData **data)
{
    MpiEncTestData *p = NULL;

    if (!data) {
        printf("invalid input data %p\n", data);
        return MPP_ERR_NULL_PTR;
    }

    p = *data;
    if (p) {
        if (p->fp_input) {
            fclose(p->fp_input);
            p->fp_input = NULL;
        }
        if (p->fp_output) {
            fclose(p->fp_output);
            p->fp_output = NULL;
        }
        free(p);
        *data = NULL;
    }

    return MPP_OK;
}

static MPP_RET test_mpp_setup(MpiEncTestData *p)
{
    MPP_RET ret;
    MppApi *mpi;
    MppCtx ctx;
    MppEncCodecCfg *codec_cfg;
    MppEncPrepCfg *prep_cfg;
    MppEncRcCfg *rc_cfg;
    MppPollType block = MPP_POLL_BLOCK;

    if (NULL == p)
        return MPP_ERR_NULL_PTR;

    mpi = p->mpi;
    ctx = p->ctx;
    codec_cfg = &p->codec_cfg;
    prep_cfg = &p->prep_cfg;
    rc_cfg = &p->rc_cfg;

    /* setup default parameter */
    p->fps = 30;
    p->gop = 60;
    p->bps = p->width * p->height / 8 * p->fps;

    prep_cfg->change        = MPP_ENC_PREP_CFG_CHANGE_INPUT |
                              MPP_ENC_PREP_CFG_CHANGE_ROTATION |
                              MPP_ENC_PREP_CFG_CHANGE_FORMAT;
    prep_cfg->width         = p->width;
    prep_cfg->height        = p->height;
    prep_cfg->hor_stride    = p->hor_stride;
    prep_cfg->ver_stride    = p->ver_stride;
    prep_cfg->format        = p->fmt;
    prep_cfg->rotation      = MPP_ENC_ROT_0;
    ret = mpi->control(ctx, MPP_ENC_SET_PREP_CFG, prep_cfg);
    if (ret) {
        printf("mpi control enc set prep cfg failed ret %d\n", ret);
        goto RET;
    }

    ret = mpi->control(ctx, MPP_SET_OUTPUT_TIMEOUT, (MppParam)&block);
    if (MPP_OK != ret) {
        printf("mpi->control MPP_SET_OUTPUT_TIMEOUT failed\n");
        goto RET;
    }

    rc_cfg->change  = MPP_ENC_RC_CFG_CHANGE_ALL;
    rc_cfg->rc_mode = MPP_ENC_RC_MODE_CBR;
    rc_cfg->quality = MPP_ENC_RC_QUALITY_MEDIUM;

    if (rc_cfg->rc_mode == MPP_ENC_RC_MODE_CBR) {
        /* constant bitrate has very small bps range of 1/16 bps */
        rc_cfg->bps_target   = p->bps;
        rc_cfg->bps_max      = p->bps * 17 / 16;
        rc_cfg->bps_min      = p->bps * 15 / 16;
    } else if (rc_cfg->rc_mode ==  MPP_ENC_RC_MODE_VBR) {
        if (rc_cfg->quality == MPP_ENC_RC_QUALITY_CQP) {
            /* constant QP does not have bps */
            rc_cfg->bps_target   = -1;
            rc_cfg->bps_max      = -1;
            rc_cfg->bps_min      = -1;
        } else {
            /* variable bitrate has large bps range */
            rc_cfg->bps_target   = p->bps;
            rc_cfg->bps_max      = p->bps * 17 / 16;
            rc_cfg->bps_min      = p->bps * 1 / 16;
        }
    }

    /* fix input / output frame rate */
    rc_cfg->fps_in_flex      = 0;
    rc_cfg->fps_in_num       = p->fps;
    rc_cfg->fps_in_denorm    = 1;
    rc_cfg->fps_out_flex     = 0;
    rc_cfg->fps_out_num      = p->fps;
    rc_cfg->fps_out_denorm   = 1;

    rc_cfg->gop              = p->gop;
    rc_cfg->skip_cnt         = 0;

    printf("mpi_enc_test bps %d fps %d gop %d\n",
            rc_cfg->bps_target, rc_cfg->fps_out_num, rc_cfg->gop);
    ret = mpi->control(ctx, MPP_ENC_SET_RC_CFG, rc_cfg);
    if (ret) {
        printf("mpi control enc set rc cfg failed ret %d\n", ret);
        goto RET;
    }

    codec_cfg->coding = p->type;
    switch (codec_cfg->coding) {
    case MPP_VIDEO_CodingAVC : {
        codec_cfg->h264.change = MPP_ENC_H264_CFG_CHANGE_PROFILE |
                                 MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                                 MPP_ENC_H264_CFG_CHANGE_TRANS_8x8;
        /*
         * H.264 profile_idc parameter
         * 66  - Baseline profile
         * 77  - Main profile
         * 100 - High profile
         */
        codec_cfg->h264.profile  = 100;
        /*
         * H.264 level_idc parameter
         * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
         * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
         * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
         * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
         * 50 / 51 / 52         - 4K@30fps
         */
        codec_cfg->h264.level    = 40;
        codec_cfg->h264.entropy_coding_mode  = 1;
        codec_cfg->h264.cabac_init_idc  = 0;
        codec_cfg->h264.transform8x8_mode = 1;
    } break;
    case MPP_VIDEO_CodingMJPEG : {
        codec_cfg->jpeg.change  = MPP_ENC_JPEG_CFG_CHANGE_QP;
        codec_cfg->jpeg.quant   = 7;
    } break;
    case MPP_VIDEO_CodingVP8 : {
    } break;
    case MPP_VIDEO_CodingHEVC : {
        codec_cfg->h265.change = MPP_ENC_H265_CFG_INTRA_QP_CHANGE;
        codec_cfg->h265.intra_qp = 26;
    } break;
    default : {
        printf("support encoder coding type %d\n", codec_cfg->coding);
    } break;
    }
    ret = mpi->control(ctx, MPP_ENC_SET_CODEC_CFG, codec_cfg);
    if (ret) {
        printf("mpi control enc set codec cfg failed ret %d\n", ret);
        goto RET;
    }

    /* optional */
    p->sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
    ret = mpi->control(ctx, MPP_ENC_SET_SEI_CFG, &p->sei_mode);
    if (ret) {
        printf("mpi control enc set sei cfg failed ret %d\n", ret);
        goto RET;
    }

RET:
    return ret;
}

static MPP_RET test_mpp_run(MpiEncTestData *p, int fd, size_t size)
{
    MPP_RET ret;
    MppApi *mpi;
    MppCtx ctx;
    MppBuffer buf = NULL;

    if (NULL == p)
        return MPP_ERR_NULL_PTR;

    mpi = p->mpi;
    ctx = p->ctx;

    if (p->type == MPP_VIDEO_CodingAVC) {
        MppPacket packet = NULL;
        ret = mpi->control(ctx, MPP_ENC_GET_EXTRA_INFO, &packet);
        if (ret) {
            printf("mpi control enc get extra info failed\n");
            goto RET;
        }

        /* get and write sps/pps for H.264 */
        if (packet) {
            void *ptr   = mpp_packet_get_pos(packet);
            size_t len  = mpp_packet_get_length(packet);

            if (p->fp_output)
                fwrite(ptr, 1, len, p->fp_output);

            packet = NULL;
        }
    }

    do {
        MppFrame frame = NULL;

        if (p->packet)
            mpp_packet_deinit(&p->packet);
        p->packet = NULL;

        ret = mpp_frame_init(&frame);
        if (ret) {
            printf("mpp_frame_init failed\n");
            goto RET;
        }

        mpp_frame_set_width(frame, p->width);
        mpp_frame_set_height(frame, p->height);
        mpp_frame_set_hor_stride(frame, p->hor_stride);
        mpp_frame_set_ver_stride(frame, p->ver_stride);
        mpp_frame_set_fmt(frame, p->fmt);
#if 0
        mpp_frame_set_buffer(frame, p->frm_buf);
#else
        MppBufferInfo inputCommit;
        memset(&inputCommit, 0, sizeof(inputCommit));
        inputCommit.type = MPP_BUFFER_TYPE_ION;
        inputCommit.size = size;
        inputCommit.fd = fd;
        ret = mpp_buffer_import(&buf, &inputCommit);
        if (ret) {
            printf("import input picture buffer failed\n");
            goto RET;
        }
        mpp_frame_set_buffer(frame, buf);
#endif
        mpp_frame_set_eos(frame, p->frm_eos);

        ret = mpi->encode_put_frame(ctx, frame);
        mpp_frame_deinit(&frame);
        if (ret) {
            printf("mpp encode put frame failed\n");
            goto RET;
        }

        ret = mpi->encode_get_packet(ctx, &p->packet);
        if (ret) {
            printf("mpp encode get packet failed\n");
            goto RET;
        }

        if (p->packet) {
            // write packet to file here
            void *ptr   = mpp_packet_get_pos(p->packet);
            size_t len  = mpp_packet_get_length(p->packet);

            p->pkt_eos = mpp_packet_get_eos(p->packet);

            if (p->fp_output)
                fwrite(ptr, 1, len, p->fp_output);
            p->enc_data = ptr;
            p->enc_len = len;
        }
    } while (0);
RET:

    if (buf)
        mpp_buffer_put(buf);
    return ret;
}

MPP_RET mpi_enc_test_init(MpiEncTestCmd *cmd, MpiEncTestData **data)
{
    MPP_RET ret = MPP_OK;
    MpiEncTestData *p = NULL;

    printf("mpi_enc_test start\n");

    ret = test_ctx_init(&p, cmd);
    if (ret) {
        printf("test data init failed ret %d\n", ret);
        return ret;
    }
    *data = p;

#if 0
    ret = mpp_buffer_get(NULL, &p->frm_buf, p->frame_size);
    if (ret) {
        printf("failed to get buffer for input frame ret %d\n", ret);
        return ret;
    }
#endif

    printf("mpi_enc_test encoder test start w %d h %d type %d\n",
            p->width, p->height, p->type);

    // encoder demo
    ret = mpp_create(&p->ctx, &p->mpi);
    if (ret) {
        printf("mpp_create failed ret %d\n", ret);
        return ret;
    }

    ret = mpp_init(p->ctx, MPP_CTX_ENC, p->type);
    if (ret) {
        printf("mpp_init failed ret %d\n", ret);
        return ret;
    }

    ret = test_mpp_setup(p);
    if (ret) {
        printf("test mpp setup failed ret %d\n", ret);
        return ret;
    }
}

MPP_RET mpi_enc_test_run(MpiEncTestData **data, int fd, size_t size)
{
    MPP_RET ret = MPP_OK;
    MpiEncTestData *p = *data;

    ret = test_mpp_run(p, fd, size);
    if (ret)
        printf("test mpp run failed ret %d\n", ret);
    return ret;
}

MPP_RET mpi_enc_test_deinit(MpiEncTestData **data)
{
    MPP_RET ret = MPP_OK;
    MpiEncTestData *p = *data;

    if (p->packet)
        mpp_packet_deinit(&p->packet);

    ret = p->mpi->reset(p->ctx);
    if (ret) {
        printf("mpi->reset failed\n");
    }

    if (p->ctx) {
        mpp_destroy(p->ctx);
        p->ctx = NULL;
    }

#if 0
    if (p->frm_buf) {
        mpp_buffer_put(p->frm_buf);
        p->frm_buf = NULL;
    }
#endif

    if (MPP_OK != ret)
        printf("mpi_enc_test failed ret %d\n", ret);

    test_ctx_deinit(&p);

    return ret;
}

static void mpi_enc_test_help()
{
    printf("usage: mpi_enc_test [options]\n");
//    show_options(mpi_enc_cmd);
    mpp_show_support_format();
}

static RK_S32 mpi_enc_test_parse_options(int argc, char **argv, MpiEncTestCmd* cmd)
{
    const char *opt;
    const char *next;
    RK_S32 optindex = 1;
    RK_S32 handleoptions = 1;
    RK_S32 err = MPP_NOK;

    if ((argc < 2) || (cmd == NULL)) {
        err = 1;
        return err;
    }

    /* parse options */
    while (optindex < argc) {
        opt  = (const char*)argv[optindex++];
        next = (const char*)argv[optindex];

        if (handleoptions && opt[0] == '-' && opt[1] != '\0') {
            if (opt[1] == '-') {
                if (opt[2] != '\0') {
                    opt++;
                } else {
                    handleoptions = 0;
                    continue;
                }
            }

            opt++;

            switch (*opt) {
            case 'i':
                if (next) {
                    strncpy(cmd->file_input, next, MAX_FILE_NAME_LENGTH);
                    cmd->file_input[strlen(next)] = '\0';
                    cmd->have_input = 1;
                } else {
                    printf("input file is invalid\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'o':
                if (next) {
                    strncpy(cmd->file_output, next, MAX_FILE_NAME_LENGTH);
                    cmd->file_output[strlen(next)] = '\0';
                    cmd->have_output = 1;
                } else {
                    printf("output file is invalid\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'd':
                if (next) {
                    cmd->debug = atoi(next);;
                } else {
                    printf("invalid debug flag\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'w':
                if (next) {
                    cmd->width = atoi(next);
                } else {
                    printf("invalid input width\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'h':
                if ((*(opt + 1) != '\0') && !strncmp(opt, "help", 4)) {
                    mpi_enc_test_help();
                    err = 1;
                    goto PARSE_OPINIONS_OUT;
                } else if (next) {
                    cmd->height = atoi(next);
                } else {
                    printf("input height is invalid\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'f':
                if (next) {
                    cmd->format = (MppFrameFormat)atoi(next);
                    err = ((cmd->format >= MPP_FMT_YUV_BUTT && cmd->format < MPP_FRAME_FMT_RGB) ||
                           cmd->format >= MPP_FMT_RGB_BUTT);
                }

                if (!next || err) {
                    printf("invalid input format %d\n", cmd->format);
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 't':
                if (next) {
                    cmd->type = (MppCodingType)atoi(next);
                    err = mpp_check_support_format(MPP_CTX_ENC, cmd->type);
                }

                if (!next || err) {
                    printf("invalid input coding type\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'n':
                if (next) {
                    cmd->num_frames = atoi(next);
                } else {
                    printf("invalid input max number of frames\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            default:
                goto PARSE_OPINIONS_OUT;
                break;
            }

            optindex++;
        }
    }

    err = 0;

PARSE_OPINIONS_OUT:
    return err;
}

static void mpi_enc_test_show_options(MpiEncTestCmd* cmd)
{
    printf("cmd parse result:\n");
    printf("input  file name: %s\n", cmd->file_input);
    printf("output file name: %s\n", cmd->file_output);
    printf("width      : %d\n", cmd->width);
    printf("height     : %d\n", cmd->height);
    printf("format     : %d\n", cmd->format);
    printf("type       : %d\n", cmd->type);
    printf("debug flag : %x\n", cmd->debug);
}
void mpi_enc_cmd_config(MpiEncTestCmd *cmd, int width, int height,int fcc)
{
    memset((void*)cmd, 0, sizeof(*cmd));
    cmd->width = width;
    cmd->height = height;
    cmd->format = g_format;
    switch (fcc) {
    case V4L2_PIX_FMT_YUYV:
        printf("%s: yuyv not need mpp encodec: %d\n", __func__, fcc);
        break;
    case V4L2_PIX_FMT_MJPEG:
        cmd->type = MPP_VIDEO_CodingMJPEG;
        break;
    case V4L2_PIX_FMT_H264:
        cmd->type = MPP_VIDEO_CodingAVC;
        break;
    default:
        printf("%s: not support fcc: %d\n", __func__, fcc);
        break;
    }

}

void mpi_enc_cmd_config_mjpg(MpiEncTestCmd *cmd, int width, int height)
{
    memset((void*)cmd, 0, sizeof(*cmd));
    cmd->width = width;
    cmd->height = height;
    cmd->format = g_format;
    cmd->type = MPP_VIDEO_CodingMJPEG;
}

void mpi_enc_cmd_config_h264(MpiEncTestCmd *cmd, int width, int height)
{
    memset((void*)cmd, 0, sizeof(*cmd));
    cmd->width = width;
    cmd->height = height;
    cmd->format = g_format;
    cmd->type = MPP_VIDEO_CodingAVC;
}
void mpi_enc_set_format(MppFrameFormat format)
{
    g_format = format;
}
int mpi_enc_get_h264_extra(MpiEncTestData *p, void *buffer, size_t *size)
{
    MPP_RET ret;
    MppApi *mpi;
    MppCtx ctx;
    if (NULL == p) {
        *size = 0;
        return -1;
    }
    mpi = p->mpi;
    ctx = p->ctx;
    MppPacket packet = NULL;
    ret = mpi->control(ctx, MPP_ENC_GET_EXTRA_INFO, &packet);
    if (ret) {
        printf("mpi control enc get extra info failed\n");
        *size = 0;
        return -1;
    }
    if (packet) {
        void *ptr   = mpp_packet_get_pos(packet);
        size_t len  = mpp_packet_get_length(packet);
        printf("%s: len = %d\n", __func__, len);
        if (*size >= len) {
            memcpy(buffer, ptr, len);
            *size = len;
        } else {
            printf("%s: input buffer size = %d\n", __func__, *size);
            *size = 0;
        }
        packet = NULL;
    }
    return 0;
}

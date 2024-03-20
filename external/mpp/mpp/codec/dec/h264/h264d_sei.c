/*
*
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

#define MODULE_TAG  "h264d_sei"

#include <stdio.h>
#include <string.h>

#include "h264d_global.h"
#include "h264d_sps.h"
#include "h264d_sei.h"
#include "h2645d_sei.h"

static MPP_RET interpret_picture_timing_info(
    BitReadCtx_t *p_bitctx,
    H264_SEI_t *sei_msg,
    H264dVideoCtx_t *p_videoctx)
{
    RK_S32 cpb_removal_delay_length = 0;
    RK_S32 dpb_output_delay_length = 0;
    RK_S32 time_offset_length = 0;
    RK_S32 cpb_dpb_delays_present_flag = 0;
    RK_U32 i = 0;
    H264_SEI_PIC_TIMING_t *pic_timing = NULL;
    RK_U32 num_clock_ts[9] = {1, 1, 1, 2, 2, 3, 3, 2, 3};
    struct h264_vui_t *vui_seq_parameters = NULL;
    RK_U32 seq_parameter_set_id = sei_msg->seq_parameter_set_id;

    if (seq_parameter_set_id >= MAXSPS || !p_videoctx->spsSet[seq_parameter_set_id]) {
        H264D_ERR("seq_parameter_set_id %d may be invalid\n", seq_parameter_set_id);
        goto __BITREAD_ERR;
    }
    vui_seq_parameters = &(p_videoctx->spsSet[sei_msg->seq_parameter_set_id]->vui_seq_parameters);
    pic_timing = &(sei_msg->pic_timing);

    if (vui_seq_parameters->nal_hrd_parameters_present_flag) {
        cpb_removal_delay_length =
            vui_seq_parameters->nal_hrd_parameters.cpb_removal_delay_length_minus1;
        dpb_output_delay_length =
            vui_seq_parameters->nal_hrd_parameters.dpb_output_delay_length_minus1;
        time_offset_length =
            vui_seq_parameters->nal_hrd_parameters.time_offset_length;
        cpb_dpb_delays_present_flag = 1;
    } else if (vui_seq_parameters->vcl_hrd_parameters_present_flag) {
        cpb_removal_delay_length =
            vui_seq_parameters->vcl_hrd_parameters.cpb_removal_delay_length_minus1;
        dpb_output_delay_length =
            vui_seq_parameters->vcl_hrd_parameters.dpb_output_delay_length_minus1;
        time_offset_length =
            vui_seq_parameters->vcl_hrd_parameters.time_offset_length;
        cpb_dpb_delays_present_flag = 1;
    }

    if (cpb_dpb_delays_present_flag) {
        READ_BITS(p_bitctx, cpb_removal_delay_length, &pic_timing->cpb_removal_delay);
        READ_BITS(p_bitctx, dpb_output_delay_length, &pic_timing->dpb_output_delay);
    }

    if (vui_seq_parameters->pic_struct_present_flag) {
        READ_BITS(p_bitctx, 4, &pic_timing->pic_struct);
        if (pic_timing->pic_struct > 8 || pic_timing->pic_struct < 0) {
            goto __BITREAD_ERR;
        }

        for (i = 0; i < num_clock_ts[pic_timing->pic_struct]; i++) {
            READ_BITS(p_bitctx, 1, &pic_timing->clock_timestamp_flag[i]);

            if (pic_timing->clock_timestamp_flag[i]) {
                READ_BITS(p_bitctx, 2, &pic_timing->ct_type[i]);
                READ_BITS(p_bitctx, 1, &pic_timing->nuit_field_based_flag[i]);

                READ_BITS(p_bitctx, 5, &pic_timing->counting_type[i]);
                if (pic_timing->counting_type[i] > 6
                    || pic_timing->counting_type[i] < 0) {
                    goto __BITREAD_ERR;
                }

                READ_BITS(p_bitctx, 1, &pic_timing->full_timestamp_flag[i]);
                READ_BITS(p_bitctx, 1, &pic_timing->discontinuity_flag[i]);
                READ_BITS(p_bitctx, 1, &pic_timing->cnt_dropped_flag[i]);

                READ_BITS(p_bitctx, 8, &pic_timing->n_frames[i]);

                if (pic_timing->full_timestamp_flag[i]) {
                    READ_BITS(p_bitctx, 6, &pic_timing->seconds_value[i]);
                    if (pic_timing->seconds_value[i] > 59) {
                        goto __BITREAD_ERR;
                    }

                    READ_BITS(p_bitctx, 6, &pic_timing->minutes_value[i]);
                    if (pic_timing->minutes_value[i] > 59) {
                        goto __BITREAD_ERR;
                    }

                    READ_BITS(p_bitctx, 5, &pic_timing->hours_value[i]);
                    if (pic_timing->hours_value[i] > 23) {
                        goto __BITREAD_ERR;
                    }
                } else {
                    READ_BITS(p_bitctx, 1, &pic_timing->seconds_flag[i]);
                    if (pic_timing->seconds_flag[i]) {
                        READ_BITS(p_bitctx, 6, &pic_timing->seconds_value[i]);
                        if (pic_timing->seconds_value[i] > 59) {
                            goto __BITREAD_ERR;
                        }

                        READ_BITS(p_bitctx, 1, &pic_timing->minutes_flag[i]);
                        if (pic_timing->minutes_flag[i]) {
                            READ_BITS(p_bitctx, 6, &pic_timing->minutes_value[i]);
                            if (pic_timing->minutes_value[i] > 59) {
                                goto __BITREAD_ERR;
                            }

                            READ_BITS(p_bitctx, 1, &pic_timing->hours_flag[i]);
                            if (pic_timing->hours_flag[i]) {
                                READ_BITS(p_bitctx, 5, &pic_timing->hours_value[i]);
                                if (pic_timing->hours_value[i] > 23) {
                                    goto __BITREAD_ERR;
                                }
                            }
                        }
                    }
                }
                if (time_offset_length) {
                    RK_S32 tmp;
                    READ_BITS(p_bitctx, time_offset_length, &tmp);
                    /* following "converts" timeOffsetLength-bit signed
                     * integer into i32 */
                    /*lint -save -e701 -e702 */
                    tmp <<= (32 - time_offset_length);
                    tmp >>= (32 - time_offset_length);
                    /*lint -restore */
                    pic_timing->time_offset[i] = tmp;
                } else
                    pic_timing->time_offset[i] = 0;
            }
        }
    }

    return MPP_OK;
__BITREAD_ERR:
    return MPP_ERR_STREAM;
}

static MPP_RET interpret_buffering_period_info(
    BitReadCtx_t *p_bitctx,
    H264_SEI_t *sei_msg,
    H264dVideoCtx_t *p_videoctx)
{
    MPP_RET ret = MPP_ERR_UNKNOW;
    RK_U32 i = 0;
    RK_U32 seq_parameter_set_id = sei_msg->seq_parameter_set_id;
    struct h264_vui_t *vui_seq_parameters = NULL;

    READ_UE(p_bitctx, &seq_parameter_set_id);

    if (seq_parameter_set_id >= MAXSPS || !p_videoctx->spsSet[seq_parameter_set_id]) {
        H264D_ERR("seq_parameter_set_id %d may be invalid\n", seq_parameter_set_id);
        goto __BITREAD_ERR;
    }

    sei_msg->seq_parameter_set_id = seq_parameter_set_id;
    vui_seq_parameters = &(p_videoctx->spsSet[sei_msg->seq_parameter_set_id]->vui_seq_parameters);

    if (vui_seq_parameters->nal_hrd_parameters_present_flag) {
        for (i = 0; i < vui_seq_parameters->nal_hrd_parameters.cpb_cnt_minus1; i++) {
            SKIP_BITS(p_bitctx,
                      vui_seq_parameters->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1); //initial_cpb_removal_delay
            SKIP_BITS(p_bitctx,
                      vui_seq_parameters->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1); //initial_cpb_removal_delay_offset
        }
    }

    if (vui_seq_parameters->vcl_hrd_parameters_present_flag) {
        for (i = 0; i < vui_seq_parameters->vcl_hrd_parameters.cpb_cnt_minus1; i++) {
            SKIP_BITS(p_bitctx,
                      vui_seq_parameters->vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1); //initial_cpb_removal_delay
            SKIP_BITS(p_bitctx,
                      vui_seq_parameters->vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1); //initial_cpb_removal_delay_offset
        }
    }

    return ret = MPP_OK;
__BITREAD_ERR:
    ret = p_bitctx->ret;
    return ret;

}

static MPP_RET interpret_recovery_point(BitReadCtx_t *p_bitctx, H264dVideoCtx_t *p_videoctx)
{
    RK_S32 recovery_frame_cnt = 0;

    READ_UE(p_bitctx, &recovery_frame_cnt);

    if (recovery_frame_cnt >= (1 << 16) || recovery_frame_cnt < 0) {
        H264D_DBG(H264D_DBG_SEI, "recovery_frame_cnt %d, is out of range %d",
                  recovery_frame_cnt, p_videoctx->max_frame_num);
        return MPP_ERR_STREAM;
    }

    memset(&p_videoctx->recovery, 0, sizeof(RecoveryPoint));

    p_videoctx->recovery.valid_flag = 1;
    p_videoctx->recovery.recovery_frame_cnt = recovery_frame_cnt;
    H264D_DBG(H264D_DBG_SEI, "Recovery point: frame_cnt %d", p_videoctx->recovery.recovery_frame_cnt);
    return MPP_OK;
__BITREAD_ERR:
    return p_bitctx->ret;
}

/*!
***********************************************************************
* \brief
*    parse SEI information
***********************************************************************
*/
//extern "C"
MPP_RET process_sei(H264_SLICE_t *currSlice)
{
    RK_S32  tmp_byte = 0;
    MPP_RET ret = MPP_ERR_UNKNOW;
    H264_SEI_t *sei_msg  = NULL;
    BitReadCtx_t *p_bitctx = &currSlice->p_Cur->bitctx;
    BitReadCtx_t payload_bitctx;
    RK_S32 i = 0;

    if (!currSlice->p_Cur->sei)
        currSlice->p_Cur->sei = mpp_calloc(H264_SEI_t, 1);

    sei_msg = currSlice->p_Cur->sei;
    sei_msg->mvc_scalable_nesting_flag = 0;  //init to false
    sei_msg->p_Dec = currSlice->p_Dec;
    do {
        tmp_byte = 0xFF;
        sei_msg->type = 0;
        while (tmp_byte == 0xFF) {
            READ_BITS(p_bitctx, 8, &tmp_byte);
            sei_msg->type += tmp_byte;
        }

        tmp_byte = 0xFF;
        sei_msg->payload_size = 0;
        while (tmp_byte == 0xFF) {
            READ_BITS(p_bitctx, 8, &tmp_byte);
            sei_msg->payload_size += tmp_byte;
        }

        H264D_DBG(H264D_DBG_SEI, "SEI type %d, payload size: %d\n", sei_msg->type, sei_msg->payload_size);

        memset(&payload_bitctx, 0, sizeof(payload_bitctx));
        mpp_set_bitread_ctx(&payload_bitctx, p_bitctx->data_, sei_msg->payload_size);
        mpp_set_bitread_pseudo_code_type(&payload_bitctx, PSEUDO_CODE_H264_H265_SEI);

        switch (sei_msg->type) {
        case H264_SEI_BUFFERING_PERIOD:
            FUN_CHECK(ret = interpret_buffering_period_info(&payload_bitctx, sei_msg, currSlice->p_Vid));
            break;
        case H264_SEI_PIC_TIMING:
            FUN_CHECK(interpret_picture_timing_info(&payload_bitctx, sei_msg, currSlice->p_Vid));
            break;
        case H264_SEI_USER_DATA_UNREGISTERED:
            FUN_CHECK(check_encoder_sei_info(&payload_bitctx, sei_msg->payload_size, &currSlice->p_Vid->deny_flag));

            if (currSlice->p_Vid->deny_flag)
                H264D_DBG(H264D_DBG_SEI, "Bitstream is encoded by special encoder.");
            break;
        case H264_SEI_RECOVERY_POINT:
            FUN_CHECK(interpret_recovery_point(&payload_bitctx, currSlice->p_Vid));
            break;
        default:
            H264D_DBG(H264D_DBG_SEI, "Skip parsing SEI type %d\n", sei_msg->type);
            break;
        }

        H264D_DBG(H264D_DBG_SEI, "After parsing SEI %d, bits left int cur byte %d, bits_used %d, bytes left %d",
                  sei_msg->type, payload_bitctx.num_remaining_bits_in_curr_byte_, payload_bitctx.used_bits,
                  payload_bitctx.bytes_left_);

        for (i = 0; i < sei_msg->payload_size; i++)
            SKIP_BITS(p_bitctx, 8);
    } while (mpp_has_more_rbsp_data(p_bitctx));    // more_rbsp_data()  msg[offset] != 0x80

    return ret = MPP_OK;
__BITREAD_ERR:
__FAILED:
    return ret;
}

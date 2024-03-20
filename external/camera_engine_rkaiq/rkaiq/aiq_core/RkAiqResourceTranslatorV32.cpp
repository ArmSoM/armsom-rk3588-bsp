/*
 * Copyright (c) 2022 Rockchip Eletronics Co., Ltd.
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
#include "RkAiqResourceTranslatorV32.h"

#include "common/rkisp32-config.h"
#include "isp20/Isp20Evts.h"
#include "isp20/Isp20StatsBuffer.h"

//#define AE_STATS_DEBUG
#define MAX_8BITS (1 << 8) - 1
#define MAX_10BITS (1 << 10) - 1
#define MAX_12BITS (1 << 12) - 1
#define MAX_29BITS (1 << 29) - 1
#define MAX_32BITS  0xffffffff

namespace RkCam {

RkAiqResourceTranslatorV32::RkAiqResourceTranslatorV32() {}

#if defined(RKAIQ_HAVE_MULTIISP) && defined(ISP_HW_V32)
void JudgeWinLocation32(
    struct isp2x_window* ori_win,
    WinSplitMode& mode,
    RkAiqResourceTranslatorV3x::Rectangle left_isp_rect_,
    RkAiqResourceTranslatorV3x::Rectangle right_isp_rect_
) {

    if (ori_win->h_offs + ori_win->h_size <= left_isp_rect_.w) {
        mode = LEFT_MODE;
    } else if(ori_win->h_offs >= right_isp_rect_.x) {
        mode = RIGHT_MODE;
    } else {
        if ((ori_win->h_offs + ori_win->h_size / 2) <= left_isp_rect_.w
                && right_isp_rect_.x <= (ori_win->h_offs + ori_win->h_size / 2)) {
            mode = LEFT_AND_RIGHT_MODE;
        }
        else {

            if ((ori_win->h_offs + ori_win->h_size / 2) < right_isp_rect_.x) {

                u16 h_size_tmp1 = left_isp_rect_.w - ori_win->h_offs;
                u16 h_size_tmp2 = (right_isp_rect_.x - ori_win->h_offs) * 2;

                if (abs(ori_win->h_size - h_size_tmp1) < abs(ori_win->h_size - h_size_tmp2))
                    mode = LEFT_MODE;
                else
                    mode = LEFT_AND_RIGHT_MODE;
            }
            else {

                u16 h_size_tmp1 = ori_win->h_offs + ori_win->h_size - right_isp_rect_.x;
                u16 h_size_tmp2 = (ori_win->h_offs + ori_win->h_size - left_isp_rect_.w) * 2;

                if (abs(ori_win->h_size - h_size_tmp1) < abs(ori_win->h_size - h_size_tmp2))
                    mode = RIGHT_MODE;
                else
                    mode = LEFT_AND_RIGHT_MODE;
            }
        }
    }
}

void MergeAwbHistBinStats(
    unsigned int *merge_stats,
    u16 *left_stats,
    u16 *right_stats,
    WinSplitMode mode
) {
    u32 tmp1, tmp2;
    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_WP_HIST_BIN_NUM; i++) {
            tmp1 = left_stats[i];
            if(left_stats[i] & 0x8000) {
                tmp1 = left_stats[i] & 0x7FFF;
                tmp1 *=    (1 << 3);
            }
            merge_stats[i] = tmp1;
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_WP_HIST_BIN_NUM; i++) {
            tmp2 = right_stats[i];
            if(right_stats[i] & 0x8000) {
                tmp2 = right_stats[i] & 0x7FFF;
                tmp2 *=    (1 << 3);
            }
            merge_stats[i] = tmp2;
        }

        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_WP_HIST_BIN_NUM; i++) {
            tmp1 = left_stats[i];
            if(left_stats[i] & 0x8000) {
                tmp1 = left_stats[i] & 0x7FFF;
                tmp1 *=    (1 << 3);
            }
            tmp2 = right_stats[i];
            if(right_stats[i] & 0x8000) {
                tmp2 = right_stats[i] & 0x7FFF;
                tmp2 *=    (1 << 3);
            }
            merge_stats[i] = tmp1 + tmp2;

        }
        break;
    }
}

void MergeAwbWinStats(
    rk_aiq_awb_stat_wp_res_light_v201_t *merge_stats,
    struct isp32_rawawb_meas_stat *left_stats,
    struct isp32_rawawb_meas_stat *right_stats,
    int lightNum,
    WinSplitMode mode
) {
    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < lightNum; i++) {
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue =
                left_stats->sum[i].rgain_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue =
                left_stats->sum[i].bgain_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo =
                left_stats->sum[i].wp_num_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue =
                left_stats->sum[i].rgain_big;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue =
                left_stats->sum[i].bgain_big;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo =
                left_stats->sum[i].wp_num_big;
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < lightNum; i++) {
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue =
                right_stats->sum[i].rgain_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue =
                right_stats->sum[i].bgain_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo =
                right_stats->sum[i].wp_num_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue =
                right_stats->sum[i].rgain_big;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue =
                right_stats->sum[i].bgain_big;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo =
                right_stats->sum[i].wp_num_big;
        }
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < lightNum; i++) {
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue =
                left_stats->sum[i].rgain_nor + right_stats->sum[i].rgain_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue =
                left_stats->sum[i].bgain_nor + right_stats->sum[i].bgain_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo =
                left_stats->sum[i].wp_num_nor + right_stats->sum[i].wp_num_nor;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue =
                left_stats->sum[i].rgain_big + right_stats->sum[i].rgain_big;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue =
                left_stats->sum[i].bgain_big + right_stats->sum[i].bgain_big;
            merge_stats[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo =
                left_stats->sum[i].wp_num_big + right_stats->sum[i].wp_num_big;
        }
        break;
    default:
        break;
    }
}

void AwbStatOverflowCheckandFixed(struct isp2x_window* win, rk_aiq_awb_blk_stat_mode_v201_t blkMeasureMode, bool blkStatisticsWithLumaWeightEn, rk_aiq_awb_xy_type_v201_t xyRangeTypeForWpHist,
                                  int lightNum, struct isp32_rawawb_meas_stat *stats)
{
    int w, h;
    w = win->h_size;
    h = win->v_size;
    float factor1 = (float)((1 << (RK_AIQ_AWB_WP_WEIGHT_BIS_V201 + 1)) - 1) / ((1 << RK_AIQ_AWB_WP_WEIGHT_BIS_V201) - 1);
    if(w * h > RK_AIQ_AWB_STAT_MAX_AREA) {
        LOGD_AWB("%s ramdata and ro_wp_num2 is fixed", __FUNCTION__);
        for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
            stats->ramdata[i].wp = (float)stats->ramdata[i].wp * factor1 + 0.5 ;
            stats->ramdata[i].r = (float)stats->ramdata[i].r * factor1 + 0.5 ;
            stats->ramdata[i].g = (float)stats->ramdata[i].g * factor1 + 0.5 ;
            stats->ramdata[i].b = (float)stats->ramdata[i].b * factor1 + 0.5 ;
        }
        if(xyRangeTypeForWpHist == RK_AIQ_AWB_XY_TYPE_BIG_V201) {
            for(int i = 0; i < lightNum; i++) {
                stats->sum[i].wp_num2 = stats->sum[i].wp_num_nor >> RK_AIQ_WP_GAIN_FRAC_BIS;
            }
        } else {
            for(int i = 0; i < lightNum; i++) {
                stats->sum[i].wp_num2 = stats->sum[i].wp_num_nor >> RK_AIQ_WP_GAIN_FRAC_BIS;
            }
        }
    } else {
        if(RK_AIQ_AWB_BLK_STAT_MODE_REALWP_V201 == blkMeasureMode && blkStatisticsWithLumaWeightEn) {
            for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
                stats->ramdata[i].wp = (float)stats->ramdata[i].wp * factor1 + 0.5 ;
                stats->ramdata[i].r = (float)stats->ramdata[i].r * factor1 + 0.5 ;
                stats->ramdata[i].g = (float)stats->ramdata[i].g * factor1 + 0.5 ;
                stats->ramdata[i].b = (float)stats->ramdata[i].b * factor1 + 0.5 ;
            }
        }
    }
}

void MergeAwbBlkStats(
    struct isp2x_window* ori_win,
    struct isp2x_window* left_win,
    struct isp2x_window* right_win,
    rk_aiq_awb_stat_blk_res_v201_t *merge_stats,
    struct isp32_rawawb_meas_stat *left_stats,
    struct isp32_rawawb_meas_stat *right_stats,
    WinSplitMode mode
) {
    u8 wnd_num = sqrt(RK_AIQ_AWB_GRID_NUM_TOTAL);

    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < wnd_num; i++) {
            for(int j = 0; j < wnd_num; j++) {
                merge_stats[i * wnd_num + j].Rvalue = left_stats->ramdata[i * wnd_num + j].r;
                merge_stats[i * wnd_num + j].Gvalue = left_stats->ramdata[i * wnd_num + j].g;
                merge_stats[i * wnd_num + j].Bvalue = left_stats->ramdata[i * wnd_num + j].b;
                merge_stats[i * wnd_num + j].WpNo = left_stats->ramdata[i * wnd_num + j].wp;
            }
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < wnd_num; i++) {
            for(int j = 0; j < wnd_num; j++) {
                merge_stats[i * wnd_num + j].Rvalue = right_stats->ramdata[i * wnd_num + j].r;
                merge_stats[i * wnd_num + j].Gvalue = right_stats->ramdata[i * wnd_num + j].g;
                merge_stats[i * wnd_num + j].Bvalue = right_stats->ramdata[i * wnd_num + j].b;
                merge_stats[i * wnd_num + j].WpNo = right_stats->ramdata[i * wnd_num + j].wp;
            }
        }
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < wnd_num; i++) {
            for(int j = 0; j < wnd_num; j++) {
                if(j < wnd_num / 2) {
                    merge_stats[i * wnd_num + j].Rvalue = left_stats->ramdata[i * wnd_num + j * 2].r + left_stats->ramdata[i * wnd_num + j * 2 + 1].r;
                    merge_stats[i * wnd_num + j].Gvalue = left_stats->ramdata[i * wnd_num + j * 2].g + left_stats->ramdata[i * wnd_num + j * 2 + 1].g;
                    merge_stats[i * wnd_num + j].Bvalue = left_stats->ramdata[i * wnd_num + j * 2].b +  left_stats->ramdata[i * wnd_num + j * 2 + 1].b;
                    merge_stats[i * wnd_num + j].WpNo = left_stats->ramdata[i * wnd_num + j * 2].wp + left_stats->ramdata[i * wnd_num + j * 2 + 1].wp;
                } else if(j > wnd_num / 2) {
                    merge_stats[i * wnd_num + j].Rvalue = right_stats->ramdata[i * wnd_num + j * 2 - wnd_num].r + right_stats->ramdata[i * wnd_num + j * 2 - wnd_num + 1].r;
                    merge_stats[i * wnd_num + j].Gvalue = right_stats->ramdata[i * wnd_num + j * 2 - wnd_num].g + right_stats->ramdata[i * wnd_num + j * 2 - wnd_num + 1].g;
                    merge_stats[i * wnd_num + j].Bvalue = right_stats->ramdata[i * wnd_num + j * 2 - wnd_num].b + right_stats->ramdata[i * wnd_num + j * 2 - wnd_num + 1].b;
                    merge_stats[i * wnd_num + j].WpNo = right_stats->ramdata[i * wnd_num + j * 2 - wnd_num].wp + right_stats->ramdata[i * wnd_num + j * 2 - wnd_num + 1].wp;
                } else {
                    merge_stats[i * wnd_num + j].Rvalue = left_stats->ramdata[i * wnd_num + wnd_num - 1].r + right_stats->ramdata[i * wnd_num + 0].r;
                    merge_stats[i * wnd_num + j].Gvalue = left_stats->ramdata[i * wnd_num + wnd_num - 1].g + right_stats->ramdata[i * wnd_num + 0].g;
                    merge_stats[i * wnd_num + j].Bvalue = left_stats->ramdata[i * wnd_num + wnd_num - 1].b + right_stats->ramdata[i * wnd_num + 0].b;
                    merge_stats[i * wnd_num + j].WpNo = left_stats->ramdata[i * wnd_num + wnd_num - 1].wp + right_stats->ramdata[i * wnd_num + 0].wp;
                }
            }
        }
        break;
    default:
        break;
    }

}

void MergeAwbExcWpStats(
    rk_aiq_awb_stat_wp_res_v201_t*merge_stats,
    isp32_rawawb_meas_stat *left_stats,
    isp32_rawawb_meas_stat *right_stats,
    WinSplitMode mode
) {
    switch(mode) {
    case LEFT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201; i++) {
            merge_stats[i].RgainValue = left_stats->sum_exc[i].rgain_exc;
            merge_stats[i].BgainValue = left_stats->sum_exc[i].bgain_exc;
            merge_stats[i].WpNo = left_stats->sum_exc[i].wp_num_exc;
        }
        break;
    case RIGHT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201; i++) {
            merge_stats[i].RgainValue = right_stats->sum_exc[i].rgain_exc;
            merge_stats[i].BgainValue = right_stats->sum_exc[i].bgain_exc;
            merge_stats[i].WpNo = right_stats->sum_exc[i].wp_num_exc;
        }
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201; i++) {
            merge_stats[i].RgainValue = left_stats->sum_exc[i].rgain_exc + right_stats->sum_exc[i].rgain_exc;
            merge_stats[i].BgainValue = left_stats->sum_exc[i].bgain_exc + right_stats->sum_exc[i].bgain_exc;
            merge_stats[i].WpNo = left_stats->sum_exc[i].wp_num_exc + right_stats->sum_exc[i].wp_num_exc;
        }
        break;
    }
}

void MergeAecWinLiteStats(
    struct isp32_rawaelite_stat*    left_stats,
    struct isp32_rawaelite_stat*    right_stats,
    rawaelite_stat_t*               merge_stats,
    uint16_t*                       raw_mean,
    unsigned char*                  weight,
    int8_t                          stats_chn_sel,
    int8_t                          y_range_mode,
    WinSplitMode                    mode,
    struct isp2x_bls_fixed_val      bls1_val,
    struct isp2x_bls_fixed_val      awb1_gain
) {

    // NOTE: R/G/B/Y channel stats (10/12/10/8bits)
    uint32_t sum_xy = 0, sum_weight = 0;
    float rcc = 0, gcc = 0, bcc = 0, off = 0;

    if (y_range_mode <= CAM_YRANGEV2_MODE_FULL) {
        rcc = 0.299;
        gcc = 0.587;
        bcc = 0.114;
        off = 0;
    } else {
        rcc = 0.25;
        gcc = 0.5;
        bcc = 0.1094;
        off = 16;  //8bit
    }

    u8 wnd_num = sqrt(ISP32_RAWAELITE_MEAN_NUM);
    for(int i = 0; i < wnd_num; i++) {
        for(int j = 0; j < wnd_num; j++) {

            // step1 copy stats
            switch(mode) {
            case LEFT_MODE:
                merge_stats->channelr_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelr_xy;
                merge_stats->channelg_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelg_xy;
                merge_stats->channelb_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelb_xy;
                break;
            case RIGHT_MODE:
                merge_stats->channelr_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelr_xy;
                merge_stats->channelg_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelg_xy;
                merge_stats->channelb_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelb_xy;
                break;
            case LEFT_AND_RIGHT_MODE:

                if(j < wnd_num / 2) {
                    merge_stats->channelr_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelr_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelg_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelb_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelb_xy) / 2;
                } else if(j > wnd_num / 2) {
                    merge_stats->channelr_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelr_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelg_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelb_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelb_xy) / 2;
                } else {
                    merge_stats->channelr_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelr_xy + right_stats->data[i * wnd_num + 0].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelg_xy + right_stats->data[i * wnd_num + 0].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelb_xy + right_stats->data[i * wnd_num + 0].channelb_xy) / 2;
                }
                break;
            default:
                break;
            }

            // step2 subtract bls1
            merge_stats->channelr_xy[i * wnd_num + j] = CLIP((int)(merge_stats->channelr_xy[i * wnd_num + j] * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            merge_stats->channelg_xy[i * wnd_num + j] = CLIP((int)(merge_stats->channelg_xy[i * wnd_num + j] * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            merge_stats->channelb_xy[i * wnd_num + j] = CLIP((int)(merge_stats->channelb_xy[i * wnd_num + j] * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);

        }
    }

    switch (stats_chn_sel) {
    case RAWSTATS_CHN_Y_EN:
    case RAWSTATS_CHN_ALL_EN:
    default:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            merge_stats->channely_xy[i] = CLIP(round(rcc * (float)(merge_stats->channelr_xy[i] >> 2) +
                                               gcc * (float)(merge_stats->channelg_xy[i] >> 4) +
                                               bcc * (float)(merge_stats->channelb_xy[i] >> 2) + off), 0, MAX_8BITS);
            sum_xy += (merge_stats->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_R_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            sum_xy += ((merge_stats->channelr_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_G_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            sum_xy += ((merge_stats->channelg_xy[i] >> 4) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_B_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            sum_xy += ((merge_stats->channelb_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_RGB_EN:
        break;

    }

}

void MergeAecWinBigStats(
    struct isp32_rawaebig_stat0*    left_stats,
    struct isp32_rawaebig_stat0*    right_stats,
    rawaebig_stat_t*                merge_stats,
    uint16_t*                       raw_mean,
    unsigned char*                  weight,
    int8_t                          stats_chn_sel,
    int8_t                          y_range_mode,
    WinSplitMode                    mode,
    struct isp2x_bls_fixed_val      bls1_val,
    struct isp2x_bls_fixed_val      awb1_gain
) {

    // NOTE: R/G/B/Y channel stats (10/12/10/8bits)
    uint32_t sum_xy = 0, sum_weight = 0;
    float rcc = 0, gcc = 0, bcc = 0, off = 0;

    if (y_range_mode <= CAM_YRANGEV2_MODE_FULL) {
        rcc = 0.299;
        gcc = 0.587;
        bcc = 0.114;
        off = 0;
    } else {
        rcc = 0.25;
        gcc = 0.5;
        bcc = 0.1094;
        off = 16;  //8bit
    }

    u8 wnd_num = sqrt(ISP32_RAWAEBIG_MEAN_NUM);
    for(int i = 0; i < wnd_num; i++) {
        for(int j = 0; j < wnd_num; j++) {

            // step1 copy stats
            switch(mode) {
            case LEFT_MODE:
                merge_stats->channelr_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelr_xy;
                merge_stats->channelg_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelg_xy;
                merge_stats->channelb_xy[i * wnd_num + j] = left_stats->data[i * wnd_num + j].channelb_xy;
                break;
            case RIGHT_MODE:
                merge_stats->channelr_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelr_xy;
                merge_stats->channelg_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelg_xy;
                merge_stats->channelb_xy[i * wnd_num + j] = right_stats->data[i * wnd_num + j].channelb_xy;
                break;
            case LEFT_AND_RIGHT_MODE:

                if(j < wnd_num / 2) {
                    merge_stats->channelr_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelr_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelg_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + j * 2].channelb_xy + left_stats->data[i * wnd_num + j * 2 + 1].channelb_xy) / 2;
                } else if(j > wnd_num / 2) {
                    merge_stats->channelr_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelr_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelg_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (right_stats->data[i * wnd_num + j * 2 - wnd_num].channelb_xy + right_stats->data[i * wnd_num + j * 2 - wnd_num + 1].channelb_xy) / 2;
                } else {
                    merge_stats->channelr_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelr_xy + right_stats->data[i * wnd_num + 0].channelr_xy) / 2;
                    merge_stats->channelg_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelg_xy + right_stats->data[i * wnd_num + 0].channelg_xy) / 2;
                    merge_stats->channelb_xy[i * wnd_num + j] = (left_stats->data[i * wnd_num + wnd_num - 1].channelb_xy + right_stats->data[i * wnd_num + 0].channelb_xy) / 2;
                }
                break;
            default:
                break;
            }

            // step2 subtract bls1
            merge_stats->channelr_xy[i * wnd_num + j] = CLIP((int)(merge_stats->channelr_xy[i * wnd_num + j] * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            merge_stats->channelg_xy[i * wnd_num + j] = CLIP((int)(merge_stats->channelg_xy[i * wnd_num + j] * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            merge_stats->channelb_xy[i * wnd_num + j] = CLIP((int)(merge_stats->channelb_xy[i * wnd_num + j] * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);

        }
    }

    switch (stats_chn_sel) {
    case RAWSTATS_CHN_Y_EN:
    case RAWSTATS_CHN_ALL_EN:
    default:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            merge_stats->channely_xy[i] = CLIP(round(rcc * (float)(merge_stats->channelr_xy[i] >> 2) +
                                               gcc * (float)(merge_stats->channelg_xy[i] >> 4) +
                                               bcc * (float)(merge_stats->channelb_xy[i] >> 2) + off), 0, MAX_8BITS);
            sum_xy += (merge_stats->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_R_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            sum_xy += ((merge_stats->channelr_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_G_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            sum_xy += ((merge_stats->channelg_xy[i] >> 4) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_B_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            sum_xy += ((merge_stats->channelb_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_RGB_EN:
        break;
    }

}

void MergeAecSubWinStats(
    struct isp32_rawaebig_stat1*    left_stats,
    struct isp32_rawaebig_stat1*    right_stats,
    rawaebig_stat_t*                merge_stats,
    struct isp2x_bls_fixed_val      bls1_val,
    struct isp2x_bls_fixed_val      awb1_gain,
    u32*                            pixel_num
) {

    for(int i = 0; i < ISP3X_RAWAEBIG_SUBWIN_NUM; i++) {
        // step1 copy stats
        merge_stats->wndx_sumr[i] = (u64)left_stats->sumr[i] + (u64)right_stats->sumr[i];
        merge_stats->wndx_sumg[i] = (u64)left_stats->sumg[i] + (u64)right_stats->sumg[i];
        merge_stats->wndx_sumb[i] = (u64)left_stats->sumb[i] + (u64)right_stats->sumb[i];

        // step2 subtract bls1
        merge_stats->wndx_sumr[i] = CLIP((s64)(merge_stats->wndx_sumr[i] * awb1_gain.r / 256 - (pixel_num[i] >> 2) * bls1_val.r), 0, MAX_29BITS);
        merge_stats->wndx_sumg[i] = CLIP((s64)(merge_stats->wndx_sumg[i] * awb1_gain.gr / 256 - (pixel_num[i] >> 2) * bls1_val.gr), 0, MAX_32BITS);
        merge_stats->wndx_sumb[i] = CLIP((s64)(merge_stats->wndx_sumb[i] * awb1_gain.b / 256 - (pixel_num[i] >> 2) * bls1_val.b), 0, MAX_29BITS);
    }

}

void MergeAecHistBinStats(
    u32*                        left_stats,
    u32*                        right_stats,
    u32*                        merge_stats,
    WinSplitMode                mode,
    u8                          hist_mode,
    u16                         ob_offset_rb,
    u16                         ob_offset_g,
    struct isp2x_bls_fixed_val  bls1_val,
    struct isp2x_bls_fixed_val  awb1_gain,
    bool                        bls1_en
) {

    if (bls1_en) {

        memset(merge_stats, 0, ISP32_HIST_BIN_N_MAX * sizeof(u32));

        s16 bls1, bls_part;
        u16 ob_part, awb_part, div_part, round_part;
        int tmp;

        switch (hist_mode) {
        case 2:
            ob_part = ob_offset_rb >> 2;
            bls1 = bls1_val.r >> 2;
            awb_part = awb1_gain.r;
            bls_part = (ob_part + bls1) * awb_part;
            div_part = 256;
            round_part = 128;
            break;
        case 3:
            ob_part = ob_offset_g >> 4;
            bls1 = bls1_val.gr >> 4;
            awb_part = awb1_gain.gr;
            bls_part = (ob_part + bls1) * awb_part;
            div_part = 256;
            round_part = 128;
            break;
        case 4:
            ob_part = ob_offset_rb >> 2;
            bls1 = bls1_val.b >> 2;
            awb_part = awb1_gain.b;
            bls_part = (ob_part + bls1) * awb_part;
            div_part = 256;
            round_part = 128;
            break;
        case 5:
        default:
            ob_part = (u16)((ob_offset_g >> 4) * 587 + (ob_offset_rb >> 2) * 299 + (ob_offset_rb >> 2) * 114 + 500) / 1000;
            bls1 = (s16)((bls1_val.gr >> 4) * 587 + (bls1_val.r >> 2) * 299 + (bls1_val.b >> 2) * 114 + 500) / 1000;
            awb_part = 100;
            bls_part = (ob_part + bls1) * awb_part;
            div_part = 7655 / awb1_gain.r + 15027 / awb1_gain.gr + 2919 / awb1_gain.b;
            round_part = div_part / 2;
            break;
        }

        switch(mode) {
        case LEFT_MODE:
            for(int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
                tmp = (i - ob_part - bls1 > 0) ? (i * awb_part - bls_part + round_part) / div_part : 0;
                tmp = (tmp > ISP32_HIST_BIN_N_MAX - 1) ? (ISP32_HIST_BIN_N_MAX - 1) : tmp;
                merge_stats[tmp] += left_stats[i];
            }
            break;
        case RIGHT_MODE:
            for(int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
                tmp = (i - ob_part - bls1 > 0) ? (i * awb_part - bls_part + round_part) / div_part : 0;
                tmp = (tmp > ISP32_HIST_BIN_N_MAX - 1) ? (ISP32_HIST_BIN_N_MAX - 1) : tmp;
                merge_stats[tmp] += right_stats[i];
            }
            break;
        case LEFT_AND_RIGHT_MODE:
            for(int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
                tmp = (i - ob_part - bls1 > 0) ? (i * awb_part - bls_part + round_part) / div_part : 0;
                tmp = (tmp > ISP32_HIST_BIN_N_MAX - 1) ? (ISP32_HIST_BIN_N_MAX - 1) : tmp;
                merge_stats[tmp] += left_stats[i] + right_stats[i];
            }
            break;
        }

    } else {

        memset(merge_stats, 0, ISP32_HIST_BIN_N_MAX * sizeof(u32));
        switch(mode) {
        case LEFT_MODE:
            for(int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
                merge_stats[i] += left_stats[i];
            }
            break;
        case RIGHT_MODE:
            for(int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
                merge_stats[i] += right_stats[i];
            }
            break;
        case LEFT_AND_RIGHT_MODE:
            for(int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
                merge_stats[i] += left_stats[i] + right_stats[i];
            }
            break;
        }
    }

}

XCamReturn RkAiqResourceTranslatorV32::translateMultiAecStats(const SmartPtr<VideoBuffer>& from,
        SmartPtr<RkAiqAecStatsProxy>& to) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    struct isp32_isp_meas_cfg* isp_params = &_ispParams.meas;
    uint8_t AeSwapMode, AeSelMode, AfUseAeHW;
    AeSwapMode             = isp_params->rawae0.rawae_sel;
    AeSelMode              = isp_params->rawae3.rawae_sel;
    AfUseAeHW              = isp_params->rawaf.ae_mode;
    unsigned int meas_type = 0;

    WinSplitMode AeWinSplitMode[4] = {LEFT_AND_RIGHT_MODE}; //0:rawae0 1:rawae1 2:rawae3
    WinSplitMode HistWinSplitMode[4] = {LEFT_AND_RIGHT_MODE}; //0:rawhist0 1:rawhist1 2:rawhist3

    JudgeWinLocation32(&_ispParams.meas.rawae0.win, AeWinSplitMode[0], GetLeftIspRect(), GetRightIspRect());
    JudgeWinLocation32(&_ispParams.meas.rawae1.win, AeWinSplitMode[1], GetLeftIspRect(), GetRightIspRect());
    JudgeWinLocation32(&_ispParams.meas.rawae3.win, AeWinSplitMode[3], GetLeftIspRect(), GetRightIspRect());

    JudgeWinLocation32(&_ispParams.meas.rawhist0.win, HistWinSplitMode[0], GetLeftIspRect(), GetRightIspRect());
    JudgeWinLocation32(&_ispParams.meas.rawhist1.win, HistWinSplitMode[1], GetLeftIspRect(), GetRightIspRect());
    JudgeWinLocation32(&_ispParams.meas.rawhist3.win, HistWinSplitMode[3], GetLeftIspRect(), GetRightIspRect());

    // ae_stats = (ae_ori_stats_u12/10 - ob_offset_u9 - bls1_val_u12) * awb1_gain_u16 * range_ratio
    struct isp32_bls_cfg* bls_cfg = &_ispParams.bls_cfg;
    struct isp32_awb_gain_cfg* awb_gain_cfg = &_ispParams.awb_gain_cfg;
    u16 isp_ob_offset_rb, isp_ob_offset_g, isp_ob_predgain;
    struct isp2x_bls_fixed_val bls1_ori_val;
    struct isp2x_bls_fixed_val bls1_val;
    struct isp2x_bls_fixed_val awb1_gain;
    u8 rawhist_mode = 0, rawhist3_mode = 0;
    u8 index0 = 0, index1 = 0;
    bool is_hdr = (getWorkingMode() > 0) ? true : false;
    bool is_bls1_en = bls_cfg->bls1_en && !is_hdr;

    isp_ob_offset_rb = MAX(bls_cfg->isp_ob_offset >> 2, 0);
    isp_ob_offset_g  = bls_cfg->isp_ob_offset;
    isp_ob_predgain =  MAX(bls_cfg->isp_ob_predgain >> 8, 1);

    if (is_bls1_en) {
        bls1_ori_val.r  = (bls_cfg->bls1_val.r / isp_ob_predgain) >> 2;
        bls1_ori_val.gr = bls_cfg->bls1_val.gr / isp_ob_predgain;
        bls1_ori_val.gb = bls_cfg->bls1_val.gb / isp_ob_predgain;
        bls1_ori_val.b  = (bls_cfg->bls1_val.b / isp_ob_predgain) >> 2;
    } else {
        bls1_ori_val.r  = 0;
        bls1_ori_val.gr = 0;
        bls1_ori_val.gb = 0;
        bls1_ori_val.b  = 0;
    }

    //awb1_gain have adapted to the range of bls1_lvl
    awb1_gain.r = MAX(256, awb_gain_cfg->awb1_gain_r);
    awb1_gain.gr = MAX(256, awb_gain_cfg->awb1_gain_gr);
    awb1_gain.gb = MAX(256, awb_gain_cfg->awb1_gain_gb);
    awb1_gain.b = MAX(256, awb_gain_cfg->awb1_gain_b);

#ifdef AE_STATS_DEBUG
    LOGE("bls1[%d-%d-%d-%d]", bls1_ori_val.r, bls1_ori_val.gr, bls1_ori_val.gb, bls1_ori_val.b);
    LOGE("isp_ob_offset_rb, isp_ob_offset_g, isp_ob_predgain [%d-%d-%d]",
         isp_ob_offset_rb, isp_ob_offset_g, isp_ob_predgain);
    LOGE("awb1_gain[%d-%d-%d-%d]", awb1_gain.r, awb1_gain.gr, awb1_gain.gb, awb1_gain.b);

    _aeAlgoStatsCfg.UpdateStats = true;
    _aeAlgoStatsCfg.RawStatsChnSel = RAWSTATS_CHN_ALL_EN;
#endif

    // bls1_val = (bls1_ori_val + ob) * awb * range_ratio
    bls1_val.r = ((isp_ob_offset_rb + bls1_ori_val.r) * awb1_gain.r + 128) / 256;
    bls1_val.gr = ((isp_ob_offset_g + bls1_ori_val.gr) * awb1_gain.gr + 128) / 256;
    bls1_val.gb = ((isp_ob_offset_g + bls1_ori_val.gb) * awb1_gain.gb + 128) / 256;
    bls1_val.b = ((isp_ob_offset_rb + bls1_ori_val.b) * awb1_gain.b + 128) / 256;

    const SmartPtr<Isp20StatsBuffer> buf = from.dynamic_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_isp_stat_buffer* left_stats;
    struct rkisp32_isp_stat_buffer* right_stats;
    SmartPtr<RkAiqAecStats> statsInt = to->data();

    left_stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if (left_stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("camId: %d, stats: frame_id: %d,  meas_type; 0x%x", mCamPhyId, left_stats->frame_id,
                  left_stats->meas_type);

    right_stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr() + buf->get_v4l2_buf_length() / 2);
    if(right_stats == NULL) {
        LOGE("fail to get right stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    if(left_stats->frame_id != right_stats->frame_id || left_stats->meas_type != right_stats->meas_type) {
        LOGE_ANALYZER("status params(frmid or meas_type) of left isp and right isp are different");
        return XCAM_RETURN_ERROR_PARAM;
    }

    SmartPtr<RkAiqIrisParamsProxy> irisParams = buf->get_iris_params();

    switch (AeSwapMode) {
    case AEC_RAWSWAP_MODE_S_LITE:
        meas_type = ((left_stats->meas_type >> 7) & (0x01)) & ((left_stats->meas_type >> 11) & (0x01));
        index0       = 0;
        index1       = 1;
        rawhist_mode = isp_params->rawhist0.mode;
        break;
    case AEC_RAWSWAP_MODE_M_LITE:
        meas_type = ((left_stats->meas_type >> 8) & (0x01)) & ((left_stats->meas_type >> 12) & (0x01));
        index0       = 1;
        index1       = 0;
        rawhist_mode = isp_params->rawhist1.mode;
        break;
    default:
        LOGE("wrong AeSwapMode=%d\n", AeSwapMode);
        return XCAM_RETURN_ERROR_PARAM;
        break;
    }

    // ae stats v3.2
    statsInt->frame_id = left_stats->frame_id;
    statsInt->af_prior = (AfUseAeHW == 0) ? false : true;
    statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;
    if (!statsInt->aec_stats_valid) return XCAM_RETURN_BYPASS;

    //HIST 0/1
    if(is_hdr || AeSwapMode == AEC_RAWSWAP_MODE_S_LITE) {
        MergeAecHistBinStats(left_stats->params.rawhist0.hist_bin,
                             right_stats->params.rawhist0.hist_bin,
                             statsInt->aec_stats.ae_data.chn[index0].rawhist_lite.bins,
                             AeWinSplitMode[index0], rawhist_mode,
                             isp_ob_offset_rb, isp_ob_offset_g,
                             bls1_ori_val, awb1_gain, is_bls1_en);
    }

    if(is_hdr || AeSwapMode == AEC_RAWSWAP_MODE_M_LITE) {
        MergeAecHistBinStats(left_stats->params.rawhist1.hist_bin,
                             right_stats->params.rawhist1.hist_bin,
                             statsInt->aec_stats.ae_data.chn[index1].rawhist_big.bins,
                             AeWinSplitMode[index1], rawhist_mode,
                             isp_ob_offset_rb, isp_ob_offset_g,
                             bls1_ori_val, awb1_gain, is_bls1_en);
    }

    if (!AfUseAeHW && (AeSelMode <= AEC_RAWSEL_MODE_CHN_1)) {
        rawhist3_mode = isp_params->rawhist3.mode;
        MergeAecHistBinStats(left_stats->params.rawhist3.hist_bin,
                             right_stats->params.rawhist3.hist_bin,
                             statsInt->aec_stats.ae_data.chn[AeSelMode].rawhist_big.bins,
                             AeWinSplitMode[AeSelMode], rawhist3_mode,
                             isp_ob_offset_rb, isp_ob_offset_g,
                             bls1_ori_val, awb1_gain, is_bls1_en);
    }

    // calc ae run flag
    uint64_t SumHistPix[2] = { 0, 0 };
    float SumHistBin[2] = { 0.0f, 0.0f };
    u32* hist_bin[2];
    u32 pixel_num[ISP32_RAWAEBIG_SUBWIN_NUM] = { 0 };

    hist_bin[index0] = statsInt->aec_stats.ae_data.chn[index0].rawhist_lite.bins;
    hist_bin[index1] = statsInt->aec_stats.ae_data.chn[index1].rawhist_big.bins;
    /*if (!AfUseAeHW && (AeSelMode <= AEC_RAWSEL_MODE_CHN_1)) {
        hist_bin[AeSelMode] = statsInt->aec_stats.ae_data.chn[AeSelMode].rawhist_big.bins;
    }*/

    for (int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
        SumHistPix[index0] += hist_bin[index0][i];
        SumHistBin[index0] += (float)(hist_bin[index0][i] * (i + 1));

        SumHistPix[index1] += hist_bin[index1][i];
        SumHistBin[index1] += (float)(hist_bin[index1][i] * (i + 1));
    }
    statsInt->aec_stats.ae_data.hist_mean[0] = (uint8_t)(SumHistBin[0] / (float)MAX(SumHistPix[0], 1));
    statsInt->aec_stats.ae_data.hist_mean[1] = (uint8_t)(SumHistBin[1] / (float)MAX(SumHistPix[1], 1));

    if (_aeRunFlag || _aeAlgoStatsCfg.UpdateStats) {

        //AE-LITE (RAWAE0)
        if(is_hdr || AeSwapMode == AEC_RAWSWAP_MODE_S_LITE) {
            MergeAecWinLiteStats(&left_stats->params.rawae0,
                                 &right_stats->params.rawae0,
                                 &statsInt->aec_stats.ae_data.chn[index0].rawae_lite,
                                 &statsInt->aec_stats.ae_data.raw_mean[index0],
                                 _aeAlgoStatsCfg.LiteWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                 AeWinSplitMode[0], bls1_val, awb1_gain);
        }

        //AE-BIG (RAWAE1)
        if(is_hdr || AeSwapMode == AEC_RAWSWAP_MODE_M_LITE) {
            MergeAecWinBigStats(&left_stats->params.rawae1_0,
                                &right_stats->params.rawae1_0,
                                &statsInt->aec_stats.ae_data.chn[index1].rawae_big,
                                &statsInt->aec_stats.ae_data.raw_mean[index1],
                                _aeAlgoStatsCfg.BigWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                AeWinSplitMode[1], bls1_val, awb1_gain);

            pixel_num[0] = isp_params->rawae1.subwin[0].h_size * isp_params->rawae1.subwin[0].v_size;
            pixel_num[1] = isp_params->rawae1.subwin[1].h_size * isp_params->rawae1.subwin[1].v_size;
            pixel_num[2] = isp_params->rawae1.subwin[2].h_size * isp_params->rawae1.subwin[2].v_size;
            pixel_num[3] = isp_params->rawae1.subwin[3].h_size * isp_params->rawae1.subwin[3].v_size;
            MergeAecSubWinStats(&left_stats->params.rawae1_1,
                                &right_stats->params.rawae1_1,
                                &statsInt->aec_stats.ae_data.chn[index1].rawae_big,
                                bls1_val, awb1_gain, pixel_num);
        }

        //AE-BIG (RAWAE3)
        if (!AfUseAeHW) {
            switch (AeSelMode) {
            case AEC_RAWSEL_MODE_CHN_0:
            case AEC_RAWSEL_MODE_CHN_1:

                MergeAecWinBigStats(&left_stats->params.rawae3_0,
                                    &right_stats->params.rawae3_0,
                                    &statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big,
                                    &statsInt->aec_stats.ae_data.raw_mean[AeSelMode],
                                    _aeAlgoStatsCfg.BigWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                    AeWinSplitMode[AeSelMode], bls1_val, awb1_gain);

                pixel_num[0] = isp_params->rawae3.subwin[0].h_size * isp_params->rawae3.subwin[0].v_size;
                pixel_num[1] = isp_params->rawae3.subwin[1].h_size * isp_params->rawae3.subwin[1].v_size;
                pixel_num[2] = isp_params->rawae3.subwin[2].h_size * isp_params->rawae3.subwin[2].v_size;
                pixel_num[3] = isp_params->rawae3.subwin[3].h_size * isp_params->rawae3.subwin[3].v_size;
                MergeAecSubWinStats(&left_stats->params.rawae3_1,
                                    &right_stats->params.rawae3_1,
                                    &statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big,
                                    bls1_val, awb1_gain, pixel_num);
                break;

            case AEC_RAWSEL_MODE_TMO:

                bls1_val.r = 0;
                bls1_val.gr = 0;
                bls1_val.gb = 0;
                bls1_val.b = 0;

                awb1_gain.r = 256;
                awb1_gain.gr = 256;
                awb1_gain.gb = 256;
                awb1_gain.b = 256;

                MergeAecWinBigStats(&left_stats->params.rawae3_0,
                                    &right_stats->params.rawae3_0,
                                    &statsInt->aec_stats.ae_data.extra.rawae_big,
                                    &statsInt->aec_stats.ae_data.raw_mean[AeSelMode],
                                    _aeAlgoStatsCfg.BigWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                    AeWinSplitMode[AeSelMode], bls1_val, awb1_gain);

                pixel_num[0] = isp_params->rawae3.subwin[0].h_size * isp_params->rawae3.subwin[0].v_size;
                pixel_num[1] = isp_params->rawae3.subwin[1].h_size * isp_params->rawae3.subwin[1].v_size;
                pixel_num[2] = isp_params->rawae3.subwin[2].h_size * isp_params->rawae3.subwin[2].v_size;
                pixel_num[3] = isp_params->rawae3.subwin[3].h_size * isp_params->rawae3.subwin[3].v_size;
                MergeAecSubWinStats(&left_stats->params.rawae3_1,
                                    &right_stats->params.rawae3_1,
                                    &statsInt->aec_stats.ae_data.extra.rawae_big,
                                    bls1_val, awb1_gain, pixel_num);

                rawhist3_mode = isp_params->rawhist3.mode;
                MergeAecHistBinStats(left_stats->params.rawhist3.hist_bin,
                                     right_stats->params.rawhist3.hist_bin,
                                     statsInt->aec_stats.ae_data.extra.rawhist_big.bins,
                                     AeWinSplitMode[AeSelMode], rawhist3_mode,
                                     isp_ob_offset_rb, isp_ob_offset_g,
                                     bls1_ori_val, awb1_gain, false);
                break;

            default:
                LOGE("wrong AeSelMode=%d\n", AeSelMode);
                return XCAM_RETURN_ERROR_PARAM;
            }
        }
    }

#ifdef AE_STATS_DEBUG
    if (AeSwapMode != 0) {
        for (int i = 0; i < 15; i++) {
            for (int j = 0; j < 15; j++) {
                printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelr_xy[i * 15 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelg_xy[i * 15 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelb_xy[i * 15 + j]);
            }
        }

        printf("====================sub-win-result======================\n");

        for (int i = 0; i < 4; i++)
            printf("chn0_subwin[%d]:sumr 0x%08" PRIx64", sumg 0x%08" PRIx64", sumb 0x%08" PRIx64"\n", i,
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumr[i],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumg[i],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumb[i]);

        printf("====================hist_result========================\n");

        // for (int i = 0; i < 256; i++)
        //     printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins[i]);

    } else {
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelr_xy[i * 5 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelg_xy[i * 5 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelb_xy[i * 5 + j]);
            }
        }
        printf("====================hist_result========================\n");
        for (int i = 0; i < 256; i++)
            printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins[i]);
    }

#endif

    /*
     *         unsigned long chn0_mean = 0, chn1_mean = 0;
     *         for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {
     *             chn0_mean += stats->params.rawae1.data[i].channelg_xy;
     *             chn1_mean += stats->params.rawae3.data[i].channelg_xy;
     *         }
     *
     *
     *         printf("frame[%d]: chn[0-1]_g_mean_xy: %ld-%ld\n",
     *                 stats->frame_id, chn0_mean/ISP3X_RAWAEBIG_MEAN_NUM,
     *                 chn1_mean/ISP3X_RAWAEBIG_MEAN_NUM);
     */

    // expsoure params
    if (_expParams.ptr()) {
        statsInt->aec_stats.ae_exp = _expParams->data()->aecExpInfo;
        /*printf("frame[%d],gain=%d,time=%d\n", stats->frame_id,
               expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.analog_gain_code_global,
               expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.coarse_integration_time);*/

        /*
         * printf("%s: L: [0x%x-0x%x], M: [0x%x-0x%x], S: [0x%x-0x%x]\n",
         *        __func__,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.analog_gain_code_global);
         */
    }

    // iris params
    if (irisParams.ptr()) {
        float sof_time   = (float)irisParams->data()->sofTime / 1000000000.0f;
        float start_time = (float)irisParams->data()->PIris.StartTim.tv_sec +
                           (float)irisParams->data()->PIris.StartTim.tv_usec / 1000000.0f;
        float end_time = (float)irisParams->data()->PIris.EndTim.tv_sec +
                         (float)irisParams->data()->PIris.EndTim.tv_usec / 1000000.0f;
        float frm_intval = 1 / (statsInt->aec_stats.ae_exp.pixel_clock_freq_mhz * 1000000.0f /
                                (float)statsInt->aec_stats.ae_exp.line_length_pixels /
                                (float)statsInt->aec_stats.ae_exp.frame_length_lines);

        /*printf("%s: step=%d,last-step=%d,start-tim=%f,end-tim=%f,sof_tim=%f\n",
            __func__,
            statsInt->aec_stats.ae_exp.Iris.PIris.step,
            irisParams->data()->PIris.laststep,start_time,end_time,sof_time);
        */

        if (sof_time < end_time + frm_intval)
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.laststep;
        else
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.step;
    }

    to->set_sequence(left_stats->frame_id);

    return ret;
}

XCamReturn RkAiqResourceTranslatorV32::translateMultiAwbStats(const SmartPtr<VideoBuffer>& from,
        SmartPtr<RkAiqAwbStatsProxy>& to) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
#if defined(ISP_HW_V32) || defined(ISP_HW_V32_LITE)
    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();
#if defined(ISP_HW_V32)
    struct rkisp32_isp_stat_buffer *left_stats;
    struct rkisp32_isp_stat_buffer *right_stats;
    left_stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr());
#elif defined(ISP_HW_V32_LITE)
    struct rkisp32_isp_stat_buffer *left_stats;
    struct rkisp32_isp_stat_buffer *right_stats;
    left_stats = (struct rkisp32_lite_stat_buffer*)(buf->get_v4l2_userptr());
#endif

    SmartPtr<RkAiqAwbStats> statsInt = to->data();

    if(left_stats == NULL) {
        LOGE("fail to get left stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    right_stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr() + buf->get_v4l2_buf_length() / 2);
    if(right_stats == NULL) {
        LOGE("fail to get right stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    if(left_stats->frame_id != right_stats->frame_id || left_stats->meas_type != right_stats->meas_type) {
        LOGE_ANALYZER("status params(frmid or meas_type) of left isp and right isp are different");
        return XCAM_RETURN_ERROR_PARAM;
    }

    LOGI_ANALYZER("awb stats: camId:%d, frame_id: %d,  meas_type; 0x%x",
                  mCamPhyId, left_stats->frame_id, left_stats->meas_type);

    statsInt->awb_stats_valid = left_stats->meas_type >> 5 & 1;
    if (!statsInt->awb_stats_valid) {
        LOGE_ANALYZER("AWB stats invalid, ignore");
        return XCAM_RETURN_BYPASS;
    }
    memset(&statsInt->awb_stats_v32, 0, sizeof(statsInt->awb_stats_v32));

    if (left_stats->params.info2ddr.owner == RKISP_INFO2DRR_OWNER_AWB) {
        statsInt->awb_stats_v32.dbginfo_fd = left_stats->params.info2ddr.buf_fd;
    } else {
        statsInt->awb_stats_v32.dbginfo_fd = -1;
    }
    statsInt->awb_stats_v32.awb_cfg_effect_v32.blkMeasureMode = _ispParams.awb_cfg_v32.blkMeasureMode;
    statsInt->awb_stats_v32.awb_cfg_effect_v32.lightNum = _ispParams.awb_cfg_v32.lightNum;
    statsInt->awb_stats_v32.awb_cfg_effect_v32.groupIllIndxCurrent = _ispParams.awb_cfg_v32.groupIllIndxCurrent;
    memcpy(statsInt->awb_stats_v32.awb_cfg_effect_v32.IllIndxSetCurrent, _ispParams.awb_cfg_v32.IllIndxSetCurrent,
           sizeof(statsInt->awb_stats_v32.awb_cfg_effect_v32.IllIndxSetCurrent));
    memcpy(statsInt->awb_stats_v32.awb_cfg_effect_v32.timeSign, _ispParams.awb_cfg_v32.timeSign,
           sizeof(statsInt->awb_stats_v32.awb_cfg_effect_v32.timeSign));
    statsInt->awb_cfg_effect_valid = true;
    statsInt->frame_id = left_stats->frame_id;

    WinSplitMode AwbWinSplitMode = LEFT_AND_RIGHT_MODE;

    struct isp2x_window ori_win;
    ori_win.h_offs = _ispParams.meas.rawawb.h_offs;
    ori_win.h_size = _ispParams.meas.rawawb.h_size;
    ori_win.v_offs = _ispParams.meas.rawawb.v_offs;
    ori_win.v_size = _ispParams.meas.rawawb.v_size;

    JudgeWinLocation32(&ori_win, AwbWinSplitMode, GetLeftIspRect(), GetRightIspRect());
    MergeAwbWinStats(statsInt->awb_stats_v32.light, &left_stats->params.rawawb, &right_stats->params.rawawb,
                     statsInt->awb_stats_v32.awb_cfg_effect_v32.lightNum, AwbWinSplitMode);

    struct isp2x_window left_win;
    left_win.h_offs = _ispParams.isp_params_v32[0].meas.rawawb.h_offs;
    left_win.h_size = _ispParams.isp_params_v32[0].meas.rawawb.h_size;
    left_win.v_offs = _ispParams.isp_params_v32[0].meas.rawawb.v_offs;
    left_win.v_size = _ispParams.isp_params_v32[0].meas.rawawb.v_size;

    struct isp2x_window right_win;
    right_win.h_offs = _ispParams.isp_params_v32[1].meas.rawawb.h_offs;
    right_win.h_size = _ispParams.isp_params_v32[1].meas.rawawb.h_size;
    right_win.v_offs = _ispParams.isp_params_v32[1].meas.rawawb.v_offs;
    right_win.v_size = _ispParams.isp_params_v32[1].meas.rawawb.v_size;

    AwbStatOverflowCheckandFixed(&left_win,
                                 (rk_aiq_awb_blk_stat_mode_v201_t)_ispParams.isp_params_v32[0].meas.rawawb.blk_measure_mode,
                                 _ispParams.isp_params_v32[0].meas.rawawb.blk_with_luma_wei_en,
                                 (rk_aiq_awb_xy_type_v201_t)_ispParams.isp_params_v32[0].meas.rawawb.wp_hist_xytype,
                                 _ispParams.isp_params_v32[0].meas.rawawb.light_num, &left_stats->params.rawawb);
    AwbStatOverflowCheckandFixed(&right_win,
                                 (rk_aiq_awb_blk_stat_mode_v201_t)_ispParams.isp_params_v32[1].meas.rawawb.blk_measure_mode,
                                 _ispParams.isp_params_v32[1].meas.rawawb.blk_with_luma_wei_en,
                                 (rk_aiq_awb_xy_type_v201_t)_ispParams.isp_params_v32[1].meas.rawawb.wp_hist_xytype,
                                 _ispParams.isp_params_v32[1].meas.rawawb.light_num, &right_stats->params.rawawb);

    MergeAwbBlkStats(&ori_win, &left_win, &right_win, statsInt->awb_stats_v3x.blockResult, &left_stats->params.rawawb, &right_stats->params.rawawb, AwbWinSplitMode);

    MergeAwbHistBinStats(statsInt->awb_stats_v32.WpNoHist, left_stats->params.rawawb.yhist_bin, right_stats->params.rawawb.yhist_bin, AwbWinSplitMode);

    switch(AwbWinSplitMode) {
    case LEFT_MODE:
        for(int i = 0; i < statsInt->awb_stats_v32.awb_cfg_effect_v32.lightNum; i++)
            statsInt->awb_stats_v32.WpNo2[i] = left_stats->params.rawawb.sum[i].wp_num2;
        break;
    case RIGHT_MODE:
        for(int i = 0; i < statsInt->awb_stats_v32.awb_cfg_effect_v32.lightNum; i++)
            statsInt->awb_stats_v32.WpNo2[i] = right_stats->params.rawawb.sum[i].wp_num2;
        break;
    case LEFT_AND_RIGHT_MODE:
        for(int i = 0; i < statsInt->awb_stats_v32.awb_cfg_effect_v32.lightNum; i++)
            statsInt->awb_stats_v32.WpNo2[i] = left_stats->params.rawawb.sum[i].wp_num2 + right_stats->params.rawawb.sum[i].wp_num2;
        break;
    default:
        break;
    }

    MergeAwbExcWpStats(statsInt->awb_stats_v32.excWpRangeResult, &left_stats->params.rawawb, &right_stats->params.rawawb, AwbWinSplitMode);

    to->set_sequence(left_stats->frame_id);
#endif
    return ret;
}

XCamReturn
RkAiqResourceTranslatorV32::translateMultiAfStats (const SmartPtr<VideoBuffer> &from, SmartPtr<RkAiqAfStatsProxy> &to)
{
    typedef enum WinSplitMode_s {
        LEFT_AND_RIGHT_MODE = 0,
        LEFT_MODE,
        RIGHT_MODE,
        FULL_MODE,
    } SplitMode;

    struct AfSplitInfo {
        SplitMode wina_side_info;
        int32_t wina_l_blknum;
        int32_t wina_r_blknum;
        int32_t wina_r_skip_blknum;
        float wina_l_ratio;
        float wina_r_ratio;

        SplitMode winb_side_info;
        float winb_l_ratio;
        float winb_r_ratio;
    };

    struct isp32_rawaf_luma_data {
        u32 channelg_xy: 12;
        u32 highlit_cnt: 16;
        u32 dummy: 4;
    } __attribute__ ((packed));

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    const SmartPtr<Isp20StatsBuffer> buf =
        from.dynamic_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_isp_stat_buffer *left_stats, *right_stats;
    SmartPtr<RkAiqAfStats> statsInt = to->data();
    SmartPtr<RkAiqAfInfoProxy> afParams = buf->get_af_params();

    left_stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if (left_stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    right_stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr() + buf->get_v4l2_buf_length() / 2);
    if (right_stats == NULL) {
        LOGE("fail to get right stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    if (left_stats->frame_id != right_stats->frame_id || left_stats->meas_type != right_stats->meas_type) {
        LOGE_ANALYZER("status params(frmid or meas_type) of left isp and right isp are different");
    } else {
        LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x", left_stats->frame_id, left_stats->meas_type);
    }

    rkisp_effect_params_v20 ispParams;
    memset(&ispParams, 0, sizeof(ispParams));
    if (buf->getEffectiveIspParams(left_stats->frame_id, ispParams) < 0) {
        LOGE("fail to get ispParams ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }

    statsInt->frame_id = left_stats->frame_id;

    struct isp32_rawaf_meas_cfg &org_af = ispParams.meas.rawaf;
    int32_t l_isp_st, l_isp_ed, r_isp_st, r_isp_ed;
    int32_t l_win_st, l_win_ed, r_win_st, r_win_ed;
    int32_t x_st, x_ed, l_blknum, r_blknum, ov_w, blk_w, r_skip_blknum;
    struct AfSplitInfo af_split_info;
    int32_t i, j, k, dst_idx, l_idx, r_idx, l_lht, r_lht, lht0, lht1;

    struct isp32_bls_cfg* bls_cfg = &ispParams.bls_cfg;
    u8 from_awb = org_af.from_awb;
    u8 from_ynr = org_af.from_ynr;
    bool is_hdr = (getWorkingMode() > 0) ? true : false;
    int temp_luma, comp_bls = 0;
    u16 max_val = (1 << 12) - 1;

    if (bls_cfg->bls1_en && !is_hdr && !from_awb && !from_ynr) {
        comp_bls = (bls_cfg->bls1_val.gr + bls_cfg->bls1_val.gb) / 2  - bls_cfg->isp_ob_offset;
        comp_bls = MAX(comp_bls, 0);
    }

    memset(&af_split_info, 0, sizeof(af_split_info));
    ov_w = GetLeftIspRect().w + GetLeftIspRect().x - GetRightIspRect().x;
    x_st = org_af.win[0].h_offs;
    x_ed = x_st + org_af.win[0].h_size;
    l_isp_st = GetLeftIspRect().x;
    l_isp_ed = GetLeftIspRect().x + GetLeftIspRect().w;
    r_isp_st = GetRightIspRect().x;
    r_isp_ed = GetRightIspRect().x + GetRightIspRect().w;
    LOGD_AF("wina.x_st %d, wina.x_ed %d, l_isp_st %d, l_isp_ed %d, r_isp_st %d, r_isp_ed %d",
            x_st, x_ed, l_isp_st, l_isp_ed, r_isp_st, r_isp_ed);

    //// winA ////
    af_split_info.wina_l_ratio = 0;
    af_split_info.wina_r_ratio = 0;
    // af win in both side
    if ((x_st < r_isp_st) && (x_ed > l_isp_ed)) {
        af_split_info.wina_side_info = LEFT_AND_RIGHT_MODE;
        // af win < one isp width
        if (org_af.win[0].h_size < GetLeftIspRect().w) {
            blk_w = org_af.win[0].h_size / ISP2X_RAWAF_SUMDATA_ROW;
            l_blknum = (l_isp_ed - x_st + blk_w - 1) / blk_w;
            r_blknum = ISP2X_RAWAF_SUMDATA_ROW - l_blknum;
            l_win_ed = l_isp_ed - 2;
            l_win_st = l_win_ed - blk_w * ISP2X_RAWAF_SUMDATA_ROW;
            if (blk_w < ov_w) {
                r_skip_blknum = ov_w / blk_w;
                r_win_st = ov_w - r_skip_blknum * blk_w;
                r_win_ed = ov_w + (ISP2X_RAWAF_SUMDATA_ROW - r_skip_blknum) * blk_w;
                af_split_info.wina_r_skip_blknum = r_skip_blknum;
            }
            else {
                r_skip_blknum = 0;
                r_win_st = 2;
                r_win_ed = r_win_st + ISP2X_RAWAF_SUMDATA_ROW * blk_w;

                // blend last block of left isp and first block of right isp
                af_split_info.wina_r_skip_blknum = 0;
                af_split_info.wina_l_ratio = (float)ov_w / (float)blk_w;
                af_split_info.wina_r_ratio = 1 - af_split_info.wina_l_ratio;
            }
        }
        // af win <= one isp width * 1.5
        else if (org_af.win[0].h_size < GetLeftIspRect().w * 3 / 2) {
            l_win_st = x_st;
            l_win_ed = l_isp_ed - 2;
            blk_w = (l_win_ed - l_win_st) / (ISP2X_RAWAF_SUMDATA_ROW + 1);
            l_win_st = l_win_ed - blk_w * ISP2X_RAWAF_SUMDATA_ROW;
            l_blknum = ((l_win_ed - l_win_st) * ISP2X_RAWAF_SUMDATA_ROW + org_af.win[0].h_size - 1) / org_af.win[0].h_size;
            r_blknum = ISP2X_RAWAF_SUMDATA_ROW - l_blknum;
            if (blk_w < ov_w) {
                r_skip_blknum = ov_w / blk_w;
                r_win_st = ov_w - r_skip_blknum * blk_w;
                r_win_ed = ov_w + (ISP2X_RAWAF_SUMDATA_ROW - r_skip_blknum) * blk_w;
                af_split_info.wina_r_skip_blknum = r_skip_blknum;
            }
            else {
                r_skip_blknum = 0;
                r_win_st = 2;
                r_win_ed = r_win_st + ISP2X_RAWAF_SUMDATA_ROW * blk_w;
                // blend last block of left isp and first block of right isp
                af_split_info.wina_r_skip_blknum = 0;
                af_split_info.wina_l_ratio = (float)ov_w / (float)blk_w;
                af_split_info.wina_r_ratio = 1 - af_split_info.wina_l_ratio;
            }
        }
        else {
            l_win_st = x_st;
            l_win_ed = l_isp_ed - 2;
            blk_w = (l_win_ed - l_win_st) / ISP2X_RAWAF_SUMDATA_ROW;
            l_win_st = l_win_ed - blk_w * ISP2X_RAWAF_SUMDATA_ROW;
            r_win_st = 2;
            r_win_ed = r_win_st + blk_w * ISP2X_RAWAF_SUMDATA_ROW;
            af_split_info.wina_side_info = FULL_MODE;
            l_blknum = ISP2X_RAWAF_SUMDATA_ROW;
            r_blknum = ISP2X_RAWAF_SUMDATA_ROW;
        }
    }
    // af win in right side
    else if ((x_st >= r_isp_st) && (x_ed > l_isp_ed)) {
        af_split_info.wina_side_info = RIGHT_MODE;
        l_blknum = 0;
        r_blknum = ISP2X_RAWAF_SUMDATA_ROW;
        r_win_st = x_st - GetRightIspRect().x;
        r_win_ed = x_ed - GetRightIspRect().x;
        l_win_st = r_win_st;
        l_win_ed = r_win_ed;
    }
    // af win in left side
    else {
        af_split_info.wina_side_info = LEFT_MODE;
        l_blknum = ISP2X_RAWAF_SUMDATA_ROW;
        r_blknum = 0;
        l_win_st = x_st;
        l_win_ed = x_ed;
        r_win_st = l_win_st;
        r_win_ed = l_win_ed;
    }

    af_split_info.wina_l_blknum = l_blknum;
    af_split_info.wina_r_blknum = r_blknum;

    //// winB ////
    af_split_info.winb_l_ratio = 0;
    af_split_info.winb_r_ratio = 0;
    x_st = org_af.win[1].h_offs;
    x_ed = x_st + org_af.win[1].h_size;
    LOGD_AF("winb.x_st %d, winb.x_ed %d, l_isp_st %d, l_isp_ed %d, r_isp_st %d, r_isp_ed %d",
            x_st, x_ed, l_isp_st, l_isp_ed, r_isp_st, r_isp_ed);

    // af win in both side
    if ((x_st < r_isp_st) && (x_ed > l_isp_ed)) {
        af_split_info.winb_side_info = LEFT_AND_RIGHT_MODE;
        l_win_st = x_st;
        l_win_ed = l_isp_ed - 2;
        r_win_st = ov_w - 2;
        r_win_ed = x_ed - GetRightIspRect().x;
        // blend winB by width of left isp winB and right isp winB
        af_split_info.winb_l_ratio = (float)(l_win_ed - l_win_st) / (float)(x_ed - x_st);
        af_split_info.winb_r_ratio = 1 - af_split_info.winb_l_ratio;
    }
    // af win in right side
    else if ((x_st >= r_isp_st) && (x_ed > l_isp_ed)) {
        af_split_info.winb_side_info = RIGHT_MODE;
        af_split_info.winb_l_ratio = 0;
        af_split_info.winb_r_ratio = 1;
        r_win_st = x_st - GetRightIspRect().x;
        r_win_ed = x_ed - GetRightIspRect().x;
        l_win_st = r_win_st;
        l_win_ed = r_win_ed;
    }
    // af win in left side
    else {
        af_split_info.winb_side_info = LEFT_MODE;
        af_split_info.winb_l_ratio = 1;
        af_split_info.winb_r_ratio = 0;
        l_win_st = x_st;
        l_win_ed = x_ed;
        r_win_st = l_win_st;
        r_win_ed = l_win_ed;
    }

    //af
    {
        if (((left_stats->meas_type >> 6) & (0x01)) && ((left_stats->meas_type >> 6) & (0x01)))
            statsInt->af_stats_valid = true;
        else
            statsInt->af_stats_valid = false;

        statsInt->af_stats_v3x.comp_bls = comp_bls >> 2;

        if (af_split_info.winb_side_info == LEFT_AND_RIGHT_MODE) {
            statsInt->af_stats_v3x.wndb_luma = left_stats->params.rawaf.afm_lum_b * af_split_info.winb_l_ratio +
                                               right_stats->params.rawaf.afm_lum_b * af_split_info.winb_r_ratio;
            statsInt->af_stats_v3x.wndb_sharpness = left_stats->params.rawaf.afm_sum_b * af_split_info.winb_l_ratio +
                                                    right_stats->params.rawaf.afm_sum_b * af_split_info.winb_r_ratio;
            statsInt->af_stats_v3x.winb_highlit_cnt = left_stats->params.rawaf.highlit_cnt_winb +
                    right_stats->params.rawaf.highlit_cnt_winb;
        } else if (af_split_info.winb_side_info == LEFT_MODE) {
            statsInt->af_stats_v3x.wndb_luma = left_stats->params.rawaf.afm_lum_b;
            statsInt->af_stats_v3x.wndb_sharpness = left_stats->params.rawaf.afm_sum_b;
            statsInt->af_stats_v3x.winb_highlit_cnt = left_stats->params.rawaf.highlit_cnt_winb;
        } else {
            statsInt->af_stats_v3x.wndb_luma = right_stats->params.rawaf.afm_lum_b;
            statsInt->af_stats_v3x.wndb_sharpness = right_stats->params.rawaf.afm_sum_b;
            statsInt->af_stats_v3x.winb_highlit_cnt = right_stats->params.rawaf.highlit_cnt_winb;
        }

        struct isp32_rawaf_luma_data *luma0 = (struct isp32_rawaf_luma_data *)&left_stats->params.rawae3_0.data[0];
        struct isp32_rawaf_luma_data *luma1 = (struct isp32_rawaf_luma_data *)&right_stats->params.rawae3_0.data[0];
        if (af_split_info.wina_side_info == FULL_MODE) {
            for (i = 0; i < ISP2X_RAWAF_SUMDATA_ROW; i++) {
                for (j = 0; j < ISP2X_RAWAF_SUMDATA_COLUMN; j++) {
                    dst_idx = i * ISP2X_RAWAF_SUMDATA_ROW + j;
                    if (j == 0) {
                        l_idx = i * ISP2X_RAWAF_SUMDATA_ROW + j;
                        statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].v1;
                        statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].v2;
                        statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].h1;
                        statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].h2;

                        temp_luma = (luma0[l_idx].channelg_xy - comp_bls) * max_val / (max_val - comp_bls);
                        statsInt->af_stats_v3x.wnda_luma[dst_idx] = MAX(temp_luma, 0);
                        statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] = luma0[l_idx].highlit_cnt;
                    } else if (j >= 1 && j <= 7) {
                        l_idx = i * ISP2X_RAWAF_SUMDATA_ROW + 2 * (j - 1) + 1;
                        statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] =
                            left_stats->params.rawaf.ramdata[l_idx].v1 + left_stats->params.rawaf.ramdata[l_idx + 1].v1;
                        statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] =
                            left_stats->params.rawaf.ramdata[l_idx].v2 + left_stats->params.rawaf.ramdata[l_idx + 1].v2;
                        statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] =
                            left_stats->params.rawaf.ramdata[l_idx].h1 + left_stats->params.rawaf.ramdata[l_idx + 1].h1;
                        statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] =
                            left_stats->params.rawaf.ramdata[l_idx].h2 + left_stats->params.rawaf.ramdata[l_idx + 1].h2;

                        temp_luma = (((luma0[l_idx].channelg_xy + luma0[l_idx + 1].channelg_xy) >> 1) - comp_bls) * max_val / (max_val - comp_bls);
                        statsInt->af_stats_v3x.wnda_luma[dst_idx] = MAX(temp_luma, 0);
                        statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] = luma0[l_idx].highlit_cnt + luma0[l_idx + 1].highlit_cnt;
                    } else {
                        r_idx = i * ISP2X_RAWAF_SUMDATA_ROW + 2 * (j - 8) + 1;
                        statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] =
                            right_stats->params.rawaf.ramdata[r_idx].v1 + right_stats->params.rawaf.ramdata[r_idx + 1].v1;
                        statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] =
                            right_stats->params.rawaf.ramdata[r_idx].v2 + right_stats->params.rawaf.ramdata[r_idx + 1].v2;
                        statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] =
                            right_stats->params.rawaf.ramdata[r_idx].h1 + right_stats->params.rawaf.ramdata[r_idx + 1].h1;
                        statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] =
                            right_stats->params.rawaf.ramdata[r_idx].h2 + right_stats->params.rawaf.ramdata[r_idx + 1].h2;

                        temp_luma = (((luma1[r_idx].channelg_xy + luma1[r_idx + 1].channelg_xy) >> 1) - comp_bls) * max_val / (max_val - comp_bls);
                        statsInt->af_stats_v3x.wnda_luma[dst_idx] = MAX(temp_luma, 0);
                        statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] = luma1[r_idx].highlit_cnt + luma1[r_idx + 1].highlit_cnt;
                    }
                }
            }
        }
        else if (af_split_info.wina_side_info == LEFT_AND_RIGHT_MODE) {
            for (i = 0; i < ISP2X_RAWAF_SUMDATA_ROW; i++) {
                j = ISP2X_RAWAF_SUMDATA_ROW - af_split_info.wina_l_blknum;
                for (k = 0; k < af_split_info.wina_l_blknum; j++, k++) {
                    dst_idx = i * ISP2X_RAWAF_SUMDATA_ROW + k;
                    l_idx = i * ISP2X_RAWAF_SUMDATA_ROW + j;
                    statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].v1;
                    statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].v2;
                    statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].h1;
                    statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] = left_stats->params.rawaf.ramdata[l_idx].h2;
                    temp_luma = (luma0[l_idx].channelg_xy - comp_bls) * max_val / (max_val - comp_bls);
                    statsInt->af_stats_v3x.wnda_luma[dst_idx] = MAX(temp_luma, 0);
                    statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] = luma0[l_idx].highlit_cnt;
                }
            }

            for (i = 0; i < ISP2X_RAWAF_SUMDATA_ROW; i++) {
                j = af_split_info.wina_r_skip_blknum;
                for (k = 0; k < af_split_info.wina_r_blknum; j++, k++) {
                    dst_idx = i * ISP2X_RAWAF_SUMDATA_ROW + k + af_split_info.wina_l_blknum;
                    r_idx = i * ISP2X_RAWAF_SUMDATA_ROW + j;
                    statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] = right_stats->params.rawaf.ramdata[r_idx].v1;
                    statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] = right_stats->params.rawaf.ramdata[r_idx].v2;
                    statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] = right_stats->params.rawaf.ramdata[r_idx].h1;
                    statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] = right_stats->params.rawaf.ramdata[r_idx].h2;

                    temp_luma = (luma1[r_idx].channelg_xy - comp_bls) * max_val / (max_val - comp_bls);
                    statsInt->af_stats_v3x.wnda_luma[dst_idx] = MAX(temp_luma, 0);
                    statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] = luma1[r_idx].highlit_cnt;
                }
            }

            if (af_split_info.wina_r_skip_blknum == 0) {
                for (j = 0; j < ISP2X_RAWAF_SUMDATA_COLUMN; j++) {
                    dst_idx = j * ISP2X_RAWAF_SUMDATA_ROW + (af_split_info.wina_l_blknum - 1);
                    l_idx = j * ISP2X_RAWAF_SUMDATA_ROW + (ISP2X_RAWAF_SUMDATA_COLUMN - 1);
                    r_idx = j * ISP2X_RAWAF_SUMDATA_ROW;
                    statsInt->af_stats_v3x.wnda_fv_v1[dst_idx] =
                        left_stats->params.rawaf.ramdata[l_idx].v1 * af_split_info.wina_l_ratio +
                        right_stats->params.rawaf.ramdata[r_idx].v1 * af_split_info.wina_r_ratio;
                    statsInt->af_stats_v3x.wnda_fv_v2[dst_idx] =
                        left_stats->params.rawaf.ramdata[l_idx].v2 * af_split_info.wina_l_ratio +
                        right_stats->params.rawaf.ramdata[r_idx].v2 * af_split_info.wina_r_ratio;
                    statsInt->af_stats_v3x.wnda_fv_h1[dst_idx] =
                        left_stats->params.rawaf.ramdata[l_idx].h1 * af_split_info.wina_l_ratio +
                        right_stats->params.rawaf.ramdata[r_idx].h1 * af_split_info.wina_r_ratio;
                    statsInt->af_stats_v3x.wnda_fv_h2[dst_idx] =
                        left_stats->params.rawaf.ramdata[l_idx].h2 * af_split_info.wina_l_ratio +
                        right_stats->params.rawaf.ramdata[r_idx].h2 * af_split_info.wina_r_ratio;

                    temp_luma = ((luma0[l_idx].channelg_xy * af_split_info.wina_l_ratio + luma1[r_idx].channelg_xy * af_split_info.wina_r_ratio) - comp_bls) * max_val / (max_val - comp_bls);
                    statsInt->af_stats_v3x.wnda_luma[dst_idx] = MAX(temp_luma, 0);
                    statsInt->af_stats_v3x.wina_highlit_cnt[dst_idx] = luma0[l_idx].highlit_cnt + luma1[r_idx].highlit_cnt;
                }
            }
        } else if (af_split_info.wina_side_info == LEFT_MODE) {
            for (i = 0; i < RKAIQ_RAWAF_SUMDATA_NUM; i++) {
                statsInt->af_stats_v3x.wnda_fv_v1[i] = left_stats->params.rawaf.ramdata[i].v1;
                statsInt->af_stats_v3x.wnda_fv_v2[i] = left_stats->params.rawaf.ramdata[i].v2;
                statsInt->af_stats_v3x.wnda_fv_h1[i] = left_stats->params.rawaf.ramdata[i].h1;
                statsInt->af_stats_v3x.wnda_fv_h2[i] = left_stats->params.rawaf.ramdata[i].h2;

                temp_luma = (luma0[i].channelg_xy - comp_bls) * max_val / (max_val - comp_bls);
                statsInt->af_stats_v3x.wnda_luma[i] = MAX(temp_luma, 0);
                statsInt->af_stats_v3x.wina_highlit_cnt[i] = luma0[i].highlit_cnt;
            }
        } else {
            for (i = 0; i < RKAIQ_RAWAF_SUMDATA_NUM; i++) {
                statsInt->af_stats_v3x.wnda_fv_v1[i] = right_stats->params.rawaf.ramdata[i].v1;
                statsInt->af_stats_v3x.wnda_fv_v2[i] = right_stats->params.rawaf.ramdata[i].v2;
                statsInt->af_stats_v3x.wnda_fv_h1[i] = right_stats->params.rawaf.ramdata[i].h1;
                statsInt->af_stats_v3x.wnda_fv_h2[i] = right_stats->params.rawaf.ramdata[i].h2;

                temp_luma = (luma1[i].channelg_xy - comp_bls) * max_val / (max_val - comp_bls);
                statsInt->af_stats_v3x.wnda_luma[i] = MAX(temp_luma, 0);
                statsInt->af_stats_v3x.wina_highlit_cnt[i] = luma1[i].highlit_cnt;
            }
        }

        LOGD_AF("af_split_info.wina: %d, %d, %d, %d, %f, %f",
                af_split_info.wina_side_info, af_split_info.wina_l_blknum, af_split_info.wina_r_blknum,
                af_split_info.wina_r_skip_blknum, af_split_info.wina_l_ratio, af_split_info.wina_r_ratio);
        LOGD_AF("af_split_info.winb: %d, %f, %f",
                af_split_info.winb_side_info, af_split_info.winb_l_ratio, af_split_info.winb_r_ratio);

        if(afParams.ptr()) {
            statsInt->af_stats_v3x.focusCode = afParams->data()->focusCode;
            statsInt->af_stats_v3x.zoomCode = afParams->data()->zoomCode;
            statsInt->af_stats_v3x.focus_endtim = afParams->data()->focusEndTim;
            statsInt->af_stats_v3x.focus_starttim = afParams->data()->focusStartTim;
            statsInt->af_stats_v3x.zoom_endtim = afParams->data()->zoomEndTim;
            statsInt->af_stats_v3x.zoom_starttim = afParams->data()->zoomStartTim;
            statsInt->af_stats_v3x.sof_tim = afParams->data()->sofTime;
            statsInt->af_stats_v3x.focusCorrection = afParams->data()->focusCorrection;
            statsInt->af_stats_v3x.zoomCorrection = afParams->data()->zoomCorrection;
            statsInt->af_stats_v3x.angleZ = afParams->data()->angleZ;
        }
    }

    return ret;
}

#if RKAIQ_HAVE_DEHAZE_V12
XCamReturn RkAiqResourceTranslatorV32::translateMultiAdehazeStats(const SmartPtr<VideoBuffer>& from,
        SmartPtr<RkAiqAdehazeStatsProxy>& to) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

#if defined(ISP_HW_V32)
    const SmartPtr<Isp20StatsBuffer> buf = from.dynamic_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_isp_stat_buffer* left_stats;
    struct rkisp32_isp_stat_buffer* right_stats;
    SmartPtr<RkAiqAdehazeStats> statsInt = to->data();

    left_stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if (left_stats == NULL) {
        LOGE("fail to get left stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOG1_ADEHAZE(
        "%s left adehaze_stats_valid:%d dhaz_adp_air_base:%d dhaz_adp_wt:%d dhaz_adp_gratio:%d "
        "dhaz_adp_tmax:%d dhaz_pic_sumh:%d\n",
        __func__, left_stats->meas_type >> 17 & 1, left_stats->params.dhaz.dhaz_adp_air_base,
        left_stats->params.dhaz.dhaz_adp_wt, left_stats->params.dhaz.dhaz_adp_gratio,
        left_stats->params.dhaz.dhaz_adp_tmax, left_stats->params.dhaz.dhaz_pic_sumh);
    LOG1_ADEHAZE(
        "%s left h_rgb_iir[0~23]: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
        "%d %d %d\n",
        __func__, left_stats->params.dhaz.h_rgb_iir[0], left_stats->params.dhaz.h_rgb_iir[1],
        left_stats->params.dhaz.h_rgb_iir[2], left_stats->params.dhaz.h_rgb_iir[3],
        left_stats->params.dhaz.h_rgb_iir[4], left_stats->params.dhaz.h_rgb_iir[5],
        left_stats->params.dhaz.h_rgb_iir[6], left_stats->params.dhaz.h_rgb_iir[7],
        left_stats->params.dhaz.h_rgb_iir[8], left_stats->params.dhaz.h_rgb_iir[9],
        left_stats->params.dhaz.h_rgb_iir[10], left_stats->params.dhaz.h_rgb_iir[11],
        left_stats->params.dhaz.h_rgb_iir[12], left_stats->params.dhaz.h_rgb_iir[13],
        left_stats->params.dhaz.h_rgb_iir[14], left_stats->params.dhaz.h_rgb_iir[15],
        left_stats->params.dhaz.h_rgb_iir[16], left_stats->params.dhaz.h_rgb_iir[17],
        left_stats->params.dhaz.h_rgb_iir[18], left_stats->params.dhaz.h_rgb_iir[19],
        left_stats->params.dhaz.h_rgb_iir[20], left_stats->params.dhaz.h_rgb_iir[21],
        left_stats->params.dhaz.h_rgb_iir[22], left_stats->params.dhaz.h_rgb_iir[23]);

    right_stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr() + buf->get_v4l2_buf_length() / 2);
    if (right_stats == NULL) {
        LOGE("fail to get right stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOG1_ADEHAZE(
        "%s right adehaze_stats_valid:%d dhaz_adp_air_base:%d dhaz_adp_wt:%d dhaz_adp_gratio:%d "
        "dhaz_adp_tmax:%d dhaz_pic_sumh:%d\n",
        __func__, right_stats->meas_type >> 17 & 1, right_stats->params.dhaz.dhaz_adp_air_base,
        right_stats->params.dhaz.dhaz_adp_wt, right_stats->params.dhaz.dhaz_adp_gratio,
        right_stats->params.dhaz.dhaz_adp_tmax, right_stats->params.dhaz.dhaz_pic_sumh);
    LOG1_ADEHAZE(
        "%s right h_rgb_iir[0~23]: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
        "%d %d %d\n",
        __func__, right_stats->params.dhaz.h_rgb_iir[0], right_stats->params.dhaz.h_rgb_iir[1],
        right_stats->params.dhaz.h_rgb_iir[2], right_stats->params.dhaz.h_rgb_iir[3],
        right_stats->params.dhaz.h_rgb_iir[4], right_stats->params.dhaz.h_rgb_iir[5],
        right_stats->params.dhaz.h_rgb_iir[6], right_stats->params.dhaz.h_rgb_iir[7],
        right_stats->params.dhaz.h_rgb_iir[8], right_stats->params.dhaz.h_rgb_iir[9],
        right_stats->params.dhaz.h_rgb_iir[10], right_stats->params.dhaz.h_rgb_iir[11],
        right_stats->params.dhaz.h_rgb_iir[12], right_stats->params.dhaz.h_rgb_iir[13],
        right_stats->params.dhaz.h_rgb_iir[14], right_stats->params.dhaz.h_rgb_iir[15],
        right_stats->params.dhaz.h_rgb_iir[16], right_stats->params.dhaz.h_rgb_iir[17],
        right_stats->params.dhaz.h_rgb_iir[18], right_stats->params.dhaz.h_rgb_iir[19],
        right_stats->params.dhaz.h_rgb_iir[20], right_stats->params.dhaz.h_rgb_iir[21],
        right_stats->params.dhaz.h_rgb_iir[22], right_stats->params.dhaz.h_rgb_iir[23]);

    if(left_stats->frame_id != right_stats->frame_id || left_stats->meas_type != right_stats->meas_type) {
        LOGE_ANALYZER("status params(frmid or meas_type) of left isp and right isp are different");
        return XCAM_RETURN_ERROR_PARAM;
    } else {
        LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x", left_stats->frame_id, left_stats->meas_type);
    }

    // dehaze
    statsInt->adehaze_stats_valid = left_stats->meas_type >> 17 & 1;
    if (!statsInt->adehaze_stats_valid) return XCAM_RETURN_BYPASS;

    statsInt->frame_id = left_stats->frame_id;

    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_air_base =
        (left_stats->params.dhaz.dhaz_adp_air_base + right_stats->params.dhaz.dhaz_adp_air_base) / 2;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_wt     = (left_stats->params.dhaz.dhaz_adp_wt + right_stats->params.dhaz.dhaz_adp_wt) / 2;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_gratio = (left_stats->params.dhaz.dhaz_adp_gratio + right_stats->params.dhaz.dhaz_adp_gratio) / 2;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_tmax =
        (left_stats->params.dhaz.dhaz_adp_tmax + right_stats->params.dhaz.dhaz_adp_tmax) / 2;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_pic_sumh   = (left_stats->params.dhaz.dhaz_pic_sumh + right_stats->params.dhaz.dhaz_pic_sumh) / 2;
    unsigned int ro_pic_sumh_left = left_stats->params.dhaz.dhaz_pic_sumh;
    if (!ro_pic_sumh_left) {
        ro_pic_sumh_left = ISP32_DHAZ_PIC_SUM_MIN;
        LOGE_ADEHAZE("%s(%d) left ro_pic_sumh is zero, set to %d !!!\n", __func__, __LINE__,
                     ISP32_DHAZ_PIC_SUM_MIN);
    }
    unsigned int ro_pic_sumh_right = right_stats->params.dhaz.dhaz_pic_sumh;
    if (!ro_pic_sumh_right) {
        ro_pic_sumh_right = ISP32_DHAZ_PIC_SUM_MIN;
        LOGE_ADEHAZE("%s(%d) right ro_pic_sumh is zero, set to %d !!!\n", __func__, __LINE__,
                     ISP32_DHAZ_PIC_SUM_MIN);
    }
    unsigned int tmp = 0;
    for (int i = 0; i < ISP32_DHAZ_HIST_IIR_NUM; i++) {
        tmp = (left_stats->params.dhaz.h_rgb_iir[i] * ro_pic_sumh_left + right_stats->params.dhaz.h_rgb_iir[i] * ro_pic_sumh_right)
              / (ro_pic_sumh_left + ro_pic_sumh_right);
        statsInt->adehaze_stats.dehaze_stats_v12.h_rgb_iir[i] =
            tmp > ISP32_DHAZ_HIST_IIR_MAX ? ISP32_DHAZ_HIST_IIR_MAX : tmp;
    }
#endif

    return ret;
}
#endif

#endif

#if defined(ISP_HW_V32)
void calcAecLiteWinStatsV32(
    isp32_rawaelite_stat*       stats_in,
    rawaelite_stat_t*           stats_out,
    uint16_t*                   raw_mean,
    unsigned char*              weight,
    int8_t                      stats_chn_sel,
    int8_t                      y_range_mode,
    struct isp2x_bls_fixed_val  bls1_val,
    struct isp2x_bls_fixed_val  awb1_gain
) {

    // NOTE: R/G/B/Y channel stats (10/12/10/8bits)
    uint32_t sum_xy = 0, sum_weight = 0;
    float rcc = 0, gcc = 0, bcc = 0, off = 0;

    if (y_range_mode <= CAM_YRANGEV2_MODE_FULL) {
        rcc = 0.299;
        gcc = 0.587;
        bcc = 0.114;
        off = 0;
    } else {
        rcc = 0.25;
        gcc = 0.5;
        bcc = 0.1094;
        off = 16;  //8bit
    }

    switch (stats_chn_sel) {
    case RAWSTATS_CHN_Y_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channely_xy[i] = CLIP(round(rcc * (float)((stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r) >> 2) +
                                                   gcc * (float)((stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr) >> 4) +
                                                   bcc * (float)((stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b) >> 2) + off), 0, MAX_8BITS);
            sum_xy += (stats_out->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_R_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            sum_xy += ((stats_out->channelr_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_G_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            sum_xy += ((stats_out->channelg_xy[i] >> 4) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_B_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
            sum_xy += ((stats_out->channelb_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_RGB_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
        }
        break;

    case RAWSTATS_CHN_ALL_EN:
    default:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
            stats_out->channely_xy[i] = CLIP(round(rcc * (float)(stats_out->channelr_xy[i] >> 2) +
                                                   gcc * (float)(stats_out->channelg_xy[i] >> 4) +
                                                   bcc * (float)(stats_out->channelb_xy[i] >> 2) + off), 0, MAX_8BITS);
            sum_xy += (stats_out->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;
    }

}

void calcAecBigWinStatsV32(
    isp32_rawaebig_stat0*       stats_in0,
    isp32_rawaebig_stat1*       stats_in1,
    rawaebig_stat_t*            stats_out,
    uint16_t*                   raw_mean,
    unsigned char*              weight,
    int8_t                      stats_chn_sel,
    int8_t                      y_range_mode,
    struct isp2x_bls_fixed_val  bls1_val,
    struct isp2x_bls_fixed_val  awb1_gain,
    u32*                        pixel_num
) {

    // NOTE: R/G/B/Y channel stats (10/12/10/8bits)
    uint32_t sum_xy = 0, sum_weight = 0;
    float rcc = 0, gcc = 0, bcc = 0, off = 0;

    if (y_range_mode <= CAM_YRANGEV2_MODE_FULL) {
        rcc = 0.299;
        gcc = 0.587;
        bcc = 0.114;
        off = 0;
    } else {
        rcc = 0.25;
        gcc = 0.5;
        bcc = 0.1094;
        off = 16;  //8bit
    }

    switch (stats_chn_sel) {
    case RAWSTATS_CHN_Y_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channely_xy[i] = CLIP(round(rcc * (float)((stats_in0->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r) >> 2) +
                                                   gcc * (float)((stats_in0->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr) >> 4) +
                                                   bcc * (float)((stats_in0->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b) >> 2) + off), 0, MAX_8BITS);
            sum_xy += (stats_out->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_R_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in0->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            sum_xy += ((stats_out->channelr_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_G_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelg_xy[i] = CLIP((int)(stats_in0->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            sum_xy += ((stats_out->channelg_xy[i] >> 4) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_B_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelb_xy[i] = CLIP((int)(stats_in0->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
            sum_xy += ((stats_out->channelb_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_RGB_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in0->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            stats_out->channelg_xy[i] = CLIP((int)(stats_in0->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            stats_out->channelb_xy[i] = CLIP((int)(stats_in0->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
        }
        break;

    case RAWSTATS_CHN_ALL_EN:
    default:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in0->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            stats_out->channelg_xy[i] = CLIP((int)(stats_in0->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            stats_out->channelb_xy[i] = CLIP((int)(stats_in0->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
            stats_out->channely_xy[i] = CLIP(round(rcc * (float)(stats_out->channelr_xy[i] >> 2) +
                                                   gcc * (float)(stats_out->channelg_xy[i] >> 4) +
                                                   bcc * (float)(stats_out->channelb_xy[i] >> 2) + off), 0, MAX_8BITS);
            sum_xy += (stats_out->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;
    }

    for (int i = 0; i < ISP32_RAWAEBIG_SUBWIN_NUM; i++) {
        stats_out->wndx_sumr[i] = CLIP((s64)((u64)stats_in1->sumr[i] * awb1_gain.r / 256 - (pixel_num[i] >> 2) * bls1_val.r), 0, MAX_29BITS);
        stats_out->wndx_sumg[i] = CLIP((s64)((u64)stats_in1->sumg[i] * awb1_gain.gr / 256 - (pixel_num[i] >> 1) * bls1_val.gb), 0, MAX_32BITS);
        stats_out->wndx_sumb[i] = CLIP((s64)((u64)stats_in1->sumb[i] * awb1_gain.b / 256 - (pixel_num[i] >> 2) * bls1_val.b), 0, MAX_29BITS);
    }
}

#endif

#if defined(ISP_HW_V32_LITE)
void calcAecLiteWinStatsV32Lite(
    isp2x_rawaelite_stat*       stats_in,
    rawaelite_stat_t*           stats_out,
    uint16_t*                   raw_mean,
    unsigned char*              weight,
    int8_t                      stats_chn_sel,
    int8_t                      y_range_mode,
    struct isp2x_bls_fixed_val  bls1_val,
    struct isp2x_bls_fixed_val  awb1_gain
) {

    // NOTE: R/G/B/Y channel stats (10/12/10/8bits)
    uint32_t sum_xy = 0, sum_weight = 0;
    float rcc = 0, gcc = 0, bcc = 0, off = 0;

    if (y_range_mode <= CAM_YRANGEV2_MODE_FULL) {
        rcc = 0.299;
        gcc = 0.587;
        bcc = 0.114;
        off = 0;
    } else {
        rcc = 0.25;
        gcc = 0.5;
        bcc = 0.1094;
        off = 16;  //8bit
    }

    switch (stats_chn_sel) {
    case RAWSTATS_CHN_Y_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channely_xy[i] = CLIP(round(rcc * (float)((stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r) >> 2) +
                                                   gcc * (float)((stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr) >> 4) +
                                                   bcc * (float)((stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b) >> 2) + off), 0, MAX_8BITS);
            sum_xy += (stats_out->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_R_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            sum_xy += ((stats_out->channelr_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_G_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            sum_xy += ((stats_out->channelg_xy[i] >> 4) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_B_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
            sum_xy += ((stats_out->channelb_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_RGB_EN:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
        }
        break;

    case RAWSTATS_CHN_ALL_EN:
    default:
        for (int i = 0; i < ISP32_RAWAELITE_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
            stats_out->channely_xy[i] = CLIP(round(rcc * (float)(stats_out->channelr_xy[i] >> 2) +
                                                   gcc * (float)(stats_out->channelg_xy[i] >> 4) +
                                                   bcc * (float)(stats_out->channelb_xy[i] >> 2) + off), 0, MAX_8BITS);
            sum_xy += (stats_out->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;
    }

}

void calcAecBigWinStatsV32Lite(
    isp32_lite_rawaebig_stat*   stats_in,
    rawaebig_stat_t*            stats_out,
    uint16_t*                   raw_mean,
    unsigned char*              weight,
    int8_t                      stats_chn_sel,
    int8_t                      y_range_mode,
    struct isp2x_bls_fixed_val  bls1_val,
    struct isp2x_bls_fixed_val  awb1_gain,
    u32                         pixel_num
) {

    // NOTE: R/G/B/Y channel stats (10/12/10/8bits)
    uint32_t sum_xy = 0, sum_weight = 0;
    float rcc = 0, gcc = 0, bcc = 0, off = 0;

    if (y_range_mode <= CAM_YRANGEV2_MODE_FULL) {
        rcc = 0.299;
        gcc = 0.587;
        bcc = 0.114;
        off = 0;
    } else {
        rcc = 0.25;
        gcc = 0.5;
        bcc = 0.1094;
        off = 16;  //8bit
    }

    switch (stats_chn_sel) {
    case RAWSTATS_CHN_Y_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channely_xy[i] = CLIP(round(rcc * (float)((stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r) >> 2) +
                                                   gcc * (float)((stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr) >> 4) +
                                                   bcc * (float)((stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b) >> 2) + off), 0, MAX_8BITS);
            sum_xy += (stats_out->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_R_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            sum_xy += ((stats_out->channelr_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_G_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            sum_xy += ((stats_out->channelg_xy[i] >> 4) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_B_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
            sum_xy += ((stats_out->channelb_xy[i] >> 2) * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;

    case RAWSTATS_CHN_RGB_EN:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
        }
        break;

    case RAWSTATS_CHN_ALL_EN:
    default:
        for (int i = 0; i < ISP32_RAWAEBIG_MEAN_NUM; i++) {
            stats_out->channelr_xy[i] = CLIP((int)(stats_in->data[i].channelr_xy * awb1_gain.r / 256 - bls1_val.r), 0, MAX_10BITS);
            stats_out->channelg_xy[i] = CLIP((int)(stats_in->data[i].channelg_xy * awb1_gain.gr / 256 - bls1_val.gr), 0, MAX_12BITS);
            stats_out->channelb_xy[i] = CLIP((int)(stats_in->data[i].channelb_xy * awb1_gain.b / 256 - bls1_val.b), 0, MAX_10BITS);
            stats_out->channely_xy[i] = CLIP(round(rcc * (float)(stats_out->channelr_xy[i] >> 2) +
                                                   gcc * (float)(stats_out->channelg_xy[i] >> 4) +
                                                   bcc * (float)(stats_out->channelb_xy[i] >> 2) + off), 0, MAX_8BITS);
            sum_xy += (stats_out->channely_xy[i] * weight[i]);
            sum_weight += weight[i];
        }
        *raw_mean = round(256.0f * (float)sum_xy / (float)sum_weight);
        break;
    }

    stats_out->wndx_sumr[0] = CLIP((s64)((u64)stats_in->sumr * awb1_gain.r / 256 - (pixel_num >> 2) * bls1_val.r), 0, MAX_29BITS);
    stats_out->wndx_sumg[0] = CLIP((s64)((u64)stats_in->sumg * awb1_gain.gr / 256 - (pixel_num >> 1) * bls1_val.gb), 0, MAX_32BITS);
    stats_out->wndx_sumb[0] = CLIP((s64)((u64)stats_in->sumb * awb1_gain.b / 256 - (pixel_num >> 2) * bls1_val.b), 0, MAX_29BITS);

    stats_out->wndx_channelr[0] = stats_out->wndx_sumr[0] / pixel_num;
    stats_out->wndx_channelg[0] = stats_out->wndx_sumg[0] / pixel_num;
    stats_out->wndx_channelb[0] = stats_out->wndx_sumb[0] / pixel_num;
}

#endif

void calcAecHistBinStatsV32(
    u32*                        hist_bin_in,
    u32*                        hist_bin_out,
    u8                          hist_mode,
    u16                         ob_offset_rb,
    u16                         ob_offset_g,
    u16                         raw_hist_bin_num,
    struct isp2x_bls_fixed_val  bls1_val,
    struct isp2x_bls_fixed_val  awb1_gain,
    bool                        bls1_en
) {

    if (bls1_en) {

        memset(hist_bin_out, 0, raw_hist_bin_num * sizeof(u32));

        s16 bls1, bls_part;
        u16 ob_part, awb_part, div_part, round_part;
        int tmp;

        switch (hist_mode) {
        case 2:
            ob_part = ob_offset_rb >> 2;
            bls1 = bls1_val.r >> 2;
            awb_part = awb1_gain.r;
            bls_part = (ob_part + bls1) * awb_part;
            div_part = 256;
            round_part = 128;
            break;
        case 3:
            ob_part = ob_offset_g >> 4;
            bls1 = bls1_val.gr >> 4;
            awb_part = awb1_gain.gr;
            bls_part = (ob_part + bls1) * awb_part;
            div_part = 256;
            round_part = 128;
            break;
        case 4:
            ob_part = ob_offset_rb >> 2;
            bls1 = bls1_val.b >> 2;
            awb_part = awb1_gain.b;
            bls_part = (ob_part + bls1) * awb_part;
            div_part = 256;
            round_part = 128;
            break;
        case 5:
        default:
            ob_part = (u16)((ob_offset_g >> 4) * 587 + (ob_offset_rb >> 2) * 299 + (ob_offset_rb >> 2) * 114 + 500) / 1000;
            bls1 = (s16)((bls1_val.gr >> 4) * 587 + (bls1_val.r >> 2) * 299 + (bls1_val.b >> 2) * 114 + 500) / 1000;
            awb_part = 100;
            bls_part = (ob_part + bls1) * awb_part;
            div_part = 7655 / awb1_gain.r + 15027 / awb1_gain.gr + 2919 / awb1_gain.b;
            round_part = div_part / 2;
            break;
        }

        if (raw_hist_bin_num == ISP32_HIST_BIN_N_MAX) {

            for (int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
                tmp = (i - ob_part - bls1 > 0) ? (i * awb_part - bls_part + round_part) / div_part : 0;
                tmp = (tmp > ISP32_HIST_BIN_N_MAX - 1) ? (ISP32_HIST_BIN_N_MAX - 1) : tmp;
                hist_bin_out[tmp] += hist_bin_in[i];
            }

        } else if (raw_hist_bin_num == ISP32L_HIST_LITE_BIN_N_MAX) {

            int oneBinWidth = 256 / ISP32L_HIST_LITE_BIN_N_MAX;
            for (int i = 0; i < ISP32L_HIST_LITE_BIN_N_MAX; i++) {
                tmp = oneBinWidth * (i + 0.5);
                tmp = (i - ob_part - bls1 > 0) ? (i * awb_part - bls_part + round_part) / div_part : 0;
                tmp = (tmp > oneBinWidth * (ISP32L_HIST_LITE_BIN_N_MAX - 0.5)) ? oneBinWidth * (ISP32L_HIST_LITE_BIN_N_MAX - 0.5) : tmp;
                tmp = tmp / oneBinWidth;
                hist_bin_out[tmp] += hist_bin_in[i];
            }
        }

    } else {

        memcpy(hist_bin_out, hist_bin_in, raw_hist_bin_num * sizeof(u32));
    }

}


XCamReturn RkAiqResourceTranslatorV32::translateAecStats(const SmartPtr<VideoBuffer>& from,
        SmartPtr<RkAiqAecStatsProxy>& to) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

#if defined(ISP_HW_V32) && defined(RKAIQ_HAVE_MULTIISP)
    if (IsMultiIspMode()) {
        return translateMultiAecStats(from, to);
    }
#endif

#if defined(ISP_HW_V32) || defined(ISP_HW_V32_LITE)
    // blc awb cfg
    struct isp32_isp_meas_cfg* isp_params = &_ispParams.meas;
    uint8_t AeSwapMode, AeSelMode, AfUseAeHW;
    AeSwapMode = isp_params->rawae0.rawae_sel;
    AeSelMode = isp_params->rawae3.rawae_sel;
    AfUseAeHW = isp_params->rawaf.ae_mode;
    unsigned int meas_type = 0;

    // ae_stats = (ae_ori_stats_u12/10 - ob_offset_u9 - bls1_val_u12) * awb1_gain_u16 * range_ratio
    struct isp32_bls_cfg* bls_cfg = &_ispParams.bls_cfg;
    struct isp32_awb_gain_cfg* awb_gain_cfg = &_ispParams.awb_gain_cfg;
    u16 isp_ob_offset_rb, isp_ob_offset_g, isp_ob_predgain;
    struct isp2x_bls_fixed_val bls1_ori_val;
    struct isp2x_bls_fixed_val bls1_val;
    struct isp2x_bls_fixed_val awb1_gain;
    u8 rawhist_mode = 0, rawhist3_mode = 0;
    u8 index0 = 0, index1 = 0;
    bool is_hdr = (getWorkingMode() > 0) ? true : false;
    bool is_bls1_en = bls_cfg->bls1_en && !is_hdr;

    isp_ob_offset_rb = MAX(bls_cfg->isp_ob_offset >> 2, 0);
    isp_ob_offset_g  = bls_cfg->isp_ob_offset;
    isp_ob_predgain =  MAX(bls_cfg->isp_ob_predgain >> 8, 1);

    if (is_bls1_en) {
        bls1_ori_val.r  = (bls_cfg->bls1_val.r / isp_ob_predgain) >> 2;
        bls1_ori_val.gr = bls_cfg->bls1_val.gr / isp_ob_predgain;
        bls1_ori_val.gb = bls_cfg->bls1_val.gb / isp_ob_predgain;
        bls1_ori_val.b  = (bls_cfg->bls1_val.b / isp_ob_predgain) >> 2;
    } else {
        bls1_ori_val.r  = 0;
        bls1_ori_val.gr = 0;
        bls1_ori_val.gb = 0;
        bls1_ori_val.b  = 0;
    }

    //awb1_gain have adapted to the range of bls1_lvl
    awb1_gain.r = MAX(256, awb_gain_cfg->awb1_gain_r);
    awb1_gain.gr = MAX(256, awb_gain_cfg->awb1_gain_gr);
    awb1_gain.gb = MAX(256, awb_gain_cfg->awb1_gain_gb);
    awb1_gain.b = MAX(256, awb_gain_cfg->awb1_gain_b);

#ifdef AE_STATS_DEBUG
    LOGE("bls1[%d-%d-%d-%d]", bls1_ori_val.r, bls1_ori_val.gr, bls1_ori_val.gb, bls1_ori_val.b);
    LOGE("isp_ob_offset_rb, isp_ob_offset_g, isp_ob_predgain [%d-%d-%d]",
         isp_ob_offset_rb, isp_ob_offset_g, isp_ob_predgain);
    LOGE("awb1_gain[%d-%d-%d-%d]", awb1_gain.r, awb1_gain.gr, awb1_gain.gb, awb1_gain.b);

    _aeAlgoStatsCfg.UpdateStats = true;
    _aeAlgoStatsCfg.RawStatsChnSel = RAWSTATS_CHN_ALL_EN;
#endif

    // bls1_val = (bls1_ori_val + ob) * awb * range_ratio
    bls1_val.r = ((isp_ob_offset_rb + bls1_ori_val.r) * awb1_gain.r + 128) / 256;
    bls1_val.gr = ((isp_ob_offset_g + bls1_ori_val.gr) * awb1_gain.gr + 128) / 256;
    bls1_val.gb = ((isp_ob_offset_g + bls1_ori_val.gb) * awb1_gain.gb + 128) / 256;
    bls1_val.b = ((isp_ob_offset_rb + bls1_ori_val.b) * awb1_gain.b + 128) / 256;

#endif

#if defined(ISP_HW_V32)

    Isp20StatsBuffer* buf = from.get_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_isp_stat_buffer* stats;
    SmartPtr<RkAiqAecStats> statsInt = to->data();

    stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if (stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("camId: %d, stats: frame_id: %d,  meas_type; 0x%x", mCamPhyId, stats->frame_id,
                  stats->meas_type);

    SmartPtr<RkAiqIrisParamsProxy> irisParams = buf->get_iris_params();

    switch (AeSwapMode) {
    case AEC_RAWSWAP_MODE_S_LITE:
        meas_type = ((stats->meas_type >> 7) & (0x01)) & ((stats->meas_type >> 11) & (0x01));
        index0       = 0;
        index1       = 1;
        rawhist_mode = isp_params->rawhist0.mode;
        break;
    case AEC_RAWSWAP_MODE_M_LITE:
        meas_type = ((stats->meas_type >> 8) & (0x01)) & ((stats->meas_type >> 12) & (0x01));
        index0       = 1;
        index1       = 0;
        rawhist_mode = isp_params->rawhist1.mode;
        break;
    default:
        LOGE("wrong AeSwapMode=%d\n", AeSwapMode);
        return XCAM_RETURN_ERROR_PARAM;
        break;
    }

    // ae stats v3.2
    statsInt->frame_id = stats->frame_id;
    statsInt->af_prior = (AfUseAeHW == 0) ? false : true;
    statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;
    if (!statsInt->aec_stats_valid) return XCAM_RETURN_BYPASS;

    // calc ae run flag
    uint64_t SumHistPix[2] = { 0, 0 };
    float SumHistBin[2] = { 0.0f, 0.0f };
    u32* hist_bin[2];
    u32 pixel_num[ISP32_RAWAEBIG_SUBWIN_NUM] = { 0 };

    hist_bin[index0] = stats->params.rawhist0.hist_bin;
    hist_bin[index1] = stats->params.rawhist1.hist_bin;
    if (!AfUseAeHW && (AeSelMode <= AEC_RAWSEL_MODE_CHN_1)) {
        hist_bin[AeSelMode] = stats->params.rawhist3.hist_bin;
    }

    for (int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
        SumHistPix[index0] += hist_bin[index0][i];
        SumHistBin[index0] += (float)(hist_bin[index0][i] * (i + 1));

        SumHistPix[index1] += hist_bin[index1][i];
        SumHistBin[index1] += (float)(hist_bin[index1][i] * (i + 1));
    }
    statsInt->aec_stats.ae_data.hist_mean[0] = (uint8_t)(SumHistBin[0] / (float)MAX(SumHistPix[0], 1));
    statsInt->aec_stats.ae_data.hist_mean[1] = (uint8_t)(SumHistBin[1] / (float)MAX(SumHistPix[1], 1));

    if (_aeRunFlag || _aeAlgoStatsCfg.UpdateStats) {

        if (is_hdr || AeSwapMode == AEC_RAWSWAP_MODE_S_LITE) {
            calcAecLiteWinStatsV32(&stats->params.rawae0,
                                   &statsInt->aec_stats.ae_data.chn[index0].rawae_lite,
                                   &statsInt->aec_stats.ae_data.raw_mean[index0],
                                   _aeAlgoStatsCfg.LiteWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                   bls1_val, awb1_gain);

            calcAecHistBinStatsV32(stats->params.rawhist0.hist_bin,
                                   statsInt->aec_stats.ae_data.chn[index0].rawhist_lite.bins,
                                   rawhist_mode, isp_ob_offset_rb, isp_ob_offset_g,
                                   ISP32_HIST_BIN_N_MAX,
                                   bls1_ori_val, awb1_gain, is_bls1_en);
        }

        if (is_hdr || AeSwapMode == AEC_RAWSWAP_MODE_M_LITE) {

            pixel_num[0] = isp_params->rawae1.subwin[0].h_size * isp_params->rawae1.subwin[0].v_size;
            pixel_num[1] = isp_params->rawae1.subwin[1].h_size * isp_params->rawae1.subwin[1].v_size;
            pixel_num[2] = isp_params->rawae1.subwin[2].h_size * isp_params->rawae1.subwin[2].v_size;
            pixel_num[3] = isp_params->rawae1.subwin[3].h_size * isp_params->rawae1.subwin[3].v_size;
            calcAecBigWinStatsV32(&stats->params.rawae1_0, &stats->params.rawae1_1,
                                  &statsInt->aec_stats.ae_data.chn[index1].rawae_big,
                                  &statsInt->aec_stats.ae_data.raw_mean[index1],
                                  _aeAlgoStatsCfg.BigWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                  bls1_val, awb1_gain, pixel_num);

            calcAecHistBinStatsV32(stats->params.rawhist1.hist_bin,
                                   statsInt->aec_stats.ae_data.chn[index1].rawhist_big.bins,
                                   rawhist_mode, isp_ob_offset_rb, isp_ob_offset_g,
                                   ISP32_HIST_BIN_N_MAX,
                                   bls1_ori_val, awb1_gain, is_bls1_en);
        }

        //AE-BIG (RAWAE3)
        if (!AfUseAeHW) {
            switch (AeSelMode) {
            case AEC_RAWSEL_MODE_CHN_0:
            case AEC_RAWSEL_MODE_CHN_1:

                pixel_num[0] = isp_params->rawae3.subwin[0].h_size * isp_params->rawae3.subwin[0].v_size;
                pixel_num[1] = isp_params->rawae3.subwin[1].h_size * isp_params->rawae3.subwin[1].v_size;
                pixel_num[2] = isp_params->rawae3.subwin[2].h_size * isp_params->rawae3.subwin[2].v_size;
                pixel_num[3] = isp_params->rawae3.subwin[3].h_size * isp_params->rawae3.subwin[3].v_size;
                calcAecBigWinStatsV32(&stats->params.rawae3_0, &stats->params.rawae3_1,
                                      &statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big,
                                      &statsInt->aec_stats.ae_data.raw_mean[AeSelMode],
                                      _aeAlgoStatsCfg.BigWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                      bls1_val, awb1_gain, pixel_num);

                rawhist3_mode = isp_params->rawhist3.mode;
                calcAecHistBinStatsV32(stats->params.rawhist3.hist_bin,
                                       statsInt->aec_stats.ae_data.chn[AeSelMode].rawhist_big.bins,
                                       rawhist3_mode, isp_ob_offset_rb, isp_ob_offset_g,
                                       ISP32_HIST_BIN_N_MAX,
                                       bls1_ori_val, awb1_gain, is_bls1_en);
                break;

            case AEC_RAWSEL_MODE_TMO:
#if 0
                bls1_val.r = 0;
                bls1_val.gr = 0;
                bls1_val.gb = 0;
                bls1_val.b = 0;

                awb1_gain.r = 256;
                awb1_gain.gr = 256;
                awb1_gain.gb = 256;
                awb1_gain.b = 256;

                calcAecBigWinStatsV32(&stats->params.rawae3_0, &stats->params.rawae3_1,
                                      &statsInt->aec_stats.ae_data.extra.rawae_big,
                                      &statsInt->aec_stats.ae_data.raw_mean[AeSelMode],
                                      _aeAlgoStatsCfg.BigWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                      bls1_val, awb1_gain, pixel_num);

                memcpy(statsInt->aec_stats.ae_data.extra.rawhist_big.bins,
                       stats->params.rawhist3.hist_bin, ISP32_HIST_BIN_N_MAX * sizeof(u32));
#endif
                break;

            default:
                LOGE("wrong AeSelMode=%d\n", AeSelMode);
                return XCAM_RETURN_ERROR_PARAM;
                break;
            }
        }
    }

#ifdef AE_STATS_DEBUG
    if (AeSwapMode != 0) {
        for (int i = 0; i < 15; i++) {
            for (int j = 0; j < 15; j++) {
                printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelr_xy[i * 15 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelg_xy[i * 15 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_big.channelb_xy[i * 15 + j]);
            }
        }
        printf("====================sub-win-result======================\n");

        for (int i = 0; i < 4; i++)
            printf("chn0_subwin[%d]:sumr 0x%08" PRIx64", sumg 0x%08" PRIx64", sumb 0x%08" PRIx64"\n", i,
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumr[i],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumg[i],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumb[i]);

        printf("====================hist_result========================\n");

        for (int i = 0; i < 256; i++)
            printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins[i]);

    } else {
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelr_xy[i * 5 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelg_xy[i * 5 + j],
                       statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelb_xy[i * 5 + j]);
            }
        }
        printf("====================hist_result========================\n");
        for (int i = 0; i < 256; i++)
            printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins[i]);
    }

#endif

    /*
     *         unsigned long chn0_mean = 0, chn1_mean = 0;
     *         for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {
     *             chn0_mean += stats->params.rawae1.data[i].channelg_xy;
     *             chn1_mean += stats->params.rawae3.data[i].channelg_xy;
     *         }
     *
     *
     *         printf("frame[%d]: chn[0-1]_g_mean_xy: %ld-%ld\n",
     *                 stats->frame_id, chn0_mean/ISP3X_RAWAEBIG_MEAN_NUM,
     *                 chn1_mean/ISP3X_RAWAEBIG_MEAN_NUM);
     */

    // expsoure params
    if (_expParams.ptr()) {
        statsInt->aec_stats.ae_exp = _expParams->data()->aecExpInfo;
        /*printf("frame[%d],gain=%d,time=%d\n", stats->frame_id,
               expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.analog_gain_code_global,
               expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.coarse_integration_time);*/

        /*
         * printf("%s: L: [0x%x-0x%x], M: [0x%x-0x%x], S: [0x%x-0x%x]\n",
         *        __func__,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.analog_gain_code_global);
         */
    }

    // iris params
    if (irisParams.ptr()) {
        float sof_time   = (float)irisParams->data()->sofTime / 1000000000.0f;
        float start_time = (float)irisParams->data()->PIris.StartTim.tv_sec +
                           (float)irisParams->data()->PIris.StartTim.tv_usec / 1000000.0f;
        float end_time = (float)irisParams->data()->PIris.EndTim.tv_sec +
                         (float)irisParams->data()->PIris.EndTim.tv_usec / 1000000.0f;
        float frm_intval = 1 / (statsInt->aec_stats.ae_exp.pixel_clock_freq_mhz * 1000000.0f /
                                (float)statsInt->aec_stats.ae_exp.line_length_pixels /
                                (float)statsInt->aec_stats.ae_exp.frame_length_lines);

        /*printf("%s: step=%d,last-step=%d,start-tim=%f,end-tim=%f,sof_tim=%f\n",
            __func__,
            statsInt->aec_stats.ae_exp.Iris.PIris.step,
            irisParams->data()->PIris.laststep,start_time,end_time,sof_time);
        */

        if (sof_time < end_time + frm_intval)
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.laststep;
        else
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.step;
    }

    to->set_sequence(stats->frame_id);
#endif


#if defined(ISP_HW_V32_LITE)
    Isp20StatsBuffer* buf = from.get_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_lite_stat_buffer* stats;
    SmartPtr<RkAiqAecStats> statsInt = to->data();

    stats = (struct rkisp32_lite_stat_buffer*)(buf->get_v4l2_userptr());
    if (stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("camId: %d, stats: frame_id: %d,  meas_type; 0x%x", mCamPhyId, stats->frame_id,
                  stats->meas_type);

    SmartPtr<RkAiqIrisParamsProxy> irisParams = buf->get_iris_params();

    switch (AeSwapMode) {
    case AEC_RAWSWAP_MODE_S_LITE:
        meas_type = ((stats->meas_type >> 7) & (0x01)) & ((stats->meas_type >> 11) & (0x01));
        index0       = 0;
        //index1       = 1;
        rawhist_mode = isp_params->rawhist0.mode;
        break;
    /*case AEC_RAWSWAP_MODE_M_LITE:
        meas_type = ((stats->meas_type >> 8) & (0x01)) & ((stats->meas_type >> 12) & (0x01));
        index0       = 1;
        index1       = 0;
        rawhist_mode = isp_params->rawhist1.mode;
        break;*/
    default:
        LOGE("wrong AeSwapMode=%d\n", AeSwapMode);
        return XCAM_RETURN_ERROR_PARAM;
        break;
    }

    // ae stats v3.2-lite
    statsInt->frame_id = stats->frame_id;
    statsInt->aec_stats_valid = (meas_type & 0x01) ? true : false;
    /*for isp32-lite, ae_sel=0 use BIG, ae_sel=1 use LITE*/
    statsInt->af_prior = (isp_params->rawaf.ae_sel == 0) ? true : false;
    if (!statsInt->aec_stats_valid)
        return XCAM_RETURN_BYPASS;

    if(is_hdr && statsInt->af_prior) {
        if(AeSelMode != AEC_RAWSEL_MODE_CHN_1) {
            LOGE("wrong AeSelMode=%d\n", AeSelMode);
            return XCAM_RETURN_ERROR_PARAM;
        }
    } else if(!is_hdr && statsInt->af_prior) {
        if(AeSelMode != AEC_RAWSEL_MODE_CHN_0) {
            LOGE("wrong AeSelMode=%d\n", AeSelMode);
            return XCAM_RETURN_ERROR_PARAM;
        }
    }

    // calc ae run flag
    uint64_t SumHistPix[2] = { 0, 0 };
    float SumHistBin[2] = { 0.0f, 0.0f };
    int oneBinWidth = 256 / ISP32L_HIST_LITE_BIN_N_MAX;
    u32 pixel_num = 0;

    for (int i = 0; i < ISP32_HIST_BIN_N_MAX; i++) {
        if (AeSelMode <= AEC_RAWSEL_MODE_CHN_1) {
            if (AeSelMode == index0) {
                SumHistPix[index0] += stats->params.rawhist3.hist_bin[i];
                SumHistBin[index0] += (float)(stats->params.rawhist3.hist_bin[i] * (i + 1));

                SumHistPix[index1] = 1;
                SumHistBin[index1] = 0;

            } else if (AeSelMode == index1) {
                if (i < ISP32L_HIST_LITE_BIN_N_MAX) {
                    SumHistPix[index0] += stats->params.rawhist0.hist_bin[i];
                    SumHistBin[index0] += (float)(stats->params.rawhist0.hist_bin[i] * (i + 1) * oneBinWidth);
                }

                SumHistPix[index1] += stats->params.rawhist3.hist_bin[i];
                SumHistBin[index1] += (float)(stats->params.rawhist3.hist_bin[i] * (i + 1));
            }

        } else {
            if (i < ISP32L_HIST_LITE_BIN_N_MAX) {
                SumHistPix[index0] += stats->params.rawhist0.hist_bin[i];
                SumHistBin[index0] += (float)(stats->params.rawhist0.hist_bin[i] * (i + 1) * oneBinWidth);
            }
            SumHistPix[index1] = 1;
            SumHistBin[index1] = 0;
        }
    }
    statsInt->aec_stats.ae_data.hist_mean[0] = (uint8_t)(SumHistBin[0] / (float)MAX(SumHistPix[0], 1));
    statsInt->aec_stats.ae_data.hist_mean[1] = (uint8_t)(SumHistBin[1] / (float)MAX(SumHistPix[1], 1));

    if (_aeRunFlag || _aeAlgoStatsCfg.UpdateStats) {

        if (!statsInt->af_prior) {
            if (is_hdr || AeSwapMode == AEC_RAWSWAP_MODE_S_LITE) {
                calcAecLiteWinStatsV32Lite(&stats->params.rawae0,
                                           &statsInt->aec_stats.ae_data.chn[index0].rawae_lite,
                                           &statsInt->aec_stats.ae_data.raw_mean[index0],
                                           _aeAlgoStatsCfg.LiteWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                           bls1_val, awb1_gain);

                calcAecHistBinStatsV32(stats->params.rawhist0.hist_bin,
                                       statsInt->aec_stats.ae_data.chn[index0].rawhist_lite.bins,
                                       rawhist_mode, isp_ob_offset_rb, isp_ob_offset_g,
                                       ISP32L_HIST_LITE_BIN_N_MAX,
                                       bls1_ori_val, awb1_gain, is_bls1_en);
            }
        }

        // AE/HIST-BIG (RAWAE3/RAWHIST3)
        switch (AeSelMode) {
        case AEC_RAWSEL_MODE_CHN_0:
        case AEC_RAWSEL_MODE_CHN_1:

            //RAWAE3 independent block, only one for isp32-lite
            pixel_num = MAX(1, isp_params->rawae3.subwin[0].h_size * isp_params->rawae3.subwin[0].v_size);
            calcAecBigWinStatsV32Lite(&stats->params.rawae3,
                                      &statsInt->aec_stats.ae_data.chn[AeSelMode].rawae_big,
                                      &statsInt->aec_stats.ae_data.raw_mean[AeSelMode],
                                      _aeAlgoStatsCfg.BigWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                      bls1_val, awb1_gain, pixel_num);

            rawhist3_mode = isp_params->rawhist3.mode;
            calcAecHistBinStatsV32(stats->params.rawhist3.hist_bin,
                                   statsInt->aec_stats.ae_data.chn[AeSelMode].rawhist_big.bins,
                                   rawhist3_mode, isp_ob_offset_rb, isp_ob_offset_g,
                                   ISP32_HIST_BIN_N_MAX,
                                   bls1_ori_val, awb1_gain, is_bls1_en);
            break;

        case AEC_RAWSEL_MODE_TMO:
            bls1_val.r = 0;
            bls1_val.gr = 0;
            bls1_val.gb = 0;
            bls1_val.b = 0;

            awb1_gain.r = 256;
            awb1_gain.gr = 256;
            awb1_gain.gb = 256;
            awb1_gain.b = 256;

            pixel_num = MAX(1, isp_params->rawae3.subwin[0].h_size * isp_params->rawae3.subwin[0].v_size);
            calcAecBigWinStatsV32Lite(&stats->params.rawae3,
                                      &statsInt->aec_stats.ae_data.extra.rawae_big,
                                      &statsInt->aec_stats.ae_data.raw_mean[AeSelMode],
                                      _aeAlgoStatsCfg.BigWeight, _aeAlgoStatsCfg.RawStatsChnSel, _aeAlgoStatsCfg.YRangeMode,
                                      bls1_val, awb1_gain, pixel_num);

            memcpy(statsInt->aec_stats.ae_data.extra.rawhist_big.bins,
                   stats->params.rawhist3.hist_bin, ISP32_HIST_BIN_N_MAX * sizeof(u32));
            break;

        default:
            LOGE("wrong AeSelMode=%d\n", AeSelMode);
            return XCAM_RETURN_ERROR_PARAM;
        }
    }

#ifdef AE_STATS_DEBUG
    if(is_hdr) {
        if(AeSelMode != 1) {
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 5; j++) {
                    printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                           statsInt->aec_stats.ae_data.chn[1].rawae_lite.channelr_xy[i * 5 + j],
                           statsInt->aec_stats.ae_data.chn[1].rawae_lite.channelg_xy[i * 5 + j],
                           statsInt->aec_stats.ae_data.chn[1].rawae_lite.channelb_xy[i * 5 + j]);
                }
            }
            printf("====================hist_result========================\n");
            for (int i = 0; i < ISP32L_HIST_LITE_BIN_N_MAX; i++)
                printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[1].rawhist_lite.bins[i]);
        } else {

            for (int i = 0; i < 15; i++) {
                for (int j = 0; j < 15; j++) {
                    printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                           statsInt->aec_stats.ae_data.chn[1].rawae_big.channelr_xy[i * 15 + j],
                           statsInt->aec_stats.ae_data.chn[1].rawae_big.channelg_xy[i * 15 + j],
                           statsInt->aec_stats.ae_data.chn[1].rawae_big.channelb_xy[i * 15 + j]);
                }
            }
            printf("====================sub-win-result======================\n");

            printf("chn0_subwin:sumr 0x%08" PRIx64", sumg 0x%08" PRIx64", sumb 0x%08" PRIx64"\n",
                   statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumr[0],
                   statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumg[0],
                   statsInt->aec_stats.ae_data.chn[1].rawae_big.wndx_sumb[0]);

            printf("====================hist_result========================\n");

            for (int i = 0; i < 256; i++)
                printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[1].rawhist_big.bins[i]);
        }
    } else {
        if (AeSelMode == 0) {
            for (int i = 0; i < 15; i++) {
                for (int j = 0; j < 15; j++) {
                    printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                           statsInt->aec_stats.ae_data.chn[0].rawae_big.channelr_xy[i * 15 + j],
                           statsInt->aec_stats.ae_data.chn[0].rawae_big.channelg_xy[i * 15 + j],
                           statsInt->aec_stats.ae_data.chn[0].rawae_big.channelb_xy[i * 15 + j]);
                }
            }
            printf("====================sub-win-result======================\n");

            printf("chn0_subwin:sumr 0x%08" PRIx64", sumg 0x%08" PRIx64", sumb 0x%08" PRIx64"\n",
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumr[0],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumg[0],
                   statsInt->aec_stats.ae_data.chn[0].rawae_big.wndx_sumb[0]);

            printf("====================hist_result========================\n");

            for (int i = 0; i < 256; i++)
                printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_big.bins[i]);

        } else {
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 5; j++) {
                    printf("chn0[%d,%d]:r 0x%x, g 0x%x, b 0x%x\n", i, j,
                           statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelr_xy[i * 5 + j],
                           statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelg_xy[i * 5 + j],
                           statsInt->aec_stats.ae_data.chn[0].rawae_lite.channelb_xy[i * 5 + j]);
                }
            }
            printf("====================hist_result========================\n");
            for (int i = 0; i < ISP32L_HIST_LITE_BIN_N_MAX; i++)
                printf("bin[%d]= 0x%08x\n", i, statsInt->aec_stats.ae_data.chn[0].rawhist_lite.bins[i]);
        }
    }

#endif

    /*
     *         unsigned long chn0_mean = 0, chn1_mean = 0;
     *         for(int i = 0; i < ISP3X_RAWAEBIG_MEAN_NUM; i++) {
     *             chn0_mean += stats->params.rawae1.data[i].channelg_xy;
     *             chn1_mean += stats->params.rawae3.data[i].channelg_xy;
     *         }
     *
     *
     *         printf("frame[%d]: chn[0-1]_g_mean_xy: %ld-%ld\n",
     *                 stats->frame_id, chn0_mean/ISP3X_RAWAEBIG_MEAN_NUM,
     *                 chn1_mean/ISP3X_RAWAEBIG_MEAN_NUM);
     */

    // expsoure params
    if (_expParams.ptr()) {
        statsInt->aec_stats.ae_exp = _expParams->data()->aecExpInfo;
        /*printf("frame[%d],gain=%d,time=%d\n", stats->frame_id,
               expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.analog_gain_code_global,
               expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.coarse_integration_time);*/

        /*
         * printf("%s: L: [0x%x-0x%x], M: [0x%x-0x%x], S: [0x%x-0x%x]\n",
         *        __func__,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[2].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[1].exp_sensor_params.analog_gain_code_global,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.coarse_integration_time,
         *        expParams->data()->aecExpInfo.HdrExp[0].exp_sensor_params.analog_gain_code_global);
         */
    }

    // iris params
    if (irisParams.ptr()) {
        float sof_time   = (float)irisParams->data()->sofTime / 1000000000.0f;
        float start_time = (float)irisParams->data()->PIris.StartTim.tv_sec +
                           (float)irisParams->data()->PIris.StartTim.tv_usec / 1000000.0f;
        float end_time = (float)irisParams->data()->PIris.EndTim.tv_sec +
                         (float)irisParams->data()->PIris.EndTim.tv_usec / 1000000.0f;
        float frm_intval = 1 / (statsInt->aec_stats.ae_exp.pixel_clock_freq_mhz * 1000000.0f /
                                (float)statsInt->aec_stats.ae_exp.line_length_pixels /
                                (float)statsInt->aec_stats.ae_exp.frame_length_lines);

        /*printf("%s: step=%d,last-step=%d,start-tim=%f,end-tim=%f,sof_tim=%f\n",
            __func__,
            statsInt->aec_stats.ae_exp.Iris.PIris.step,
            irisParams->data()->PIris.laststep,start_time,end_time,sof_time);
        */

        if (sof_time < end_time + frm_intval)
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.laststep;
        else
            statsInt->aec_stats.ae_exp.Iris.PIris.step = irisParams->data()->PIris.step;
    }

    to->set_sequence(stats->frame_id);
#endif

    return ret;
}


XCamReturn RkAiqResourceTranslatorV32::translateAwbStats(const SmartPtr<VideoBuffer>& from,
        SmartPtr<RkAiqAwbStatsProxy>& to) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
#if defined(ISP_HW_V32) || defined(ISP_HW_V32_LITE)
    Isp20StatsBuffer* buf =
        from.get_cast_ptr<Isp20StatsBuffer>();
#if defined(ISP_HW_V32)
    struct rkisp32_isp_stat_buffer *stats;
    stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr());
#elif defined(ISP_HW_V32_LITE)
    struct rkisp32_lite_stat_buffer *stats;
    stats = (struct rkisp32_lite_stat_buffer*)(buf->get_v4l2_userptr());
#endif

#if defined(ISP_HW_V32) && defined(RKAIQ_HAVE_MULTIISP)
    if (IsMultiIspMode()) {
        return translateMultiAwbStats(from, to);
    }
#endif

    SmartPtr<RkAiqAwbStats> statsInt = to->data();

    if(stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("awb stats: camId:%d, frame_id: %d,  meas_type; 0x%x",
                  mCamPhyId, stats->frame_id, stats->meas_type);

    statsInt->awb_stats_valid = stats->meas_type >> 5 & 1;
    if (!statsInt->awb_stats_valid) {
        LOGE_ANALYZER("AWB stats invalid, ignore");
        return XCAM_RETURN_BYPASS;
    }

    if (stats->params.info2ddr.owner == RKISP_INFO2DRR_OWNER_AWB) {
        statsInt->awb_stats_v32.dbginfo_fd = stats->params.info2ddr.buf_fd;
    } else {
        statsInt->awb_stats_v32.dbginfo_fd = -1;
    }
    statsInt->awb_stats_v32.awb_cfg_effect_v32.blkMeasureMode = _ispParams.awb_cfg_v32.blkMeasureMode;
    statsInt->awb_stats_v32.awb_cfg_effect_v32.lightNum = _ispParams.awb_cfg_v32.lightNum;
    statsInt->awb_stats_v32.awb_cfg_effect_v32.groupIllIndxCurrent = _ispParams.awb_cfg_v32.groupIllIndxCurrent;
    memcpy(statsInt->awb_stats_v32.awb_cfg_effect_v32.IllIndxSetCurrent, _ispParams.awb_cfg_v32.IllIndxSetCurrent,
           sizeof(statsInt->awb_stats_v32.awb_cfg_effect_v32.IllIndxSetCurrent));
    memcpy(statsInt->awb_stats_v32.awb_cfg_effect_v32.timeSign, _ispParams.awb_cfg_v32.timeSign,
           sizeof(statsInt->awb_stats_v32.awb_cfg_effect_v32.timeSign));
    statsInt->awb_cfg_effect_valid = true;
    statsInt->frame_id = stats->frame_id;
    for(int i = 0; i < statsInt->awb_stats_v32.awb_cfg_effect_v32.lightNum; i++) {
        statsInt->awb_stats_v32.light[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].RgainValue =
            stats->params.rawawb.sum[i].rgain_nor;
        statsInt->awb_stats_v32.light[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].BgainValue =
            stats->params.rawawb.sum[i].bgain_nor;
        statsInt->awb_stats_v32.light[i].xYType[RK_AIQ_AWB_XY_TYPE_NORMAL_V201].WpNo =
            stats->params.rawawb.sum[i].wp_num_nor;
        statsInt->awb_stats_v32.light[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].RgainValue =
            stats->params.rawawb.sum[i].rgain_big;
        statsInt->awb_stats_v32.light[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].BgainValue =
            stats->params.rawawb.sum[i].bgain_big;
        statsInt->awb_stats_v32.light[i].xYType[RK_AIQ_AWB_XY_TYPE_BIG_V201].WpNo =
            stats->params.rawawb.sum[i].wp_num_big;
        statsInt->awb_stats_v32.WpNo2[i] =  stats->params.rawawb.sum[i].wp_num2;
    }
    memset(&statsInt->awb_stats_v32.sumBlkRGB, 0, sizeof(statsInt->awb_stats_v32.sumBlkRGB));
#if defined(ISP_HW_V32)
    for(int i = 0; i < RK_AIQ_AWB_GRID_NUM_TOTAL; i++) {
        statsInt->awb_stats_v32.blockResult[i].Rvalue = stats->params.rawawb.ramdata[i].r;
        statsInt->awb_stats_v32.blockResult[i].Gvalue = stats->params.rawawb.ramdata[i].g;
        statsInt->awb_stats_v32.blockResult[i].Bvalue = stats->params.rawawb.ramdata[i].b;
        statsInt->awb_stats_v32.blockResult[i].WpNo = stats->params.rawawb.ramdata[i].wp;
        statsInt->awb_stats_v32.sumBlkRGB.Rvalue += statsInt->awb_stats_v32.blockResult[i].Rvalue ;
        statsInt->awb_stats_v32.sumBlkRGB.Gvalue += statsInt->awb_stats_v32.blockResult[i].Gvalue;
        statsInt->awb_stats_v32.sumBlkRGB.Bvalue +=  statsInt->awb_stats_v32.blockResult[i].Bvalue;
    }

#elif defined(ISP_HW_V32_LITE)
    for(int i = 0; i < ISP32L_RAWAWB_RAMDATA_RGB_NUM; i++) {
        statsInt->awb_stats_v32.blockResult[i].Rvalue = stats->params.rawawb.ramdata_r[i];
        statsInt->awb_stats_v32.blockResult[i].Gvalue = stats->params.rawawb.ramdata_g[i];
        statsInt->awb_stats_v32.blockResult[i].Bvalue = stats->params.rawawb.ramdata_b[i];
        if(i % 2 == 0) {
            statsInt->awb_stats_v32.blockResult[i].WpNo = stats->params.rawawb.ramdata_wpnum0[i / 2];
        } else {
            statsInt->awb_stats_v32.blockResult[i].WpNo = stats->params.rawawb.ramdata_wpnum1[i / 2];
        }
        statsInt->awb_stats_v32.sumBlkRGB.Rvalue += statsInt->awb_stats_v32.blockResult[i].Rvalue ;
        statsInt->awb_stats_v32.sumBlkRGB.Gvalue += statsInt->awb_stats_v32.blockResult[i].Gvalue;
        statsInt->awb_stats_v32.sumBlkRGB.Bvalue +=  statsInt->awb_stats_v32.blockResult[i].Bvalue;
    }
#endif
    for(int i = 0; i < RK_AIQ_AWB_WP_HIST_BIN_NUM; i++) {
        statsInt->awb_stats_v32.WpNoHist[i] = stats->params.rawawb.yhist_bin[i];
        // move the shift code here to make WpNoHist merged by several cameras easily
        if( stats->params.rawawb.yhist_bin[i]  & 0x8000 ) {
            statsInt->awb_stats_v32.WpNoHist[i] = stats->params.rawawb.yhist_bin[i] & 0x7FFF;
            statsInt->awb_stats_v32.WpNoHist[i] *=    (1 << 3);
        }
    }

    for(int i = 0; i < RK_AIQ_AWB_STAT_WP_RANGE_NUM_V201; i++) {
        statsInt->awb_stats_v32.excWpRangeResult[i].RgainValue = stats->params.rawawb.sum_exc[i].rgain_exc;
        statsInt->awb_stats_v32.excWpRangeResult[i].BgainValue = stats->params.rawawb.sum_exc[i].bgain_exc;
        statsInt->awb_stats_v32.excWpRangeResult[i].WpNo =    stats->params.rawawb.sum_exc[i].wp_num_exc;

    }

    to->set_sequence(stats->frame_id);
#endif
    return ret;
}

XCamReturn RkAiqResourceTranslatorV32::translateAfStats(const SmartPtr<VideoBuffer>& from,
        SmartPtr<RkAiqAfStatsProxy>& to) {

    struct isp32_rawaf_luma_data {
        u32 channelg_xy: 12;
        u32 highlit_cnt: 16;
        u32 dummy: 4;
    } __attribute__ ((packed));

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
#if defined(ISP_HW_V32)
    Isp20StatsBuffer* buf =
        from.get_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_isp_stat_buffer *stats;
    SmartPtr<RkAiqAfStats> statsInt = to->data();

#if defined(ISP_HW_V32) && defined(RKAIQ_HAVE_MULTIISP)
    if (IsMultiIspMode()) {
        return translateMultiAfStats(from, to);
    }
#endif

    stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if(stats == NULL) {
        LOGE_AF("fail to get stats, ignore");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x",
                  stats->frame_id, stats->meas_type);

    statsInt->af_stats_valid =
        (stats->meas_type >> 6) & (0x01) ? true : false;
    if (!statsInt->af_stats_valid)
        return XCAM_RETURN_BYPASS;

    SmartPtr<RkAiqAfInfoProxy> afParams = buf->get_af_params();
    struct isp32_bls_cfg* bls_cfg = &_ispParams.bls_cfg;
    u8 from_awb = _ispParams.meas.rawaf.from_awb;
    u8 from_ynr = _ispParams.meas.rawaf.from_ynr;
    bool is_hdr = (getWorkingMode() > 0) ? true : false;
    int temp_luma, comp_bls = 0;
    u16 max_val = (1 << 12) - 1;

    if (bls_cfg->bls1_en && !is_hdr && !from_awb && !from_ynr) {
        comp_bls = (bls_cfg->bls1_val.gr + bls_cfg->bls1_val.gb) / 2  - bls_cfg->isp_ob_offset;
        comp_bls = MAX(comp_bls, 0);
    }

#if 0
    struct isp32_awb_gain_cfg* awb_gain_cfg = &_ispParams.awb_gain_cfg;

    LOGE("bls0[%d-%d]", bls_cfg->fixed_val.gr, bls_cfg->fixed_val.gb);
    LOGE("bls1[%d-%d]", bls_cfg->bls1_val.gr, bls_cfg->bls1_val.gb);
    LOGE("isp_ob_offset, isp_ob_max, isp_ob_predgain [%d-%d-%d]",
         bls_cfg->isp_ob_offset, bls_cfg->isp_ob_max, bls_cfg->isp_ob_predgain);
    LOGE("awb0_gain[%d-%d], awb1_gain[%d-%d]",
         awb_gain_cfg->gain0_green_b, awb_gain_cfg->gain0_green_r,
         awb_gain_cfg->gain1_green_b, awb_gain_cfg->gain1_green_r);
    LOGE("comp_bls %d", comp_bls);
#endif

    memset(&statsInt->af_stats_v3x, 0, sizeof(rk_aiq_isp_af_stats_v3x_t));
    statsInt->frame_id = stats->frame_id;

    //af
    {
        statsInt->af_stats_v3x.comp_bls = comp_bls >> 2;
        statsInt->af_stats_v3x.wndb_luma = stats->params.rawaf.afm_lum_b;
        statsInt->af_stats_v3x.wndb_sharpness = stats->params.rawaf.afm_sum_b;
        statsInt->af_stats_v3x.winb_highlit_cnt = stats->params.rawaf.highlit_cnt_winb;
        for (int i = 0; i < RKAIQ_RAWAF_SUMDATA_NUM; i++) {
            statsInt->af_stats_v3x.wnda_fv_v1[i] = stats->params.rawaf.ramdata[i].v1;
            statsInt->af_stats_v3x.wnda_fv_v2[i] = stats->params.rawaf.ramdata[i].v2;
            statsInt->af_stats_v3x.wnda_fv_h1[i] = stats->params.rawaf.ramdata[i].h1;
            statsInt->af_stats_v3x.wnda_fv_h2[i] = stats->params.rawaf.ramdata[i].h2;
        }

        struct isp32_rawaf_luma_data *luma = (struct isp32_rawaf_luma_data *)&stats->params.rawae3_0.data[0];
        for (int i = 0; i < RKAIQ_RAWAF_SUMDATA_NUM; i++) {
            temp_luma = (luma[i].channelg_xy - comp_bls) * max_val / (max_val - comp_bls);
            statsInt->af_stats_v3x.wnda_luma[i] = MAX(temp_luma, 0);
            statsInt->af_stats_v3x.wina_highlit_cnt[i] = luma[i].highlit_cnt;
        }

        if(afParams.ptr()) {
            statsInt->af_stats_v3x.focusCode = afParams->data()->focusCode;
            statsInt->af_stats_v3x.zoomCode = afParams->data()->zoomCode;
            statsInt->af_stats_v3x.focus_endtim = afParams->data()->focusEndTim;
            statsInt->af_stats_v3x.focus_starttim = afParams->data()->focusStartTim;
            statsInt->af_stats_v3x.zoom_endtim = afParams->data()->zoomEndTim;
            statsInt->af_stats_v3x.zoom_starttim = afParams->data()->zoomStartTim;
            statsInt->af_stats_v3x.sof_tim = afParams->data()->sofTime;
            statsInt->af_stats_v3x.focusCorrection = afParams->data()->focusCorrection;
            statsInt->af_stats_v3x.zoomCorrection = afParams->data()->zoomCorrection;
            statsInt->af_stats_v3x.angleZ = afParams->data()->angleZ;
        }

        if (_expParams.ptr())
            statsInt->aecExpInfo = _expParams->data()->aecExpInfo;
    }

#endif

#if defined(ISP_HW_V32_LITE)
    Isp20StatsBuffer* buf =
        from.get_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_lite_stat_buffer *stats_lite;
    SmartPtr<RkAiqAfStats> statsInt = to->data();

    stats_lite = (struct rkisp32_lite_stat_buffer*)(buf->get_v4l2_userptr());
    if(stats_lite == NULL) {
        LOGE_AF("fail to get stats, ignore");
        return XCAM_RETURN_BYPASS;
    }
    LOGD_AF("stats_lite: frame_id: %d,  meas_type; 0x%x",
            stats_lite->frame_id, stats_lite->meas_type);

    statsInt->af_stats_valid =
        (stats_lite->meas_type >> 6) & (0x01) ? true : false;
    if (!statsInt->af_stats_valid)
        return XCAM_RETURN_BYPASS;

    SmartPtr<RkAiqAfInfoProxy> afParams = buf->get_af_params();
    struct isp32_bls_cfg* bls_cfg = &_ispParams.bls_cfg;
    u8 from_awb = _ispParams.meas.rawaf.from_awb;
    u8 from_ynr = _ispParams.meas.rawaf.from_ynr;
    bool is_hdr = (getWorkingMode() > 0) ? true : false;
    int temp_luma, comp_bls = 0;
    u16 max_val = (1 << 12) - 1;

    if (bls_cfg->bls1_en && !is_hdr && !from_awb && !from_ynr) {
        comp_bls = (bls_cfg->bls1_val.gr + bls_cfg->bls1_val.gb) / 2  - bls_cfg->isp_ob_offset;
        comp_bls = MAX(comp_bls, 0);
    }

    memset(&statsInt->af_stats_v3x, 0, sizeof(rk_aiq_isp_af_stats_v3x_t));
    statsInt->frame_id = stats_lite->frame_id;

    //af
    {
        statsInt->af_stats_v3x.comp_bls = comp_bls >> 2;
        statsInt->af_stats_v3x.wndb_luma = stats_lite->params.rawaf.afm_lum_b;
        statsInt->af_stats_v3x.wndb_sharpness = stats_lite->params.rawaf.afm_sum_b;
        statsInt->af_stats_v3x.winb_highlit_cnt = stats_lite->params.rawaf.highlit_cnt_winb;
        for (int i = 0; i < ISP32L_RAWAF_WND_DATA; i++) {
            statsInt->af_stats_v3x.wnda_fv_v1[i] = stats_lite->params.rawaf.ramdata.viir_wnd_data[i];
            statsInt->af_stats_v3x.wnda_fv_h1[i] = stats_lite->params.rawaf.ramdata.hiir_wnd_data[i];
            statsInt->af_stats_v3x.wnda_fv_v2[i] = statsInt->af_stats_v3x.wnda_fv_v1[i];
            statsInt->af_stats_v3x.wnda_fv_h2[i] = statsInt->af_stats_v3x.wnda_fv_h1[i];
        }

        for (int i = 0; i < ISP32L_RAWAF_WND_DATA; i++) {
            temp_luma = (stats_lite->params.rawae0.data[i].channelg_xy - comp_bls) * max_val / (max_val - comp_bls);
            statsInt->af_stats_v3x.wnda_luma[i] = MAX(temp_luma, 0);
            statsInt->af_stats_v3x.wina_highlit_cnt[i] =
                ((stats_lite->params.rawae0.data[i].channelr_xy & 0x3FF) << 10) | (stats_lite->params.rawae0.data[i].channelb_xy & 0x3FF);
        }

        if(afParams.ptr()) {
            statsInt->af_stats_v3x.focusCode = afParams->data()->focusCode;
            statsInt->af_stats_v3x.zoomCode = afParams->data()->zoomCode;
            statsInt->af_stats_v3x.focus_endtim = afParams->data()->focusEndTim;
            statsInt->af_stats_v3x.focus_starttim = afParams->data()->focusStartTim;
            statsInt->af_stats_v3x.zoom_endtim = afParams->data()->zoomEndTim;
            statsInt->af_stats_v3x.zoom_starttim = afParams->data()->zoomStartTim;
            statsInt->af_stats_v3x.sof_tim = afParams->data()->sofTime;
            statsInt->af_stats_v3x.focusCorrection = afParams->data()->focusCorrection;
            statsInt->af_stats_v3x.zoomCorrection = afParams->data()->zoomCorrection;
            statsInt->af_stats_v3x.angleZ = afParams->data()->angleZ;
        }

        if (_expParams.ptr())
            statsInt->aecExpInfo = _expParams->data()->aecExpInfo;
    }

#endif

    return ret;

}

#if RKAIQ_HAVE_DEHAZE_V12
XCamReturn RkAiqResourceTranslatorV32::translateAdehazeStats(const SmartPtr<VideoBuffer>& from,
        SmartPtr<RkAiqAdehazeStatsProxy>& to) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

#if defined(ISP_HW_V32)
    Isp20StatsBuffer* buf = from.get_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_isp_stat_buffer* stats;
    SmartPtr<RkAiqAdehazeStats> statsInt = to->data();

#if defined(ISP_HW_V32) && defined(RKAIQ_HAVE_MULTIISP)
    if (IsMultiIspMode()) {
        return translateMultiAdehazeStats(from, to);
    }
#endif

    stats = (struct rkisp32_isp_stat_buffer*)(buf->get_v4l2_userptr());
    if (stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x", stats->frame_id, stats->meas_type);

    // dehaze
    statsInt->adehaze_stats_valid = stats->meas_type >> 17 & 1;
    if (!statsInt->adehaze_stats_valid) return XCAM_RETURN_BYPASS;

    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_air_base =
        stats->params.dhaz.dhaz_adp_air_base;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_wt     = stats->params.dhaz.dhaz_adp_wt;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_gratio = stats->params.dhaz.dhaz_adp_gratio;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_tmax   = stats->params.dhaz.dhaz_adp_tmax;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_pic_sumh   = stats->params.dhaz.dhaz_pic_sumh;
    for (int i = 0; i < ISP32_DHAZ_HIST_IIR_NUM; i++)
        statsInt->adehaze_stats.dehaze_stats_v12.h_rgb_iir[i] = stats->params.dhaz.h_rgb_iir[i];
#endif

#if defined(ISP_HW_V32_LITE)
    Isp20StatsBuffer* buf = from.get_cast_ptr<Isp20StatsBuffer>();
    struct rkisp32_lite_stat_buffer* stats;
    SmartPtr<RkAiqAdehazeStats> statsInt = to->data();

    stats = (struct rkisp32_lite_stat_buffer*)(buf->get_v4l2_userptr());
    if (stats == NULL) {
        LOGE("fail to get stats ,ignore\n");
        return XCAM_RETURN_BYPASS;
    }
    LOGI_ANALYZER("stats: frame_id: %d,  meas_type; 0x%x", stats->frame_id, stats->meas_type);

    // dehaze
    statsInt->adehaze_stats_valid = stats->meas_type >> 17 & 1;
    if (!statsInt->adehaze_stats_valid) return XCAM_RETURN_BYPASS;

    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_air_base =
        stats->params.dhaz.dhaz_adp_air_base;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_wt     = stats->params.dhaz.dhaz_adp_wt;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_gratio = stats->params.dhaz.dhaz_adp_gratio;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_adp_tmax   = stats->params.dhaz.dhaz_adp_tmax;
    statsInt->adehaze_stats.dehaze_stats_v12.dhaz_pic_sumh   = stats->params.dhaz.dhaz_pic_sumh;
    for (int i = 0; i < ISP32_DHAZ_HIST_IIR_NUM; i++)
        statsInt->adehaze_stats.dehaze_stats_v12.h_rgb_iir[i] = stats->params.dhaz.h_rgb_iir[i];

#endif

    return ret;
}
#endif

}  // namespace RkCam

/*
 * Copyright 2020 Rockchip Electronics Co. LTD
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

#include "iep2_api.h"

#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include "mpp_env.h"
#include "mpp_debug.h"
#include "mpp_buffer.h"

#include "iep2_ff.h"
#include "iep2_pd.h"
#include "iep2_gmv.h"
#include "iep2_osd.h"
#include "iep2_roi.h"

#include "iep2.h"
#include "mpp_service.h"
#include "mpp_platform.h"

#define IEP2_TILE_W_MAX     120
#define IEP2_TILE_H_MAX     480
#define IEP2_OSD_EN         0

RK_U32 iep_debug = 0;
RK_U32 iep_md_pre_en = 0;

/* default iep2 mtn table */
static RK_U32 iep2_mtn_tab[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x01010000, 0x06050302, 0x0f0d0a08, 0x1c191512,
    0x2b282420, 0x3634312e, 0x3d3c3a38, 0x40403f3e,
    0x40404040, 0x40404040, 0x40404040, 0x40404040
};

static MPP_RET get_param_from_env(struct iep2_api_ctx *ctx)
{
    struct iep2_params *params = &ctx->params;
    mpp_env_get_u32("md_theta", &params->md_theta, ctx->params.md_theta);
    mpp_env_get_u32("md_r", &params->md_r, ctx->params.md_r);
    mpp_env_get_u32("md_lambda", &params->md_lambda, ctx->params.md_lambda);

    mpp_env_get_u32("mv_similar_thr", &params->mv_similar_thr, ctx->params.mv_similar_thr);
    mpp_env_get_u32("mv_similar_num_thr0", &params->mv_similar_num_thr0, ctx->params.mv_similar_num_thr0);

    mpp_env_get_u32("eedi_thr0", &params->eedi_thr0, ctx->params.eedi_thr0);
    mpp_env_get_u32("comb_t_thr", &params->comb_t_thr, ctx->params.comb_t_thr);
    mpp_env_get_u32("comb_feature_thr", &params->comb_feature_thr, ctx->params.comb_feature_thr);
    return MPP_OK;
}

static MPP_RET iep2_init(IepCtx *ictx)
{
    MPP_RET ret;
    struct iep2_api_ctx *ctx = *ictx;
    MppReqV1 mpp_req;
    RK_U32 client_data = IEP_CLIENT_TYPE;

    mpp_env_get_u32("iep_debug", &iep_debug, 0);

    ctx->fd = open("/dev/mpp_service", O_RDWR | O_CLOEXEC);
    if (ctx->fd < 0) {
        mpp_err("can NOT find device /dev/iep2\n");
        return MPP_NOK;
    }

    mpp_req.cmd = MPP_CMD_INIT_CLIENT_TYPE;
    mpp_req.flag = 0;
    mpp_req.size = sizeof(client_data);
    mpp_req.data_ptr = REQ_DATA_PTR(&client_data);

    ret = (RK_S32)ioctl(ctx->fd, MPP_IOC_CFG_V1, &mpp_req);
    if (ret) {
        mpp_err("ioctl set_client failed\n");
        return MPP_NOK;
    }

    // set default parameters.
    ctx->params.src_fmt = IEP2_FMT_YUV420;
    ctx->params.src_yuv_swap = IEP2_YUV_SWAP_SP_UV;
    ctx->params.dst_fmt = IEP2_FMT_YUV420;
    ctx->params.dst_yuv_swap = IEP2_YUV_SWAP_SP_UV;

    ctx->params.src_y_stride = 720 / 4;
    ctx->params.src_uv_stride = 720 / 4;
    ctx->params.dst_y_stride = 720 / 4;

    ctx->params.tile_cols = 720 / 16;
    ctx->params.tile_rows = 480 / 4;

    ctx->params.dil_mode = IEP2_DIL_MODE_I1O1T;

    ctx->params.dil_out_mode = IEP2_OUT_MODE_LINE;
    ctx->params.dil_field_order = IEP2_FIELD_ORDER_TFF;

    ctx->params.md_theta = 1;
    ctx->params.md_r = 6;

    if (mpp_get_soc_type() == ROCKCHIP_SOC_RK3528) {
        mpp_env_get_u32("iep_md_pre_en", &iep_md_pre_en, 0);
        if (iep_md_pre_en) {
            ctx->params.md_lambda = 4;
        } else {
            ctx->params.md_lambda = 8;
        }
    } else {
        ctx->params.md_lambda = 4;
    }

    ctx->params.dect_resi_thr = 30;
    ctx->params.osd_area_num = 0;
    ctx->params.osd_gradh_thr = 60;
    ctx->params.osd_gradv_thr = 60;

    ctx->params.osd_pos_limit_en = 0;
    ctx->params.osd_pos_limit_num = 0;

    ctx->params.osd_pec_thr = 20;
    ctx->params.osd_line_num = 2;

    ctx->params.me_pena = 5;
    ctx->params.mv_similar_thr = 4;
    ctx->params.mv_similar_num_thr0 = 4;
    ctx->params.mv_bonus = 10;
    ctx->params.me_thr_offset = 20;

    ctx->params.mv_left_limit = 28;
    ctx->params.mv_right_limit = 27;

    ctx->params.eedi_thr0 = 12;

    memset(ctx->params.comb_osd_vld, 1, sizeof(ctx->params.comb_osd_vld));
    ctx->params.comb_t_thr = 4;
    ctx->params.comb_feature_thr = 16;
    ctx->params.comb_cnt_thr = 0;

    ctx->params.ble_backtoma_num = 1;

    ctx->params.mtn_en = 1;
    memcpy(ctx->params.mtn_tab, iep2_mtn_tab, sizeof(ctx->params.mtn_tab));

    ctx->params.roi_en = 0;
    ctx->params.roi_layer_num = 0;

    ctx->params.dil_mode = IEP2_DIL_MODE_I1O1T;
    ctx->params.src_fmt = IEP2_FMT_YUV420;
    ctx->params.src_yuv_swap = IEP2_YUV_SWAP_SP_UV;
    ctx->params.dst_fmt = IEP2_FMT_YUV420;
    ctx->params.dst_yuv_swap = IEP2_YUV_SWAP_SP_UV;
    ctx->params.src_y_stride = 720 / 4;
    ctx->params.src_uv_stride = 720 / 4;
    ctx->params.dst_y_stride = 720 / 4;
    ctx->params.tile_cols = 720 / 16;
    ctx->params.tile_rows = 480 / 4;

    memset(&ctx->ff_inf, 0, sizeof(ctx->ff_inf));

    memset(&ctx->pd_inf, 0, sizeof(ctx->pd_inf));
    ctx->pd_inf.pdtype = PD_TYPES_UNKNOWN;
    ctx->pd_inf.step = -1;

    ret = mpp_buffer_group_get_internal(&ctx->memGroup, MPP_BUFFER_TYPE_DRM);
    if (MPP_OK != ret) {
        close(ctx->fd);
        mpp_err("memGroup mpp_buffer_group_get failed\n");
        return ret;
    }

    ret = mpp_buffer_get(ctx->memGroup, &ctx->mv_buf,
                         IEP2_TILE_W_MAX * IEP2_TILE_H_MAX);
    if (ret) {
        close(ctx->fd);
        mpp_buffer_group_put(ctx->memGroup);
        mpp_err_f("allocate mv buffer failed\n");
        return MPP_NOK;
    }

    ret = mpp_buffer_get(ctx->memGroup, &ctx->md_buf, 1920 * 1088);
    if (ret) {
        close(ctx->fd);
        mpp_buffer_group_put(ctx->memGroup);
        mpp_buffer_put(ctx->mv_buf);
        mpp_err_f("allocate md buffer failed\n");
        return MPP_NOK;
    }

    ctx->params.mv_addr = mpp_buffer_get_fd(ctx->mv_buf);
    ctx->params.md_addr = mpp_buffer_get_fd(ctx->md_buf);

    return MPP_OK;
}

static MPP_RET iep2_deinit(IepCtx ictx)
{
    struct iep2_api_ctx *ctx = ictx;

    close(ctx->fd);

    mpp_buffer_put(ctx->mv_buf);
    mpp_buffer_put(ctx->md_buf);

    if (ctx->memGroup) {
        mpp_buffer_group_put(ctx->memGroup);
        ctx->memGroup = NULL;
    }

    return MPP_OK;
}

static MPP_RET iep2_done(struct iep2_api_ctx *ctx)
{
    iep_dbg_trace("deinterlace detect osd cnt %d, combo %d\n",
                  ctx->output.dect_osd_cnt,
                  ctx->output.out_osd_comb_cnt);

    if (ctx->params.dil_mode == IEP2_DIL_MODE_I5O2 ||
        ctx->params.dil_mode == IEP2_DIL_MODE_I5O1T ||
        ctx->params.dil_mode == IEP2_DIL_MODE_I5O1B) {
        struct mv_list ls;

#if IEP2_OSD_EN
        iep2_set_osd(ctx, &ls);
#else
        memset(&ls, 0, sizeof(struct mv_list));
#endif
        iep2_update_gmv(ctx, &ls);
        iep2_check_ffo(ctx);
        iep2_check_pd(ctx);
        get_param_from_env(ctx);
#if 0
        if (ctx->params.roi_en && ctx->params.osd_area_num > 0) {
            struct iep2_rect r;

            ctx->params.roi_layer_num = 0;

            r.x = 0;
            r.y = 0;
            r.w = ctx->params.tile_cols;
            r.h = ctx->params.tile_rows;
            iep2_set_roi(ctx, &r, ROI_MODE_MA);

            r.x = ctx->params.osd_x_sta[0];
            r.y = ctx->params.osd_y_sta[0];
            r.w = ctx->params.osd_x_end[0] - ctx->params.osd_x_sta[0];
            r.h = ctx->params.osd_y_end[0] - ctx->params.osd_y_sta[0];
            iep2_set_roi(ctx, &r, ROI_MODE_MA_MC);
        }
#endif
    }

    if (ctx->params.dil_mode == IEP2_DIL_MODE_DECT ||
        ctx->params.dil_mode == IEP2_DIL_MODE_PD) {
        iep2_check_ffo(ctx);
        iep2_check_pd(ctx);
        get_param_from_env(ctx);
    }

    if (ctx->pd_inf.pdtype != PD_TYPES_UNKNOWN) {
        ctx->params.dil_mode = IEP2_DIL_MODE_PD;
        ctx->params.pd_mode = iep2_pd_get_output(&ctx->pd_inf);
    } else {
        // TODO, revise others mode replace by I5O2
        //ctx->params.dil_mode = IEP2_DIL_MODE_I5O2;
    }

    return 0;
}

static void iep2_set_param(struct iep2_api_ctx *ctx,
                           union iep2_api_content *param,
                           enum IEP2_PARAM_TYPE type)
{
    switch (type) {
    case IEP2_PARAM_TYPE_COM:
            ctx->params.src_fmt = param->com.sfmt;
        ctx->params.src_yuv_swap = param->com.sswap;
        ctx->params.dst_fmt = param->com.dfmt;
        ctx->params.dst_yuv_swap = param->com.dswap;
        ctx->params.src_y_stride = param->com.hor_stride;
        ctx->params.src_y_stride /= 4;
        ctx->params.src_uv_stride =
            param->com.sswap == IEP2_YUV_SWAP_P ?
            (param->com.hor_stride / 2 + 15) / 16 * 16 : param->com.hor_stride;
        ctx->params.src_uv_stride /= 4;
        ctx->params.dst_y_stride = param->com.hor_stride;
        ctx->params.dst_y_stride /= 4;
        ctx->params.tile_cols = (param->com.width + 15) / 16;
        ctx->params.tile_rows = (param->com.height + 3) / 4;
        iep_dbg_trace("set tile size (%d, %d)\n", param->com.width, param->com.height);
        ctx->params.osd_pec_thr = (param->com.width * 26) >> 7;
        break;
    case IEP2_PARAM_TYPE_MODE:
        ctx->params.dil_mode = param->mode.dil_mode;
        ctx->params.dil_out_mode = param->mode.out_mode;
        if (!ctx->ff_inf.fo_detected) {
            ctx->params.dil_field_order = param->mode.dil_order;
        }

        iep_dbg_trace("deinterlace, mode %d, out mode %d, fo_detected %d, dil_order %d\n",
                      param->mode.dil_mode, param->mode.out_mode, ctx->ff_inf.fo_detected, param->mode.dil_order);

        if (param->mode.dil_order == IEP2_FIELD_ORDER_UND) {
            ctx->ff_inf.frm_offset = 6;
            ctx->ff_inf.fie_offset = 0;
        } else {
            ctx->ff_inf.frm_offset = 0;
            ctx->ff_inf.fie_offset = 10;
        }

        if (param->mode.dil_order == IEP2_FIELD_ORDER_TFF) {
            ctx->ff_inf.tff_offset = 3;
            ctx->ff_inf.bff_offset = 0;
        } else {
            ctx->ff_inf.tff_offset = 0;
            ctx->ff_inf.bff_offset = 3;
        }
        break;
    case IEP2_PARAM_TYPE_MD:
        ctx->params.md_theta = param->md.md_theta;
        ctx->params.md_r = param->md.md_r;
        ctx->params.md_lambda = param->md.md_lambda;
        break;
    case IEP2_PARAM_TYPE_DECT:
    case IEP2_PARAM_TYPE_OSD:
    case IEP2_PARAM_TYPE_ME:
    case IEP2_PARAM_TYPE_EEDI:
    case IEP2_PARAM_TYPE_BLE:
    case IEP2_PARAM_TYPE_COMB:
        break;
    case IEP2_PARAM_TYPE_ROI:
        ctx->params.roi_en = param->roi.roi_en;
        break;
    default:
        ;
    }
}

static MPP_RET iep2_param_check(struct iep2_api_ctx *ctx)
{
    if (ctx->params.tile_cols <= 0 || ctx->params.tile_cols > IEP2_TILE_W_MAX ||
        ctx->params.tile_rows <= 0 || ctx->params.tile_rows > IEP2_TILE_H_MAX) {
        mpp_err("invalidate size (%u, %u)\n",
                ctx->params.tile_cols, ctx->params.tile_rows);
        return MPP_NOK;
    }

    return MPP_OK;
}

static MPP_RET iep2_start(struct iep2_api_ctx *ctx)
{
    MPP_RET ret;
    MppReqV1 mpp_req[2];

    mpp_assert(ctx);

    mpp_req[0].cmd = MPP_CMD_SET_REG_WRITE;
    mpp_req[0].flag = MPP_FLAGS_MULTI_MSG;
    mpp_req[0].size =  sizeof(ctx->params);
    mpp_req[0].offset = 0;
    mpp_req[0].data_ptr = REQ_DATA_PTR(&ctx->params);

    mpp_req[1].cmd = MPP_CMD_SET_REG_READ;
    mpp_req[1].flag = MPP_FLAGS_MULTI_MSG | MPP_FLAGS_LAST_MSG;
    mpp_req[1].size =  sizeof(ctx->output);
    mpp_req[1].offset = 0;
    mpp_req[1].data_ptr = REQ_DATA_PTR(&ctx->output);

    iep_dbg_func("in\n");

    ret = (RK_S32)ioctl(ctx->fd, MPP_IOC_CFG_V1, &mpp_req[0]);

    if (ret) {
        mpp_err_f("ioctl SET_REG failed ret %d errno %d %s\n",
                  ret, errno, strerror(errno));
        ret = errno;
    }

    return ret;
}

static MPP_RET iep2_wait(struct iep2_api_ctx *ctx)
{
    MppReqV1 mpp_req;
    MPP_RET ret;

    memset(&mpp_req, 0, sizeof(mpp_req));
    mpp_req.cmd = MPP_CMD_POLL_HW_FINISH;
    mpp_req.flag |= MPP_FLAGS_LAST_MSG;

    ret = (RK_S32)ioctl(ctx->fd, MPP_IOC_CFG_V1, &mpp_req);

    return ret;
}

static inline void set_addr(struct iep2_addr *addr, IepImg *img)
{
    addr->y = img->mem_addr;
    addr->cbcr = img->uv_addr;
    addr->cr = img->v_addr;
}

static MPP_RET iep2_control(IepCtx ictx, IepCmd cmd, void *iparam)
{
    struct iep2_api_ctx *ctx = ictx;

    switch (cmd) {
    case IEP_CMD_SET_DEI_CFG: {
        struct iep2_api_params *param = (struct iep2_api_params *)iparam;

        iep2_set_param(ctx, &param->param, param->ptype);
    }
    break;
    case IEP_CMD_SET_SRC:
        set_addr(&ctx->params.src[0], (IepImg *)iparam);
        break;
    case IEP_CMD_SET_DEI_SRC1:
        set_addr(&ctx->params.src[1], (IepImg *)iparam);
        break;
    case IEP_CMD_SET_DEI_SRC2:
        set_addr(&ctx->params.src[2], (IepImg *)iparam);
        break;
    case IEP_CMD_SET_DST:
        set_addr(&ctx->params.dst[0], (IepImg *)iparam);
        break;
    case IEP_CMD_SET_DEI_DST1:
        set_addr(&ctx->params.dst[1], (IepImg *)iparam);
        break;
    case IEP_CMD_RUN_SYNC: {
        struct iep2_api_info *inf = (struct iep2_api_info*)iparam;

        if (0 > iep2_param_check(ctx))
            break;
        if (0 > iep2_start(ctx))
            return MPP_NOK;
        iep2_wait(ctx);

        if (ctx->params.dil_mode == IEP2_DIL_MODE_PD) {
            ctx->params.dil_mode = IEP2_DIL_MODE_DECT;
            if (0 > iep2_start(ctx))
                return MPP_NOK;
            iep2_wait(ctx);
        }

        // store current pd mode;
        if (inf)
            inf->pd_flag = ctx->params.pd_mode;
        iep2_done(ctx);
        if (inf) {
            inf->dil_order = ctx->params.dil_field_order;
            inf->frm_mode = ctx->ff_inf.is_frm;
            inf->pd_types = ctx->pd_inf.pdtype;
            inf->dil_order_confidence_ratio = ctx->ff_inf.fo_ratio_avg;
        }
    }
    break;
    default:
        ;
    }

    return MPP_OK;
}

static iep_com_ops iep2_ops = {
    .init = iep2_init,
    .deinit = iep2_deinit,
    .control = iep2_control,
    .release = NULL,
};

iep_com_ctx* rockchip_iep2_api_alloc_ctx(void)
{
    iep_com_ctx *com_ctx = calloc(sizeof(*com_ctx), 1);
    struct iep2_api_ctx *iep2_ctx = calloc(sizeof(*iep2_ctx), 1);

    mpp_assert(com_ctx && iep2_ctx);

    com_ctx->ops = &iep2_ops;
    com_ctx->priv = iep2_ctx;

    return com_ctx;
}

void rockchip_iep2_api_release_ctx(iep_com_ctx *com_ctx)
{
    if (com_ctx->priv) {
        free(com_ctx->priv);
        com_ctx->priv = NULL;
    }

    free(com_ctx);
}


/*
* rk_aiq_accm_algo_v1.cpp

* for rockchip v2.0.0
*
*  Copyright (c) 2019 Rockchip Corporation
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
*
*/
/* for rockchip v2.0.0*/

#include "accm/rk_aiq_accm_algo_com.h"
#include "xcam_log.h"
#include "interpolation.h"

RKAIQ_BEGIN_DECLARE

void CCMV1PrintReg(const rk_aiq_ccm_cfg_t* hw_param) {
    LOG1_ACCM(
        " CCM V1 reg values: "
        " sw_ccm_highy_adjust_dis 0"
        " sw_ccm_en_i %d"
        " sw_ccm_coeff ([%f,%f,%f,%f,%f,%f,%f,%f,%f]-E)X128"
        " sw_ccm_offset [%f,%f,%f]"
        " sw_ccm_coeff_y [%f,%f,%f]"
        " sw_ccm_alp_y [%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f]"
        " sw_ccm_bound_bit %f",
        hw_param->ccmEnable,
        hw_param->matrix[0], hw_param->matrix[1], hw_param->matrix[2],
        hw_param->matrix[3], hw_param->matrix[4], hw_param->matrix[5],
        hw_param->matrix[6], hw_param->matrix[7], hw_param->matrix[8],
        hw_param->offs[0],   hw_param->offs[1],   hw_param->offs[2],
        hw_param->rgb2y_para[0], hw_param->rgb2y_para[1], hw_param->rgb2y_para[2],
        hw_param->alp_y[0], hw_param->alp_y[1], hw_param->alp_y[2], hw_param->alp_y[3],
        hw_param->alp_y[4], hw_param->alp_y[5], hw_param->alp_y[6], hw_param->alp_y[7],
        hw_param->alp_y[8], hw_param->alp_y[9], hw_param->alp_y[10],hw_param->alp_y[11],
        hw_param->alp_y[12],hw_param->alp_y[13],hw_param->alp_y[14],hw_param->alp_y[15],
        hw_param->alp_y[16], hw_param->bound_bit);
}

void CCMV1PrintDBG(const accm_context_t* accm_context) {
    const float *pMatrixUndamped = accm_context->accmRest.undampedCcmMatrix;
    const float *pOffsetUndamped = accm_context->accmRest.undampedCcOffset;
    const float *pMatrixDamped = accm_context->ccmHwConf.matrix;
    const float *pOffsetDamped = accm_context->ccmHwConf.offs;

    LOG1_ACCM("Illu Probability Estimation Enable: %d"
                "color_inhibition sensorGain: %f,%f,%f,%f "
                "color_inhibition level: %f,%f,%f,%f"
                "color_saturation sensorGain: %f,%f,%f,%f "
                "color_saturation level: %f,%f,%f,%f"
                "dampfactor: %f"
                " undampedCcmMatrix: %f,%f,%f,%f,%f,%f,%f,%f,%f"
                " undampedCcOffset: %f,%f,%f "
                " dampedCcmMatrix: %f,%f,%f,%f,%f,%f,%f,%f,%f"
                " dampedCcOffset:%f,%f,%f",
                accm_context->stIlluestCfg.interp_enable,
                accm_context->mCurAtt.stAuto.color_inhibition.sensorGain[0],
                accm_context->mCurAtt.stAuto.color_inhibition.sensorGain[1],
                accm_context->mCurAtt.stAuto.color_inhibition.sensorGain[2],
                accm_context->mCurAtt.stAuto.color_inhibition.sensorGain[3],
                accm_context->mCurAtt.stAuto.color_inhibition.level[0],
                accm_context->mCurAtt.stAuto.color_inhibition.level[1],
                accm_context->mCurAtt.stAuto.color_inhibition.level[2],
                accm_context->mCurAtt.stAuto.color_inhibition.level[3],
                accm_context->mCurAtt.stAuto.color_saturation.sensorGain[0],
                accm_context->mCurAtt.stAuto.color_saturation.sensorGain[1],
                accm_context->mCurAtt.stAuto.color_saturation.sensorGain[2],
                accm_context->mCurAtt.stAuto.color_saturation.sensorGain[3],
                accm_context->mCurAtt.stAuto.color_saturation.level[0],
                accm_context->mCurAtt.stAuto.color_saturation.level[1],
                accm_context->mCurAtt.stAuto.color_saturation.level[2],
                accm_context->mCurAtt.stAuto.color_saturation.level[3],
                accm_context->stCalib_v1.damp_enable,
                pMatrixUndamped[0], pMatrixUndamped[1], pMatrixUndamped[2],
                pMatrixUndamped[3], pMatrixUndamped[4], pMatrixUndamped[5],
                pMatrixUndamped[6], pMatrixUndamped[7], pMatrixUndamped[8],
                pOffsetUndamped[0], pOffsetUndamped[1], pOffsetUndamped[2],
                pMatrixDamped[0],   pMatrixDamped[1],   pMatrixDamped[2],
                pMatrixDamped[3],   pMatrixDamped[4],   pMatrixDamped[5],
                pMatrixDamped[6],   pMatrixDamped[7],   pMatrixDamped[8],
                pOffsetDamped[0],   pOffsetDamped[1],   pOffsetDamped[2]);

}

XCamReturn AccmAutoConfig
(
    accm_handle_t hAccm
) {

    LOG1_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (hAccm == NULL) {
        return XCAM_RETURN_ERROR_PARAM;
    }

    const rk_aiq_ccm_iqparam_attrib_t* pCcm = NULL;
    float sensorGain                     = hAccm->accmSwInfo.sensorGain;
    float fSaturation                    = 0;
    bool  updateMat                      = false;
    bool  updateYAlp                     = false;
    bool  updUndampMat                   = false;
    pCcm = &hAccm->stCalib_v1;
    if (hAccm->update) {
        if (hAccm->stIlluestCfg.interp_enable) {
            hAccm->isReCal_ = true;
            ret = interpCCMbywbgain(&hAccm->stIlluestCfg, pCcm->aCcmCof,
                                    pCcm->aCcmCof_len, hAccm, fSaturation);
            RETURN_RESULT_IF_DIFFERENT(ret, XCAM_RETURN_NO_ERROR);
        } else {
            ret = selectCCM(pCcm->aCcmCof, pCcm->aCcmCof_len, hAccm, fSaturation, &updUndampMat);
            RETURN_RESULT_IF_DIFFERENT(ret, XCAM_RETURN_NO_ERROR);
        }
    }
    if (hAccm->update || hAccm->updateAtt) {

        //4) calc scale for y_alpha_curve
        float fScale = 1.0;
    #if 1
        //real use
        interpolation(pCcm->lumaCCM.gain_alphaScale_curve.gain,
                      pCcm->lumaCCM.gain_alphaScale_curve.scale,
                      9,
                      sensorGain, &fScale);
    #else
        //for test, to be same with demo
        for( int i = 0; i < 9; i++)
        {
            int j = uint16_t(sensorGain);
            j = (j > (1 << 8)) ? (1 << 8) : j;

            if(j <= (1 << i))
            {
                fScale = pCcm->lumaCCM.gain_alphaScale_curve.scale[i];
                break;
            }
        }
    #endif
    // 5) color inhibition adjust for api
        interpolation(hAccm->mCurAtt.stAuto.color_inhibition.sensorGain,
                      hAccm->mCurAtt.stAuto.color_inhibition.level,
                      RK_AIQ_ACCM_COLOR_GAIN_NUM,
                      sensorGain, &hAccm->accmRest.color_inhibition_level);

        if(hAccm->accmRest.color_inhibition_level > 100 || hAccm->accmRest.color_inhibition_level < 0) {
            LOGE_ACCM("flevel2: %f is out of range [0 100]\n",  hAccm->accmRest.color_inhibition_level);
            return XCAM_RETURN_ERROR_PARAM;
        }

        fScale *= (100 - hAccm->accmRest.color_inhibition_level) / 100;

    // 6)   saturation adjust for api
        float saturation_level = 100;
        interpolation(hAccm->mCurAtt.stAuto.color_saturation.sensorGain,
                      hAccm->mCurAtt.stAuto.color_saturation.level,
                      RK_AIQ_ACCM_COLOR_GAIN_NUM,
                      sensorGain, &saturation_level );

        if(saturation_level  > 100 || saturation_level  < 0) {
            LOGE_ACCM("flevel1: %f is out of range [0 100]\n",  saturation_level);
            return XCAM_RETURN_ERROR_PARAM;
        }

        LOGD_ACCM("CcmProfile changed: %d, fScale: %f->%f, sat_level: %f->%f",
                  updUndampMat, hAccm->accmRest.fScale, fScale,
                  hAccm->accmRest.color_saturation_level, saturation_level);

        bool flag = updUndampMat ||
                    fabs(fScale - hAccm->accmRest.fScale) > DIVMIN ||
                    fabs(saturation_level - hAccm->accmRest.color_saturation_level) > DIVMIN;
        updUndampMat = false;
        if (flag || (!hAccm->invarMode)) {
            if (flag) {
                hAccm->accmRest.fScale = fScale;
                hAccm->accmRest.color_saturation_level = saturation_level;
                Saturationadjust(fScale, saturation_level, hAccm->accmRest.undampedCcmMatrix);
                LOGD_ACCM("Adjust ccm by sat: %d, undampedCcmMatrix[0]: %f", hAccm->isReCal_, hAccm->accmRest.undampedCcmMatrix[0]);
            }

            if (!hAccm->invarMode) {
                hAccm->ccmHwConf.bound_bit = pCcm->lumaCCM.low_bound_pos_bit;
                memcpy(hAccm->ccmHwConf.rgb2y_para, pCcm->lumaCCM.rgb2y_para,
                       sizeof(pCcm->lumaCCM.rgb2y_para));
            }

            for(int i = 0; i < CCM_CURVE_DOT_NUM; i++) { //set to ic  to do bit check
                hAccm->ccmHwConf.alp_y[i] = fScale * pCcm->lumaCCM.y_alpha_curve[i];
            }
            updUndampMat = true;
            updateYAlp   = true;
        }
    }
    // 7) . Damping
    float dampCoef = (pCcm->damp_enable && hAccm->count > 1 && hAccm->invarMode > 0) ? hAccm->accmSwInfo.awbIIRDampCoef : 0;
    if (!hAccm->accmSwInfo.ccmConverged || updUndampMat) {
        ret = Damping(dampCoef,
                    hAccm->accmRest.undampedCcmMatrix, hAccm->ccmHwConf.matrix,
                    hAccm->accmRest.undampedCcOffset, hAccm->ccmHwConf.offs,
                    &hAccm->accmSwInfo.ccmConverged);
        updateMat = true;
        LOGD_ACCM("damping: %f, ccm coef[0]: %f->%f, ccm coef[8]: %f->%f \n",
            dampCoef, hAccm->accmRest.undampedCcmMatrix[0], hAccm->ccmHwConf.matrix[0],
            hAccm->accmRest.undampedCcmMatrix[8], hAccm->ccmHwConf.matrix[8]);
    }

    hAccm->isReCal_ = hAccm->isReCal_ || updateMat || updateYAlp;

    LOGD_ACCM("final isReCal_ = %d \n", hAccm->isReCal_);
    LOG1_ACCM("%s: (exit)\n", __FUNCTION__);

    return (ret);
}

XCamReturn AccmManualConfig
(
    accm_handle_t hAccm
) {
    LOG1_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    memcpy(hAccm->ccmHwConf.matrix, hAccm->mCurAtt.stManual.ccMatrix, sizeof(hAccm->mCurAtt.stManual.ccMatrix));
    memcpy(hAccm->ccmHwConf.offs, hAccm->mCurAtt.stManual.ccOffsets, sizeof(hAccm->mCurAtt.stManual.ccOffsets));
    memcpy(hAccm->ccmHwConf.alp_y, hAccm->mCurAtt.stManual.y_alpha_curve, sizeof(hAccm->mCurAtt.stManual.y_alpha_curve));
    hAccm->ccmHwConf.bound_bit = hAccm->mCurAtt.stManual.low_bound_pos_bit;
    LOG1_ACCM("%s: (exit)\n", __FUNCTION__);
    return ret;

}

XCamReturn AccmConfig
(
    accm_handle_t hAccm
) {
    LOG1_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD_ACCM("%s: byPass: %d  mode:%d updateAtt: %d \n", __FUNCTION__,
            hAccm->mCurAtt.byPass, hAccm->mCurAtt.mode, hAccm->updateAtt);
    if (hAccm->mCurAtt.byPass != true && hAccm->accmSwInfo.grayMode != true) {
        hAccm->ccmHwConf.ccmEnable = true;
        if (hAccm->mCurAtt.mode == RK_AIQ_CCM_MODE_AUTO) {
            hAccm->update = JudgeCcmRes3aConverge(&hAccm->accmRest.res3a_info, &hAccm->accmSwInfo,
                                          hAccm->stCalib_v1.control.gain_tolerance,
                                          hAccm->stCalib_v1.control.wbgain_tolerance);
            hAccm->update = hAccm->update || hAccm->calib_update;
            LOGD_ACCM("%s: CCM update (gain/awbgain/calib): %d, CCM Converged: %d\n",
                    __FUNCTION__, hAccm->update, hAccm->accmSwInfo.ccmConverged);
            if (hAccm->updateAtt || hAccm->update ||(!hAccm->accmSwInfo.ccmConverged)) {
                AccmAutoConfig(hAccm);
                CCMV1PrintDBG(hAccm);
            }
        } else if (hAccm->mCurAtt.mode == RK_AIQ_CCM_MODE_MANUAL) {
            if (hAccm->updateAtt) {
                AccmManualConfig(hAccm);
                hAccm->isReCal_ = true;
            }
        } else {
            LOGE_ACCM("%s: hAccm->mCurAtt.mode(%d) is invalid \n", __FUNCTION__, hAccm->mCurAtt.mode);
        }
    } else {
        hAccm->ccmHwConf.ccmEnable = false;
        // change to graymode / bypass by api/calib
        hAccm->isReCal_ = hAccm->isReCal_ || hAccm->updateAtt || hAccm->calib_update;

    }
    hAccm->updateAtt = false;
    hAccm->calib_update = false;
    hAccm->count = ((hAccm->count + 2) > (65536)) ? 2 : (hAccm->count + 1);

    CCMV1PrintReg(&hAccm->ccmHwConf);

    LOG1_ACCM("%s: (exit)\n", __FUNCTION__);
    return ret;

}

XCamReturn ConfigbyCalib(accm_handle_t hAccm) {
    LOG1_ACCM("%s: (enter)  \n", __FUNCTION__);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    ret = pCcmMatrixAll_init(hAccm->stCalib_v1.aCcmCof,
                             hAccm->stCalib_v1.aCcmCof_len,
                             hAccm->stCalib_v1.matrixAll,
                             hAccm->stCalib_v1.matrixAll_len,
                             hAccm->pCcmMatrixAll);

    if (hAccm->mCurAtt.mode == RK_AIQ_CCM_MODE_AUTO) {

        hAccm->mCurAtt.byPass = !(hAccm->stCalib_v1.control.enable);

        hAccm->ccmHwConf.bound_bit = hAccm->stCalib_v1.lumaCCM.low_bound_pos_bit;
        memcpy(hAccm->ccmHwConf.rgb2y_para, hAccm->stCalib_v1.lumaCCM.rgb2y_para,
                sizeof(hAccm->stCalib_v1.lumaCCM.rgb2y_para));
        memcpy(hAccm->ccmHwConf.alp_y, hAccm->stCalib_v1.lumaCCM.y_alpha_curve, sizeof(hAccm->ccmHwConf.alp_y));

        hAccm->accmSwInfo.ccmConverged = false;
        hAccm->calib_update = true;
    }

    clear_list(&hAccm->accmRest.problist);

    LOG1_ACCM("%s: (exit)\n", __FUNCTION__);
    return (ret);
}

/**********************************
*Update CCM CalibV2 Para
*      Prepare init
*      Mode change: reinit
*      Res change: continue
*      Calib change: continue
***************************************/
static XCamReturn UpdateCcmCalibV2ParaV1(accm_handle_t hAccm)
{
    LOG1_ACCM("%s: (enter)  \n", __FUNCTION__);
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    bool config_calib = !!(hAccm->accmSwInfo.prepare_type & RK_AIQ_ALGO_CONFTYPE_UPDATECALIB);
    if (!config_calib)
    {
        return(ret);
    }
    const CalibDbV2_Ccm_Para_V2_t* calib_ccm = hAccm->ccm_v1;

#if RKAIQ_ACCM_ILLU_VOTE
    if (hAccm->stCalib_v1.aCcmCof_len != calib_ccm->TuningPara.aCcmCof_len)
        clear_list(&hAccm->accmRest.dominateIlluList);
#endif

    ReloadCCMCalibV2(calib_ccm, &hAccm->stCalib_v1, &hAccm->stIlluestCfg);

    ret = ConfigbyCalib(hAccm);

    LOG1_ACCM("%s: (exit)\n", __FUNCTION__);
    return(ret);
}

static XCamReturn ApiAttrInit(const rk_aiq_ccm_cfg_t*            ccmHwConf,
                              const rk_aiq_ccm_iqparam_attrib_t* stCalib,
                              rk_aiq_ccm_attrib_t*               mCurAtt)
{
    LOGI_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret               = XCAM_RETURN_NO_ERROR;
    mCurAtt->byPass = !(stCalib->control.enable);
    // StAuto
    for (int i = 0; i < RK_AIQ_ACCM_COLOR_GAIN_NUM; i++) {
        mCurAtt->stAuto.color_inhibition.sensorGain[i] = 1;
        mCurAtt->stAuto.color_inhibition.level[i]      = 0;
        mCurAtt->stAuto.color_saturation.sensorGain[i] = 1;
        mCurAtt->stAuto.color_saturation.level[i]      = 50;
    }
    // StManual
    if (stCalib->matrixAll_len > 0) {
        memcpy(mCurAtt->stManual.ccMatrix, stCalib->matrixAll[0].ccMatrix,
               sizeof(stCalib->matrixAll[0].ccMatrix));
        memcpy(mCurAtt->stManual.ccOffsets, stCalib->matrixAll[0].ccOffsets,
               sizeof(stCalib->matrixAll[0].ccOffsets));
    } else {
        memset(mCurAtt->stManual.ccMatrix, 0, sizeof(mCurAtt->stManual.ccMatrix));
        memset(mCurAtt->stManual.ccOffsets, 0, sizeof(mCurAtt->stManual.ccOffsets));
        mCurAtt->stManual.ccMatrix[0] = 1.0;
        mCurAtt->stManual.ccMatrix[4] = 1.0;
        mCurAtt->stManual.ccMatrix[8] = 1.0;
    }

    memcpy(mCurAtt->stManual.y_alpha_curve, ccmHwConf->alp_y,
           sizeof(ccmHwConf->alp_y));
    mCurAtt->stManual.low_bound_pos_bit = ccmHwConf->bound_bit;

    LOGI_ACCM("%s: (exit)\n", __FUNCTION__);
    return (ret);
}

XCamReturn AccmInit(accm_handle_t *hAccm, const CamCalibDbV2Context_t* calibv2)
{
    LOGI_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if(calibv2 == NULL) {
        return  XCAM_RETURN_ERROR_PARAM;
    }

    const CalibDbV2_Ccm_Para_V2_t *calib_ccm =
        (CalibDbV2_Ccm_Para_V2_t*)(CALIBDBV2_GET_MODULE_PTR((void*)calibv2, ccm_calib));
    if (calib_ccm == NULL)
        return XCAM_RETURN_ERROR_MEM;

    *hAccm = (accm_context_t*)malloc(sizeof(accm_context_t));
    accm_context_t* accm_context = *hAccm;
    memset(accm_context, 0, sizeof(accm_context_t));

    accm_context->accmSwInfo.sensorGain = 1.0;
    accm_context->accmSwInfo.awbIIRDampCoef = 0;
    accm_context->accmSwInfo.varianceLuma = 255;
    accm_context->accmSwInfo.awbConverged = false;

    accm_context->accmSwInfo.awbGain[0] = 1;
    accm_context->accmSwInfo.awbGain[1] = 1;

    accm_context->accmRest.res3a_info.sensorGain = 1.0;
    accm_context->accmRest.res3a_info.awbGain[0] = 1.0;
    accm_context->accmRest.res3a_info.awbGain[1] = 1.0;

    accm_context->count = 0;
    accm_context->isReCal_ = 1;
    accm_context->invarMode = 1;

    accm_context->accmSwInfo.prepare_type = RK_AIQ_ALGO_CONFTYPE_UPDATECALIB | RK_AIQ_ALGO_CONFTYPE_NEEDRESET;

    // todo whm --- CalibDbV2_Ccm_Para
    accm_context->ccm_v1 = calib_ccm;
    accm_context->mCurAtt.mode = RK_AIQ_CCM_MODE_AUTO;
#if RKAIQ_ACCM_ILLU_VOTE
    INIT_LIST_HEAD(&accm_context->accmRest.dominateIlluList);
#endif
    INIT_LIST_HEAD(&accm_context->accmRest.problist);
    ret = UpdateCcmCalibV2ParaV1(accm_context);
    ApiAttrInit(&accm_context->ccmHwConf, &accm_context->stCalib_v1,
                  &accm_context->mCurAtt);

    accm_context->accmRest.fScale = 1;
    accm_context->accmRest.color_inhibition_level = 0;
    accm_context->accmRest.color_saturation_level = 100;

    LOGI_ACCM("%s: (exit)\n", __FUNCTION__);
    return(ret);
}

XCamReturn AccmRelease(accm_handle_t hAccm)
{
    LOGI_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
#if RKAIQ_ACCM_ILLU_VOTE
    clear_list(&hAccm->accmRest.dominateIlluList);
#endif
    clear_list(&hAccm->accmRest.problist);
    free(hAccm->stIlluestCfg.default_illu);
    free(hAccm);
    hAccm = NULL;

    LOGI_ACCM("%s: (exit)\n", __FUNCTION__);
    return(ret);

}

// todo whm
XCamReturn AccmPrepare(accm_handle_t hAccm)
{
    LOGI_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    ret = UpdateCcmCalibV2ParaV1(hAccm);
    RETURN_RESULT_IF_DIFFERENT(ret, XCAM_RETURN_NO_ERROR);

    LOGI_ACCM("%s: (exit)\n", __FUNCTION__);
    return ret;
}


XCamReturn AccmPreProc(accm_handle_t hAccm)
{

    LOG1_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    LOG1_ACCM("%s: (exit)\n", __FUNCTION__);
    return(ret);

}
XCamReturn AccmProcessing(accm_handle_t hAccm)
{
    LOG1_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    LOG1_ACCM("%s: (exit)\n", __FUNCTION__);
    return(ret);
}




RKAIQ_END_DECLARE



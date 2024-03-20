#include "asharpV33/rk_aiq_uapi_camgroup_asharp_int_v33.h"
#include "rk_aiq_types_camgroup_asharp_prvt_v33.h"


#define CAMGROUP_ASHSRPV33_STRENGTH_SLOPE_FACTOR (4.0)

XCamReturn rk_aiq_uapi_camgroup_asharpV33_SetAttrib(RkAiqAlgoContext* ctx,
                                                    const rk_aiq_sharp_attrib_v33_t* attr,
                                                    bool /* need_sync */) {
    LOGD_ASHARP("%s:%d\n", __FUNCTION__, __LINE__);
#if RKAIQ_HAVE_SHARP_V33

    CamGroup_AsharpV33_Contex_t *pGroupAsharpCtx = (CamGroup_AsharpV33_Contex_t *)ctx;
    Asharp_Context_V33_t* pAsharpCtx = pGroupAsharpCtx->asharp_contex_v33;

    pAsharpCtx->eMode = attr->eMode;
    if (pAsharpCtx->eMode == ASHARP_V33_OP_MODE_AUTO) {
        pAsharpCtx->stAuto = attr->stAuto;
    } else if (pAsharpCtx->eMode == ASHARP_V33_OP_MODE_MANUAL) {
        pAsharpCtx->stManual.stSelect = attr->stManual.stSelect;
    } else if (pAsharpCtx->eMode == ASHARP_V33_OP_MODE_REG_MANUAL) {
        pAsharpCtx->stManual.stFix = attr->stManual.stFix;
    }
    pAsharpCtx->isReCalculate |= 1;
#endif
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn rk_aiq_uapi_camgroup_asharpV33_GetAttrib(const RkAiqAlgoContext* ctx,
                                                    rk_aiq_sharp_attrib_v33_t* attr) {
    LOGD_ASHARP("%s:%d\n", __FUNCTION__, __LINE__);

#if RKAIQ_HAVE_SHARP_V33
    CamGroup_AsharpV33_Contex_t* pGroupAsharpCtx = (CamGroup_AsharpV33_Contex_t*)ctx;
    Asharp_Context_V33_t* pAsharpCtx             = pGroupAsharpCtx->asharp_contex_v33;

    attr->eMode = pAsharpCtx->eMode;
    memcpy(&attr->stAuto, &pAsharpCtx->stAuto, sizeof(attr->stAuto));
    memcpy(&attr->stManual, &pAsharpCtx->stManual, sizeof(attr->stManual));
#endif
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn rk_aiq_uapi_camgroup_asharpV33Lite_SetAttrib(RkAiqAlgoContext* ctx,
                                                        const rk_aiq_sharp_attrib_v33LT_t* attr,
                                                        bool /* need_sync */) {
    LOGD_ASHARP("%s:%d\n", __FUNCTION__, __LINE__);
#if RKAIQ_HAVE_SHARP_V33_LITE
    CamGroup_AsharpV33_Contex_t* pGroupAsharpCtx = (CamGroup_AsharpV33_Contex_t*)ctx;
    Asharp_Context_V33_t* pAsharpCtx             = pGroupAsharpCtx->asharp_contex_v33;

    pAsharpCtx->eMode = attr->eMode;
    if (pAsharpCtx->eMode == ASHARP_V33_OP_MODE_AUTO) {
        pAsharpCtx->stAuto = attr->stAuto;
    } else if (pAsharpCtx->eMode == ASHARP_V33_OP_MODE_MANUAL) {
        pAsharpCtx->stManual.stSelect = attr->stManual.stSelect;
    } else if (pAsharpCtx->eMode == ASHARP_V33_OP_MODE_REG_MANUAL) {
        pAsharpCtx->stManual.stFix = attr->stManual.stFix;
    }
    pAsharpCtx->isReCalculate |= 1;
#endif
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn rk_aiq_uapi_camgroup_asharpV33Lite_GetAttrib(const RkAiqAlgoContext* ctx,
                                                        rk_aiq_sharp_attrib_v33LT_t* attr) {
    LOGD_ASHARP("%s:%d\n", __FUNCTION__, __LINE__);

#if RKAIQ_HAVE_SHARP_V33_LITE
    CamGroup_AsharpV33_Contex_t *pGroupAsharpCtx = (CamGroup_AsharpV33_Contex_t *)ctx;
    Asharp_Context_V33_t* pAsharpCtx = pGroupAsharpCtx->asharp_contex_v33;

    attr->eMode = pAsharpCtx->eMode;
    memcpy(&attr->stAuto, &pAsharpCtx->stAuto, sizeof(attr->stAuto));
    memcpy(&attr->stManual, &pAsharpCtx->stManual, sizeof(attr->stManual));
#endif

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_uapi_camgroup_asharpV33_SetStrength(const RkAiqAlgoContext *ctx,
        const rk_aiq_sharp_strength_v33_t* pStrength)
{
    LOGD_ASHARP("%s:%d\n", __FUNCTION__, __LINE__);

    CamGroup_AsharpV33_Contex_t *pGroupAsharpCtx = (CamGroup_AsharpV33_Contex_t *)ctx;
    Asharp_Context_V33_t* pAsharpAsharpCtx = pGroupAsharpCtx->asharp_contex_v33;

    float fslope = CAMGROUP_ASHSRPV33_STRENGTH_SLOPE_FACTOR;
    float fStrength = 1.0f;
    float fPercent = 0.5f;

    fPercent = pStrength->percent;

    if (fPercent <= 0.5) {
        fStrength = fPercent / 0.5;
    } else {
        if (fPercent >= 0.999999) fPercent = 0.999999;
        fStrength = 0.5 * fslope / (1.0 - fPercent) - fslope + 1;
    }

    pAsharpAsharpCtx->stStrength = *pStrength;
    pAsharpAsharpCtx->stStrength.percent = fStrength;
    pAsharpAsharpCtx->isReCalculate |= 1;

    LOGD_ASHARP("%s:%d percent:%f fStrength:%f \n", __FUNCTION__, __LINE__, fStrength, fPercent);

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_uapi_camgroup_asharpV33_GetStrength(const RkAiqAlgoContext *ctx,
        rk_aiq_sharp_strength_v33_t* pStrength)
{
    LOGD_ASHARP("%s:%d\n", __FUNCTION__, __LINE__);

    CamGroup_AsharpV33_Contex_t *pGroupAsharpCtx = (CamGroup_AsharpV33_Contex_t *)ctx;
    Asharp_Context_V33_t* pAsharpAsharpCtx = pGroupAsharpCtx->asharp_contex_v33;

    float fslope = CAMGROUP_ASHSRPV33_STRENGTH_SLOPE_FACTOR;
    float fStrength = 1.0f;
    float fPercent = 0.5f;

    fStrength = pAsharpAsharpCtx->stStrength.percent;

    if (fStrength <= 1) {
        fPercent = fStrength * 0.5;
    } else {
        float tmp = 1.0;
        tmp       = 1 - 0.5 * fslope / (fStrength + fslope - 1);
        if (abs(tmp - 0.999999) < 0.000001) {
            tmp = 1.0;
        }
        fPercent = tmp;
    }

    *pStrength = pAsharpAsharpCtx->stStrength;
    pStrength->percent = fPercent;

    LOGD_ASHARP("%s:%d fStrength:%f percent:%f\n", __FUNCTION__, __LINE__, fStrength, fPercent);

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn rk_aiq_uapi_camgroup_asharpV33_GetInfo(const RkAiqAlgoContext* ctx,
        rk_aiq_sharp_info_v33_t* pInfo) {

    CamGroup_AsharpV33_Contex_t *pGroupAsharpCtx = (CamGroup_AsharpV33_Contex_t *)ctx;
    Asharp_Context_V33_t* pAsharpAsharpCtx = pGroupAsharpCtx->asharp_contex_v33;

    pInfo->iso = pAsharpAsharpCtx->stExpInfo.arIso[pAsharpAsharpCtx->stExpInfo.hdr_mode];


    pInfo->expo_info = pAsharpAsharpCtx->stExpInfo;
    return XCAM_RETURN_NO_ERROR;
}


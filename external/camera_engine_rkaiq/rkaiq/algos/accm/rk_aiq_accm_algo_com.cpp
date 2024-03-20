/*
* rk_aiq_accm_algo_com.cpp

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

XCamReturn Swinfo_wbgain_init(float                      awbGain[2],
                              const rk_aiq_ccm_illucfg_t aCcmCof[],
                              int                        aCcmCof_len,
                              const char*                illuName)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOG1_ACCM("%s(%d): (enter)\n", __FUNCTION__, __LINE__);

    if (aCcmCof == NULL) {
        ret = XCAM_RETURN_ERROR_PARAM;
        LOGE_ACCM("%s(%d): invalid input params\n", __FUNCTION__, __LINE__);
        return ret;
    }

    bool lsFound = false;

    for(int i = 0; i < aCcmCof_len; i++) {
        if(strcmp(aCcmCof[i].name, illuName) == 0) {
            awbGain[0] = aCcmCof[i].awbGain[0];
            awbGain[1] = aCcmCof[i].awbGain[1];
            lsFound = true;
            LOGD_ACCM("%s: accm lsForFirstFrame:%s", __FUNCTION__, aCcmCof[i].name);
            break;
        }
    }
    if(aCcmCof_len> 0 && lsFound == false) {
        awbGain[0] = aCcmCof[0].awbGain[0];
        awbGain[1] = aCcmCof[0].awbGain[1];
        LOGD_ACCM("%s: accm lsForFirstFrame:%s", __FUNCTION__, aCcmCof[0].name);
    }
    LOGV_ACCM("%s: accm illunum:%d", __FUNCTION__, aCcmCof_len);
    LOG1_ACCM( "%s(%d): (exit)\n", __FUNCTION__, __LINE__);
    return ret;
}

XCamReturn illuminant_index_estimation_ccm(int light_num, const rk_aiq_ccm_illucfg_t illAll[], float awbGain[2], int* illuminant_index)
{

    LOG1_ACCM( "%s: (enter)\n", __FUNCTION__);
    float minDist = 9999999;
    float dist[8];
    float nRG, nBG;
    nRG = awbGain[0];
    nBG = awbGain[1];
    *illuminant_index = 0;
    XCamReturn ret = XCAM_RETURN_ERROR_FAILED;
    for(int i = 0; i < light_num; i++)
    {
        dist[i] = (nRG - illAll[i].awbGain[0]) * (nRG -  illAll[i].awbGain[0])
                       + (nBG -  illAll[i].awbGain[1]) * (nBG -  illAll[i].awbGain[1]);
        if(dist[i] < minDist)
        {
            minDist = dist[i];
            *illuminant_index = i;
            ret = XCAM_RETURN_NO_ERROR;
        }
    }
    if(ret != XCAM_RETURN_NO_ERROR) {
        LOGE_ACCM("fail to estimate illuminant!!!\n");
    }

    LOGD_ACCM( "wbGain:%f,%f, estimation illuminant  is %s(%d) \n", awbGain[0], awbGain[1],
               illAll[*illuminant_index].name, *illuminant_index);

    LOG1_ACCM( "%s: (exit)\n", __FUNCTION__);
    return ret;
}

XCamReturn illuminant_index_candidate_ccm(int light_num, const rk_aiq_ccm_illucfg_t illAll[], float awbGain[2], char* default_illu, float prob_limit, const float weight_rb[2], float* prob)
{
    LOG1_ACCM( "%s: (enter)\n", __FUNCTION__);
    float dist[8];
    float nRG, nBG;
    nRG = awbGain[0];
    nBG = awbGain[1];
    float wr = weight_rb[0];
    float wb = weight_rb[1];
    XCamReturn ret = XCAM_RETURN_ERROR_FAILED;
    if (light_num == 0) {
        LOGE_ACCM(" Illuminant Profile does not exit  !!!\n");
        return ret;
    }
    LOGD_ACCM("wbGain: %f, %f \n", nRG, nBG);
    memset(prob, 0, light_num*sizeof(float));
    if (light_num == 1) {
        prob[0] =1;
        LOGD_ACCM(" Only one illuminant profile: %s\n", illAll[0].name);
        ret = XCAM_RETURN_NO_ERROR;}
    else {
        float mean_dist = 0;
        int idx = 0;
        float minDist = 9999999;
        int default_illu_idx = 0;
        for(int i = 0; i < light_num; i++) {
            dist[i] = sqrt((nRG - illAll[i].awbGain[0]) * (nRG -  illAll[i].awbGain[0])*wr + (nBG -  illAll[i].awbGain[1]) * (nBG -  illAll[i].awbGain[1])*wb);
            if (strcmp( default_illu, illAll[i].name) == 0)
                default_illu_idx = i;
            if(dist[i] < minDist)
            {
                minDist = dist[i];
                idx = i;
            }
            mean_dist = mean_dist + dist[i];
        }
        if (minDist  < illAll[idx].minDist) {
            prob[idx] = 1;
            LOGD_ACCM(" pick Illu: %s, dist = %f, prob = %f \n", illAll[idx].name, dist[idx],
                      prob[idx]);
            ret = XCAM_RETURN_NO_ERROR;
            LOG1_ACCM( "%s: (exit)\n", __FUNCTION__);
            return ret;
        }
        float sigma = 0;
        mean_dist = mean_dist/light_num;
        for(int i = 0; i < light_num; i++)
            sigma = sigma + (dist[i] - mean_dist)* (dist[i] - mean_dist);
        sigma = sigma/light_num;

        if (fabs(sigma)>DIVMIN) {
            float sum_prob = 0;
            for(int i = 0; i < light_num; i++) {
                prob[i] = exp(-0.5*dist[i]*dist[i]/sigma);
                sum_prob = sum_prob + prob[i];
            }
            // normalize prob
            float new_sum_prob = 0;
            for(int i = 0; i < light_num; i++) {
                float prob_tmp;
                prob_tmp = prob[i]/sum_prob;
                LOGD_ACCM(" Illu Name: %s, dist = %f, ori prob = %f \n", illAll[i].name, dist[i],
                          prob_tmp);
                if (prob_tmp < prob_limit ) {
                    prob[i] = 0;
                }
                new_sum_prob += prob[i];
            }
            if (fabs(new_sum_prob)<DIVMIN) {
                LOGE_ACCM(" prob_limit is too large  !!!\n");
                float max_prob = DIVMIN;
                int maxprob_idx = 0;
                for (int i = 0; i < light_num; i++) {
                    if (prob[i] > max_prob){
                        max_prob = prob[i];
                        maxprob_idx = i;
                    }
                }
                memset(prob, 0, light_num*sizeof(float));
                prob[maxprob_idx] = 1;
                LOGD_ACCM(" pick max prob illu : %s \n", illAll[maxprob_idx].name);
            } else {
                float *prob_test = (float*)malloc(light_num*sizeof(float));
                memset(prob_test, 0, light_num*sizeof(float));
                float fsum_prob = 0;
                for(int i = 0; i < light_num; i++) {
                    prob_test[i] = prob[i]/new_sum_prob;
                    if (prob_test[i]<0.333333)
                        prob_test[i] = 0;
                    fsum_prob += prob_test[i];
                }
                if (fabs(fsum_prob)<DIVMIN) {
                    fsum_prob = new_sum_prob;
                } else {
                    memcpy(prob,  prob_test, light_num*sizeof(float));
                }
                for(int i = 0; i < light_num; i++) {
                    prob[i] = prob[i]/fsum_prob;
                    LOGD_ACCM(" Illu Name: %s, prob = %f \n", illAll[i].name, prob[i]);
                }
                free(prob_test);
                prob_test = NULL;
            }
        }
        else {
            prob[default_illu_idx] = 1;
            LOGD_ACCM(" All dist values are equal, pick Default Illu: %s\n",
                      illAll[default_illu_idx].name);
        }

        ret = XCAM_RETURN_NO_ERROR;
    }
    if(ret != XCAM_RETURN_NO_ERROR)
            LOGE_ACCM(" Fail to get Illuminant Probability !!!\n");
    LOG1_ACCM( "%s: (exit)\n", __FUNCTION__);
    return ret;
}

static XCamReturn AwbOrderCcmProfilesBySaturation
(
    const rk_aiq_ccm_matrixcfg_t* pCcmProfiles[],
    const int32_t   cnt
) {
    int32_t i, j;

    for (i = 0; i < (cnt - 1); ++i) {
        for (j = 0; j < (cnt - i - 1); ++j) {
            if (pCcmProfiles[j]->saturation < pCcmProfiles[j + 1]->saturation) {
                const rk_aiq_ccm_matrixcfg_t* temp   = pCcmProfiles[j];
                pCcmProfiles[j]         = pCcmProfiles[j + 1];
                pCcmProfiles[j + 1]       = temp;
            }
        }
    }

    return (XCAM_RETURN_NO_ERROR);
}

XCamReturn pCcmMatrixAll_init(const rk_aiq_ccm_illucfg_t*   aCcmCof,
                              int                           aCcmCof_len,
                              const rk_aiq_ccm_matrixcfg_t* matrixAll,
                              int                           matrixAll_len,
                             const rk_aiq_ccm_matrixcfg_t*  pCcmMatrixAll[][CCM_PROFILES_NUM_MAX])
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOG1_ACCM("%s(%d): (enter)\n", __FUNCTION__, __LINE__);

    if (aCcmCof == NULL) {
        ret = XCAM_RETURN_ERROR_PARAM;
        LOGE_ACCM("%s(%d): invalid input params\n", __FUNCTION__, __LINE__);
        return ret;
    }

    //Config  pCcmMatrixAll (normal and hdr)
       // 1) get and reorder para
    for(int i = 0; i < aCcmCof_len; i++) {
        for (int j = 0; j < aCcmCof[i].matrixUsed_len; j++) {
            char name[CCM_PROFILE_NAME];
            sprintf(name, "%s", aCcmCof[i].matrixUsed[j]);
            const rk_aiq_ccm_matrixcfg_t* pCcmMatrixProfile = NULL;
            // get a ccm-profile from database
            ret = CamCalibDbGetCcmProfileByName(&matrixAll[0], matrixAll_len, name, &pCcmMatrixProfile);
            RETURN_RESULT_IF_DIFFERENT(ret, XCAM_RETURN_NO_ERROR);
            // store ccm-profile in pointer array
            pCcmMatrixAll[i][j] = pCcmMatrixProfile;
            LOGV_ACCM("CCM name  %s coef:%f,%f,%f  %f,%f,%f  %f,%f,%f  \n", name,
                      pCcmMatrixAll[i][j]->ccMatrix[0],
                      pCcmMatrixAll[i][j]->ccMatrix[1],
                      pCcmMatrixAll[i][j]->ccMatrix[2],
                      pCcmMatrixAll[i][j]->ccMatrix[3],
                      pCcmMatrixAll[i][j]->ccMatrix[4],
                      pCcmMatrixAll[i][j]->ccMatrix[5],
                      pCcmMatrixAll[i][j]->ccMatrix[6],
                      pCcmMatrixAll[i][j]->ccMatrix[7],
                      pCcmMatrixAll[i][j]->ccMatrix[8]);
            LOGV_ACCM("off:%f,%f,%f  \n", pCcmMatrixAll[i][j]->ccOffsets[0],
                      pCcmMatrixAll[i][j]->ccOffsets[1],
                      pCcmMatrixAll[i][j]->ccOffsets[2]);
        }
        // order ccm-profiles by saturation
        ret = AwbOrderCcmProfilesBySaturation(pCcmMatrixAll[i],
                                              aCcmCof[i].matrixUsed_len);
    }

    LOG1_ACCM( "%s(%d): (exit)\n", __FUNCTION__, __LINE__);
    return ret;
}
static XCamReturn SatSelectCcmProfiles
(
    const float     fSaturation,
    int32_t         no_ccm,
    const rk_aiq_ccm_matrixcfg_t* pCcmProfiles[],
    const rk_aiq_ccm_matrixcfg_t** pCcmProfile1,
    const rk_aiq_ccm_matrixcfg_t** pCcmProfile2
) {
    XCamReturn XCamReturn = XCAM_RETURN_NO_ERROR;

    if ((no_ccm == 0) || (pCcmProfiles == NULL)
            || (pCcmProfile1 == NULL) || (pCcmProfile2 == NULL)) {
        return (XCAM_RETURN_ERROR_PARAM);
    }

    if (fSaturation >= pCcmProfiles[0]->saturation) {
        *pCcmProfile1 = pCcmProfiles[0];
        *pCcmProfile2 = pCcmProfiles[0];
        LOGV_ACCM( "select:%s \n", (*pCcmProfile1)->name);
        XCamReturn = XCAM_RETURN_ERROR_OUTOFRANGE;
    } else {
        int32_t nLast = no_ccm - 1;
        if (fSaturation <= pCcmProfiles[nLast]->saturation) {
            *pCcmProfile1 = pCcmProfiles[nLast];
            *pCcmProfile2 = pCcmProfiles[nLast];
            LOGV_ACCM( "select:%s \n", (*pCcmProfile1)->name);
            XCamReturn = XCAM_RETURN_ERROR_OUTOFRANGE;
        } else {
            uint16_t n = 0;

            /* find the segment */
            while ((fSaturation <= pCcmProfiles[n]->saturation) && (n <= nLast)) {
                n++;
            }

            if (n > 0)
                n--;

            *pCcmProfile1 = pCcmProfiles[n];
            *pCcmProfile2 = pCcmProfiles[n + 1];

        }
    }

    return (XCamReturn);
}


/******************************************************************************
 * InterpolateMatrices
 *****************************************************************************/
static XCamReturn SatInterpolateMatrices
(
    const float             fSat,
    const rk_aiq_ccm_matrixcfg_t*   pCcProfileA,
    const rk_aiq_ccm_matrixcfg_t*   pCcProfileB,
    float*          pResMatrix
) {
    XCamReturn iXCamReturn = XCAM_RETURN_ERROR_PARAM;

    if ((pCcProfileA != NULL) && (pCcProfileA != NULL) && (pResMatrix != NULL)) {
        const float *pMatrixA = pCcProfileA->ccMatrix;
        const float *pMatrixB = pCcProfileB->ccMatrix;

        float fSatA = pCcProfileA->saturation;
        float fSatB = pCcProfileB->saturation;

        float f1 = ( fSatB - fSat ) / ( fSatB - fSatA ); // test: if fSat == fSatA => f1 = 1 => choose A: ok
        float f2 = 1.0f - f1;

        int i;

        for ( i = 0; i < 9; i++)
        {
            pResMatrix[i] = f1 * pMatrixA[i] + f2 * pMatrixB[i];
        }

        iXCamReturn = XCAM_RETURN_NO_ERROR;

    }

    return (iXCamReturn);
}

/******************************************************************************
 * InterpolateMatrices
 *****************************************************************************/
static XCamReturn SatInterpolateOffset
(
    const float             fSat,
    const rk_aiq_ccm_matrixcfg_t*   pCcProfileA,
    const rk_aiq_ccm_matrixcfg_t*   pCcProfileB,
    float     *pResOffset
) {
    XCamReturn result = XCAM_RETURN_ERROR_PARAM;

    if ( (pCcProfileA != NULL) && (pCcProfileB != NULL) && (pResOffset != NULL) )
    {
        const float *pOffsetA = pCcProfileA->ccOffsets;
        const float *pOffsetB = pCcProfileB->ccOffsets;

        float fSatA = pCcProfileA->saturation;
        float fSatB = pCcProfileB->saturation;

        float f1 = ( fSatB - fSat ) / ( fSatB - fSatA ); // test: if fSat == fSatA => f1 = 1 => choose A: ok
        float f2 = 1.0f - f1;

        pResOffset[CAM_3CH_COLOR_COMPONENT_RED]
            = f1 * pOffsetA[CAM_3CH_COLOR_COMPONENT_RED] + f2 * pOffsetB[CAM_3CH_COLOR_COMPONENT_RED];
        pResOffset[CAM_3CH_COLOR_COMPONENT_GREEN]
            = f1 * pOffsetA[CAM_3CH_COLOR_COMPONENT_GREEN] + f2 * pOffsetB[CAM_3CH_COLOR_COMPONENT_GREEN];
        pResOffset[CAM_3CH_COLOR_COMPONENT_BLUE]
            = f1 * pOffsetA[CAM_3CH_COLOR_COMPONENT_BLUE] + f2 * pOffsetB[CAM_3CH_COLOR_COMPONENT_BLUE];

        result = XCAM_RETURN_NO_ERROR;
    }

    return ( result );

}

/******************************************************************************
 * Damping
 *****************************************************************************/
XCamReturn Damping
(
    const float       damp,                /**< damping coefficient */
    float *pMatrixUndamped,   /**< undamped new computed matrices */
    float *pMatrixDamped,     /**< old matrices and result */
    float *pOffsetUndamped,   /**< undamped new computed offsets */
    float *pOffsetDamped,     /**< old offset and result */
    bool *converge_flag
)
{
    XCamReturn result = XCAM_RETURN_ERROR_PARAM;

    if ( (pMatrixUndamped != NULL) && (pMatrixDamped != NULL)
            && (pOffsetUndamped != NULL) && (pOffsetDamped != NULL) )
    {
        bool flag = false;
        int32_t i;
        float f = (1.0f - damp);

        /* calc. damped cc matrix */
        for( i = 0; i < 9; i++ )
        {
            pMatrixDamped[i] = (damp * pMatrixDamped[i]) + (f *  pMatrixUndamped[i]);
            if (!flag) flag = (fabs(pMatrixDamped[i] - pMatrixUndamped[i]) > DIVMIN);
        }

        /* calc. damped cc offsets */
        pOffsetDamped[CAM_3CH_COLOR_COMPONENT_RED]
            = (damp * pOffsetDamped[CAM_3CH_COLOR_COMPONENT_RED])
            + (f * pOffsetUndamped[CAM_3CH_COLOR_COMPONENT_RED]);
        pOffsetDamped[CAM_3CH_COLOR_COMPONENT_GREEN]
            = (damp * pOffsetDamped[CAM_3CH_COLOR_COMPONENT_GREEN])
            + (f * pOffsetUndamped[CAM_3CH_COLOR_COMPONENT_GREEN]);
        pOffsetDamped[CAM_3CH_COLOR_COMPONENT_BLUE]
            = (damp * pOffsetDamped[CAM_3CH_COLOR_COMPONENT_BLUE])
            + (f * pOffsetUndamped[CAM_3CH_COLOR_COMPONENT_BLUE]);

        for (i = 0; i < 3 && !flag; i++)
            flag = (fabs(pOffsetDamped[i] - pOffsetUndamped[i]) > DIVMIN);

        *converge_flag = !flag;

        result = XCAM_RETURN_NO_ERROR;
    }

    return ( result );
}

void Saturationadjust(float fScale, float flevel1, float *pccMatrixA)
{
    float  Matrix_tmp[9];
    if (fScale < DIVMIN) {
        if(fabs((flevel1-50))>DIVMIN){
            LOGW_ACCM("fSclae is  %f, so saturation adjust bypass\n", fScale);
        }
     } else {
        flevel1 = (flevel1 - 50) / 50 + 1;
        LOGV_ACCM("Satura: %f \n", flevel1);
        if (pccMatrixA == NULL) {
            LOGE_ACCM("%s: pointer pccMatrixA is NULL", __FUNCTION__);
            return;
        }
        memcpy(&Matrix_tmp, pccMatrixA, sizeof(Matrix_tmp));
        float *pccMatrixB;
        pccMatrixB = Matrix_tmp;
        /* ************************************
        *  M_A =  (M0 - E) * fscale + E
        *  M_B = rgb2ycbcr(M_A)
        *  M_B' = ycbcr2rgb[ sat_matrix * M_B ]
        *  M_A' = (M_B' - E) / fscale + E
        *  return (M_A')
        * ***********************************/
        if ( (pccMatrixA != NULL) && (pccMatrixB != NULL) )
        {
            for(int i =0; i < 9; i++)
            {
                if (i == 0 || i == 4 || i == 8){
                  pccMatrixA[i] = (pccMatrixA[i] - 1)*fScale+1;
                }
                else{
                  pccMatrixA[i] = pccMatrixA[i]*fScale;
                }
            }
            pccMatrixB[0] = 0.299 * pccMatrixA[0] + 0.587 * pccMatrixA[3] + 0.114 * pccMatrixA[6];
            pccMatrixB[1] = 0.299 * pccMatrixA[1] + 0.587 * pccMatrixA[4] + 0.114 * pccMatrixA[7];
            pccMatrixB[2] = 0.299 * pccMatrixA[2] + 0.587 * pccMatrixA[5] + 0.114 * pccMatrixA[8];
            pccMatrixB[3] = -0.1687 * pccMatrixA[0] - 0.3313 * pccMatrixA[3] + 0.5 * pccMatrixA[6];
            pccMatrixB[4] = -0.1687 * pccMatrixA[1] - 0.3313 * pccMatrixA[4] + 0.5 * pccMatrixA[7];
            pccMatrixB[5] = -0.1687 * pccMatrixA[2] - 0.3313 * pccMatrixA[5] + 0.5 * pccMatrixA[8];
            pccMatrixB[6] = 0.5 * pccMatrixA[0]  - 0.4187 * pccMatrixA[3] - 0.0813 * pccMatrixA[6];
            pccMatrixB[7] = 0.5 * pccMatrixA[1]  - 0.4187 * pccMatrixA[4] - 0.0813 * pccMatrixA[7];
            pccMatrixB[8] = 0.5 * pccMatrixA[2]  - 0.4187 * pccMatrixA[5] - 0.0813 * pccMatrixA[8];

            for(int i = 3; i < 9; i++)
            {
                 pccMatrixB[i] = flevel1 * pccMatrixB[i];
             }
            pccMatrixA[0] = 1 * pccMatrixB[0] + 0 * pccMatrixB[3] + 1.402 * pccMatrixB[6];
            pccMatrixA[1] = 1 * pccMatrixB[1] + 0 * pccMatrixB[4] + 1.402 * pccMatrixB[7];
            pccMatrixA[2] = 1 * pccMatrixB[2] + 0 * pccMatrixB[5] + 1.402 * pccMatrixB[8];
            pccMatrixA[3] = 1 * pccMatrixB[0] - 0.34414 * pccMatrixB[3]  - 0.71414 * pccMatrixB[6];
            pccMatrixA[4] = 1 * pccMatrixB[1] - 0.34414 * pccMatrixB[4]  - 0.71414 * pccMatrixB[7];
            pccMatrixA[5] = 1 * pccMatrixB[2] - 0.34414 * pccMatrixB[5]  - 0.71414 * pccMatrixB[8];
            pccMatrixA[6] = 1 * pccMatrixB[0]  + 1.772 * pccMatrixB[3] + 0 * pccMatrixB[6];
            pccMatrixA[7] = 1 * pccMatrixB[1]  + 1.772 * pccMatrixB[4] + 0 * pccMatrixB[7];
            pccMatrixA[8] = 1 * pccMatrixB[2]  + 1.772 * pccMatrixB[5] + 0 * pccMatrixB[8];


            for(int i =0; i < 9; i++)
            {
                if (i == 0 || i == 4 || i == 8){
                  pccMatrixA[i] = (pccMatrixA[i] - 1)/fScale+1;
                }
                else{
                  pccMatrixA[i] = pccMatrixA[i]/fScale;
                }
            }
        }

      }

}


XCamReturn CamCalibDbGetCcmProfileByName(const rk_aiq_ccm_matrixcfg_t* matrixAll,
                                         int matrixAll_len,
                                         char* name,
                                         const rk_aiq_ccm_matrixcfg_t **pCcmMatrixProfile)
{
    LOG1_ACCM("%s: (enter)\n", __FUNCTION__);

    XCamReturn ret = XCAM_RETURN_ERROR_FAILED;

    for(int i = 0; i <matrixAll_len; i++) {
        if(strcmp(matrixAll[i].name, name) == 0) {
            *pCcmMatrixProfile = &matrixAll[i];
            ret = XCAM_RETURN_NO_ERROR;
            break;
        }
    }
    if(ret != XCAM_RETURN_NO_ERROR) {
        LOGE_ACCM("can't find %s in matrixAll \n", name);
    }
    LOG1_ACCM("%s: (exit)\n", __FUNCTION__);

    return ret;
}

static void UpdateIlluProbList(struct list_head *l, int illu, float prob, int listMaxSize)
{
    prob_node_t *pCurNode, *pNode0;

    if(listMaxSize == 0) {
        return;
    }
    int sizeList = get_list_num(l);
    if (sizeList < listMaxSize) {
        pCurNode = (prob_node_t*)malloc(sizeof(prob_node_t));
        pCurNode->value = illu;
        pCurNode->prob = prob;
        list_prepare_item(&pCurNode->node);
        list_add_tail((struct list_head*)(&pCurNode->node), l);
    } else {
        // input list
        //     |-------------------------|
        //     head<->n0<->n1<->n2<->n3
        // output list
        //     |-------------------------|
        //     n0'<->head<->n1<->n2<->n3
        //     n0'->value = illu;
        pNode0 = (prob_node_t*)(l->next);
        pNode0->value = illu;
        pNode0->prob = prob;
        struct list_head* nodeH = l;
        struct list_head* node0 = nodeH->next;
        list_swap_item(nodeH, node0);

    }
}


static void StableProbEstimation(struct list_head *l, int listSize, int count, int illuNum, float* probSet)
{
    int sizeList = get_list_num(l);
    if(sizeList < listSize || listSize == 0) {
        return;
    }
    float *prob_tmp = (float*)malloc(illuNum*sizeof(float));
    memset(prob_tmp, 0, illuNum*sizeof(float));
    struct list_head *pNextNode = l->next;
    while (NULL != pNextNode)
    {
        prob_node_t *pL;
        pL = (prob_node_t*)pNextNode;
        prob_tmp[pL->value] += pL->prob;
        pNextNode = pNextNode->next;
    }

    for(int i=0; i<illuNum; i++){
        probSet[i] = prob_tmp[i]/(float)count;
        LOGD_ACCM("illu(%d), stable prob = %f \n", i, probSet[i]);
    }
    free(prob_tmp);
    prob_tmp = NULL;
}

#if RKAIQ_ACCM_ILLU_VOTE
static void UpdateDominateIlluList(struct list_head* l_head, int illu, int listMaxSize)
{
    illu_node_t* pCurNode, * pNode0;
    if (listMaxSize == 0) {
        return;
    }
    int sizeList = get_list_num(l_head);
    if (sizeList < listMaxSize) {
        pCurNode = (illu_node_t*)malloc(sizeof(illu_node_t));
        pCurNode->value = illu;
        list_prepare_item(&pCurNode->node);
        list_add_tail((struct list_head*)(&pCurNode->node), l_head);
    }
    else {
        // input list
        //     |-------------------------|
        //     head<->n0<->n1<->n2<->n3
        // output list
        //     |-------------------------|
        //     n0'<->head<->n1<->n2<->n3
        //     n0'->value = illu;
        pNode0 = (illu_node_t*)(l_head->next);
        pNode0->value = illu;
        struct list_head* nodeH = l_head;
        struct list_head* node0 = nodeH->next;
        list_swap_item(nodeH, node0);

    }
}

static void StableIlluEstimation(struct list_head * head, int listSize, int illuNum, float varianceLuma, float varianceLumaTh, bool awbConverged, int preIllu, int *newIllu)
{
    int sizeList = get_list_num(head);
    if (sizeList < listSize || listSize == 0) {
        return;
    }
    /*if( awbConverged) {
        *newIllu = preIllu;
        LOGD_ACCM("awb is converged , reserve the last illumination(%d) \n", preIllu );
        return;
    }*/
    /*if( varianceLuma <= varianceLumaTh) {
        *newIllu = preIllu;
        LOGD_ACCM("varianceLuma %f < varianceLumaTh %f , reserve the last illumination(%d) \n", varianceLuma,varianceLumaTh,preIllu );
        return;
    }*/
    struct list_head* pNextNode = head->next;
    illu_node_t* pL;
    int* illuSet = (int*)malloc(illuNum * sizeof(int));
    memset(illuSet, 0, illuNum * sizeof(int));
    while (head != pNextNode)
    {
        pL = (illu_node_t*)pNextNode;
        illuSet[pL->value]++;
        pNextNode = pNextNode->next;
    }
    int max_count = 0;
    for (int i = 0; i < illuNum; i++) {
        LOGV_ACCM("illu(%d), count(%d)\n", i, illuSet[i]);
        if (illuSet[i] > max_count) {
            max_count = illuSet[i];
            *newIllu = i;
        }
    }
    free(illuSet);
    LOGD_ACCM("varianceLuma %f, varianceLumaTh %f final estmination illu is %d\n", varianceLuma, varianceLumaTh, *newIllu);

}
#endif

XCamReturn interpCCMbywbgain(const CalibDbV2_Ccm_illu_est_Para_t* illu_estim,
                             const rk_aiq_ccm_illucfg_t           aCcmCof[],
                             int                                  aCcmCof_len,
                             accm_handle_t                        hAccm,
                             float                                fSaturation)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    //1) estimate illuminant prob
    float probfSaturation = 0;
    const rk_aiq_ccm_matrixcfg_t* pCcmProfile1 = NULL;
    const rk_aiq_ccm_matrixcfg_t* pCcmProfile2 = NULL;

    memset(hAccm->accmRest.undampedCcmMatrix, 0, sizeof(hAccm->accmRest.undampedCcmMatrix));
    memset(hAccm->accmRest.undampedCcOffset, 0, sizeof(hAccm->accmRest.undampedCcOffset));
    float* prob = (float*)malloc(aCcmCof_len * sizeof(float));
    ret         = illuminant_index_candidate_ccm(aCcmCof_len, aCcmCof,
                                                 hAccm->accmSwInfo.awbGain,
                                                 illu_estim->default_illu,
                                                 illu_estim->prob_limit,
                                                 illu_estim->weightRB, prob);
    RETURN_RESULT_IF_DIFFERENT(ret, XCAM_RETURN_NO_ERROR);

    // calculate stable prob
    int problistsize = illu_estim->frame_no * aCcmCof_len;
    for (int i = 0; i < aCcmCof_len; i++)
        UpdateIlluProbList(&hAccm->accmRest.problist, i, prob[i], problistsize);
    int frames = (int)hAccm->count > (illu_estim->frame_no - 1) ? illu_estim->frame_no
                                                                : hAccm->count;  // todo

    StableProbEstimation(&hAccm->accmRest.problist, problistsize, frames, aCcmCof_len, prob);

    // 2) all illuminant do interp by fSaturation
    float undampedCcmMatrix[9];
    float undampedCcOffset[3];
    for (int i = 0; i < aCcmCof_len; i++) {
        if (fabs(prob[i])<DIVMIN)
            continue;
        //     (1) get IlluProfiles of Candidate illuminants, and calculate fSaturation
        const rk_aiq_ccm_illucfg_t* pDomIlluProfile = &aCcmCof[i];
        interpolation(pDomIlluProfile->gain_sat_curve.gains,
                      pDomIlluProfile->gain_sat_curve.sat,
                      4,
                      hAccm->accmSwInfo.sensorGain, &fSaturation);

        //     (2) interp CCM matrix and offset
        ret = SatSelectCcmProfiles(fSaturation, pDomIlluProfile->matrixUsed_len, hAccm->pCcmMatrixAll[i],
                                   &pCcmProfile1, &pCcmProfile2);
        if (ret == XCAM_RETURN_NO_ERROR) {
            XCamReturn ret1 = XCAM_RETURN_NO_ERROR;
            if (pCcmProfile1 && pCcmProfile2) {
                LOGV_ACCM("Illu : %s interp by sat, final fSaturation: %f (%f .. %f)\n",
                          pDomIlluProfile->name, fSaturation, pCcmProfile1->saturation,
                          pCcmProfile2->saturation);
                ret = SatInterpolateMatrices(fSaturation, pCcmProfile1, pCcmProfile2,
                                             undampedCcmMatrix);
                ret1 =
                    SatInterpolateOffset(fSaturation, pCcmProfile1, pCcmProfile2, undampedCcOffset);
                if ((ret != XCAM_RETURN_NO_ERROR) && (ret1 != XCAM_RETURN_NO_ERROR)) {
                    free(prob);
                    return (ret);
                }
            }
        } else if (ret == XCAM_RETURN_ERROR_OUTOFRANGE) {
            /* we don't need to interpolate */
            LOGV_ACCM("Illu : %s, final fSaturation: %f (%f)\n", pDomIlluProfile->name, fSaturation,
                      pCcmProfile1->saturation);
            memcpy(undampedCcmMatrix, pCcmProfile1->ccMatrix, sizeof(pCcmProfile1->ccMatrix));
            memcpy(undampedCcOffset, pCcmProfile1->ccOffsets, sizeof(pCcmProfile1->ccOffsets));
            ret = XCAM_RETURN_NO_ERROR;
        } else {
            free(prob);
            return (ret);
        }
        probfSaturation = probfSaturation + fSaturation*prob[i];

        for (int j = 0; j < 9; j++)
            hAccm->accmRest.undampedCcmMatrix[j] += undampedCcmMatrix[j]*prob[i];
        for (int j = 0; j < 3; j++)
            hAccm->accmRest.undampedCcOffset[j] += undampedCcOffset[j]*prob[i];
    }
    hAccm->accmRest.fSaturation =  probfSaturation;
    hAccm->accmRest.pCcmProfile1 = pCcmProfile1;
    hAccm->accmRest.pCcmProfile2 = pCcmProfile2;

    // 3)
    free(prob);
    prob = NULL;
    return ret;
}

XCamReturn selectCCM(const rk_aiq_ccm_illucfg_t aCcmCof[],
                     int                        aCcmCof_len,
                     accm_handle_t              hAccm,
                     float                      fSaturation,
                     bool*                      updUndampMat)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    const rk_aiq_ccm_matrixcfg_t* pCcmProfile1 = NULL;
    const rk_aiq_ccm_matrixcfg_t* pCcmProfile2 = NULL;
    int dominateIlluProfileIdx;
#if RKAIQ_ACCM_ILLU_VOTE
    int dominateIlluListSize = 15;//to do from xml;
    float varianceLumaTh = 0.006;//to do from xml;
#endif

    ret = illuminant_index_estimation_ccm(aCcmCof_len, aCcmCof,
                                          hAccm->accmSwInfo.awbGain,
                                          &dominateIlluProfileIdx);
    RETURN_RESULT_IF_DIFFERENT(ret, XCAM_RETURN_NO_ERROR);
#if RKAIQ_ACCM_ILLU_VOTE
    UpdateDominateIlluList(&hAccm->accmRest.dominateIlluList, dominateIlluProfileIdx, dominateIlluListSize);
    StableIlluEstimation(&hAccm->accmRest.dominateIlluList, dominateIlluListSize, aCcmCof_len,
                         hAccm->accmSwInfo.varianceLuma, varianceLumaTh,
                         hAccm->accmSwInfo.awbConverged, hAccm->accmRest.dominateIlluProfileIdx,
                         &dominateIlluProfileIdx);
#endif

    // 2)
    const rk_aiq_ccm_illucfg_t* pDomIlluProfile = &aCcmCof[dominateIlluProfileIdx];
    interpolation(pDomIlluProfile->gain_sat_curve.gains,
                  pDomIlluProfile->gain_sat_curve.sat,
                  4,
                  hAccm->accmSwInfo.sensorGain, &fSaturation);
    bool flag0 = false;
    flag0 = hAccm->isReCal_ ||
            hAccm->calib_update ||
            (dominateIlluProfileIdx != hAccm->accmRest.dominateIlluProfileIdx) ||
            (fabs(fSaturation - hAccm->accmRest.fSaturation) > DIVMIN);
    LOGD_ACCM("pickCCMprof = graymode chg (%d) || calib_update (%d) || dominateIlluProfileIdx: %d->%d || fSaturation: %f->%f\n",
            flag0, hAccm->calib_update, hAccm->accmRest.dominateIlluProfileIdx, dominateIlluProfileIdx,
            hAccm->accmRest.fSaturation, fSaturation);

    //3)
    if (flag0) {
        *updUndampMat = hAccm->calib_update ||
                       (fabs(fSaturation - hAccm->accmRest.fSaturation) > 0);
        hAccm->accmRest.fSaturation =  fSaturation;
        hAccm->accmRest.dominateIlluProfileIdx = dominateIlluProfileIdx;

        ret = SatSelectCcmProfiles(hAccm->accmRest.fSaturation, pDomIlluProfile->matrixUsed_len,
                                hAccm->pCcmMatrixAll[dominateIlluProfileIdx], &pCcmProfile1,
                                &pCcmProfile2);
        if (pCcmProfile1 && pCcmProfile2) {
            *updUndampMat = (*updUndampMat) ||
                            strcmp(pCcmProfile1->name, hAccm->accmRest.pCcmProfile1->name) ||
                            strcmp(pCcmProfile2->name, hAccm->accmRest.pCcmProfile2->name);
            LOGD_ACCM("CcmProfile changed: %d = calib_update(%d) || fSaturation changed || pCcmProfile1/2 changed",
                      *updUndampMat, hAccm->calib_update);
        } else {
            LOGD_ACCM("check %s pCcmProfile: %p %p \n", pDomIlluProfile->name, pCcmProfile1, pCcmProfile2);
            return XCAM_RETURN_ERROR_PARAM;
        }
        if (*updUndampMat) {
            if (ret == XCAM_RETURN_NO_ERROR) {
                LOGD_ACCM("final fSaturation: %f (%f .. %f)\n", hAccm->accmRest.fSaturation,
                        pCcmProfile1->saturation, pCcmProfile2->saturation);
                ret = SatInterpolateMatrices(hAccm->accmRest.fSaturation, pCcmProfile1, pCcmProfile2,
                                            hAccm->accmRest.undampedCcmMatrix);
                RETURN_RESULT_IF_DIFFERENT(ret, XCAM_RETURN_NO_ERROR);

                ret = SatInterpolateOffset(hAccm->accmRest.fSaturation, pCcmProfile1, pCcmProfile2,
                                        hAccm->accmRest.undampedCcOffset);
                RETURN_RESULT_IF_DIFFERENT(ret, XCAM_RETURN_NO_ERROR);
            } else if (ret == XCAM_RETURN_ERROR_OUTOFRANGE) {
                /* we don't need to interpolate */
                LOGD_ACCM("final fSaturation: %f (%f)\n",   hAccm->accmRest.fSaturation, pCcmProfile1->saturation);
                memcpy(hAccm->accmRest.undampedCcmMatrix, pCcmProfile1->ccMatrix, sizeof(float)*9);
                memcpy(hAccm->accmRest.undampedCcOffset, pCcmProfile1->ccOffsets, sizeof(float)*3);
                ret = XCAM_RETURN_NO_ERROR;
            } else {
                return (ret);
            }
        }
        hAccm->accmRest.pCcmProfile1 = pCcmProfile1;
        hAccm->accmRest.pCcmProfile2 = pCcmProfile2;
    }

    return (ret);
}

bool JudgeCcmRes3aConverge
(
    ccm_3ares_info_t *res3a_info,
    accm_sw_info_t *accmSwInfo,
    float gain_th,
    float wbgain_th
) {
    bool gain_upd = true;
    bool wbgain_upd = true;
    float wb_th = wbgain_th*wbgain_th;
    if (fabs(res3a_info->sensorGain - accmSwInfo->sensorGain) > gain_th) {
        res3a_info->sensorGain = accmSwInfo->sensorGain;
    } else {
        gain_upd = false;
        accmSwInfo->sensorGain = res3a_info->sensorGain;
    }

    if ((res3a_info->awbGain[0]-accmSwInfo->awbGain[0])*(res3a_info->awbGain[0]-accmSwInfo->awbGain[0])
         + (res3a_info->awbGain[1]-accmSwInfo->awbGain[1])*(res3a_info->awbGain[1]-accmSwInfo->awbGain[1]) > wb_th) {
        res3a_info->awbGain[0] = accmSwInfo->awbGain[0];
        res3a_info->awbGain[1] = accmSwInfo->awbGain[1];
        LOGD_ACCM("update wbgain: %f, %f\n", accmSwInfo->awbGain[0], accmSwInfo->awbGain[1]);
    } else {
        wbgain_upd = false;
        accmSwInfo->awbGain[0] = res3a_info->awbGain[0];
        accmSwInfo->awbGain[1] = res3a_info->awbGain[1];
    }

    return (gain_upd || wbgain_upd);
}

/**************************************************
  * ReloadCCMCalibV2
  *      config ccm_tune used new CalibV2 json para
***************************************************/
#if RKAIQ_HAVE_CCM_V1
XCamReturn ReloadCCMCalibV2(const CalibDbV2_Ccm_Para_V2_t* newCalib,
                            rk_aiq_ccm_iqparam_attrib_t*   stCalib,
                            CalibDbV2_Ccm_illu_est_Para_t* stIlluestCfg) {
#elif RKAIQ_HAVE_CCM_V2
XCamReturn ReloadCCMCalibV2(const CalibDbV2_Ccm_Para_V32_t* newCalib,
                            rk_aiq_ccm_v2_iqparam_attrib_t* stCalib,
                            CalibDbV2_Ccm_illu_est_Para_t*  stIlluestCfg) {
#endif

    if (newCalib == NULL || stCalib == NULL){
        LOGE_ACCM("%s: newCalib or stCalib is NULL !!!", __FUNCTION__);
        return XCAM_RETURN_ERROR_PARAM;
    }

#if RKAIQ_HAVE_CCM_V1
    memcpy(&stCalib->lumaCCM, &newCalib->lumaCCM, sizeof(CalibDbV2_Ccm_Luma_Ccm_t));
#elif RKAIQ_HAVE_CCM_V2
    memcpy(&stCalib->lumaCCM, &newCalib->lumaCCM, sizeof(CalibDbV2_Ccm_Luma_Ccm_V2_t));
    memcpy(&stCalib->enhCCM, &newCalib->enhCCM, sizeof(CalibDbV2_Ccm_Enhance_Para_t));
#endif
    memcpy(&stCalib->control, &newCalib->control, sizeof(CalibDbV2_Ccm_Control_Para_t));

    stCalib->damp_enable = newCalib->TuningPara.damp_enable;

    const CalibDbV2_Ccm_Tuning_Para_t* TuningPara = &newCalib->TuningPara;
    stCalib->aCcmCof_len = TuningPara->aCcmCof_len;
    stCalib->matrixAll_len = TuningPara->matrixAll_len;
    if (!stCalib->aCcmCof_len || !stCalib->matrixAll_len ||
        !TuningPara->aCcmCof || !TuningPara->matrixAll) {
        return XCAM_RETURN_ERROR_PARAM;
    }

    for (int i = 0; i < stCalib->aCcmCof_len; i++) {
        strcpy(stCalib->aCcmCof[i].name, TuningPara->aCcmCof[i].name);

        memcpy(stCalib->aCcmCof[i].awbGain,
               TuningPara->aCcmCof[i].awbGain,
               sizeof(stCalib->aCcmCof[i].awbGain));

        stCalib->aCcmCof[i].minDist = TuningPara->aCcmCof[i].minDist;
        stCalib->aCcmCof[i].matrixUsed_len = TuningPara->aCcmCof[i].matrixUsed_len;

        for (int j = 0; j < stCalib->aCcmCof[i].matrixUsed_len; j ++) {
            sprintf(stCalib->aCcmCof[i].matrixUsed[j], "%s",
                   TuningPara->aCcmCof[i].matrixUsed[j]);
        }
        stCalib->aCcmCof[i].gain_sat_curve = TuningPara->aCcmCof[i].gain_sat_curve;
    }

    for (int i = 0; i < stCalib->matrixAll_len; i++) {
        strcpy(stCalib->matrixAll[i].name, TuningPara->matrixAll[i].name);
        strcpy(stCalib->matrixAll[i].illumination,
               TuningPara->matrixAll[i].illumination);

        memcpy(stCalib->matrixAll[i].ccMatrix,
               TuningPara->matrixAll[i].ccMatrix,
               sizeof(stCalib->matrixAll[i].ccMatrix));

        memcpy(stCalib->matrixAll[i].ccOffsets,
               TuningPara->matrixAll[i].ccOffsets,
               sizeof(stCalib->matrixAll[i].ccOffsets));

        stCalib->matrixAll[i].saturation = TuningPara->matrixAll[i].saturation;
    }

    if (stIlluestCfg->default_illu == NULL)
        stIlluestCfg->default_illu = (char*)malloc(sizeof(char)*CCM_ILLUMINATION_NAME);
    strcpy(stIlluestCfg->default_illu, TuningPara->illu_estim.default_illu);
    memcpy(stIlluestCfg->weightRB, TuningPara->illu_estim.weightRB, sizeof(stIlluestCfg->weightRB));
    stIlluestCfg->interp_enable    = TuningPara->illu_estim.interp_enable;
    stIlluestCfg->prob_limit       = TuningPara->illu_estim.prob_limit;
    stIlluestCfg->frame_no         = TuningPara->illu_estim.frame_no;

    return (XCAM_RETURN_NO_ERROR);
}


RKAIQ_END_DECLARE



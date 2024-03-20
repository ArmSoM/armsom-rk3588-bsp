/*
 *rk_aiq_accm_algo_com.h
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

#ifndef _RK_AIQ_ACCM_ALGO_COM_H_
#define _RK_AIQ_ACCM_ALGO_COM_H_
#include "accm/rk_aiq_types_accm_algo_prvt.h"
#include "RkAiqCalibDbTypes.h"
#include "RkAiqCalibDbTypesV2.h"
#include "RkAiqCalibDbV2Helper.h"


RKAIQ_BEGIN_DECLARE

XCamReturn illuminant_index_estimation_ccm(int light_num, const rk_aiq_ccm_illucfg_t illAll[], float awbGain[2], int* illuminant_index);
XCamReturn Damping(const float damp, float *pMatrixUndamped, float *pMatrixDamped, float *pOffsetUndamped, float *pOffsetDamped, bool *converge_flag);
void Saturationadjust(float fScale, float flevel1, float *pccMatrixA);

XCamReturn CamCalibDbGetCcmProfileByName(const rk_aiq_ccm_matrixcfg_t* matrixAll,
                                         int matrixAll_len,
                                         char* name,
                                         const rk_aiq_ccm_matrixcfg_t **pCcmMatrixProfile);

XCamReturn interpCCMbywbgain(const CalibDbV2_Ccm_illu_est_Para_t* illu_estim,
                             const rk_aiq_ccm_illucfg_t           aCcmCof[],
                             int                                  aCcmCof_len,
                             accm_handle_t                        hAccm,
                             float                                fSaturation);

XCamReturn selectCCM(const rk_aiq_ccm_illucfg_t aCcmCof[],
                     int                        aCcmCof_len,
                     accm_handle_t              hAccm,
                     float                      fSaturation,
                     bool*                      updUndampMat);

bool JudgeCcmRes3aConverge(ccm_3ares_info_t *res3a_info, accm_sw_info_t *accmSwInfo, float gain_th, float wbgain_th);

XCamReturn Swinfo_wbgain_init(float                      awbGain[2],
                              const rk_aiq_ccm_illucfg_t aCcmCof[],
                              int                        aCcmCof_len,
                              const char*                illuName);

XCamReturn pCcmMatrixAll_init(const rk_aiq_ccm_illucfg_t*   aCcmCof,
                              int                           aCcmCof_len,
                              const rk_aiq_ccm_matrixcfg_t* matrixAll,
                              int                           matrixAll_len,
                              const rk_aiq_ccm_matrixcfg_t* pCcmMatrixAll[][CCM_PROFILES_NUM_MAX]);

#if RKAIQ_HAVE_CCM_V1
XCamReturn ReloadCCMCalibV2(const CalibDbV2_Ccm_Para_V2_t* newCalib,
                            rk_aiq_ccm_iqparam_attrib_t*   stCalib,
                            CalibDbV2_Ccm_illu_est_Para_t* stIlluestCfg);
#elif RKAIQ_HAVE_CCM_V2
XCamReturn ReloadCCMCalibV2(const CalibDbV2_Ccm_Para_V32_t* newCalib,
                            rk_aiq_ccm_v2_iqparam_attrib_t* stCalib,
                            CalibDbV2_Ccm_illu_est_Para_t*  stIlluestCfg);
#endif



RKAIQ_END_DECLARE

#endif


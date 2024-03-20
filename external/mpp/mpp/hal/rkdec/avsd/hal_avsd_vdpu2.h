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

#ifndef __HAL_AVSD_VDPU2_H__
#define __HAL_AVSD_VDPU2_H__

#include "mpp_hal.h"

#ifdef  __cplusplus
extern "C" {
#endif

MPP_RET hal_avsd_vdpu2_init    (void *decoder, MppHalCfg *cfg);
MPP_RET hal_avsd_vdpu2_deinit  (void *decoder);
MPP_RET hal_avsd_vdpu2_gen_regs(void *decoder, HalTaskInfo *task);
MPP_RET hal_avsd_vdpu2_start   (void *decoder, HalTaskInfo *task);
MPP_RET hal_avsd_vdpu2_wait    (void *decoder, HalTaskInfo *task);
MPP_RET hal_avsd_vdpu2_reset   (void *decoder);
MPP_RET hal_avsd_vdpu2_flush   (void *decoder);
MPP_RET hal_avsd_vdpu2_control (void *decoder, MpiCmd cmd_type, void *param);

#ifdef  __cplusplus
}
#endif

#endif /*__HAL_AVSD_VDPU2_H__*/

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
 *
 */
#ifndef SRC_GRAPH_GRAPH_CONTROL_H_
#define SRC_GRAPH_GRAPH_CONTROL_H_

#include "uac_common_def.h"
#include <rt_header.h>
#include <rt_metadata.h>
#include <RTUACGraph.h>
#include <RTMediaBuffer.h>
#include <rt_metadata.h>
#include <RTMediaMetaKeys.h>

void graph_set_samplerate(RTUACGraph* uac, int type, UacAudioConfig& config);
void graph_set_volume(RTUACGraph* uac, int type, UacAudioConfig& config);
void graph_set_ppm(RTUACGraph* uac, int type, UacAudioConfig& config);

#endif  // SRC_GRAPH_GRAPH_CONTROL_H_

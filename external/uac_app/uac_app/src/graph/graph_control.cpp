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

#include "uac_log.h"
#include "graph_control.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "graph_control"
#endif // LOG_TAG

void graph_set_samplerate(RTUACGraph* uac, int type, UacAudioConfig& config) {
    if (uac == NULL)
        return;

    int sampleRate = config.samplerate;
    if (sampleRate == 0)
        return;

    RtMetaData meta;
    meta.setInt32(OPT_SAMPLE_RATE, sampleRate);
    ALOGD("%s: type = %d, sampleRate = %d\n", __FUNCTION__, type, sampleRate);
    /*
     * 1. for usb capture, we update audio config to capture
     * 2. for usb playback, if there is resample before usb playback,
     *    we set audio config to this resample, the new config will
     *    pass to usb playback from resample to usb playback when
     *    the datas move from resample to usb.
     * 3. we alway use samperate=48K to open mic and speaker,
     *    because usually, they use the same group i2s, and
     *    not allowned to use diffrent samplerate.
     */
    if (type == UAC_STREAM_RECORD) {
        // the usb record always the first node
        meta.setInt32(kKeyTaskNodeId, 0);
        meta.setCString(kKeyPipeInvokeCmd, OPT_SET_ALSA_CAPTURE);
    } else {
        // find the resample before usb playback, see mic_recode_usb_playback.json
        meta.setInt32(kKeyTaskNodeId, 1);
        meta.setCString(kKeyPipeInvokeCmd, OPT_SET_RESAMPLE);
    }

    uac->invoke(GRAPH_CMD_TASK_NODE_PRIVATE_CMD, &meta);
}

void graph_set_volume(RTUACGraph* uac, int type, UacAudioConfig& config) {
    if (uac == NULL)
        return;

    RtMetaData meta;
    int mute = config.mute;
    float volume = config.floatVol;
    ALOGD("type = %d, mute = %d, volume = %f\n", type, mute, volume);
    if (type == UAC_STREAM_RECORD) {
       /*
        * for usb capture, we update audio volume config to capture,
        * see usb_recode_speaker_playback.json, in this json, the volume filter node id is 2,
        * so we set the key kKeyTaskNodeId to 2, this can modify for need.
        */
        meta.setInt32(kKeyTaskNodeId, 2);
        meta.setFloat(OPT_VOLUME, volume);
        meta.setInt32(OPT_MUTE, mute);
        meta.setCString(kKeyPipeInvokeCmd, OPT_SET_VOLUME);
    } else {
        // add usb playback if need,  see mic_recode_usb_playback.json,
        // the volume filter node id is 2,  this can modify for need.
        meta.setInt32(kKeyTaskNodeId, 2);
        meta.setFloat(OPT_VOLUME, volume);
        meta.setInt32(OPT_MUTE, mute);
        meta.setCString(kKeyPipeInvokeCmd, OPT_SET_VOLUME);
    }

    uac->invoke(GRAPH_CMD_TASK_NODE_PRIVATE_CMD, &meta);
}

void graph_set_ppm(RTUACGraph* uac, int type, UacAudioConfig& config) {
    if (uac == NULL)
        return;

    RtMetaData meta;
    int ppm = config.ppm;
    ALOGD("type = %d, ppm = %d\n", type, ppm);
    if (type == UAC_STREAM_RECORD) {
        meta.setInt32(kKeyTaskNodeId, 3);
        meta.setInt32(OPT_PPM, ppm);
        meta.setCString(kKeyPipeInvokeCmd, OPT_SET_PPM);
    } else {
        meta.setInt32(kKeyTaskNodeId, 0);
        meta.setInt32(OPT_PPM, ppm);
        meta.setCString(kKeyPipeInvokeCmd, OPT_SET_PPM);
    }

    uac->invoke(GRAPH_CMD_TASK_NODE_PRIVATE_CMD, &meta);
}


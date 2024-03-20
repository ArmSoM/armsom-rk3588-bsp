/*
 * Copyright 2022 Rockchip Electronics Co. LTD
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
#include "uac_control_graph.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "uac_graph"
#endif // LOG_TAG

/*
 * pc datas -> rv1109
 * usb record->xxxx process->speaker playback
 */
#define UAC_USB_RECORD_SPK_PLAY_CONFIG_FILE "/oem/usr/share/uac_app/usb_recode_speaker_playback.json"

/*
 * rv1109 datas -> pc
 * mic record->>xxxx process->usb playback
 */
#define UAC_MIC_RECORD_USB_PLAY_CONFIG_FILE "/oem/usr/share/uac_app/mic_recode_usb_playback.json"

typedef struct _UacStream {
    UacAudioConfig  config;
    RTUACGraph     *uac;
} UacGraphStream;

typedef struct _UACControlGraph {
    int mode;
    UacGraphStream stream;
} UacControlGraph;

UacControlGraph* getContextGraph(void* context) {
    UacControlGraph* ctx = reinterpret_cast<UacControlGraph *>(context);
    return ctx;
}

UACControlGraph::UACControlGraph(int mode) {
    UacControlGraph *ctx= (UacControlGraph*)calloc(1, sizeof(UacControlGraph));
    memset(ctx, 0, sizeof(UacControlGraph));

    ctx->mode = mode;
    ctx->stream.config.samplerate = 48000;
    ctx->stream.config.floatVol = 1.0;
    ctx->stream.config.mute = 0;
    ctx->stream.config.ppm = 0;

    mCtx = reinterpret_cast<void *>(ctx);
}

UACControlGraph::~UACControlGraph() {
    UacControlGraph* ctx = reinterpret_cast<UacControlGraph *>(mCtx);
    if (ctx) {
        if (ctx->stream.uac != NULL) {
            delete(ctx->stream.uac);
            ctx->stream.uac = NULL;
        }
        free(ctx);
        mCtx = RT_NULL;
    }
}

void UACControlGraph::uacSetSampleRate(int sampleRate) {
    ALOGD("samplerate = %d\n", sampleRate);
    UacControlGraph* ctx = reinterpret_cast<UacControlGraph *>(mCtx);
    ctx->stream.config.samplerate = sampleRate;
    RTUACGraph* uac = ctx->stream.uac;
    if (uac != NULL) {
        graph_set_samplerate(uac, ctx->mode, ctx->stream.config);
    }
}

void UACControlGraph::uacSetVolume(int volume) {
    ALOGD("volume = %d\n", volume);
    UacControlGraph* ctx = reinterpret_cast<UacControlGraph *>(mCtx);
    ctx->stream.config.floatVol = ((float)volume/100.0);
    RTUACGraph* uac = ctx->stream.uac;
    if (uac != NULL) {
        graph_set_volume(uac, ctx->mode, ctx->stream.config);
    }
}

void UACControlGraph::uacSetMute(int mute) {
    ALOGD("type = %d, mute = %d\n", mute);
    UacControlGraph* ctx = reinterpret_cast<UacControlGraph *>(mCtx);
    ctx->stream.config.mute = mute;
    RTUACGraph* uac = ctx->stream.uac;
    if (uac != NULL) {
        graph_set_volume(uac, ctx->mode, ctx->stream.config);
    }
}

void UACControlGraph::uacSetPpm(int ppm) {
    ALOGD("type = %d, ppm = %d\n", ppm);
    UacControlGraph* ctx = reinterpret_cast<UacControlGraph *>(mCtx);
    ctx->stream.config.ppm = ppm;
    RTUACGraph* uac = ctx->stream.uac;
    if (uac != NULL) {
        graph_set_ppm(uac, ctx->mode, ctx->stream.config);
    }
}

int UACControlGraph::uacStart() {
    UacControlGraph* ctx = reinterpret_cast<UacControlGraph *>(mCtx);

    uacStop();

    char* config = (char*)UAC_MIC_RECORD_USB_PLAY_CONFIG_FILE;
    char* name = (char*)"uac_playback";
    if (ctx->mode == UAC_STREAM_RECORD) {
        name = (char*)"uac_record";
        config = (char*)UAC_USB_RECORD_SPK_PLAY_CONFIG_FILE;
    }
    ALOGD("config = %s\n", config);
    RTUACGraph* uac = new RTUACGraph(name);
    if (uac == NULL) {
        ALOGE("error, malloc fail\n");
        return -1;
    }

    // default configs will be readed in json file
    uac->autoBuild(config);
    uac->prepare();
    graph_set_volume(uac, ctx->mode, ctx->stream.config);
    graph_set_samplerate(uac, ctx->mode, ctx->stream.config);
    graph_set_ppm(uac, ctx->mode, ctx->stream.config);
    uac->start();

    ctx->stream.uac = uac;

    return 0;
}

void UACControlGraph::uacStop() {
    UacControlGraph* ctx = reinterpret_cast<UacControlGraph *>(mCtx);
    ALOGD("stop\n");
    RTUACGraph *uac = ctx->stream.uac;
    ctx->stream.uac = NULL;

    if (uac != NULL) {
        uac->stop();
        uac->waitUntilDone();
        delete uac;
    }
}

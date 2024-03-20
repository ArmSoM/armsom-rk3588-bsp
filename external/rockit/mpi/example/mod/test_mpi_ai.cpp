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

#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <sys/poll.h>
#include "rk_defines.h"
#include "rk_debug.h"
#include "rk_mpi_ai.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"

#include "test_comm_argparse.h"

static RK_BOOL gAiExit = RK_FALSE;
#define TEST_AI_WITH_FD 0
#define AI_ALGO_FRAMES 256  // baed on 16kHz, it's  128 during 8kHz

typedef struct _rkMpiAICtx {
    const char *srcFilePath;
    const char *dstFilePath;
    RK_S32      s32LoopCount;
    RK_S32      s32ChnNum;
    RK_S32      s32DeviceSampleRate;
    RK_S32      s32SampleRate;
    RK_S32      s32DeviceChannel;
    RK_S32      s32Channel;
    RK_S32      s32BitWidth;
    RK_S32      s32DevId;
    RK_S32      s32FrameNumber;
    RK_S32      s32FrameLength;
    char       *chCardName;
    RK_S32      s32ChnIndex;
    RK_S32      s32SetVolumeCurve;
    RK_S32      s32SetVolume;
    RK_S32      s32SetMute;
    RK_S32      s32SetFadeRate;
    RK_S32      s32SetTrackMode;
    RK_S32      s32GetVolume;
    RK_S32      s32GetMute;
    RK_S32      s32GetTrackMode;
    RK_S32      s32LoopbackMode;
    RK_S32      s32DevFd;
    RK_S32      s32DataReadEnable;
    RK_S32      s32AedEnable;
    RK_S32      s32BcdEnable;
    RK_S32      s32BuzEnable;
    RK_S32      s32GbsEnable;
    RK_S32      s32AedLoudCount;
    RK_S32      s32BcdCount;
    RK_S32      s32BuzCount;
    RK_S32      s32GbsCount;
    RK_S32      s32VqeGapMs;
    RK_S32      s32VqeEnable;
    RK_S32      s32DumpAlgo;
    const char *pVqeCfgPath;
} TEST_AI_CTX_S;

static AUDIO_SOUND_MODE_E ai_find_sound_mode(RK_S32 ch) {
    AUDIO_SOUND_MODE_E channel = AUDIO_SOUND_MODE_BUTT;
    switch (ch) {
      case 1:
        channel = AUDIO_SOUND_MODE_MONO;
        break;
      case 2:
        channel = AUDIO_SOUND_MODE_STEREO;
        break;
      default:
        RK_LOGE("channel = %d not support", ch);
        return AUDIO_SOUND_MODE_BUTT;
    }

    return channel;
}

static AUDIO_BIT_WIDTH_E ai_find_bit_width(RK_S32 bit) {
    AUDIO_BIT_WIDTH_E bitWidth = AUDIO_BIT_WIDTH_BUTT;
    switch (bit) {
      case 8:
        bitWidth = AUDIO_BIT_WIDTH_8;
        break;
      case 16:
        bitWidth = AUDIO_BIT_WIDTH_16;
        break;
      case 24:
        bitWidth = AUDIO_BIT_WIDTH_24;
        break;
      default:
        RK_LOGE("bitwidth(%d) not support", bit);
        return AUDIO_BIT_WIDTH_BUTT;
    }

    return bitWidth;
}

RK_S32 test_ai_poll_event(RK_S32 timeoutMsec, RK_S32 fd) {
    RK_S32 num_fds = 1;
    struct pollfd pollFds[num_fds];
    RK_S32 ret = 0;

    RK_ASSERT(fd > 0);
    memset(pollFds, 0, sizeof(pollFds));
    pollFds[0].fd = fd;
    pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

    ret = poll(pollFds, num_fds, timeoutMsec);

    if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP))) {
        RK_LOGE("fd:%d polled error", fd);
        return -1;
    }

    return ret;
}

RK_S32 test_open_device_ai(TEST_AI_CTX_S *ctx) {
    AUDIO_DEV aiDevId = ctx->s32DevId;
    AUDIO_SOUND_MODE_E soundMode;

    AIO_ATTR_S aiAttr;
    RK_S32 result;
    memset(&aiAttr, 0, sizeof(AIO_ATTR_S));

    if (ctx->chCardName) {
        snprintf(reinterpret_cast<char *>(aiAttr.u8CardName),
                 sizeof(aiAttr.u8CardName), "%s", ctx->chCardName);
    }

    aiAttr.soundCard.channels = ctx->s32DeviceChannel;
    aiAttr.soundCard.sampleRate = ctx->s32DeviceSampleRate;
    aiAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

    AUDIO_BIT_WIDTH_E bitWidth = ai_find_bit_width(ctx->s32BitWidth);
    if (bitWidth == AUDIO_BIT_WIDTH_BUTT) {
        goto __FAILED;
    }
    aiAttr.enBitwidth = bitWidth;
    aiAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)ctx->s32SampleRate;
    soundMode = ai_find_sound_mode(ctx->s32Channel);
    if (soundMode == AUDIO_SOUND_MODE_BUTT) {
        goto __FAILED;
    }
    aiAttr.enSoundmode = soundMode;
    aiAttr.u32FrmNum = ctx->s32FrameNumber;
    aiAttr.u32PtNumPerFrm = ctx->s32FrameLength;

    if (!ctx->s32DataReadEnable)
        aiAttr.u32EXFlag = 0;
    else
        aiAttr.u32EXFlag = 1;

    aiAttr.u32ChnCnt = 2;

    result = RK_MPI_AI_SetPubAttr(aiDevId, &aiAttr);
    if (result != 0) {
        RK_LOGE("ai set attr fail, reason = %d", result);
        goto __FAILED;
    }

    result = RK_MPI_AI_Enable(aiDevId);
    if (result != 0) {
        RK_LOGE("ai enable fail, reason = %d", result);
        goto __FAILED;
    }

    return RK_SUCCESS;
__FAILED:
    return RK_FAILURE;
}

RK_S32 test_init_ai_data_read(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32DataReadEnable) {
        result = RK_MPI_AI_EnableDataRead(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: RK_MPI_AI_EnableDataRead(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

RK_S32 test_init_ai_aed(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32AedEnable) {
        AI_AED_CONFIG_S stAiAedConfig, stAiAedConfig2;

        stAiAedConfig.fSnrDB = 10.0f;
        stAiAedConfig.fLsdDB = -25.0f;
        stAiAedConfig.s32Policy = 1;

        result = RK_MPI_AI_SetAedAttr(params->s32DevId, params->s32ChnIndex, &stAiAedConfig);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetAedAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = RK_MPI_AI_GetAedAttr(params->s32DevId, params->s32ChnIndex, &stAiAedConfig2);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetAedAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = memcmp(&stAiAedConfig, &stAiAedConfig2, sizeof(AI_AED_CONFIG_S));
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: set/get aed config is different: %d", __FUNCTION__, result);
            return result;
        }

        result = RK_MPI_AI_EnableAed(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: EnableAed(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 test_init_ai_aed2(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32AedEnable) {
        AI_AED_CONFIG_S stAiAedConfig, stAiAedConfig2;

        stAiAedConfig.fSnrDB = 10.0f;
        stAiAedConfig.fLsdDB = -25.0f;
        stAiAedConfig.s32Policy = 2;

        result = RK_MPI_AI_SetAedAttr(params->s32DevId, params->s32ChnIndex, &stAiAedConfig);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetAedAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = RK_MPI_AI_GetAedAttr(params->s32DevId, params->s32ChnIndex, &stAiAedConfig2);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetAedAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = memcmp(&stAiAedConfig, &stAiAedConfig2, sizeof(AI_AED_CONFIG_S));
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: set/get aed config is different: %d", __FUNCTION__, result);
            return result;
        }

        result = RK_MPI_AI_EnableAed(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: EnableAed(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 test_init_ai_bcd(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32BcdEnable) {
        AI_BCD_CONFIG_S stAiBcdConfig, stAiBcdConfig2;

        stAiBcdConfig.mFrameLen = 100;

        result = RK_MPI_AI_SetBcdAttr(params->s32DevId, params->s32ChnIndex, &stAiBcdConfig);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetBcdAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = RK_MPI_AI_GetBcdAttr(params->s32DevId, params->s32ChnIndex, &stAiBcdConfig2);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetBcdAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = memcmp(&stAiBcdConfig, &stAiBcdConfig2, sizeof(AI_BCD_CONFIG_S));
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: set/get aed config is different: %d", __FUNCTION__, result);
            return result;
        }

        result = RK_MPI_AI_EnableBcd(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: EnableBcd(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 test_init_ai_bcd2(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32BcdEnable) {
        AI_BCD_CONFIG_S stAiBcdConfig, stAiBcdConfig2;

        stAiBcdConfig.mFrameLen = 100;

        result = RK_MPI_AI_SetBcdAttr(params->s32DevId, params->s32ChnIndex, &stAiBcdConfig);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetBcdAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = RK_MPI_AI_GetBcdAttr(params->s32DevId, params->s32ChnIndex, &stAiBcdConfig2);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetBcdAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = memcmp(&stAiBcdConfig, &stAiBcdConfig2, sizeof(AI_BCD_CONFIG_S));
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: set/get aed config is different: %d", __FUNCTION__, result);
            return result;
        }

        result = RK_MPI_AI_EnableBcd(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: EnableBcd(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 test_init_ai_buz(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32BuzEnable) {
        AI_BUZ_CONFIG_S stAiBuzConfig, stAiBuzConfig2;

        stAiBuzConfig.mFrameLen = 100;

        result = RK_MPI_AI_SetBuzAttr(params->s32DevId, params->s32ChnIndex, &stAiBuzConfig);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetBuzAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = RK_MPI_AI_GetBuzAttr(params->s32DevId, params->s32ChnIndex, &stAiBuzConfig2);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetBuzAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = memcmp(&stAiBuzConfig, &stAiBuzConfig2, sizeof(AI_BUZ_CONFIG_S));
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: set/get aed config is different: %d", __FUNCTION__, result);
            return result;
        }

        result = RK_MPI_AI_EnableBuz(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: EnableBuz(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 test_init_ai_buz2(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32BuzEnable) {
        AI_BUZ_CONFIG_S stAiBuzConfig, stAiBuzConfig2;

        stAiBuzConfig.mFrameLen = 100;

        result = RK_MPI_AI_SetBuzAttr(params->s32DevId, params->s32ChnIndex, &stAiBuzConfig);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetBuzAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = RK_MPI_AI_GetBuzAttr(params->s32DevId, params->s32ChnIndex, &stAiBuzConfig2);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetBuzAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = memcmp(&stAiBuzConfig, &stAiBuzConfig2, sizeof(AI_BUZ_CONFIG_S));
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: set/get aed config is different: %d", __FUNCTION__, result);
            return result;
        }

        result = RK_MPI_AI_EnableBuz(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: EnableBuz(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 test_init_ai_gbs(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32GbsEnable) {
        AI_GBS_CONFIG_S stAiGbsConfig, stAiGbsConfig2;

        stAiGbsConfig.mFrameLen = 30;

        result = RK_MPI_AI_SetGbsAttr(params->s32DevId, params->s32ChnIndex, &stAiGbsConfig);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetGbsAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = RK_MPI_AI_GetGbsAttr(params->s32DevId, params->s32ChnIndex, &stAiGbsConfig2);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetGbsAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = memcmp(&stAiGbsConfig, &stAiGbsConfig2, sizeof(AI_GBS_CONFIG_S));
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: set/get aed config is different: %d", __FUNCTION__, result);
            return result;
        }

        result = RK_MPI_AI_EnableGbs(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: EnableGbs(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 test_init_ai_gbs2(TEST_AI_CTX_S *params) {
    RK_S32 result;

    if (params->s32GbsEnable) {
        AI_GBS_CONFIG_S stAiGbsConfig, stAiGbsConfig2;

        stAiGbsConfig.mFrameLen = 30;

        result = RK_MPI_AI_SetGbsAttr(params->s32DevId, params->s32ChnIndex, &stAiGbsConfig);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetGbsAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = RK_MPI_AI_GetGbsAttr(params->s32DevId, params->s32ChnIndex, &stAiGbsConfig2);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: SetGbsAttr(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }

        result = memcmp(&stAiGbsConfig, &stAiGbsConfig2, sizeof(AI_GBS_CONFIG_S));
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: set/get aed config is different: %d", __FUNCTION__, result);
            return result;
        }

        result = RK_MPI_AI_EnableGbs(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: EnableGbs(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
    }

    return RK_SUCCESS;
}

RK_S32 test_init_ai_vqe(TEST_AI_CTX_S *params) {
    AI_VQE_CONFIG_S stAiVqeConfig, stAiVqeConfig2;
    RK_S32 result;

    if (params->s32VqeEnable == 0)
        return RK_SUCCESS;

    // Need to config enCfgMode to VQE attr even the VQE is not enabled
    memset(&stAiVqeConfig, 0, sizeof(AI_VQE_CONFIG_S));
    if (params->pVqeCfgPath != RK_NULL) {
        stAiVqeConfig.enCfgMode = AIO_VQE_CONFIG_LOAD_FILE;
        memcpy(stAiVqeConfig.aCfgFile, params->pVqeCfgPath, strlen(params->pVqeCfgPath));
    }

    if (params->s32VqeGapMs != 16 && params->s32VqeGapMs != 10) {
        RK_LOGE("Invalid gap: %d, just supports 16ms or 10ms for AI VQE", params->s32VqeGapMs);
        return RK_FAILURE;
    }

    stAiVqeConfig.s32WorkSampleRate = params->s32SampleRate;
    stAiVqeConfig.s32FrameSample = params->s32SampleRate * params->s32VqeGapMs / 1000;
    result = RK_MPI_AI_SetVqeAttr(params->s32DevId, params->s32ChnIndex, 0, 0, &stAiVqeConfig);
    if (result != RK_SUCCESS) {
        RK_LOGE("%s: SetVqeAttr(%d,%d) failed with %#x",
            __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
        return result;
    }

    result = RK_MPI_AI_GetVqeAttr(params->s32DevId, params->s32ChnIndex, &stAiVqeConfig2);
    if (result != RK_SUCCESS) {
        RK_LOGE("%s: SetVqeAttr(%d,%d) failed with %#x",
            __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
        return result;
    }

    result = memcmp(&stAiVqeConfig, &stAiVqeConfig2, sizeof(AI_VQE_CONFIG_S));
    if (result != RK_SUCCESS) {
        RK_LOGE("%s: set/get vqe config is different: %d", __FUNCTION__, result);
        return result;
    }

    result = RK_MPI_AI_EnableVqe(params->s32DevId, params->s32ChnIndex);
    if (result != RK_SUCCESS) {
        RK_LOGE("%s: EnableVqe(%d,%d) failed with %#x",
            __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
        return result;
    }

    return RK_SUCCESS;
}

RK_S32 test_init_mpi_ai(TEST_AI_CTX_S *params) {
    RK_S32 result;

    result = test_init_ai_data_read(params);
    if (result != 0) {
        RK_LOGE("ai file read init fail, reason = %x, aiChn = %d", result, params->s32ChnIndex);
        return RK_FAILURE;
    }

    result = test_init_ai_aed(params);
    if (result != 0) {
        RK_LOGE("ai aed init fail, reason = %x, aiChn = %d", result, params->s32ChnIndex);
        return RK_FAILURE;
    }

    result = test_init_ai_bcd(params);
    if (result != 0) {
        RK_LOGE("ai bcd init fail, reason = %x, aiChn = %d", result, params->s32ChnIndex);
        return RK_FAILURE;
    }

    result = test_init_ai_buz(params);
    if (result != 0) {
        RK_LOGE("ai buz init fail, reason = %x, aiChn = %d", result, params->s32ChnIndex);
        return RK_FAILURE;
    }

    result = test_init_ai_gbs(params);
    if (result != 0) {
        RK_LOGE("ai gbs init fail, reason = %x, aiChn = %d", result, params->s32ChnIndex);
        return RK_FAILURE;
    }

    result = test_init_ai_vqe(params);
    if (result != 0) {
        RK_LOGE("ai vqe init fail, reason = %x, aiChn = %d", result, params->s32ChnIndex);
        return RK_FAILURE;
    }

    result =  RK_MPI_AI_EnableChn(params->s32DevId, params->s32ChnIndex);
    if (result != 0) {
        RK_LOGE("ai enable channel fail, aiChn = %d, reason = %x", params->s32ChnIndex, result);
        return RK_FAILURE;
    }

#if TEST_AI_WITH_FD
    // open fd immediate after enable chn will be better.
    params->s32DevFd = RK_MPI_AI_GetFd(params->s32DevId, params->s32ChnIndex);
    RK_LOGI("ai (devId: %d, chnId: %d), selectFd:%d", params->s32DevId, params->s32ChnIndex, params->s32DevFd);
#endif

    RK_BOOL needResample = (params->s32DeviceSampleRate != params->s32SampleRate) ? RK_TRUE : RK_FALSE;

    if (needResample == RK_TRUE) {
        RK_LOGI("need to resample %d -> %d", params->s32DeviceSampleRate, params->s32SampleRate);
        result = RK_MPI_AI_EnableReSmp(params->s32DevId, params->s32ChnIndex,
                                      (AUDIO_SAMPLE_RATE_E)params->s32SampleRate);
        if (result != 0) {
            RK_LOGE("ai enable channel fail, reason = %x, aiChn = %d", result, params->s32ChnIndex);
            return RK_FAILURE;
        }
    }

    RK_LOGI("Set volume curve type: %d", params->s32SetVolumeCurve);
    if ((params->s32SetVolumeCurve == AUDIO_CURVE_LOGARITHM) ||
        (params->s32SetVolumeCurve == AUDIO_CURVE_CUSTOMIZE)) {
        AUDIO_VOLUME_CURVE_S volumeCurve;

        volumeCurve.enCurveType = (AUDIO_VOLUME_CURVE_E)params->s32SetVolumeCurve;
        volumeCurve.s32Resolution = 101;
        volumeCurve.fMinDB = -51.0f;
        volumeCurve.fMaxDB = 0.0f;
        volumeCurve.pCurveTable = RK_NULL; // here none means using default logarithm curve by default.
        if (volumeCurve.enCurveType == AUDIO_CURVE_CUSTOMIZE) {
            volumeCurve.pCurveTable = (RK_U32 *)calloc(volumeCurve.s32Resolution, sizeof(RK_U32));
            RK_ASSERT(volumeCurve.pCurveTable != RK_NULL);
            // TODO: fill your customize table of volume curve folllowing to:
            // volumeCurve.pCurveTable[0, resolution-1]
        }
        RK_MPI_AI_SetVolumeCurve(params->s32DevId, &volumeCurve);
    }

    return RK_SUCCESS;
}

RK_S32 test_deinit_mpi_ai(TEST_AI_CTX_S *params) {
    RK_S32 result;

    RK_MPI_AI_DisableReSmp(params->s32DevId, params->s32ChnIndex);
    if (params->s32BuzEnable) {
        result = RK_MPI_AI_DisableBuz(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: RK_MPI_AI_DisableBuz(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
        params->s32BuzEnable = 0;
    }

    if (params->s32BcdEnable) {
        result = RK_MPI_AI_DisableBcd(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: RK_MPI_AI_DisableBcd(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
        params->s32BcdEnable = 0;
    }

    if (params->s32AedEnable) {
        result = RK_MPI_AI_DisableAed(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: RK_MPI_AI_DisableAed(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
        params->s32AedEnable = 0;
    }

    if (params->s32GbsEnable) {
        result = RK_MPI_AI_DisableGbs(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: RK_MPI_AI_DisableGbs(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
        params->s32GbsEnable = 0;
    }

    if (params->s32VqeEnable) {
        result = RK_MPI_AI_DisableVqe(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: RK_MPI_AI_DisableVqe(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
        params->s32VqeEnable = 0;
    }

    if (params->s32DataReadEnable) {
        result = RK_MPI_AI_DisableDataRead(params->s32DevId, params->s32ChnIndex);
        if (result != RK_SUCCESS) {
            RK_LOGE("%s: RK_MPI_AI_DisableDataRead(%d,%d) failed with %#x",
                __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
            return result;
        }
        params->s32DataReadEnable = 0;
    }

    result = RK_MPI_AI_DisableChn(params->s32DevId, params->s32ChnIndex);
    if (result != 0) {
        RK_LOGE("ai disable channel fail, reason = %d", result);
        return RK_FAILURE;
    }

    result =  RK_MPI_AI_Disable(params->s32DevId);
    if (result != 0) {
        RK_LOGE("ai disable fail, reason = %d", result);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

static AUDIO_BIT_WIDTH_E find_bit_width(RK_S32 bit) {
    AUDIO_BIT_WIDTH_E bitWidth = AUDIO_BIT_WIDTH_BUTT;
    switch (bit) {
      case 8:
        bitWidth = AUDIO_BIT_WIDTH_8;
        break;
      case 16:
        bitWidth = AUDIO_BIT_WIDTH_16;
        break;
      case 24:
        bitWidth = AUDIO_BIT_WIDTH_24;
        break;
      default:
        RK_LOGE("bitwidth(%d) not support", bit);
        return AUDIO_BIT_WIDTH_BUTT;
    }

    return bitWidth;
}

static AUDIO_SOUND_MODE_E find_sound_mode(RK_S32 ch) {
    AUDIO_SOUND_MODE_E channel = AUDIO_SOUND_MODE_BUTT;
    switch (ch) {
      case 1:
        channel = AUDIO_SOUND_MODE_MONO;
        break;
      case 2:
        channel = AUDIO_SOUND_MODE_STEREO;
        break;
      default:
        RK_LOGE("channel = %d not support", ch);
        return AUDIO_SOUND_MODE_BUTT;
    }

    return channel;
}

void* sendDataThread(void * ptr) {
    TEST_AI_CTX_S *params = reinterpret_cast<TEST_AI_CTX_S *>(ptr);

    RK_S32 result = 0;
    RK_S32 s32MilliSec = -1;
    RK_S32 bufferLen = 1024;
    RK_S32 readLen = bufferLen;
    RK_S32 frames = 0;
    AUDIO_FRAME_S sendFrame;
    RK_U8  *srcData = RK_NULL;
    RK_U8 *tmpData = RK_NULL;
    RK_S32 size = 0;
    RK_U64 timeStamp = 0;
    FILE *file = RK_NULL;

    file = fopen(params->srcFilePath, "rb");
    if (file == RK_NULL) {
        RK_LOGE("open input file %s failed because %s.", params->srcFilePath, strerror(errno));
        goto __EXIT;
    }

    srcData = reinterpret_cast<RK_U8 *>(calloc(bufferLen, sizeof(RK_U8)));
    if (!srcData) {
        RK_LOGE("malloc pstVdecCtx falied");
        goto __EXIT;
    }

    while (!gAiExit) {
        size = fread(srcData, 1, readLen, file);

        sendFrame.u32Len = size;
        sendFrame.u64TimeStamp = timeStamp++;
        sendFrame.enBitWidth = find_bit_width(params->s32BitWidth);
        sendFrame.enSoundMode = find_sound_mode(params->s32DeviceChannel);
        sendFrame.bBypassMbBlk = RK_FALSE;

        MB_EXT_CONFIG_S extConfig;
        memset(&extConfig, 0, sizeof(extConfig));
        extConfig.pOpaque = srcData;
        extConfig.pu8VirAddr = srcData;
        extConfig.u64Size = size;
        RK_MPI_SYS_CreateMB(&(sendFrame.pMbBlk), &extConfig);
__RETRY:
        result = RK_MPI_AI_SendFrame(params->s32DevId, params->s32ChnIndex, &sendFrame, s32MilliSec);
        if (result < 0) {
            RK_LOGE("send frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                result, sendFrame.u64TimeStamp, s32MilliSec);
            goto __RETRY;
        }
        RK_MPI_MB_ReleaseMB(sendFrame.pMbBlk);

        if (size <= 0) {
            RK_LOGI("eof");
            break;
        }
    }

    if (gAiExit) {
        sendFrame.u32Len = 0;
        sendFrame.u64TimeStamp = timeStamp++;
        sendFrame.enBitWidth = find_bit_width(params->s32BitWidth);
        sendFrame.enSoundMode = find_sound_mode(params->s32DeviceChannel);
        sendFrame.bBypassMbBlk = RK_FALSE;

        MB_EXT_CONFIG_S extConfig;
        memset(&extConfig, 0, sizeof(extConfig));
        extConfig.pOpaque = srcData;
        extConfig.pu8VirAddr = srcData;
        extConfig.u64Size = 0;
        RK_MPI_SYS_CreateMB(&(sendFrame.pMbBlk), &extConfig);
        RK_LOGI("ai send frame exit");
        result = RK_MPI_AI_SendFrame(params->s32DevId, params->s32ChnIndex, &sendFrame, s32MilliSec);
        if (result < 0) {
            RK_LOGE("send frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                result, sendFrame.u64TimeStamp, s32MilliSec);
        }

        RK_MPI_MB_ReleaseMB(sendFrame.pMbBlk);
    }

__EXIT:
    if (file) {
        fclose(file);
        file = RK_NULL;
    }

    if (srcData)
        free(srcData);

    if (tmpData)
        free(tmpData);

    pthread_exit(NULL);
    return RK_NULL;
}

void* getDataThread(void * ptr) {
    TEST_AI_CTX_S *params = reinterpret_cast<TEST_AI_CTX_S *>(ptr);

    RK_S32 result = 0;
    RK_S32 s32MilliSec = -1;
    AUDIO_FRAME_S getFrame;
    FILE *fp_aed, *fp_bcd, *fp_buz, *fp_gbs;
    RK_U8  *srcData = RK_NULL;
    RK_S16 *buf_aed, *buf_bcd, *buf_buz, *buf_gbs;
    RK_S32 s32AiAlgoFrames = 0;
    RK_U32 aed_count = 0, aed_flag = 0;
    RK_U32 bcd_count = 0, bcd_flag = 0;
    RK_U32 buz_count = 0, buz_flag = 0;
    RK_U32 gbs_count = 0, gbs_flag = 0;

    if (params->dstFilePath) {
        AUDIO_SAVE_FILE_INFO_S save;
        save.bCfg = RK_TRUE;
        save.u32FileSize = 1024;
        snprintf(save.aFilePath, sizeof(save.aFilePath), "%s", params->dstFilePath);
        snprintf(save.aFileName, sizeof(save.aFileName), "%s", "cap_out.pcm");
        RK_MPI_AI_SaveFile(params->s32DevId, params->s32ChnIndex, &save);
    }

    if (params->s32SampleRate == 16000)
        s32AiAlgoFrames = AI_ALGO_FRAMES;
    else if (params->s32SampleRate == 8000)
        s32AiAlgoFrames = (AI_ALGO_FRAMES >> 1);

    /* Do not dump if s32AiAlgoFrames is invalid */
    if (s32AiAlgoFrames == 0)
        params->s32DumpAlgo = 0;

    if (params->s32DumpAlgo) {
        buf_aed = (RK_S16 *)calloc(s32AiAlgoFrames * 2 * sizeof(RK_S16), 1);
        RK_ASSERT(buf_aed != RK_NULL);
        fp_aed = fopen("/tmp/cap_aed_2ch.pcm", "wb");
        RK_ASSERT(fp_aed != RK_NULL);

        buf_bcd = (RK_S16 *)calloc(s32AiAlgoFrames * 1 * sizeof(RK_S16), 1);
        RK_ASSERT(buf_bcd != RK_NULL);
        fp_bcd = fopen("/tmp/cap_bcd_1ch.pcm", "wb");
        RK_ASSERT(fp_bcd != RK_NULL);

        buf_buz = (RK_S16 *)calloc(s32AiAlgoFrames * 1 * sizeof(RK_S16), 1);
        RK_ASSERT(buf_aed != RK_NULL);
        fp_buz = fopen("/tmp/cap_buz_1ch.pcm", "wb");
        RK_ASSERT(fp_buz != RK_NULL);

        buf_gbs = (RK_S16 *)calloc(s32AiAlgoFrames * 1 * sizeof(RK_S16), 1);
        RK_ASSERT(buf_aed != RK_NULL);
        fp_gbs = fopen("/tmp/cap_gbs_1ch.pcm", "wb");
        RK_ASSERT(fp_gbs != RK_NULL);
    }

    while (!gAiExit || params->s32DataReadEnable) {
#if TEST_AI_WITH_FD
        test_ai_poll_event(-1, params->s32DevFd);
#endif
        if (params->s32AedEnable == 2) {
            if ((aed_count + 1) % 50 == 0) {
                result = RK_MPI_AI_DisableAed(params->s32DevId, params->s32ChnIndex);
                if (result != RK_SUCCESS) {
                    RK_LOGE("%s: RK_MPI_AI_DisableAed(%d,%d) failed with %#x",
                        __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
                    return RK_NULL;
                }
                if (aed_flag) {
                    RK_LOGI("%s - aed_count=%ld test_init_ai_aed\n", __func__, aed_count);
                    test_init_ai_aed(params);
                    aed_flag = 0;
                } else {
                    RK_LOGI("%s - aed_count=%ld test_init_ai_aed2\n", __func__, aed_count);
                    test_init_ai_aed2(params);
                    aed_flag = 1;
                }
            }
            aed_count++;
        }

        if (params->s32BcdEnable == 2) {
            if ((bcd_count + 1) % 50 == 0) {
                result = RK_MPI_AI_DisableBcd(params->s32DevId, params->s32ChnIndex);
                if (result != RK_SUCCESS) {
                    RK_LOGE("%s: RK_MPI_AI_DisableBcd(%d,%d) failed with %#x",
                        __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
                    return RK_NULL;
                }
                if (bcd_flag) {
                    RK_LOGI("%s - bcd_count=%ld test_init_ai_bcd\n", __func__, bcd_count);
                    test_init_ai_bcd(params);
                    bcd_flag = 0;
                } else {
                    RK_LOGI("%s - bcd_count=%ld test_init_ai_bcd2\n", __func__, bcd_count);
                    test_init_ai_bcd2(params);
                    bcd_flag = 1;
                }
            }
            bcd_count++;
        }

        if (params->s32BuzEnable == 2) {
            if ((buz_count + 1) % 50 == 0) {
                result = RK_MPI_AI_DisableBuz(params->s32DevId, params->s32ChnIndex);
                if (result != RK_SUCCESS) {
                    RK_LOGE("%s: RK_MPI_AI_DisableBuz(%d,%d) failed with %#x",
                        __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
                    return RK_NULL;
                }
                if (buz_flag) {
                    RK_LOGI("%s - buz_count=%ld test_init_ai_buz\n", __func__, buz_count);
                    test_init_ai_buz(params);
                    buz_flag = 0;
                } else {
                    RK_LOGI("%s - buz_count=%ld test_init_ai_buz2\n", __func__, buz_count);
                    test_init_ai_buz2(params);
                    buz_flag = 1;
                }
            }
            buz_count++;
        }

        if (params->s32GbsEnable == 2) {
            if ((gbs_count + 1) % 50 == 0) {
                result = RK_MPI_AI_DisableGbs(params->s32DevId, params->s32ChnIndex);
                if (result != RK_SUCCESS) {
                    RK_LOGE("%s: RK_MPI_AI_DisableGbs(%d,%d) failed with %#x",
                        __FUNCTION__, params->s32DevId, params->s32ChnIndex, result);
                    return RK_NULL;
                }
                if (gbs_flag) {
                    RK_LOGI("%s - gbs_count=%ld test_init_ai_gbs\n", __func__, gbs_count);
                    test_init_ai_gbs(params);
                    gbs_flag = 0;
                } else {
                    RK_LOGI("%s - gbs_count=%ld test_init_ai_gbs2\n", __func__, gbs_count);
                    test_init_ai_gbs2(params);
                    gbs_flag = 1;
                }
            }
            gbs_count++;
        }

        result = RK_MPI_AI_GetFrame(params->s32DevId, params->s32ChnIndex, &getFrame, RK_NULL, s32MilliSec);
        if (result == 0) {
            void* data = RK_MPI_MB_Handle2VirAddr(getFrame.pMbBlk);
            RK_LOGV("data = %p, len = %d", data, getFrame.u32Len);
            if (getFrame.u32Len <= 0) {
                RK_LOGD("get ai frame end");
                break;
            }

            RK_MPI_AI_ReleaseFrame(params->s32DevId, params->s32ChnIndex, &getFrame, RK_NULL);
        }

        //dump results of SED(AED/BCD) modules
        if (params->s32AedEnable) {
            AI_AED_RESULT_S aed_result;

            memset(&aed_result, 0, sizeof(aed_result));
            result = RK_MPI_AI_GetAedResult(params->s32DevId, params->s32ChnIndex, &aed_result);
            if (result == 0) {
                if (aed_result.bAcousticEventDetected)
                    RK_LOGI("AED Result: AcousticEvent:%d",
                            aed_result.bAcousticEventDetected);
                if (aed_result.bLoudSoundDetected) {
                    params->s32AedLoudCount++;
                    RK_LOGI("AED Result: LoudSound:%d",
                            aed_result.bLoudSoundDetected);
                }

                if (aed_result.bLoudSoundDetected)
                    RK_LOGI("AED Result: LoudSound Volume Result:%f db",
                            aed_result.lsdResult);
            }
            if (params->s32DumpAlgo) {
                for (RK_S32 i = 0; i < s32AiAlgoFrames; i++) {
                    *(buf_aed + 2 * i + 0) = 10000 * aed_result.bAcousticEventDetected;
                    *(buf_aed + 2 * i + 1) = 10000 * aed_result.bLoudSoundDetected;
                }
                fwrite(buf_aed, s32AiAlgoFrames * 2 * sizeof(RK_S16), 1, fp_aed);
            }
        }

        if (params->s32BcdEnable) {
            AI_BCD_RESULT_S bcd_result;

            memset(&bcd_result, 0, sizeof(bcd_result));
            result = RK_MPI_AI_GetBcdResult(params->s32DevId, params->s32ChnIndex, &bcd_result);
            if (result == 0 && bcd_result.bBabyCry) {
                params->s32BcdCount++;
                RK_LOGI("BCD Result: BabyCry:%d", bcd_result.bBabyCry);
            }
            if (params->s32DumpAlgo) {
                for (RK_S32 i = 0; i < s32AiAlgoFrames; i++) {
                    *(buf_bcd + 1 * i) = 10000 * bcd_result.bBabyCry;
                }
                fwrite(buf_bcd, s32AiAlgoFrames * 1 * sizeof(RK_S16), 1, fp_bcd);
            }
        }

        if (params->s32BuzEnable) {
            AI_BUZ_RESULT_S buz_result;

            memset(&buz_result, 0, sizeof(buz_result));
            result = RK_MPI_AI_GetBuzResult(params->s32DevId, params->s32ChnIndex, &buz_result);
            if (result == 0 && buz_result.bBuzz) {
                params->s32BuzCount++;
                RK_LOGI("BUZ Result: Buzz:%d", buz_result.bBuzz);
            }
            if (params->s32DumpAlgo) {
                for (RK_S32 i = 0; i < s32AiAlgoFrames; i++) {
                    *(buf_buz + 1 * i) = 10000 * buz_result.bBuzz;
                }
                fwrite(buf_buz, s32AiAlgoFrames * 1 * sizeof(RK_S16), 1, fp_buz);
            }
        }

        if (params->s32GbsEnable) {
            AI_GBS_RESULT_S gbs_result;

            memset(&gbs_result, 0, sizeof(gbs_result));
            result = RK_MPI_AI_GetGbsResult(params->s32DevId, params->s32ChnIndex, &gbs_result);
            if (result == 0 && gbs_result.bGbs) {
                params->s32GbsCount++;
                RK_LOGI("GBS Result: Gbs:%d", gbs_result.bGbs);
            }
            if (params->s32DumpAlgo) {
                for (RK_S32 i = 0; i < s32AiAlgoFrames; i++) {
                    *(buf_gbs + 1 * i) = 10000 * gbs_result.bGbs;
                }
                fwrite(buf_gbs, s32AiAlgoFrames * 1 * sizeof(RK_S16), 1, fp_gbs);
            }
        }
    }

    if (params->s32DumpAlgo) {
        if (buf_aed)
            free(buf_aed);
        if (buf_bcd)
            free(buf_bcd);
        if (buf_buz)
            free(buf_buz);
        if (buf_gbs)
            free(buf_gbs);

        if (fp_aed)
            fclose(fp_aed);
        if (fp_bcd)
            fclose(fp_bcd);
        if (fp_buz)
            fclose(fp_buz);
        if (fp_gbs)
            fclose(fp_gbs);
    }

    pthread_exit(NULL);
    return RK_NULL;
}

void* commandThread(void * ptr) {
    TEST_AI_CTX_S *params = reinterpret_cast<TEST_AI_CTX_S *>(ptr);

    AUDIO_FADE_S aFade;
    aFade.bFade = RK_FALSE;
    aFade.enFadeOutRate = (AUDIO_FADE_RATE_E)params->s32SetFadeRate;
    aFade.enFadeInRate = (AUDIO_FADE_RATE_E)params->s32SetFadeRate;
    RK_BOOL mute = (params->s32SetMute == 0) ? RK_FALSE : RK_TRUE;
    RK_LOGI("test info : mute = %d, volume = %d", mute, params->s32SetVolume);
    RK_MPI_AI_SetMute(params->s32DevId, mute, &aFade);
    RK_MPI_AI_SetVolume(params->s32DevId, params->s32SetVolume);
    if (params->s32SetTrackMode) {
        RK_LOGI("test info : set track mode = %d", params->s32SetTrackMode);
        RK_MPI_AI_SetTrackMode(params->s32DevId, (AUDIO_TRACK_MODE_E)params->s32SetTrackMode);
        params->s32SetTrackMode = 0;
    }

    if (params->s32GetVolume) {
        RK_S32 volume = 0;
        RK_MPI_AI_GetVolume(params->s32DevId, &volume);
        RK_LOGI("test info : get volume = %d", volume);
        params->s32GetVolume = 0;
    }

    if (params->s32GetMute) {
        RK_BOOL mute = RK_FALSE;
        AUDIO_FADE_S fade;
        RK_MPI_AI_GetMute(params->s32DevId, &mute, &fade);
        RK_LOGI("test info : is mute = %d", mute);
        params->s32GetMute = 0;
    }

    if (params->s32GetTrackMode) {
        AUDIO_TRACK_MODE_E trackMode;
        RK_MPI_AI_GetTrackMode(params->s32DevId, &trackMode);
        RK_LOGI("test info : get track mode = %d", trackMode);
        params->s32GetTrackMode = 0;
    }

    pthread_exit(NULL);
    return RK_NULL;
}

static RK_S32 test_set_channel_params_ai(TEST_AI_CTX_S *params) {
    AUDIO_DEV aiDevId = params->s32DevId;
    AI_CHN aiChn = params->s32ChnIndex;
    RK_S32 result = 0;
    AI_CHN_PARAM_S pstParams;

    memset(&pstParams, 0, sizeof(AI_CHN_PARAM_S));
    pstParams.enLoopbackMode = (AUDIO_LOOPBACK_MODE_E)params->s32LoopbackMode;
    pstParams.s32UsrFrmDepth = 4;
    result = RK_MPI_AI_SetChnParam(aiDevId, aiChn, &pstParams);
    if (result != RK_SUCCESS) {
        RK_LOGE("ai set channel params, aiChn = %d", aiChn);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

RK_S32 unit_test_mpi_ai(TEST_AI_CTX_S *ctx) {
    RK_S32 i = 0;
    TEST_AI_CTX_S params[AI_MAX_CHN_NUM];
    pthread_t tidSend[AI_MAX_CHN_NUM];
    pthread_t tidGet[AI_MAX_CHN_NUM];
    pthread_t tidComand[AI_MAX_CHN_NUM];
    RK_S32 result = RK_SUCCESS;

    if (test_open_device_ai(ctx) != RK_SUCCESS) {
        goto __FAILED;
    }

    for (i = 0; i < ctx->s32ChnNum; i++) {
        memcpy(&(params[i]), ctx, sizeof(TEST_AI_CTX_S));
        params[i].s32ChnIndex = i;
        params[i].s32DevFd = -1;
        result = test_set_channel_params_ai(&params[i]);
        if (result != RK_SUCCESS)
            goto __FAILED;
        result = test_init_mpi_ai(&params[i]);
        if (result != RK_SUCCESS)
            goto __FAILED;

        if (ctx->s32DataReadEnable)
            pthread_create(&tidSend[i], RK_NULL, sendDataThread, reinterpret_cast<void *>(&params[i]));
        pthread_create(&tidGet[i], RK_NULL, getDataThread, reinterpret_cast<void *>(&params[i]));
        pthread_create(&tidComand[i], RK_NULL, commandThread, reinterpret_cast<void *>(&params[i]));
    }

    for (i = 0; i < ctx->s32ChnNum; i++) {
        if (ctx->s32DataReadEnable)
            pthread_join(tidSend[i], RK_NULL);
        pthread_join(tidComand[i], RK_NULL);
        pthread_join(tidGet[i], RK_NULL);

        ctx->s32AedLoudCount    = params[i].s32AedLoudCount;
        ctx->s32BcdCount        = params[i].s32BcdCount;
        ctx->s32BuzCount        = params[i].s32BuzCount;
        ctx->s32GbsCount        = params[i].s32GbsCount;
        result = test_deinit_mpi_ai(&params[i]);
        if (result != RK_SUCCESS)
            goto __FAILED;
    }

    return RK_SUCCESS;
__FAILED:

    return RK_FAILURE;
}

static void mpi_ai_test_show_options(const TEST_AI_CTX_S *ctx) {
    RK_PRINT("cmd parse result:\n");
    RK_PRINT("input  file name      : %s\n", ctx->srcFilePath);
    RK_PRINT("output file name      : %s\n", ctx->dstFilePath);
    RK_PRINT("loop count            : %d\n", ctx->s32LoopCount);
    RK_PRINT("channel number        : %d\n", ctx->s32ChnNum);
    RK_PRINT("open sound rate       : %d\n", ctx->s32DeviceSampleRate);
    RK_PRINT("record data rate      : %d\n", ctx->s32SampleRate);
    RK_PRINT("sound card channel    : %d\n", ctx->s32DeviceChannel);
    RK_PRINT("output channel        : %d\n", ctx->s32Channel);
    RK_PRINT("bit_width             : %d\n", ctx->s32BitWidth);
    RK_PRINT("frame_number          : %d\n", ctx->s32FrameNumber);
    RK_PRINT("frame_length          : %d\n", ctx->s32FrameLength);
    RK_PRINT("sound card name       : %s\n", ctx->chCardName);
    RK_PRINT("device id             : %d\n", ctx->s32DevId);
    RK_PRINT("set volume curve      : %d\n", ctx->s32SetVolumeCurve);
    RK_PRINT("set volume            : %d\n", ctx->s32SetVolume);
    RK_PRINT("set mute              : %d\n", ctx->s32SetMute);
    RK_PRINT("set track_mode        : %d\n", ctx->s32SetTrackMode);
    RK_PRINT("get volume            : %d\n", ctx->s32GetVolume);
    RK_PRINT("get mute              : %d\n", ctx->s32GetMute);
    RK_PRINT("get track_mode        : %d\n", ctx->s32GetTrackMode);
    RK_PRINT("data read enable      : %d\n", ctx->s32DataReadEnable);
    RK_PRINT("aed enable            : %d\n", ctx->s32AedEnable);
    RK_PRINT("bcd enable            : %d\n", ctx->s32BcdEnable);
    RK_PRINT("buz enable            : %d\n", ctx->s32BuzEnable);
    RK_PRINT("vqe gap duration (ms) : %d\n", ctx->s32VqeGapMs);
    RK_PRINT("vqe enable            : %d\n", ctx->s32VqeEnable);
    RK_PRINT("vqe config file       : %s\n", ctx->pVqeCfgPath);
    RK_PRINT("dump algo pcm data    : %d\n", ctx->s32DumpAlgo);
}

static const char *const usages[] = {
    "./rk_mpi_ai_test [--device_rate rate] [--device_ch ch] [--out_rate rate] [--out_ch ch] "
                     "[--aed_enable] [--bcd_enable] [--buz_enable] [--vqe_enable] [--vqe_cfg]...",
    NULL,
};

int main(int argc, const char **argv) {
    RK_S32          i;
    RK_S32          s32Ret = 0;
    TEST_AI_CTX_S  *ctx;

    ctx = reinterpret_cast<TEST_AI_CTX_S *>(malloc(sizeof(TEST_AI_CTX_S)));
    memset(ctx, 0, sizeof(TEST_AI_CTX_S));

    ctx->srcFilePath        = RK_NULL;
    ctx->dstFilePath        = RK_NULL;
    ctx->s32LoopCount       = 1;
    ctx->s32ChnNum          = 1;
    ctx->s32BitWidth        = 16;
    ctx->s32FrameNumber     = 4;
    ctx->s32FrameLength     = 1024;
    ctx->chCardName         = RK_NULL;
    ctx->s32DevId           = 0;
    ctx->s32SetVolumeCurve  = 0;
    ctx->s32SetVolume       = 100;
    ctx->s32SetMute         = 0;
    ctx->s32SetTrackMode    = 0;
    ctx->s32SetFadeRate     = 0;
    ctx->s32GetVolume       = 0;
    ctx->s32GetMute         = 0;
    ctx->s32GetTrackMode    = 0;
    ctx->s32DataReadEnable  = 0;
    ctx->s32AedEnable       = 0;
    ctx->s32BcdEnable       = 0;
    ctx->s32BuzEnable       = 0;
    ctx->s32GbsEnable       = 0;
    ctx->s32VqeGapMs        = 16;
    ctx->s32VqeEnable       = 0;
    ctx->pVqeCfgPath        = RK_NULL;
    ctx->s32LoopbackMode    = AUDIO_LOOPBACK_NONE;
    ctx->s32DumpAlgo        = 0;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("basic options:"),

        OPT_INTEGER('\0', "device_rate", &(ctx->s32DeviceSampleRate),
                    "the sample rate of open sound card.  <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "device_ch", &(ctx->s32DeviceChannel),
                    "the number of sound card channels. <required>.", NULL, 0, 0),
        OPT_INTEGER('\0', "out_ch", &(ctx->s32Channel),
                    "the channels of out data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "out_rate", &(ctx->s32SampleRate),
                    "the sample rate of out data. <required>", NULL, 0, 0),
        OPT_STRING('i', "input", &(ctx->srcFilePath),
                    "input file name, e.g.(./ai). default(NULL).", NULL, 0, 0),
        OPT_STRING('o', "output", &(ctx->dstFilePath),
                    "output file name, e.g.(./ai). default(NULL).", NULL, 0, 0),
        OPT_INTEGER('n', "loop_count", &(ctx->s32LoopCount),
                    "loop running count. can be any count. default(1)", NULL, 0, 0),
        OPT_INTEGER('c', "channel_count", &(ctx->s32ChnNum),
                    "the count of adec channel. default(1).", NULL, 0, 0),
        OPT_INTEGER('\0', "bit", &(ctx->s32BitWidth),
                    "the bit width of open sound card, range(8, 16, 24), default(16)", NULL, 0, 0),
        OPT_INTEGER('\0', "frame_length", &(ctx->s32FrameLength),
                    "the bytes size of output frame", NULL, 0, 0),
        OPT_INTEGER('\0', "frame number", &(ctx->s32FrameNumber),
                    "the max frame num in buf", NULL, 0, 0),
        OPT_STRING('\0', "sound_card_name", &(ctx->chCardName),
                    "the sound name for open sound card, default(NULL)", NULL, 0, 0),
        OPT_INTEGER('\0', "set_volume_curve", &(ctx->s32SetVolumeCurve),
                    "set volume curve(builtin linear), 0:unset 1:linear 2:logarithm 3:customize. default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "set_volume", &(ctx->s32SetVolume),
                    "set volume test, range(0, 100), default(100)", NULL, 0, 0),
        OPT_INTEGER('\0', "set_mute", &(ctx->s32SetMute),
                    "set mute test, range(0, 1), default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "set_fade", &(ctx->s32SetFadeRate),
                    "set fade rate, range(0, 7), default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "set_track_mode", &(ctx->s32SetTrackMode),
                    "set track mode test, range(0:normal, 1:both_left, 2:both_right, 3:exchange, 4:mix,"
                    "5:left_mute, 6:right_mute, 7:both_mute, 8: only left, 9: only right, 10:out stereo), default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "get_volume", &(ctx->s32GetVolume),
                    "get volume test, range(0, 1), default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "get_mute", &(ctx->s32GetMute),
                    "get mute test, range(0, 1), default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "get_track_mode", &(ctx->s32GetTrackMode),
                    "get track mode test, range(0, 1), default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "data_read_enable", &(ctx->s32DataReadEnable),
                    "the data read enable, 0:disable 1:enable. default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "aed_enable", &(ctx->s32AedEnable),
                    "the aed enable, 0:disable 1:enable 2:reload test. default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "bcd_enable", &(ctx->s32BcdEnable),
                    "the bcd enable, 0:disable 1:enable. default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "buz_enable", &(ctx->s32BuzEnable),
                    "the buz enable, 0:disable 1:enable. default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "gbs_enable", &(ctx->s32GbsEnable),
                    "the gbs enable, 0:disable 1:enable. default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "vqe_gap", &(ctx->s32VqeGapMs),
                    "the vqe gap duration in milliseconds. only supports 16ms or 10ms. default(16).", NULL, 0, 0),
        OPT_INTEGER('\0', "vqe_enable", &(ctx->s32VqeEnable),
                    "the vqe enable, 0:disable 1:enable. default(0).", NULL, 0, 0),
        OPT_STRING('\0', "vqe_cfg", &(ctx->pVqeCfgPath),
                    "the vqe config file, default(NULL)", NULL, 0, 0),
        OPT_INTEGER('\0', "loopback_mode", &(ctx->s32LoopbackMode),
                    "configure the loopback mode during ai runtime", NULL, 0, 0),
        OPT_INTEGER('\0', "dump_algo", &(ctx->s32DumpAlgo),
                    "dump algorithm pcm data during ai runtime", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nselect a test case to run.",
                                 "\nuse --help for details.");

    argc = argparse_parse(&argparse, argc, argv);
    mpi_ai_test_show_options(ctx);

    if (ctx->s32Channel <= 0
        || ctx->s32SampleRate <= 0
        || ctx->s32DeviceSampleRate <= 0
        || ctx->s32DeviceChannel <= 0
        || (ctx->s32DataReadEnable && (ctx->srcFilePath == NULL))) {
        argparse_usage(&argparse);
        s32Ret = -1;
        goto __FAILED;
    }

    RK_MPI_SYS_Init();

    for (i = 0; i < ctx->s32LoopCount; i++) {
        ctx->s32AedLoudCount    = 0;
        ctx->s32BcdCount        = 0;
        ctx->s32BuzCount        = 0;
        ctx->s32GbsCount        = 0;
        RK_LOGI("start running loop count  = %d", i);
        s32Ret = unit_test_mpi_ai(ctx);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("unit_test_mpi_ai failed: %d", s32Ret);
            s32Ret = -1;
            goto __FAILED;
        }
        RK_LOGI("end running loop count  = %d", i);
    }

    if (ctx->s32AedEnable && ctx->s32AedLoudCount < 10) {
        RK_LOGE("Aed Loud event count: %d", ctx->s32AedLoudCount);
        s32Ret = -1;
    } else if (ctx->s32AedEnable) {
        RK_LOGD("Aed Loud event count: %d", ctx->s32AedLoudCount);
    }

    if (ctx->s32BcdEnable && ctx->s32BcdCount < 10) {
        RK_LOGE("Bcd event count: %d", ctx->s32BcdCount);
        s32Ret = -1;
    } else if (ctx->s32BcdEnable) {
        RK_LOGD("Bcd event count: %d", ctx->s32BcdCount);
    }

    if (ctx->s32BuzEnable && ctx->s32BuzCount < 10) {
        RK_LOGE("Buz event count: %d", ctx->s32BuzCount);
        s32Ret = -1;
    } else if (ctx->s32BuzEnable) {
        RK_LOGD("Buz event count: %d", ctx->s32BuzCount);
    }

    if (ctx->s32GbsEnable && ctx->s32GbsCount < 10) {
        RK_LOGE("Gbs event count: %d", ctx->s32GbsCount);
        s32Ret = -1;
    } else if (ctx->s32GbsEnable) {
        RK_LOGD("Gbs event count: %d", ctx->s32GbsCount);
    }

__FAILED:
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }

    RK_MPI_SYS_Exit();
    return s32Ret;
}

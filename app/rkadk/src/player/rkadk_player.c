/*
 * Copyright (c) 2022 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "rkadk_common.h"
#include "rkadk_log.h"
#include "rkadk_signal.h"
#include "rkadk_thread.h"
#include "rkadk_param.h"
#include "rkadk_media_comm.h"
#include "rkadk_player.h"
#include "rkadk_demuxer.h"
#include "rkadk_audio_decoder.h"
#include "rk_debug.h"
#include "rk_defines.h"
#include <math.h>
#include "rk_comm_vdec.h"
#include "rk_mpi_ao.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rkdemuxer.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <fcntl.h>

#define PLAYER_SNAPSHOT_MAX_WIDTH 4096
#define PLAYER_SNAPSHOT_MAX_HEIGHT 4096

typedef enum {
  RKADK_PLAYER_PAUSE_FALSE = 0x0,
  RKADK_PLAYER_PAUSE_START,
} RKADK_PLAYER_PAUSE_STATUS_E;

typedef enum {
  RKADK_PLAYER_SEEK_NO = 0x0,
  RKADK_PLAYER_SEEK_WAIT,
  RKADK_PLAYER_SEEK_VIDEO_DOING,
  RKADK_PLAYER_SEEK_VIDEO_DONE,
  RKADK_PLAYER_SEEK_DONE,
} RKADK_PLAYER_SEEK_STATUS_E;

typedef enum {
  RKADK_PLAYER_EOF_NO = 0x00,
  RKADK_PLAYER_VIDEO_EOF = 0x01,
  RKADK_PLAYER_AUDIO_EOF = 0x10,
  RKADK_PLAYER_ALL_EOF = 0x11,
} RKADK_PLAYER_EOF_STATE_E;

typedef struct {
  const RKADK_CHAR *srcFileUri;
  const RKADK_CHAR *dstFilePath;
  RKADK_U32   srcWidth;
  RKADK_U32   srcHeight;
  RKADK_U32   chnIndex;
  RKADK_U32   chNum;
  RKADK_U32   inputMode;
  RKADK_CODEC_TYPE_E eCodecType;
  RKADK_U32   compressMode;
  RKADK_U32   frameBufferCnt;
  RKADK_U32   streamBufferCnt;
  RKADK_U32   extraDataSize;
  RKADK_U32   readSize;
  RKADK_S32   chnFd;
  MB_POOL     pool;
  RKADK_S32   outputPixFmt;
  RKADK_BOOL  bEnableDei;
  RKADK_BOOL  bEnableColmv;
} RKADK_PLAYER_VDEC_CTX_S;

typedef struct {
  RKADK_S32  enIntfSync;
  RKADK_U32  u32Width;
  RKADK_U32  u32Height;
} VO_Timing_S;

typedef struct {
  RKADK_U32 u32FrameBufferCnt;
  COMPRESS_MODE_E enCompressMode;
} VDEC_CFG_S;

typedef struct {
  RKADK_U32 u32Screen0VoLayer;
  RKADK_U32 u32Screen1VoLayer;

  RKADK_U32 u32Screen0Rows;
  RKADK_U32 u32Screen1Rows;
  RK_BOOL bDoubleScreen;
} VO_CFG_S;

typedef struct {
  RKADK_U32 mode;
  VO_INTF_SYNC_E enIntfSync;
} VO_MODE_S;

typedef struct {
  RKADK_U32 u32VoLay;
  RKADK_U32 u32VoDev;
  RKADK_U32 u32VoChn;
  RKADK_VO_INTF_TYPE_E  enIntfType;
  RKADK_VO_INTF_SYNC_E  enIntfSync;
  RKADK_U32  x;
  RKADK_U32  y;
  RKADK_U32  dispWidth;
  RKADK_U32  dispHeight;
  RKADK_U32  imgeWidth;
  RKADK_U32  imageHeight;
  PIXEL_FORMAT_E pixFormat;
  RKADK_U32  dispFrmRt;
  MIRROR_E    enMirror;
  ROTATION_E  enRotation;
  RKADK_VO_SPLICE_MODE_E enVoSpliceMode;
} RKADK_PLAYER_VO_CTX_S;

typedef struct {
  RKADK_S32      chnNum;
  RKADK_S32      sampleRate;
  RKADK_S32      channel;
  RKADK_CODEC_TYPE_E eCodecType;
  RKADK_S32      decMode;
  RK_BOOL        bBlock;
  RKADK_S32      chnIndex;
  RKADK_S32      clrChnBuf;
} RKADK_PLAYER_ADEC_CTX_S;

typedef struct {
  const RKADK_CHAR *dstFilePath;
  RKADK_S32 chnNum;
  RKADK_S32 sampleRate;
  RKADK_S32 reSmpSampleRate;
  RKADK_S32 channel;
  RKADK_S32 deviceChannel;
  RKADK_S32 bitWidth;
  RKADK_S32 devId;
  RKADK_S32 periodCount;
  RKADK_S32 periodSize;
  RKADK_CHAR cardName[RKADK_BUFFER_LEN];
  RKADK_S32 chnIndex;
  RKADK_S32 setVolume;
  RKADK_S32 setMute;
  RKADK_S32 setFadeRate;
  RKADK_S32 setTrackMode;
  RKADK_S32 getVolume;
  RKADK_S32 getMute;
  RKADK_S32 getTrackMode;
  RKADK_S32 queryChnStat;
  RKADK_S32 pauseResumeChn;
  RKADK_S32 saveFile;
  RKADK_S32 queryFileStat;
  RKADK_S32 clrChnBuf;
  RKADK_S32 clrPubAttr;
  RKADK_S32 getPubAttr;
} RKADK_PLAYER_AO_CTX_S;

typedef struct {
  pthread_t tidVideoSend;
  pthread_t tidAudioSend;
} RKADK_PLAYER_THREAD_PARAM_S;

typedef struct {
  PIXEL_FORMAT_E enPixelFormat;
  RKADK_U32 u32Width;
  RKADK_U32 u32Height;
  RKADK_U32 u32VirWidth;
  RKADK_U32 u32VirHeight;
  MB_BLK pMbBlk;
} PLAYER_SNAPSHOT_FRAME_S;

typedef struct {
  bool bSnapshot;
  bool bVencChnExist;
  RKADK_U32 u32VencChn;
  RKADK_U32 u32MaxWidth;
  RKADK_U32 u32MaxHeight;
  PLAYER_SNAPSHOT_FRAME_S stFrame;

  void *pThread;
  void *pSignal;
  pthread_mutex_t mutex;
  RKADK_PPLAYER_SNAPSHOT_RECV_FN pfnDataCallback;
} RKADK_PLAYER_SNAPSHOT_PARAM_S;

typedef struct {
  RKADK_CHAR pFilePath[RKADK_PATH_LEN];
  RKADK_PLAYER_STATE_E enStatus;
  RKADK_PLAYER_EOF_STATE_E enEofStatus;
  pthread_mutex_t mutex;
  RKADK_BOOL bIsRtsp;
  RKADK_BOOL bEnableBlackBackground;

  RKADK_BOOL bEnableVideo;
  RKADK_BOOL bVideoExist;
  RKADK_S64 videoTimeStamp;
  RKADK_PLAYER_VDEC_CTX_S stVdecCtx;
  RKADK_PLAYER_VO_CTX_S stVoCtx;

  RKADK_BOOL bEnableAudio;
  RKADK_BOOL bAudioExist;
  RKADK_PLAYER_ADEC_CTX_S stAdecCtx;
  RKADK_PLAYER_AO_CTX_S stAoCtx;

  RKADK_VOID *pDemuxerCfg;
  RKADK_DEMUXER_PARAM_S stDemuxerParam;

  RKADK_PLAYER_SEEK_STATUS_E enSeekStatus;
  RKADK_S64 seekTimeStamp;
  RKADK_U32 duration;
  RKADK_S64 positionTimeStamp;
  RKADK_PLAYER_THREAD_PARAM_S stThreadParam;
  RKADK_PLAYER_EVENT_FN pfnPlayerCallback;

  RKADK_PLAYER_SNAPSHOT_PARAM_S stSnapshotParam;
} RKADK_PLAYER_HANDLE_S;

static void RKADK_PLAYER_ProcessEvent(RKADK_PLAYER_HANDLE_S *pstPlayer,
                                            RKADK_PLAYER_EVENT_E enEvent,
                                            RKADK_VOID *pData) {
  RKADK_CHECK_POINTER_N(pstPlayer);

  switch (pstPlayer->enSeekStatus) {
    case RKADK_PLAYER_SEEK_NO:
    case RKADK_PLAYER_SEEK_DONE:
      break;
    default:
      return;
  }

  if (pstPlayer->pfnPlayerCallback)
    pstPlayer->pfnPlayerCallback((RKADK_MW_PTR)pstPlayer, enEvent, pData);
}

static RKADK_S32 VdecCtxInit(RKADK_PLAYER_VDEC_CTX_S *pstVdecCtx, RKADK_PLAYER_VDEC_CFG_S stVdecCfg) {
  memset(pstVdecCtx, 0, sizeof(RKADK_PLAYER_VDEC_CTX_S));

  pstVdecCtx->inputMode = VIDEO_MODE_FRAME;
  pstVdecCtx->compressMode = COMPRESS_MODE_NONE;

  pstVdecCtx->frameBufferCnt = stVdecCfg.u32FrameBufCnt;
  pstVdecCtx->streamBufferCnt = stVdecCfg.u32StreamBufCnt;
  pstVdecCtx->readSize = 1024;
  pstVdecCtx->chNum = 1;
  pstVdecCtx->chnIndex = 0;
  pstVdecCtx->bEnableColmv = RKADK_TRUE;
  pstVdecCtx->outputPixFmt = (RKADK_S32)RK_FMT_YUV420SP;
  pstVdecCtx->eCodecType = RKADK_CODEC_TYPE_H264;
  pstVdecCtx->srcWidth = 320;
  pstVdecCtx->srcHeight = 240;

  if (pstVdecCtx->frameBufferCnt <= 0)
    pstVdecCtx->frameBufferCnt = 3;

  if (pstVdecCtx->streamBufferCnt <= 0)
    pstVdecCtx->streamBufferCnt = 3;

  return RKADK_SUCCESS;
}

static RKADK_S32 CreateVdec(RKADK_PLAYER_VDEC_CTX_S *pstVdecCtx) {
  RKADK_S32 ret = RKADK_SUCCESS;
  VDEC_CHN_ATTR_S stAttr;
  VDEC_CHN_PARAM_S stVdecParam;
  VDEC_PIC_BUF_ATTR_S stVdecPicBufAttr;
  MB_PIC_CAL_S stMbPicCalResult;
  VDEC_MOD_PARAM_S stModParam;

  memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));
  memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));
  memset(&stModParam, 0, sizeof(VDEC_MOD_PARAM_S));
  memset(&stVdecPicBufAttr, 0, sizeof(VDEC_PIC_BUF_ATTR_S));
  memset(&stMbPicCalResult, 0, sizeof(MB_PIC_CAL_S));

  RKADK_LOGI("found video width %d height %d pixfmt %d",
              pstVdecCtx->srcWidth, pstVdecCtx->srcHeight, pstVdecCtx->outputPixFmt);

  stVdecPicBufAttr.enCodecType = RKADK_MEDIA_GetRkCodecType(pstVdecCtx->eCodecType);
  stVdecPicBufAttr.stPicBufAttr.u32Width = pstVdecCtx->srcWidth;
  stVdecPicBufAttr.stPicBufAttr.u32Height = pstVdecCtx->srcHeight;
  stVdecPicBufAttr.stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)pstVdecCtx->outputPixFmt;
  stVdecPicBufAttr.stPicBufAttr.enCompMode = (COMPRESS_MODE_E)pstVdecCtx->compressMode;
  ret = RK_MPI_CAL_VDEC_GetPicBufferSize(&stVdecPicBufAttr, &stMbPicCalResult);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("get picture buffer size failed[%x]", ret);
    return ret;
  }

  stAttr.enMode = VIDEO_MODE_FRAME;
  stAttr.enType = RKADK_MEDIA_GetRkCodecType(pstVdecCtx->eCodecType);
  stAttr.u32PicWidth = pstVdecCtx->srcWidth;
  stAttr.u32PicHeight = pstVdecCtx->srcHeight;
  if (pstVdecCtx->bEnableDei) {
    // iep must remain 5 buffers for deinterlace
    stAttr.u32FrameBufCnt = pstVdecCtx->frameBufferCnt + 5;
  } else
    stAttr.u32FrameBufCnt = pstVdecCtx->frameBufferCnt;

  stAttr.u32StreamBufCnt = pstVdecCtx->streamBufferCnt;
  /*
    * if decode 10bit stream, need specify the u32FrameBufSize,
    * other conditions can be set to 0, calculated internally.
    */
  stAttr.u32FrameBufSize = stMbPicCalResult.u32MBSize;

  if (!pstVdecCtx->bEnableColmv)
    stAttr.stVdecVideoAttr.bTemporalMvpEnable = RK_FALSE;

  ret = RK_MPI_VDEC_CreateChn(pstVdecCtx->chnIndex, &stAttr);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Create vdec chn[%d] failed[%x]", pstVdecCtx->chnIndex, ret);
    return ret;
  }

  stVdecParam.stVdecVideoParam.enCompressMode = (COMPRESS_MODE_E)pstVdecCtx->compressMode;
  if (pstVdecCtx->bEnableDei)
    stVdecParam.stVdecVideoParam.bDeiEn = (RK_BOOL)pstVdecCtx->bEnableDei;

  // it is only effective to disable MV when decoding sequence output
  if (!pstVdecCtx->bEnableColmv)
    stVdecParam.stVdecVideoParam.enOutputOrder = VIDEO_OUTPUT_ORDER_DEC;

  ret = RK_MPI_VDEC_SetChnParam(pstVdecCtx->chnIndex, &stVdecParam);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Set chn[%d] param failed[%x]", pstVdecCtx->chnIndex, ret);
    goto __FAILED;
  }

  pstVdecCtx->chnFd = RK_MPI_VDEC_GetFd(pstVdecCtx->chnIndex);
  if (pstVdecCtx->chnFd <= 0) {
    RKADK_LOGE("Get chn[%d] fd[%d] failed", pstVdecCtx->chnIndex, pstVdecCtx->chnFd);
    goto __FAILED;
  }

  ret = RK_MPI_VDEC_StartRecvStream(pstVdecCtx->chnIndex);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("start recv chn[%d] failed[%x]", pstVdecCtx->chnIndex, ret);
    RK_MPI_VDEC_CloseFd(pstVdecCtx->chnIndex);
    goto __FAILED;
  }

  return RKADK_SUCCESS;

__FAILED:
  RK_MPI_VDEC_DestroyChn(pstVdecCtx->chnIndex);
  return RKADK_FAILURE;
}

static RKADK_S32 DestroyVdec(RKADK_PLAYER_VDEC_CTX_S *ctx) {
  RKADK_S32 ret = 0;
  ret = RK_MPI_VDEC_StopRecvStream(ctx->chnIndex);
  if (ret) {
    RKADK_LOGE("Stop Vdec stream failed[%x]", ret);
    return RKADK_FAILURE;
  }

  if (ctx->chnFd > 0) {
    ret = RK_MPI_VDEC_CloseFd(ctx->chnIndex);
    if (ret) {
      RKADK_LOGE("close Vdec fd failed[%x]", ret);
      return RKADK_FAILURE;
    }
  }

  ret = RK_MPI_VDEC_DestroyChn(ctx->chnIndex);
  if (ret) {
    RKADK_LOGE("Destroy Vdec channel[%d] failed[%x]", ctx->chnIndex, ret);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 SetVoCtx(RKADK_PLAYER_VO_CTX_S *pstVoCtx, RKADK_PLAYER_FRAME_INFO_S *pstFrameInfo) {
  memset(pstVoCtx, 0, sizeof(RKADK_PLAYER_VO_CTX_S));

  pstVoCtx->u32VoDev = pstFrameInfo->u32VoDev;
  pstVoCtx->u32VoLay = pstFrameInfo->u32VoLay;
  pstVoCtx->u32VoChn = pstFrameInfo->u32VoChn;
  pstVoCtx->x = pstFrameInfo->u32FrmInfoX;
  pstVoCtx->y = pstFrameInfo->u32FrmInfoY;
  pstVoCtx->dispWidth = pstFrameInfo->u32DispWidth;
  pstVoCtx->dispHeight = pstFrameInfo->u32DispHeight;
  pstVoCtx->imgeWidth = pstFrameInfo->u32ImgWidth;
  pstVoCtx->imageHeight = pstFrameInfo->u32ImgHeight;
  pstVoCtx->dispFrmRt = pstFrameInfo->stSyncInfo.u16FrameRate;
  pstVoCtx->enIntfType = pstFrameInfo->u32EnIntfType;
  pstVoCtx->enVoSpliceMode = pstFrameInfo->enVoSpliceMode;

  switch (pstFrameInfo->u32VoFormat) {
    case VO_FORMAT_RGB888:
      pstVoCtx->pixFormat = RK_FMT_RGB888;
      break;
    case VO_FORMAT_NV12:
      pstVoCtx->pixFormat = RK_FMT_YUV420SP;
      break;
    default:
      RKADK_LOGW("unsupport pix format[%d], use default[%d]", pstVoCtx->pixFormat, RK_FMT_RGB888);
      pstVoCtx->pixFormat = RK_FMT_RGB888;
  }

  pstVoCtx->enIntfSync = pstFrameInfo->enIntfSync;
  if (pstVoCtx->enIntfSync >= RKADK_VO_OUTPUT_BUTT) {
    RKADK_LOGE("Unsupport IntfSync[%d]", pstVoCtx->enIntfSync);
    return RKADK_FAILURE;
  }

  if (pstFrameInfo->bMirror && !pstFrameInfo->bFlip)
    pstVoCtx->enMirror = MIRROR_HORIZONTAL;
  else if (pstFrameInfo->bFlip && !pstFrameInfo->bMirror)
    pstVoCtx->enMirror = MIRROR_VERTICAL;
  else if (pstFrameInfo->bMirror && pstFrameInfo->bFlip)
    pstVoCtx->enMirror = MIRROR_BOTH;
  else
    pstVoCtx->enMirror = MIRROR_NONE;

  pstVoCtx->enRotation = (ROTATION_E)pstFrameInfo->u32Rotation;
  return RKADK_SUCCESS;
}

static RKADK_S32 AdecCtxInit(RKADK_PLAYER_ADEC_CTX_S *pstAdecCtx) {
  memset(pstAdecCtx, 0, sizeof(RKADK_PLAYER_ADEC_CTX_S));

  pstAdecCtx->chnIndex = PLAYER_ADEC_CHN;
  pstAdecCtx->sampleRate = 16000;
  pstAdecCtx->channel = 2;
  pstAdecCtx->decMode = ADEC_MODE_STREAM;
  pstAdecCtx->eCodecType = RKADK_CODEC_TYPE_MP3;
  pstAdecCtx->bBlock = RK_TRUE;
  return RKADK_SUCCESS;
}

static RKADK_S32 CreateAdec(RKADK_PLAYER_ADEC_CTX_S *pstAdecCtx) {
  RKADK_S32 ret = 0;
  ADEC_CHN_ATTR_S pstChnAttr;

  memset(&pstChnAttr, 0, sizeof(ADEC_CHN_ATTR_S));
  pstChnAttr.stCodecAttr.enType = RKADK_MEDIA_GetRkCodecType(pstAdecCtx->eCodecType);
  pstChnAttr.stCodecAttr.u32Channels = pstAdecCtx->channel;
  pstChnAttr.stCodecAttr.u32SampleRate = pstAdecCtx->sampleRate;
  pstChnAttr.stCodecAttr.u32BitPerCodedSample = 4;
  pstChnAttr.enType = RKADK_MEDIA_GetRkCodecType(pstAdecCtx->eCodecType);
  pstChnAttr.enMode = (ADEC_MODE_E)pstAdecCtx->decMode;
  pstChnAttr.u32BufCount = 4;
  pstChnAttr.u32BufSize = 50 * 1024;

  ret = RK_MPI_ADEC_CreateChn(pstAdecCtx->chnIndex, &pstChnAttr);
  if (ret)
    RKADK_LOGE("create adec chn[%d] err[%x]", pstAdecCtx->chnIndex, ret);

  return ret;
}

static RKADK_S32 AoCtxInit(RKADK_PLAYER_AO_CTX_S *pstAoCtx) {
  RKADK_PARAM_AUDIO_CFG_S *pstAudioParam = RKADK_PARAM_GetAudioCfg();
  if (!pstAudioParam) {
    RKADK_LOGE("RKADK_PARAM_GetAudioCfg failed");
    return RKADK_FAILURE;
  }

  memset(pstAoCtx, 0, sizeof(RKADK_PLAYER_AO_CTX_S));

  pstAoCtx->dstFilePath     = RKADK_NULL;
  pstAoCtx->chnNum          = 1;
  pstAoCtx->sampleRate      = AUDIO_SAMPLE_RATE;
  pstAoCtx->reSmpSampleRate = 0;
  pstAoCtx->deviceChannel   = AUDIO_DEVICE_CHANNEL;
  pstAoCtx->channel         = 2;
  pstAoCtx->bitWidth        = AUDIO_BIT_WIDTH;
  pstAoCtx->periodCount     = 4;
  pstAoCtx->periodSize      = 1024;
  memcpy(pstAoCtx->cardName, pstAudioParam->ao_audio_node,
         strlen(pstAudioParam->ao_audio_node));
  pstAoCtx->devId           = 0;
  pstAoCtx->chnIndex        = PLAYER_AO_CHN;
  pstAoCtx->setVolume       = 100;
  pstAoCtx->setMute         = 0;
  pstAoCtx->setTrackMode    = 0;
  pstAoCtx->setFadeRate     = 0;
  pstAoCtx->getVolume       = 1;
  pstAoCtx->getMute         = 0;
  pstAoCtx->getTrackMode    = 0;
  pstAoCtx->queryChnStat    = 0;
  pstAoCtx->pauseResumeChn  = 0;
  pstAoCtx->saveFile        = 0;
  pstAoCtx->queryFileStat   = 0;
  pstAoCtx->clrChnBuf       = 0;
  pstAoCtx->clrPubAttr      = 0;
  pstAoCtx->getPubAttr      = 0;

  return RKADK_SUCCESS;
}

static RKADK_VOID QueryAoFlowGraphStat(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RKADK_S32 ret = 0;
  AO_CHN_STATE_S pstStat;

  memset(&pstStat, 0, sizeof(AO_CHN_STATE_S));
  ret = RK_MPI_AO_QueryChnStat(aoDevId, aoChn, &pstStat);
  if (ret == RKADK_SUCCESS) {
    RKADK_LOGI("query ao flow status:");
    RKADK_LOGI("total number of channel buffer : %d", pstStat.u32ChnTotalNum);
    RKADK_LOGI("free number of channel buffer : %d", pstStat.u32ChnFreeNum);
    RKADK_LOGI("busy number of channel buffer : %d", pstStat.u32ChnBusyNum);
  }
}

static AUDIO_SOUND_MODE_E FindSoundMode(RKADK_S32 ch) {
  AUDIO_SOUND_MODE_E channel = AUDIO_SOUND_MODE_BUTT;
  switch (ch) {
    case 1:
      channel = AUDIO_SOUND_MODE_MONO;
      break;
    case 2:
      channel = AUDIO_SOUND_MODE_STEREO;
      break;
    default:
      RKADK_LOGE("Unsupport channel[%d]", ch);
      return AUDIO_SOUND_MODE_BUTT;
  }

  return channel;
}

static AUDIO_BIT_WIDTH_E FindBitWidth(RKADK_S32 bit) {
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
      RKADK_LOGE("Unsupport bitwidth[%d]", bit);
      return AUDIO_BIT_WIDTH_BUTT;
  }

  return bitWidth;
}

static RKADK_S32 SetAoChannelMode(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RKADK_S32 ret = 0;
  AO_CHN_PARAM_S pstParams;

  memset(&pstParams, 0, sizeof(AO_CHN_PARAM_S));

  //aoChn0 output left channel,  aoChn1 output right channel,
  if (aoChn == 0)
    pstParams.enMode = AUDIO_CHN_MODE_LEFT;
  else if (aoChn == 1)
    pstParams.enMode = AUDIO_CHN_MODE_RIGHT;

  ret = RK_MPI_AO_SetChnParams(aoDevId, aoChn, &pstParams);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("Ao set channel[%d] params failed[%x]", aoChn, ret);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 CreateDeviceAo(RKADK_PLAYER_AO_CTX_S *ctx) {
  RKADK_S32 ret = 0;
  AUDIO_SOUND_MODE_E soundMode;
  AIO_ATTR_S aoAttr;

  memset(&aoAttr, 0, sizeof(AIO_ATTR_S));

  if (strlen(ctx->cardName)) {
    snprintf((RKADK_CHAR *)(aoAttr.u8CardName),
              sizeof(aoAttr.u8CardName), "%s", ctx->cardName);
  }
  aoAttr.soundCard.channels = ctx->deviceChannel;
  aoAttr.soundCard.sampleRate = ctx->sampleRate;
  aoAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

  aoAttr.enBitwidth = FindBitWidth(ctx->bitWidth);
  if (aoAttr.enBitwidth == AUDIO_BIT_WIDTH_BUTT) {
    RKADK_LOGE("Unsupport audio bitWidth[%d]", aoAttr.enBitwidth);
    return RKADK_FAILURE;
  }

  aoAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)ctx->reSmpSampleRate;
  soundMode = FindSoundMode(ctx->channel);
  if (soundMode == AUDIO_SOUND_MODE_BUTT) {
    RKADK_LOGE("Unsupport audio soundMode[%d]", soundMode);
    return RKADK_FAILURE;
  }
  aoAttr.enSoundmode = soundMode;

  aoAttr.u32FrmNum = ctx->periodCount;
  aoAttr.u32PtNumPerFrm = ctx->periodSize;
  aoAttr.u32EXFlag = 0;
  aoAttr.u32ChnCnt = 2;
  ret = RK_MPI_AO_SetPubAttr(ctx->devId, &aoAttr);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_SetPubAttr failde[%x]", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_AO_Enable(ctx->devId);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_Enable failed[%x]", ret);
    return RKADK_FAILURE;
  }

  if (USE_AO_MIXER)
    SetAoChannelMode(ctx->devId, ctx->chnIndex);

  if (ctx->channel >= 2)
    ctx->setTrackMode = AUDIO_TRACK_NORMAL;
  else if (ctx->channel == 1)
    ctx->setTrackMode = AUDIO_TRACK_OUT_STEREO;
  else {
    RKADK_LOGE("illegal input channel[%d]", ctx->channel);
    goto __FAILED;
  }

  ret = RK_MPI_AO_SetTrackMode(ctx->devId, (AUDIO_TRACK_MODE_E)ctx->setTrackMode);
  if (ret != 0) {
    RK_LOGE("RK_MPI_AO_SetTrackMode failed[%x] aoChn[%d]", ret, ctx->chnIndex);
    goto __FAILED;
  }

  ret = RK_MPI_AO_EnableChn(ctx->devId, ctx->chnIndex);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_EnableChn failed[%x] aoChn[%d]", ret, ctx->chnIndex);
    goto __FAILED;
  }

  // set sample rate of input data
  ret = RK_MPI_AO_EnableReSmp(ctx->devId, ctx->chnIndex,
                                (AUDIO_SAMPLE_RATE_E)ctx->reSmpSampleRate);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_EnableReSmp failed[%x] aoChn[%d]", ret, ctx->chnIndex);
    RK_MPI_AO_DisableChn(ctx->devId, ctx->chnIndex);
    goto __FAILED;
  }

  return RKADK_SUCCESS;

__FAILED:
  RK_MPI_AO_Disable(ctx->devId);
  return RKADK_FAILURE;
}

static RKADK_S32 DestoryDeviceAo(RKADK_PLAYER_AO_CTX_S *ctx) {
  RKADK_S32 ret;

  ret = RK_MPI_AO_DisableReSmp(ctx->devId, ctx->chnIndex);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_DisableReSmp failed[%x]", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_AO_DisableChn(ctx->devId, ctx->chnIndex);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_DisableChn failed[%x]", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_AO_Disable(ctx->devId);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_Disable failed[%x]", ret);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

//Query whether there is a vdec stream
static RKADK_S32 VdecPollEvent(RKADK_S32 timeoutMsec, RKADK_S32 fd) {
  RKADK_S32 num_fds = 1;
  struct pollfd pollFds[num_fds];
  RKADK_S32 ret = 0;

  RK_ASSERT(fd > 0);
  memset(pollFds, 0, sizeof(pollFds));
  pollFds[0].fd = fd;
  pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

  ret = poll(pollFds, num_fds, timeoutMsec);
  if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP))) {
    RKADK_LOGE("fd[%d] polled error", fd);
    return -1;
  }

  return ret;
}

static bool SnapshotProc(void *pHandle) {
  int ret;
  RKADK_PLAYER_SNAPSHOT_S stData;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  VENC_CHN_ATTR_S stAttr;
  VIDEO_FRAME_INFO_S stFrame;
  VENC_STREAM_S stStream;
  VENC_PACK_S stPack;
  PLAYER_SNAPSHOT_FRAME_S stSnapshotFrame;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pHandle;

  memset(&stSnapshotFrame, 0, sizeof(PLAYER_SNAPSHOT_FRAME_S));
  if (!pstPlayer) {
    RKADK_LOGE("pstPlayer is null");
    goto __EXIT;
  }

  RKADK_SIGNAL_Wait(pstPlayer->stSnapshotParam.pSignal, -1);

  if (!pstPlayer->stSnapshotParam.bSnapshot)
    return false;

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP)
    goto __EXIT;

  if (!pstPlayer->stSnapshotParam.pfnDataCallback) {
    RKADK_LOGE("Unregistered snapshot data callback");
    goto __EXIT;
  }

  if (!pstPlayer->stSnapshotParam.stFrame.pMbBlk) {
    RKADK_LOGE("Snapshot pMbBlk is null");
    goto __EXIT;
  }

  pthread_mutex_lock(&pstPlayer->stSnapshotParam.mutex);
  memcpy(&stSnapshotFrame, &pstPlayer->stSnapshotParam.stFrame, sizeof(PLAYER_SNAPSHOT_FRAME_S));
  RK_MPI_MB_AddUserCnt(stSnapshotFrame.pMbBlk);
  pthread_mutex_unlock(&pstPlayer->stSnapshotParam.mutex);

  if (!pstPlayer->stSnapshotParam.bVencChnExist) {
    memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

    stAttr.stVencAttr.enType = RK_VIDEO_ID_JPEG;
    stAttr.stVencAttr.enPixelFormat = stSnapshotFrame.enPixelFormat;
    stAttr.stVencAttr.u32MaxPicWidth = pstPlayer->stSnapshotParam.u32MaxWidth;
    stAttr.stVencAttr.u32MaxPicHeight = pstPlayer->stSnapshotParam.u32MaxHeight;
    stAttr.stVencAttr.u32PicWidth = stSnapshotFrame.u32Width;
    stAttr.stVencAttr.u32PicHeight = stSnapshotFrame.u32Height;
    stAttr.stVencAttr.u32VirWidth = stSnapshotFrame.u32VirWidth;
    stAttr.stVencAttr.u32VirHeight = stSnapshotFrame.u32VirHeight;
    stAttr.stVencAttr.u32StreamBufCnt = 1;
    stAttr.stVencAttr.u32BufSize =
        stAttr.stVencAttr.u32MaxPicWidth * stAttr.stVencAttr.u32MaxPicHeight;

    ret = RK_MPI_VENC_CreateChn(pstPlayer->stSnapshotParam.u32VencChn, &stAttr);
    if (ret != RK_SUCCESS) {
      RKADK_LOGD("Create snapshot venc[%d] failed[%x]", pstPlayer->stSnapshotParam.u32VencChn, ret);
      goto __EXIT;
    }

    memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
    stRecvParam.s32RecvPicNum = -1;
    ret = RK_MPI_VENC_StartRecvFrame(pstPlayer->stSnapshotParam.u32VencChn, &stRecvParam);
    if (ret != RK_SUCCESS) {
      RKADK_LOGD("Snapshot ven[%d] start recv frame failed[%x]", pstPlayer->stSnapshotParam.u32VencChn, ret);
      RK_MPI_VENC_DestroyChn(pstPlayer->stSnapshotParam.u32VencChn);
      goto __EXIT;
    }

    pstPlayer->stSnapshotParam.bVencChnExist = true;
  } else {
    ret = RK_MPI_VENC_GetChnAttr(pstPlayer->stSnapshotParam.u32VencChn, &stAttr);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("Get snapshot venc[%d] attr failed[%x]", pstPlayer->stSnapshotParam.u32VencChn, ret);
      goto __EXIT;
    }

    if (stAttr.stVencAttr.enPixelFormat != stSnapshotFrame.enPixelFormat)
      RKADK_LOGW("venc chn pix[%x] != snapshot pix[%x]",stAttr.stVencAttr.enPixelFormat,
                  stSnapshotFrame.enPixelFormat);

    if (stAttr.stVencAttr.u32PicWidth != stSnapshotFrame.u32Width
        || stAttr.stVencAttr.u32PicHeight != stSnapshotFrame.u32Height) {
      RKADK_LOGD("Reset vencAttr[%d, %d], cell[%d, %d]", stAttr.stVencAttr.u32PicWidth,
                  stAttr.stVencAttr.u32PicHeight, stSnapshotFrame.u32Width, stSnapshotFrame.u32Height);
      stAttr.stVencAttr.u32PicWidth = stSnapshotFrame.u32Width;
      stAttr.stVencAttr.u32PicHeight = stSnapshotFrame.u32Height;
      stAttr.stVencAttr.u32VirWidth = stSnapshotFrame.u32VirWidth;
      stAttr.stVencAttr.u32VirHeight = stSnapshotFrame.u32VirHeight;

      ret = RK_MPI_VENC_SetChnAttr(pstPlayer->stSnapshotParam.u32VencChn, &stAttr);
      if (ret != RK_SUCCESS) {
        RKADK_LOGE("Set snapshot venc[%d] attr failed[%x]",
                    pstPlayer->stSnapshotParam.u32VencChn, ret);
        goto __EXIT;
      }
    }
  }

  memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  stFrame.stVFrame.pMbBlk = stSnapshotFrame.pMbBlk;
  stFrame.stVFrame.u32Width = stSnapshotFrame.u32Width;
  stFrame.stVFrame.u32Height = stSnapshotFrame.u32Height;
  stFrame.stVFrame.u32VirWidth = stSnapshotFrame.u32VirWidth;
  stFrame.stVFrame.u32VirHeight = stSnapshotFrame.u32VirHeight;
  stFrame.stVFrame.enPixelFormat = stSnapshotFrame.enPixelFormat;

  ret = RK_MPI_VENC_SendFrame(pstPlayer->stSnapshotParam.u32VencChn, &stFrame, -1);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Snapshot venc[%d] send frame failed[%x]",
                pstPlayer->stSnapshotParam.u32VencChn, ret);
    goto __EXIT;
  }

  ret = RK_MPI_MB_ReleaseMB(stSnapshotFrame.pMbBlk);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_MPI_MB_ReleaseMB failed[%x]", ret);
    goto __EXIT;
  }
  stSnapshotFrame.pMbBlk = NULL;

  memset(&stStream, 0, sizeof(VENC_STREAM_S));
  memset(&stPack, 0, sizeof(VENC_PACK_S));
  stStream.pstPack = &stPack;
  ret = RK_MPI_VENC_GetStream(pstPlayer->stSnapshotParam.u32VencChn, &stStream, -1);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Snapshot venc[%d] get stream failed[%x]",
                pstPlayer->stSnapshotParam.u32VencChn, ret);
    goto __EXIT;
  }

  memset(&stData, 0, sizeof(RKADK_PLAYER_SNAPSHOT_S));
  stData.u32Width = stSnapshotFrame.u32Width;
  stData.u32Height = stSnapshotFrame.u32Height;
  stData.u32DataLen = stStream.pstPack->u32Len;
  stData.pu8DataBuf = (RKADK_U8 *)RK_MPI_MB_Handle2VirAddr(stStream.pstPack->pMbBlk);
  pstPlayer->stSnapshotParam.pfnDataCallback(&stData);
  RKADK_LOGD("Snapshot done, w*h[%d, %d], pu8DataBuf[%p, %d]",
              stData.u32Width, stData.u32Height, stData.pu8DataBuf, stData.u32DataLen);

  RK_MPI_VENC_ReleaseStream(pstPlayer->stSnapshotParam.u32VencChn, &stStream);
  pstPlayer->stSnapshotParam.bSnapshot = false;
  return true;

__EXIT:
  if (stSnapshotFrame.pMbBlk) {
    ret = RK_MPI_MB_ReleaseMB(stSnapshotFrame.pMbBlk);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("RK_MPI_MB_ReleaseMB failed[%x]", ret);
      return -1;
    }
  }

  pstPlayer->stSnapshotParam.bSnapshot = false;
  return false;
}

static int SnapshotEnable(RKADK_PLAYER_HANDLE_S *pstPlayer, RKADK_PLAYER_SNAPSHOT_CFG_S stSnapshotCfg) {
  char name[20];

   pstPlayer->stSnapshotParam.u32VencChn = stSnapshotCfg.u32VencChn;
  if (stSnapshotCfg.u32MaxWidth > 0)
    pstPlayer->stSnapshotParam.u32MaxWidth = stSnapshotCfg.u32MaxWidth;
  else
    pstPlayer->stSnapshotParam.u32MaxWidth = PLAYER_SNAPSHOT_MAX_WIDTH;

  if (stSnapshotCfg.u32MaxHeight > 0)
    pstPlayer->stSnapshotParam.u32MaxHeight = stSnapshotCfg.u32MaxHeight;
  else
    pstPlayer->stSnapshotParam.u32MaxHeight = PLAYER_SNAPSHOT_MAX_HEIGHT;

  // Create signal
  pstPlayer->stSnapshotParam.pSignal = RKADK_SIGNAL_Create(0, 1);
  if (!pstPlayer->stSnapshotParam.pSignal) {
    RKADK_LOGE("Create snapshot signal failed");
    return -1;
  }

  // Create thread
  snprintf(name, sizeof(name), "%s", "snapshot");
  pstPlayer->stSnapshotParam.pThread = RKADK_THREAD_Create(SnapshotProc, pstPlayer, name);
  if (!pstPlayer->stSnapshotParam.pThread) {
    RKADK_LOGE("Create snapshot thread failed");
    RKADK_SIGNAL_Destroy(pstPlayer->stSnapshotParam.pSignal);
    return -1;
  }

  pthread_mutex_init(&pstPlayer->stSnapshotParam.mutex, NULL);
  pstPlayer->stSnapshotParam.pfnDataCallback = stSnapshotCfg.pfnDataCallback;
  return 0;
}

static int SnapshotDisable(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  int ret;

  // Exit thread
  if (pstPlayer->stSnapshotParam.pThread)
    RKADK_THREAD_SetExit(pstPlayer->stSnapshotParam.pThread);

  if (pstPlayer->stSnapshotParam.pSignal)
    RKADK_SIGNAL_Give(pstPlayer->stSnapshotParam.pSignal);

  // Destroy thread
  if (pstPlayer->stSnapshotParam.pThread) {
    RKADK_THREAD_Destory(pstPlayer->stSnapshotParam.pThread);
    pstPlayer->stSnapshotParam.pThread = NULL;
  }

  // Destroy signal
  if (pstPlayer->stSnapshotParam.pSignal)
    RKADK_SIGNAL_Destroy(pstPlayer->stSnapshotParam.pSignal);

  // Destory mutex
  pthread_mutex_destroy(&pstPlayer->stSnapshotParam.mutex);

  if (pstPlayer->stSnapshotParam.bVencChnExist) {
    ret = RK_MPI_VENC_DestroyChn(pstPlayer->stSnapshotParam.u32VencChn);
    if (ret != RK_SUCCESS)
      RKADK_LOGE("Destory snapshot venc chn failed[%x]", ret);
  }

  return 0;
}

static RKADK_VOID* SendVideoDataThread(RKADK_VOID *ptr) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  VIDEO_FRAME_INFO_S sFrame;
  VIDEO_FRAME_INFO_S tFrame;
  RK_U8 *lastFrame = RK_NULL;
  struct timespec t_begin, t_end;
  RKADK_S32 ret = 0;
  RKADK_S32 flagGetTframe = 0;
  RKADK_S32 voSendTime = 0, frameTime = 0, costtime = 0;

  if (pstPlayer->stDemuxerParam.videoAvgFrameRate <= 0) {
    RKADK_LOGE("Invalid video framerate[%d]", pstPlayer->stDemuxerParam.videoAvgFrameRate);
    return RKADK_NULL;
  }

  frameTime = 1000000 / pstPlayer->stDemuxerParam.videoAvgFrameRate;
  memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  memset(&tFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

  while (1) {
    if (pstPlayer->enStatus != RKADK_PLAYER_STATE_PAUSE || pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DONE) {
      if (pstPlayer->stVdecCtx.chnFd > 0) {
        ret = VdecPollEvent(MAX_TIME_OUT_MS, pstPlayer->stVdecCtx.chnFd);
        if (ret < 0)
          continue;
      }

      if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DONE)
        pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_DONE;

      ret = RK_MPI_VDEC_GetFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame, MAX_TIME_OUT_MS);
      if (ret == 0) {
        if (pstPlayer->bEnableBlackBackground) {
          if (!flagGetTframe) {
            tFrame.stVFrame.u32Width = sFrame.stVFrame.u32Width;
            tFrame.stVFrame.u32Height = sFrame.stVFrame.u32Height;
            tFrame.stVFrame.u32VirWidth = sFrame.stVFrame.u32VirWidth;
            tFrame.stVFrame.u32VirHeight = sFrame.stVFrame.u32VirHeight;
            tFrame.stVFrame.enPixelFormat = sFrame.stVFrame.enPixelFormat;
            tFrame.stVFrame.enCompressMode = sFrame.stVFrame.enCompressMode;
            flagGetTframe = 1;
          }
        }

        if ((sFrame.stVFrame.u32FrameFlag & (RKADK_U32)FRAME_FLAG_SNAP_END) == (RKADK_U32)FRAME_FLAG_SNAP_END) {
          if (pstPlayer->bEnableBlackBackground) {
            sFrame.stVFrame.u32Width = tFrame.stVFrame.u32Width;
            sFrame.stVFrame.u32Height = tFrame.stVFrame.u32Height;
            sFrame.stVFrame.u32VirWidth = tFrame.stVFrame.u32VirWidth;
            sFrame.stVFrame.u32VirHeight = tFrame.stVFrame.u32VirHeight;
            sFrame.stVFrame.enPixelFormat = tFrame.stVFrame.enPixelFormat;
            sFrame.stVFrame.enCompressMode = tFrame.stVFrame.enCompressMode;
            lastFrame = (RK_U8 *)RK_MPI_MB_Handle2VirAddr(sFrame.stVFrame.pMbBlk);

            if (sFrame.stVFrame.enPixelFormat == RK_FMT_YUV420P) {
              memset(lastFrame, 0, sFrame.stVFrame.u32VirWidth * sFrame.stVFrame.u32VirHeight);
              lastFrame += sFrame.stVFrame.u32VirWidth * sFrame.stVFrame.u32VirHeight;
              memset(lastFrame, 128, sFrame.stVFrame.u32VirWidth * sFrame.stVFrame.u32VirHeight / 2);
              ret = RK_MPI_SYS_MmzFlushCache(sFrame.stVFrame.pMbBlk, false);
              if (ret != 0)
                RKADK_LOGE("sys mmz flush cache failed[%x]", ret);

              ret = RK_MPI_VO_SendFrame(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn, &sFrame, -1);
              if (ret != 0)
                RKADK_LOGE("black backgound send fail[%x]", ret);
            }
          }
          RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);

          RKADK_LOGI("chn %d reach eos frame", pstPlayer->stVdecCtx.chnIndex);
          break;
        }

        if (!pstPlayer->bAudioExist)
          pstPlayer->positionTimeStamp = sFrame.stVFrame.u64PTS;

        clock_gettime(CLOCK_MONOTONIC, &t_end);

        if (pstPlayer->videoTimeStamp >= 0) {
          costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
          if ((RKADK_S64)sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp > (RKADK_S64)costtime) {
            voSendTime = sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp - costtime;

            if (!pstPlayer->bIsRtsp)
              usleep(voSendTime);
          }
          voSendTime = frameTime;
        } else {
          voSendTime = frameTime;
        }
        pstPlayer->videoTimeStamp = sFrame.stVFrame.u64PTS;

        ret = RK_MPI_SYS_MmzFlushCache(sFrame.stVFrame.pMbBlk, false);
        if (ret != RK_SUCCESS)
          RKADK_LOGE("sys mmz flush cache failed[%x]", ret);

        // save snapshot frame info
        pthread_mutex_lock(&pstPlayer->stSnapshotParam.mutex);
        pstPlayer->stSnapshotParam.stFrame.u32Width = sFrame.stVFrame.u32Width;
        pstPlayer->stSnapshotParam.stFrame.u32Height = sFrame.stVFrame.u32Height;
        pstPlayer->stSnapshotParam.stFrame.u32VirWidth = sFrame.stVFrame.u32VirWidth;
        pstPlayer->stSnapshotParam.stFrame.u32VirHeight = sFrame.stVFrame.u32VirHeight;
        pstPlayer->stSnapshotParam.stFrame.pMbBlk = sFrame.stVFrame.pMbBlk;

        //vo only support 420sp display, bypass mode internally converts 420p to 420sp
        if (pstPlayer->stVoCtx.enVoSpliceMode == SPLICE_MODE_BYPASS
            && sFrame.stVFrame.enPixelFormat == RK_FMT_YUV420P
            && pstPlayer->stVoCtx.pixFormat == RK_FMT_YUV420SP)
          pstPlayer->stSnapshotParam.stFrame.enPixelFormat = RK_FMT_YUV420SP;
        else
          pstPlayer->stSnapshotParam.stFrame.enPixelFormat = sFrame.stVFrame.enPixelFormat;
        pthread_mutex_unlock(&pstPlayer->stSnapshotParam.mutex);

        ret = RK_MPI_VO_SendFrame(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn, &sFrame, -1);
        if (ret != RK_SUCCESS)
          RKADK_LOGE("send vo failed[%x]", ret);

        clock_gettime(CLOCK_MONOTONIC, &t_begin);

        if (!pstPlayer->bIsRtsp)
          usleep(voSendTime);

        RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
      } else {
        //RKADK_LOGW("RK_MPI_VDEC_GetFrame timeout[%x]", ret);
      }
    } else {
      usleep(1000);
    }
  }

  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    if (pstPlayer->bAudioExist) {
      if (pstPlayer->enEofStatus == RKADK_PLAYER_EOF_NO)
        pstPlayer->enEofStatus = RKADK_PLAYER_VIDEO_EOF;
      else
        pstPlayer->enEofStatus |= RKADK_PLAYER_VIDEO_EOF;

      if (pstPlayer->enEofStatus == RKADK_PLAYER_ALL_EOF)
        if (pstPlayer->pfnPlayerCallback != NULL)
          pstPlayer->pfnPlayerCallback(ptr, RKADK_PLAYER_EVENT_EOF, NULL);
    } else {
      if (pstPlayer->duration != 0)
        pstPlayer->positionTimeStamp = (RKADK_S64)(pstPlayer->duration * 1000);

      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(ptr, RKADK_PLAYER_EVENT_EOF, NULL);
    }
  }

  RKADK_LOGI("Exit send vo thread");
  return RKADK_NULL;
}

static RKADK_VOID* SendAudioDataThread(RKADK_VOID *ptr) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  RKADK_S32 s32MilliSec = -1, result = 0, size = 0;
  AUDIO_FRAME_INFO_S stFrmInfo;

  memset(&stFrmInfo, 0, sizeof(AUDIO_FRAME_INFO_S));

  #ifdef WRITE_DECODER_FILE
  FILE *fp = RKADK_NULL;
  RKADK_CHAR name[128] = {0};
  mkdir("tmp", 0777);
  snprintf(name, sizeof(name), "/tmp/audio_out.pcm");

  fp = fopen(name, "wb");
  if (fp == RKADK_NULL) {
    RKADK_LOGE("can't open output file %s\n", name);
    return NULL;
  }
  #endif

  while (1) {
    ret = RK_MPI_ADEC_GetFrame(pstPlayer->stAdecCtx.chnIndex, &stFrmInfo, pstPlayer->stAdecCtx.bBlock);
    if (!ret) {
      size = stFrmInfo.pstFrame->u32Len;
      if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
        if (size > 0)
          pstPlayer->positionTimeStamp = stFrmInfo.pstFrame->u64TimeStamp;

        result = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, stFrmInfo.pstFrame, s32MilliSec);
        if (result < 0)
          RKADK_LOGE("send frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                      result, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
      }

      #ifdef WRITE_DECODER_FILE
      MB_BLK bBlk = stFrmInfo.pstFrame->pMbBlk;
      RK_VOID *pstFrame = RK_MPI_MB_Handle2VirAddr(bBlk);
      fwrite(pstFrame, 1, size, fp);
      #endif

      RK_MPI_ADEC_ReleaseFrame(pstPlayer->stAdecCtx.chnIndex, &stFrmInfo);

      if (size <= 0) {
        if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP)
          RKADK_LOGI("audio data send eof");

        break;
      }
    } else {
      RKADK_LOGE("RK_MPI_ADEC_GetFrame failed[%x]", ret);
      break;
    }

    if (pstPlayer->stAdecCtx.clrChnBuf) {
      RKADK_LOGI("adec clear chn(%d) buf", pstPlayer->stAdecCtx.chnIndex);
      RK_MPI_ADEC_ClearChnBuf(pstPlayer->stAdecCtx.chnIndex);
      pstPlayer->stAdecCtx.clrChnBuf = 0;
    }
  }

  #ifdef WRITE_DECODER_FILE
  if (fp)
    fclose(fp);
  #endif

  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    if (pstPlayer->duration != 0)
      pstPlayer->positionTimeStamp = (RKADK_S64)(pstPlayer->duration * 1000);

    RKADK_LOGD("wait ao eos!");
    RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);

    if (pstPlayer->bVideoExist) {
      if (pstPlayer->enEofStatus == RKADK_PLAYER_EOF_NO)
        pstPlayer->enEofStatus = RKADK_PLAYER_AUDIO_EOF;
      else
        pstPlayer->enEofStatus |= RKADK_PLAYER_AUDIO_EOF;

      if (pstPlayer->enEofStatus == RKADK_PLAYER_ALL_EOF)
        if (pstPlayer->pfnPlayerCallback != NULL)
          pstPlayer->pfnPlayerCallback(ptr, RKADK_PLAYER_EVENT_EOF, NULL);
    } else {
      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(ptr, RKADK_PLAYER_EVENT_EOF, NULL);
    }
  }

  RKADK_LOGI("Exit send ao thread");
  return RKADK_NULL;
}

static void AoVolumeControl(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  AUDIO_FADE_S aFade;
  aFade.bFade = RK_FALSE;
  RK_BOOL mute;
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = NULL;

  pstCommCfg = RKADK_PARAM_GetCommCfg();
  if (!pstCommCfg) {
    RKADK_LOGE("RKADK_PARAM_GetCommCfg failed");
    return;
  }

  aFade.enFadeOutRate = (AUDIO_FADE_RATE_E)pstPlayer->stAoCtx.setFadeRate;
  aFade.enFadeInRate = (AUDIO_FADE_RATE_E)pstPlayer->stAoCtx.setFadeRate;
  mute = (pstPlayer->stAoCtx.setMute == 0) ? RK_FALSE : RK_TRUE;
  RK_MPI_AO_SetMute(pstPlayer->stAoCtx.devId, mute, &aFade);
  RK_MPI_AO_SetVolume(pstPlayer->stAoCtx.devId, pstCommCfg->speaker_volume);
}

static void AoDebugProcess(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  if (pstPlayer->stAoCtx.getVolume) {
    RKADK_S32 volume = 0;
    RK_MPI_AO_GetVolume(pstPlayer->stAoCtx.devId, &volume);
    RKADK_LOGI("info : get volume = %d", volume);
    pstPlayer->stAoCtx.getVolume = 0;
  }

  if (pstPlayer->stAoCtx.getMute) {
    RK_BOOL mute = RK_FALSE;
    AUDIO_FADE_S fade;
    RK_MPI_AO_GetMute(pstPlayer->stAoCtx.devId, &mute, &fade);
    RKADK_LOGI("info : is mute = %d", mute);
    pstPlayer->stAoCtx.getMute = 0;
  }

  if (pstPlayer->stAoCtx.getTrackMode) {
    AUDIO_TRACK_MODE_E trackMode;
    RK_MPI_AO_GetTrackMode(pstPlayer->stAoCtx.devId, &trackMode);
    RKADK_LOGI("info : get track mode = %d", trackMode);
    pstPlayer->stAoCtx.getTrackMode = 0;
  }

  if (pstPlayer->stAoCtx.queryChnStat) {
    QueryAoFlowGraphStat(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    pstPlayer->stAoCtx.queryChnStat = 0;
  }

  if (pstPlayer->stAoCtx.saveFile) {
    AUDIO_SAVE_FILE_INFO_S saveFile;
    memset(&saveFile, 0, sizeof(AUDIO_SAVE_FILE_INFO_S));
    if (pstPlayer->stAoCtx.dstFilePath) {
      saveFile.bCfg = RK_TRUE;
      saveFile.u32FileSize = 1024 * 1024;
      snprintf(saveFile.aFileName, sizeof(saveFile.aFileName), "%s", "ao_save_file.bin");
      snprintf(saveFile.aFilePath, sizeof(saveFile.aFilePath), "%s", pstPlayer->stAoCtx.dstFilePath);
    }
    RK_MPI_AO_SaveFile(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, &saveFile);
    pstPlayer->stAoCtx.saveFile = 0;
  }

  if (pstPlayer->stAoCtx.queryFileStat) {
    AUDIO_FILE_STATUS_S fileStat;
    RK_MPI_AO_QueryFileStatus(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, &fileStat);
    RKADK_LOGI("info : query save file status = %d", fileStat.bSaving);
    pstPlayer->stAoCtx.queryFileStat = 0;
  }

  if (pstPlayer->stAoCtx.pauseResumeChn) {
    usleep(500 * 1000);
    RK_MPI_AO_PauseChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    RKADK_LOGI("AO pause");
    usleep(1000 * 1000);
    RK_MPI_AO_ResumeChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    RKADK_LOGI("AO resume");
    pstPlayer->stAoCtx.pauseResumeChn = 0;
  }

  if (pstPlayer->stAoCtx.clrChnBuf) {
    RK_MPI_AO_ClearChnBuf(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    pstPlayer->stAoCtx.clrChnBuf = 0;
  }

  if (pstPlayer->stAoCtx.clrPubAttr) {
    RK_MPI_AO_ClrPubAttr(pstPlayer->stAoCtx.devId);
    pstPlayer->stAoCtx.clrPubAttr = 0;
  }

  if (pstPlayer->stAoCtx.getPubAttr) {
    AIO_ATTR_S pstAttr;
    RK_MPI_AO_GetPubAttr(pstPlayer->stAoCtx.devId, &pstAttr);
    RKADK_LOGI("input stream rate = %d", pstAttr.enSamplerate);
    RKADK_LOGI("input stream sound mode = %d", pstAttr.enSoundmode);
    RKADK_LOGI("open sound card rate = %d", pstAttr.soundCard.sampleRate);
    RKADK_LOGI("open sound card channel = %d", pstAttr.soundCard.channels);
    pstPlayer->stAoCtx.getPubAttr = 0;
  }
}

static RKADK_S32 BufferFree(RKADK_VOID *opaque) {
  if (opaque) {
    free(opaque);
    opaque = NULL;
  }

  return 0;
}

static RKADK_VOID DoPullDemuxerVideoPacket(RKADK_VOID* pHandle) {
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;
  RKADK_S32 ret = 0;
  VDEC_STREAM_S stStream;
  MB_BLK buffer = RKADK_NULL;
  MB_EXT_CONFIG_S stMbExtConfig;

  if (pstDemuxerPacket->s8EofFlag || (pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_WAIT
        && pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_VIDEO_DOING)
        || (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DOING
      && pstDemuxerPacket->s8SpecialFlag && pstDemuxerPacket->s64Pts >= pstPlayer->seekTimeStamp)) {
    if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DOING) {
      pstPlayer->seekTimeStamp = pstDemuxerPacket->s64Pts;
      pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_VIDEO_DONE;
    }

    memset(&stMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    stMbExtConfig.pFreeCB = BufferFree;
    stMbExtConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
    stMbExtConfig.pu8VirAddr = (RK_U8 *)pstDemuxerPacket->s8PacketData;
    stMbExtConfig.u64Size = pstDemuxerPacket->s32PacketSize;

    RK_MPI_SYS_CreateMB(&buffer, &stMbExtConfig);

    stStream.u64PTS = pstDemuxerPacket->s64Pts;
    stStream.pMbBlk = buffer;
    stStream.u32Len = pstDemuxerPacket->s32PacketSize;
    stStream.bEndOfStream = pstDemuxerPacket->s8EofFlag ? RK_TRUE : RK_FALSE;
    stStream.bEndOfFrame = pstDemuxerPacket->s8EofFlag ? RK_TRUE : RK_FALSE;
    stStream.bBypassMbBlk = RK_TRUE;

__RETRY:
    ret = RK_MPI_VDEC_SendStream(pstPlayer->stVdecCtx.chnIndex, &stStream, -1);
    if (ret) {
      if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP) {
        RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
        return;
      }

      RKADK_LOGE("RK_MPI_VDEC_SendStream failed[%x]", ret);

      usleep(1000llu);
      goto  __RETRY;
    } else {
      RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
    }
  } else {
    if (pstDemuxerPacket->s8PacketData) {
      free(pstDemuxerPacket->s8PacketData);
      pstDemuxerPacket->s8PacketData = NULL;
    }
  }

  return;
}

static RKADK_VOID DoPullDemuxerAudioPacket(RKADK_VOID* pHandle) {
  RKADK_S32 ret = 0;
  MB_EXT_CONFIG_S stExtConfig;
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  AUDIO_STREAM_S stAudioStream;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;

  if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_WAIT) {
    if (pstDemuxerPacket->s8PacketData) {
      free(pstDemuxerPacket->s8PacketData);
      pstDemuxerPacket->s8PacketData = NULL;
    }

    return;
  } else if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DOING) {
    while(pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_DONE || !pstPlayer->enSeekStatus)
      usleep(1000);

    if (pstDemuxerPacket->s8PacketData) {
      free(pstDemuxerPacket->s8PacketData);
      pstDemuxerPacket->s8PacketData = NULL;
    }
  } else if (pstDemuxerPacket->s8EofFlag) {
    if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP)
      RKADK_LOGI("read eos packet, send eos to adec!");

    RK_MPI_ADEC_SendEndOfStream(pstPlayer->stAdecCtx.chnIndex, RK_FALSE);
  } else if (!pstPlayer->enSeekStatus || (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_DONE
             && pstDemuxerPacket->s64Pts >= pstPlayer->seekTimeStamp)) {
    if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_DONE)
      pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;

    stAudioStream.u32Len = pstDemuxerPacket->s32PacketSize;
    stAudioStream.u64TimeStamp = pstDemuxerPacket->s64Pts;
    stAudioStream.u32Seq = pstDemuxerPacket->s32Series;
    stAudioStream.bBypassMbBlk = RK_TRUE;

    memset(&stExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    stExtConfig.pFreeCB = BufferFree;
    stExtConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
    stExtConfig.pu8VirAddr = (RK_U8*)pstDemuxerPacket->s8PacketData;
    stExtConfig.u64Size    = pstDemuxerPacket->s32PacketSize;
    RK_MPI_SYS_CreateMB(&(stAudioStream.pMbBlk), &stExtConfig);

__RETRY:
    ret = RK_MPI_ADEC_SendStream(pstPlayer->stAdecCtx.chnIndex, &stAudioStream, pstPlayer->stAdecCtx.bBlock);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("RK_MPI_ADEC_SendStream failed[%x]", ret);
      goto __RETRY;
    }

    RK_MPI_MB_ReleaseMB(stAudioStream.pMbBlk);
  } else {
    if (pstDemuxerPacket->s8PacketData) {
      free(pstDemuxerPacket->s8PacketData);
      pstDemuxerPacket->s8PacketData = NULL;
    }
  }

  return;
}

static RKADK_VOID DoPullDemuxerWavPacket(RKADK_VOID* pHandle) {
  RKADK_S32 ret = 0;
  MB_EXT_CONFIG_S stMbExtConfig;
  RKADK_S32 s32MilliSec = -1;
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  AUDIO_FRAME_S frame;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;

  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_WAIT) {
      if (pstDemuxerPacket->s8PacketData) {
        free(pstDemuxerPacket->s8PacketData);
        pstDemuxerPacket->s8PacketData = NULL;
      }

      return;
    } else {
      if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_DONE)
        pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;

      if (!pstDemuxerPacket->s8EofFlag)
        pstPlayer->positionTimeStamp = pstDemuxerPacket->s64Pts;

      frame.u32Len = pstDemuxerPacket->s32PacketSize;
      frame.u64TimeStamp = pstDemuxerPacket->s64Pts;
      frame.enBitWidth = FindBitWidth(pstPlayer->stAoCtx.bitWidth);
      frame.enSoundMode = FindSoundMode(pstPlayer->stDemuxerParam.audioChannels);
      frame.bBypassMbBlk = RK_FALSE;

      memset(&stMbExtConfig, 0, sizeof(stMbExtConfig));
      stMbExtConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
      stMbExtConfig.pu8VirAddr = (RK_U8*)pstDemuxerPacket->s8PacketData;
      stMbExtConfig.u64Size = pstDemuxerPacket->s32PacketSize;
      RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &stMbExtConfig);

__RETRY:
      ret = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, &frame, s32MilliSec);
      if (ret < 0) {
        RK_LOGE("RK_MPI_AO_SendFrame fauled[%x], TimeStamp[%lld], s32MilliSec[%d]",
                  ret, frame.u64TimeStamp, s32MilliSec);
        goto __RETRY;
      }

      RK_MPI_MB_ReleaseMB(frame.pMbBlk);
      if (pstDemuxerPacket->s8PacketData) {
        free(pstDemuxerPacket->s8PacketData);
        pstDemuxerPacket->s8PacketData = NULL;
      }

      if (pstDemuxerPacket->s32PacketSize <= 0) {
        RKADK_LOGI("read eos packet, wait ao eos!");
        RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);

        if (pstPlayer->duration != 0)
          pstPlayer->positionTimeStamp = (RKADK_S64)(pstPlayer->duration * 1000);

        if (pstPlayer->pfnPlayerCallback != NULL)
          pstPlayer->pfnPlayerCallback(pHandle, RKADK_PLAYER_EVENT_EOF, NULL);
      }
    }
  } else {
    if (pstDemuxerPacket->s8PacketData) {
     free(pstDemuxerPacket->s8PacketData);
     pstDemuxerPacket->s8PacketData = NULL;
    }
  }

  return;
}

static RKADK_S32 CreateDeviceVo(RKADK_PLAYER_HANDLE_S *pstPlayer, RKADK_PLAYER_FRAME_INFO_S *pstFrameInfo) {
  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0;
  VO_VIDEO_LAYER_ATTR_S    stLayerAttr;
  VO_PUB_ATTR_S            stPubAttr;
  VO_CHN_ATTR_S stChnAttr;

  memset(&stPubAttr, 0, sizeof(VO_PUB_ATTR_S));
  memset(&stChnAttr, 0, sizeof(VO_CHN_ATTR_S));
  memset(&stLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));

  stPubAttr.enIntfType = RKADK_MEDIA_GetRkVoIntfTpye(pstPlayer->stVoCtx.enIntfType);
  stPubAttr.enIntfSync = VO_OUTPUT_DEFAULT;

  stLayerAttr.enPixFormat = pstPlayer->stVoCtx.pixFormat;
  stLayerAttr.u32DispFrmRt = pstPlayer->stVoCtx.dispFrmRt;
  if (pstFrameInfo->enVoSpliceMode == SPLICE_MODE_BYPASS) {
    stLayerAttr.stDispRect.s32X = pstPlayer->stVoCtx.x;
    stLayerAttr.stDispRect.s32Y = pstPlayer->stVoCtx.y;
    stLayerAttr.stDispRect.u32Width = pstPlayer->stVoCtx.dispWidth;
    stLayerAttr.stDispRect.u32Height = pstPlayer->stVoCtx.dispHeight;
  }

  stChnAttr.stRect.s32X = pstPlayer->stVoCtx.x;
  stChnAttr.stRect.s32Y = pstPlayer->stVoCtx.y;
  stChnAttr.stRect.u32Width = pstPlayer->stVoCtx.dispWidth;
  stChnAttr.stRect.u32Height = pstPlayer->stVoCtx.dispHeight;
  stChnAttr.u32FgAlpha = 255;
  stChnAttr.u32BgAlpha = 0;
  stChnAttr.enMirror = pstPlayer->stVoCtx.enMirror;
  stChnAttr.enRotation = pstPlayer->stVoCtx.enRotation;
  stChnAttr.u32Priority = 1;
  RKADK_LOGE("rect: [%d %d %d %d]",
            stChnAttr.stRect.s32X, stChnAttr.stRect.s32Y = pstPlayer->stVoCtx.y,
            stChnAttr.stRect.u32Width, stChnAttr.stRect.u32Height);

  ret = RKADK_MPI_VO_Init(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev,
                        pstPlayer->stVoCtx.u32VoChn, &stPubAttr, &stLayerAttr, &stChnAttr,
                        pstFrameInfo->enVoSpliceMode);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_Vo_Init failed[%x]", ret);
    return ret;
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 DestroyDeviceVo(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0;

  ret = RKADK_MPI_VO_DeInit(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev,
                        pstPlayer->stVoCtx.u32VoChn);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_DisableChn failed[%x]", ret);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_Create(RKADK_MW_PTR *pPlayer,
                              RKADK_PLAYER_CFG_S *pstPlayCfg) {
  RKADK_DEMUXER_INPUT_S stDemuxerInput;
  bool bSysInit = false;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pstPlayCfg, RKADK_FAILURE);

  if (*pPlayer) {
    RKADK_LOGE("player has been created");
    return RKADK_FAILURE;
  }

  bSysInit = RKADK_MPI_SYS_CHECK();
  if (!bSysInit) {
    RKADK_LOGE("System is not initialized");
    return RKADK_FAILURE;
  }

  RKADK_LOGI("Create Player[%d, %d] Start...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);

  if (pstPlayCfg->bEnableVideo == RKADK_FALSE && pstPlayCfg->bEnableAudio == RKADK_FALSE) {
    RKADK_LOGE("bEnableVideo and bEnableAudio are not enable");
    return RKADK_FAILURE;
  }

  pstPlayer = (RKADK_PLAYER_HANDLE_S *)malloc(sizeof(RKADK_PLAYER_HANDLE_S));
  if (!pstPlayer) {
    RKADK_LOGE("malloc pstPlayer failed");
    return RKADK_FAILURE;
  }

  memset(pstPlayer, 0, sizeof(RKADK_PLAYER_HANDLE_S));
  pstPlayer->pfnPlayerCallback = pstPlayCfg->pfnPlayerCallback;
  pstPlayer->bEnableVideo = pstPlayCfg->bEnableVideo;
  pstPlayer->bEnableAudio = pstPlayCfg->bEnableAudio;
  pstPlayer->bEnableBlackBackground = pstPlayCfg->bEnableBlackBackground;
  pstPlayer->enStatus = RKADK_PLAYER_STATE_BUTT;

  stDemuxerInput.ptr = (RKADK_VOID *)pstPlayer;
  stDemuxerInput.readModeFlag = DEMUXER_TYPE_PASSIVE;
  stDemuxerInput.videoEnableFlag = pstPlayer->bEnableVideo;
  stDemuxerInput.audioEnableFlag = pstPlayer->bEnableAudio;
  stDemuxerInput.transport = pstPlayCfg->stRtspCfg.transport;
  stDemuxerInput.u32IoTimeout = pstPlayCfg->stRtspCfg.u32IoTimeout;
  if (RKADK_DEMUXER_Create(&pstPlayer->pDemuxerCfg, &stDemuxerInput)) {
    RKADK_LOGE("RKADK_DEMUXER_Create failed");
    free(pstPlayer);
    return RKADK_FAILURE;
  }

  if (pstPlayer->bEnableVideo) {
    if (VdecCtxInit(&pstPlayer->stVdecCtx, pstPlayCfg->stVdecCfg)) {
      RKADK_LOGE("Create VDEC ctx failed");
      goto __FAILED;
    }

    if (SetVoCtx(&(pstPlayer->stVoCtx), &pstPlayCfg->stFrmInfo)) {
      RKADK_LOGE("Create VO ctx failed");
      goto __FAILED;
    }

    if (CreateDeviceVo(pstPlayer, &pstPlayCfg->stFrmInfo)) {
      RKADK_LOGE("Create VO failed");
      goto __FAILED;
    }
  }

  if (pstPlayer->bEnableAudio) {
    if (RKADK_AUDIO_DECODER_Register(RKADK_CODEC_TYPE_MP3)) {
      RKADK_LOGE("register mp3 failed");
      goto __FAILED;
    }

    if (AdecCtxInit(&pstPlayer->stAdecCtx)) {
      RKADK_LOGE("Create ADEC ctx failed");
      goto __FAILED;
    }

    if(AoCtxInit(&pstPlayer->stAoCtx)) {
      RKADK_LOGE("Create AO ctx failed");
      goto __FAILED;
    }
  }

  pthread_mutex_init(&(pstPlayer->mutex), NULL);

  if (pstPlayCfg->stSnapshotCfg.pfnDataCallback) {
    if (SnapshotEnable(pstPlayer, pstPlayCfg->stSnapshotCfg)) {
      RKADK_LOGE("Enable snapshot failed");
      goto __FAILED;
    }
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_IDLE;
  RKADK_LOGI("Create Player[%d, %d] End...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);

  *pPlayer = (RKADK_MW_PTR)pstPlayer;
  return RKADK_SUCCESS;

__FAILED:
  if (pstPlayer->bEnableVideo)
    DestroyDeviceVo(pstPlayer);

  RKADK_DEMUXER_Destroy(&pstPlayer->pDemuxerCfg);
  RKADK_PLAYER_ProcessEvent((RKADK_MW_PTR)pstPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);

  if (pstPlayer)
    free(pstPlayer);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Destroy(RKADK_MW_PTR pPlayer) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  RKADK_LOGI("Destory Player Start...");
  ret = RKADK_PLAYER_Stop(pPlayer);
  if (ret && ret != RKADK_STATE_ERR) {
    RKADK_LOGE("RKADK_PLAYER_Stop failed");
    return RKADK_FAILURE;
  }

  if (pstPlayer->bEnableVideo == RKADK_TRUE) {
    ret = DestroyDeviceVo(pstPlayer);
    if (ret) {
      RKADK_LOGE("DestroyDeviceVo failed");
      return RKADK_FAILURE;
    }
  }

  if (pstPlayer->bEnableAudio)
    if (RKADK_AUDIO_DECODER_UnRegister(RKADK_CODEC_TYPE_MP3)) {
      RKADK_LOGE("unregister mp3 failed");
      return RKADK_FAILURE;
    }

  if (pstPlayer->pDemuxerCfg)
    RKADK_DEMUXER_Destroy(&pstPlayer->pDemuxerCfg);

  pthread_mutex_destroy(&(pstPlayer->mutex));

  if (pstPlayer->stSnapshotParam.pfnDataCallback)
    if (SnapshotDisable(pstPlayer))
      RKADK_LOGE("Disable snapshot failed");

  if (pstPlayer)
    free(pstPlayer);

  RKADK_LOGI("Destory Player End...");
  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_SetDataSource(RKADK_MW_PTR pPlayer,
                                     const RKADK_CHAR *pszfilePath) {
  const char *suffix = NULL;
  RKADK_PLAYER_HANDLE_S *pstPlayer;
  RKADK_CODEC_TYPE_E eAudioCodecType = RKADK_CODEC_TYPE_BUTT;

  RKADK_CHECK_POINTER(pszfilePath, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_IDLE
      && pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  if ((strlen(pszfilePath) <= 0) || (strlen(pszfilePath) >= RKADK_PATH_LEN)) {
    RKADK_LOGE("Invalid pszfilePath[%s] lenght[%d]", pszfilePath, strlen(pszfilePath));
    goto __FAILED;
  }

  if (strstr(pszfilePath, "rtsp://"))
    pstPlayer->bIsRtsp = RKADK_TRUE;
  else
    suffix = strrchr(pszfilePath, '.');


  if (!suffix && !pstPlayer->bIsRtsp) {
    RKADK_LOGD("Non-file format or rtsp: %s", pszfilePath);
    goto __FAILED;
  }

  pstPlayer->bVideoExist = RKADK_FALSE;
  pstPlayer->bAudioExist = RKADK_FALSE;
  pstPlayer->videoTimeStamp = -1;

  if (pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_WAIT) {
    memset(pstPlayer->pFilePath, 0, RKADK_PATH_LEN);
    memcpy(pstPlayer->pFilePath, pszfilePath, strlen(pszfilePath));
  }

  if((suffix && !strcmp(suffix, ".mp4")) || pstPlayer->bIsRtsp) {
    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;
    if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, &pstPlayer->stDemuxerParam)) {
      RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
      goto __FAILED;
    }

    if (pstPlayer->bEnableVideo == RKADK_TRUE) {
      if (pstPlayer->stDemuxerParam.pVideoCodec != NULL) {
        if (strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h264") && strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h265")) {
          RKADK_LOGE("Unsupported video format(%s)", pstPlayer->stDemuxerParam.pVideoCodec);
          goto __FAILED;
        }

        if (pstPlayer->stDemuxerParam.videoWidth <= 0 || pstPlayer->stDemuxerParam.videoHeigh <= 0) {
          RKADK_LOGE("error: videoWidth = %d\n, videoHeigh = %d\n", pstPlayer->stDemuxerParam.videoWidth,
                      pstPlayer->stDemuxerParam.videoHeigh);
          goto __FAILED;
        }

        if (!strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h264")) {
          pstPlayer->stVdecCtx.eCodecType = RKADK_CODEC_TYPE_H264;
        } else if (!strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h265")) {
#ifdef RV1106_1103
          RKADK_LOGE("rv1106 nonsupport h265");
          goto __FAILED;
#else
          pstPlayer->stVdecCtx.eCodecType = RKADK_CODEC_TYPE_H265;
#endif
        }

        pstPlayer->bVideoExist = RKADK_TRUE;
        pstPlayer->stVdecCtx.srcWidth = pstPlayer->stDemuxerParam.videoWidth;
        pstPlayer->stVdecCtx.srcHeight = pstPlayer->stDemuxerParam.videoHeigh;

        if (pstPlayer->stDemuxerParam.VideoFormat == DEMUXER_VIDEO_YUV420SP_10BIT)
          pstPlayer->stVdecCtx.outputPixFmt = RK_FMT_YUV420SP_10BIT;
        else
          pstPlayer->stVdecCtx.outputPixFmt = RK_FMT_YUV420SP;
      } else if (pstPlayer->bEnableAudio){
        RKADK_LOGW("Video codec not find, try to play audio");
      } else {
        RKADK_LOGE("Video codec not find");
        goto __FAILED;
      }
    } else
      RKADK_LOGW("Video is unable");

    if (pstPlayer->bEnableAudio == RKADK_TRUE) {
      if (pstPlayer->stDemuxerParam.pAudioCodec != NULL) {
        if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "mp3"))
          eAudioCodecType = RKADK_CODEC_TYPE_MP3;
        else if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "wav"))
          eAudioCodecType = RKADK_CODEC_TYPE_PCM;
        else if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "pcm_alaw"))
          eAudioCodecType = RKADK_CODEC_TYPE_G711A;
        else {
          RKADK_LOGE("Unsupported audio format(%s)", pstPlayer->stDemuxerParam.pAudioCodec);
          goto __FAILED;
        }

        if (pstPlayer->stDemuxerParam.audioFormat == 0)
          pstPlayer->stAoCtx.bitWidth = 8;
        else if (pstPlayer->stDemuxerParam.audioFormat == 1)
          pstPlayer->stAoCtx.bitWidth = 16;
        else {
          RKADK_LOGE("Unsupported audioFormat = %d", pstPlayer->stDemuxerParam.audioFormat);
          goto __FAILED;
        }

        if (pstPlayer->stDemuxerParam.audioChannels <= 0 ||
            pstPlayer->stDemuxerParam.audioSampleRate <= 0) {
          RKADK_LOGE("Invalid channel(%d) or reSmpSampleRate(%d)",
                      pstPlayer->stDemuxerParam.audioChannels, pstPlayer->stDemuxerParam.audioSampleRate);
          goto __FAILED;
        }

        pstPlayer->bAudioExist = RKADK_TRUE;
        pstPlayer->stAdecCtx.sampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
        pstPlayer->stAdecCtx.channel = pstPlayer->stDemuxerParam.audioChannels;
        pstPlayer->stAdecCtx.eCodecType = eAudioCodecType;
        pstPlayer->stAoCtx.reSmpSampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
        pstPlayer->stAoCtx.channel = pstPlayer->stDemuxerParam.audioChannels;
      } else if (pstPlayer->bEnableVideo) {
        RKADK_LOGE("Audio codec not find, try to play video");
      } else {
        RKADK_LOGE("Audio codec not find");
        goto __FAILED;
      }
    } else
      RKADK_LOGW("audio is unable");
  } else if (suffix && (!strcmp(suffix, ".h264") || !strcmp(suffix, ".h265"))) {
    if (!pstPlayer->bEnableVideo) {
      RKADK_LOGE("video is unable");
      goto __FAILED;
    }

    if (!strcmp(suffix, ".h264")) {
      pstPlayer->stVdecCtx.eCodecType = RKADK_CODEC_TYPE_H264;
    } else if (!strcmp(suffix, ".h265")) {
#ifdef RV1106_1103
      RKADK_LOGE("1106 nonsupport h265");
      goto __FAILED;
#else
      pstPlayer->stVdecCtx.eCodecType = RKADK_CODEC_TYPE_H265;
#endif
    }

    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;
    if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, &pstPlayer->stDemuxerParam)) {
      RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
      goto __FAILED;
    }

    if (pstPlayer->stDemuxerParam.pVideoCodec != NULL) {
      if (strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h264")) {
        RKADK_LOGE("Unsupported video format(%s)", pstPlayer->stDemuxerParam.pVideoCodec);
        goto __FAILED;
      }

      if (pstPlayer->stDemuxerParam.videoWidth <= 0 || pstPlayer->stDemuxerParam.videoHeigh <= 0) {
        RKADK_LOGE("error: videoWidth = %d\n, videoHeigh = %d\n", pstPlayer->stDemuxerParam.videoWidth,
                    pstPlayer->stDemuxerParam.videoHeigh);
        goto __FAILED;
      }

      pstPlayer->bVideoExist = RKADK_TRUE;
      pstPlayer->stVdecCtx.srcWidth = pstPlayer->stDemuxerParam.videoWidth;
      pstPlayer->stVdecCtx.srcHeight = pstPlayer->stDemuxerParam.videoHeigh;

      if (pstPlayer->stDemuxerParam.VideoFormat == DEMUXER_VIDEO_YUV420SP_10BIT)
        pstPlayer->stVdecCtx.outputPixFmt = RK_FMT_YUV420SP_10BIT;
      else
        pstPlayer->stVdecCtx.outputPixFmt = RK_FMT_YUV420SP;
    }else {
      RKADK_LOGE("Video codec not find");
      goto __FAILED;
    }
  } else if (suffix && (!strcmp(suffix, ".mp3") || !strcmp(suffix, ".wav"))) {
    if (!pstPlayer->bEnableAudio) {
      RKADK_LOGE("audio is unable");
      goto __FAILED;
    }

    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
    if (!strcmp(suffix, ".wav"))
      pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerWavPacket;
    else
      pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;

    if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, &pstPlayer->stDemuxerParam)) {
      RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
      goto __FAILED;
    }

    if (pstPlayer->stDemuxerParam.pAudioCodec != NULL) {
      if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "mp3"))
        eAudioCodecType = RKADK_CODEC_TYPE_MP3;
      else if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "wav"))
        eAudioCodecType = RKADK_CODEC_TYPE_PCM;
        else if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "pcm_alaw"))
          eAudioCodecType = RKADK_CODEC_TYPE_G711A;
      else {
        RKADK_LOGE("Unsupported audio format(%s)", pstPlayer->stDemuxerParam.pAudioCodec);
        goto __FAILED;
      }

      if (pstPlayer->stDemuxerParam.audioFormat == 0)
        pstPlayer->stAoCtx.bitWidth = 8;
      else if (pstPlayer->stDemuxerParam.audioFormat == 1)
        pstPlayer->stAoCtx.bitWidth = 16;
      else {
        RKADK_LOGE("Unsupported audioFormat = %d", pstPlayer->stDemuxerParam.audioFormat);
        goto __FAILED;
      }

      if (pstPlayer->stDemuxerParam.audioChannels <= 0 ||
          pstPlayer->stDemuxerParam.audioSampleRate <= 0) {
        RKADK_LOGE("Invalid channel(%d) or reSmpSampleRate(%d)",
                    pstPlayer->stDemuxerParam.audioChannels, pstPlayer->stDemuxerParam.audioSampleRate);
        goto __FAILED;
      }

      pstPlayer->bAudioExist = RKADK_TRUE;
      pstPlayer->stAdecCtx.sampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
      pstPlayer->stAdecCtx.channel = pstPlayer->stDemuxerParam.audioChannels;
      pstPlayer->stAdecCtx.eCodecType = eAudioCodecType;

      pstPlayer->stAoCtx.reSmpSampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
      pstPlayer->stAoCtx.channel = pstPlayer->stDemuxerParam.audioChannels;
    } else {
      RKADK_LOGE("Audio codec not find");
    }
  } else {
    RKADK_LOGE("Unsupported file format(%s)", pszfilePath);
    goto __FAILED;
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_INIT;
  pthread_mutex_unlock(&pstPlayer->mutex);
  return RKADK_SUCCESS;

__FAILED:
  pstPlayer->bAudioExist = RKADK_FALSE;
  memset(pstPlayer->pFilePath, 0, RKADK_PATH_LEN);
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Prepare(RKADK_MW_PTR pPlayer) {
  int ret;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_INIT) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  if (pstPlayer->bVideoExist) {
    ret = CreateVdec(&pstPlayer->stVdecCtx);
    if (ret) {
      RKADK_LOGE("Create VDEC failed");
      goto __FAILED;
    }
  } else {
    RKADK_LOGW("video stream not exist, bEnableVideo: %d", pstPlayer->bEnableVideo);
  }

  if (pstPlayer->bAudioExist) {
    if (pstPlayer->stAdecCtx.eCodecType != RKADK_CODEC_TYPE_PCM) {
      if (CreateAdec(&pstPlayer->stAdecCtx)) {
        RKADK_LOGE("Create ADEC failed");
        goto __FAILED;
      }
    }

    ret = CreateDeviceAo(&pstPlayer->stAoCtx);
    if (ret) {
      RKADK_LOGE("Create AO failed");
      goto __FAILED;
    }
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_PREPARED;
  pthread_mutex_unlock(&pstPlayer->mutex);

  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_PREPARED, NULL);
  return RKADK_SUCCESS;

__FAILED:
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Play(RKADK_MW_PTR pPlayer) {
  RKADK_S32 ret = 0;
  RKADK_S64 startPts = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_PREPARED
      && pstPlayer->enStatus != RKADK_PLAYER_STATE_PAUSE) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_WAIT)
    startPts = pstPlayer->seekTimeStamp;

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PREPARED) {
    ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg, startPts);
    if (ret != 0) {
      RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
      goto __FAILED;
    }

    if (pstPlayer->bVideoExist) {
      ret = pthread_create(&pstPlayer->stThreadParam.tidVideoSend, RKADK_NULL, SendVideoDataThread, pPlayer);
      if (ret) {
        RKADK_LOGE("Create send video thread failed [%d]", ret);
        goto __FAILED;
      }
    }

    if (pstPlayer->bAudioExist) {
      if (pstPlayer->stAdecCtx.eCodecType != RKADK_CODEC_TYPE_PCM) {
        ret = pthread_create(&pstPlayer->stThreadParam.tidAudioSend, RKADK_NULL, SendAudioDataThread, pPlayer);
        if (ret) {
          RKADK_LOGE("Create send audio thread failed [%d]", ret);
          goto __FAILED;
        }
      }
      AoVolumeControl(pstPlayer);
      AoDebugProcess(pstPlayer);
    }
  } else if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PAUSE) {
    if (pstPlayer->bAudioExist) {
      ret = RK_MPI_AO_ResumeChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
      if (ret != RKADK_SUCCESS) {
        RKADK_LOGE("RK_MPI_AO_ResumeChn failed[%x]", ret);
       goto __FAILED;
      }
    }
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_PLAY;
  pthread_mutex_unlock(&pstPlayer->mutex);

  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_PLAY, NULL);
  return RKADK_SUCCESS;

__FAILED:
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Stop(RKADK_MW_PTR pPlayer) {
  RKADK_S32 ret = 0, ret1 = 0;
  RKADK_PLAYER_STATE_E enStatus = RKADK_PLAYER_STATE_BUTT;
  RKADK_PLAYER_SEEK_STATUS_E enSeekStatus = RKADK_PLAYER_SEEK_NO;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_IDLE
      || pstPlayer->enStatus == RKADK_PLAYER_STATE_INIT) {
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_SUCCESS;
  }

  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_PREPARED
      && pstPlayer->enStatus != RKADK_PLAYER_STATE_PLAY
      && pstPlayer->enStatus != RKADK_PLAYER_STATE_PAUSE) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  enStatus = pstPlayer->enStatus;
  pstPlayer->enStatus = RKADK_PLAYER_STATE_STOP;

  enSeekStatus = pstPlayer->enSeekStatus;
  pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;
  pstPlayer->seekTimeStamp = 0;

  if (enStatus == RKADK_PLAYER_STATE_PAUSE) {
    if (pstPlayer->bVideoExist) {
        ret = RK_MPI_VO_ResumeChn(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn);
        if (ret != RKADK_SUCCESS) {
          RKADK_LOGE("RK_MPI_VO_ResumeChn failed[%x]", ret);
          goto __FAILED;
        }
    }

    if (pstPlayer->bAudioExist) {
      ret = RK_MPI_AO_ResumeChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
      if (ret) {
        RKADK_LOGE("RK_MPI_AO_ResumeChn failed[%x]", ret);
        ret1 |= RKADK_FAILURE;
      }
    }
  }

  if (enSeekStatus != RKADK_PLAYER_SEEK_WAIT)
    pstPlayer->positionTimeStamp = 0;

  RKADK_DEMUXER_ReadPacketStop(pstPlayer->pDemuxerCfg);

  if (pstPlayer->stThreadParam.tidVideoSend) {
    pthread_join(pstPlayer->stThreadParam.tidVideoSend, RKADK_NULL);
    pstPlayer->stThreadParam.tidVideoSend = 0;
  }

  if (pstPlayer->stThreadParam.tidAudioSend) {
    pthread_join(pstPlayer->stThreadParam.tidAudioSend, RKADK_NULL);
    pstPlayer->stThreadParam.tidAudioSend = 0;
  }

  pstPlayer->stSnapshotParam.bSnapshot = false;
  pstPlayer->stSnapshotParam.stFrame.pMbBlk = NULL;

  pstPlayer->enEofStatus = RKADK_PLAYER_EOF_NO;
  if (pstPlayer->bVideoExist) {
      if (DestroyVdec(&pstPlayer->stVdecCtx))
        ret1 |= RKADK_FAILURE;
  }

  if (pstPlayer->bAudioExist) {
    if (pstPlayer->stAdecCtx.eCodecType != RKADK_CODEC_TYPE_PCM) {
      ret = RK_MPI_ADEC_DestroyChn(pstPlayer->stAdecCtx.chnIndex);
      if (ret) {
        RKADK_LOGE("destory Adec channel failed[%x]", ret);
        ret1 |= RKADK_FAILURE;
      }
    }

    if (DestoryDeviceAo(&pstPlayer->stAoCtx))
      ret1 |= RKADK_FAILURE;
  }

  if (ret1)
    goto __FAILED;

  pstPlayer->enSeekStatus = enSeekStatus;
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_STOPPED, NULL);
  return ret1;

__FAILED:
  if (enStatus != RKADK_PLAYER_STATE_BUTT)
    pstPlayer->enStatus = enStatus;

  if (enSeekStatus != RKADK_PLAYER_SEEK_NO)
    pstPlayer->enSeekStatus = enSeekStatus;

  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Pause(RKADK_MW_PTR pPlayer) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_PLAY) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  if (pstPlayer->bAudioExist) {
    ret = RK_MPI_AO_PauseChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_AO_PauseChn failed, ret = %X\n", ret);
      goto __FAILED;
    }
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_PAUSE;
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_PAUSED, NULL);
  return RKADK_SUCCESS;

__FAILED:
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Seek(RKADK_MW_PTR pPlayer, RKADK_S64 s64TimeInMs) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_STATE_E enStatus = RKADK_PLAYER_STATE_BUTT;
  RKADK_S64 maxSeekTimeInMs = (RKADK_S64)pow(2, 63) / 1000;
  RKADK_S64 seekDelta;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPlayer->enStatus < RKADK_PLAYER_STATE_PREPARED
      || pstPlayer->enStatus > RKADK_PLAYER_STATE_STOP) {
    RKADK_LOGW("State[%d] err", pstPlayer->enStatus);
    return RKADK_STATE_ERR;
  }

  if (strstr(pstPlayer->pFilePath, "rtsp://")) {
    RKADK_LOGI("Nonsupport rtsp seek");
    return RKADK_FAILURE;
  }

  if (pstPlayer->duration > 0 && (pstPlayer->duration < s64TimeInMs)) {
    RKADK_LOGE("Invalid s64TimeInMs[%lld] > duration[%d]", s64TimeInMs, pstPlayer->duration);
    return RKADK_FAILURE;
  }

  if (s64TimeInMs > maxSeekTimeInMs) {
    RKADK_LOGE("Invalid s64TimeInMs[%lld]", s64TimeInMs);
    return RKADK_FAILURE;
  }

  seekDelta = RKADK_ABS(s64TimeInMs - pstPlayer->positionTimeStamp / 1000);
  if (pstPlayer->bVideoExist && seekDelta < 500) {
    RKADK_LOGW("mini seek margin is 500ms, s64TimeInMs: %lld, position: %lld, seekDelta: %lld",
                s64TimeInMs, pstPlayer->positionTimeStamp / 1000, seekDelta);
    RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_SEEK_END, NULL);
    return RKADK_SUCCESS;
  }

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PAUSE)
    enStatus = RKADK_PLAYER_STATE_PAUSE;

  pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_WAIT;
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    ret = RKADK_PLAYER_Stop(pPlayer);
    if (ret && ret != RKADK_STATE_ERR) {
      RKADK_LOGE("RKADK_PLAYER_Stop failed");
      goto __FAILED;
    }
  }

  ret = RKADK_PLAYER_SetDataSource(pstPlayer, pstPlayer->pFilePath);
  if (ret) {
    RKADK_LOGD("RKADK_PLAYER_SetDataSource failed");
    goto __FAILED;
  }

  ret = RKADK_PLAYER_Prepare(pstPlayer);
  if (ret) {
    RKADK_LOGD("RKADK_PLAYER_Prepare failed");
    goto __FAILED;
  }

  pstPlayer->seekTimeStamp = s64TimeInMs * 1000;
  ret = RKADK_PLAYER_Play(pstPlayer);
  if (ret) {
    RKADK_LOGD("RKADK_PLAYER_Prepare failed");
    goto __FAILED;
  }

  if (enStatus == RKADK_PLAYER_STATE_PAUSE)
    RKADK_PLAYER_Pause(pstPlayer);

  if (pstPlayer->bVideoExist)
    pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_VIDEO_DOING;

  if (pstPlayer->bAudioExist && !pstPlayer->bVideoExist)
      pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_DONE;

  if (pstPlayer->pfnPlayerCallback != NULL)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_SEEK_END, NULL);
  return RKADK_SUCCESS;

__FAILED:
  pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_GetPlayStatus(RKADK_MW_PTR pPlayer,
                                     RKADK_PLAYER_STATE_E *penState) {
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_CHECK_POINTER(penState, RKADK_FAILURE);

  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  *penState = pstPlayer->enStatus;
  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_GetDuration(RKADK_MW_PTR pPlayer, RKADK_U32 *pDuration) {
  void *demuxerCfg;
  RKADK_DEMUXER_INPUT_S demuxerInput;
  RKADK_DEMUXER_PARAM_S demuxerParam;
  RKADK_S64 vDuration = 0, aDuration = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  RKADK_CHECK_POINTER(pDuration, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);

  if (strlen(pstPlayer->pFilePath) == 0) {
    RKADK_LOGE("Invalid pFilePath[%s] length", pstPlayer->pFilePath);
    return RKADK_FAILURE;
  }

  if (strstr(pstPlayer->pFilePath, "rtsp://")) {
    RKADK_LOGI("Nonsupport get rtsp duration");
    return RKADK_FAILURE;
  }

  demuxerInput.ptr = RK_NULL;
  demuxerInput.readModeFlag = DEMUXER_TYPE_ACTIVE;
  demuxerInput.videoEnableFlag = pstPlayer->bVideoExist;
  demuxerInput.audioEnableFlag = pstPlayer->bAudioExist;
  demuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = NULL;
  demuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = NULL;

  if (RKADK_DEMUXER_Create(&demuxerCfg, &demuxerInput)) {
    RKADK_LOGE("RKADK_DEMUXER_Create failed");
    return RKADK_FAILURE;
  }

  if (RKADK_DEMUXER_GetParam(demuxerCfg, pstPlayer->pFilePath, &demuxerParam)) {
    RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
    goto __FAILED;
  }

  if (pstPlayer->bAudioExist) {
    if (RKADK_DEMUXER_ReadAudioDuration(demuxerCfg, &aDuration))
      goto __FAILED;

    *pDuration = aDuration / 1000;
  } else if (pstPlayer->bVideoExist) {
    if (RKADK_DEMUXER_ReadVideoDuration(demuxerCfg, &vDuration))
      goto __FAILED;

    *pDuration = vDuration / 1000;
  } else {
    RKADK_LOGE("get duration failed, bAudioExist: %d, bVideoExist: %d", pstPlayer->bAudioExist, pstPlayer->bVideoExist);
    goto __FAILED;
  }

  pstPlayer->duration = *pDuration;
  RKADK_DEMUXER_Destroy(&demuxerCfg);
  return RKADK_SUCCESS;

__FAILED:
  RKADK_DEMUXER_Destroy(&demuxerCfg);
  return RKADK_FAILURE;
}

RKADK_S64 RKADK_PLAYER_GetCurrentPosition(RKADK_MW_PTR pPlayer) {
  RKADK_S64 duration = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PLAY
      || pstPlayer->enStatus == RKADK_PLAYER_STATE_PAUSE) {
    duration = pstPlayer->positionTimeStamp / 1000;
    return duration;
  } else {
    return RKADK_FAILURE;
  }
}

RKADK_S32 RKADK_PLAYER_Snapshot(RKADK_MW_PTR pPlayer) {
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (!pstPlayer->bVideoExist) {
    RKADK_LOGW("%s no video track", pstPlayer->pFilePath);
    return -1;
  }

  if (!pstPlayer->stSnapshotParam.bSnapshot) {
    pstPlayer->stSnapshotParam.bSnapshot = true;
    RKADK_SIGNAL_Give(pstPlayer->stSnapshotParam.pSignal);
  } else {
    RKADK_LOGD("Have been in snapshot");
  }

  return 0;
}

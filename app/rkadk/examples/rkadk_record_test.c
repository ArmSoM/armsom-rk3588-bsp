/*
 * Copyright (c) 2021 Rockchip, Inc. All Rights Reserved.
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
#include "rkadk_media_comm.h"
#include "rkadk_log.h"
#include "rkadk_param.h"
#include "rkadk_record.h"
#include "isp/sample_isp.h"
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int optind;
extern char *optarg;

static RKADK_CHAR optstr[] = "a:I:p:m:h";

static bool is_quit = false;
#define IQ_FILE_PATH "/etc/iqfiles"

static void print_usage(const RKADK_CHAR *name) {
  printf("usage example:\n");
  printf("\t%s [-a /etc/iqfiles] [-I 0] [-t 0]\n", name);
  printf("\t-a: enable aiq with dirpath provided, eg:-a "
         "/oem/etc/iqfiles/, Default /etc/iqfiles,"
         "without this option aiq should run in other application\n");
  printf("\t-I: Camera id, Default:0\n");
  printf("\t-p: param ini directory path, Default:/data/rkadk\n");
  printf("\t-m: multiple sensors, Default:0, options: 1(all isp sensors), 2(isp+ahd sensors)\n");
}

static RKADK_S32
GetRecordFileName(RKADK_MW_PTR pRecorder, RKADK_U32 u32FileCnt,
                  RKADK_CHAR (*paszFilename)[RKADK_MAX_FILE_PATH_LEN]) {
  static RKADK_U32 u32FileIdx = 0;

  RKADK_LOGD("u32FileCnt:%d, pRecorder:%p", u32FileCnt, pRecorder);

  if (u32FileIdx >= 10)
    u32FileIdx = 0;

  for (RKADK_U32 i = 0; i < u32FileCnt; i++) {
    sprintf(paszFilename[i], "/mnt/sdcard/RecordTest_%u.mp4", u32FileIdx);
    u32FileIdx++;
  }

  return 0;
}

static RKADK_VOID
RecordEventCallback(RKADK_MW_PTR pRecorder,
                    const RKADK_MUXER_EVENT_INFO_S *pstEventInfo) {
  switch (pstEventInfo->enEvent) {
  case RKADK_MUXER_EVENT_STREAM_START:
    printf("+++++ RKADK_MUXER_EVENT_STREAM_START +++++\n");
    break;
  case RKADK_MUXER_EVENT_STREAM_STOP:
    printf("+++++ RKADK_MUXER_EVENT_STREAM_STOP +++++\n");
    break;
  case RKADK_MUXER_EVENT_FILE_BEGIN:
    printf("+++++ RKADK_MUXER_EVENT_FILE_BEGIN +++++\n");
    printf("\tstFileInfo: %s\n",
           pstEventInfo->unEventInfo.stFileInfo.asFileName);
    printf("\tu32Duration: %d\n",
           pstEventInfo->unEventInfo.stFileInfo.u32Duration);
    break;
  case RKADK_MUXER_EVENT_FILE_END:
    printf("+++++ RKADK_MUXER_EVENT_FILE_END +++++\n");
    printf("\tstFileInfo: %s\n",
           pstEventInfo->unEventInfo.stFileInfo.asFileName);
    printf("\tu32Duration: %d\n",
           pstEventInfo->unEventInfo.stFileInfo.u32Duration);
    break;
  case RKADK_MUXER_EVENT_MANUAL_SPLIT_END:
    printf("+++++ RKADK_MUXER_EVENT_MANUAL_SPLIT_END +++++\n");
    printf("\tstFileInfo: %s\n",
           pstEventInfo->unEventInfo.stFileInfo.asFileName);
    printf("\tu32Duration: %d\n",
           pstEventInfo->unEventInfo.stFileInfo.u32Duration);
    break;
  case RKADK_MUXER_EVENT_ERR_CREATE_FILE_FAIL:
    printf("+++++ RKADK_MUXER_EVENT_ERR_CREATE_FILE_FAIL +++++\n");
    break;
  case RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL:
    printf("+++++ RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL +++++\n");
    break;
  default:
    printf("+++++ Unknown event(%d) +++++\n", pstEventInfo->enEvent);
    break;
  }
}

#ifdef RKAIQ
static int IspProcess(RKADK_S32 u32CamId) {
  int ret;
  bool mirror = false, flip = false;

  // set mirror flip
  ret = RKADK_PARAM_GetCamParam(u32CamId, RKADK_PARAM_TYPE_MIRROR, &mirror);
  if (ret)
    RKADK_LOGE("RKADK_PARAM_GetCamParam mirror failed");

  ret = RKADK_PARAM_GetCamParam(u32CamId, RKADK_PARAM_TYPE_FLIP, &flip);
  if (ret)
    RKADK_LOGE("RKADK_PARAM_GetCamParam flip failed");

  if (mirror || flip) {
    ret = SAMPLE_ISP_SET_MirrorFlip(u32CamId, mirror, flip);
    if (ret)
      RKADK_LOGE("SAMPLE_ISP_SET_MirrorFlip failed");
  }

#ifdef RKADK_DUMP_ISP_RESULT
  // mirror flip
  ret = SAMPLE_ISP_GET_MirrorFlip(u32CamId, &mirror, &flip);
  if (ret)
    RKADK_LOGE("SAMPLE_ISP_GET_MirrorFlip failed");
  else
    RKADK_LOGD("GET mirror = %d, flip = %d", mirror, flip);
#endif

  return 0;
}
#endif

static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  is_quit = true;
}

int main(int argc, char *argv[]) {
  int c, ret;
  RKADK_RECORD_ATTR_S stRecAttr;
  RKADK_MW_PTR pRecorder = NULL, pRecorder1 = NULL;
  RK_BOOL bMultiSensor = RK_FALSE;
  RKADK_REC_MANUAL_SPLIT_ATTR_S stSplitAttr;
  const char *iniPath = NULL;
  RKADK_PARAM_RES_E type;
  RKADK_PARAM_FPS_S stFps;
  RKADK_PARAM_GOP_S stGop;
  RKADK_PARAM_CODEC_CFG_S stCodecType;
  char path[RKADK_PATH_LEN];
  char sensorPath[RKADK_MAX_SENSOR_CNT][RKADK_PATH_LEN];
  RKADK_S32 s32CamId = 0;

#ifdef RKAIQ
  int inCmd = 0;
  RKADK_CHAR *pIqfilesPath = IQ_FILE_PATH;
  rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
  RK_BOOL bMultiCam = RK_FALSE;
  const char *tmp_optarg = optarg;
#endif

  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
#ifdef RKAIQ
    case 'a':
      if (!optarg && NULL != argv[optind] && '-' != argv[optind][0]) {
        tmp_optarg = argv[optind++];
      }

      if (tmp_optarg)
        pIqfilesPath = (char *)tmp_optarg;
      break;
    case 'm':
      inCmd = atoi(optarg);
      if (inCmd == 1) {
        bMultiCam = RKADK_TRUE;
        bMultiSensor = RKADK_TRUE;
      } else if (inCmd == 2)
        bMultiSensor = RKADK_TRUE;
      break;
#endif
    case 'I':
      s32CamId = atoi(optarg);
      break;
    case 'p':
      iniPath = optarg;
      RKADK_LOGD("iniPath: %s", iniPath);
      break;
    case 'h':
    default:
      print_usage(argv[0]);
      optind = 0;
      return -1;
    }
  }
  optind = 0;

  if (bMultiSensor)
    s32CamId = 0;

  RKADK_MPI_SYS_Init();

  if (iniPath) {
    memset(path, 0, RKADK_PATH_LEN);
    memset(sensorPath, 0, RKADK_MAX_SENSOR_CNT * RKADK_PATH_LEN);
    sprintf(path, "%s/rkadk_setting.ini", iniPath);
    for (int i = 0; i < RKADK_MAX_SENSOR_CNT; i++)
      sprintf(sensorPath[i], "%s/rkadk_setting_sensor_%d.ini", iniPath, i);

    /*
    lg:
      char *sPath[] = {"/data/rkadk/rkadk_setting_sensor_0.ini",
      "/data/rkadk/rkadk_setting_sensor_1.ini", NULL};
    */
    char *sPath[] = {sensorPath[0], sensorPath[1], NULL};

    RKADK_PARAM_Init(path, sPath);
  } else {
    RKADK_PARAM_Init(NULL, NULL);
  }

record:
#ifdef RKAIQ
  stFps.enStreamType = RKADK_STREAM_TYPE_SENSOR;
  ret = RKADK_PARAM_GetCamParam(s32CamId, RKADK_PARAM_TYPE_FPS, &stFps);
  if (ret) {
    RKADK_LOGE("RKADK_PARAM_GetCamParam u32CamId[%d] fps failed", s32CamId);
    return -1;
  }

  SAMPLE_ISP_Start(s32CamId, hdr_mode, bMultiCam, pIqfilesPath, stFps.u32Framerate);
  //IspProcess(s32CamId);

  if (bMultiCam) {
    ret = RKADK_PARAM_GetCamParam(1, RKADK_PARAM_TYPE_FPS, &stFps);
    if (ret) {
      RKADK_LOGE("RKADK_PARAM_GetCamParam u32CamId[1] fps failed");
      SAMPLE_ISP_Stop(s32CamId);
      return -1;
    }

    SAMPLE_ISP_Start(1, hdr_mode, bMultiCam, pIqfilesPath, stFps.u32Framerate);
    //IspProcess(1);
  }
#endif

  stRecAttr.s32CamID = s32CamId;
  stRecAttr.pfnRequestFileNames = GetRecordFileName;
  stRecAttr.pfnEventCallback = RecordEventCallback;

  if (RKADK_RECORD_Create(&stRecAttr, &pRecorder)) {
    RKADK_LOGE("s32CamId[%d] Create recorder failed", s32CamId);
#ifdef RKAIQ
    SAMPLE_ISP_Stop(s32CamId);
    if (bMultiCam)
      SAMPLE_ISP_Stop(1);
#endif
    return -1;
  }
  RKADK_RECORD_Start(pRecorder);

  if (bMultiSensor) {
    stRecAttr.s32CamID = 1;
    if (RKADK_RECORD_Create(&stRecAttr, &pRecorder1)) {
      RKADK_LOGE("s32CamId[1] Create recorder failed");
#ifdef RKAIQ
      SAMPLE_ISP_Stop(s32CamId);
      if (bMultiCam)
        SAMPLE_ISP_Stop(1);
#endif
      return -1;
    }
    RKADK_RECORD_Start(pRecorder1);
  }

  RKADK_LOGD("initial finish\n");

  signal(SIGINT, sigterm_handler);
  char cmd[64];
  printf("\n#Usage: input 'quit' to exit programe!\n"
         "peress any other key to quit\n");

  while (!is_quit) {
    fgets(cmd, sizeof(cmd), stdin);
    if (strstr(cmd, "quit") || is_quit) {
      RKADK_LOGD("#Get 'quit' cmd!");
      break;
    } else if (strstr(cmd, "MS")) { //Manual Split
      RKADK_PARAM_REC_TIME_S stRecTime;
      stRecTime.enStreamType = RKADK_STREAM_TYPE_VIDEO_MAIN;
      stRecTime.time = 20; //default time

      RKADK_PARAM_GetCamParam(s32CamId, RKADK_PARAM_TYPE_SPLITTIME, &stRecTime);
      stSplitAttr.enManualType = MUXER_PRE_MANUAL_SPLIT;
      stSplitAttr.u32DurationSec = stRecTime.time;
      RKADK_RECORD_ManualSplit(pRecorder, &stSplitAttr);
    } else if (strstr(cmd, "LR")) { //Lapse Record
      type = RKADK_REC_TYPE_LAPSE;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_RECORD_TYPE, &type);
      RKADK_RECORD_Reset(&pRecorder);
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "NR")) { //Normal Record
      type = RKADK_REC_TYPE_NORMAL;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_RECORD_TYPE, &type);
      RKADK_RECORD_Reset(&pRecorder);
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "720")) {
      type = RKADK_RES_720P;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_RES, &type);
      ret = RKADK_RECORD_Reset(&pRecorder);
      if (ret < 0) {
#ifndef RV1106_1103
        RKADK_RECORD_Stop(pRecorder);
        RKADK_RECORD_Destroy(pRecorder);
        pRecorder = NULL;
#ifdef RKAIQ
        SAMPLE_ISP_Stop(stRecAttr.s32CamID);
#endif
        goto record;
#endif
      }
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "1080")) {
      type = RKADK_RES_1080P;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_RES, &type);
      ret = RKADK_RECORD_Reset(&pRecorder);
      if (ret < 0) {
#ifndef RV1106_1103
        RKADK_RECORD_Stop(pRecorder);
        RKADK_RECORD_Destroy(pRecorder);
        pRecorder = NULL;
#ifdef RKAIQ
        SAMPLE_ISP_Stop(stRecAttr.s32CamID);
#endif
        goto record;
#endif
      }
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "1620")) {
      type = RKADK_RES_1620P;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_RES, &type);
      ret = RKADK_RECORD_Reset(&pRecorder);
      if (ret < 0) {
#ifndef RV1106_1103
        RKADK_RECORD_Stop(pRecorder);
        RKADK_RECORD_Destroy(pRecorder);
        pRecorder = NULL;
#ifdef RKAIQ
        SAMPLE_ISP_Stop(stRecAttr.s32CamID);
#endif
        goto record;
#endif
      }
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "1296")) {
      type = RKADK_RES_1296P;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_RES, &type);
      ret = RKADK_RECORD_Reset(&pRecorder);
      if (ret < 0) {
#ifndef RV1106_1103
        RKADK_RECORD_Stop(pRecorder);
        RKADK_RECORD_Destroy(pRecorder);
        pRecorder = NULL;
#ifdef RKAIQ
        SAMPLE_ISP_Stop(stRecAttr.s32CamID);
#endif
        goto record;
#endif
      }
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "264")) {
      stCodecType.enCodecType = RKADK_CODEC_TYPE_H264;
      stCodecType.enStreamType = RKADK_STREAM_TYPE_VIDEO_MAIN;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_CODEC_TYPE, &stCodecType);
      stCodecType.enStreamType = RKADK_STREAM_TYPE_VIDEO_SUB;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_CODEC_TYPE, &stCodecType);
      ret = RKADK_RECORD_Reset(&pRecorder);
      if (ret < 0) {
#ifndef RV1106_1103
        RKADK_RECORD_Stop(pRecorder);
        RKADK_RECORD_Destroy(pRecorder);
        pRecorder = NULL;
#ifdef RKAIQ
        SAMPLE_ISP_Stop(stRecAttr.s32CamID);
#endif
        goto record;
#endif
      }
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "265")) {
      stCodecType.enCodecType = RKADK_CODEC_TYPE_H265;
      stCodecType.enStreamType = RKADK_STREAM_TYPE_VIDEO_MAIN;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_CODEC_TYPE, &stCodecType);
      stCodecType.enStreamType = RKADK_STREAM_TYPE_VIDEO_SUB;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_CODEC_TYPE, &stCodecType);
      ret = RKADK_RECORD_Reset(&pRecorder);
      if (ret < 0) {
#ifndef RV1106_1103
        RKADK_RECORD_Stop(pRecorder);
        RKADK_RECORD_Destroy(pRecorder);
        pRecorder = NULL;
#ifdef RKAIQ
        SAMPLE_ISP_Stop(stRecAttr.s32CamID);
#endif
        goto record;
#endif
      }
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "start")) {
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "stop")) {
      RKADK_RECORD_Stop(pRecorder);
    } else if (strstr(cmd, "fps-25")) {
      //set main record fps
      stFps.u32Framerate = 25;
      stFps.enStreamType = RKADK_STREAM_TYPE_VIDEO_MAIN;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_FPS, &stFps);
      //set main record gop
      stGop.enStreamType = stFps.enStreamType;
      stGop.u32Gop = stFps.u32Framerate;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_GOP, &stGop);

      //set sub record fps
      stFps.enStreamType = RKADK_STREAM_TYPE_VIDEO_SUB;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_FPS, &stFps);
      //set sub record gop
      stGop.enStreamType = stFps.enStreamType;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_GOP, &stGop);

      RKADK_RECORD_Reset(&pRecorder);
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "fps-30")) {
      //set main record fps
      stFps.u32Framerate = 30;
      stFps.enStreamType = RKADK_STREAM_TYPE_VIDEO_MAIN;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_FPS, &stFps);
      //set main record gop
      stGop.enStreamType = stFps.enStreamType;
      stGop.u32Gop = stFps.u32Framerate;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_GOP, &stGop);

      //set sub record fps
      stFps.enStreamType = RKADK_STREAM_TYPE_VIDEO_SUB;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_FPS, &stFps);
      //set sub record gop
      stGop.enStreamType = stFps.enStreamType;
      RKADK_PARAM_SetCamParam(s32CamId, RKADK_PARAM_TYPE_GOP, &stGop);

      RKADK_RECORD_Reset(&pRecorder);
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "r-90")) {
      RKADK_RECORD_SetRotation(pRecorder, ROTATION_90, RKADK_STREAM_TYPE_VIDEO_MAIN);
    } else if (strstr(cmd, "r-180")) {
      RKADK_RECORD_SetRotation(pRecorder, ROTATION_180, RKADK_STREAM_TYPE_VIDEO_MAIN);
    } else if (strstr(cmd, "r-270")) {
      RKADK_RECORD_SetRotation(pRecorder, ROTATION_270, RKADK_STREAM_TYPE_VIDEO_MAIN);
    } else if (strstr(cmd, "m-1")) {
      RKADK_RECORD_ToggleMirror(pRecorder, RKADK_STREAM_TYPE_VIDEO_MAIN, 1);
    } else if (strstr(cmd, "m-0")) {
      RKADK_RECORD_ToggleMirror(pRecorder, RKADK_STREAM_TYPE_VIDEO_MAIN, 0);
    } else if (strstr(cmd, "f-1")) {
       RKADK_RECORD_ToggleFlip(pRecorder, RKADK_STREAM_TYPE_VIDEO_MAIN, 1);
    } else if (strstr(cmd, "f-0")) {
      RKADK_RECORD_ToggleFlip(pRecorder, RKADK_STREAM_TYPE_VIDEO_MAIN, 0);
    }

    usleep(500000);
  }

  RKADK_RECORD_Stop(pRecorder);
  RKADK_RECORD_Destroy(pRecorder);

#ifdef RKAIQ
  SAMPLE_ISP_Stop(s32CamId);
#endif

  if (bMultiSensor) {
    RKADK_RECORD_Stop(pRecorder1);
    RKADK_RECORD_Destroy(pRecorder1);

#ifdef RKAIQ
    if (bMultiCam)
      SAMPLE_ISP_Stop(1);
#endif
  }

  RKADK_MPI_SYS_Exit();
  RKADK_LOGD("exit!");
  return 0;
}

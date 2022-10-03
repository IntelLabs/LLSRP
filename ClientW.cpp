/*
 * Copyright 2022 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * Written by: Evgeny Stupachenko
 *             e-mail: evgeny.v.stupachenko@intel.com
 */

#include "multilink.h"

#include <stdio.h>

#include "Config.h"
#include "VideoDecodeW.h"
#include "DisplayWindowW.h"
#include "MemoryBufferW.h"
#include "TimeStat.h"
#include "MouseKbdReadW.h"
#include "OptionsW.h"

MultiLink Ml;
bool WindowOpened = false;
int32_t Running = 1;
int32_t AvgRT;

uint64_t GlobalTime;

class DecodeSlice {
public:
  int32_t Num;
  int32_t FrameNum;
  uint8_t *InData;
  uint16_t InSize;
  uint8_t *OutData;
  VideoDecode *V;
  int32_t Ret;
  bool WindowOpened;
  DecodeSlice() {
    Num = -1;
    FrameNum = -1;
    InData = NULL;
    InSize = 0;
    OutData = NULL;
    V = NULL;
    Ret = -1;
    WindowOpened = false;
  }
  ~DecodeSlice() {
  }
};

MemoryBuffer Mb(SLICE_NUM * 4 + 1);

DWORD WINAPI decodeOneSlice(LPVOID Arg) {
  if (Arg == NULL) {
    printf("Error! Wrong decode slice thread parameter!\n");
    return NULL;
  }
  DecodeSlice *DS = (DecodeSlice *)Arg;
  int32_t FrameNum = -1;
  while (Running) {
    int16_t SPtr;
    uint8_t *SlicePtr = Mb.getSlicePtr(DS->Num, &FrameNum, &SPtr);
    if (!SlicePtr) {
      Sleep(1);
      continue;
    }
    DS->Ret = DS->V->decodeFrame(DS->OutData, Mb.getSliceSize(SPtr),
                                 SlicePtr, DS->Num, DS->WindowOpened);
    if (FrameNum != -1)
      FrameNum = (FrameNum + 1) & 255;
#ifdef PRINT_WARN
    if (DS->Ret != 0)
      printf("Warning: unable to decode received packet size %d, slice %d "
             "maybe missing, error %d\n", Mb.SInfo[SPtr].Size, DS->Num, DS->Ret);
#endif
  }
  return NULL;
}

int wmain(int argc, wchar_t *argv[]) {
  uint32_t Cntr = 0;
  HANDLE ReadMouseThread = NULL;
  HANDLE DecodeThread[SLICE_NUM];
  char **Args = NULL;
  int32_t LNum = 1;
  int32_t RNum = 1;
  int32_t LPort = 3000;
  int32_t RPort = 4000;
  int32_t Nom = 1;
  int32_t Denom = 1;
  int32_t GameMode = 0;
  int32_t Iterations = 1000000;

  DWORD dwThreadIdArray;
  DisplayWindow DW;
#ifdef MOUSE_TEST
  DW.createWindow(1024, 768, 0);
  Running = 1;
  ReadMouseThread = CreateThread(NULL, 0, testVirtualMouse, &DW, 0, &dwThreadIdArray);
  while (Running && Cntr < 500) {
    // running test for not more than 500 sec
    Sleep(1000);
    Cntr++;
    Running = DW.isRunning();
  }
  if (ReadMouseThread)
    TerminateThread(ReadMouseThread, 0);
  return 0;
#else
  avcodec_register_all();
  Ml.setCommDeviceNumber(LNum, RNum);
  int ArgsSize = 0;
  Args = parseClientOpts(&ArgsSize, argc, argv, &LNum, &RNum, &LPort, &RPort,
                         &Nom, &Denom, &GameMode, &Iterations);
  if (!Args) {
    printf("Error reading options!\n");
    return -1;
  }

  printf("Setting up %d local and %d remote:\n", LNum, RNum);
  Ml.setCommDeviceNumber(LNum, RNum);
  if (LNum < 0 || LNum > 32 || RNum < 0 || RNum > 32) {
    printf("Error! Unsupported number of local or remote links defined by -l, -r\n");
    printf("Should be between 0 and 32.\n");
    delete [] Args;
    return -1;
  }
  if (Iterations < 0 || Iterations > 1 << 30) {
    printf("Error! Unsupported number of iterations defined by -I\n");
    printf("Should be between 0 and 2^30.\n");
    delete [] Args;
    return -1;
  }
  for (int i = 0; i < LNum; i++) {
    char *IfaceName;
    int32_t Port;
    if (Args[i] == NULL || i >= ArgsSize) {
      printf("Warning: local name is missing, set only %d local devices!\n", i);
      break;
    }
    // if port specified "lo:3000"
    int32_t Len = strlen(Args[i]);
    IfaceName = new char[Len];
    if (splitNamePort(Args[i], Len, IfaceName, Port)) {
      printf("Local %s, port %d\n", IfaceName, Port);
      Ml.addLocalIfaceAndPort(IfaceName, Port);
    // if only name specified
    } else {
      printf("Local %s, port %d\n", Args[i], LPort + i);
      Ml.addLocalIfaceAndPort((char *)Args[i], LPort + i);
    }
    delete [] IfaceName;
  }
  for (int i = 0; i < RNum; i++) {
    char *IfaceName;
    int32_t Port;
    if (Args[LNum + i] == NULL || LNum + i >= ArgsSize) {
      printf("Warning: local name is missing, set only %d remote addrs!\n", i);
      break;
    }
    // if port specified "127.0.0.1:3000"
    int32_t Len = strlen(Args[LNum + i]);
    IfaceName = new char[Len];
    if (splitNamePort(Args[LNum + i], Len, IfaceName, Port)) {
      printf("Remote %s, port %d\n", IfaceName, Port);
      Ml.addRemoteAddrAndPort(IfaceName, Port);
    // if only name specified
    } else {
      printf("Remote %s, port %d\n", Args[LNum + i], RPort + i);
      Ml.addRemoteAddrAndPort((char *)Args[LNum + i], RPort + i);
    }
    delete [] IfaceName;
  }
  printf("Setting redundancy info(%d), redundancy(%d)\n", Nom, Denom);
  Ml.setRedundancy(Denom, Nom);
  Ml.initiateLinks();
#endif /* MOUSE_TEST */

  Sleep(1000);
  VideoDecode *V = new VideoDecode[SLICE_NUM];

  int32_t DInit = 0;
  for (int i = 0; i < SLICE_NUM; i++) {
    DInit |= V[i].init(i);
  }
  if (DInit == 0)
    printf("Init of %d slices decoders compled\n", SLICE_NUM);
  else
    printf("Warning! Some slices decoders init incomple\n");

  Ml.printLinksInfo();
  Cntr = 1;
  bool WindowOpened = false;
  uint8_t *FrameNum = new uint8_t[SLICE_NUM];
  *((uint64_t *)FrameNum) = 0;

  TimeStat RtTime;
  int64_t RtTimeDiff = 0;
  DecodeSlice DS[SLICE_NUM];
  for (int i = 0; i < SLICE_NUM; i++) {
    DS[i].V = &V[i];
    DS[i].Num = i;
    DecodeThread[i] = CreateThread(NULL, 0, decodeOneSlice, &DS[i], 0, &dwThreadIdArray);
  }
  // Main loop
  while (Cntr < Iterations && Running) {
    for (int i = 0; i < SLICE_NUM; i++) {
      uint16_t RecSizeTmp;
      uint16_t Ptr;
      uint8_t *RecDataTmp = Mb.getFreePtr(&Ptr);
      if (!Running)
        break;
      Ml.receive(&RecSizeTmp, RecDataTmp);
#ifdef ROUND_TRIP
      if (RecSizeTmp == 12) {
        uint64_t TimeRec = *((uint64_t *)RecDataTmp);
        uint64_t CurTime = Ml.getLocalTime();
        RtTimeDiff = CurTime - TimeRec;
        printf("Round trip time %ld\n", RtTimeDiff / 1000);
#ifdef PRINT_WARN
        if (RtTimeDiff < 0) {
          printf("Warning! Bad round trip time %ld %lu %lu\n", RtTimeDiff,
                 CurTime, TimeRec);
          continue;
        }
#endif
        RtTime.add(RtTimeDiff);
        AvgRT = (int32_t)RtTime.getAvg();
        continue;
      }
#endif
      if (RecSizeTmp >= 4 && RecDataTmp[2] == 0 &&
          RecDataTmp[3] == 1) {
        uint32_t SliceNum = RecDataTmp[0];
        if (SliceNum < SLICE_NUM) {
          DS[SliceNum].FrameNum = RecDataTmp[1];
          RecDataTmp[0] = 0;
          RecDataTmp[1] = 0;
          Mb.writeSlice(SliceNum, DS[SliceNum].FrameNum, RecSizeTmp, Ptr);
          DS[SliceNum].InSize = RecSizeTmp;
        }
      }
    }
    if (!WindowOpened && DS[0].Ret == 0) {
      int32_t Flags = 0;
      if (GameMode)
        Flags |= GAME_MODE;
      DW.createWindow(V[0].getCtxWidth() * SLICE_NUM_X,
                      V[0].getCtxHeight() * SLICE_NUM_Y, Flags);
      // let sometime to create a window
      Sleep(1000);
      for (int i = 0; i < SLICE_NUM; i++) {
        DS[i].OutData = DW.getSlicePtr(i);
        DS[i].WindowOpened = true;
      }
      ReadMouseThread = CreateThread(NULL, 0, readVirtualMouseKbd, &DW, 0, &dwThreadIdArray);
      WindowOpened = true;
    }
    Cntr++;
  }
#ifdef ROUND_TRIP
  RtTime.print("Network round trip");
#endif
  Ml.printLinksInfo();
  Running = 0;
  DW.stopRunning();
  Sleep(1000);
  delete[] FrameNum;
  delete[] V;
  if (ReadMouseThread)
    TerminateThread(ReadMouseThread, 0);

  for (int i = 0; i < SLICE_NUM; i++) {
    if (DecodeThread[i])
      TerminateThread(DecodeThread[i], 0);
  }
  delete [] Args;
  return 0;
}

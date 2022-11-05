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
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fcntl.h>

#include "Config.h"
#include "Options.h"
#include "VideoDecode.h"
#include "DisplayWindow.h"
#include "MouseKbdRead.h"
#include "MemoryBuffer.h"
#include "TimeStat.h"

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

void *decodeSlice(void *Ptr) {
  if (Ptr == NULL) {
    printf("Error! Wrong decode slice thread parameter!\n");
    return NULL;
  }
  DecodeSlice *DS = (DecodeSlice *)Ptr;
  int32_t FrameNum = -1;
  while (Running) {
    int16_t SPtr;
    uint8_t *SlicePtr = Mb.getSlicePtr(DS->Num, &FrameNum, &SPtr);
    if (!SlicePtr) {
      sleep(0.001);
      continue;
    }
    DS->Ret = DS->V->decodeFrame(DS->OutData, Mb.getSliceSize(SPtr), SlicePtr,
                                 DS->Num, DS->WindowOpened);
    if (FrameNum != -1)
      FrameNum = (FrameNum + 1) & 255;
#ifdef PRINT_WARN
    if (DS->Ret != 0)
      printf("Warning: unable to decode received packet size %d, slice %d "
             "maybe missing, error %d\n", Mb.getSliceSize(SPtr), DS->Num, DS->Ret);
#endif
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  uint32_t Cntr = 0;
  pthread_t ReadMouseThread;
  pthread_t DecodeThread[SLICE_NUM];
  const char **Args;
  poptContext optCon;
  int32_t LNum = 1;
  int32_t RNum = 1;
  int32_t LPort = 3000;
  int32_t RPort = 4000;
  int32_t Nom = 1;
  int32_t Denom = 1;
  int32_t GameMode = 0;
  int32_t FlipWait = 0;
  int32_t Iterations = 1000000;

  avcodec_register_all();
  DisplayWindow DW;

#ifdef MOUSE_TEST
  DW.createWindow(1024, 768, 0);
  sleep(1);
  pthread_create(&ReadMouseThread, NULL, testVirtualMouse, &DW);
  while (DW.isRunning() && Cntr < 500) {
    // running test for not more than 500 sec
    sleep(1);
    Cntr++;
  }
  pthread_cancel(ReadMouseThread);
  return 0;
#endif /* MOUSE_TEST */
  Args = parseClientOpts(&optCon, argc, argv, &LNum, &RNum, &LPort, &RPort,
                         &Nom, &Denom, &GameMode, &FlipWait, &Iterations);
  if (!Args) {
    printf("Error reading options!\n");
    return -1;
  }
  Mb.setFlipWait(FlipWait);
  printf("Setting up %d local and %d remote:\n", LNum, RNum);
  Ml.setCommDeviceNumber(LNum, RNum);
  for (int i = 0; i < LNum; i++) {
    char *IfaceName;
    int32_t Port;
    if (Args[i] == NULL) {
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
    if (Args[LNum + i] == NULL) {
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
//  setuid(1002);

  sleep(1);
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
    pthread_create(&DecodeThread[i], NULL, decodeSlice, &DS[i]);
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
      sleep(1);
      for (int i = 0; i < SLICE_NUM; i++) {
        DS[i].OutData = DW.getSlicePtr(i);
        DS[i].WindowOpened = true;
      }
      pthread_create(&ReadMouseThread, NULL, readVirtualMouseKbd, &DW);
      WindowOpened = true;
    }
    Cntr++;
  }
#ifdef ROUND_TRIP
  RtTime.print("Network round trip");
#endif
  Ml.printLinksInfo();
  Running = 0;
  sleep(1);
  DW.stopRunning();
  delete[] FrameNum;
  delete[] V;
  if (WindowOpened)
    pthread_cancel(ReadMouseThread);

  for (int i = 0; i < SLICE_NUM; i++) {
    pthread_cancel(DecodeThread[i]);
  }
  poptFreeContext(optCon);
  return 0;
}

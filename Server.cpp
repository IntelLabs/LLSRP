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
 *             Andrey Belogolovy
 *             e-mail: andrey.belogolovy@intel.com
 *                     a.v.belogolovy@gmail.com
 */

#include "multilink.h"
#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include "Config.h"
#include "VideoEncodeX.h"
#include "ScreenStreamX.h"
#include "Options.h"
#include "MouseKbdWrite.h"
#include "TimeStat.h"
#include "GrabWindow.h"

MultiLink Ml;
int32_t MouseX = 0;
int32_t MouseY = 0;
uint64_t GlobalTime;
int32_t Running = 1;

extern char *DispStr;

int main(int argc, char *argv[]) {
  uint32_t Cntr;
  int32_t FPS;
  char *DisplayStr = NULL;
  pthread_t MouseThread;
  int32_t ConstRate = 0;
  poptContext optCon;
  const char **Args;
  int32_t VSize = 0;
  int32_t LNum = 1;
  int32_t RNum = 1;
  int32_t LPort = 4000;
  int32_t Nom = 1;
  int32_t Denom = 1;
  int32_t Iterations = 1000000;
  int32_t Width, Height;

//  av_register_all();
//  avdevice_register_all();
  avcodec_register_all();

  Args = parseServerOpts(&optCon, argc, argv, &LNum, &RNum, &LPort, &Nom, &Denom,
                         &ConstRate, &VSize, &DisplayStr, &Iterations);

  if (!Args) {
    printf("Error reading options!\n");
    return -1;
  }

  ModeOption MOpt = static_cast<ModeOption>(VSize);
  setDimentions(MOpt, Width, Height, FPS);

  GrabWindow GW;
  if (!GW.grabWindowInit(Width, Height, DisplayStr)) {
    printf("Unable to grab a screen for display");
    if (DisplayStr)
      printf(" %s\n", DisplayStr);
    else
      printf(" (default)\n");
    poptFreeContext(optCon);
    return -1;
  }

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
//  setuid(1002);

  printf("Setting redundancy info(%d), redundancy(%d)\n", Nom, Denom);
  Ml.setRedundancy(Denom, Nom);

  XImage *image = GW.getImage();
  VideoEncodeX *V = new VideoEncodeX[SLICE_NUM];
  ScreenStreamX *S = new ScreenStreamX[SLICE_NUM];
  int32_t SInit = 0;
  int32_t VInit = 0;
  for (int i = 0; i < SLICE_NUM; i++) {
    if (image)
      SInit |= S[i].init(i % SLICE_NUM, (uint32_t *)image->data,
                         FPS, Width, Height);
    else
      SInit |= S[i].init(i % SLICE_NUM, GW.getData(),
                         FPS, Width, Height);
    VInit |= V[i].init(S[i].getCtxWidth(),
                       S[i].getCtxHeight(), S[i].getCtxFPS(), i);
  }
  if (SInit == 0)
    printf("Init of %d stream slices compled\n", SLICE_NUM);
  else
    printf("Warning! Some stream slices init incomple\n");
  if (VInit == 0)
    printf("Init of %d encode slices compled\n", SLICE_NUM);
  else
    printf("Warning! Some encode slices init incomple\n");

  Ml.printLinksInfo();
  Cntr = 0;

  int32_t FrameNum = 0, Rate;
  FPS = S[0].getCtxFPS();
  if (FPS == 0) {
    delete [] V;
    delete [] S;
    printf("Error FPS can't be Zero\n");
    exit(-1);
  }

  sleep(3);
  if ((DisplayStr && *DisplayStr != '\0') || !DisplayStr)
    pthread_create(&MouseThread, NULL, emulateVirtualMouseKbd, &GW);
  else
    pthread_create(&MouseThread, NULL, testVirtualMouseKbd, &GW);

  uint8_t *PktData[SLICE_NUM];
  int32_t PktSize[SLICE_NUM];
  int32_t RetDec[SLICE_NUM], RetEnc[SLICE_NUM];
  TimeStat Frame;
  int64_t FrameTime = 0;
  FPS = S[0].getCtxFPS();
  if (FPS == 0) {
    printf("Error FPS can't be Zero\n");
    exit(-1);
  }
  int32_t Quality = 31;
  int32_t AvgRate = 2048 * SLICE_NUM;
  while (Cntr < Iterations && Running) {
    uint64_t StopTime;
    uint64_t StartTime = Ml.getLocalTime();
    if (ConstRate > 0)
      Rate = ConstRate;
    else
      Rate = Ml.getRate(); // Assume in Kb per packet

    if (image) {
      XShmGetImage(GW.getDisplay(), XDefaultRootWindow(GW.getDisplay()), image, 0, 0, AllPlanes);
      XSync(GW.getDisplay(), 0);
    }
#pragma omp parllel for
    for (int i = 0; i < SLICE_NUM; i++) {
      V[i].initPkt();
      RetDec[i] = S[i].decodeFrame(V[i].getFramePtr());
      RetEnc[i] = V[i].encodeFrame(FrameNum, &PktSize[i], &PktData[i], Quality);
      if (RetEnc[i] == 0 && PktSize[i]) {
        Ml.send(PktSize[i], PktData[i]);
      }
    }
    int32_t FrameSize = 0;
    for (int i = 0; i < SLICE_NUM; i++) {
#ifdef CODEC_OUTPUT_CHECK
      if (RetDec[i] < 0 || RetEnc[i] < 0) {
        Running = 0;
        sleep(1);
        pthread_cancel(MouseThread);
        Ml.printLinksInfo();
        poptFreeContext(optCon);
        delete [] V;
        delete [] S;
        if (RetDec[i] < 0) {
          return RetDec[i]; // Error decoding stream
        if (RetEnc[i] < 0) {
          return RetEnc[i]; // Error encoding frame
      }
#endif /* CODEC_OUTPUT_CHECK */
      FrameSize += PktSize[i];
      V[i].unrefPkt();
    }
    // Rate control.
    // If average rate is bigger than target then increase frame rate factor
    //  until meet target rate
    // If average rate is less than target then decrease frame rate factor
    //  until meet target rate
    AvgRate = (2 * AvgRate + FrameSize) / 3;
    float RateRatio = 1;
    if (AvgRate != 0)
      RateRatio = (float)Rate / AvgRate / FPS;

    if (Quality < 50 && RateRatio < 0.9 && FrameSize > 2048 * SLICE_NUM) {
      Quality += 1;
    } else if (Quality > 20 && RateRatio > 1.1 &&
               FrameSize < 16384 * SLICE_NUM) {
      Quality -= 1;
    }

    FrameNum++;
    StopTime = Ml.getLocalTime();
    FrameTime = (StopTime - StartTime);
    if (FrameTime < 0)
      FrameTime = 1;
    FrameTime++;
    if (1000000 < FPS * FrameTime) {
      printf("Warning! Screen capture and/or encoding (%ld) is slower than FPS(%d)!\n",
             1000000 / FrameTime,
             FPS);
    }
    if (FrameTime < 100) {
      printf("Warning! Wrong time calculation. Time %ld is less than 1ms.\n",
             FrameTime);
    } else {
      Frame.add(FrameTime);
      FrameTime = (Ml.getLocalTime() - StartTime);
      // Waiting to reach desired FPS
      while (1000000 - 10000 >= FPS * FrameTime) {
        sleep(0.001);
        FrameTime = (Ml.getLocalTime() - StartTime);
      }
    }
    Cntr++;
  }
  Frame.print("Encoding and capture");
  Running = 0;
  sleep(1);
  pthread_cancel(MouseThread);
  uint8_t Quit[5] = {'Q', 'u', 'i', 't', '\0'};
  Ml.send(5, Quit);
  Ml.printLinksInfo();
  poptFreeContext(optCon);
  delete [] V;
  delete [] S;
  return 0;
}

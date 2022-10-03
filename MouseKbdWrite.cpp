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
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include "Config.h"

#include "DisplayWindow.h"
#include "GrabWindow.h"
#include "TimeStat.h"

extern MultiLink Ml;
extern int32_t MouseX;
extern int32_t MouseY;
extern int64_t GlobalTime;
extern int32_t Running;

char *DispStr;

void *emulateVirtualMouseKbd(void *Ptr) {
  GrabWindow *GW = (GrabWindow *)Ptr;
  if (!GW) {
    printf("Error: can not get Grab Window pointer\n");
    return NULL;
  }
  Display *DisplayCur;
  DisplayCur = GW->getIODisplay();
  if (!DisplayCur)
    return NULL;
  uint16_t RecSize;
  uint8_t *RecData = new uint8_t[65536];
#ifdef ROUND_TRIP
  int32_t AvgRT = 0;
#endif
//  Window MainWindow;
//  MainWindow = RootWindow(DisplayCur, DefaultScreen(DisplayCur));
//  Window WinFocus = MainWindow;
//  XGetInputFocus(DisplayCur, &WinFocus, &Revert);
  int32_t BMask = 0, PrevMask = 0;
  TimeStat Io;
  bool SyncMouse = true;
  bool FirstPacket = true;
  while (Running) {
    Ml.receive(&RecSize, RecData);
    uint64_t StartTime = Ml.getLocalTime();
    uint64_t StopTime;
    if (RecSize == 7 + 8) {
      int32_t AbsX = *((int16_t *)&RecData[1 + 8]);
      int32_t AbsY = *((int16_t *)&RecData[3 + 8]);
      // update cursor position

      MouseX = AbsX;
      MouseY = AbsY;

      int32_t RelX = (int8_t) RecData[5 + 8];
      int32_t RelY = (int8_t) RecData[6 + 8];

      GlobalTime = *((uint64_t *)RecData);

      // get mouse click data
      BMask = RecData[8];
      int32_t ButtonP = 0;
      bool ButtonStatus = False;
      if (int32_t DiffMask = PrevMask ^ BMask) {
          if (DiffMask & 1) {
            ButtonP = 1;
            if (BMask & 1)
              ButtonStatus = True;
            else
              ButtonStatus = False;
          }
          DiffMask >>= 1;
          BMask >>= 1;
          if (DiffMask & 1) {
            ButtonP = 2;
            if (BMask & 1)
              ButtonStatus = True;
            else
              ButtonStatus = False;
          }
          DiffMask >>= 1;
          BMask >>= 1;
          if (DiffMask & 1) {
            ButtonP = 3;
            if (BMask & 1) {
              ButtonStatus = True;
            } else {
              ButtonStatus = False;
            }
          }
          DiffMask >>= 1;
          BMask >>= 1;
          if (DiffMask & 1) {
            ButtonP = 4;
            if (BMask & 1) {
              ButtonStatus = True;
            } else {
              ButtonStatus = False;
            }
          }
          DiffMask >>= 1;
          BMask >>= 1;
          if (DiffMask & 1) {
            ButtonP = 5;
            if (BMask & 1) {
              ButtonStatus = True;
            } else {
              ButtonStatus = False;
            }
          }
      }
      BMask = RecData[8];

      // perform actual mouse move and click
      if (FirstPacket || SyncMouse) {
        Window Win, Child;
        FirstPacket = false;
        // sync mouse coordinates each second
        int32_t RootX, RootY, WinX, WinY;
        uint32_t Mask;
        XQueryPointer(DisplayCur, RootWindow(DisplayCur, DefaultScreen(DisplayCur)),
                      &Win, &Child, &RootX, &RootY, &WinX, &WinY, &Mask);
        if (PrevMask ^ BMask && ButtonP) {
          XTestFakeButtonEvent(DisplayCur, ButtonP, ButtonStatus, 0);
        }
        if (RootX != AbsX || RootY != AbsY) {
          XTestFakeMotionEvent(DisplayCur, 0, AbsX, AbsY, 0);
        }
      } else {
        if (PrevMask ^ BMask && ButtonP) {
          XTestFakeButtonEvent(DisplayCur, ButtonP, ButtonStatus, 0);
        }
        if (RelX || RelY) {
          XTestFakeRelativeMotionEvent(DisplayCur, RelX, -RelY, 0);
        }
      }
      PrevMask = RecData[8];
    }
#ifdef ROUND_TRIP
    // service packet - measure round trip time
    else if (RecSize == 12) {
      AvgRT = *((int32_t *)(RecData + 8));
      Ml.send(12, RecData);
      printf("Average round trip time %d ms\n", AvgRT / 1000);
    }
#endif
    else if (RecSize == sizeof(XKeyEvent)) {
        Display *DisplayCur = GW->getIODisplay();
        XKeyEvent *KEvent = (XKeyEvent *)RecData;
        KEvent->keycode = XKeysymToKeycode(DisplayCur, KEvent->serial);
        if (XK_Escape == KEvent->serial && (BMask & 1)) {
          // release all mouse buttons before exit
          XTestFakeButtonEvent(DisplayCur, 1, False, 0);
          XTestFakeButtonEvent(DisplayCur, 2, False, 0);
          XTestFakeButtonEvent(DisplayCur, 3, False, 0);
          XTestFakeButtonEvent(DisplayCur, 4, False, 0);
          XTestFakeButtonEvent(DisplayCur, 5, False, 0);
          printf("Exiting...\n");
          Io.print("Mouse/kbd emulate");
          sleep(1);
          Running = 0;
          delete [] RecData;
          return NULL;
        }
        if (XK_Shift_L == KEvent->serial && (BMask & 1)) {
          printf("Mouse is relative now...\n");
          SyncMouse = false;
        } else if (XK_Shift_R == KEvent->serial && (BMask & 1)) {
          printf("Mouse is synced now...\n");
          SyncMouse = true;
        }
        if (KEvent->type == KeyPress) {
          XTestFakeKeyEvent(DisplayCur, KEvent->keycode, true, 0);
        } else if(KEvent->type == KeyRelease) {
          XTestFakeKeyEvent(DisplayCur, KEvent->keycode, false, 0);
        }
    }
    XFlush(DisplayCur);
    XSync(DisplayCur, true);
    StopTime = Ml.getLocalTime();
    Io.add(StopTime - StartTime);
  }
  delete [] RecData;
  Io.print("Mouse/kbd emulate");
  // release all mouse buttons before exit
  XTestFakeButtonEvent(DisplayCur, 1, False, 0);
  XTestFakeButtonEvent(DisplayCur, 2, False, 0);
  XTestFakeButtonEvent(DisplayCur, 3, False, 0);
  return NULL;
}

void *testVirtualMouseKbd(void *Ptr) {
  GrabWindow *GW = (GrabWindow *)Ptr;
  if (!GW) {
    printf("Error: can not get Grab Window pointer\n");
    return NULL;
  }
  uint16_t RecSize;
  uint8_t *RecData = new uint8_t[65536];
#ifdef ROUND_TRIP
  int32_t AvgRT = 0;
#endif
  int32_t BMask = 0;
  TimeStat Io;
  int32_t PAbsX = 0;
  int32_t PAbsY = 0;
  while (Running) {
    Ml.receive(&RecSize, RecData);
    uint64_t StartTime = Ml.getLocalTime();
    uint64_t StopTime;
    if (RecSize == 7 + 8) {
      int32_t AbsX = *((int16_t *)&RecData[1 + 8]);
      int32_t AbsY = *((int16_t *)&RecData[3 + 8]);
      // update cursor position

      MouseX = AbsX;
      MouseY = AbsY;

      GlobalTime = *((uint64_t *)RecData);

      // get mouse click data
      BMask = RecData[8];
      // perform actual mouse move and click
      if (BMask & 1)
        GW->drawLine(AbsX, AbsY, PAbsX, PAbsY, 0x00ff00);
      PAbsX = AbsX;
      PAbsY = AbsY;
    }
#ifdef ROUND_TRIP
    // service packet - measure round trip time
    else if (RecSize == 12) {
      AvgRT = *((int32_t *)(RecData + 8));
      Ml.send(12, RecData);
      printf("Average round trip time %d ms\n", AvgRT / 1000);
    }
#endif
    else if (RecSize == sizeof(XKeyEvent)) {
        XKeyEvent *KEvent = (XKeyEvent *)RecData;
//        KEvent->keycode = XKeysymToKeycode(DisplayCur, KEvent->serial);
        if (XK_Escape == KEvent->serial && (BMask & 1)) {
          printf("Exiting...\n");
          Io.print("Mouse/kbd emulate");
          sleep(1);
          Running = 0;
          delete [] RecData;
          return NULL;
        }
    }
    StopTime = Ml.getLocalTime();
    Io.add(StopTime - StartTime);
  }
  delete [] RecData;
  Io.print("Mouse/kbd emulate");
  return NULL;
}



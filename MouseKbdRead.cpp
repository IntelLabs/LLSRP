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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <X11/Xlib.h>

#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include "DisplayWindow.h"

#include "Config.h"

extern MultiLink Ml;
extern int32_t Running;
extern int32_t AvgRT;

#include "DisplayWindow.h"
#include "TimeStat.h"

void *readVirtualMouseKbd(void *Ptr) {
  int8_t Data[7 + 8]; // array for recieved data
  DisplayWindow *DW = (DisplayWindow *)Ptr;

  while(!DW || !DW->isWindowOpened()) {
    sleep(0.01);
  }
  Display *DisplayCur;
  int32_t RootX, RootY, WinX, WinY;
  uint32_t Mask = 0, PrevMask;
  Window Win, Child;
  int32_t RootXp, RootYp, WinXp, WinYp;
  DisplayCur = XOpenDisplay(NULL);
  if (!DisplayCur) {
    DW->stopRunning();
    printf("Error! Unable to open display!\n");
    exit(1);
  }

  XEvent Event;
  XKeyEvent *KEvent;
  Window WinFocus;
//  XAllowEvents(DisplayCur, ReplayKeyboard, 0);
  WinFocus = DW->getDrawWin();
//  XGetInputFocus(DisplayCur, &WinFocus, &Revert);
  XGrabKeyboard(DisplayCur, WinFocus, false, GrabModeAsync, GrabModeAsync, 0);
//  XSelectInput(DisplayCur, WinFocus, KeyPress|KeyRelease);
  TimeStat Io;
  int64_t IoTime = 0;
  int32_t MouseRel = 0;
  int32_t SendGameMode = 0;
  if (DW->checkFlags(GAME_MODE)) {
    MouseRel = 1;
    SendGameMode = 1;
  }
#ifdef ROUND_TRIP
  int64_t RoundTripTime = Ml.getLocalTime();
#endif
  XQueryPointer(DisplayCur, WinFocus, &Win, &Child,
                &RootXp, &RootYp, &WinXp, &WinYp, &PrevMask);

  while (DW->isRunning()) {
    uint64_t StartTime = Ml.getLocalTime();
    uint64_t StopTime;
    XQueryPointer(DisplayCur, WinFocus, &Win, &Child,
                  &RootX, &RootY, &WinX, &WinY, &Mask);
    // Fill Data with time, relative and absolute  mouse coordinates
    *((uint64_t *)Data) = Ml.getLocalTime();
    Data[8 + 5] = RootX - RootXp;
    Data[8 + 6] = -(RootY - RootYp);
    Data[8 + 0] = Mask >> 8;

    RootXp = RootX;
    RootYp = RootY;
    if (MouseRel) {
      if (WinX <= 0) {
        XTestFakeMotionEvent(DisplayCur, 0, -WinX + RootX + 1, RootY, 0);
        RootXp = -WinX + RootX + 1;
      }
      if (WinX >= DW->getWidth()) {
        XTestFakeMotionEvent(DisplayCur, 0, -WinX + RootX + DW->getWidth() - 1, RootY, 0);
        RootXp = -WinX + RootX + DW->getWidth() - 1;
      }
      if (WinY <= 0) {
        XTestFakeMotionEvent(DisplayCur, 0, RootX, -WinY + RootY + 1, 0);
        RootYp = -WinY + RootY + 1;
      }
      if (WinY >= DW->getHeight()) {
        XTestFakeMotionEvent(DisplayCur, 0, RootX, -WinY + RootY + DW->getHeight() - 1, 0);
        RootYp = -WinY + RootY + DW->getHeight() - 1;
      }
    }

    if (WinX >= DW->getWidth())
      WinX = DW->getWidth() - 1;
    if (WinY >= DW->getHeight())
      WinY = DW->getHeight() - 1;
    if (WinX < 0)
      WinX = 0;
    if (WinY < 0)
      WinY = 0;
    *((int16_t *)(Data + 8 + 1)) = (int16_t)WinX;
    *((int16_t *)(Data + 8 + 3)) = (int16_t)WinY;
    if (Data[8 + 5] || Data[8 + 6] || Mask != PrevMask) {
      Ml.send(7 + 8, (uint8_t *)Data);
    }
    // draw client mouse cursor
    if (Mask)
      DW->drawCursor(WinX, WinY, 250 << 8);
    else
      DW->drawCursor(WinX, WinY, 100 << 8);

    if (SendGameMode) {
      KEvent = (XKeyEvent *)&Event;
      SendGameMode = 0;
      Data[8 + 0] = 1;
      Ml.send(7 + 8, (uint8_t *)Data);
      KEvent->serial = XKeycodeToKeysym(DisplayCur,
          XKeysymToKeycode(DisplayCur, XK_Shift_L), 0);
      Ml.send(sizeof(XKeyEvent), (uint8_t *)KEvent);
      WinXp = WinX;
      WinYp = WinY;
      PrevMask = Mask;
      continue;
    }

    if (XCheckMaskEvent(DisplayCur, KeyPress, &Event)) {
      KEvent = (XKeyEvent *)&Event;
      KEvent->serial = XKeycodeToKeysym(DisplayCur, KEvent->keycode, 0);
      Ml.send(sizeof(XKeyEvent), (uint8_t *)KEvent);
      // Exit on Left mouse button + ESC
      if (XKeysymToKeycode(DisplayCur, XK_Escape) == KEvent->keycode && (Mask & 256)) {
        Data[8 + 0] = 1;
        Ml.send(7 + 8, (uint8_t *)Data);
        KEvent->serial = XKeycodeToKeysym(DisplayCur, KEvent->keycode, 0);
        Ml.send(sizeof(XKeyEvent), (uint8_t *)KEvent);
        printf("End keyboard grab\n");
        XUngrabKeyboard(DisplayCur, CurrentTime);
        DW->stopRunning();
        Running = 0;
        break;
      }
      if (XKeysymToKeycode(DisplayCur, XK_Shift_L) == KEvent->keycode && (Mask & 256)) {
        printf("Mouse now is in window bounds\n");
        MouseRel = 1;
        Data[8 + 0] = 1;
        Ml.send(7 + 8, (uint8_t *)Data);
        KEvent->serial = XKeycodeToKeysym(DisplayCur, KEvent->keycode, 0);
        Ml.send(sizeof(XKeyEvent), (uint8_t *)KEvent);
      } else if (XKeysymToKeycode(DisplayCur, XK_Shift_R) == KEvent->keycode && (Mask & 256)) {
        printf("Mouse now is out of window bounds\n");
        MouseRel = 0;
        Data[8 + 0] = 1;
        Ml.send(7 + 8, (uint8_t *)Data);
        KEvent->serial = XKeycodeToKeysym(DisplayCur, KEvent->keycode, 0);
        Ml.send(sizeof(XKeyEvent), (uint8_t *)KEvent);
      }
    } else if (XCheckMaskEvent(DisplayCur, KeyRelease, &Event)) {
      KEvent = (XKeyEvent *)&Event;
      KEvent->serial = XKeycodeToKeysym(DisplayCur, KEvent->keycode, 0);
      Ml.send(sizeof(XKeyEvent), (uint8_t *)KEvent);
    }

#ifdef ROUND_TRIP
    if (Ml.getLocalTime() - RoundTripTime > 1000000) {
      RoundTripTime = Ml.getLocalTime();
      printf("Send time and avg round trip\n");
      uint8_t SendTime[12];
      *(uint64_t *)SendTime = RoundTripTime;
      *((int32_t *)(SendTime + 8)) = AvgRT;
      Ml.send(12, SendTime);
    }
#endif
    WinXp = WinX;
    WinYp = WinY;
    PrevMask = Mask;

    StopTime = Ml.getLocalTime();
    IoTime = (StopTime - StartTime);
    Io.add(IoTime);
  }
  XCloseDisplay(DisplayCur);
  Io.print("Mouse read");
  return NULL;
}

void *testVirtualMouse(void *Ptr) {
  DisplayWindow *DW = (DisplayWindow *)Ptr;

  while(!DW || !DW->isWindowOpened()) {
    sleep(0.1);
  }

  Display *DisplayCur;
  int32_t RootX, RootY, WinX, WinY;
  uint32_t Mask, PrevMask;
  Window Win, Child;
  int32_t RootXp, RootYp;
  DisplayCur = XOpenDisplay(NULL);
  if (!DisplayCur) {
    DW->stopRunning();
    printf("Error! Unable to open display!\n");
    exit(1);
  }

  XEvent Event;
  XKeyEvent *KEvent;
  Window WinFocus = DW->getDrawWin();
  XGrabKeyboard(DisplayCur, WinFocus, false,
                GrabModeAsync, GrabModeAsync, 0);

  int32_t Running = DW->isRunning();
  XQueryPointer(DisplayCur, WinFocus, &Win, &Child,
                &RootXp, &RootYp, &WinX, &WinY, &PrevMask);

  while (Running) {
    if (XCheckMaskEvent(DisplayCur, KeyPress, &Event)) {
      KEvent = (XKeyEvent *)&Event;
      if (XKeysymToKeycode(DisplayCur, XK_Escape) == KEvent->keycode) {
        printf("End keyboard grab\n");
        XUngrabKeyboard(DisplayCur, CurrentTime);
        XCloseDisplay(DisplayCur);
        DW->stopRunning();
        Running = 0;
        exit(1);
      }
      if (XKeysymToKeycode(DisplayCur, XK_C) == KEvent->keycode) {
        DW->clearImage();
      }
    }
    XQueryPointer(DisplayCur, WinFocus, &Win, &Child,
                  &RootX, &RootY, &WinX, &WinY, &Mask);
    RootX = WinX;
    RootY = WinY;

    if (Mask >> 8 == 1)
      DW->drawLine(RootX, RootY, RootXp, RootYp, 200);

    RootXp = RootX;
    RootYp = RootY;
    PrevMask = Mask;
  }
  return NULL;
}

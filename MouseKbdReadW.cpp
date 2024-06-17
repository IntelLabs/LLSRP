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
#include <winuser.h>
#include "DisplayWindowW.h"
#include "Config.h"

extern MultiLink Ml;
extern int32_t Running;
extern int32_t AvgRT;

#include "TimeStat.h"
DWORD WINAPI readVirtualMouseKbd(LPVOID Arg) {
  int8_t Data[7 + 8]; // array for recieved data
  DisplayWindow *DW = (DisplayWindow *)Arg;

  while(!DW || !DW->isWindowOpened()) {
    Sleep(10);
  }
  int32_t RootX = 0, RootY = 0, WinX = 0, WinY = 0;
  uint32_t Mask = 0, PrevMask = 0;
  int32_t RootXp = 0, RootYp = 0, WinXp = 0, WinYp = 0;

  TimeStat Io;
  int64_t IoTime = 0;
  int32_t MouseRel = 0;
  int32_t SendGameMode = 0;
  uint8_t SendB[96];
  SDL_GetMouseState(&WinXp, &WinYp);
  if (DW->checkFlags(GAME_MODE)) {
    MouseRel = 1;
    SendGameMode = 1;
    printf("Game mode!\n");
    if (SendGameMode) {
      SendGameMode = 0;
      Data[8 + 5] = 0;
      Data[8 + 6] = 0;
      Data[8 + 0] = 1;
      SDL_GetMouseState(&WinX, &WinY);
      *((int16_t *)(Data + 8 + 1)) = (int16_t)WinX;
      *((int16_t *)(Data + 8 + 3)) = (int16_t)WinY;
      *((uint64_t *)Data) = Ml.getLocalTime();
      Ml.send(7 + 8, (uint8_t *)Data);
      *((int32_t *)SendB) = 2; // key down
      *((int64_t *)SendB + 1) = 65505;
      Ml.send(96, SendB);
      Sleep(16);
      Data[8 + 0] = 0;
      Ml.send(7 + 8, (uint8_t *)Data);
      *((int32_t *)SendB) = 3;  // key up
      Ml.send(96, SendB);
      printf("Game mode sent to server\n");
    }
    SDL_CaptureMouse(SDL_TRUE);
  }
#ifdef ROUND_TRIP
  int64_t RoundTripTime = Ml.getLocalTime();
#endif
  SDL_Event e;
  while (DW->isRunning()) {
    uint64_t StartTime = Ml.getLocalTime();
    uint64_t StopTime;
    SDL_PollEvent(&e);
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
      SDL_KeyboardEvent *key = (SDL_KeyboardEvent *)&e;
      *((int32_t *)SendB) = ((e.type == SDL_KEYDOWN) ? 2 : 3);
      switch(key->keysym.scancode) {
        case SDL_SCANCODE_UP: // up
          *((int64_t *)SendB + 1) = 65362;
          break;
        case SDL_SCANCODE_DOWN: // down
          *((int64_t *)SendB + 1) = 65364;
          break;
        case SDL_SCANCODE_LEFT: // left
          *((int64_t *)SendB + 1) = 65361;
          break;
        case SDL_SCANCODE_RIGHT: // right
          *((int64_t *)SendB + 1) = 65363;
          break;
        case SDL_SCANCODE_ESCAPE: // esc
          *((int64_t *)SendB + 1) = 65307;
          break;
        case SDL_SCANCODE_BACKSPACE: // backspace
          *((int64_t *)SendB + 1) = 65288;
          break;
        case SDL_SCANCODE_RETURN: // enter
          *((int64_t *)SendB + 1) = 65293;
          break;
        case SDL_SCANCODE_LSHIFT: // left_shift
          *((int64_t *)SendB + 1) = 65505;
          if (Mask & 1) {
            SDL_CaptureMouse(SDL_TRUE);
            MouseRel = 1;
          }
          break;
        case SDL_SCANCODE_RSHIFT: // right_shift
          *((int64_t *)SendB + 1) = 65506;
          if (Mask & 1) {
            SDL_CaptureMouse(SDL_FALSE);
            MouseRel = 0;
          }
          break;
        case SDL_SCANCODE_LCTRL: // left_cntr
          *((int64_t *)SendB + 1) = 65507;
          break;
        default:
          // all letters
          if ((key->keysym.sym >= 44 && key->keysym.sym <= 122) || key->keysym.sym == 32)
            *((int64_t *)SendB + 1) = (int32_t)key->keysym.sym;
          break;
      }
      Ml.send(96, SendB);
      if (key->keysym.scancode == SDL_SCANCODE_ESCAPE && (Mask & 1)) {
        DW->stopRunning();
        Running = 0;
        break;
      }
    }
    // Fill Data with time, relative and absolute  mouse coordinates
    Mask = SDL_GetMouseState(&WinX, &WinY);
    *((uint64_t *)Data) = Ml.getLocalTime();
    Data[8 + 5] = WinX - WinXp;
    Data[8 + 6] = -(WinY - WinYp);
    Data[8 + 0] = Mask;

    RootXp = RootX;
    RootYp = RootY;
    if (MouseRel) {
      if (WinX <= 0) {
        SDL_WarpMouseInWindow(DW->getDrawWin(), 1, WinY);
        RootXp = -WinX + RootX + 1;
      }
      if (WinX >= DW->getWidth()) {
        SDL_WarpMouseInWindow(DW->getDrawWin(), DW->getWidth() - 1, WinY);
        RootXp = -WinX + RootX + DW->getWidth() - 1;
      }
      if (WinY <= 0) {
        SDL_WarpMouseInWindow(DW->getDrawWin(), WinX, 1);
        RootYp = -WinY + RootY + 1;
      }
      if (WinY >= DW->getHeight()) {
        SDL_WarpMouseInWindow(DW->getDrawWin(), WinX, DW->getHeight() - 1);
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
    if (WinX != WinXp || WinY != WinYp || Mask != PrevMask) {
      Ml.send(7 + 8, (uint8_t *)Data);
    }
    // draw client mouse cursor
    if (Mask)
      DW->drawCursor(WinX, WinY, 250 << 8);
    else
      DW->drawCursor(WinX, WinY, 100 << 8);

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
  Io.print("Mouse read");
  return NULL;
}

DWORD WINAPI testVirtualMouse(LPVOID Arg) {
  DisplayWindow *DW = (DisplayWindow *)Arg;

  while(!DW || !DW->isWindowOpened()) {
    Sleep(100);
  }

  int32_t RootX = 0, RootY = 0, WinX = 0, WinY = 0;
  uint32_t Mask = 0, PrevMask = 0;
  int32_t RootXp = 0, RootYp = 0;
  int32_t WinXp = 0, WinYp = 0;

  int16_t EscCnt = 0;
  int32_t Running = 1;//DW->Running;
  SDL_GetMouseState(&WinXp, &WinYp);
  PrevMask = SDL_GetGlobalMouseState(&RootXp, &RootYp);
  SDL_Event e;
  TimeStat Io;
  while (Running) {
    uint64_t StartTime = Ml.getLocalTime();
    uint64_t StopTime;
    SDL_PollEvent(&e);
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
      SDL_KeyboardEvent *key = (SDL_KeyboardEvent *)&e;
      if (key->keysym.scancode == SDL_SCANCODE_ESCAPE) {
        DW->stopRunning();
        Running = 0;
        break;
      }
    }
    SDL_GetMouseState(&WinX, &WinY);
    Mask = SDL_GetGlobalMouseState(&RootX, &RootY);
    if (WinX <= 0) {
      SDL_WarpMouseInWindow(DW->getDrawWin(), 1, WinY);
      RootXp = -WinX + RootX + 1;
    }
    if (WinX >= DW->getWidth()) {
      SDL_WarpMouseInWindow(DW->getDrawWin(), DW->getWidth() - 1, WinY);
      RootXp = -WinX + RootX + DW->getWidth() - 1;
    }
    if (WinY <= 0) {
      SDL_WarpMouseInWindow(DW->getDrawWin(), WinX, 1);
      RootYp = -WinY + RootY + 1;
    }
    if (WinY >= DW->getHeight()) {
      SDL_WarpMouseInWindow(DW->getDrawWin(), WinX, DW->getHeight() - 1);
      RootYp = -WinY + RootY + DW->getHeight() - 1;
    }

    if (WinX >= DW->getWidth())
      WinX = DW->getWidth() - 1;
    if (WinY >= DW->getHeight())
      WinY = DW->getHeight() - 1;
    if (WinX < 0)
      WinX = 0;
    if (WinY < 0)
      WinY = 0;

    if (Mask == 1)
      DW->drawLine(WinX, WinY, WinXp, WinYp, 200);
    RootXp = RootX;
    RootYp = RootY;
    WinXp = WinX;
    WinYp = WinY;
    PrevMask = Mask;
    StopTime = Ml.getLocalTime();
    Io.add(StopTime - StartTime);
  }
  Io.print("Mouse read");
  return NULL;
}

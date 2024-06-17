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

#ifndef GRABWINDOW_H
#define GRABWINDOW_H

#include <stdint.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

class GrabWindow {
private:
  int32_t BufWidth;
  int32_t BufHeight;
  XShmSegmentInfo ShmInfo;
  Display *IOScreen;
  Display *Screen;
  XImage *Image;
  uint32_t *Data;
  GrabWindow(GrabWindow &GW);
  GrabWindow &operator=(GrabWindow &GW);
public:
  GrabWindow();
  ~GrabWindow();
  bool grabWindowInit(int32_t Width, int32_t Height, char *DispName);
  void drawCursor(int32_t X, int32_t Y, int32_t Color);
  void drawLine(int32_t X0, int32_t Y0, int32_t X1, int32_t Y1, int32_t Color);
  void drawCursor(int32_t X, int32_t Y, int32_t Color, int32_t Idx);
  void drawLine(int32_t X0, int32_t Y0, int32_t X1, int32_t Y1, int32_t Color, int32_t Idx);
  int32_t getWidth();
  int32_t getHeight();
  XImage *getImage();
  uint32_t *getData();
  Display *getDisplay();
  Display *getIODisplay();
};

#endif /* GRABWINDOW_H */

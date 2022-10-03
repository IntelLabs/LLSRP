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

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "GrabWindow.h"

#define abs(x) ((x) < 0 ? (-(x)) : (x))

GrabWindow::GrabWindow(GrabWindow &GW) {
  printf("Copy is not allowed for GrabWindow class, exiting\n");
  exit(1);
}

GrabWindow &GrabWindow::operator =(GrabWindow &GW) {
  printf("operator = is not allowed for GrabWindow class, exiting\n");
  exit(1);
}

XImage *allocateImage(Display *Display, int32_t Width, int32_t Height,
                      XShmSegmentInfo &ShmInfo, uint32_t *Data) {
  XImage *Image;
  // Create a shared memory area
  ShmInfo.shmid = shmget(IPC_PRIVATE, Width * Height * 4, IPC_CREAT | 0606);
  if (ShmInfo.shmid == -1)
    return NULL;

  // Map the shared memory segment into the address space of this process
  ShmInfo.shmaddr = (char *)shmat(ShmInfo.shmid, 0, 0);
  if (ShmInfo.shmaddr == (char *)-1)
    return NULL;

  Data = (uint32_t *)ShmInfo.shmaddr;
  ShmInfo.readOnly = false;

  // Mark the shared memory segment for removal
  // It will be removed even if this program crashes
  shmctl(ShmInfo.shmid, IPC_RMID, 0);
  Image = XShmCreateImage(Display,
                          XDefaultVisual(Display, XDefaultScreen(Display)),
                          DefaultDepth(Display, XDefaultScreen(Display)),
                          ZPixmap, 0, &ShmInfo, 0, 0);

  Image->width = Width;
  Image->height = Height;
  Image->data = (char *)Data;

  // Ask the X server to attach the shared memory segment and sync
  XShmAttach(Display, &ShmInfo);
  XSync(Display, false);
  return Image;
}

GrabWindow::GrabWindow() {
  Screen = NULL;
  IOScreen = NULL;
}

GrabWindow::~GrabWindow() {
  if (Screen)
    XCloseDisplay(Screen);
  if (IOScreen)
    XCloseDisplay(IOScreen);
  if (!Image && Data)
    delete [] Data;
}

bool GrabWindow::grabWindowInit(int32_t Width, int32_t Height, char *DispStr) {
  BufHeight = Height;
  BufWidth = Width;
  if (DispStr) {
    if (*DispStr == '\0') {
      Data = new uint32_t[Width * Height];
      for (int x = 0; x < Width * Height; x++)
        Data[x] = 0;
      Image = NULL;
      return true;
    }
    Screen = XOpenDisplay(DispStr);
    if (!Screen) {
      printf("Unable to open screen %s\n", DispStr);
      return false;
    }
    IOScreen = XOpenDisplay(DispStr);
    if (!IOScreen) {
      printf("Unable to open I/O display %s\n", DispStr);
      return false;
    }
    if (!ScreenOfDisplay(Screen, 0)) {
      printf("Unable to get screen resolution for %s\n", DispStr);
      return false;
    }
    if (ScreenOfDisplay(Screen, 0)->width < Width ||
        ScreenOfDisplay(Screen, 0)->height < Height) {
      printf("Screen resolution is less than requested grab window\n");
      return false;
    }
    Image = allocateImage(Screen, Width, Height, ShmInfo, Data);
  } else {
    printf("Warning: opening current screen for streaming\n");
    Screen = XOpenDisplay(NULL);
    if (!Screen) {
      printf("Unable to open current screen\n");
      return false;
    }
    IOScreen = XOpenDisplay(NULL);
    if (!IOScreen) {
      printf("Unable to open current I/O display\n");
      return false;
    }
    if (ScreenOfDisplay(Screen, 0)->width < Width ||
        ScreenOfDisplay(Screen, 0)->height < Height) {
      printf("Screen resolution is less than requested grab window\n");
      return false;
    }
    Image = allocateImage(Screen, Width, Height, ShmInfo, Data);
    return true;
  }
  return true;
}

void GrabWindow::drawCursor(int32_t X, int32_t Y, int32_t Color) {
  uint8_t *RGB = (uint8_t *)(Data);
  for (int x = -3; x < 4; x++)
    for (int y = -3; y < 4; y++) {
      int32_t cur = 4 * ((Y + y) * BufWidth + (X + x));
      if (y + Y < 0 || y + Y >= BufHeight ||
          x + X < 0 || x + X >= BufWidth)
        continue;
      RGB[cur + 0] = Color & 0xFF;
      RGB[cur + 1] = (Color & 0xFF00) >> 8;
      RGB[cur + 2] = (Color & 0xFF0000) >> 16;
    }
}

void GrabWindow::drawLine(int32_t X0, int32_t Y0, int32_t X1, int32_t Y1, int32_t Color) {
  uint8_t *RGB = (uint8_t *)(Data);
  float x = X0 - X1;
  float y = Y0 - Y1;
  float max = abs(x) + abs(y) + 0.0001;
  x /= max; y /= max;
  if (Y0 < 0 || Y0 >= BufHeight ||
      X0 < 0 || X0 >= BufWidth)
    return;
  if (Y1 < 0 || Y1 >= BufHeight ||
      X1 < 0 || X1 >= BufWidth)
    return;

  for (float n = 0; n <= max; n++) {
    int32_t cur = 4 * (((int)(n * y) + Y1) * BufWidth + ((int)(n * x) + X1));
    RGB[cur + 0] = Color & 0xFF;
    RGB[cur + 1] = (Color & 0xFF00) >> 8;
    RGB[cur + 2] = (Color & 0xFF0000) >> 16;
  }
}

int32_t GrabWindow::getWidth() {
  return BufWidth;
}

int32_t GrabWindow::getHeight() {
  return BufHeight;
}

XImage *GrabWindow::getImage() {
  return Image;
}

uint32_t *GrabWindow::getData() {
  return Data;
}

Display *GrabWindow::getDisplay() {
  return Screen;
}
Display *GrabWindow::getIODisplay() {
  return IOScreen;
}

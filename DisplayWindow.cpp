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
#include <unistd.h>
#include <stdint.h>

#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <pthread.h>

#include "Config.h"
#include "DisplayWindow.h"

void *updateImage(void *Ptr) {
  if (Ptr == NULL) {
    printf("Error! Thread parameter is NULL!\n");
    return NULL;
  }
  DisplayWindow *DW = (DisplayWindow *)Ptr;
  Display *Display = XOpenDisplay(NULL);
  if (Display == NULL) {
    printf("Error! Can't open current X11 Display!\n");
    return NULL;
  }
  if (!ScreenOfDisplay(Display, 0)) {
    printf("Unable to get screen resolution for\n");
    return NULL;
  }
  int32_t Width = DW->getWidth();
  int32_t Height = DW->getHeight();
  XImage *Image;
  XShmSegmentInfo ShmInfo;
  unsigned int *data;
  // Create a shared memory area
  ShmInfo.shmid = shmget(IPC_PRIVATE, Width * Height * 4, IPC_CREAT | 0606);
  if (ShmInfo.shmid == -1)
    return NULL;

  // Map the shared memory segment into the address space of this process
  ShmInfo.shmaddr = (char *)shmat(ShmInfo.shmid, 0, 0);
  if (ShmInfo.shmaddr == (char *)-1)
    return NULL;

  data = (unsigned int *)ShmInfo.shmaddr;
  DW->setDataPtr((void *)ShmInfo.shmaddr);
  ShmInfo.readOnly = false;

  // Mark the shared memory segment for removal
  // It will be removed even if this program crashes
  shmctl(ShmInfo.shmid, IPC_RMID, 0);
  Image = XShmCreateImage(Display, XDefaultVisual(Display, XDefaultScreen(Display)),
                          DefaultDepth(Display, XDefaultScreen(Display)), ZPixmap, 0,
                          &ShmInfo, 0, 0);
  Image->width = Width;
  Image->height = Height;
  Image->data = (char *)data;

  // Ask the X server to attach the shared memory segment and sync
  XShmAttach(Display, &ShmInfo);
  XSync(Display, false);
  XSetWindowAttributes Attributes;
  Attributes.backing_store = NotUseful;
  Window WindowN = XCreateWindow(Display, DefaultRootWindow(Display),
      0, 0, Width, Height, 0, DefaultDepth(Display, XDefaultScreen(Display)),
      InputOutput, CopyFromParent, CWBackingStore, &Attributes);

  XStoreName(Display, WindowN, "Display window");
  XSelectInput(Display, WindowN, StructureNotifyMask);
  XMapWindow(Display, WindowN);

  // Get center coordinates for the window width, height
  if (!ScreenOfDisplay(Display, 0)) {
    printf("Unable to get screen resolution for\n");
    return NULL;
  }
  int32_t StartX = (ScreenOfDisplay(Display, 0)->width - Width) / 2;
  StartX = StartX > 0 ? StartX : 0;
  int32_t StartY = (ScreenOfDisplay(Display, 0)->height - Height) / 2;
  StartX = StartY > 0 ? StartY : 0;
  // Move window to the center if we are in game mode
  if (DW->Flags & GAME_MODE)
    XMoveWindow(Display, WindowN, StartX, StartY);
  else
    XMoveWindow(Display, WindowN, 0, 0);

  XGCValues Values;
  Values.graphics_exposures = False;
  GC Gc = XCreateGC(Display, WindowN, GCGraphicsExposures, &Values);

  Atom DeleteAtom = XInternAtom(Display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(Display, WindowN, &DeleteAtom, True);
  DW->setDataPtr((void *)Image->data);
  DW->DrawWin = WindowN;
  DW->DrawWinOpened = true;
  while (DW->Running) {
    XShmPutImage(Display, WindowN, Gc, Image,
                 0, 0, 0, 0, Width, Height, False);
    XSync(Display, False);
    usleep(10);
  }
  sleep(1);
  XDestroyWindow(Display, WindowN);
  XCloseDisplay(Display);
  return NULL;
}

DisplayWindow::~DisplayWindow() {
  pthread_cancel(DisplayThread);
}

bool DisplayWindow::createWindow(int32_t Width, int32_t Height, int32_t DWFlags) {
  BufHeight = Height;
  BufWidth = Width;
  Flags = DWFlags;
  pthread_create(&DisplayThread, NULL, &updateImage, this);
  return true;
}

void DisplayWindow::clearImage() {
  for (int x = 0; x < BufWidth; x++)
    for (int y = 0; y < BufHeight; y++) {
      uint8_t *RGB = (uint8_t *)BufData;
      int32_t cur = 4 * (y * BufWidth + x);
      RGB[cur + 0] = 0;
      RGB[cur + 1] = 0;
      RGB[cur + 2] = 0;
    }
}
void DisplayWindow::drawCursor(int32_t X, int32_t Y, int32_t Color) {
  uint8_t *RGB = (uint8_t *)BufData;
  const int32_t w = 0;
  for (int x = -w; x < w + 1; x++)
    for (int y = -w; y < w + 1; y++) {
      int32_t cur = 4 * ((Y + y) * BufWidth + (X + x));
      if (y + Y < 0 || y + Y >= BufHeight ||
          x + X < 0 || x + X >= BufWidth)
        continue;
      RGB[cur + 0] = Color & 0xFF;
      RGB[cur + 1] = (Color & 0xFF00) >> 8;
      RGB[cur + 2] = (Color & 0xFF0000) >> 16;
    }
}

#define abs(x) ((x) < 0 ? (-(x)) : (x))

void DisplayWindow::drawLine(int32_t X0, int32_t Y0, int32_t X1, int32_t Y1, int32_t Color) {
  uint8_t *RGB = (uint8_t *)BufData;
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

DisplayWindow::DisplayWindow() {
  BufWidth = 0;
  BufHeight = 0;
  BufData = NULL;
  DrawWinOpened = false;
  DrawWin = (Window)-1;
  Running = true;
}

//inline
int32_t DisplayWindow::getWidth() {
  return BufWidth;
}

//inline
int32_t DisplayWindow::getHeight() {
  return BufHeight;
}

//inline
uint8_t *DisplayWindow::getSlicePtr(int32_t Slice) {
  return (uint8_t *)(BufData + (Slice / SLICE_NUM_X) *
      BufWidth * BufHeight / SLICE_NUM_Y);
}

//inline
void *DisplayWindow::getDataPtr() {
  return (void *)BufData;
}

//inline
void DisplayWindow::setDataPtr(void *Ptr) {
  BufData = (int32_t *)Ptr;
}


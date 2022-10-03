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
#include <stdint.h>
#include <stdlib.h>

#include "Config.h"
#include "DisplayWindowW.h"

SDL_Renderer *renderer;
SDL_Texture *texture;

DisplayWindow::DisplayWindow(DisplayWindow &DW) {
  printf("Copy is not allowed for DisplayWindow class, exiting\n");
  exit(1);
}

DisplayWindow &DisplayWindow::operator=(const DisplayWindow &DW) {
  printf("operator= is not allowed for DisplayWindow class, exiting\n");
  exit(1);
}

static DWORD WINAPI updateImage(LPVOID Arg) {
  if (Arg == NULL) {
    printf("Error! Thread parameter is NULL!\n");
    return NULL;
  }
  DisplayWindow *DW = (DisplayWindow *)Arg;
  int32_t Width = DW->getWidth();
  int32_t Height = DW->getHeight();
  uint32_t format = SDL_PIXELFORMAT_ARGB8888;

  SDL_Init(SDL_INIT_VIDEO);

  SDL_DisplayMode DM;
  SDL_GetDesktopDisplayMode(0, &DM);
  int32_t StartX = (DM.w - Width) / 2;
  StartX = StartX > 0 ? StartX : 0;
  int32_t StartY = (DM.h - Height) / 2;
  StartX = StartY > 0 ? StartY : 0;
  // Move window to the center if we are in game mode
  if (!(DW->checkFlags(GAME_MODE)))
    StartX = StartY = 0;

  DW->setDrawWin(SDL_CreateWindow("Display Window" , StartX, StartY, Width, Height, 0));
  DW->setWindowOpened();

  renderer = SDL_CreateRenderer(DW->getDrawWin(), -1, SDL_RENDERER_ACCELERATED);
  texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING, Width, Height);
  DW->startRunning();
  int32_t *Data = DW->getDataPtr();
  while (DW->isRunning()) {
    SDL_PumpEvents();
    SDL_UpdateTexture(texture, NULL, Data, Width * sizeof(*Data));
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    Sleep(10);
  }
  return NULL;
}

DisplayWindow::~DisplayWindow() {
  if (BufData)
    delete [] BufData;
  TerminateThread(DisplayThread, 0);
}

bool DisplayWindow::isWindowOpened() {
  return DrawWinOpened;
}

void DisplayWindow::setWindowOpened() {
  DrawWinOpened = true;
}

bool DisplayWindow::isRunning() {
  return Running;
}

bool DisplayWindow::checkFlags(int32_t Flag) {
  return Flags & Flag;
}

void DisplayWindow::startRunning() {
  Running = 1;
}

void DisplayWindow::stopRunning() {
  Running = 0;
}

void DisplayWindow::setDrawWin(SDL_Window *DrawWindow) {
  DrawWin = DrawWindow;
}

SDL_Window *DisplayWindow::getDrawWin() {
  return DrawWin;
}

bool DisplayWindow::createWindow(int32_t Width, int32_t Height, int32_t DWFlags) {
  DWORD dwThreadIdArray;
  BufHeight = Height;
  BufWidth = Width;
  Flags = DWFlags;
  BufData = new int32_t[4 * BufHeight * BufWidth + 16];
  if (BufData == NULL)
    return false;
  clearImage();
  DisplayThread = CreateThread(NULL, 0, updateImage, this, 0, &dwThreadIdArray);
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
int32_t *DisplayWindow::getDataPtr() {
  return BufData;
}

//inline
void DisplayWindow::setDataPtr(void *Ptr) {
  BufData = (int32_t *)Ptr;
}


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

#ifndef DISPLAYWINDOW_H
#define DISPLAYWINDOW_H

#define GAME_MODE 1

#include <stdint.h>

class DisplayWindow {
private:
  int32_t BufWidth;
  int32_t BufHeight;
  int32_t *BufData;
  pthread_t DisplayThread;
public:
  int32_t Flags;
  Window DrawWin;
  int32_t DrawWinOpened;
  int32_t Running;
  DisplayWindow();
  ~DisplayWindow();
  bool createWindow(int32_t Width, int32_t Height, int32_t Flags);
  void clearImage();
  void drawCursor(int32_t X, int32_t Y, int32_t Color);
  void drawLine(int32_t X0, int32_t Y0, int32_t X1, int32_t Y1, int32_t Color);
  uint8_t *getSlicePtr(int32_t Slice);
  int32_t getWidth();
  int32_t getHeight();
  void *getDataPtr();
  void setDataPtr(void *Ptr);
};

#endif /* DISPLAYWINDOWX_H */

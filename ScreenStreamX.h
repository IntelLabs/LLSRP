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

#ifndef ScreenStreamX_H
#define ScreenStreamX_H

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

extern "C" {
#include <libavcodec/avcodec.h>
};

enum class ModeOption {
  FullHD_60,
  FullHD_25,
  VGA_60,
  VGA_25,
  P720_60,
  P720_25
};

class ScreenStreamX {
private:
  AVFrame *Frame;
  AVCodec *Codec;
  AVCodecContext *CodecCtx;
  uint32_t *DataPtr;
  int32_t Slice;

public:
  ScreenStreamX();
  ~ScreenStreamX();
  void setFrameDimentions(int32_t Width, int32_t Height);
  int32_t getCtxWidth();
  int32_t getCtxHeight();
  int32_t getCtxFPS();
  int32_t init(int32_t SliceNum,
               uint32_t *Data, int32_t FPS,
               int32_t width, int32_t height);
  int32_t decodeFrame(AVFrame *OutputFrame);
};
void setDimentions(ModeOption MOpt, int32_t &Width, int32_t &Height,
                   int32_t &FPS);

#endif /* ScreenStreamX_H */

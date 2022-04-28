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

#ifndef VIDEOENCODEX_H
#define VIDEOENCODEX_H

#include <stdint.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <x264.h>
};

class VideoEncodeX {
private:
  x264_picture_t XFrame;
  x264_t *Encoder;
  x264_param_t Ctx;
  x264_picture_t XPkt;
  AVFrame *Frame;
  uint8_t *Buf;
  int32_t TargetRate;
  int32_t AvgRate;
  uint16_t Slice;
  uint16_t Id;
  int64_t EncodeTime;
  int64_t EncodeFrame;

public:
  VideoEncodeX();
  ~VideoEncodeX();
  int32_t init(int32_t Width, int32_t Height, int32_t FPS, int32_t SliceNum);
  void initPkt();
  void unrefPkt();
  AVFrame *getFramePtr();
  int32_t encodeFrame(int32_t FrameNum, int32_t *PktSize,
                      uint8_t **PktData, int32_t Quality);
};
#endif /* VIDEOENCODEX_H */

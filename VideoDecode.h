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

#ifndef VIDEODECODE_H
#define VIDEODECODE_H

#include <stdint.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
};

class VideoDecode {
private:
  AVFrame *Frame;
  AVCodec *Codec;
  AVCodecContext *CodecCtx;
  AVDictionary *Options;
  AVPacket Pkt;
  int64_t DecodeTime;
  int64_t DecodeFrame;

public:
  VideoDecode();
  ~VideoDecode();
  int32_t init(int32_t SliceNum);
  int32_t getCtxWidth();
  int32_t getCtxHeight();
  // Decode one Pkt to BGRA Frame.  Write result to OutputFrame.
  int32_t decodeFrame(uint8_t *OutputData, int32_t PktSize,
                      uint8_t *PktData, int32_t Num, bool WriteData = true);
};
#endif /* VIDEODECODE_H */

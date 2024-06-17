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

#include <stdint.h>
#include <sys/time.h>
#include "VideoDecode.h"
#include "Config.h"

VideoDecode::VideoDecode() {
  CodecCtx = NULL;
  Options = NULL;
  Frame = NULL;
  DecodeTime = 0;
  DecodeFrame = 0;
}

VideoDecode::~VideoDecode() {
#ifdef PRINT_STAT
  printf("Average Decode Time %g\n", 1.0 * DecodeTime / DecodeFrame);
#endif
  if (CodecCtx) {
    avcodec_close(CodecCtx);
    av_free(CodecCtx);
  }
  if (Frame) {
    av_frame_free(&Frame);
  }
}

int32_t VideoDecode::init(int32_t SliceNum) {
#ifdef HWD_ACCEL
  Codec = avcodec_find_decoder_by_name("h264_qsv");
  if (!Codec) {
#ifdef PRINT_WARN
    printf("Hardware accel not found! Decode could be slow!\n");
#endif
#endif
    Codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!Codec) {
      printf("Screen codec not found!\n");
      return -1; // Screen Codec not found
    }
#ifdef HWD_ACCEL
  }
#endif

  CodecCtx = avcodec_alloc_context3(Codec);
  if (!CodecCtx) {
    return -2; // Could not allocate video codec context
  }

  CodecCtx->thread_count = 1;
//  CodecCtx->delay = 0;
  CodecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
  if (avcodec_open2(CodecCtx, Codec, NULL) < 0) {
    return -3; // Could not open screen codec
  }
  av_log_set_level(0);
  Frame = av_frame_alloc();
  if (!Frame) {
    return -4; // Could not allocate video frame
  }

  return 0;
}

int32_t VideoDecode::getCtxWidth() { return CodecCtx->width; }

int32_t VideoDecode::getCtxHeight() { return CodecCtx->height; }

void YUV444toBGRA (uint8_t *InY, uint8_t *InU, uint8_t *InV, uint8_t *Out,
                   int width, int height, int32_t Num) {
  int i, j;
  for (i = 0; i < height; i++) {
    int baseI = i * width;
    int baseIO = i * width * SLICE_NUM_X;
    for (j = 0; j < width; j++) {
      int baseJO = j * 4 + (Num % SLICE_NUM_X) * 4 * width;
#ifdef NO_CONVERT
      Out[4 * baseIO + baseJO] = InY[baseI + j];
      Out[4 * baseIO + baseJO + 1] = InU[baseI + j];
      Out[4 * baseIO + baseJO + 2] = InV[baseI + j];
      Out[4 * baseIO + baseJO + 3] = 255;
#else
      int c = InY[baseI + j] - 16;
      int d = InU[baseI + j] - 128;
      int e = InV[baseI + j] - 128;
      int B = (298 * c + 516 * d + 128) >> 8;
      int G = (298 * c - 100 * d - 208 * e + 128) >> 8;
      int R = (298 * c + 409 * e + 128) >> 8;

      B = B < 0 ? 0 : B;
      B = B > 255 ? 255 : B;
      G = G < 0 ? 0 : G;
      G = G > 255 ? 255 : G;
      R = R < 0 ? 0 : R;
      R = R > 255 ? 255 : R;
      Out[4 * baseIO + baseJO] = B;
      Out[4 * baseIO + baseJO + 1] = G;
      Out[4 * baseIO + baseJO + 2] = R;
      Out[4 * baseIO + baseJO + 3] = 255;
#endif
    }
  }
}

// Decode one frame from Screen Stream  Write result to OutputFrame.
int32_t VideoDecode::decodeFrame(uint8_t *OutputData, int32_t PktSize,
                                 uint8_t *PktData, int32_t Num, bool WriteData) {
  struct timeval Start;
  struct timeval Stop;
  gettimeofday(&Start, NULL);
  AVPacket Pkt;
  int32_t RetDec, HasFrame = 0;
  av_init_packet(&Pkt);
  Pkt.size = PktSize;
  Pkt.data = PktData;
  CodecCtx->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 100)
  RetDec = avcodec_send_packet(CodecCtx, &Pkt);
  HasFrame = avcodec_receive_frame(CodecCtx, Frame);
  if (RetDec < 0) {
    av_packet_unref(&Pkt);
    return -2; // Decode error
  }
  if (HasFrame < 0) {
    av_packet_unref(&Pkt);
    return -3; // Cannot decode frame
  }
#else
  RetDec = avcodec_decode_video2(CodecCtx, Frame, &HasFrame, &Pkt);
  if (RetDec < 0) {
    av_packet_unref(&Pkt);
    return -2; // Decode error
  }
  if (!HasFrame) {
    av_packet_unref(&Pkt);
    return -3; // Cannot decode frame
  }
#endif
  CodecCtx->width = Frame->width;
  CodecCtx->height = Frame->height;
  CodecCtx->pix_fmt = (AVPixelFormat)Frame->format;

  if (WriteData) {
    YUV444toBGRA(Frame->data[0], Frame->data[1], Frame->data[2], OutputData, Frame->width, Frame->height, Num);
  }
  av_packet_unref(&Pkt);
  gettimeofday(&Stop, NULL);
  DecodeTime += (Stop.tv_sec - Start.tv_sec) * 1000000 + Stop.tv_usec - Start.tv_usec;
  DecodeFrame++;
  return 0;
}

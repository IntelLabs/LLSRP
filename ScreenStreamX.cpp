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

#include "ScreenStreamX.h"
#include "Config.h"
#include <stdint.h>

extern int32_t MouseX;
extern int32_t MouseY;

ScreenStreamX::ScreenStreamX() {
  CodecCtx = NULL;
  Frame = NULL;
}

ScreenStreamX::~ScreenStreamX() {
  if (CodecCtx) {
    avcodec_close(CodecCtx);
    av_free(CodecCtx);
  }
  if (Frame) {
    av_freep(&Frame->data[0]);
    av_frame_free(&Frame);
  }
}

void ScreenStreamX::setFrameDimentions(int32_t Width, int32_t Height) {
  Frame->width = Width;
  Frame->height = Height;
}

int32_t ScreenStreamX::getCtxWidth() {
  return CodecCtx->width;
}

int32_t ScreenStreamX::getCtxHeight() {
  return CodecCtx->height;
}

int32_t ScreenStreamX::getCtxFPS() {
  return CodecCtx->time_base.den;
}

int32_t ScreenStreamX::init(int32_t SliceNum,
                            uint32_t *Data, int32_t FPS,
                            int32_t width, int32_t height) {
  Slice = SliceNum;
  DataPtr = Data;
  Frame = av_frame_alloc();
  if (!Frame) {
    return -6; // Could not allocate video frame
  }

  // Finish decode init (there will be no stream to decode it will be generated
  Codec = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
  if (!Codec) {
    return -1; // Screen Codec not found
  }

  CodecCtx = avcodec_alloc_context3(Codec);
  if (!CodecCtx) {
    return -2; // Could not allocate video codec context
  }

  CodecCtx->time_base.den = FPS;
  CodecCtx->width = width / SLICE_NUM_X;
  CodecCtx->height = height / SLICE_NUM_Y;

  setFrameDimentions(CodecCtx->width, CodecCtx->height);

  return 0;
}

void BGRAtoYUV444(uint8_t *In,
                  uint8_t *OutY, uint8_t *OutU, uint8_t *OutV,
                  int width, int height, int Slice) {
  int i, j;
  int32_t SliceX = (Slice % SLICE_NUM_X) * width;
  int32_t SliceY = (Slice / SLICE_NUM_X) * height;
  for (i = 0; i < height; i++) {
    int baseI = i * width;
    for (j = 0; j < width; j++) {
      int baseJ = j * 4;
#ifdef NO_CONVERT
      OutY[baseI + j] = In[(i + SliceY) * 4 * width * SLICE_NUM_X + 4 * SliceX + baseJ];
      OutU[baseI + j] = In[(i + SliceY) * 4 * width * SLICE_NUM_X + 4 * SliceX + baseJ + 1];
      OutV[baseI + j] = In[(i + SliceY) * 4 * width * SLICE_NUM_X + 4 * SliceX + baseJ + 2];
#else
      int B = In[(i + SliceY) * 4 * width * SLICE_NUM_X + 4 * SliceX + baseJ];
      int G = In[(i + SliceY) * 4 * width * SLICE_NUM_X + 4 * SliceX + baseJ + 1];
      int R = In[(i + SliceY) * 4 * width * SLICE_NUM_X + 4 * SliceX + baseJ + 2];
      int A = In[(i + SliceY) * 4 * width * SLICE_NUM_X + 4 * SliceX + baseJ + 3];
      uint16_t Y = 76 * R + 150 * G + 29 * B;
      int16_t U = -43 * R - 84 * G + 127 * B;
      int16_t V = 127 * R - 106 * G - 21 * B;
      Y = (Y + 128) >> 8;
      U = (U + 128) >> 8;
      V = (V + 128) >> 8;
      OutY[baseI + j] = Y;
      OutU[baseI + j] = U + 128;
      OutV[baseI + j] = V + 128;
#endif
    }
  }
}

// Decode one frame from Screen Stream  Write result to OutputFrame.
int32_t ScreenStreamX::decodeFrame(AVFrame *OutputFrame) {
  BGRAtoYUV444((uint8_t *)DataPtr,
               OutputFrame->data[0], OutputFrame->data[1], OutputFrame->data[2],
               Frame->width, Frame->height, Slice);
  return 0;
}

void setDimentions(ModeOption MOpt, int32_t &Width, int32_t &Height,
                   int32_t &FPS) {
  switch (MOpt) {
    case ModeOption::FullHD_60:
      FPS = 60;
      Width = 1920;
      Height = 1080;
      break;
    case ModeOption::FullHD_25:
      FPS = 25;
      Width = 1920;
      Height = 1080;
      break;
    case ModeOption::VGA_60:
      FPS = 60;
      Width = 640;
      Height = 480;
      break;
    case ModeOption::VGA_25:
      FPS = 25;
      Width = 640;
      Height = 480;
      break;
    case ModeOption::P720_60:
      FPS = 60;
      Width = 1024;
      Height = 768;
      break;
    case ModeOption::P720_25:
      default:
      FPS = 25;
      Width = 1024;
      Height = 768;
      break;
  }
}


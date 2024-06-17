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

#include "VideoEncodeX.h"
#include "Config.h"
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

VideoEncodeX::VideoEncodeX(VideoEncodeX &VE) {
  printf("Copy is not allowed for VideoEncodeX class, exiting\n");
  exit(1);
}

VideoEncodeX &VideoEncodeX::operator=(VideoEncodeX &VE) {
  printf("operator= is not allowed for VideoEncodeX class, exiting\n");
  exit(1);
}

VideoEncodeX::VideoEncodeX() {}
VideoEncodeX::~VideoEncodeX() {
#ifdef PRINT_STAT
  printf("Average Encode Time %g\n", 1.0 * EncodeTime / EncodeFrame);
#endif
  delete [] Buf;
}

int32_t VideoEncodeX::init(int32_t Width, int32_t Height,
                           int32_t FPS, int32_t SliceNum) {

  Id = SliceNum;
  Slice = SliceNum % SLICE_NUM;
  Buf = new uint8_t[65536];
  EncodeFrame = 0;
  EncodeTime = 0;
  // choose thefastes decoding and no latency
  x264_param_default_preset(&Ctx, "ultrafast", "zerolatency");
  Ctx.i_width = Width;
  Ctx.i_height = Height;
  Ctx.i_fps_num = FPS;
  Ctx.i_fps_den = 1;
  Ctx.i_csp = LIBX264_PIX_FMT;
  Ctx.b_intra_refresh = 1;
  Ctx.i_slice_count = 0;
  Ctx.i_slice_count_max = 0;
  Ctx.i_threads = 0;
  Ctx.b_sliced_threads = 0;
  Ctx.rc.i_rc_method = X264_RC_CRF;
  Ctx.rc.i_bitrate = 6000 * Ctx.i_fps_num * SLICE_NUM / 1000;
  AvgRate = Ctx.rc.i_bitrate * 1000 / Ctx.i_fps_num / SLICE_NUM;
  TargetRate = Ctx.rc.i_bitrate * 1000 / Ctx.i_fps_num / SLICE_NUM;
  Ctx.rc.i_vbv_buffer_size = 300000;
  Ctx.rc.i_vbv_max_bitrate = 60000;
  // set initial quality
  Ctx.rc.f_rf_constant = 34;
  Ctx.rc.f_rf_constant_max = 51;
  // set intra-refresh to 2 colomns of macroblocks
  Ctx.i_keyint_max = (Width / 16 - 3);
  Ctx.i_bframe_pyramid = 0;
  Ctx.i_bframe = 0;
  Ctx.i_frame_reference = 0;
  Ctx.b_cabac = 0;
  Ctx.i_log_level = 0;
  x264_param_apply_profile(&Ctx, LIBX264_PROFILE);

  Encoder = x264_encoder_open(&Ctx);

  x264_picture_alloc(&XFrame, LIBX264_PIX_FMT, Width, Height);
  XFrame.i_type = X264_TYPE_AUTO;

  // Allocating frame and context
  Frame = av_frame_alloc();
  if (!Frame) {
    return -2; // Could not allocate video frame
  }

  Frame->format = FFMPEG_PIX_FMT;
  Frame->width = Width;
  Frame->height = Height;

  if (av_image_alloc(Frame->data, Frame->linesize, Width, Height,
                     FFMPEG_PIX_FMT, 1) < 0) {
    return -5; // Could not allocate raw picture buffer
  }

  return 0;
}

void VideoEncodeX::initPkt() {}

void VideoEncodeX::unrefPkt() {}

AVFrame *VideoEncodeX::getFramePtr() { return Frame; }

int32_t VideoEncodeX::encodeFrame(int32_t FrameNum, int32_t *PktSize,
                                  uint8_t **PktData, int32_t Quality) {
  int32_t i, NalShift;
  x264_nal_t *Nal;
  int32_t NalNum;
  struct timeval Start;
  struct timeval Stop;
  gettimeofday(&Start, NULL);

  for (i = 0; i < XFrame.img.i_plane; i++) {
    XFrame.img.plane[i] = Frame->data[i];
    XFrame.img.i_stride[i] = Frame->linesize[i];
  }
  XFrame.i_pts = Frame->pts;

  XFrame.i_type = X264_TYPE_P;

  NalShift = 0;

  if (x264_encoder_encode(Encoder, &Nal, &NalNum, &XFrame, &XPkt) < 0)
    return -1; // Error encoding frame

  for (i = 0; i < NalNum; i++) {
    if (NalShift + Nal[i].i_payload < 65536)
      memcpy(Buf + NalShift, Nal[i].p_payload, Nal[i].i_payload);
    NalShift += Nal[i].i_payload;
  }

    if ((int32_t)Ctx.rc.f_rf_constant != Quality) {
      Ctx.rc.f_rf_constant = (float)Quality;
      x264_encoder_reconfig(Encoder, &Ctx);
    }

  // Return warning if packet size is greater than buffer size.
  //  Rate control should lower size of next packet
//  if (NalShift >= 65535) {
//    return 1;
//  }

  Buf[0] = (uint8_t)Slice;
  Buf[1] = (uint8_t)(FrameNum & 255);
  *PktSize = NalShift;
  *PktData = Buf;
  if (NalShift >= 65535) {
#ifdef PRINT_WARN
    printf("Warning packet size is greater than 65535!\n");
#endif
    return 1;  // Encoded packet is bigger than buffer size
  }

  gettimeofday(&Stop, NULL);
  EncodeTime += (Stop.tv_sec - Start.tv_sec) * 1000000 + Stop.tv_usec - Start.tv_usec;
  EncodeFrame++;

  return 0;
}


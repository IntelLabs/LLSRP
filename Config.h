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

#ifndef CONFIG_H
#define CONFIG_H

// Common defines
#define SLICE_NUM_Y 8
#define SLICE_NUM_X 2
#define SLICE_NUM (SLICE_NUM_Y * SLICE_NUM_X)

#if SLICE_NUM == 0
Error number of slices should be greater than 0
#endif

#define FFMPEG_PIX_FMT AV_PIX_FMT_YUV444P
#define LIBX264_PIX_FMT X264_CSP_I444
#define LIBX264_PROFILE "high444"

#endif /* CONFIG_H */

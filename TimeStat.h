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

class TimeStat {
public:
  int64_t Avg;
  int64_t Min;
  int64_t Max;
  int64_t Cnt;
  TimeStat() {
    Avg = 0;
    Min = 65536 * 256;
    Max = 0;
    Cnt = 0;
  }
  ~TimeStat() {
  }
  inline
  void print(const char *Name) {
    if (Cnt <= 0) {
      printf("%s data is missing.\n", Name);
      return;
    }
    printf("%s in ms (min, avg, max): %ld %ld %ld\n",
           Name,
           (long int)(Min / 1000),
           (long int)(Avg / Cnt / 1000),
           (long int)(Max / 1000));
  }
  inline
  void add(int64_t TimeDiff) {
    if (TimeDiff <= 0) {
      if (TimeDiff < 0)
        printf("Warning! Time difference %ld is < 0!\n",
               (long int)TimeDiff);
      return;
    }
    Cnt++;
    if (Min > TimeDiff)
      Min = TimeDiff;
    if (Max < TimeDiff)
      Max = TimeDiff;
    Avg += TimeDiff;
  }
  inline float getAvg() {
    return Avg / Cnt;
  }
};

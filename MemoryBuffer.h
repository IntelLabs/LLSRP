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

class SliceInfo {
public:
  uint8_t SliceNum;
  uint8_t FrameNum;
  uint16_t Size;
};

#define SLICE_BUF 3
#define MAX_SIZE 65536
class MemoryBuffer {
public:
  uint8_t *SliceData;
  int32_t Length;
  int16_t SlicePtrs[SLICE_NUM * SLICE_BUF];
  SliceInfo *SInfo;
  pthread_mutex_t Mutex;
  uint32_t WritePtr;
  MemoryBuffer(int32_t Size) {
    Length = Size;
    Mutex = PTHREAD_MUTEX_INITIALIZER;
    SliceData = new uint8_t[MAX_SIZE * Length];
    SInfo = new SliceInfo[Length];
    WritePtr = 0;
    for (int32_t i = 0; i < SLICE_NUM * SLICE_BUF; i++)
      SlicePtrs[i] = -1;
  }
  ~MemoryBuffer() {
    delete [] SliceData;
    delete [] SInfo;
  }
  uint8_t *getFreePtr(uint16_t *Ptr) {
    *Ptr = WritePtr;
    WritePtr++;
    if (WritePtr >= Length)
      WritePtr = 0;
    uint8_t *Ret = &SliceData[*Ptr * MAX_SIZE];
    return Ret;
  }
  uint8_t *getSlicePtr(int32_t SliceNum, int32_t *FrameNum, int16_t *Ptr) {
    uint8_t *Ret;
    int16_t CurPtr;
    int32_t i = 0;
    int32_t CurPtrCnt = 0;
    while (i < SLICE_BUF) {
      CurPtr = SlicePtrs[SliceNum * SLICE_BUF + i];
      i++;
      if (CurPtr == -1) { // there is data for the slice, but not for the next
        CurPtrCnt++;
        continue;
      }
      if (*FrameNum == -1 || SInfo[CurPtr].FrameNum == *FrameNum) {
        *Ptr = CurPtr;
        Ret = &SliceData[CurPtr * MAX_SIZE];
        *FrameNum = SInfo[CurPtr].FrameNum;
#ifdef PRINT_WARN
        if (*FrameNum == -1)
          printf("Starting deocode for slice %d with frame %d\n", SliceNum, *FrameNum);
#endif
        return Ret;
      }
      int32_t CurFrameNum = SInfo[CurPtr].FrameNum;
      if (CurFrameNum - *FrameNum < 0 && CurFrameNum - *FrameNum > -3)
        CurPtrCnt++;
      else if (CurFrameNum - *FrameNum >= 253)
        CurPtrCnt++;
    }
    if (CurPtrCnt == 0) { // lost next frame slice
#ifdef PRINT_WARN
      printf("Lost next frame for slice %d, Framenum expected %d\n",
             SliceNum, *FrameNum);
#endif
      *FrameNum = -1;
      return NULL;
    }
    return NULL;
  }
  void writeSlice(int32_t SliceNum, int32_t FrameNum, uint16_t Size, int16_t Ptr) {
    pthread_mutex_lock(&Mutex);
    SInfo[Ptr].SliceNum = SliceNum;
    SInfo[Ptr].FrameNum = FrameNum;
    SInfo[Ptr].Size = Size;
    int32_t i;
    for (i = 0; i < SLICE_BUF; i++) {
      if (SlicePtrs[SliceNum * SLICE_BUF + i] == -1) {
        SlicePtrs[SliceNum * SLICE_BUF + i] = Ptr;
        pthread_mutex_unlock(&Mutex);
        return;
      }
    }
    for (i = 1; i < SLICE_BUF; i++) {
      SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUF - i] = SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUF - i - 1];
    }
    SlicePtrs[SliceNum * SLICE_BUF] = Ptr;
    pthread_mutex_unlock(&Mutex);
    return;
  }
  void printStatus() {
    printf("Writing to: %d\n", WritePtr);
  }
};

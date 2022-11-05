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

#define SLICE_BUF 2
#define SLICE_BUFP (SLICE_BUF - 1)
#define MAX_SIZE 65536
class MemoryBuffer {
private:
  uint8_t *SliceData;
  int32_t FlipWait;
  int32_t Length;
  int16_t SlicePtrs[SLICE_NUM * SLICE_BUF];
  SliceInfo *SInfo;
  HANDLE Mutex;
  uint32_t WritePtr;
  MemoryBuffer(MemoryBuffer &MB) {
    printf("Copy is not allowed for MemoryBuffer class, exiting\n");
    exit(1);
  }
  MemoryBuffer &operator=(const MemoryBuffer &MB) {
    printf("operator = is not allowed for MemoryBuffer class, exiting\n");
    exit(1);
  }
public:
  MemoryBuffer(int32_t Size) {
    Length = Size;
    Mutex = CreateMutex(NULL, false, NULL);
    SliceData = new uint8_t[MAX_SIZE * Length];
    SInfo = new SliceInfo[Length];
    WritePtr = 0;
    FlipWait = 0;
    for (int32_t i = 0; i < SLICE_NUM * SLICE_BUF; i++)
      SlicePtrs[i] = -1;
  }
  ~MemoryBuffer() {
    delete [] SliceData;
    delete [] SInfo;
  }
  void setFlipWait(int32_t FW) {
    FlipWait = FW;
  }
  uint16_t getSliceSize(int32_t SPtr) {
    return SInfo[SPtr].Size;
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
    int16_t CurPtr, RetPtr = -1;
    int32_t i = 0;
    int32_t HasPrevFrame = 0;
    // no new slices
    if (SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP] == -1)
      return NULL;
    if (!FlipWait) {
      CurPtr = SlicePtrs[SliceNum * SLICE_BUF];
      if (CurPtr == -1)
        return NULL;
      int32_t CurFrameNum = SInfo[CurPtr].FrameNum;
      // if frame is too old - skip it
      if (((256 + CurFrameNum - *FrameNum) & 255) > 200) {
#ifdef PRINT_WARN
        printf("Skipped frame %d as it is too old (previous was %d)\n",
               CurFrameNum, *FrameNum);
#endif
        SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP] = -1;
        return NULL;
      }
      *Ptr = CurPtr;
      Ret = &SliceData[CurPtr * MAX_SIZE];
      *FrameNum = SInfo[CurPtr].FrameNum;
      SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP] = -1;
      return Ret;
    }
    while (i < SLICE_BUFP) {
      CurPtr = SlicePtrs[SliceNum * SLICE_BUF + i];
      i++;
      // there is data for the slice, but not for the next
      if (CurPtr == -1) {
        continue;
      } else {
        RetPtr = CurPtr;
      }
      if (*FrameNum == -1 || SInfo[CurPtr].FrameNum == *FrameNum) {
        *Ptr = CurPtr;
        Ret = &SliceData[CurPtr * MAX_SIZE];
        *FrameNum = SInfo[CurPtr].FrameNum;
#ifdef PRINT_WARN
        if (*FrameNum == -1)
          printf("Starting deocode for slice %d\n", SliceNum);
#endif
        SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP] = -1;
        return Ret;
      }
      int32_t CurFrameNum = SInfo[CurPtr].FrameNum;
      if (((256 + CurFrameNum - *FrameNum) & 255) == 1) {
        HasPrevFrame = 1;
        continue;
      }
    }
    if (HasPrevFrame) {
#ifdef PRINT_WARN
        printf("A slice from the next frame arrived earlier.\n");
#endif
    }
    if (!HasPrevFrame && RetPtr != -1) {
        *Ptr = RetPtr;
        Ret = &SliceData[RetPtr * MAX_SIZE];
        *FrameNum = SInfo[RetPtr].FrameNum;
#ifdef PRINT_WARN
        printf("Starting deocode for slice %d with frame %d\n", SliceNum, *FrameNum);
#endif
        SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP] = 1;
        return Ret;
    }
    return NULL;
  }
  void writeSlice(int32_t SliceNum, int32_t FrameNum, uint16_t Size, int16_t Ptr) {
    WaitForSingleObject(Mutex, INFINITE);
    SInfo[Ptr].SliceNum = SliceNum;
    SInfo[Ptr].FrameNum = FrameNum;
    SInfo[Ptr].Size = Size;
    int32_t i;
    if (!FlipWait) {
      SlicePtrs[SliceNum * SLICE_BUF] = Ptr;
      SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP] = 1;
      ReleaseMutex(Mutex);
      return;
    }

    for (i = 0; i < SLICE_BUFP; i++) {
      if (SlicePtrs[SliceNum * SLICE_BUF + i] == -1) {
        SlicePtrs[SliceNum * SLICE_BUF + i] = Ptr;
        SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP] = 1;
        ReleaseMutex(Mutex);
        return;
      }
    }
    for (i = 1; i < SLICE_BUFP; i++) {
      SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP - i] = SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP - i - 1];
    }
    SlicePtrs[SliceNum * SLICE_BUF] = Ptr;
    SlicePtrs[SliceNum * SLICE_BUF + SLICE_BUFP] = 1;
    ReleaseMutex(Mutex);
    return;
  }
  void printStatus() {
    printf("Writing to: %d\n", WritePtr);
  }
};

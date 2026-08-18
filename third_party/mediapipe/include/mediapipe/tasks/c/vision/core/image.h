#ifndef MEDIAPIPE_TASKS_C_VISION_CORE_IMAGE_H_
#define MEDIAPIPE_TASKS_C_VISION_CORE_IMAGE_H_

#include <stdint.h>

#include "mediapipe/tasks/c/core/mp_status.h"

#ifndef MP_EXPORT
#if defined(_MSC_VER)
#define MP_EXPORT __declspec(dllexport)
#else
#define MP_EXPORT __attribute__((visibility("default")))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum RunningMode {
  IMAGE = 1,
  VIDEO = 2,
  LIVE_STREAM = 3,
};

typedef enum MpImageFormat {
  kMpImageFormatUnknown = 0,
  kMpImageFormatSrgb = 1,
  kMpImageFormatSrgba = 2,
  kMpImageFormatGray8 = 3,
  kMpImageFormatGray16 = 4,
  kMpImageFormatSrgb48 = 7,
  kMpImageFormatSrgba64 = 8,
  kMpImageFormatVec32F1 = 9,
  kMpImageFormatVec32F2 = 12,
  kMpImageFormatVec32F4 = 13,
} MpImageFormat;

typedef struct MpImageInternal* MpImagePtr;

MP_EXPORT MpStatus MpImageCreateFromUint8Data(MpImageFormat format, int width,
                                              int height,
                                              const uint8_t* pixel_data,
                                              int pixel_data_size,
                                              MpImagePtr* out, char** error_msg);

MP_EXPORT void MpImageFree(MpImagePtr image);
MP_EXPORT int MpImageGetWidth(MpImagePtr image);
MP_EXPORT int MpImageGetHeight(MpImagePtr image);

#ifdef __cplusplus
}
#endif

#endif

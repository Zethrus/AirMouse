#ifndef MEDIAPIPE_TASKS_C_VISION_HAND_LANDMARKER_HAND_LANDMARKER_H_
#define MEDIAPIPE_TASKS_C_VISION_HAND_LANDMARKER_HAND_LANDMARKER_H_

#include <cstdint>

#include "mediapipe/tasks/c/core/base_options.h"
#include "mediapipe/tasks/c/core/common.h"
#include "mediapipe/tasks/c/core/mp_status.h"
#include "mediapipe/tasks/c/vision/core/image.h"
#include "mediapipe/tasks/c/vision/core/image_processing_options.h"
#include "mediapipe/tasks/c/vision/hand_landmarker/hand_landmarker_result.h"

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

typedef struct MpHandLandmarkerInternal* MpHandLandmarkerPtr;

struct HandLandmarkerOptions {
  struct BaseOptions base_options;
  RunningMode running_mode;
  int num_hands;
  float min_hand_detection_confidence;
  float min_hand_presence_confidence;
  float min_tracking_confidence;
  typedef void (*result_callback_fn)(MpStatus status,
                                     const HandLandmarkerResult* result,
                                     MpImagePtr image, int64_t timestamp_ms);
  result_callback_fn result_callback;
};

MP_EXPORT MpStatus MpHandLandmarkerCreate(struct HandLandmarkerOptions* options,
                                          MpHandLandmarkerPtr* landmarker,
                                          char** error_msg);

MP_EXPORT MpStatus MpHandLandmarkerDetectForVideo(
    MpHandLandmarkerPtr landmarker, MpImagePtr image,
    const struct ImageProcessingOptions* options, int64_t timestamp_ms,
    HandLandmarkerResult* result, char** error_msg);

MP_EXPORT void MpHandLandmarkerCloseResult(HandLandmarkerResult* result);

MP_EXPORT MpStatus MpHandLandmarkerClose(MpHandLandmarkerPtr landmarker,
                                         char** error_msg);

#ifdef __cplusplus
}
#endif

#endif

#ifndef MEDIAPIPE_TASKS_C_VISION_HAND_LANDMARKER_RESULT_HAND_LANDMARKER_RESULT_H_
#define MEDIAPIPE_TASKS_C_VISION_HAND_LANDMARKER_RESULT_HAND_LANDMARKER_RESULT_H_

#include <stdint.h>

#include "mediapipe/tasks/c/components/containers/category.h"
#include "mediapipe/tasks/c/components/containers/landmark.h"

#ifdef __cplusplus
extern "C" {
#endif

struct HandLandmarkerResult {
  struct Categories* handedness;
  uint32_t handedness_count;
  struct NormalizedLandmarks* hand_landmarks;
  uint32_t hand_landmarks_count;
  struct Landmarks* hand_world_landmarks;
  uint32_t hand_world_landmarks_count;
};

#ifdef __cplusplus
}
#endif

#endif

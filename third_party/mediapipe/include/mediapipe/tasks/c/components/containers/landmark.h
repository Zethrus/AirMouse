#ifndef MEDIAPIPE_TASKS_C_COMPONENTS_CONTAINERS_LANDMARK_H_
#define MEDIAPIPE_TASKS_C_COMPONENTS_CONTAINERS_LANDMARK_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Landmark {
  float x;
  float y;
  float z;
  bool has_visibility;
  float visibility;
  bool has_presence;
  float presence;
  char* name;
};

struct NormalizedLandmark {
  float x;
  float y;
  float z;
  bool has_visibility;
  float visibility;
  bool has_presence;
  float presence;
  char* name;
};

struct Landmarks {
  struct Landmark* landmarks;
  uint32_t landmarks_count;
};

struct NormalizedLandmarks {
  struct NormalizedLandmark* landmarks;
  uint32_t landmarks_count;
};

#ifdef __cplusplus
}
#endif

#endif

#ifndef MEDIAPIPE_TASKS_C_VISION_CORE_IMAGE_PREROCESSING_OPTIONS_H_
#define MEDIAPIPE_TASKS_C_VISION_CORE_IMAGE_PREROCESSING_OPTIONS_H_

#include "mediapipe/tasks/c/components/containers/rect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ImageProcessingOptions {
  int has_region_of_interest;
  MPRectF region_of_interest;
  int rotation_degrees;
} ImageProcessingOptions;

#ifdef __cplusplus
}
#endif

#endif

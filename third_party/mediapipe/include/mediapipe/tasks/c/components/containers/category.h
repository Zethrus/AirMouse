#ifndef MEDIAPIPE_TASKS_C_COMPONENTS_CONTAINERS_CATEGORY_H_
#define MEDIAPIPE_TASKS_C_COMPONENTS_CONTAINERS_CATEGORY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Category {
  int index;
  float score;
  char* category_name;
  char* display_name;
};

struct Categories {
  struct Category* categories;
  uint32_t categories_count;
};

#ifdef __cplusplus
}
#endif

#endif

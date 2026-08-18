#ifndef MEDIAPIPE_TASKS_C_COMPONENTS_CONTAINERS_RECT_H_
#define MEDIAPIPE_TASKS_C_COMPONENTS_CONTAINERS_RECT_H_

#ifdef __cplusplus
extern "C" {
#endif

struct MPRect {
  int left;
  int top;
  int bottom;
  int right;
};

struct MPRectF {
  float left;
  float top;
  float bottom;
  float right;
};

#ifdef __cplusplus
}
#endif

#endif

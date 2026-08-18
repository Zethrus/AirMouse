#ifndef MEDIAPIPE_TASKS_C_CORE_COMMON_H_
#define MEDIAPIPE_TASKS_C_CORE_COMMON_H_

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

typedef struct {
  char** strings;
  int num_strings;
} MpStringList;

MP_EXPORT void MpStringListFree(MpStringList* string_list);
MP_EXPORT void MpErrorFree(char* error_message);

#ifdef __cplusplus
}
#endif

#endif

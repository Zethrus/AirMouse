#ifndef MEDIAPIPE_TASKS_C_CORE_MP_STATUS_H_
#define MEDIAPIPE_TASKS_C_CORE_MP_STATUS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MpStatus {
  kMpOk = 0,
  kMpCancelled = 1,
  kMpUnknown = 2,
  kMpInvalidArgument = 3,
  kMpDeadlineExceeded = 4,
  kMpNotFound = 5,
  kMpAlreadyExists = 6,
  kMpPermissionDenied = 7,
  kMpResourceExhausted = 8,
  kMpFailedPrecondition = 9,
  kMpAborted = 10,
  kMpOutOfRange = 11,
  kMpUnimplemented = 12,
  kMpInternal = 13,
  kMpUnavailable = 14,
  kMpDataLoss = 15,
  kMpUnauthenticated = 16,
} MpStatus;

#ifdef __cplusplus
}
#endif

#endif

#ifndef MEDIAPIPE_TASKS_C_CORE_BASE_OPTIONS_H_
#define MEDIAPIPE_TASKS_C_CORE_BASE_OPTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

enum Delegate {
  CPU = 0,
  GPU = 1,
  EDGETPU_NNAPI = 2,
};

enum HostEnvironment {
  HOST_ENVIRONMENT_UNKNOWN = 0,
  HOST_ENVIRONMENT_ANDROID = 1,
  HOST_ENVIRONMENT_IOS = 2,
  HOST_ENVIRONMENT_PYTHON = 3,
  HOST_ENVIRONMENT_WEB = 4,
};

enum HostSystem {
  HOST_SYSTEM_UNKNOWN = 0,
  HOST_SYSTEM_LINUX = 1,
  HOST_SYSTEM_MAC = 2,
  HOST_SYSTEM_WINDOWS = 3,
  HOST_SYSTEM_IOS = 4,
  HOST_SYSTEM_ANDROID = 5,
};

struct BaseOptions {
  const char* model_asset_buffer;
  unsigned int model_asset_buffer_count;
  const char* model_asset_path;
  enum Delegate delegate;
  enum HostEnvironment host_environment;
  enum HostSystem host_system;
  const char* host_version;
  const char* ca_bundle_path;
};

#ifdef __cplusplus
}
#endif

#endif

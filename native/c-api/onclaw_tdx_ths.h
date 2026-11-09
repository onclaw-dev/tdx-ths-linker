#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(ONCLAW_TDX_THS_CAPI_EXPORTS)
#define ONCLAW_TDX_THS_API __declspec(dllexport)
#elif defined(_WIN32)
#define ONCLAW_TDX_THS_API __declspec(dllimport)
#else
#define ONCLAW_TDX_THS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ONCLAW_TDX_THS_ABI_VERSION 1u
#define ONCLAW_TDX_THS_NAME_CAPACITY 8u
#define ONCLAW_TDX_THS_CODE_CAPACITY 16u
#define ONCLAW_TDX_THS_HEALTH_CAPACITY 16u
#define ONCLAW_TDX_THS_STATUS_CAPACITY 32u
#define ONCLAW_TDX_THS_MESSAGE_CAPACITY 256u

typedef enum onclaw_tdx_ths_status {
  ONCLAW_TDX_THS_OK = 0,
  ONCLAW_TDX_THS_INVALID_ARGUMENT = 1,
  ONCLAW_TDX_THS_INVALID_HANDLE = 2,
  ONCLAW_TDX_THS_INTERNAL_ERROR = 3
} onclaw_tdx_ths_status;

typedef struct onclaw_tdx_ths_runtime* onclaw_tdx_ths_handle;

typedef struct onclaw_tdx_ths_endpoint_state_v1 {
  uint32_t struct_size;
  char name[ONCLAW_TDX_THS_NAME_CAPACITY];
  char code[ONCLAW_TDX_THS_CODE_CAPACITY];
  char health[ONCLAW_TDX_THS_HEALTH_CAPACITY];
  char last_error[ONCLAW_TDX_THS_MESSAGE_CAPACITY];
  uint64_t updated_at;
  uint64_t last_scan_at;
  uint64_t last_resolved_offset;
  uint32_t available;
  uint32_t using_fallback;
  uint32_t direct_read;
  uint32_t pid;
} onclaw_tdx_ths_endpoint_state_v1;

typedef struct onclaw_tdx_ths_runtime_state_v1 {
  uint32_t struct_size;
  uint32_t initialized;
  uint32_t supported;
  char runtime_health[ONCLAW_TDX_THS_HEALTH_CAPACITY];
  char license_state[ONCLAW_TDX_THS_HEALTH_CAPACITY];
  char message[ONCLAW_TDX_THS_MESSAGE_CAPACITY];
  onclaw_tdx_ths_endpoint_state_v1 ths;
  onclaw_tdx_ths_endpoint_state_v1 tdx;
} onclaw_tdx_ths_runtime_state_v1;

typedef struct onclaw_tdx_ths_init_result_v1 {
  uint32_t struct_size;
  uint32_t ok;
  char license_state[ONCLAW_TDX_THS_HEALTH_CAPACITY];
  char runtime_health[ONCLAW_TDX_THS_HEALTH_CAPACITY];
  char message[ONCLAW_TDX_THS_MESSAGE_CAPACITY];
} onclaw_tdx_ths_init_result_v1;

typedef struct onclaw_tdx_ths_command_result_v1 {
  uint32_t struct_size;
  uint32_t ok;
  uint32_t accepted;
  char status[ONCLAW_TDX_THS_STATUS_CAPACITY];
  char target[ONCLAW_TDX_THS_NAME_CAPACITY];
  char code[ONCLAW_TDX_THS_CODE_CAPACITY];
  char message[ONCLAW_TDX_THS_MESSAGE_CAPACITY];
} onclaw_tdx_ths_command_result_v1;

ONCLAW_TDX_THS_API uint32_t onclaw_tdx_ths_abi_version(void);
ONCLAW_TDX_THS_API onclaw_tdx_ths_handle onclaw_tdx_ths_create(void);
ONCLAW_TDX_THS_API void onclaw_tdx_ths_destroy(onclaw_tdx_ths_handle handle);
ONCLAW_TDX_THS_API onclaw_tdx_ths_status onclaw_tdx_ths_initialize(
    onclaw_tdx_ths_handle handle, uint32_t license_accepted,
    onclaw_tdx_ths_init_result_v1* result);
ONCLAW_TDX_THS_API onclaw_tdx_ths_status onclaw_tdx_ths_get_state(
    onclaw_tdx_ths_handle handle, onclaw_tdx_ths_runtime_state_v1* state);
ONCLAW_TDX_THS_API onclaw_tdx_ths_status onclaw_tdx_ths_set_code(
    onclaw_tdx_ths_handle handle, const char* target, const char* code,
    onclaw_tdx_ths_command_result_v1* result);

#ifdef __cplusplus
}
#endif

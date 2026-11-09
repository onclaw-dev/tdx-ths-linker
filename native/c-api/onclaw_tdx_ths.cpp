#include "onclaw_tdx_ths.h"

#include "linker_runtime.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <string>

struct onclaw_tdx_ths_runtime {
  onclaw_tdx_ths::Runtime value;
};

namespace {
template <size_t Size>
void CopyText(char (&target)[Size], const std::string& value) {
  const auto count = (std::min)(value.size(), Size - 1);
  std::memcpy(target, value.data(), count);
  target[count] = '\0';
}

bool BoundedText(const char* value, size_t capacity, std::string* result) {
  if (!value || !result) return false;
  const auto* end = static_cast<const char*>(std::memchr(value, '\0', capacity));
  if (!end) return false;
  result->assign(value, end);
  return true;
}

bool Valid(const onclaw_tdx_ths_init_result_v1* value) {
  return value && value->struct_size >= sizeof(*value);
}

bool Valid(const onclaw_tdx_ths_runtime_state_v1* value) {
  return value && value->struct_size >= sizeof(*value);
}

bool Valid(const onclaw_tdx_ths_command_result_v1* value) {
  return value && value->struct_size >= sizeof(*value);
}

void EndpointDto(const onclaw_tdx_ths::EndpointState& source,
                 onclaw_tdx_ths_endpoint_state_v1* target) {
  *target = {};
  target->struct_size = sizeof(*target);
  CopyText(target->name, source.name);
  CopyText(target->code, source.code);
  CopyText(target->health, source.health);
  CopyText(target->last_error, source.lastError);
  target->updated_at = source.updatedAt;
  target->last_scan_at = source.lastScanAt;
  target->last_resolved_offset = source.lastOffset;
  target->available = source.available ? 1u : 0u;
  target->using_fallback = source.usingFallback ? 1u : 0u;
  target->direct_read = source.directRead ? 1u : 0u;
  target->pid = source.pid;
}
}  // namespace

uint32_t onclaw_tdx_ths_abi_version(void) {
  return ONCLAW_TDX_THS_ABI_VERSION;
}

onclaw_tdx_ths_handle onclaw_tdx_ths_create(void) {
  try {
    return new (std::nothrow) onclaw_tdx_ths_runtime{};
  } catch (...) {
    return nullptr;
  }
}

void onclaw_tdx_ths_destroy(onclaw_tdx_ths_handle handle) {
  try {
    delete handle;
  } catch (...) {
  }
}

onclaw_tdx_ths_status onclaw_tdx_ths_initialize(
    onclaw_tdx_ths_handle handle, uint32_t license_accepted,
    onclaw_tdx_ths_init_result_v1* result) {
  if (!handle) return ONCLAW_TDX_THS_INVALID_HANDLE;
  if (!Valid(result)) return ONCLAW_TDX_THS_INVALID_ARGUMENT;
  try {
    const auto value = handle->value.Init(license_accepted != 0);
    *result = {};
    result->struct_size = sizeof(*result);
    result->ok = value.ok ? 1u : 0u;
    CopyText(result->license_state, value.licenseState);
    CopyText(result->runtime_health, value.runtimeHealth);
    CopyText(result->message, value.message);
    return ONCLAW_TDX_THS_OK;
  } catch (...) {
    return ONCLAW_TDX_THS_INTERNAL_ERROR;
  }
}

onclaw_tdx_ths_status onclaw_tdx_ths_get_state(
    onclaw_tdx_ths_handle handle, onclaw_tdx_ths_runtime_state_v1* state) {
  if (!handle) return ONCLAW_TDX_THS_INVALID_HANDLE;
  if (!Valid(state)) return ONCLAW_TDX_THS_INVALID_ARGUMENT;
  try {
    const auto value = handle->value.State();
    *state = {};
    state->struct_size = sizeof(*state);
    state->initialized = value.initialized ? 1u : 0u;
    state->supported = value.supported ? 1u : 0u;
    CopyText(state->runtime_health, value.runtimeHealth);
    CopyText(state->license_state, value.licenseState);
    CopyText(state->message, value.message);
    EndpointDto(value.ths, &state->ths);
    EndpointDto(value.tdx, &state->tdx);
    return ONCLAW_TDX_THS_OK;
  } catch (...) {
    return ONCLAW_TDX_THS_INTERNAL_ERROR;
  }
}

onclaw_tdx_ths_status onclaw_tdx_ths_set_code(
    onclaw_tdx_ths_handle handle, const char* target, const char* code,
    onclaw_tdx_ths_command_result_v1* result) {
  if (!handle) return ONCLAW_TDX_THS_INVALID_HANDLE;
  if (!Valid(result)) return ONCLAW_TDX_THS_INVALID_ARGUMENT;
  std::string targetValue;
  std::string codeValue;
  if (!BoundedText(target, ONCLAW_TDX_THS_NAME_CAPACITY, &targetValue) ||
      !BoundedText(code, ONCLAW_TDX_THS_CODE_CAPACITY, &codeValue)) {
    return ONCLAW_TDX_THS_INVALID_ARGUMENT;
  }
  try {
    const auto value = handle->value.Queue(targetValue, codeValue);
    *result = {};
    result->struct_size = sizeof(*result);
    result->ok = value.ok ? 1u : 0u;
    result->accepted = value.accepted ? 1u : 0u;
    CopyText(result->status, value.status);
    CopyText(result->target, value.target);
    CopyText(result->code, value.code);
    CopyText(result->message, value.message);
    return ONCLAW_TDX_THS_OK;
  } catch (...) {
    return ONCLAW_TDX_THS_INTERNAL_ERROR;
  }
}

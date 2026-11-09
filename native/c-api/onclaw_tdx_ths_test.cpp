#include "onclaw_tdx_ths.h"

#include <cassert>

int main() {
  assert(onclaw_tdx_ths_abi_version() == ONCLAW_TDX_THS_ABI_VERSION);

  onclaw_tdx_ths_init_result_v1 invalid{};
  assert(onclaw_tdx_ths_initialize(nullptr, 0, &invalid) ==
         ONCLAW_TDX_THS_INVALID_HANDLE);

  const auto handle = onclaw_tdx_ths_create();
  assert(handle != nullptr);
  assert(onclaw_tdx_ths_initialize(handle, 0, &invalid) ==
         ONCLAW_TDX_THS_INVALID_ARGUMENT);

  onclaw_tdx_ths_init_result_v1 initialized{};
  initialized.struct_size = sizeof(initialized);
  assert(onclaw_tdx_ths_initialize(handle, 0, &initialized) ==
         ONCLAW_TDX_THS_OK);
  assert(initialized.ok == 0);

  onclaw_tdx_ths_runtime_state_v1 state{};
  state.struct_size = sizeof(state);
  assert(onclaw_tdx_ths_get_state(handle, &state) == ONCLAW_TDX_THS_OK);
  assert(state.initialized == 0);

  onclaw_tdx_ths_command_result_v1 command{};
  command.struct_size = sizeof(command);
  assert(onclaw_tdx_ths_set_code(handle, "tdx", "600825", &command) ==
         ONCLAW_TDX_THS_OK);
  assert(command.accepted == 0);

  onclaw_tdx_ths_destroy(handle);
  return 0;
}

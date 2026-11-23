#include "linker_runtime.h"

#include <windows.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace std::chrono_literals;

alignas(16) char pollingCode[16] = "600123";

std::string ExecutableName() {
  char path[MAX_PATH]{};
  const auto length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  assert(length > 0 && length < MAX_PATH);
  const auto* slash = std::strrchr(path, '\\');
  return slash ? slash + 1 : path;
}

bool WaitUntil(const std::function<bool()>& predicate,
               std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) return true;
    std::this_thread::sleep_for(5ms);
  }
  return predicate();
}

void TestCommandScheduling() {
  std::mutex observedMutex;
  std::vector<std::pair<std::string, std::string>> observed;

  auto config = onclaw_tdx_ths::DefaultRuntimeConfig();
  config.pollMs = 5;
  config.scanRetryMs = 1000;
  config.burstWindowMs = 40;
  config.idleTimeoutMs = 80;
  config.ths.processes = {"__onclaw_missing_ths__.exe"};
  config.tdx.processes = {"__onclaw_missing_tdx__.exe"};
  config.commandObserver = [&](const std::string& target,
                               const std::string& code) {
    std::lock_guard lock(observedMutex);
    observed.emplace_back(target, code);
  };

  onclaw_tdx_ths::Runtime runtime(config);
  assert(runtime.Init(true).ok);
  assert(runtime.Snapshot().active);

  const auto leading = runtime.Queue("tdx", "600000", "client-a", "request-1");
  assert(leading.status == "accepted");
  assert(runtime.Queue("tdx", "600001", "client-a", "request-2").status ==
         "coalesced");
  assert(runtime.Queue("tdx", "600002", "client-a", "request-3").status ==
         "coalesced");
  assert(runtime.Queue("tdx", "600003", "client-a", "request-4").status ==
         "coalesced");
  assert(runtime.Queue("tdx", "600004", "client-a", "request-5").status ==
         "coalesced");
  assert(runtime.Queue("tdx", "600004", "client-a", "request-5").status ==
         "duplicate");
  assert(runtime.Queue("ths", "000001", "client-b", "request-5").status ==
         "accepted");

  std::this_thread::sleep_for(70ms);
  {
    std::lock_guard lock(observedMutex);
    const auto count = [&](const std::string& target, const std::string& code) {
      return std::count(observed.begin(), observed.end(),
                        std::make_pair(target, code));
    };
    assert(count("tdx", "600000") == 1);
    assert(count("tdx", "600004") == 1);
    assert(count("tdx", "600001") == 0);
    assert(count("tdx", "600002") == 0);
    assert(count("tdx", "600003") == 0);
    assert(count("ths", "000001") == 1);
  }

  std::this_thread::sleep_for(80ms);
  assert(!runtime.Snapshot().active);
  assert(runtime.Snapshot().runtimeHealth == "idle");
  assert(runtime.State().active);
  assert(runtime.Queue("tdx", "600005", "client-a", "request-6").accepted);
  runtime.Stop();
}

void TestPollingCacheAndRetry() {
  auto config = onclaw_tdx_ths::DefaultRuntimeConfig();
  config.pollMs = 5;
  config.scanRetryMs = 80;
  config.scanTimeoutMs = 100;
  config.idleTimeoutMs = 1000;
  config.ths.processes = {"__onclaw_missing_ths__.exe"};

  const auto executable = ExecutableName();
  const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
  const auto address = reinterpret_cast<std::uintptr_t>(pollingCode);
  assert(address >= base && address - base <= UINT32_MAX);
  config.tdx = {"tdx",
                {executable},
                executable,
                "invalid-pattern",
                {static_cast<std::uint32_t>(address - base)},
                true};

  std::memcpy(pollingCode, "600123\0", 7);
  onclaw_tdx_ths::Runtime runtime(config);
  assert(runtime.Init(true).ok);
  assert(WaitUntil(
      [&] {
        const auto state = runtime.State();
        return state.tdx.available && state.tdx.code == "600123" &&
               state.tdx.usingFallback;
      },
      500ms));

  const auto initial = runtime.Snapshot();
  assert(initial.tdx.lastScanAt != 0);
  for (int i = 0; i < 4; ++i) {
    std::this_thread::sleep_for(50ms);
    runtime.State();
  }
  const auto stable = runtime.Snapshot();
  assert(stable.tdx.available);
  assert(stable.tdx.lastScanAt == initial.tdx.lastScanAt);

  std::memcpy(pollingCode, "ABCDEF\0", 7);
  assert(WaitUntil(
      [&] {
        runtime.State();
        return !runtime.Snapshot().tdx.available;
      },
      200ms));
  const auto failed = runtime.Snapshot();
  assert(failed.tdx.lastOffset == 0);
  std::this_thread::sleep_for(35ms);
  runtime.State();
  assert(runtime.Snapshot().tdx.lastScanAt == failed.tdx.lastScanAt);

  assert(WaitUntil(
      [&] {
        runtime.State();
        return runtime.Snapshot().tdx.lastScanAt > failed.tdx.lastScanAt;
      },
      300ms));

  std::memcpy(pollingCode, "600124\0", 7);
  assert(WaitUntil(
      [&] {
        const auto state = runtime.State();
        return state.tdx.available && state.tdx.code == "600124";
      },
      400ms));
  runtime.Stop();
}
}  // namespace

int main() {
  TestCommandScheduling();
  TestPollingCacheAndRetry();
  return 0;
}

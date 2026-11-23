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
  config.hotLeaseMs = 80;
  config.commandConfirmMs = 40;
  config.warmHandleMs = 500;
  config.coldCacheMs = 1000;
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
  const auto command = runtime.CommandState("client-b", "request-5");
  assert(command && command->state == "timed_out");
  assert(!runtime.Snapshot().active);
  assert(runtime.Snapshot().activityState == "warm");
  assert(runtime.Snapshot().stale);
  assert(runtime.State().active);
  assert(runtime.Queue("tdx", "600005", "client-a", "request-6").accepted);
  runtime.Stop();
}

void TestPollingCacheAndRetry() {
  auto config = onclaw_tdx_ths::DefaultRuntimeConfig();
  config.pollMs = 5;
  config.scanRetryMs = 80;
  config.scanTimeoutMs = 100;
  config.hotLeaseMs = 1000;
  config.ths.processes = {"__onclaw_missing_ths__.exe"};

  const auto executable = ExecutableName();
  const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
  const auto address = reinterpret_cast<std::uintptr_t>(pollingCode);
  assert(address >= base && address - base <= UINT32_MAX);
  config.tdx = {"tdx",
                {executable},
                executable,
                "invalid-pattern",
                {static_cast<std::uint32_t>(address - base + 8),
                 static_cast<std::uint32_t>(address - base)},
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

  std::memcpy(pollingCode, "12A3456\0", 8);
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

void TestActivityLifecycle() {
  auto config = onclaw_tdx_ths::DefaultRuntimeConfig();
  config.pollMs = 5;
  config.hotLeaseMs = 20;
  config.warmHandleMs = 80;
  config.coldCacheMs = 160;
  config.ths.processes = {"__onclaw_missing_ths__.exe"};
  config.tdx.processes = {"__onclaw_missing_tdx__.exe"};

  onclaw_tdx_ths::Runtime runtime(config);
  assert(runtime.Init(true).ok);
  assert(WaitUntil([&] { return runtime.Snapshot().activityState == "warm"; },
                   250ms));
  assert(!runtime.Snapshot().active);
  assert(runtime.Snapshot().stale);
  assert(WaitUntil([&] { return runtime.Snapshot().activityState == "cool"; },
                   250ms));
  assert(WaitUntil([&] { return runtime.Snapshot().activityState == "cold"; },
                   250ms));
  const auto before = runtime.Snapshot().stateGeneration;
  const auto fresh = runtime.State();
  assert(fresh.activityState == "hot");
  assert(!fresh.stale);
  assert(fresh.stateGeneration > before);
  runtime.Stop();
}

void TestWarmStopsPollingAndWakeRefreshes() {
  auto config = onclaw_tdx_ths::DefaultRuntimeConfig();
  config.pollMs = 5;
  config.hotLeaseMs = 30;
  config.warmHandleMs = 300;
  config.coldCacheMs = 600;
  config.ths.processes = {"__onclaw_missing_ths__.exe"};
  const auto executable = ExecutableName();
  const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
  const auto address = reinterpret_cast<std::uintptr_t>(pollingCode);
  config.tdx = {"tdx", {executable}, executable, "invalid-pattern",
                {static_cast<std::uint32_t>(address - base)}, true};

  std::memcpy(pollingCode, "600200\0", 7);
  onclaw_tdx_ths::Runtime runtime(config);
  assert(runtime.Init(true).ok);
  assert(WaitUntil([&] { return runtime.Snapshot().tdx.code == "600200"; },
                   300ms));
  assert(runtime.Snapshot().runtimeHealth == "degraded");
  assert(WaitUntil([&] { return runtime.Snapshot().activityState == "warm"; },
                   250ms));
  const auto generation = runtime.Snapshot().stateGeneration;
  std::memcpy(pollingCode, "600201\0", 7);
  std::this_thread::sleep_for(50ms);
  assert(runtime.Snapshot().tdx.code == "600200");
  assert(runtime.Snapshot().stateGeneration == generation);
  const auto refreshed = runtime.State();
  assert(!refreshed.stale);
  assert(refreshed.tdx.code == "600201");
  assert(refreshed.stateGeneration > generation);
  runtime.Stop();
}

void TestCommandReadbackConfirmation() {
  auto config = onclaw_tdx_ths::DefaultRuntimeConfig();
  config.pollMs = 5;
  config.burstWindowMs = 10;
  config.commandConfirmMs = 200;
  config.ths.processes = {"__onclaw_missing_ths__.exe"};
  const auto executable = ExecutableName();
  const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
  const auto address = reinterpret_cast<std::uintptr_t>(pollingCode);
  config.tdx = {"tdx", {executable}, executable, "invalid-pattern",
                {static_cast<std::uint32_t>(address - base)}, true};
  config.commandObserver = [](const std::string& target,
                              const std::string& code) {
    if (target == "tdx") std::memcpy(pollingCode, code.c_str(), 6);
  };

  std::memcpy(pollingCode, "600300\0", 7);
  onclaw_tdx_ths::Runtime runtime(config);
  assert(runtime.Init(true).ok);
  assert(WaitUntil([&] { return runtime.Snapshot().tdx.available; }, 300ms));
  assert(runtime.Queue("tdx", "600301", "client-confirm", "request-1")
             .accepted);
  assert(WaitUntil(
      [&] {
        const auto status =
            runtime.CommandState("client-confirm", "request-1");
        return status && status->state == "confirmed";
      },
      300ms));
  assert(runtime.Snapshot().lastCommand &&
         runtime.Snapshot().lastCommand->state == "confirmed");
  runtime.Stop();
}
}  // namespace

int main() {
  TestCommandScheduling();
  TestPollingCacheAndRetry();
  TestActivityLifecycle();
  TestWarmStopsPollingAndWakeRefreshes();
  TestCommandReadbackConfirmation();
  return 0;
}

#include "linker_runtime.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

int main() {
  using namespace std::chrono_literals;
  std::mutex observedMutex;
  std::vector<std::pair<std::string, std::string>> observed;

  auto config = onclaw_tdx_ths::DefaultRuntimeConfig();
  config.pollMs = 5;
  config.burstWindowMs = 40;
  config.idleTimeoutMs = 80;
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
  return 0;
}

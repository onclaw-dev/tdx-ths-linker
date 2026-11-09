#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace onclaw_tdx_ths {

struct EndpointConfig {
  std::string name;
  std::vector<std::string> processes;
  std::string moduleName;
  std::string pattern;
  std::vector<std::uint32_t> fallbacks;
  bool directRead = false;
};

struct RuntimeConfig {
  std::uint32_t pollMs = 350;
  std::uint32_t scanRetryMs = 4000;
  std::uint32_t scanTimeoutMs = 2500;
  std::uint32_t commandTimeoutMs = 1500;
  EndpointConfig ths;
  EndpointConfig tdx;
};

struct EndpointState {
  std::string name;
  std::string code;
  std::string health = "idle";
  std::string lastError;
  std::uint64_t updatedAt = 0;
  std::uint64_t lastScanAt = 0;
  std::uint32_t lastOffset = 0;
  bool available = false;
  bool usingFallback = false;
  bool directRead = false;
  std::uint32_t pid = 0;
};

struct RuntimeState {
  bool initialized = false;
  bool supported = true;
  std::string runtimeHealth = "idle";
  std::string licenseState = "unaccepted";
  std::string message;
  EndpointState ths{"ths"};
  EndpointState tdx{"tdx"};
};

struct InitResult {
  bool ok = false;
  std::string licenseState;
  std::string message;
  std::string runtimeHealth;
};

struct CommandResult {
  bool ok = false;
  bool accepted = false;
  std::string status;
  std::string target;
  std::string code;
  std::string message;
};

RuntimeConfig DefaultRuntimeConfig();

class Runtime {
 public:
  Runtime();
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  InitResult Init(bool licenseAccepted);
  RuntimeState State();
  CommandResult Queue(const std::string& target, const std::string& code);
  void Stop();

 private:
  struct Command { std::string target; std::string code; };
  void Start();
  void Loop();
  void ProcessCommands(const RuntimeConfig& config);
  void Poll(const RuntimeConfig& config, const EndpointConfig& endpoint);
  void ResolveOffset(void* process, std::uint32_t pid, std::uintptr_t base,
                     std::uint32_t size, const RuntimeConfig& runtime,
                     const EndpointConfig& endpoint, EndpointState* state);
  std::string ReadCode(void* process, std::uintptr_t base,
                       const EndpointConfig& config,
                       const EndpointState& state);
  void ExecuteThs(std::uint32_t timeoutMs, const std::string& code);
  void ExecuteTdx(const std::string& code);
  void* FindMainWindow(std::uint32_t pid);
  EndpointState CopyEndpoint(const std::string& name);
  void StoreEndpoint(const EndpointState& endpoint);
  void SetUnavailable(const std::string& name, const std::string& error);
  void SetError(const std::string& name, const std::string& error);
  void RecomputeHealth();

  std::mutex mutex_;
  RuntimeConfig config_;
  RuntimeState state_;
  std::mutex queueMutex_;
  std::condition_variable condition_;
  std::deque<Command> commands_;
  std::atomic<bool> running_{false};
  std::thread worker_;
};

}  // namespace onclaw_tdx_ths

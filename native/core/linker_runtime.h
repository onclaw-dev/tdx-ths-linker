#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <utility>
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
  std::uint32_t burstWindowMs = 300;
  std::uint32_t hotLeaseMs = 10000;
  std::uint32_t warmHandleMs = 300000;
  std::uint32_t coldCacheMs = 600000;
  std::uint32_t stateFreshnessMs = 500;
  std::uint32_t discoveryRetryMinMs = 250;
  std::uint32_t discoveryRetryMaxMs = 2000;
  std::uint32_t scanRetryMaxMs = 60000;
  std::uint32_t readFailureThreshold = 3;
  std::uint32_t commandConfirmMs = 10000;
  std::uint32_t dedupeCapacity = 256;
  std::function<void(const std::string&, const std::string&)> commandObserver;
  EndpointConfig ths;
  EndpointConfig tdx;
};

struct CommandStatus {
  std::string target;
  std::string code;
  std::string clientId;
  std::string requestId;
  std::string state;
  std::string errorStage;
  std::string message;
  std::uint64_t queuedAt = 0;
  std::uint64_t dispatchedAt = 0;
  std::uint64_t completedAt = 0;
  std::uint32_t win32Error = 0;
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
  bool active = false;
  std::uint64_t lastActivityAt = 0;
  std::string activityState = "cold";
  std::uint64_t stateGeneration = 0;
  bool stale = true;
  std::uint64_t refreshedAt = 0;
  std::string runtimeHealth = "idle";
  std::string licenseState = "unaccepted";
  std::string message;
  EndpointState ths{"ths"};
  EndpointState tdx{"tdx"};
  std::optional<CommandStatus> lastCommand;
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
  std::string clientId;
  std::string requestId;
  std::string message;
};

RuntimeConfig DefaultRuntimeConfig();

class Runtime {
 public:
  Runtime();
  explicit Runtime(RuntimeConfig config);
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  InitResult Init(bool licenseAccepted);
  RuntimeState State();
  RuntimeState Snapshot();
  CommandResult Queue(const std::string& target, const std::string& code,
                      const std::string& clientId = "anonymous",
                      const std::string& requestId = "");
  std::optional<CommandStatus> CommandState(const std::string& clientId,
                                             const std::string& requestId);
  void Stop();

 private:
  using TimePoint = std::chrono::steady_clock::time_point;
  struct Command {
    std::string target;
    std::string code;
    std::string clientId;
    std::string requestId;
  };
  struct CommandLane {
    std::optional<Command> ready;
    std::optional<Command> trailing;
    std::string leadingCode;
    TimePoint deadline{};
    bool windowActive = false;
  };
  struct EndpointSession {
    void* process = nullptr;
    std::uint32_t pid = 0;
    std::uintptr_t base = 0;
    std::uint32_t size = 0;
    std::uint64_t creationTime = 0;
    std::uint32_t discoveryDelayMs = 0;
    std::uint32_t resolveDelayMs = 0;
    std::uint32_t readFailures = 0;
    std::uintptr_t signatureAddress = 0;
    std::vector<std::uint8_t> signatureBytes;
    std::vector<bool> signatureMask;
    TimePoint firstReadFailureAt{};
    TimePoint nextSignatureAt{};
    TimePoint nextDiscoveryAt{};
    TimePoint nextResolveAt{};
  };
  struct DeferredAllocation {
    void* process = nullptr;
    void* address = nullptr;
    TimePoint releaseAt{};
  };
  void Start();
  void Loop();
  void ProcessCommands(const RuntimeConfig& config);
  void TouchActivity();
  void PromoteDueLocked(CommandLane* lane, TimePoint now);
  CommandLane& Lane(const std::string& target);
  EndpointSession& Session(const std::string& target);
  void ResetSession(EndpointSession* session);
  void CloseSessionHandle(EndpointSession* session);
  void ResetSessions();
  void Poll(const RuntimeConfig& config, const EndpointConfig& endpoint);
  void ResolveOffset(void* process, std::uint32_t pid, std::uintptr_t base,
                     std::uint32_t size, const RuntimeConfig& runtime,
                     const EndpointConfig& endpoint, EndpointState* state);
  std::string ReadCode(void* process, std::uintptr_t base,
                       const EndpointConfig& config,
                       const EndpointState& state);
  bool ExecuteThs(std::uint32_t timeoutMs, const std::string& code,
                  std::string* error, std::uint32_t* win32Error);
  bool ExecuteTdx(const std::string& code, std::string* error,
                  std::uint32_t* win32Error);
  void* FindMainWindow(std::uint32_t pid);
  EndpointState CopyEndpoint(const std::string& name);
  void StoreEndpoint(const EndpointState& endpoint);
  void SetUnavailable(const std::string& name, const std::string& error);
  void RecomputeHealth();
  void UpdateLifecycle(TimePoint now, const RuntimeConfig& config,
                       bool hasPendingCommands);
  void CompleteConfirmations(TimePoint now, const RuntimeConfig& config);
  void CleanupDeferredAllocations(TimePoint now);
  void StoreCommandStatus(const CommandStatus& status);
  std::string CommandKey(const std::string& clientId,
                         const std::string& requestId) const;

  std::mutex mutex_;
  RuntimeConfig config_;
  RuntimeState state_;
  std::mutex queueMutex_;
  std::condition_variable condition_;
  CommandLane thsCommands_;
  CommandLane tdxCommands_;
  EndpointSession thsSession_;
  EndpointSession tdxSession_;
  std::deque<std::string> recentRequestOrder_;
  std::unordered_set<std::string> recentRequests_;
  std::deque<std::string> commandOrder_;
  std::unordered_map<std::string, CommandStatus> commandHistory_;
  std::vector<CommandStatus> pendingConfirmations_;
  std::deque<DeferredAllocation> deferredAllocations_;
  TimePoint activeUntil_{};
  TimePoint lastBusinessActivity_{};
  std::condition_variable stateCondition_;
  std::atomic<bool> running_{false};
  std::thread worker_;
};

}  // namespace onclaw_tdx_ths

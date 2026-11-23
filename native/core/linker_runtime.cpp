#include "linker_runtime.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace onclaw_tdx_ths {
namespace {
using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;

std::uint64_t NowMs() { return GetTickCount64(); }

std::vector<std::string> Split(const std::string& value, char delimiter) {
  std::vector<std::string> result;
  std::stringstream stream(value);
  std::string part;
  while (std::getline(stream, part, delimiter)) {
    if (!part.empty()) result.push_back(part);
  }
  return result;
}

bool SixDigits(const std::string& code) {
  return code.size() == 6 &&
         std::all_of(code.begin(), code.end(), [](unsigned char ch) {
           return ch >= '0' && ch <= '9';
         });
}

std::string Digits(const char* data, std::size_t size, bool fromEnd) {
  std::vector<std::string> matches;
  for (std::size_t at = 0; at < size;) {
    if (data[at] < '0' || data[at] > '9') {
      ++at;
      continue;
    }
    const auto begin = at;
    while (at < size && data[at] >= '0' && data[at] <= '9') ++at;
    if (at - begin == 6) matches.emplace_back(data + begin, 6);
  }
  if (matches.empty()) return {};
  return fromEnd ? matches.back() : matches.front();
}

std::uint64_t ProcessCreationTime(HANDLE process) {
  FILETIME created{}, exited{}, kernel{}, user{};
  if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) return 0;
  ULARGE_INTEGER value{};
  value.LowPart = created.dwLowDateTime;
  value.HighPart = created.dwHighDateTime;
  return value.QuadPart;
}

DWORD FindProcess(const std::vector<std::string>& names) {
  const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return 0;
  PROCESSENTRY32 entry{};
  entry.dwSize = sizeof(entry);
  DWORD pid = 0;
  if (Process32First(snapshot, &entry)) {
    do {
      for (const auto& name : names) {
        if (_stricmp(entry.szExeFile, name.c_str()) == 0) {
          pid = entry.th32ProcessID;
          break;
        }
      }
    } while (pid == 0 && Process32Next(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return pid;
}

std::uintptr_t FindModule(DWORD pid, const std::string& name, DWORD* size) {
  const auto snapshot =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snapshot == INVALID_HANDLE_VALUE) return 0;
  MODULEENTRY32 entry{};
  entry.dwSize = sizeof(entry);
  std::uintptr_t base = 0;
  if (Module32First(snapshot, &entry)) {
    do {
      if (_stricmp(entry.szModule, name.c_str()) == 0) {
        base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
        *size = entry.modBaseSize;
        break;
      }
    } while (Module32Next(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return base;
}

struct WindowSearch {
  DWORD pid;
  std::vector<HWND> windows;
};

BOOL CALLBACK FindWindowCallback(HWND window, LPARAM value) {
  auto* search = reinterpret_cast<WindowSearch*>(value);
  DWORD pid = 0;
  GetWindowThreadProcessId(window, &pid);
  const auto exStyle = static_cast<LONG_PTR>(GetWindowLongPtrW(window, GWL_EXSTYLE));
  wchar_t className[256]{};
  wchar_t title[512]{};
  if (pid == search->pid && IsWindowVisible(window) &&
      GetWindow(window, GW_OWNER) == nullptr && (exStyle & WS_EX_TOOLWINDOW) == 0 &&
      GetClassNameW(window, className, 256) > 0 &&
      GetWindowTextW(window, title, 512) > 0) {
    search->windows.push_back(window);
  }
  return TRUE;
}
}  // namespace

RuntimeConfig DefaultRuntimeConfig() {
  RuntimeConfig config;
  config.ths = {
      "ths",
      {"hexin.exe"},
      "hexin.exe",
      "FF 35 ?? ?? ?? ?? 56 FF 15 ?? ?? ?? ?? 83 C4 08 85 C0 75 1F",
      {0x01B145F0, 0x01AA2680, 0x018F83C4, 0x0150E228, 0x017160E8},
      false,
  };
  config.tdx = {
      "tdx",
      {"tdxw.exe", "tdx.exe"},
      "Viewthem.dll",
      "B8 ?? ?? ?? ?? 8A 10 3A 11 75 1A 84 D2 74 12 8A 50 01 3A 51 01 "
      "75 0E 83 C0 02 83 C1 02 84 D2 75 E4 33 C0 EB 05 1B C0 83 D8 FF "
      "85 C0 75 44",
      {0x00038260, 0x000365E0, 0x00036560, 0x000366D0},
      false,
  };
  return config;
}

Runtime::Runtime() : Runtime(DefaultRuntimeConfig()) {}
Runtime::Runtime(RuntimeConfig config) : config_(std::move(config)) {}
Runtime::~Runtime() { Stop(); }

InitResult Runtime::Init(bool licenseAccepted) {
  Stop();
  {
    std::lock_guard lock(queueMutex_);
    thsCommands_ = {};
    tdxCommands_ = {};
    recentRequestOrder_.clear();
    recentRequests_.clear();
    commandOrder_.clear();
    commandHistory_.clear();
    pendingConfirmations_.clear();
    activeUntil_ = {};
    lastBusinessActivity_ = {};
  }
  {
    std::lock_guard lock(mutex_);
    state_ = RuntimeState{};
    state_.licenseState = licenseAccepted ? "accepted" : "unaccepted";
    state_.initialized = licenseAccepted;
    state_.runtimeHealth = licenseAccepted ? "initializing" : "unavailable";
    state_.activityState = licenseAccepted ? "hot" : "cold";
    state_.stale = true;
    state_.message = licenseAccepted
                         ? "onclaw-tdx-ths runtime initialized"
                         : "accept the noncommercial license before initialization";
    state_.ths.health = licenseAccepted ? "initializing" : "unavailable";
    state_.tdx.health = licenseAccepted ? "initializing" : "unavailable";
  }
  if (licenseAccepted) {
    Start();
    TouchActivity();
  }
  return {licenseAccepted,
          licenseAccepted ? "accepted" : "unaccepted",
          licenseAccepted ? "onclaw-tdx-ths runtime initialized"
                          : "accept the noncommercial license before initialization",
          licenseAccepted ? "initializing" : "unavailable"};
}

RuntimeState Runtime::State() {
  std::uint64_t generation = 0;
  bool needsRefresh = false;
  {
    std::lock_guard lock(mutex_);
    generation = state_.stateGeneration;
    needsRefresh = state_.stale || state_.activityState != "hot";
  }
  TouchActivity();
  if (needsRefresh) {
    std::unique_lock lock(mutex_);
    stateCondition_.wait_for(
        lock, Ms(config_.stateFreshnessMs == 0 ? 500 : config_.stateFreshnessMs),
        [&] { return state_.stateGeneration > generation || !running_; });
    if (state_.stateGeneration <= generation) state_.stale = true;
  }
  return Snapshot();
}

RuntimeState Runtime::Snapshot() {
  std::lock_guard lock(mutex_);
  return state_;
}

CommandResult Runtime::Queue(const std::string& target,
                             const std::string& code,
                             const std::string& clientId,
                             const std::string& requestId) {
  RuntimeState state;
  {
    std::lock_guard lock(mutex_);
    state = state_;
  }
  if (!state.initialized) {
    return {false, false, "unavailable", target, code, clientId, requestId,
            state.message};
  }
  if (!SixDigits(code)) {
    return {false, false, "invalid_code", target, code, clientId, requestId,
            "stock code must be six digits"};
  }
  if (target != "ths" && target != "tdx") {
    return {false, false, "unsupported_target", target, code, clientId,
            requestId,
            "unsupported target"};
  }
  const auto effectiveRequest =
      requestId.empty() ? std::string{} : CommandKey(clientId, requestId);
  std::string status;
  {
    std::lock_guard lock(queueMutex_);
    if (!effectiveRequest.empty() &&
        recentRequests_.find(effectiveRequest) != recentRequests_.end()) {
      return {true, true, "duplicate", target, code, clientId, requestId,
              "duplicate command ignored"};
    }
    if (!effectiveRequest.empty()) {
      recentRequests_.insert(effectiveRequest);
      recentRequestOrder_.push_back(effectiveRequest);
    }
    const auto capacity =
        config_.dedupeCapacity == 0 ? 256 : config_.dedupeCapacity;
    while (recentRequestOrder_.size() > capacity) {
      recentRequests_.erase(recentRequestOrder_.front());
      recentRequestOrder_.pop_front();
    }

    const auto now = Clock::now();
    auto& lane = Lane(target);
    PromoteDueLocked(&lane, now);
    Command command{target, code, clientId, requestId};
    CommandStatus commandStatus{target, code, clientId, requestId, "queued"};
    commandStatus.queuedAt = NowMs();
    StoreCommandStatus(commandStatus);
    if (!lane.windowActive && !lane.ready) {
      lane.ready = command;
      lane.leadingCode = code;
      lane.deadline =
          now + Ms(config_.burstWindowMs == 0 ? 300 : config_.burstWindowMs);
      lane.windowActive = true;
      status = "accepted";
    } else {
      if (!lane.windowActive) {
        lane.leadingCode = lane.ready ? lane.ready->code : code;
        lane.deadline =
            now + Ms(config_.burstWindowMs == 0 ? 300 : config_.burstWindowMs);
        lane.windowActive = true;
      }
      if (lane.trailing) {
        const auto oldKey = CommandKey(lane.trailing->clientId,
                                       lane.trailing->requestId);
        auto old = commandHistory_.find(oldKey);
        if (old != commandHistory_.end()) {
          old->second.state = "superseded";
          old->second.completedAt = NowMs();
          old->second.message = "command superseded by a newer coalesced value";
          StoreCommandStatus(old->second);
        }
      }
      if (code == lane.leadingCode) {
        lane.trailing.reset();
        commandStatus.state = "superseded";
        commandStatus.completedAt = NowMs();
        commandStatus.message = "equivalent leading command already queued";
        StoreCommandStatus(commandStatus);
      } else {
        lane.trailing = command;
      }
      status = "coalesced";
    }
  }
  TouchActivity();
  condition_.notify_all();
  return {true, true, status, target, code, clientId, requestId,
          status == "accepted" ? "command queued" : "latest command retained"};
}

void Runtime::TouchActivity() {
  RuntimeConfig config;
  {
    std::lock_guard lock(mutex_);
    if (!state_.initialized) return;
    config = config_;
    state_.active = true;
    state_.activityState = "hot";
    state_.lastActivityAt = NowMs();
    if (state_.runtimeHealth == "idle") state_.runtimeHealth = "initializing";
  }
  {
    std::lock_guard lock(queueMutex_);
    lastBusinessActivity_ = Clock::now();
    activeUntil_ = lastBusinessActivity_ +
                   Ms(config.hotLeaseMs == 0 ? 10000 : config.hotLeaseMs);
  }
  condition_.notify_all();
}

Runtime::CommandLane& Runtime::Lane(const std::string& target) {
  return target == "ths" ? thsCommands_ : tdxCommands_;
}

void Runtime::PromoteDueLocked(CommandLane* lane, TimePoint now) {
  if (!lane->windowActive || now < lane->deadline) return;
  if (!lane->ready && lane->trailing) {
    lane->ready = std::move(lane->trailing);
    lane->trailing.reset();
    lane->windowActive = false;
    lane->leadingCode.clear();
  } else if (!lane->trailing) {
    lane->windowActive = false;
    lane->leadingCode.clear();
  }
}

void Runtime::Start() {
  bool expected = false;
  if (running_.compare_exchange_strong(expected, true)) {
    worker_ = std::thread([this] { Loop(); });
  }
}

void Runtime::Stop() {
  bool expected = true;
  if (running_.compare_exchange_strong(expected, false)) {
    condition_.notify_all();
    stateCondition_.notify_all();
    if (worker_.joinable()) worker_.join();
  }
  ResetSessions();
}

void Runtime::Loop() {
  while (running_) {
    RuntimeConfig config;
    {
      std::lock_guard lock(mutex_);
      config = config_;
    }
    bool pending = false;
    const auto now = Clock::now();
    {
      std::lock_guard lock(queueMutex_);
      PromoteDueLocked(&thsCommands_, now);
      PromoteDueLocked(&tdxCommands_, now);
      pending = thsCommands_.ready || tdxCommands_.ready ||
                !pendingConfirmations_.empty();
    }
    UpdateLifecycle(now, config, pending);
    CleanupDeferredAllocations(now);
    bool hot = false;
    {
      std::lock_guard lock(mutex_);
      hot = state_.activityState == "hot";
    }
    if (!hot) {
      std::unique_lock lock(queueMutex_);
      condition_.wait_for(lock, Ms(100));
      continue;
    }
    ProcessCommands(config);
    Poll(config, config.ths);
    Poll(config, config.tdx);
    CompleteConfirmations(Clock::now(), config);
    RecomputeHealth();
    {
      std::lock_guard lock(mutex_);
      state_.stale = false;
      state_.refreshedAt = NowMs();
      ++state_.stateGeneration;
    }
    stateCondition_.notify_all();
    std::unique_lock lock(queueMutex_);
    auto wait = Ms(config.pollMs == 0 ? 350 : config.pollMs);
    const auto waitNow = Clock::now();
    for (const auto* lane : {&thsCommands_, &tdxCommands_}) {
      if (lane->windowActive && lane->deadline > waitNow) {
        wait = (std::min)(wait,
                          std::chrono::duration_cast<Ms>(lane->deadline - waitNow));
      }
    }
    condition_.wait_for(lock, wait);
  }
}

void Runtime::ProcessCommands(const RuntimeConfig& config) {
  std::deque<Command> pending;
  {
    std::lock_guard lock(queueMutex_);
    const auto now = Clock::now();
    PromoteDueLocked(&thsCommands_, now);
    PromoteDueLocked(&tdxCommands_, now);
    if (thsCommands_.ready) {
      pending.push_back(std::move(*thsCommands_.ready));
      thsCommands_.ready.reset();
    }
    if (tdxCommands_.ready) {
      pending.push_back(std::move(*tdxCommands_.ready));
      tdxCommands_.ready.reset();
    }
  }
  for (const auto& command : pending) {
    CommandStatus status{command.target, command.code, command.clientId,
                         command.requestId, "dispatching"};
    status.queuedAt = NowMs();
    {
      std::lock_guard lock(queueMutex_);
      const auto found = commandHistory_.find(CommandKey(command.clientId, command.requestId));
      if (found != commandHistory_.end()) status.queuedAt = found->second.queuedAt;
    }
    std::string error;
    std::uint32_t win32Error = 0;
    bool dispatched = true;
    if (config.commandObserver) {
      config.commandObserver(command.target, command.code);
    } else if (command.target == "ths") {
      dispatched = ExecuteThs(config.commandTimeoutMs, command.code, &error,
                              &win32Error);
    } else {
      dispatched = ExecuteTdx(command.code, &error, &win32Error);
    }
    status.dispatchedAt = NowMs();
    if (dispatched) {
      status.state = "dispatched";
      status.message = "command dispatched; awaiting readback";
      std::lock_guard lock(queueMutex_);
      StoreCommandStatus(status);
      pendingConfirmations_.push_back(status);
    } else {
      status.state = "failed";
      status.errorStage = "dispatch";
      status.message = error;
      status.win32Error = win32Error;
      status.completedAt = NowMs();
      std::lock_guard lock(queueMutex_);
      StoreCommandStatus(status);
    }
  }
}

Runtime::EndpointSession& Runtime::Session(const std::string& target) {
  return target == "ths" ? thsSession_ : tdxSession_;
}

void Runtime::ResetSession(EndpointSession* session) {
  CloseSessionHandle(session);
  *session = {};
}

void Runtime::CloseSessionHandle(EndpointSession* session) {
  if (session->process) CloseHandle(static_cast<HANDLE>(session->process));
  session->process = nullptr;
}

void Runtime::ResetSessions() {
  ResetSession(&thsSession_);
  ResetSession(&tdxSession_);
  while (!deferredAllocations_.empty()) {
    const auto allocation = deferredAllocations_.front();
    deferredAllocations_.pop_front();
    if (allocation.process && allocation.address)
      VirtualFreeEx(static_cast<HANDLE>(allocation.process), allocation.address,
                    0, MEM_RELEASE);
    if (allocation.process) CloseHandle(static_cast<HANDLE>(allocation.process));
  }
}

void Runtime::Poll(const RuntimeConfig& runtime,
                   const EndpointConfig& endpointConfig) {
  auto& session = Session(endpointConfig.name);
  const auto steadyNow = Clock::now();
  const auto scanRetry = runtime.scanRetryMs == 0 ? 4000 : runtime.scanRetryMs;

  if (session.process &&
      WaitForSingleObject(static_cast<HANDLE>(session.process), 0) !=
          WAIT_TIMEOUT) {
    ResetSession(&session);
    SetUnavailable(endpointConfig.name, "target process exited");
  }

  if (!session.process) {
    if (steadyNow < session.nextDiscoveryAt) return;
    auto pid = session.pid;
    HANDLE process = nullptr;
    if (pid) {
      process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION |
                                SYNCHRONIZE,
                            FALSE, pid);
      if (!process || (session.creationTime != 0 &&
                       ProcessCreationTime(process) != session.creationTime)) {
        if (process) CloseHandle(process);
        process = nullptr;
        pid = 0;
        ResetSession(&session);
      }
    }
    if (!pid) pid = FindProcess(endpointConfig.processes);
    if (!pid) {
      session.discoveryDelayMs = session.discoveryDelayMs == 0
          ? (runtime.discoveryRetryMinMs == 0 ? 250 : runtime.discoveryRetryMinMs)
          : (std::min)(session.discoveryDelayMs * 2,
                       runtime.discoveryRetryMaxMs == 0 ? 2000u
                                                        : runtime.discoveryRetryMaxMs);
      session.nextDiscoveryAt = steadyNow + Ms(session.discoveryDelayMs);
      SetUnavailable(endpointConfig.name, "target process not found");
      return;
    }
    DWORD size = 0;
    const auto base = FindModule(pid, endpointConfig.moduleName, &size);
    if (!base || !size) {
      session.nextDiscoveryAt = steadyNow + Ms(runtime.discoveryRetryMinMs == 0
                                                   ? 250
                                                   : runtime.discoveryRetryMinMs);
      SetUnavailable(endpointConfig.name, "target module not found");
      if (process) CloseHandle(process);
      return;
    }
    if (!process)
      process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION |
                                SYNCHRONIZE,
                            FALSE, pid);
    if (!process) {
      session.nextDiscoveryAt = steadyNow + Ms(runtime.discoveryRetryMinMs == 0
                                                   ? 250
                                                   : runtime.discoveryRetryMinMs);
      SetUnavailable(endpointConfig.name, "failed to open target process");
      return;
    }
    const bool reused = session.pid == pid && session.creationTime != 0 &&
                        session.creationTime == ProcessCreationTime(process);
    session.process = process;
    session.pid = pid;
    session.base = base;
    session.size = size;
    session.creationTime = ProcessCreationTime(process);
    session.discoveryDelayMs = 0;
    session.nextDiscoveryAt = {};
    session.nextResolveAt = {};

    auto fresh = CopyEndpoint(endpointConfig.name);
    fresh.pid = pid;
    if (!reused) {
      fresh.lastOffset = 0;
      fresh.lastScanAt = 0;
    }
    fresh.available = false;
    fresh.usingFallback = false;
    fresh.directRead = endpointConfig.directRead;
    fresh.health = "initializing";
    fresh.lastError.clear();
    StoreEndpoint(fresh);
  }

  auto endpoint = CopyEndpoint(endpointConfig.name);
  const auto now = NowMs();
  if (endpoint.lastOffset && session.signatureAddress != 0 &&
      steadyNow >= session.nextSignatureAt) {
    std::vector<std::uint8_t> current(session.signatureBytes.size());
    SIZE_T signatureRead = 0;
    bool valid = ReadProcessMemory(
                     static_cast<HANDLE>(session.process),
                     reinterpret_cast<const void*>(session.signatureAddress),
                     current.data(), current.size(), &signatureRead) &&
                 signatureRead == current.size();
    for (std::size_t i = 0; valid && i < current.size(); ++i) {
      if (session.signatureMask[i] && current[i] != session.signatureBytes[i])
        valid = false;
    }
    session.nextSignatureAt = steadyNow + Ms(30000);
    if (!valid) {
      endpoint.lastOffset = 0;
      endpoint.lastError = "cached pattern signature changed";
      session.nextResolveAt = steadyNow;
      session.signatureAddress = 0;
      session.signatureBytes.clear();
      session.signatureMask.clear();
    }
  }
  bool attemptedResolve = false;
  if (!endpoint.lastOffset && steadyNow >= session.nextResolveAt) {
    ResolveOffset(session.process, session.pid, session.base, session.size,
                  runtime, endpointConfig, &endpoint);
    attemptedResolve = true;
  }
  const auto code = endpoint.lastOffset
                        ? ReadCode(session.process, session.base, endpointConfig,
                                   endpoint)
                        : "";
  endpoint.pid = session.pid;
  endpoint.available = !code.empty();
  if (!code.empty()) {
    endpoint.code = code;
    endpoint.health = endpoint.usingFallback ? "degraded" : "ready";
    endpoint.lastError.clear();
    endpoint.updatedAt = now;
    session.nextResolveAt = {};
    session.resolveDelayMs = 0;
    session.readFailures = 0;
    session.firstReadFailureAt = {};
  } else {
    const bool cachedReadFailed = endpoint.lastOffset != 0;
    if (cachedReadFailed) {
      if (session.readFailures++ == 0) session.firstReadFailureAt = steadyNow;
      const auto failureAge = std::chrono::duration_cast<Ms>(
          steadyNow - session.firstReadFailureAt);
      if (session.readFailures < (runtime.readFailureThreshold == 0
                                      ? 3
                                      : runtime.readFailureThreshold) &&
          failureAge < Ms(1000)) {
        endpoint.health = "degraded";
        endpoint.available = false;
        endpoint.lastError = "transient stock-code read failure";
        StoreEndpoint(endpoint);
        return;
      }
      endpoint.lastOffset = 0;
    }
    endpoint.health = "unavailable";
    if (endpoint.lastError.empty())
      endpoint.lastError = "failed to read current stock code";
    if (attemptedResolve || cachedReadFailed) {
      session.resolveDelayMs = session.resolveDelayMs == 0
          ? scanRetry
          : (std::min)(session.resolveDelayMs * 2,
                       runtime.scanRetryMaxMs == 0 ? 60000u
                                                   : runtime.scanRetryMaxMs);
      session.nextResolveAt = steadyNow + Ms(session.resolveDelayMs);
    }
  }
  StoreEndpoint(endpoint);
}

void Runtime::ResolveOffset(void* rawProcess, std::uint32_t pid,
                            std::uintptr_t base, std::uint32_t size,
                            const RuntimeConfig& runtime,
                            const EndpointConfig& config,
                            EndpointState* endpoint) {
  endpoint->lastScanAt = NowMs();
  endpoint->pid = pid;
  endpoint->directRead = config.directRead;
  const auto process = static_cast<HANDLE>(rawProcess);
  const auto tokens = Split(config.pattern, ' ');
  std::vector<int> pattern;
  for (const auto& token : tokens) {
    if (token == "?" || token == "??") {
      pattern.push_back(-1);
    } else {
      char* end = nullptr;
      const auto value = std::strtoul(token.c_str(), &end, 16);
      if (!end || *end) {
        pattern.clear();
        break;
      }
      pattern.push_back(static_cast<int>(value));
    }
  }
  const auto deadline = Clock::now() +
                        Ms(runtime.scanTimeoutMs == 0 ? 2500
                                                      : runtime.scanTimeoutMs);
  const auto scanBlock = [&](std::uintptr_t blockBase,
                             const std::vector<std::uint8_t>& data) {
    for (std::size_t at = 0; !pattern.empty() &&
                             at + pattern.size() <= data.size() &&
                             Clock::now() < deadline; ++at) {
      bool match = true;
      for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] >= 0 && data[at + i] != pattern[i]) {
          match = false;
          break;
        }
      }
      if (!match) continue;
      const auto immediate = at + (config.name == "ths" ? 2 : 1);
      if (immediate + 4 <= data.size()) {
        std::uint32_t address = 0;
        std::memcpy(&address, data.data() + immediate, 4);
        if (address < static_cast<std::uint32_t>(base) ||
            address - static_cast<std::uint32_t>(base) >= size)
          continue;
        EndpointState candidate = *endpoint;
        candidate.lastOffset = address - static_cast<std::uint32_t>(base);
        candidate.directRead = config.name == "tdx";
        candidate.usingFallback = false;
        if (!ReadCode(rawProcess, base, config, candidate).empty()) {
          *endpoint = candidate;
          endpoint->lastError.clear();
          auto& session = Session(config.name);
          session.signatureAddress = blockBase + at;
          session.signatureBytes.assign(data.begin() + at,
                                        data.begin() + at + pattern.size());
          session.signatureMask.clear();
          for (const auto value : pattern)
            session.signatureMask.push_back(value >= 0);
          session.nextSignatureAt = Clock::now() + Ms(30000);
          return true;
        }
      }
    }
    return false;
  };

  std::vector<std::uint8_t> headers(16384);
  SIZE_T headerRead = 0;
  if (!pattern.empty() &&
      ReadProcessMemory(process, reinterpret_cast<const void*>(base),
                        headers.data(), headers.size(), &headerRead)) {
    headers.resize(headerRead);
    if (headers.size() >= sizeof(IMAGE_DOS_HEADER)) {
      const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(headers.data());
      const auto ntAt = static_cast<std::size_t>(dos->e_lfanew);
      if (dos->e_magic == IMAGE_DOS_SIGNATURE &&
          ntAt + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) <= headers.size()) {
        const auto signature = *reinterpret_cast<const DWORD*>(headers.data() + ntAt);
        const auto* file = reinterpret_cast<const IMAGE_FILE_HEADER*>(
            headers.data() + ntAt + sizeof(DWORD));
        const auto sectionAt = ntAt + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
                               file->SizeOfOptionalHeader;
        if (signature == IMAGE_NT_SIGNATURE &&
            sectionAt + file->NumberOfSections * sizeof(IMAGE_SECTION_HEADER) <=
                headers.size()) {
          const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
              headers.data() + sectionAt);
          constexpr std::size_t kChunk = 1024 * 1024;
          for (WORD i = 0; i < file->NumberOfSections && Clock::now() < deadline;
               ++i) {
            if ((sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
              continue;
            const auto sectionRva = sections[i].VirtualAddress;
            const auto virtualSize =
                static_cast<std::uint32_t>(sections[i].Misc.VirtualSize);
            const auto availableSize = static_cast<std::uint32_t>(
                size > sectionRva ? size - sectionRva : 0u);
            const auto sectionSize = (std::min)(virtualSize, availableSize);
            for (std::size_t offset = 0; offset < sectionSize &&
                                         Clock::now() < deadline;) {
              const auto wanted = (std::min)(kChunk + pattern.size(),
                                              sectionSize - offset);
              std::vector<std::uint8_t> data(wanted);
              SIZE_T read = 0;
              if (ReadProcessMemory(
                      process,
                      reinterpret_cast<const void*>(base + sectionRva + offset),
                      data.data(), data.size(), &read)) {
                data.resize(read);
                if (scanBlock(base + sectionRva + offset, data)) return;
              }
              offset += (std::min)(kChunk, sectionSize - offset);
            }
          }
        }
      }
    }
  }
  for (const auto fallback : config.fallbacks) {
    EndpointState candidate = *endpoint;
    candidate.lastOffset = fallback;
    candidate.directRead = config.directRead;
    candidate.usingFallback = true;
    if (!ReadCode(rawProcess, base, config, candidate).empty()) {
      *endpoint = candidate;
      endpoint->lastError = "dynamic scan failed, using validated fallback offset";
      auto& session = Session(config.name);
      session.signatureAddress = 0;
      session.signatureBytes.clear();
      session.signatureMask.clear();
      return;
    }
  }
  endpoint->lastOffset = 0;
  endpoint->lastError = "dynamic scan and fallback validation failed";
  auto& session = Session(config.name);
  session.signatureAddress = 0;
  session.signatureBytes.clear();
  session.signatureMask.clear();
}

std::string Runtime::ReadCode(void* rawProcess, std::uintptr_t base,
                              const EndpointConfig& config,
                              const EndpointState& endpoint) {
  const auto process = static_cast<HANDLE>(rawProcess);
  char buffer[16]{};
  SIZE_T read = 0;
  if (config.name == "tdx" && endpoint.directRead) {
    return ReadProcessMemory(
               process,
               reinterpret_cast<const void*>(base + endpoint.lastOffset),
               buffer, sizeof(buffer), &read)
               ? Digits(buffer, read, true)
               : "";
  }
  std::uint32_t pointer = 0;
  if (!ReadProcessMemory(
          process,
          reinterpret_cast<const void*>(base + endpoint.lastOffset), &pointer,
          sizeof(pointer), &read) ||
      !pointer) {
    return "";
  }
  if (!ReadProcessMemory(
          process,
          reinterpret_cast<const void*>(static_cast<std::uintptr_t>(pointer)),
          buffer, sizeof(buffer), &read)) {
    return "";
  }
  return Digits(buffer, read, config.name == "tdx");
}

void* Runtime::FindMainWindow(std::uint32_t pid) {
  WindowSearch search{pid, {}};
  EnumWindows(FindWindowCallback, reinterpret_cast<LPARAM>(&search));
  return search.windows.size() == 1 ? search.windows.front() : nullptr;
}

bool Runtime::ExecuteThs(std::uint32_t timeoutMs, const std::string& code,
                         std::string* error, std::uint32_t* win32Error) {
  const auto pid = FindProcess({"hexin.exe"});
  if (!pid) {
    *error = "ths process not found for command dispatch";
    return false;
  }
  if (deferredAllocations_.size() >= 16) {
    *error = "ths deferred command cleanup queue is full";
    return false;
  }
  const auto process = OpenProcess(
      PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
          PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
      FALSE, pid);
  if (!process) {
    *win32Error = GetLastError();
    *error = "failed to open ths process for command dispatch";
    return false;
  }
  const unsigned char prefix =
      code[0] == '6' ? 0x11
                     : ((code[0] == '8' || code[0] == '4' || code[0] == '9')
                            ? 0x97
                            : 0x21);
  std::vector<unsigned char> payload{prefix};
  payload.insert(payload.end(), code.begin(), code.end());
  payload.push_back(0);
  const auto remote = VirtualAllocEx(process, nullptr, payload.size(),
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  SIZE_T written = 0;
  if (!remote ||
      !WriteProcessMemory(process, remote, payload.data(), payload.size(),
                          &written)) {
    *win32Error = GetLastError();
    *error = "failed to allocate or write ths command";
    if (remote) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);
    return false;
  } else {
    DWORD_PTR ignored = 0;
    const auto window = static_cast<HWND>(FindMainWindow(pid));
    if (!window) {
      *error = "ths compatible top-level window is missing or ambiguous";
      VirtualFreeEx(process, remote, 0, MEM_RELEASE);
      CloseHandle(process);
      return false;
    }
    if (!SendMessageTimeoutA(window, 1168, 0, reinterpret_cast<LPARAM>(remote),
                             SMTO_ABORTIFHUNG, timeoutMs, &ignored)) {
      *win32Error = GetLastError();
      *error = "ths command message timed out; cleanup deferred";
      deferredAllocations_.push_back(
          {process, remote, Clock::now() + Ms(30000)});
      return false;
    }
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
  }
  CloseHandle(process);
  return true;
}

bool Runtime::ExecuteTdx(const std::string& code, std::string* error,
                         std::uint32_t* win32Error) {
  const std::string value =
      (code[0] == '6' ? "7" : ((code[0] == '0' || code[0] == '3') ? "6" : "4")) +
      code;
  const auto message = RegisterWindowMessageA("stock");
  try {
    if (!message ||
        !PostMessageA(HWND_BROADCAST, message,
                      static_cast<WPARAM>(std::stoul(value)), 0)) {
      *win32Error = GetLastError();
      *error = "failed to dispatch tdx command";
      return false;
    }
  } catch (...) {
    *error = "invalid tdx command code";
    return false;
  }
  return true;
}

EndpointState Runtime::CopyEndpoint(const std::string& name) {
  std::lock_guard lock(mutex_);
  return name == "ths" ? state_.ths : state_.tdx;
}

void Runtime::StoreEndpoint(const EndpointState& endpoint) {
  std::lock_guard lock(mutex_);
  (endpoint.name == "ths" ? state_.ths : state_.tdx) = endpoint;
}

void Runtime::SetUnavailable(const std::string& name,
                             const std::string& error) {
  auto value = CopyEndpoint(name);
  value.available = false;
  value.health = "unavailable";
  value.lastError = error;
  value.pid = 0;
  StoreEndpoint(value);
}

void Runtime::RecomputeHealth() {
  std::lock_guard lock(mutex_);
  state_.active = true;
  const bool thsReady = state_.ths.health == "ready";
  const bool tdxReady = state_.tdx.health == "ready";
  const bool degraded = state_.ths.health == "degraded" ||
                        state_.tdx.health == "degraded" ||
                        state_.ths.available || state_.tdx.available;
  const bool fallback = state_.ths.usingFallback || state_.tdx.usingFallback;
  state_.runtimeHealth = thsReady && tdxReady && !fallback
                             ? "ready"
                             : (degraded ? "degraded" : "unavailable");
}

std::string Runtime::CommandKey(const std::string& clientId,
                                const std::string& requestId) const {
  return std::to_string(clientId.size()) + ":" + clientId + requestId;
}

void Runtime::StoreCommandStatus(const CommandStatus& status) {
  const auto key = CommandKey(status.clientId, status.requestId);
  if (commandHistory_.find(key) == commandHistory_.end()) commandOrder_.push_back(key);
  commandHistory_[key] = status;
  const auto capacity = config_.dedupeCapacity == 0 ? 256 : config_.dedupeCapacity;
  while (commandOrder_.size() > capacity) {
    commandHistory_.erase(commandOrder_.front());
    commandOrder_.pop_front();
  }
  {
    std::lock_guard lock(mutex_);
    state_.lastCommand = status;
  }
}

std::optional<CommandStatus> Runtime::CommandState(
    const std::string& clientId, const std::string& requestId) {
  std::lock_guard lock(queueMutex_);
  const auto found = commandHistory_.find(CommandKey(clientId, requestId));
  return found == commandHistory_.end()
             ? std::optional<CommandStatus>{}
             : std::optional<CommandStatus>{found->second};
}

void Runtime::CompleteConfirmations(TimePoint now,
                                    const RuntimeConfig& config) {
  std::lock_guard lock(queueMutex_);
  for (auto at = pendingConfirmations_.begin();
       at != pendingConfirmations_.end();) {
    const auto endpoint = CopyEndpoint(at->target);
    if (endpoint.available && endpoint.code == at->code) {
      at->state = "confirmed";
      at->completedAt = NowMs();
      at->message = "target code confirmed by readback";
      StoreCommandStatus(*at);
      at = pendingConfirmations_.erase(at);
      continue;
    }
    const auto timeout = config.commandConfirmMs == 0 ? 10000
                                                       : config.commandConfirmMs;
    if (at->dispatchedAt != 0 && NowMs() - at->dispatchedAt >= timeout) {
      at->state = "timed_out";
      at->errorStage = "confirmation";
      at->completedAt = NowMs();
      at->message = "target code confirmation timed out";
      StoreCommandStatus(*at);
      at = pendingConfirmations_.erase(at);
      continue;
    }
    ++at;
  }
}

void Runtime::CleanupDeferredAllocations(TimePoint now) {
  for (auto at = deferredAllocations_.begin(); at != deferredAllocations_.end();) {
    const auto process = static_cast<HANDLE>(at->process);
    if (now < at->releaseAt &&
        WaitForSingleObject(process, 0) == WAIT_TIMEOUT) {
      ++at;
      continue;
    }
    VirtualFreeEx(process, at->address, 0, MEM_RELEASE);
    CloseHandle(process);
    at = deferredAllocations_.erase(at);
  }
}

void Runtime::UpdateLifecycle(TimePoint now, const RuntimeConfig& config,
                              bool hasPendingCommands) {
  std::string next = "cold";
  TimePoint last;
  TimePoint activeUntil;
  {
    std::lock_guard lock(queueMutex_);
    last = lastBusinessActivity_;
    activeUntil = activeUntil_;
  }
  if (hasPendingCommands || now < activeUntil) {
    next = "hot";
  } else if (last != TimePoint{}) {
    const auto elapsed = std::chrono::duration_cast<Ms>(now - last).count();
    if (elapsed < (config.warmHandleMs == 0 ? 300000
                                            : config.warmHandleMs))
      next = "warm";
    else if (elapsed < (config.coldCacheMs == 0 ? 600000
                                                : config.coldCacheMs))
      next = "cool";
  }

  std::string previous;
  {
    std::lock_guard lock(mutex_);
    previous = state_.activityState;
    state_.activityState = next;
    state_.active = next == "hot";
    if (next != "hot") {
      state_.stale = true;
      state_.runtimeHealth = "idle";
    }
  }
  if (next == "cool" && previous != "cool") {
    CloseSessionHandle(&thsSession_);
    CloseSessionHandle(&tdxSession_);
  } else if (next == "cold" && previous != "cold") {
    ResetSessions();
    std::lock_guard lock(mutex_);
    for (auto* endpoint : {&state_.ths, &state_.tdx}) {
      endpoint->pid = 0;
      endpoint->lastOffset = 0;
      endpoint->available = false;
      endpoint->health = "idle";
    }
  }
}

}  // namespace onclaw_tdx_ths

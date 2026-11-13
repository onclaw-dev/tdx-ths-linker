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
           return std::isdigit(ch) != 0;
         });
}

std::string Digits(const char* data, std::size_t size, bool fromEnd) {
  std::string result;
  for (std::size_t i = 0; i < size; ++i) {
    if (std::isdigit(static_cast<unsigned char>(data[i])) != 0) {
      result.push_back(data[i]);
    }
  }
  if (result.size() <= 6) return result;
  return fromEnd ? result.substr(result.size() - 6) : result.substr(0, 6);
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
  HWND window;
};

BOOL CALLBACK FindWindowCallback(HWND window, LPARAM value) {
  auto* search = reinterpret_cast<WindowSearch*>(value);
  DWORD pid = 0;
  GetWindowThreadProcessId(window, &pid);
  if (pid == search->pid && IsWindowVisible(window)) {
    search->window = window;
    return FALSE;
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
    activeUntil_ = {};
  }
  {
    std::lock_guard lock(mutex_);
    state_ = RuntimeState{};
    state_.licenseState = licenseAccepted ? "accepted" : "unaccepted";
    state_.initialized = licenseAccepted;
    state_.runtimeHealth = licenseAccepted ? "initializing" : "unavailable";
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
  TouchActivity();
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
      requestId.empty() ? std::string{} : clientId + ":" + requestId;
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
      if (code == lane.leadingCode)
        lane.trailing.reset();
      else
        lane.trailing = command;
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
    state_.lastActivityAt = NowMs();
    if (state_.runtimeHealth == "idle") state_.runtimeHealth = "initializing";
  }
  {
    std::lock_guard lock(queueMutex_);
    activeUntil_ = Clock::now() +
                   Ms(config.idleTimeoutMs == 0 ? 5000 : config.idleTimeoutMs);
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
  if (!running_.compare_exchange_strong(expected, false)) return;
  condition_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void Runtime::Loop() {
  while (running_) {
    RuntimeConfig config;
    {
      std::lock_guard lock(mutex_);
      config = config_;
    }
    bool active = false;
    {
      std::lock_guard lock(queueMutex_);
      const auto now = Clock::now();
      PromoteDueLocked(&thsCommands_, now);
      PromoteDueLocked(&tdxCommands_, now);
      active = now < activeUntil_ || thsCommands_.ready || tdxCommands_.ready;
    }
    if (!active) {
      {
        std::lock_guard lock(mutex_);
        state_.active = false;
        if (state_.initialized) state_.runtimeHealth = "idle";
      }
      std::unique_lock lock(queueMutex_);
      condition_.wait(lock, [this] {
        return !running_ || Clock::now() < activeUntil_ ||
               thsCommands_.ready.has_value() || tdxCommands_.ready.has_value();
      });
      continue;
    }
    ProcessCommands(config);
    Poll(config, config.ths);
    Poll(config, config.tdx);
    RecomputeHealth();
    std::unique_lock lock(queueMutex_);
    auto wait = Ms(config.pollMs == 0 ? 350 : config.pollMs);
    const auto now = Clock::now();
    for (const auto* lane : {&thsCommands_, &tdxCommands_}) {
      if (lane->windowActive && lane->deadline > now) {
        wait = (std::min)(wait,
                          std::chrono::duration_cast<Ms>(lane->deadline - now));
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
    if (config.commandObserver) {
      config.commandObserver(command.target, command.code);
    } else if (command.target == "ths") {
      ExecuteThs(config.commandTimeoutMs, command.code);
    } else {
      ExecuteTdx(command.code);
    }
  }
}

void Runtime::Poll(const RuntimeConfig& runtime,
                   const EndpointConfig& endpointConfig) {
  const auto pid = FindProcess(endpointConfig.processes);
  if (!pid) {
    SetUnavailable(endpointConfig.name, "target process not found");
    return;
  }
  DWORD size = 0;
  const auto base = FindModule(pid, endpointConfig.moduleName, &size);
  if (!base || !size) {
    SetUnavailable(endpointConfig.name, "target module not found");
    return;
  }
  const auto process =
      OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (!process) {
    SetUnavailable(endpointConfig.name, "failed to open target process");
    return;
  }
  auto endpoint = CopyEndpoint(endpointConfig.name);
  const auto now = NowMs();
  if (endpoint.pid != pid) endpoint.lastOffset = 0;
  if (!endpoint.lastOffset || now - endpoint.lastScanAt >= runtime.scanRetryMs) {
    ResolveOffset(process, pid, base, size, runtime, endpointConfig, &endpoint);
  }
  const auto code = endpoint.lastOffset
                        ? ReadCode(process, base, endpointConfig, endpoint)
                        : "";
  endpoint.pid = pid;
  endpoint.available = !code.empty();
  if (!code.empty()) {
    endpoint.code = code;
    endpoint.health = endpoint.usingFallback ? "degraded" : "ready";
    endpoint.lastError.clear();
    endpoint.updatedAt = now;
  } else if (endpoint.lastError.empty()) {
    endpoint.health = "unavailable";
    endpoint.lastError = "failed to read current stock code";
  }
  StoreEndpoint(endpoint);
  CloseHandle(process);
}

void Runtime::ResolveOffset(void* rawProcess, std::uint32_t pid,
                            std::uintptr_t base, std::uint32_t size,
                            const RuntimeConfig& runtime,
                            const EndpointConfig& config,
                            EndpointState* endpoint) {
  endpoint->lastScanAt = NowMs();
  endpoint->pid = pid;
  endpoint->directRead = config.directRead;
  std::vector<std::uint8_t> data(size);
  SIZE_T read = 0;
  const auto process = static_cast<HANDLE>(rawProcess);
  if (ReadProcessMemory(process, reinterpret_cast<const void*>(base),
                        data.data(), size, &read)) {
    data.resize(read);
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
    const auto deadline = Clock::now() + Ms(runtime.scanTimeoutMs);
    for (std::size_t at = 0;
         !pattern.empty() && at + pattern.size() <= data.size() &&
         Clock::now() < deadline;
         ++at) {
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
        endpoint->lastOffset = address - static_cast<std::uint32_t>(base);
        endpoint->directRead = config.name == "tdx";
        endpoint->usingFallback = false;
        endpoint->lastError.clear();
        return;
      }
    }
  }
  if (!config.fallbacks.empty()) {
    endpoint->lastOffset = config.fallbacks.front();
    endpoint->usingFallback = true;
    endpoint->lastError = "dynamic scan failed, using fallback offset";
  } else {
    endpoint->lastOffset = 0;
    endpoint->lastError = "dynamic scan failed";
  }
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
  WindowSearch search{pid, nullptr};
  EnumWindows(FindWindowCallback, reinterpret_cast<LPARAM>(&search));
  return search.window;
}

void Runtime::ExecuteThs(std::uint32_t timeoutMs, const std::string& code) {
  const auto pid = FindProcess({"hexin.exe"});
  if (!pid) {
    SetError("ths", "ths process not found for command dispatch");
    return;
  }
  const auto process = OpenProcess(
      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
      FALSE, pid);
  if (!process) {
    SetError("ths", "failed to open ths process for command dispatch");
    return;
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
    SetError("ths", "failed to allocate or write ths command");
  } else {
    DWORD_PTR ignored = 0;
    const auto window = static_cast<HWND>(FindMainWindow(pid));
    if (!window ||
        !SendMessageTimeoutA(window, 1168, 0,
                             reinterpret_cast<LPARAM>(remote),
                             SMTO_ABORTIFHUNG, timeoutMs, &ignored)) {
      SetError("ths", "ths command timed out");
    }
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
  }
  CloseHandle(process);
}

void Runtime::ExecuteTdx(const std::string& code) {
  const std::string value =
      (code[0] == '6' ? "7" : ((code[0] == '0' || code[0] == '3') ? "6" : "4")) +
      code;
  const auto message = RegisterWindowMessageA("stock");
  try {
    if (!message ||
        !PostMessageA(HWND_BROADCAST, message,
                      static_cast<WPARAM>(std::stoul(value)), 0)) {
      SetError("tdx", "failed to dispatch tdx command");
    }
  } catch (...) {
    SetError("tdx", "invalid tdx command code");
  }
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

void Runtime::SetError(const std::string& name, const std::string& error) {
  auto value = CopyEndpoint(name);
  value.lastError = error;
  if (value.health == "ready") value.health = "degraded";
  StoreEndpoint(value);
}

void Runtime::RecomputeHealth() {
  std::lock_guard lock(mutex_);
  state_.active = true;
  const bool ready = state_.ths.health == "ready" || state_.tdx.health == "ready";
  const bool fallback = state_.ths.usingFallback || state_.tdx.usingFallback;
  state_.runtimeHealth = ready ? (fallback ? "degraded" : "ready") : "degraded";
}

}  // namespace onclaw_tdx_ths

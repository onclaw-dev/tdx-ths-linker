#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj_core.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <future>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "linker_runtime.h"

#ifndef TDX_THS_ENABLE_THS_WRITE
#define TDX_THS_ENABLE_THS_WRITE 0
#endif
#ifndef TDX_THS_AGENT_CHANNEL
#define TDX_THS_AGENT_CHANNEL "stable"
#endif
#ifndef TDX_THS_LINKER_VERSION
#define TDX_THS_LINKER_VERSION "0.1.0"
#endif

namespace {
constexpr wchar_t kWindowClass[] = L"TdxThsLinkerAgentWindow";
constexpr wchar_t kMutexName[] = L"Local\\TdxThsLinkerAgent";
constexpr wchar_t kProtocol[] = L"tdx-ths-linker";
constexpr unsigned short kPort = 17654;
constexpr int kMaxRequestBytes = 32768;
constexpr UINT kActivateMessage = WM_APP + 1;
constexpr bool kThsWriteEnabled = TDX_THS_ENABLE_THS_WRITE != 0;
constexpr char kAgentChannel[] = TDX_THS_AGENT_CHANNEL;
constexpr char kAgentVersion[] = TDX_THS_LINKER_VERSION;

std::atomic<bool> running{true};
std::mutex activationMutex;
std::string pendingNonce;
std::string allowedOrigin;
std::string sessionToken;
std::chrono::steady_clock::time_point sessionExpires;
tdx_ths_linker::Runtime runtime;

std::string Utf8(const std::wstring& value) {
  if (value.empty()) return {};
  const auto size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                        static_cast<int>(value.size()), nullptr,
                                        0, nullptr, nullptr);
  std::string result(size, 0);
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

std::string Trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
    value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
    value.pop_back();
  return value;
}

std::string UrlDecode(const std::string& value) {
  std::string result;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      char hex[3]{value[i + 1], value[i + 2], 0};
      char* end = nullptr;
      const auto parsed = std::strtoul(hex, &end, 16);
      if (end && !*end) {
        result.push_back(static_cast<char>(parsed));
        i += 2;
        continue;
      }
    }
    result.push_back(value[i] == '+' ? ' ' : value[i]);
  }
  return result;
}

bool SafeNonce(const std::string& value) {
  return value.size() >= 32 && value.size() <= 160 &&
         std::all_of(value.begin(), value.end(), [](unsigned char ch) {
           return std::isalnum(ch) || ch == '-' || ch == '_';
         });
}

bool SafeOrigin(const std::string& value) {
  if (value.size() < 9 || value.size() > 512 ||
      value.find_first_of("/?#@ \t\r\n", 8) != std::string::npos)
    return false;
  const auto validPort = [](const std::string& authority,
                            const std::string& host) {
    if (authority == host) return true;
    if (authority.rfind(host + ":", 0) != 0) return false;
    const auto port = authority.substr(host.size() + 1);
    return !port.empty() &&
           std::all_of(port.begin(), port.end(), [](unsigned char ch) {
             return std::isdigit(ch) != 0;
           });
  };
  if (value.rfind("https://", 0) == 0) return value.size() > 8;
  if (value.rfind("http://", 0) != 0) return false;
  const auto authority = value.substr(7);
  return validPort(authority, "localhost") ||
         validPort(authority, "127.0.0.1");
}

bool Activate(const std::string& uri) {
  constexpr char prefix[] = "tdx-ths-linker://activate?";
  constexpr char slashPrefix[] = "tdx-ths-linker://activate/?";
  if (uri.size() > 2048) return false;
  std::size_t queryAt = 0;
  if (uri.rfind(prefix, 0) == 0)
    queryAt = sizeof(prefix) - 1;
  else if (uri.rfind(slashPrefix, 0) == 0)
    queryAt = sizeof(slashPrefix) - 1;
  else
    return false;

  std::string version = "1", nonce, origin;
  bool sawVersion = false, sawNonce = false, sawOrigin = false;
  std::stringstream query(uri.substr(queryAt));
  std::string part;
  while (std::getline(query, part, '&')) {
    const auto equals = part.find('=');
    if (equals == std::string::npos || equals == 0) return false;
    const auto key = part.substr(0, equals);
    const auto value = UrlDecode(part.substr(equals + 1));
    if (key == "v" && !sawVersion) {
      version = value;
      sawVersion = true;
    } else if (key == "nonce" && !sawNonce) {
      nonce = value;
      sawNonce = true;
    } else if (key == "origin" && !sawOrigin) {
      origin = value;
      sawOrigin = true;
    } else {
      return false;
    }
  }
  if (version != "1" || !SafeNonce(nonce) || !SafeOrigin(origin)) return false;
  std::lock_guard lock(activationMutex);
  pendingNonce = nonce;
  allowedOrigin = origin;
  sessionToken.clear();
  return true;
}

std::string RandomToken() {
  unsigned char bytes[32]{};
  if (BCryptGenRandom(nullptr, bytes, sizeof(bytes),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
    return {};
  static constexpr char alphabet[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (const auto byte : bytes) {
    result.push_back(alphabet[byte >> 4]);
    result.push_back(alphabet[byte & 15]);
  }
  return result;
}

std::string JsonEscape(const std::string& value) {
  std::string result;
  for (const char ch : value) {
    if (ch == '"' || ch == '\\') result.push_back('\\');
    if (static_cast<unsigned char>(ch) >= 0x20) result.push_back(ch);
  }
  return result;
}

std::string JsonString(const std::string& body, const std::string& key) {
  const auto marker = "\"" + key + "\"";
  auto at = body.find(marker);
  if (at == std::string::npos) return {};
  at = body.find(':', at + marker.size());
  if (at == std::string::npos) return {};
  at = body.find('"', at + 1);
  if (at == std::string::npos) return {};
  std::string result;
  for (++at; at < body.size(); ++at) {
    if (body[at] == '"') return result;
    if (body[at] == '\\' && at + 1 < body.size()) ++at;
    result.push_back(body[at]);
  }
  return {};
}

bool JsonBool(const std::string& body, const std::string& key) {
  const auto marker = "\"" + key + "\"";
  const auto field = body.find(marker);
  if (field == std::string::npos) return false;
  const auto colon = body.find(':', field + marker.size());
  if (colon == std::string::npos) return false;
  const auto value = body.find_first_not_of(" \t\r\n", colon + 1);
  return value != std::string::npos && body.compare(value, 4, "true") == 0;
}

struct Request {
  std::string method;
  std::string path;
  std::string host;
  std::string origin;
  std::string authorization;
  std::string body;
};

bool ParseRequest(const std::string& raw, Request* request) {
  const auto headerEnd = raw.find("\r\n\r\n");
  if (headerEnd == std::string::npos) return false;
  std::istringstream stream(raw.substr(0, headerEnd));
  std::string line;
  if (!std::getline(stream, line)) return false;
  if (!line.empty() && line.back() == '\r') line.pop_back();
  std::istringstream first(line);
  std::string version;
  if (!(first >> request->method >> request->path >> version) ||
      version.rfind("HTTP/1.", 0) != 0)
    return false;
  std::size_t contentLength = 0;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto separator = line.find(':');
    if (separator == std::string::npos) continue;
    const auto key = Lower(Trim(line.substr(0, separator)));
    const auto value = Trim(line.substr(separator + 1));
    if (key == "host") request->host = value;
    else if (key == "origin") request->origin = value;
    else if (key == "authorization") request->authorization = value;
    else if (key == "content-length")
      contentLength = std::strtoul(value.c_str(), nullptr, 10);
  }
  if (contentLength > 16384 || headerEnd + 4 + contentLength > raw.size())
    return false;
  request->body = raw.substr(headerEnd + 4, contentLength);
  return true;
}

std::string EndpointJson(const tdx_ths_linker::EndpointState& value) {
  return "{\"name\":\"" + JsonEscape(value.name) + "\",\"code\":\"" +
         JsonEscape(value.code) + "\",\"health\":\"" +
         JsonEscape(value.health) + "\",\"lastError\":\"" +
         JsonEscape(value.lastError) + "\",\"updatedAt\":" +
         std::to_string(value.updatedAt) + ",\"lastScanAt\":" +
         std::to_string(value.lastScanAt) + ",\"lastResolvedOffset\":" +
         std::to_string(value.lastOffset) + ",\"available\":" +
         (value.available ? "true" : "false") + ",\"usingFallback\":" +
         (value.usingFallback ? "true" : "false") +
         ",\"directRead\":" + (value.directRead ? "true" : "false") +
         ",\"pid\":" + std::to_string(value.pid) + "}";
}

std::string StateJson() {
  const auto value = runtime.State();
  return "{\"initialized\":" + std::string(value.initialized ? "true" : "false") +
         ",\"isSupported\":true,\"runtimeHealth\":\"" +
         JsonEscape(value.runtimeHealth) + "\",\"licenseState\":\"" +
         JsonEscape(value.licenseState) + "\",\"message\":\"" +
         JsonEscape(value.message) + "\",\"ths\":" + EndpointJson(value.ths) +
         ",\"tdx\":" + EndpointJson(value.tdx) + "}";
}

std::string Response(int status, const std::string& body,
                     const std::string& origin = {}) {
  const char* text = status == 200   ? "OK"
                     : status == 204 ? "No Content"
                     : status == 400 ? "Bad Request"
                     : status == 401 ? "Unauthorized"
                     : status == 403 ? "Forbidden"
                                     : "Not Found";
  std::string headers =
      "HTTP/1.1 " + std::to_string(status) + " " + text +
      "\r\nContent-Type: application/json; charset=utf-8\r\n"
      "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
      std::to_string(body.size()) + "\r\n";
  if (!origin.empty()) {
    headers += "Access-Control-Allow-Origin: " + origin +
               "\r\nVary: Origin\r\nAccess-Control-Allow-Headers: Authorization, "
               "Content-Type\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Private-Network: true\r\n"
               "Access-Control-Max-Age: 300\r\n";
  }
  return headers + "\r\n" + body;
}

bool Authorized(const Request& request) {
  std::lock_guard lock(activationMutex);
  return !sessionToken.empty() &&
         std::chrono::steady_clock::now() < sessionExpires &&
         request.authorization == "Bearer " + sessionToken;
}

std::string Handle(const Request& request) {
  if (request.host != "127.0.0.1:17654")
    return Response(403, "{\"error\":\"invalid_host\"}");
  if (!SafeOrigin(request.origin))
    return Response(403, "{\"error\":\"invalid_origin\"}");
  if (request.method == "OPTIONS") return Response(204, "", request.origin);
  if (request.method == "GET" && request.path == "/v1/health") {
    return Response(200,
                    "{\"ok\":true,\"protocolVersion\":1,\"agentVersion\":\"" +
                        std::string(kAgentVersion) + "\",\"channel\":\"" +
                        std::string(kAgentChannel) +
                        "\",\"writableTargets\":[\"tdx\"" +
                        (kThsWriteEnabled ? ",\"ths\"" : "") +
                        "],\"paired\":" + (Authorized(request) ? "true" : "false") +
                        "}",
                    request.origin);
  }
  std::string origin;
  {
    std::lock_guard lock(activationMutex);
    origin = allowedOrigin;
  }
  if (request.origin != origin)
    return Response(403, "{\"error\":\"invalid_origin\"}", request.origin);
  if (request.method == "POST" && request.path == "/v1/session/claim") {
    const auto nonce = JsonString(request.body, "nonce");
    std::lock_guard lock(activationMutex);
    if (pendingNonce.empty() || nonce != pendingNonce)
      return Response(401, "{\"error\":\"invalid_or_consumed_nonce\"}", origin);
    pendingNonce.clear();
    sessionToken = RandomToken();
    sessionExpires = std::chrono::steady_clock::now() + std::chrono::minutes(10);
    return Response(200,
                    "{\"sessionToken\":\"" + sessionToken +
                        "\",\"expiresIn\":600,\"channel\":\"" +
                        std::string(kAgentChannel) +
                        "\",\"writableTargets\":[\"tdx\"" +
                        (kThsWriteEnabled ? ",\"ths\"" : "") + "]}",
                    origin);
  }
  if (!Authorized(request))
    return Response(401, "{\"error\":\"local_session_required\"}", origin);
  if (request.method == "GET" && request.path == "/v1/state")
    return Response(200, StateJson(), origin);
  if (request.method == "POST" && request.path == "/v1/initialize") {
    const auto value = runtime.Init(JsonBool(request.body, "licenseAccepted"),
                                    kThsWriteEnabled);
    return Response(200,
                    "{\"ok\":" + std::string(value.ok ? "true" : "false") +
                        ",\"licenseState\":\"" + JsonEscape(value.licenseState) +
                        "\",\"message\":\"" + JsonEscape(value.message) +
                        "\",\"runtimeHealth\":\"" +
                        JsonEscape(value.runtimeHealth) + "\"}",
                    origin);
  }
  if (request.method == "POST" && request.path == "/v1/set-code") {
    const auto value = runtime.Queue(JsonString(request.body, "target"),
                                     JsonString(request.body, "code"));
    return Response(200,
                    "{\"ok\":" + std::string(value.ok ? "true" : "false") +
                        ",\"accepted\":" + (value.accepted ? "true" : "false") +
                        ",\"status\":\"" + JsonEscape(value.status) +
                        "\",\"target\":\"" + JsonEscape(value.target) +
                        "\",\"code\":\"" + JsonEscape(value.code) +
                        "\",\"message\":\"" + JsonEscape(value.message) + "\"}",
                    origin);
  }
  return Response(404, "{\"error\":\"not_found\"}", origin);
}

void HttpServer(std::promise<int> startup) {
  WSADATA data{};
  const auto startupError = WSAStartup(MAKEWORD(2, 2), &data);
  if (startupError != 0) {
    startup.set_value(startupError);
    return;
  }
  const auto server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server == INVALID_SOCKET) {
    startup.set_value(WSAGetLastError());
    WSACleanup();
    return;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(kPort);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
          SOCKET_ERROR ||
      listen(server, SOMAXCONN) == SOCKET_ERROR) {
    const auto error = WSAGetLastError();
    startup.set_value(error);
    closesocket(server);
    WSACleanup();
    return;
  }
  startup.set_value(0);
  while (running) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(server, &set);
    timeval timeout{0, 200000};
    const auto selected = select(0, &set, nullptr, nullptr, &timeout);
    if (selected == SOCKET_ERROR) {
      running = false;
      const auto window = FindWindowW(kWindowClass, nullptr);
      if (window) PostMessageW(window, WM_CLOSE, 0, 0);
      break;
    }
    if (selected == 0) continue;
    const auto client = accept(server, nullptr, nullptr);
    if (client == INVALID_SOCKET) continue;
    std::string raw;
    char buffer[4096];
    int count = 0;
    while (raw.size() < kMaxRequestBytes &&
           (count = recv(client, buffer, sizeof(buffer), 0)) > 0) {
      raw.append(buffer, count);
      const auto headers = raw.find("\r\n\r\n");
      if (headers == std::string::npos) continue;
      const auto length = Lower(raw.substr(0, headers)).find("content-length:");
      if (length == std::string::npos ||
          raw.size() >= headers + 4 +
                            std::strtoul(raw.c_str() + length + 15, nullptr, 10))
        break;
    }
    Request request;
    const auto response = ParseRequest(raw, &request)
                              ? Handle(request)
                              : Response(400, "{\"error\":\"invalid_request\"}");
    send(client, response.data(), static_cast<int>(response.size()), 0);
    shutdown(client, SD_BOTH);
    closesocket(client);
  }
  closesocket(server);
  WSACleanup();
}

std::wstring ExecutablePath() {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return path;
}

bool SetRegistry(bool remove) {
  const std::wstring root = L"Software\\Classes\\" + std::wstring(kProtocol);
  if (remove) {
    RegDeleteTreeW(HKEY_CURRENT_USER, root.c_str());
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
  }
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, root.c_str(), 0, nullptr, 0, KEY_WRITE,
                      nullptr, &key, nullptr) != ERROR_SUCCESS)
    return false;
  const std::wstring description = L"URL:TDX THS Linker Protocol";
  const std::wstring empty;
  const std::wstring command = L"\"" + ExecutablePath() + L"\" \"%1\"";
  RegSetValueExW(key, nullptr, 0, REG_SZ,
                 reinterpret_cast<const BYTE*>(description.c_str()),
                 static_cast<DWORD>((description.size() + 1) * sizeof(wchar_t)));
  RegSetValueExW(key, L"URL Protocol", 0, REG_SZ,
                 reinterpret_cast<const BYTE*>(empty.c_str()), sizeof(wchar_t));
  RegCloseKey(key);
  const auto commandPath = root + L"\\shell\\open\\command";
  if (RegCreateKeyExW(HKEY_CURRENT_USER, commandPath.c_str(), 0, nullptr, 0,
                      KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
    return false;
  RegSetValueExW(key, nullptr, 0, REG_SZ,
                 reinterpret_cast<const BYTE*>(command.c_str()),
                 static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return true;
}

bool VerifyRegistry() {
  const std::wstring path = L"Software\\Classes\\" +
                            std::wstring(kProtocol) +
                            L"\\shell\\open\\command";
  wchar_t value[2048]{};
  DWORD bytes = sizeof(value);
  return RegGetValueW(HKEY_CURRENT_USER, path.c_str(), nullptr, RRF_RT_REG_SZ,
                      nullptr, value, &bytes) == ERROR_SUCCESS &&
         std::wstring(value) == L"\"" + ExecutablePath() + L"\" \"%1\"";
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  if (message == WM_COPYDATA) {
    const auto* data = reinterpret_cast<COPYDATASTRUCT*>(lParam);
    if (data && data->dwData == kActivateMessage && data->lpData)
      Activate(Utf8(static_cast<const wchar_t*>(data->lpData)));
    return TRUE;
  }
  if (message == WM_DESTROY) {
    running = false;
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  int count = 0;
  auto** args = CommandLineToArgvW(GetCommandLineW(), &count);
  const std::wstring argument = count > 1 ? args[1] : L"";
  LocalFree(args);
  if (argument == L"--register" || argument == L"--repair")
    return SetRegistry(false) && VerifyRegistry() ? 0 : 1;
  if (argument == L"--unregister") return SetRegistry(true) ? 0 : 1;
  if (argument == L"--verify-registration") return VerifyRegistry() ? 0 : 2;

  const auto mutex = CreateMutexW(nullptr, FALSE, kMutexName);
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    const auto window = FindWindowW(kWindowClass, nullptr);
    if (window && !argument.empty()) {
      COPYDATASTRUCT data{kActivateMessage,
                          static_cast<DWORD>((argument.size() + 1) * sizeof(wchar_t)),
                          const_cast<wchar_t*>(argument.c_str())};
      SendMessageTimeoutW(window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data),
                          SMTO_ABORTIFHUNG, 2000, nullptr);
    }
    if (mutex) CloseHandle(mutex);
    return 0;
  }

  WNDCLASSW type{};
  type.lpfnWndProc = WindowProcedure;
  type.hInstance = instance;
  type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  type.lpszClassName = kWindowClass;
  RegisterClassW(&type);
  const auto window = CreateWindowExW(0, kWindowClass, L"TDX THS Linker",
                                      WS_OVERLAPPED, 0, 0, 0, 0, nullptr,
                                      nullptr, instance, nullptr);
  if (!argument.empty() && !Activate(Utf8(argument))) {
    MessageBoxW(nullptr, L"无效的 TDX THS Linker 唤起参数。",
                L"TDX THS Linker", MB_ICONWARNING);
    DestroyWindow(window);
    CloseHandle(mutex);
    return 3;
  }

  std::promise<int> startup;
  auto startupResult = startup.get_future();
  std::thread server(HttpServer, std::move(startup));
  const auto serverError = startupResult.get();
  if (serverError != 0) {
    running = false;
    server.join();
    DestroyWindow(window);
    runtime.Stop();
    CloseHandle(mutex);
    return 4;
  }
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  running = false;
  server.join();
  runtime.Stop();
  CloseHandle(mutex);
  return 0;
}

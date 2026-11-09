#include <napi.h>

#include "linker_runtime.h"

namespace {
using onclaw_tdx_ths::CommandResult;
using onclaw_tdx_ths::EndpointState;
using onclaw_tdx_ths::InitResult;
using onclaw_tdx_ths::Runtime;
using onclaw_tdx_ths::RuntimeState;

Runtime& GetRuntime() {
  static Runtime runtime;
  return runtime;
}

Napi::Object EndpointDto(Napi::Env env, const EndpointState& value) {
  auto result = Napi::Object::New(env);
  result.Set("name", value.name);
  result.Set("code", value.code);
  result.Set("health", value.health);
  result.Set("lastError", value.lastError);
  result.Set("updatedAt", Napi::Number::New(env, static_cast<double>(value.updatedAt)));
  result.Set("lastScanAt", Napi::Number::New(env, static_cast<double>(value.lastScanAt)));
  result.Set("lastResolvedOffset", Napi::Number::New(env, value.lastOffset));
  result.Set("available", value.available);
  result.Set("usingFallback", value.usingFallback);
  result.Set("directRead", value.directRead);
  result.Set("pid", Napi::Number::New(env, value.pid));
  return result;
}

Napi::Object InitDto(Napi::Env env, const InitResult& value) {
  auto result = Napi::Object::New(env);
  result.Set("ok", value.ok);
  result.Set("licenseState", value.licenseState);
  result.Set("message", value.message);
  result.Set("runtimeHealth", value.runtimeHealth);
  return result;
}

Napi::Object StateDto(Napi::Env env, const RuntimeState& value) {
  auto result = Napi::Object::New(env);
  result.Set("initialized", value.initialized);
  result.Set("isSupported", value.supported);
  result.Set("runtimeHealth", value.runtimeHealth);
  result.Set("licenseState", value.licenseState);
  result.Set("message", value.message);
  result.Set("ths", EndpointDto(env, value.ths));
  result.Set("tdx", EndpointDto(env, value.tdx));
  return result;
}

Napi::Object CommandDto(Napi::Env env, const CommandResult& value) {
  auto result = Napi::Object::New(env);
  result.Set("ok", value.ok);
  result.Set("accepted", value.accepted);
  result.Set("status", value.status);
  result.Set("target", value.target);
  result.Set("code", value.code);
  result.Set("message", value.message);
  return result;
}

Napi::Value InitRuntime(const Napi::CallbackInfo& info) {
  const auto env = info.Env();
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected an options object")
        .ThrowAsJavaScriptException();
    return env.Null();
  }
  const auto options = info[0].As<Napi::Object>();
  const bool accepted = options.Has("licenseAccepted") &&
                        options.Get("licenseAccepted").ToBoolean().Value();
  return InitDto(env, GetRuntime().Init(accepted));
}

Napi::Value GetRuntimeState(const Napi::CallbackInfo& info) {
  return StateDto(info.Env(), GetRuntime().State());
}

Napi::Value SetRuntimeCode(const Napi::CallbackInfo& info) {
  const auto env = info.Env();
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected target and code strings")
        .ThrowAsJavaScriptException();
    return env.Null();
  }
  return CommandDto(
      env, GetRuntime().Queue(info[0].As<Napi::String>().Utf8Value(),
                              info[1].As<Napi::String>().Utf8Value()));
}
}  // namespace

Napi::Object Register(Napi::Env env, Napi::Object exports) {
  exports.Set("initRuntime", Napi::Function::New(env, InitRuntime));
  exports.Set("getRuntimeState", Napi::Function::New(env, GetRuntimeState));
  exports.Set("setRuntimeCode", Napi::Function::New(env, SetRuntimeCode));
  return exports;
}

NODE_API_MODULE(tdx_ths_linker, Register)

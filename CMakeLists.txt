cmake_minimum_required(VERSION 3.20)
project(onclaw_tdx_ths VERSION 0.1.3 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "onclaw-tdx-ths currently supports Windows only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ONCLAW_TDX_THS_VERSION "${PROJECT_VERSION}" CACHE STRING "Agent version")
option(ONCLAW_BUILD_AGENT "Build the Web Agent executable" ON)
option(ONCLAW_BUILD_TESTS "Build native tests" OFF)

if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(onclaw_tdx_ths_core STATIC
  native/core/linker_runtime.cpp
)
target_include_directories(onclaw_tdx_ths_core PUBLIC native/core)
target_compile_definitions(onclaw_tdx_ths_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(onclaw_tdx_ths_core PRIVATE /utf-8 /EHsc)
target_link_libraries(onclaw_tdx_ths_core PUBLIC user32 kernel32)

if(ONCLAW_BUILD_AGENT)
  add_executable(onclaw-tdx-ths-agent WIN32
    native/agent/main.cpp
    native/agent/agent.rc
  )
  target_link_libraries(onclaw-tdx-ths-agent PRIVATE onclaw_tdx_ths_core ws2_32 shell32 advapi32)
  target_compile_options(onclaw-tdx-ths-agent PRIVATE /utf-8 /EHsc)
  target_compile_definitions(onclaw-tdx-ths-agent PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    ONCLAW_TDX_THS_VERSION="${ONCLAW_TDX_THS_VERSION}"
  )
endif()

if(ONCLAW_BUILD_TESTS)
  enable_testing()
  add_executable(onclaw_tdx_ths_runtime_test native/core/linker_runtime_test.cpp)
  target_link_libraries(onclaw_tdx_ths_runtime_test PRIVATE onclaw_tdx_ths_core)
  target_compile_options(onclaw_tdx_ths_runtime_test PRIVATE /utf-8 /EHsc)
  add_test(NAME onclaw_tdx_ths_runtime_test COMMAND onclaw_tdx_ths_runtime_test)
endif()

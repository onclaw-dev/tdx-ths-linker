cmake_minimum_required(VERSION 3.20)
project(onclaw_tdx_ths VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "onclaw-tdx-ths currently supports Windows only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ONCLAW_TDX_THS_VERSION "${PROJECT_VERSION}" CACHE STRING "Agent version")
option(ONCLAW_BUILD_CAPI "Build the public C ABI DLL" ON)
option(ONCLAW_BUILD_NODE "Build the Node-API module" ON)
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

if(ONCLAW_BUILD_CAPI)
  add_library(onclaw_tdx_ths_capi SHARED
    native/c-api/onclaw_tdx_ths.cpp
  )
  set_target_properties(onclaw_tdx_ths_capi PROPERTIES OUTPUT_NAME "onclaw-tdx-ths")
  target_include_directories(onclaw_tdx_ths_capi PUBLIC native/c-api)
  target_link_libraries(onclaw_tdx_ths_capi PRIVATE onclaw_tdx_ths_core)
  target_compile_definitions(onclaw_tdx_ths_capi PRIVATE ONCLAW_TDX_THS_CAPI_EXPORTS)
  target_compile_options(onclaw_tdx_ths_capi PRIVATE /utf-8 /EHsc)
  install(TARGETS onclaw_tdx_ths_capi
    RUNTIME DESTINATION tdx_ths_linker/_native
    LIBRARY DESTINATION tdx_ths_linker/_native
  )
endif()

if(ONCLAW_BUILD_NODE)
  execute_process(
    COMMAND node -p "require('node-addon-api').include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )
  string(REPLACE "\"" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")

  add_library(onclaw_tdx_ths_node SHARED
    native/node/node.cpp
    ${CMAKE_JS_SRC}
  )
  set_target_properties(onclaw_tdx_ths_node PROPERTIES PREFIX "" SUFFIX ".node" OUTPUT_NAME "tdx-ths-linker")
  target_include_directories(onclaw_tdx_ths_node PRIVATE ${NODE_ADDON_API_DIR} ${CMAKE_JS_INC})
  target_link_libraries(onclaw_tdx_ths_node PRIVATE onclaw_tdx_ths_core ${CMAKE_JS_LIB})
  target_compile_options(onclaw_tdx_ths_node PRIVATE /utf-8 /EHsc)
endif()

if(ONCLAW_BUILD_AGENT)
  add_executable(onclaw-tdx-ths-agent WIN32
    native/agent/main.cpp
    native/agent/agent.rc
  )
  target_link_libraries(onclaw-tdx-ths-agent PRIVATE onclaw_tdx_ths_core ws2_32 bcrypt shell32 advapi32)
  target_compile_options(onclaw-tdx-ths-agent PRIVATE /utf-8 /EHsc)
  target_compile_definitions(onclaw-tdx-ths-agent PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    ONCLAW_TDX_THS_VERSION="${ONCLAW_TDX_THS_VERSION}"
  )
endif()

if(ONCLAW_BUILD_TESTS)
  enable_testing()
  add_executable(onclaw_tdx_ths_capi_test native/c-api/onclaw_tdx_ths_test.cpp)
  target_link_libraries(onclaw_tdx_ths_capi_test PRIVATE onclaw_tdx_ths_capi)
  target_compile_options(onclaw_tdx_ths_capi_test PRIVATE /utf-8 /EHsc)
  add_test(NAME onclaw_tdx_ths_capi_test COMMAND onclaw_tdx_ths_capi_test)
endif()

cmake_minimum_required(VERSION 3.20)
project(tdx_ths_linker VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "tdx-ths-linker currently supports Windows only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(TDX_THS_LINKER_VERSION "${PROJECT_VERSION}" CACHE STRING "Agent version")

add_library(tdx_ths_linker_core STATIC
  native/core/linker_runtime.cpp
)
target_include_directories(tdx_ths_linker_core PUBLIC native/core)
target_compile_definitions(tdx_ths_linker_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(tdx_ths_linker_core PRIVATE /utf-8 /EHsc)
target_link_libraries(tdx_ths_linker_core PUBLIC user32 kernel32)

execute_process(
  COMMAND node -p "require('node-addon-api').include"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE NODE_ADDON_API_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
string(REPLACE "\"" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")

add_library(tdx_ths_linker_node SHARED
  native/node-addon/addon.cpp
  ${CMAKE_JS_SRC}
)
set_target_properties(tdx_ths_linker_node PROPERTIES PREFIX "" SUFFIX ".node" OUTPUT_NAME "tdx-ths-linker")
target_include_directories(tdx_ths_linker_node PRIVATE ${NODE_ADDON_API_DIR} ${CMAKE_JS_INC})
target_link_libraries(tdx_ths_linker_node PRIVATE tdx_ths_linker_core ${CMAKE_JS_LIB})
target_compile_definitions(tdx_ths_linker_node PRIVATE TDX_THS_ENABLE_THS_WRITE=1)
target_compile_options(tdx_ths_linker_node PRIVATE /utf-8 /EHsc)

function(add_linker_agent target output_name channel ths_write)
  add_executable(${target} WIN32 native/agent/main.cpp)
  target_link_libraries(${target} PRIVATE tdx_ths_linker_core ws2_32 bcrypt shell32 advapi32)
  target_compile_options(${target} PRIVATE /utf-8 /EHsc)
  target_compile_definitions(${target} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    TDX_THS_ENABLE_THS_WRITE=${ths_write}
    TDX_THS_AGENT_CHANNEL="${channel}"
    TDX_THS_LINKER_VERSION="${TDX_THS_LINKER_VERSION}"
  )
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${output_name}")
endfunction()

add_linker_agent(tdx-ths-linker-agent tdx-ths-linker-agent stable 0)
add_linker_agent(tdx-ths-linker-agent-beta tdx-ths-linker-agent-beta beta 1)
add_custom_target(tdx-ths-linker-agents DEPENDS tdx-ths-linker-agent tdx-ths-linker-agent-beta)

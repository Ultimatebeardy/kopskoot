cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# KopSkoot needs the frontend API (dock, hotkeys, events) and Qt (UI).
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(KILLSENSE_BUILD_TESTS "Also build unit tests and the replay tool" OFF)
option(KILLSENSE_ENABLE_WINDOWS_OCR "Use Windows.Media.Ocr (C++/WinRT) on Windows" ON)

if(NOT ENABLE_FRONTEND_API OR NOT ENABLE_QT)
  message(FATAL_ERROR "KopSkoot requires ENABLE_FRONTEND_API=ON and ENABLE_QT=ON")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

# OBS-independent detection core (+ tests/replay tool when enabled).
add_subdirectory(core)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_package(Qt6 COMPONENTS Widgets Core REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE killsense::core)

# Vendored obs-websocket-api.h takes precedence over any system copy.
target_include_directories(
  ${CMAKE_PROJECT_NAME}
  BEFORE
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/obs-websocket-api" "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /EHsc)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/controller.cpp
    src/controller.hpp
    src/obs/frame-sampler.cpp
    src/obs/frame-sampler.hpp
    src/obs/obs-raii.hpp
    src/obs/obs-sources.cpp
    src/obs/obs-sources.hpp
    src/outputs/websocket-vendor-output.cpp
    src/outputs/websocket-vendor-output.hpp
    src/ui/capture-dialog.cpp
    src/ui/capture-dialog.hpp
    src/ui/debug-view.cpp
    src/ui/debug-view.hpp
    src/ui/killsense-dock.cpp
    src/ui/killsense-dock.hpp
    src/ui/profile-manager-dialog.cpp
    src/ui/profile-manager-dialog.hpp
    src/ui/settings-dialog.cpp
    src/ui/settings-dialog.hpp
    src/ui/ui-common.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Developer tool: headless libobs integration test of the GPU capture path.
option(KILLSENSE_BUILD_OBS_HARNESS "Build the headless libobs integration harness (developer tool)" OFF)
if(KILLSENSE_BUILD_OBS_HARNESS)
  add_executable(killsense-obs-harness tools/libobs-harness/main.cpp src/obs/frame-sampler.cpp)
  target_include_directories(killsense-obs-harness PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(killsense-obs-harness PRIVATE OBS::libobs killsense::core)
  if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(killsense-obs-harness PRIVATE X11::X11)
  endif()
endif()

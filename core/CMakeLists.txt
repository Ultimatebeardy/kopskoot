# REALM KillSense - OBS-independent core library.
#
# Can be built on its own (no OBS needed) for unit tests and the replay tool:
#   cmake -S core -B build_core
#   cmake --build build_core --config Release
#   ctest --test-dir build_core -C Release
#
# Or as part of the plugin build (root CMakeLists.txt adds this directory).

cmake_minimum_required(VERSION 3.24...3.30)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(killsense-core VERSION 0.1.0 LANGUAGES C CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(KILLSENSE_STANDALONE ON)
  option(KILLSENSE_BUILD_TESTS "Build unit tests and the replay tool" ON)
  option(KILLSENSE_ENABLE_WINDOWS_OCR "Use Windows.Media.Ocr (C++/WinRT) when building on Windows" ON)
  if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive- /Zc:__cplusplus /EHsc)
  else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
  endif()
else()
  set(KILLSENSE_STANDALONE OFF)
endif()

set(KILLSENSE_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

include(FetchContent)

# --- nlohmann/json (MIT) -------------------------------------------------------
# Header-only JSON library used for profiles, configuration and events.
# Uses an installed package if available, otherwise downloads the pinned release.
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  set(JSON_Install OFF CACHE INTERNAL "")
  FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(Threads REQUIRED)

add_library(killsense-core STATIC)
add_library(killsense::core ALIAS killsense-core)

target_sources(
  killsense-core
  PRIVATE
    src/calibration.cpp
    src/capture.cpp
    src/config.cpp
    src/engine.cpp
    src/event-bus.cpp
    src/event.cpp
    src/fsutil.cpp
    src/fusion.cpp
    src/image.cpp
    src/logging.cpp
    src/profile.cpp
    src/replay.cpp
    src/session.cpp
    src/temporal.cpp
    src/text-match.cpp
    src/worker.cpp
    src/detectors/basic-detectors.cpp
    src/detectors/detectors.hpp
    src/detectors/ocr-detector.cpp
    src/detectors/template-detector.cpp
    src/detectors/template-matcher.cpp
    src/ocr/ocr-common.cpp
)

target_include_directories(killsense-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# stb is private: only image.cpp includes it.
target_include_directories(killsense-core SYSTEM PRIVATE "${KILLSENSE_REPO_ROOT}/third_party/stb")
target_link_libraries(killsense-core PUBLIC nlohmann_json::nlohmann_json Threads::Threads)
target_compile_features(killsense-core PUBLIC cxx_std_17)
set_target_properties(killsense-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(killsense-core PRIVATE /EHsc)
  target_compile_definitions(killsense-core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# --- Windows.Media.Ocr (built into Windows 10/11, no redistribution) ----------
if(WIN32 AND KILLSENSE_ENABLE_WINDOWS_OCR)
  include(CheckIncludeFileCXX)
  set(CMAKE_REQUIRED_FLAGS "/std:c++17 /EHsc")
  check_include_file_cxx("winrt/Windows.Media.Ocr.h" KILLSENSE_HAVE_CPPWINRT)
  unset(CMAKE_REQUIRED_FLAGS)
  if(KILLSENSE_HAVE_CPPWINRT)
    target_sources(killsense-core PRIVATE src/ocr/windows-ocr-engine.cpp)
    target_compile_definitions(killsense-core PUBLIC KILLSENSE_HAS_WINDOWS_OCR=1)
    target_link_libraries(killsense-core PUBLIC WindowsApp)
    message(STATUS "KillSense: Windows.Media.Ocr backend enabled")
  else()
    message(
      WARNING
      "KillSense: C++/WinRT headers (winrt/Windows.Media.Ocr.h) not found in the Windows SDK. "
      "OCR detectors will be unavailable. Install Windows SDK 10.0.20348+ or set KILLSENSE_ENABLE_WINDOWS_OCR=OFF."
    )
  endif()
endif()

if(KILLSENSE_BUILD_TESTS)
  enable_testing()
  add_subdirectory("${KILLSENSE_REPO_ROOT}/tools/replay" "${CMAKE_CURRENT_BINARY_DIR}/tools/replay")
  add_subdirectory("${KILLSENSE_REPO_ROOT}/tests" "${CMAKE_CURRENT_BINARY_DIR}/tests")
endif()

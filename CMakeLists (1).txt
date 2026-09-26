# REALM KillSense unit tests (GoogleTest, BSD-3-Clause, fetched at configure time).
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
  URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(
  killsense-tests
  test-helpers.hpp
  test-calibration.cpp
  test-config.cpp
  test-event-session.cpp
  test-fusion.cpp
  test-geometry-capture.cpp
  test-killfeed-personal.cpp
  test-profile.cpp
  test-replay.cpp
  test-temporal.cpp
  test-template-match.cpp
  test-text-match.cpp
  test-worker.cpp
)
target_link_libraries(killsense-tests PRIVATE killsense::core GTest::gtest_main)
target_compile_definitions(
  killsense-tests
  PRIVATE
    KILLSENSE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    KILLSENSE_PROFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/profiles"
)
if(MSVC)
  target_compile_options(killsense-tests PRIVATE /EHsc /bigobj)
endif()

include(GoogleTest)
gtest_discover_tests(killsense-tests DISCOVERY_TIMEOUT 60)

# End-to-end replay through the CLI (same code path as the unit test, but
# exercises argument parsing and exit codes).
add_test(
  NAME replay-cli-sample-killfeed
  COMMAND
    killsense-replay --profile "${CMAKE_CURRENT_SOURCE_DIR}/../data/profiles/sample-killfeed.json" --frames
    "${CMAKE_CURRENT_SOURCE_DIR}/data/sequences/sample-killfeed" --fps 6 --ocr sidecar --expect 4
)

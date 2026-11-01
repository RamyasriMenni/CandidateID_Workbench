cmake_minimum_required(VERSION 3.16)
project(UnifiedWorkbench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

# ---- Core library: no UI, no transport, no third-party dependencies ----
add_library(wb_core STATIC
  src/core/fem1d.cpp src/core/batch.cpp src/core/backprop.cpp
  src/core/stl_parser.cpp src/core/nodal_csv.cpp
  src/core/devices.cpp src/core/sysinfo.cpp)
target_include_directories(wb_core PUBLIC src/core)
target_link_libraries(wb_core PUBLIC Threads::Threads)
if(MSVC)
  target_compile_options(wb_core PRIVATE /W4 /permissive-)
  set(WB_FLAGS "/O2")
else()
  target_compile_options(wb_core PRIVATE -Wall -Wextra -Wpedantic)
  set(WB_FLAGS "${CMAKE_CXX_FLAGS_RELEASE}")
endif()
target_compile_definitions(wb_core PRIVATE WB_BUILD_FLAGS="${WB_FLAGS}")

# ---- Command-line front end (also the process the GUI/transport layer drives) ----
add_executable(wb_cli src/cli/main.cpp)
target_link_libraries(wb_cli PRIVATE wb_core)

# ---- Tests (self-contained harness; no downloads) ----
enable_testing()
add_executable(wb_tests tests/test_main.cpp)
target_link_libraries(wb_tests PRIVATE wb_core)
add_test(NAME wb_tests COMMAND wb_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

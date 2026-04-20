cmake_minimum_required(VERSION 3.25)
project(Premonition)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(IPLUG2_DIR ${CMAKE_SOURCE_DIR}/iPlug2)
include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

# signalsmith-stretch (MIT): spectral time-stretching used by the offline
# pipeline for Stretch and FitToBar stages.
include(FetchContent)
FetchContent_Declare(
  signalsmith-stretch
  GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
  GIT_TAG        main
  GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(signalsmith-stretch)

# stb (public domain): we use stb_vorbis.c for OGG decoding. WAV/AIFF/MP3/M4A
# are handled via macOS ExtAudioFile (no dep).
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
  GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

add_subdirectory(Premonition)

# Offline tools (IR renderer) — opt-in
option(PREMONITION_BUILD_TOOLS "Build offline tools (IR renderer)" ON)
if(PREMONITION_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

# Tests (Catch2) — opt-in
option(PREMONITION_BUILD_TESTS "Build Premonition unit tests" ON)
if(PREMONITION_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

cmake_minimum_required(VERSION 3.14)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.3
  GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(Catch2)

add_executable(premonition_tests
  test_reverb.cpp
  test_reverse.cpp
  test_fit_to_bar.cpp
  test_pipeline.cpp
  test_convolution.cpp
  ${CMAKE_SOURCE_DIR}/iPlug2/WDL/fft.c
)

target_include_directories(premonition_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/Premonition
  ${CMAKE_SOURCE_DIR}/iPlug2
)

target_link_libraries(premonition_tests PRIVATE
  Catch2::Catch2WithMain
  signalsmith-stretch
)

include(CTest)
include(Catch)
catch_discover_tests(premonition_tests)

cmake_minimum_required(VERSION 3.14)
project(Premonition VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2" CACHE PATH "iPlug2 root directory")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)

find_package(iPlug2 REQUIRED)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    Premonition.cpp
    Premonition.h
    Parameters.h
    dsp/DelayLine.h
    dsp/Reverb.h
    dsp/Reverse.h
    dsp/TimeStretch.h
    dsp/FitToBar.h
    dsp/OfflinePipeline.h
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
)

# signalsmith-stretch (MIT, FetchContent in root CMakeLists.txt)
target_link_libraries(${PROJECT_NAME} PRIVATE signalsmith-stretch)
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

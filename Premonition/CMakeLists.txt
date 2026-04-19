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
    Presets.h
    dsp/DelayLine.h
    dsp/Reverb.h
    dsp/Reverse.h
    dsp/TimeStretch.h
    dsp/FitToBar.h
    dsp/OfflinePipeline.h
    dsp/Convolution.h
    dsp/AudioLoader.h
    dsp/AudioLoader.mm
    dsp/WavWriter.h
    dsp/stb_vorbis_impl.cpp
    ${IPLUG2_DIR}/WDL/fft.c
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
  LINK
    signalsmith-stretch
    stb
)

# macOS-native audio decoding for WAV/AIFF/MP3/M4A via AudioToolbox.
# OGG is handled by stb_vorbis (header-only).
if(APPLE)
  foreach(_fmt app vst3 au clap aax auv3)
    if(TARGET ${PROJECT_NAME}-${_fmt})
      target_link_libraries(${PROJECT_NAME}-${_fmt} PRIVATE
        "-framework AudioToolbox"
        "-framework CoreFoundation")
    endif()
  endforeach()
endif()

# Propagate Premonition/ include root to every per-format target so
# sources can #include "dsp/..." and "Parameters.h" directly.
foreach(_fmt app vst3 au clap aax auv3)
  if(TARGET ${PROJECT_NAME}-${_fmt})
    target_include_directories(${PROJECT_NAME}-${_fmt} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${IPLUG2_DIR}
    )
  endif()
endforeach()

project(obs-webrtc)

set(OBS_WEBRTC_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_library(obs-webrtc MODULE)
add_library(OBS::webrtc ALIAS obs-webrtc)

target_sources(
  obs-webrtc PRIVATE 
  webrtc-output.cpp 
  webrtc-output.h 
  webrtc.cpp 
  webrtc-stream.cpp 
  webrtc-stream.h
  whip-service.c 
  whip.c
  whip.h)

  INCLUDE_DIRECTORIES("/Users/ddrboxman/Development/libdatachannel/include")
  LINK_DIRECTORIES("/Users/ddrboxman/Development/libdatachannel/build")

target_link_libraries(obs-webrtc 
  OBS::libobs 
  OBS::file-updater
  /Users/ddrboxman/Development/libdatachannel/build/libdatachannel.0.18.2.dylib)

target_compile_features(obs-webrtc PRIVATE cxx_std_17)

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS webrtc module")
  configure_file("${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in"
                 obs-webrtc.rc)
  target_sources(obs-webrtc PRIVATE obs-webrtc.rc)
endif()

set_target_properties(obs-webrtc PROPERTIES FOLDER "plugins")

setup_plugin_target(obs-webrtc data)
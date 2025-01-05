cmake_minimum_required(VERSION 3.28...3.30)

option(ENABLE_AUDIO_UNIT "Enable Audio Unit Support" ON)
if(NOT ENABLE_AUDIO_UNIT)
  target_disable(mac-au)
  target_disable_feature(mac-au "Audio Unit support")
  return()
else()
  target_enable_feature(mac-au "Audio Unit support")
endif()

add_library(mac-au MODULE)
add_library(OBS::au ALIAS mac-au)

target_sources(
  mac-au
  PRIVATE plugin-main.c
            mac-au.m
            util.h)

target_link_libraries(
  mac-au
  PRIVATE
    OBS::libobs
    Qt::Widgets
    "$<LINK_LIBRARY:FRAMEWORK,AVFAudio.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,CoreFoundation.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,Cocoa.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,AudioToolbox.framework>"
)

set_target_properties_obs(
  mac-au
  PROPERTIES FOLDER plugins
             PREFIX ""
             AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

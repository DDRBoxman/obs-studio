cmake_minimum_required(VERSION 3.22...3.25)

legacy_check()

option(ENABLE_SHADERTOY "Enable Shadertoy Source" ON)
if(NOT ENABLE_SHADERTOY)
  target_disable(shadertoy)
  return()
endif()

find_package(CURL REQUIRED)

add_library(shadertoy MODULE)
add_library(OBS::shadertoy ALIAS shadertoy)

target_sources(
  shadertoy PRIVATE # cmake-format: sortable
                     shadertoy.cpp)

target_link_libraries(shadertoy PRIVATE OBS::libobs CURL::libcurl)

# cmake-format: off
set_target_properties_obs(shadertoy PROPERTIES FOLDER plugins PREFIX "")
# cmake-format: on

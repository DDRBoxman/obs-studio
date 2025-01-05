cmake_minimum_required(VERSION 3.22...3.25)

legacy_check()

option(ENABLE_VST3 "Enable VST3 Source" ON)
if(NOT ENABLE_VST3)
  target_disable(obs-vst3)
  return()
endif()

add_library(obs-vst3 MODULE)
add_library(OBS::vst3 ALIAS obs-vst3)

find_library(COCOA_LIBRARY Cocoa)

add_library(sdk_common
    STATIC
        ./vst3sdk/public.sdk/source/common/commoniids.cpp
        ./vst3sdk/public.sdk/source/common/openurl.cpp
        ./vst3sdk/public.sdk/source/common/openurl.h
        ./vst3sdk/public.sdk/source/common/systemclipboard.h
        ./vst3sdk/public.sdk/source/common/systemclipboard_linux.cpp
        ./vst3sdk/public.sdk/source/common/systemclipboard_win32.cpp
        ./vst3sdk/public.sdk/source/common/threadchecker.h
        ./vst3sdk/public.sdk/source/common/threadchecker_linux.cpp
        ./vst3sdk/public.sdk/source/common/threadchecker_win32.cpp
        ./vst3sdk/pluginterfaces/gui/iplugview.h
)

target_include_directories(sdk_common PRIVATE ./vst3sdk/)


add_library(pluginterfaces STATIC
    ./vst3sdk/pluginterfaces/base/conststringtable.cpp
    ./vst3sdk/pluginterfaces/base/conststringtable.h
    ./vst3sdk/pluginterfaces/base/coreiids.cpp
    ./vst3sdk/pluginterfaces/base/falignpop.h
    ./vst3sdk/pluginterfaces/base/falignpush.h
    ./vst3sdk/pluginterfaces/base/fplatform.h
    ./vst3sdk/pluginterfaces/base/fstrdefs.h
    ./vst3sdk/pluginterfaces/base/ftypes.h
    ./vst3sdk/pluginterfaces/base/funknown.cpp
    ./vst3sdk/pluginterfaces/base/funknown.h
    ./vst3sdk/pluginterfaces/base/funknownimpl.h
    ./vst3sdk/pluginterfaces/base/futils.h
    ./vst3sdk/pluginterfaces/base/fvariant.h
    ./vst3sdk/pluginterfaces/base/geoconstants.h
    ./vst3sdk/pluginterfaces/base/ibstream.h
    ./vst3sdk/pluginterfaces/base/icloneable.h
    ./vst3sdk/pluginterfaces/base/ierrorcontext.h
    ./vst3sdk/pluginterfaces/base/ipersistent.h
    ./vst3sdk/pluginterfaces/base/ipluginbase.h
    ./vst3sdk/pluginterfaces/base/istringresult.h
    ./vst3sdk/pluginterfaces/base/iupdatehandler.h
    ./vst3sdk/pluginterfaces/base/keycodes.h
    ./vst3sdk/pluginterfaces/base/pluginbasefwd.h
    ./vst3sdk/pluginterfaces/base/smartpointer.h
    ./vst3sdk/pluginterfaces/base/typesizecheck.h
    ./vst3sdk/pluginterfaces/base/ucolorspec.h
    ./vst3sdk/pluginterfaces/base/ustring.cpp
    ./vst3sdk/pluginterfaces/base/ustring.h
)

target_sources(pluginterfaces
        PRIVATE
            ./vst3sdk/pluginterfaces/gui/iplugview.h
            ./vst3sdk/pluginterfaces/gui/iplugviewcontentscalesupport.h
            ./vst3sdk/pluginterfaces/vst/ivstattributes.h
            ./vst3sdk/pluginterfaces/vst/ivstaudioprocessor.h
            ./vst3sdk/pluginterfaces/vst/ivstautomationstate.h
            ./vst3sdk/pluginterfaces/vst/ivstchannelcontextinfo.h
            ./vst3sdk/pluginterfaces/vst/ivstcomponent.h
            ./vst3sdk/pluginterfaces/vst/ivstcontextmenu.h
            ./vst3sdk/pluginterfaces/vst/ivsteditcontroller.h
            ./vst3sdk/pluginterfaces/vst/ivstevents.h
            ./vst3sdk/pluginterfaces/vst/ivstdataexchange.h
            ./vst3sdk/pluginterfaces/vst/ivsthostapplication.h
            ./vst3sdk/pluginterfaces/vst/ivstinterappaudio.h
            ./vst3sdk/pluginterfaces/vst/ivstmessage.h
            ./vst3sdk/pluginterfaces/vst/ivstmidicontrollers.h
            ./vst3sdk/pluginterfaces/vst/ivstmidilearn.h
            ./vst3sdk/pluginterfaces/vst/ivstnoteexpression.h
            ./vst3sdk/pluginterfaces/vst/ivstparameterchanges.h
            ./vst3sdk/pluginterfaces/vst/ivstparameterfunctionname.h
            ./vst3sdk/pluginterfaces/vst/ivstphysicalui.h
            ./vst3sdk/pluginterfaces/vst/ivstpluginterfacesupport.h
            ./vst3sdk/pluginterfaces/vst/ivstplugview.h
            ./vst3sdk/pluginterfaces/vst/ivstprefetchablesupport.h
            ./vst3sdk/pluginterfaces/vst/ivstprocesscontext.h
            ./vst3sdk/pluginterfaces/vst/ivstrepresentation.h
            ./vst3sdk/pluginterfaces/vst/ivstunits.h
            ./vst3sdk/pluginterfaces/vst/vstpresetkeys.h
            ./vst3sdk/pluginterfaces/vst/vstpshpack4.h
            ./vst3sdk/pluginterfaces/vst/vstspeaker.h
            ./vst3sdk/pluginterfaces/vst/vsttypes.h
)

target_compile_features(pluginterfaces
    PUBLIC
        cxx_std_17
)

target_include_directories(pluginterfaces PRIVATE ./vst3sdk/)

add_library(sdk_hosting 
STATIC
    ./vst3sdk/public.sdk/source/vst/hosting/connectionproxy.cpp
    ./vst3sdk/public.sdk/source/vst/hosting/connectionproxy.h
    ./vst3sdk/public.sdk/source/vst/hosting/eventlist.cpp
    ./vst3sdk/public.sdk/source/vst/hosting/eventlist.h
    ./vst3sdk/public.sdk/source/vst/hosting/hostclasses.cpp
    ./vst3sdk/public.sdk/source/vst/hosting/hostclasses.h
    ./vst3sdk/public.sdk/source/vst/hosting/module.cpp
    ./vst3sdk/public.sdk/source/vst/hosting/module.h
    ./vst3sdk/public.sdk/source/vst/hosting/parameterchanges.cpp
    ./vst3sdk/public.sdk/source/vst/hosting/parameterchanges.h
    ./vst3sdk/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
    ./vst3sdk/public.sdk/source/vst/hosting/pluginterfacesupport.h
    ./vst3sdk/public.sdk/source/vst/hosting/processdata.cpp
    ./vst3sdk/public.sdk/source/vst/hosting/processdata.h
    ./vst3sdk/public.sdk/source/vst/utility/optional.h
    ./vst3sdk/public.sdk/source/vst/utility/stringconvert.cpp
    ./vst3sdk/public.sdk/source/vst/utility/stringconvert.h
    ./vst3sdk/public.sdk/source/vst/utility/uid.h
    ./vst3sdk/public.sdk/source/vst/utility/versionparser.h
    ./vst3sdk/public.sdk/source/vst/vstinitiids.cpp
)

target_compile_options(
  sdk_hosting PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Wno-deprecated-declarations>)

target_link_libraries(sdk_hosting 
PUBLIC 
    sdk_common
)
target_include_directories(sdk_hosting PRIVATE ./vst3sdk/)

target_sources(
  obs-vst3 PRIVATE # cmake-format: sortable
                     main.cpp
                     VST3Filter.cpp
                     VST3Filter.h
                     ./vst3sdk/public.sdk/source/vst/hosting/module_mac.mm
                     )
set_source_files_properties(
    "./vst3sdk/public.sdk/source/vst/hosting/module_mac.mm" PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

target_include_directories(obs-vst3 PRIVATE ./vst3sdk/)

target_link_libraries(obs-vst3 PRIVATE OBS::libobs sdk_hosting pluginterfaces)

# cmake-format: off
set_target_properties_obs(obs-vst3 PROPERTIES FOLDER plugins PREFIX "")
# cmake-format: on
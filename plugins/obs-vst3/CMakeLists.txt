cmake_minimum_required(VERSION 3.28...3.30)


option(ENABLE_VST3 "Enable building OBS with VST3 plugin" ON)

if(NOT ENABLE_VST3)
  target_disable(obs-vst3)
  return()
endif()

add_library(obs-vst3 MODULE)
add_library(OBS::vst3 ALIAS obs-vst3)

find_library(COCOA_LIBRARY Cocoa)

add_library(sdk_common
    STATIC
        ./vst3_sdk/public.sdk/source/common/commoniids.cpp
        ./vst3_sdk/public.sdk/source/common/commonstringconvert.cpp
        ./vst3_sdk/public.sdk/source/common/commonstringconvert.h
        ./vst3_sdk/public.sdk/source/common/commoniids.cpp
        ./vst3_sdk/public.sdk/source/common/openurl.cpp
        ./vst3_sdk/public.sdk/source/common/openurl.h
        ./vst3_sdk/public.sdk/source/common/systemclipboard.h
        ./vst3_sdk/public.sdk/source/common/systemclipboard_linux.cpp
        ./vst3_sdk/public.sdk/source/common/systemclipboard_win32.cpp
        ./vst3_sdk/public.sdk/source/common/threadchecker.h
        ./vst3_sdk/public.sdk/source/common/threadchecker_linux.cpp
        ./vst3_sdk/public.sdk/source/common/threadchecker_win32.cpp
        #./vst3_sdk/pluginterfaces/gui/iplugview.h

        ./vst3_sdk/public.sdk/source/common/threadchecker_mac.mm
        ./vst3_sdk/public.sdk/source/common/systemclipboard_mac.mm

        		    ./vst3_sdk/public.sdk/source/common/readfile.cpp
           ./vst3_sdk/public.sdk/source/common/readfile.h
)
target_include_directories(sdk_common PRIVATE ./vst3_sdk/)

add_library(pluginterfaces STATIC
    ./vst3_sdk/pluginterfaces/base/conststringtable.cpp
    ./vst3_sdk/pluginterfaces/base/conststringtable.h
    ./vst3_sdk/pluginterfaces/base/coreiids.cpp
    ./vst3_sdk/pluginterfaces/base/falignpop.h
    ./vst3_sdk/pluginterfaces/base/falignpush.h
    ./vst3_sdk/pluginterfaces/base/fplatform.h
    ./vst3_sdk/pluginterfaces/base/fstrdefs.h
    ./vst3_sdk/pluginterfaces/base/ftypes.h
    ./vst3_sdk/pluginterfaces/base/funknown.cpp
    ./vst3_sdk/pluginterfaces/base/funknown.h
    ./vst3_sdk/pluginterfaces/base/funknownimpl.h
    ./vst3_sdk/pluginterfaces/base/futils.h
    ./vst3_sdk/pluginterfaces/base/fvariant.h
    ./vst3_sdk/pluginterfaces/base/geoconstants.h
    ./vst3_sdk/pluginterfaces/base/ibstream.h
    ./vst3_sdk/pluginterfaces/base/icloneable.h
    ./vst3_sdk/pluginterfaces/base/ierrorcontext.h
    ./vst3_sdk/pluginterfaces/base/ipersistent.h
    ./vst3_sdk/pluginterfaces/base/ipluginbase.h
    ./vst3_sdk/pluginterfaces/base/istringresult.h
    ./vst3_sdk/pluginterfaces/base/iupdatehandler.h
    ./vst3_sdk/pluginterfaces/base/keycodes.h
    ./vst3_sdk/pluginterfaces/base/pluginbasefwd.h
    ./vst3_sdk/pluginterfaces/base/smartpointer.h
    ./vst3_sdk/pluginterfaces/base/typesizecheck.h
    ./vst3_sdk/pluginterfaces/base/ucolorspec.h
    ./vst3_sdk/pluginterfaces/base/ustring.cpp
    ./vst3_sdk/pluginterfaces/base/ustring.h
)
target_sources(pluginterfaces
        PRIVATE
            ./vst3_sdk/pluginterfaces/gui/iplugview.h
            ./vst3_sdk/pluginterfaces/gui/iplugviewcontentscalesupport.h
            ./vst3_sdk/pluginterfaces/vst/ivstattributes.h
            ./vst3_sdk/pluginterfaces/vst/ivstaudioprocessor.h
            ./vst3_sdk/pluginterfaces/vst/ivstautomationstate.h
            ./vst3_sdk/pluginterfaces/vst/ivstchannelcontextinfo.h
            ./vst3_sdk/pluginterfaces/vst/ivstcomponent.h
            ./vst3_sdk/pluginterfaces/vst/ivstcontextmenu.h
            ./vst3_sdk/pluginterfaces/vst/ivsteditcontroller.h
            ./vst3_sdk/pluginterfaces/vst/ivstevents.h
            ./vst3_sdk/pluginterfaces/vst/ivstdataexchange.h
            ./vst3_sdk/pluginterfaces/vst/ivsthostapplication.h
            ./vst3_sdk/pluginterfaces/vst/ivstinterappaudio.h
            ./vst3_sdk/pluginterfaces/vst/ivstmessage.h
            ./vst3_sdk/pluginterfaces/vst/ivstmidicontrollers.h
            ./vst3_sdk/pluginterfaces/vst/ivstmidilearn.h
            ./vst3_sdk/pluginterfaces/vst/ivstnoteexpression.h
            ./vst3_sdk/pluginterfaces/vst/ivstparameterchanges.h
            ./vst3_sdk/pluginterfaces/vst/ivstparameterfunctionname.h
            ./vst3_sdk/pluginterfaces/vst/ivstphysicalui.h
            ./vst3_sdk/pluginterfaces/vst/ivstpluginterfacesupport.h
            ./vst3_sdk/pluginterfaces/vst/ivstplugview.h
            ./vst3_sdk/pluginterfaces/vst/ivstprefetchablesupport.h
            ./vst3_sdk/pluginterfaces/vst/ivstprocesscontext.h
            ./vst3_sdk/pluginterfaces/vst/ivstrepresentation.h
            ./vst3_sdk/pluginterfaces/vst/ivstunits.h
            ./vst3_sdk/pluginterfaces/vst/vstpresetkeys.h
            ./vst3_sdk/pluginterfaces/vst/vstpshpack4.h
            ./vst3_sdk/pluginterfaces/vst/vstspeaker.h
            ./vst3_sdk/pluginterfaces/vst/vsttypes.h
)
target_compile_features(pluginterfaces
    PUBLIC
        cxx_std_17
)
target_include_directories(pluginterfaces PRIVATE ./vst3_sdk/)
add_library(sdk_hosting
STATIC
  ./vst3_sdk/public.sdk/source/vst/hosting/connectionproxy.cpp
  ./vst3_sdk/public.sdk/source/vst/hosting/connectionproxy.h
  ./vst3_sdk/public.sdk/source/vst/hosting/eventlist.cpp
  ./vst3_sdk/public.sdk/source/vst/hosting/eventlist.h
  ./vst3_sdk/public.sdk/source/vst/hosting/hostclasses.cpp
  ./vst3_sdk/public.sdk/source/vst/hosting/hostclasses.h
  ./vst3_sdk/public.sdk/source/vst/hosting/module.cpp
  ./vst3_sdk/public.sdk/source/vst/hosting/module.h
  ./vst3_sdk/public.sdk/source/vst/hosting/parameterchanges.cpp
  ./vst3_sdk/public.sdk/source/vst/hosting/parameterchanges.h
  ./vst3_sdk/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
  ./vst3_sdk/public.sdk/source/vst/hosting/pluginterfacesupport.h
  ./vst3_sdk/public.sdk/source/vst/hosting/processdata.cpp
  ./vst3_sdk/public.sdk/source/vst/hosting/processdata.h
  ./vst3_sdk/public.sdk/source/vst/utility/optional.h
  ./vst3_sdk/public.sdk/source/vst/utility/stringconvert.cpp
  ./vst3_sdk/public.sdk/source/vst/utility/stringconvert.h
  ./vst3_sdk/public.sdk/source/vst/utility/uid.h
  ./vst3_sdk/public.sdk/source/vst/utility/versionparser.h
  ./vst3_sdk/public.sdk/source/vst/vstinitiids.cpp
)
target_compile_options(
  sdk_hosting PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Wno-deprecated-declarations>)
target_link_libraries(sdk_hosting
PUBLIC
    sdk_common
)
target_include_directories(sdk_hosting PRIVATE ./vst3_sdk/)

target_sources(
  obs-vst3 PRIVATE # cmake-format: sortable
                     main.cpp
                     VST3Filter.cpp
                     VST3Filter.h
                     VST3Manager.cpp
                     VST3Manager.h
                    ./vst3_sdk/public.sdk/source/vst/hosting/module_mac.mm
                    ./vst3_sdk/public.sdk/source/vst/hosting/plugprovider.cpp
                     )
set_source_files_properties(
    "./vst3_sdk/public.sdk/source/vst/hosting/module_mac.mm" PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

target_include_directories(obs-vst3 PRIVATE ./vst3_sdk/)

target_link_libraries(obs-vst3 PRIVATE OBS::libobs sdk_hosting pluginterfaces)

set_target_properties_obs(obs-vst3 PROPERTIES FOLDER plugins PREFIX "")

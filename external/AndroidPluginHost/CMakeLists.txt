cmake_minimum_required(VERSION 3.18)
project(AndroidPluginHost)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external/JUCE)

juce_add_gui_app(AndroidPluginHost
        PRODUCT_NAME AndroidPluginHost
        VERSION "0.1.0"
        BUNDLE_ID "org.androidaudioplugin.simple_plugin_host"
        COMPANY_NAME "Audio Plugins For Android Project"
        COMPANY_WEBSITE "https://androidaudioplugin.org"
        # LAMESPEC: This never worked because...
        #NEEDS_CURL=0
        #NEEDS_WEB_BROWSER=0
)

# begin JUCEAAP specific -->
if (JUCEAAP_ENABLED)

include_directories(
        "${AAP_DIR}/include"
        "${JUCE_DIR}/modules"
)

# It is not beautiful, but there does not seem to be any way to add custom include directory for modules.
juce_add_module(${AAP_JUCE_DIR}/modules/juceaap_audio_processors)

# dependencies
find_package(androidaudioplugin REQUIRED CONFIG)

target_link_libraries(AndroidPluginHost
        PRIVATE
        androidaudioplugin::androidaudioplugin
        juceaap_audio_processors
        )

if (ANDROID)
find_library(log "log")
find_library(android "android")
find_library(glesv2 "GLESv2")
find_library(egl "EGL")
set(cpufeatures_lib "cpufeatures")
set(oboe_lib "oboe")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

target_link_libraries(AndroidPluginHost
        PRIVATE
        ${log}
        ${android}
        ${glesv2}
        ${egl}
        )
endif (ANDROID)

target_compile_definitions(AndroidPluginHost PUBLIC
        JUCEAAP_ENABLED=1
        JUCEAAP_HAVE_AUDIO_PLAYHEAD_NEW_POSITION_INFO=1
        JUCEAAP_USE_UNIQUE_ID=1
        JUCEAAP_HOSTED_PARAMETER=1
        # LAMESPEC: JuceActivity blindly calls appOnResume() which is actually generated ONLY IF  JUCE_PUSH_NOTIFICATIONS_ACTIVITY is specified.
        # What happens if not? java.lang.UnsatisfiedLinkError: No implementation found for void com.rmsl.juce.JuceActivity.appOnResume() !
        -DJUCE_PUSH_NOTIFICATIONS_ACTIVITY="com/rmsl/juce/JuceActivity"
        -DJUCE_PUSH_NOTIFICATIONS=1
        -DJUCE_USE_ANDROID_OBOE_STABILIZED_CALLBACK=1
        )

endif(JUCEAAP_ENABLED)
# <-- end JUCEAAP specific


target_compile_definitions(AndroidPluginHost PUBLIC
        # LAMESPEC: (contd.) ...because the documentation is wrong at all! These undocumented values are used
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_LV2=1
        )

target_sources(AndroidPluginHost PRIVATE
        main.cpp)

target_link_libraries(AndroidPluginHost PUBLIC
        juce::juce_data_structures
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_warning_flags
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

juce_generate_juce_header(AndroidPluginHost)

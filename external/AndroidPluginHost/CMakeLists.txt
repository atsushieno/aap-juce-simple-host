cmake_minimum_required(VERSION 3.22)

get_filename_component(app_source "${CMAKE_CURRENT_LIST_DIR}" REALPATH)
add_subdirectory("${app_source}/../aap-juce" "${CMAKE_CURRENT_BINARY_DIR}/aap-juce" EXCLUDE_FROM_ALL)
aap_juce_configure(JUCE_SOURCE_DIR external/JUCE MODE HOST
        EXTRA_JUCE_PATCHES "${app_source}/../../juce-modules.patch" ADOPT_EXISTING_PATCHES)
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

aap_juce_host(TARGET AndroidPluginHost)


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
        juce::juce_gui_extra
        juce::juce_recommended_warning_flags
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

juce_generate_juce_header(AndroidPluginHost)

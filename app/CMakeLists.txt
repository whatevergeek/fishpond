juce_add_gui_app(FishpondApp
    PRODUCT_NAME "Fishpond"
)

target_sources(FishpondApp PRIVATE
    Main.cpp
    MainComponent.cpp
    "${PROJECT_SOURCE_DIR}/src/AudioShell.cpp"
    "${PROJECT_SOURCE_DIR}/src/host/ControlledVST3Bass.cpp"
    "${PROJECT_SOURCE_DIR}/src/host/SingleChannelHost.cpp"
    "${PROJECT_SOURCE_DIR}/src/mixer/ChannelRegistry.cpp"
    "${PROJECT_SOURCE_DIR}/src/runtime/Runtime.cpp"
)

target_link_libraries(FishpondApp PRIVATE
    fishpond_build_config
    fishpond_embedded_python
    juce::juce_audio_utils
)

target_include_directories(FishpondApp PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
)

if(APPLE)
    set(FISHPOND_CONTROLLED_BASS_LOAD_PATH "$<TARGET_BUNDLE_DIR:FishpondFixtureInstrument_VST3>")
else()
    set(FISHPOND_CONTROLLED_BASS_LOAD_PATH "$<TARGET_FILE:FishpondFixtureInstrument_VST3>")
endif()

target_compile_definitions(FishpondApp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    FISHPOND_CONTROLLED_BASS_PATH="${FISHPOND_CONTROLLED_BASS_LOAD_PATH}"
)

add_dependencies(FishpondApp FishpondFixtureInstrument_VST3)

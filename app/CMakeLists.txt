juce_add_gui_app(FishpondApp
    PRODUCT_NAME "Fishpond"
)

target_sources(FishpondApp PRIVATE
    Main.cpp
    MainComponent.cpp
    "${PROJECT_SOURCE_DIR}/src/AudioShell.cpp"
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

target_compile_definitions(FishpondApp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

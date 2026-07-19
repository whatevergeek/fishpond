juce_add_gui_app(FishpondApp
    PRODUCT_NAME "Fishpond"
)

target_sources(FishpondApp PRIVATE
    FishpondApplication.cpp
)

target_link_libraries(FishpondApp PRIVATE
    fishpond_build_config
    juce::juce_audio_utils
)

target_compile_definitions(FishpondApp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

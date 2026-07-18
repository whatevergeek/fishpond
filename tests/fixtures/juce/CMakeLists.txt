# This target proves that the locked JUCE source can build a deterministic VST3
# instrument fixture. It is deliberately outside the offline P0 contract path.
juce_add_plugin(FishpondFixtureInstrument
    COMPANY_NAME Fishpond
    PLUGIN_MANUFACTURER_CODE Fshp
    PLUGIN_CODE Fpfi
    FORMATS VST3
    PRODUCT_NAME "Fishpond Fixture Instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FishpondFixtureInstrument PRIVATE FixtureInstrument.cpp)
target_link_libraries(FishpondFixtureInstrument PRIVATE juce::juce_audio_utils)

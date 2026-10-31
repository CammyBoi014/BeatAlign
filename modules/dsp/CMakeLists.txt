add_library(BeatAlign_DSP STATIC
        OnsetDetector.h
        OnsetDetector.cpp
)

target_include_directories(BeatAlign_DSP PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BeatAlign_DSP PUBLIC
        juce::juce_core
        juce::juce_dsp
        juce::juce_audio_basics
)

target_compile_features(BeatAlign_DSP PUBLIC cxx_std_20)
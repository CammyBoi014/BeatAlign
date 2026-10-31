juce_add_console_app(DSPTestRunner
        PRODUCT_NAME "DSPTestRunner"
)

target_sources(DSPTestRunner PRIVATE
        main.cpp
)

target_link_libraries(DSPTestRunner PRIVATE
        BeatAlign_DSP
        juce::juce_core
)

juce_generate_juce_header(DSPTestRunner)
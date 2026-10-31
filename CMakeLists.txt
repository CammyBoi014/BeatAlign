cmake_minimum_required(VERSION 3.22)
project(BeatAlign VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

add_subdirectory(modules/dsp)
add_subdirectory(tests)
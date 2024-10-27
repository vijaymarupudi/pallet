cmake_minimum_required(VERSION 3.30)

project(RT LANGUAGES CXX)

add_library(rtmidi STATIC RtMidi.cpp)
target_include_directories(rtmidi PUBLIC .)
target_compile_options(rtmidi PUBLIC -D__UNIX_JACK__)
target_link_libraries(rtmidi jack)

add_library(rtaudio STATIC RtAudio.cpp)
target_include_directories(rtaudio PUBLIC .)
target_compile_options(rtaudio PUBLIC -D__UNIX_JACK__)
target_link_libraries(rtaudio jack)

add_library(RT::RtMidi ALIAS rtmidi)
add_library(RT::RtAudio ALIAS rtaudio)

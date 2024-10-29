cmake_minimum_required(VERSION 3.13)

project(amy C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

add_library(miniaudio
            src/miniaudio.c)

target_include_directories(miniaudio PUBLIC src)
target_compile_definitions(miniaudio PUBLIC MA_ENABLE_ONLY_SPECIFIC_BACKENDS MA_ENABLE_JACK=1)

add_library(Miniaudio::Miniaudio ALIAS miniaudio)

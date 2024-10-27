cmake_minimum_required(VERSION 3.13)

project(amy C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

add_library(amy
            src/algorithms.c
            src/amy.c
            src/delay.c
            src/envelope.c
            src/custom.c
            src/filters.c
            src/patches.c
            src/log2_exp2.c
            src/oscillators.c
            src/partials.c
            src/pcm.c)

target_include_directories(amy PUBLIC src)

add_library(Amy::Amy ALIAS amy)

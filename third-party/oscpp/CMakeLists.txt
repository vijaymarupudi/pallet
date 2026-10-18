cmake_minimum_required(VERSION 3.24)
project(oscpp VERSION 1.0.0 LANGUAGES CXX)

add_library(oscpp INTERFACE)
add_library(oscpp::oscpp ALIAS oscpp)

target_include_directories(oscpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# the error handling uses std::expected
target_compile_features(oscpp INTERFACE cxx_std_23)

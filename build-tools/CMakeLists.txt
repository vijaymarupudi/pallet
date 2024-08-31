cmake_minimum_required(VERSION 3.30)

project(bytecode-compiler LANGUAGES CXX)

add_subdirectory(../lua "${CMAKE_BINARY_DIR}/lua")

add_executable(bytecode-compiler bytecode-compiler.cpp)
set_property(TARGET bytecode-compiler PROPERTY CXX_STANDARD 20)
target_link_libraries(bytecode-compiler Lua::Lua)

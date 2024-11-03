cmake_minimum_required(VERSION 3.30)

project(MyProject LANGUAGES CXX)

include(ExternalProject)

set(BYTECODE_COMPILER_PATH "${CMAKE_CURRENT_BINARY_DIR}/bytecode-compiler-prefix/src/bytecode-compiler-build/bytecode-compiler")

ExternalProject_Add(bytecode-compiler
                    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build-tools
                    CONFIGURE_COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/build-tools -DCMAKE_BUILD_TYPE=Release
                    BUILD_COMMAND make -j8
                    BUILD_BYPRODUCTS bytecode-compiler
                    INSTALL_COMMAND "")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(LO REQUIRED IMPORTED_TARGET liblo)

add_subdirectory(third-party/rt)
add_subdirectory(third-party/miniaudio)
add_subdirectory(third-party/lua)
add_subdirectory(third-party/amy)


file(GLOB FILESYSTEM_FILES filesystem/**/*)

add_custom_command(OUTPUT filesystem.h
                   COMMAND ${BYTECODE_COMPILER_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/filesystem ${CMAKE_CURRENT_BINARY_DIR}/filesystem.h
                   DEPENDS bytecode-compiler ${FILESYSTEM_FILES})

add_executable(main src/main.cpp src/LuaInterface.cpp src/Platform.cpp src/Clock.cpp filesystem.h)

set_property(TARGET main PROPERTY CXX_STANDARD 23)
target_link_libraries(main RT::RtMidi Miniaudio::Miniaudio Lua::Lua Amy::Amy PkgConfig::SDL2 PkgConfig::LO)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test src/test.cpp)

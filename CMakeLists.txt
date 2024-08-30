cmake_minimum_required(VERSION 3.30)

project(MyProject LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(LO REQUIRED IMPORTED_TARGET liblo)

add_subdirectory(rt)
add_subdirectory(lua)

add_executable(main main.cpp)
set_property(TARGET main PROPERTY CXX_STANDARD 23)
target_link_libraries(main RT::RtMidi Lua::Lua PkgConfig::SDL2 PkgConfig::LO)

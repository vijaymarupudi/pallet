cmake_minimum_required(VERSION 3.30)

project(MyProject LANGUAGES CXX)

include(ExternalProject)

set(BYTECODE_COMPILER_PATH "${CMAKE_CURRENT_BINARY_DIR}/bytecode-compiler-prefix/src/bytecode-compiler-build/bytecode-compiler")

ExternalProject_Add(bytecode-compiler
                    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build-tools
                    CONFIGURE_COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/build-tools -DCMAKE_BUILD_TYPE=Release
                    BUILD_COMMAND make -j4
                    BUILD_BYPRODUCTS ${BYTECODE_COMPILER_PATH} # ${CMAKE_CURRENT_BINARY_DIR}/bytecode-compiler
                    INSTALL_COMMAND "")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

add_subdirectory(third-party/rt)
add_subdirectory(third-party/miniaudio)
add_subdirectory(third-party/lua)
add_subdirectory(third-party/amy)
add_subdirectory(third-party/argparse)
add_subdirectory(third-party/oscpp)

file(GLOB FILESYSTEM_FILES filesystem/**/*)

add_custom_command(OUTPUT filesystem.h
                   COMMAND ${BYTECODE_COMPILER_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/filesystem ${CMAKE_CURRENT_BINARY_DIR}/filesystem.h
                   DEPENDS bytecode-compiler ${FILESYSTEM_FILES})

add_custom_target (PalletLuaFilesystem DEPENDS filesystem.h)

# cmake -DPALLET_SANITIZE=ON instruments everything below, including the tests
option(PALLET_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if (PALLET_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

add_compile_options(-Wall -Wextra -pedantic)

# add_compile_options(-Wall -Wextra -pedantic -pg)
# add_link_options(-pg)


set(CXX_FILES
  lib/LuaInterface.cpp
  lib/LuaInterface/LuaGraphicsInterface.cpp
  lib/LuaInterface/LuaGridInterface.cpp
  lib/LuaInterface/LuaClockInterface.cpp
  lib/LuaInterface/LuaBeatClockInterface.cpp
  lib/LuaInterface/LuaMidiInterface.cpp
  lib/LuaInterface/LuaOscInterface.cpp
  lib/PosixPlatform.cpp
  lib/LinuxPlatform.cpp
  lib/Clock.cpp
  lib/MidiInterface.cpp
  lib/BeatClockScheduler.cpp
  lib/GraphicsInterface.cpp
  lib/PosixGraphicsInterface.cpp
  lib/BeatClock.cpp
  lib/PosixOscInterface.cpp
  lib/OscMonomeGridInterface.cpp
  lib/io.cpp
  )

add_library(pallet STATIC ${CXX_FILES})
set_property(TARGET pallet PROPERTY CXX_STANDARD 23)
target_link_libraries(pallet PUBLIC RT::RtMidi Miniaudio::Miniaudio Lua::Lua Amy::Amy PkgConfig::SDL3)
target_link_libraries(pallet PRIVATE oscpp::oscpp)
target_include_directories(pallet PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(pallet PUBLIC include)
target_compile_options(pallet PRIVATE -fno-exceptions)
add_dependencies(pallet PalletLuaFilesystem)

if (PROJECT_IS_TOP_LEVEL)

add_executable(main src/main.cpp)
set_property(TARGET main PROPERTY CXX_STANDARD 23)
target_link_libraries(main pallet)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_executable(cli src/cli.cpp src/cliMain.cpp)
set_property(TARGET cli PROPERTY CXX_STANDARD 23)
target_link_libraries(cli pallet argparse::argparse)
target_include_directories(cli PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# a scratch program, not one of the tests (CMake reserves the target name
# "test" once testing is enabled)
add_executable(scratch src/test.cpp)
target_link_libraries(scratch pallet)
target_include_directories(scratch PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
# set_property(TARGET scratch PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set_property(TARGET scratch PROPERTY CXX_STANDARD 23)

add_executable(measure-timing-latency
               src/measure-timing-latency.cpp)
target_link_libraries(measure-timing-latency pallet)
set_property(TARGET measure-timing-latency PROPERTY CXX_STANDARD 23)

enable_testing()
add_subdirectory(tests)

endif()

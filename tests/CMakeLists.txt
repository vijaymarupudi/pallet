# Run with: ctest --test-dir <build dir> --output-on-failure
# The tests use doctest (third-party/doctest) and CHECK/REQUIRE, so they keep
# working in release builds, where assert() is compiled out.

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/doctest)

# the vendored OSC library, which must not throw
add_executable(test_oscpp oscpp.cpp)
set_property(TARGET test_oscpp PROPERTY CXX_STANDARD 23)
target_link_libraries(test_oscpp PRIVATE oscpp::oscpp doctest)
target_compile_options(test_oscpp PRIVATE -fno-exceptions)
add_test(NAME oscpp COMMAND test_oscpp)

# OSC over real loopback sockets
add_executable(test_osc_interface osc_interface.cpp)
set_property(TARGET test_osc_interface PROPERTY CXX_STANDARD 23)
target_link_libraries(test_osc_interface PRIVATE pallet doctest)
add_test(NAME osc_interface COMMAND test_osc_interface)

# the serialosc grid code against a fake OscInterface
add_executable(test_grid grid.cpp)
set_property(TARGET test_grid PROPERTY CXX_STANDARD 23)
target_link_libraries(test_grid PRIVATE pallet doctest)
add_test(NAME grid COMMAND test_grid)

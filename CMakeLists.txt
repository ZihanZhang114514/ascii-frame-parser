cmake_minimum_required(VERSION 3.10)
project(ascii_frame_parser VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(sci_ascii STATIC
    src/sci_parse.c
    src/sci_rx.c
)
target_include_directories(sci_ascii PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(NOT MSVC)
    target_compile_options(sci_ascii PRIVATE -Wall -Wextra)
endif()

option(SCI_BUILD_TESTS    "Build the unit tests" ON)
option(SCI_BUILD_EXAMPLES "Build the PC demo"   ON)

if(SCI_BUILD_EXAMPLES)
    add_executable(pc_demo examples/pc_demo.c)
    target_link_libraries(pc_demo PRIVATE sci_ascii)
endif()

if(SCI_BUILD_TESTS)
    enable_testing()
    add_executable(test_sci_parse tests/test_sci_parse.c)
    target_link_libraries(test_sci_parse PRIVATE sci_ascii)
    if(UNIX)
        target_link_libraries(test_sci_parse PRIVATE m)
    endif()
    add_test(NAME test_sci_parse COMMAND test_sci_parse)
endif()

cmake_minimum_required(VERSION 3.16)

# UTF-8 source
add_compile_options("$<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:__cplusplus>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")

project(dummy)

set(TPSTUB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

option(THREEPP_BUILD_EXAMPLES "Build examples" OFF)
option(THREEPP_BUILD_TESTS "Build test suite" OFF)
option(THREEPP_WITH_SVG "Build with SVGLoader" ON)
option(THREEPP_WITH_AUDIO "Build with Audio" OFF)
option(THREEPP_WITH_GENERIC "Build with Generic frontend (user-provided interface)" ON)

add_subdirectory(threepp)

krkrz_plugin(krkr_threepp
    NCBIND
    SOURCES
        main.cpp
    LIBRARIES
        threepp
)
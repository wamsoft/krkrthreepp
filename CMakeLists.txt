cmake_minimum_required(VERSION 3.16)

# UTF-8 source
add_compile_options("$<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:__cplusplus>")

# MSVC static runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(PROJECT_NAME threepp)
set(PROJECT_VERSION 1.00)

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION})

if(NOT TARGET ncbind)
add_subdirectory(../ncbind ${CMAKE_CURRENT_BINARY_DIR}/ncbind)
endif()

option(THREEPP_BUILD_EXAMPLES "Build examples" OFF)
option(THREEPP_BUILD_TESTS "Build test suite" OFF)
option(THREEPP_WITH_SVG "Build with SVGLoader" ON)
option(THREEPP_WITH_AUDIO "Build with Audio" OFF)
option(THREEPP_WITH_GENERIC "Build with Generic frontend (user-provided interface)" ON)

add_subdirectory(threepp ${CMAKE_CURRENT_BINARY_DIR}/threepp)

add_library(${PROJECT_NAME} SHARED
	main.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ncbind
    threepp
)

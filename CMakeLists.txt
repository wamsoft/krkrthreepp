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

add_subdirectory(external/threepp)

# VRM ロード検証用スタンドアロン exe (GLES/kirikiri 不要)。既定 OFF。
option(BUILD_VRM_SMOKE_TEST "Build standalone VRM load smoke test" OFF)
if(BUILD_VRM_SMOKE_TEST)
    add_executable(vrm_smoke test/vrm_smoke.cpp src/vrm/VRM.cpp)
    target_include_directories(vrm_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vrm
        ${CMAKE_CURRENT_SOURCE_DIR}/external/VRM/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/threepp/src/external/nlohmann)
    target_compile_definitions(vrm_smoke PRIVATE USE_VRMC_VRM_1_0)
    target_link_libraries(vrm_smoke PRIVATE threepp)
endif()

krkrz_plugin(krkrthreepp
    NCBIND
    SOURCES
        src/main.cpp
        src/LicensesGen.cpp
        src/vrm/VRM.cpp
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vrm
        ${CMAKE_CURRENT_SOURCE_DIR}/external/VRM/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/threepp/src/external/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}/external/threepp/src/external/glad
    DEFINITIONS
        USE_VRMC_VRM_1_0
    LIBRARIES
        threepp
)
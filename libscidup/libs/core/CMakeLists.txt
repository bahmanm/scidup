cmake_minimum_required( VERSION 3.23 )

project(
    libscidup-core
    LANGUAGES CXX )

set(
    SCIDUP_CORE_PUBLIC_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/board_def.h" )

add_library(
    scidup_core
    INTERFACE )
add_library(
    ScidUp::Core
    ALIAS scidup_core )
target_sources(
    scidup_core
    INTERFACE
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
        FILES ${SCIDUP_CORE_PUBLIC_HEADERS} )
target_compile_features(
    scidup_core
    INTERFACE cxx_std_20 )
target_include_directories(
    scidup_core
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" )

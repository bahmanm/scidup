cmake_minimum_required( VERSION 3.23 )

project(
    libscidup
    LANGUAGES CXX )

if( PROJECT_IS_TOP_LEVEL )
    if( NOT DEFINED BUILD_TESTING )
        set( BUILD_TESTING OFF CACHE BOOL "Build tests" )
    endif()
    include( CTest )
endif()

add_subdirectory( libs/core )

add_library(
    scidup_eco_code
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/eco/src/code.cpp" )
add_library(
    ScidUp::EcoCode
    ALIAS scidup_eco_code )
target_sources(
    scidup_eco_code
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/libs/eco/include"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/libs/eco/include/scidup/eco/code.h" )
target_compile_features(
    scidup_eco_code
    PUBLIC cxx_std_20 )
target_include_directories(
    scidup_eco_code
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libs/eco/include" )

add_subdirectory( libs/database )
add_subdirectory( libs/eco )
add_subdirectory( libs/spelling )

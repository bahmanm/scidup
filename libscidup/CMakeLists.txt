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

add_subdirectory( libs/database )
add_subdirectory( libs/eco )
add_subdirectory( libs/spelling )

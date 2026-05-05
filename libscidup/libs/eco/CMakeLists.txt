cmake_minimum_required( VERSION 3.23 )

project(
    libscidup-eco
    LANGUAGES CXX )

set(
    SCIDUP_ECO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/book.cpp" )

set(
    SCIDUP_ECO_PUBLIC_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/eco/book.h" )

add_library(
    scidup_eco
    ${SCIDUP_ECO_SOURCES} )
add_library(
    ScidUp::Eco
    ALIAS scidup_eco )
target_sources(
    scidup_eco
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
        FILES ${SCIDUP_ECO_PUBLIC_HEADERS} )
target_compile_features(
    scidup_eco
    PUBLIC cxx_std_20 )
target_include_directories(
    scidup_eco
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_link_libraries(
    scidup_eco
    PUBLIC ScidUp::Database )

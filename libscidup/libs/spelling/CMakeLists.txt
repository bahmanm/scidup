cmake_minimum_required( VERSION 3.23 )

project(
    libscidup-spelling
    LANGUAGES CXX )

set(
    SCIDUP_SPELLING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/spellchk.cpp" )

set(
    SCIDUP_SPELLING_PUBLIC_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/spelling/spellchk.h" )

add_library(
    scidup_spelling
    ${SCIDUP_SPELLING_SOURCES} )
add_library(
    ScidUp::Spelling
    ALIAS scidup_spelling )
target_sources(
    scidup_spelling
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
        FILES ${SCIDUP_SPELLING_PUBLIC_HEADERS} )
target_compile_features(
    scidup_spelling
    PUBLIC cxx_std_20 )
target_include_directories(
    scidup_spelling
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_link_libraries(
    scidup_spelling
    PUBLIC ScidUp::Database )

option( SPELLCHKVALIDATE "Verify the integrity of spelling files" OFF )
if( SPELLCHKVALIDATE )
    target_compile_definitions( scidup_spelling PRIVATE SPELLCHKVALIDATE )
endif()

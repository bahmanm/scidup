cmake_minimum_required( VERSION 3.23 )

project(
    libscidup-core
    LANGUAGES CXX )

set(
    SCIDUP_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_cursor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/movetext_cursor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/movetext_location.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nags.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/notation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pgn_decode.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pgn_traversal.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/position.cpp" )

set(
    SCIDUP_CORE_PUBLIC_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/attacks.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/board.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/date.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/dstring.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/error.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/fullmove.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/game.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/game_cursor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/movetext_cursor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/movetext_location.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/game_result.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/hash.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/movelist.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/move_predicates.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/nags.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/notation.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/pgn/decode.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/pgn/encode.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/pgn/traversal.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/position.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/primitives.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/rating.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/square_collections.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/core/square_moves.h" )

add_library(
    scidup_core
    ${SCIDUP_CORE_SOURCES} )
add_library(
    ScidUp::Core
    ALIAS scidup_core )
target_sources(
    scidup_core
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
        FILES ${SCIDUP_CORE_PUBLIC_HEADERS} )
target_compile_features(
    scidup_core
    PUBLIC cxx_std_20 )
target_include_directories(
    scidup_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/internal" )

if( PROJECT_IS_TOP_LEVEL )
    include( CTest )
endif()

if( BUILD_TESTING )
    if( NOT TARGET gtest_main )
        include( FetchContent )
        set( INSTALL_GTEST OFF CACHE BOOL "Disable installation rules for googletest." FORCE )
        set( BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock; core tests use GoogleTest only." FORCE )
        set( gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE )

        fetchcontent_declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c )
        fetchcontent_makeavailable( googletest )
    endif()

    file(
        GLOB SCIDUP_CORE_TEST_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cxx/*.cpp" )
    add_executable(
        scidup_tests_core_cpp
        ${SCIDUP_CORE_TEST_SOURCES} )
    target_include_directories(
        scidup_tests_core_cpp
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/internal" )
    target_link_libraries(
        scidup_tests_core_cpp
        PRIVATE ScidUp::Core gtest_main )
    add_executable(
        ScidUp::Tests::Core::Cpp
        ALIAS scidup_tests_core_cpp )

    add_test(
        NAME core_cpp_test
        COMMAND $<TARGET_FILE:ScidUp::Tests::Core::Cpp> )
    set_tests_properties(
        core_cpp_test
        PROPERTIES LABELS "cpp;core" )
endif()

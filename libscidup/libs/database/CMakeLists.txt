cmake_minimum_required( VERSION 3.23 )

project(
    libscidup-database
    LANGUAGES CXX )

find_package( Threads REQUIRED )

set(
    SCIDUP_DATABASE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/codec_scid4.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/filter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_search.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_storage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_annotations.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_lifecycle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_move_tree.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_state.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/index.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_pgn.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/matsig.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/misc.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nag_format.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/piece_translation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pgnparse.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scidbase.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/searchpos.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/searchindex.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sortcache.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/stored.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/textbuf.cpp" )

set(
    SCIDUP_DATABASE_PUBLIC_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/bytebuf.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/common.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/game.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/game_id.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/gameview.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/hfilter.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/index.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/indexentry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/matsig.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/misc.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/namebase.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/pgnparse.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/scidbase.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/searchpos.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scidup/database/tree.h" )

set(
    SCIDUP_DATABASE_PRIVATE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/codec.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/codec_memory.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/codec_pgn.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/codec_proxy.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/codec_scid4.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/codec_scid5.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/containers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/filebuf.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/game_positions.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/game_search.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/game_storage.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/legacy_encode_options.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/legacy_pgn.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/movetree.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/nag_format.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/naglatex.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/nagtext.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/piece_translation.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/pgn_lexer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/pgnparse_impl.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/sortcache.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/stored.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/tree_cache.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/textbuf.h" )

add_library(
    scidup_database
    ${SCIDUP_DATABASE_SOURCES} )
add_library(
    ScidUp::Database
    ALIAS scidup_database )
target_sources(
    scidup_database
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
        FILES ${SCIDUP_DATABASE_PUBLIC_HEADERS}
    PRIVATE
        ${SCIDUP_DATABASE_PRIVATE_HEADERS} )
target_compile_features(
    scidup_database
    PUBLIC cxx_std_20 )
target_include_directories(
    scidup_database
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/internal" )
target_link_libraries(
    scidup_database
    PUBLIC ScidUp::Core ScidUp::Eco Threads::Threads )

if( PROJECT_IS_TOP_LEVEL )
    include( CTest )
endif()

if( BUILD_TESTING )
    if( NOT TARGET gtest_main )
        include( FetchContent )
        set( INSTALL_GTEST OFF CACHE BOOL "Disable installation rules for googletest." FORCE )
        set( BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock; database tests use GoogleTest only." FORCE )
        set( gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE )

        fetchcontent_declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c )
        fetchcontent_makeavailable( googletest )
    endif()

    file(
        GLOB SCIDUP_DATABASE_TEST_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cxx/*.cpp" )
    add_executable(
        scidup_tests_database_cpp
        ${SCIDUP_DATABASE_TEST_SOURCES} )
    target_compile_definitions(
        scidup_tests_database_cpp
        PRIVATE SCIDUP_TEST_RESOURCES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/cxx/\" )
    target_include_directories(
        scidup_tests_database_cpp
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/internal" )
    target_link_libraries(
        scidup_tests_database_cpp
        PRIVATE ScidUp::Database gtest_main )
    add_executable(
        ScidUp::Tests::Database::Cpp
        ALIAS scidup_tests_database_cpp )

    add_test(
        NAME database_cpp_test
        COMMAND $<TARGET_FILE:ScidUp::Tests::Database::Cpp> )
    set_tests_properties(
        database_cpp_test
        PROPERTIES LABELS "cpp;database" )
endif()

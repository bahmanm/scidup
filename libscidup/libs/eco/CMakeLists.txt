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

if( PROJECT_IS_TOP_LEVEL )
    include( CTest )
endif()

if( BUILD_TESTING )
    if( NOT TARGET gtest_main )
        include( FetchContent )
        set( INSTALL_GTEST OFF CACHE BOOL "Disable installation rules for googletest." FORCE )
        set( BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock; ECO tests use GoogleTest only." FORCE )
        set( gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE )

        fetchcontent_declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c )
        fetchcontent_makeavailable( googletest )
    endif()

    file(
        GLOB SCIDUP_ECO_TEST_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cxx/*.cpp" )
    add_executable(
        scidup_tests_eco_cpp
        ${SCIDUP_ECO_TEST_SOURCES} )
    target_link_libraries(
        scidup_tests_eco_cpp
        PRIVATE ScidUp::Eco gtest_main )
    add_executable(
        ScidUp::Tests::Eco::Cpp
        ALIAS scidup_tests_eco_cpp )

    add_test(
        NAME eco_cpp_test
        COMMAND $<TARGET_FILE:ScidUp::Tests::Eco::Cpp> )
    set_tests_properties(
        eco_cpp_test
        PROPERTIES LABELS "cpp;eco" )
endif()

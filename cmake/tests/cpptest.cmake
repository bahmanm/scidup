include( cmake/libs/threads.cmake )

include( FetchContent )
set( INSTALL_GTEST OFF CACHE BOOL "Disable installation rules for googletest." FORCE )
set( BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock (ScidUp tests use GoogleTest only)." FORCE )
fetchcontent_declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c )
fetchcontent_makeavailable( googletest )

set( gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE )

set(
    SCIDUP_TESTS_LIBS_CPPSUPPORT_SOURCES
    "${CMAKE_SOURCE_DIR}/src/cxx/dbasepool.cpp"
)
add_library(
    scidup_tests_libs_cppsupport
    ${SCIDUP_TESTS_LIBS_CPPSUPPORT_SOURCES} )
target_include_directories(
    scidup_tests_libs_cppsupport
    PUBLIC "${CMAKE_SOURCE_DIR}/src/cxx" )
target_link_libraries(
    scidup_tests_libs_cppsupport
    PUBLIC ScidUp::Libs::Database Threads::Threads )
add_library(
    ScidUp::Tests::Libs::CppBase
    ALIAS scidup_tests_libs_cppsupport )

file(
    GLOB SCIDUP_TESTS_BINS_CPPTEST_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/cxx/*.cpp" )
add_executable(
    scidup_tests_bins_cpptest
    ${SCIDUP_TESTS_BINS_CPPTEST_SOURCES} )
target_compile_definitions(
    scidup_tests_bins_cpptest
    PRIVATE SCIDUP_TEST_RESOURCES_DIR=\"${CMAKE_SOURCE_DIR}/tests/cxx/\" )
target_link_libraries(
    scidup_tests_bins_cpptest
    PRIVATE ScidUp::Tests::Libs::CppBase gtest_main )
add_executable(
    ScidUp::Tests::Bins::CppTest
    ALIAS scidup_tests_bins_cpptest )

add_test(
    NAME cpp_test
    COMMAND $<TARGET_FILE:ScidUp::Tests::Bins::CppTest> )
set_tests_properties(
    cpp_test
    PROPERTIES LABELS "cpp" )

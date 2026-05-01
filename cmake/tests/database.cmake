include( cmake/tests/gtest.cmake )

file(
    GLOB SCIDUP_TESTS_DATABASE_CPP_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/database/cxx/*.cpp" )
add_executable(
    scidup_tests_database_cpp
    ${SCIDUP_TESTS_DATABASE_CPP_SOURCES} )
target_compile_definitions(
    scidup_tests_database_cpp
    PRIVATE SCIDUP_TEST_RESOURCES_DIR=\"${CMAKE_SOURCE_DIR}/tests/database/cxx/\" )
target_include_directories(
    scidup_tests_database_cpp
    PRIVATE "${CMAKE_SOURCE_DIR}/src/database/private" )
target_link_libraries(
    scidup_tests_database_cpp
    PRIVATE ScidUp::Libs::Database gtest_main )
add_executable(
    ScidUp::Tests::Database::Cpp
    ALIAS scidup_tests_database_cpp )

add_test(
    NAME database_cpp_test
    COMMAND $<TARGET_FILE:ScidUp::Tests::Database::Cpp> )
set_tests_properties(
    database_cpp_test
    PROPERTIES LABELS "cpp;database" )

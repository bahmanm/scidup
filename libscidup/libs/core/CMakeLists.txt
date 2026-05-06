cmake_minimum_required( VERSION 3.23 )

project(
    libscidup-core
    LANGUAGES CXX )

add_library(
    scidup_core
    INTERFACE )
add_library(
    ScidUp::Core
    ALIAS scidup_core )
target_compile_features(
    scidup_core
    INTERFACE cxx_std_20 )
target_include_directories(
    scidup_core
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" )

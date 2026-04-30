set(
    SCIDUP_LIBS_DATABASE_SOURCES
    "${CMAKE_SOURCE_DIR}/src/cxx/codec_scid4.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/filter.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/game.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/matsig.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/misc.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/position.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/scidbase.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/searchindex.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/sortcache.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/stored.cpp"
    "${CMAKE_SOURCE_DIR}/src/cxx/textbuf.cpp"
)

add_library(
    scidup_libs_database
    ${SCIDUP_LIBS_DATABASE_SOURCES} )
add_library(
    ScidUp::Libs::Database
    ALIAS scidup_libs_database )
target_include_directories(
    scidup_libs_database
    PUBLIC "${CMAKE_SOURCE_DIR}/src/cxx" )
target_link_libraries(
    scidup_libs_database
    PUBLIC Threads::Threads )

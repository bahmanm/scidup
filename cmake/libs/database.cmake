set(
    SCIDUP_LIBS_DATABASE_SOURCES
    "${CMAKE_SOURCE_DIR}/src/database/src/codec_scid4.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/filter.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/game.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/matsig.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/misc.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/position.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/scidbase.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/searchindex.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/sortcache.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/stored.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/src/textbuf.cpp"
)

set(
    SCIDUP_LIBS_DATABASE_PUBLIC_HEADERS
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/board_def.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/attacks.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/bytebuf.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/codec.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/common.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/containers.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/date.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/dstring.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/error.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/filebuf.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/fullmove.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/game.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/gameview.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/hash.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/hfilter.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/index.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/indexentry.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/matsig.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/misc.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/movegen.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/movelist.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/movetree.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/namebase.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/pgn_encode.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/pgn_lexer.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/pgnparse.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/position.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/scidbase.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/sqmove.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/stored.h"
    "${CMAKE_SOURCE_DIR}/src/database/include/scidup/database/tree.h"
)

set(
    SCIDUP_LIBS_DATABASE_PRIVATE_HEADERS
    "${CMAKE_SOURCE_DIR}/src/database/private/codec_memory.h"
    "${CMAKE_SOURCE_DIR}/src/database/private/codec_pgn.h"
    "${CMAKE_SOURCE_DIR}/src/database/private/codec_proxy.h"
    "${CMAKE_SOURCE_DIR}/src/database/private/codec_scid4.h"
    "${CMAKE_SOURCE_DIR}/src/database/private/codec_scid5.h"
    "${CMAKE_SOURCE_DIR}/src/database/private/naglatex.h"
    "${CMAKE_SOURCE_DIR}/src/database/private/nagtext.h"
    "${CMAKE_SOURCE_DIR}/src/database/private/sortcache.h"
    "${CMAKE_SOURCE_DIR}/src/database/private/textbuf.h"
)

add_library(
    scidup_libs_database
    ${SCIDUP_LIBS_DATABASE_SOURCES} )
add_library(
    ScidUp::Libs::Database
    ALIAS scidup_libs_database )
target_sources(
    scidup_libs_database
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_SOURCE_DIR}/src/database/include"
        FILES ${SCIDUP_LIBS_DATABASE_PUBLIC_HEADERS}
    PRIVATE
        ${SCIDUP_LIBS_DATABASE_PRIVATE_HEADERS} )
target_include_directories(
    scidup_libs_database
    PUBLIC "${CMAKE_SOURCE_DIR}/src/database/include"
    PRIVATE "${CMAKE_SOURCE_DIR}/src/database/private" )
target_link_libraries(
    scidup_libs_database
    PUBLIC Threads::Threads )

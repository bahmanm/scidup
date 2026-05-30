file(
    GLOB SCIDUP_MAIN_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/cxx/*.h"
    "${CMAKE_SOURCE_DIR}/src/cxx/*.cpp" )

	if( MSVC )
	    add_executable(
	        scidup_main
	        WIN32
	        ${SCIDUP_MAIN_SOURCES}
	        "${CMAKE_SOURCE_DIR}/resources/win/scidup.rc"
	        "${CMAKE_SOURCE_DIR}/resources/win/scid.manifest" )
	    target_link_options(
	        scidup_main
	        PRIVATE /ENTRY:mainCRTStartup )
    target_compile_definitions(
        scidup_main
        PRIVATE _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS )
else()
    add_executable(
        scidup_main
        ${SCIDUP_MAIN_SOURCES} )
endif()

add_executable(
    ScidUp::Bins::Main
    ALIAS scidup_main )
set_target_properties(
    scidup_main
    PROPERTIES OUTPUT_NAME "scidup" )

if( SCIDUP_PORTABLE_ARCHIVE )
    target_compile_definitions(
        scidup_main
        PRIVATE SCIDUP_PORTABLE_ARCHIVE_MODE=1 )
endif()

if( SCIDUP_PORTABLE_ARCHIVE AND UNIX AND NOT WIN32 )
    file(
        RELATIVE_PATH
        _scidup_relative_library_directory
        "${CMAKE_INSTALL_FULL_BINDIR}"
        "${CMAKE_INSTALL_FULL_LIBDIR}" )

    if( _scidup_relative_library_directory STREQUAL "." )
        set( _scidup_relative_library_directory "" )
    endif()

    if( APPLE )
        set( _scidup_install_rpath "@loader_path/${_scidup_relative_library_directory}" )
    else()
        set( _scidup_install_rpath "$ORIGIN/${_scidup_relative_library_directory}" )
    endif()

    set_target_properties(
        scidup_main
        PROPERTIES INSTALL_RPATH "${_scidup_install_rpath}" )
endif()

set_property(
    TARGET scidup_main
    PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE True )
target_include_directories(
    scidup_main
    PRIVATE "${SCIDUP_GENERATED_INCLUDE_DIR}" )
target_link_libraries(
    scidup_main
    PRIVATE ScidUp::Database ScidUp::Eco ScidUp::Spelling ScidUp::Libs::Polyglot Threads::Threads ScidUp::Libs::Tcl )

set( CPACK_PACKAGE_NAME "scidup" )
if( SCIDUP_PORTABLE_ARCHIVE )
    if( NOT DEFINED SCIDUP_RELEASE_VERSION OR SCIDUP_RELEASE_VERSION STREQUAL "" )
        message(
            FATAL_ERROR
            "SCIDUP_PORTABLE_ARCHIVE is enabled, but SCIDUP_RELEASE_VERSION is empty.\n" )
    endif()

    if( NOT DEFINED SCIDUP_RELEASE_DATE OR SCIDUP_RELEASE_DATE STREQUAL "" )
        message(
            FATAL_ERROR
            "SCIDUP_PORTABLE_ARCHIVE is enabled, but SCIDUP_RELEASE_DATE is empty.\n" )
    endif()

    if( NOT DEFINED SCIDUP_RELEASE_PLATFORM OR SCIDUP_RELEASE_PLATFORM STREQUAL "" )
        message(
            FATAL_ERROR
            "SCIDUP_PORTABLE_ARCHIVE is enabled, but SCIDUP_RELEASE_PLATFORM is empty.\n" )
    endif()

    set( CPACK_PACKAGE_VERSION "${SCIDUP_RELEASE_VERSION}" )
else()
    set( CPACK_PACKAGE_VERSION "0" )
endif()
set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-Platform Chess Database and Analysis GUI" )

include( CPack )

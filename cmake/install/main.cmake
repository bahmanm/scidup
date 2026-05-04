install(
    TARGETS scidup_main
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" )

set(
    SCIDUP_DATA_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/scidup" )

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/resources/books"
    DESTINATION "${SCIDUP_DATA_INSTALL_DIR}" )
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/resources/html"
    DESTINATION "${SCIDUP_DATA_INSTALL_DIR}" )
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/resources/help"
    DESTINATION "${SCIDUP_DATA_INSTALL_DIR}" )
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/resources/images"
    DESTINATION "${SCIDUP_DATA_INSTALL_DIR}" )
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/resources/scripts"
    DESTINATION "${SCIDUP_DATA_INSTALL_DIR}" )
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/resources/sounds"
    DESTINATION "${SCIDUP_DATA_INSTALL_DIR}" )
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/src/tcl"
    DESTINATION "${SCIDUP_DATA_INSTALL_DIR}" )

if( WIN32 )
    install(
        FILES "${CMAKE_SOURCE_DIR}/src/tools/check-newer-release/script.ps1"
        DESTINATION "${SCIDUP_DATA_INSTALL_DIR}/tools"
        RENAME "check-newer-release.ps1" )
else()
    install(
        PROGRAMS "${CMAKE_SOURCE_DIR}/src/tools/check-newer-release/script.sh"
        DESTINATION "${SCIDUP_DATA_INSTALL_DIR}/tools"
        RENAME "check-newer-release" )
endif()

file(
    GLOB ECO_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/resources/eco/*.eco" )
install(
    FILES ${ECO_FILES}
    DESTINATION "${SCIDUP_DATA_INSTALL_DIR}" )

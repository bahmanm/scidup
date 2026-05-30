set( SCIDUP_LIBSCID_VERSION "0.1.0" CACHE STRING "Required libscid package version." )

find_package( LibScid "${SCIDUP_LIBSCID_VERSION}" EXACT CONFIG REQUIRED )

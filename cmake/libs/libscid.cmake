set( SCIDUP_LIBSCID_CPP_VERSION "0.3.0" CACHE STRING "Required libscid-cpp package version." )

find_package( libscid-cpp "${SCIDUP_LIBSCID_CPP_VERSION}" EXACT CONFIG REQUIRED )

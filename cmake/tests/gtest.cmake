include( FetchContent )

if( TARGET gtest_main )
    return()
endif()

set( INSTALL_GTEST OFF CACHE BOOL "Disable installation rules for googletest." FORCE )
set( BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock; ScidUp tests use GoogleTest only." FORCE )
set( gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE )

fetchcontent_declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c )
fetchcontent_makeavailable( googletest )

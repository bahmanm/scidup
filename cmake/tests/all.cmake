if( NOT BUILD_TESTING )
    return()
endif()

include( cmake/tests/cpptest.cmake )
include( cmake/tests/tcl/bridge.cmake )
include( cmake/tests/tcl/unit.cmake )
include( cmake/tests/tcl/gui.cmake )
include( cmake/tests/portable.cmake )

namespace eval ::scidup {}
namespace eval ::scidup::dirs {}

proc ::scidup::dirs::homeDir {} {
    # Tcl does not expand "~" in file paths; use `glob` to resolve the home directory.
    return [file normalize [glob ~]]
}

proc ::scidup::dirs::configRoot {{platform ""} {os ""} {exeDir ""}} {
    if {$platform eq ""} {
        set platform $::tcl_platform(platform)
    }
    if {$os eq ""} {
        set os $::tcl_platform(os)
    }
    if {$exeDir eq ""} {
        set exeDir [file dirname [info nameofexecutable]]
    }

    set configHome ""
    if {[info exists ::env(SCIDUP_CONFIG_HOME)] && $::env(SCIDUP_CONFIG_HOME) ne ""} {
        set configHome $::env(SCIDUP_CONFIG_HOME)
    } elseif {[info exists ::env(XDG_CONFIG_HOME)] && $::env(XDG_CONFIG_HOME) ne ""} {
        set configHome [file join $::env(XDG_CONFIG_HOME) scidup]
    }

    if {$configHome ne ""} {
        return [file nativename $configHome]
    }

    if {$platform eq "windows"} {
        if {[info exists ::env(APPDATA)] && $::env(APPDATA) ne ""} {
            return [file nativename [file join $::env(APPDATA) "scidup"]]
        }
        return [file nativename $exeDir]
    }

    if {$os eq "Darwin"} {
        return [file nativename [file join [::scidup::dirs::homeDir] "Library" "Application Support" "scidup"]]
    }

    return [file nativename [file join [::scidup::dirs::homeDir] ".config" "scidup"]]
}

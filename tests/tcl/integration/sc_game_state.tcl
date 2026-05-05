proc progressCallBack {done {msg ""}} {
    return 1
}

proc assertEq {expected actual message} {
    if {$expected ne $actual} {
        error "$message\nexpected: <$expected>\nactual:   <$actual>"
    }
}

set root [file normalize [file join [pwd] tests tcl _tmp sc_game_state__[pid]__[clock clicks -milliseconds]]]
file mkdir $root

set exitCode 0

try {
    set basePath [file join [pwd] libscidup libs database tests cxx res_database]
    set base [sc_base open SCID4 $basePath]

    sc_game load 1
    assertEq 1 [sc_game number] "loaded game should become the active game"
    assertEq 0 [sc_game altered] "loading a game should clear altered state"
    assertEq 0 [sc_game undo size] "loading a game should clear undo history"

    set saved [sc_base getGame $base 1]
    set liveInitial [sc_base getGame $base 1 live]
    assertEq 1 [expr {$saved eq $liveInitial}] "live view should match stored game before edits"

    sc_game undoPoint
    assertEq 1 [sc_game undo size] "undoPoint should create one undo entry"

    sc_game truncate
    assertEq 1 [sc_game altered] "editing should mark the active game altered"
    set liveEdited [sc_base getGame $base 1 live]
    assertEq 0 [expr {$saved eq $liveEdited}] "live view should diverge after editing"

    sc_game undo
    set liveUndone [sc_base getGame $base 1 live]
    assertEq 1 [expr {$saved eq $liveUndone}] "undo should restore the live game"
    assertEq 1 [sc_game altered] "undo should preserve altered state until explicitly reset"

    sc_game undoAll
    assertEq 0 [sc_game altered] "undoAll should clear altered state"
    set liveReset [sc_base getGame $base 1 live]
    assertEq 1 [expr {$saved eq $liveReset}] "undoAll should restore the stored game"

    sc_game push copy
    sc_game truncate
    set pushedLive [sc_base getGame $base 1 live]
    assertEq 0 [expr {$saved eq $pushedLive}] "push copy should isolate a mutable temporary game"
    sc_game pop
    set poppedLive [sc_base getGame $base 1 live]
    assertEq 1 [expr {$saved eq $poppedLive}] "pop should restore the pushed game state"
    assertEq 0 [sc_game altered] "pop should restore the prior altered flag"
    assertEq 1 [sc_game number] "pop should preserve the active game number"
} on error {msg opts} {
    puts stderr $msg
    puts stderr [dict get $opts -errorinfo]
    set exitCode 1
} finally {
    foreach handle [sc_base list] {
        if {$handle == 9} {
            continue
        }
        catch {sc_base close $handle}
    }
    file delete -force $root
}

exit $exitCode

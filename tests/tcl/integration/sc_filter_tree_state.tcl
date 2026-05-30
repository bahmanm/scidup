proc progressCallBack {done {msg ""}} {
    return 1
}

proc assertEq {expected actual message} {
    if {$expected ne $actual} {
        error "$message\nexpected: <$expected>\nactual:   <$actual>"
    }
}

proc assertListEq {expected actual message} {
    set expectedList [list {*}$expected]
    set actualList [list {*}$actual]
    if {$expectedList ne $actualList} {
        error "$message\nexpected: <$expectedList>\nactual:   <$actualList>"
    }
}

set root [file normalize [file join [pwd] tests tcl _tmp sc_filter_tree_state__[pid]__[clock clicks -milliseconds]]]
file mkdir $root

set exitCode 0

try {
    set basePath [file join [pwd] tests fixtures libscid database res_database]
    set base [sc_base open SCID4 $basePath]
    set total [sc_base numGames $base]

    assertEq 1339 $total "expected test resource database size"
    assertListEq [list $total $total $total] [sc_filter sizes $base dbfilter] "dbfilter should start full"

    set filterName [sc_filter new $base]
    assertListEq [list $total $total $total] [sc_filter sizes $base $filterName] "new filters should start full"

    sc_filter reset $base $filterName empty
    assertListEq [list 0 $total 0] [sc_filter sizes $base $filterName] "reset empty should clear the filter"

    sc_filter reset $base $filterName full
    assertListEq [list $total $total $total] [sc_filter sizes $base $filterName] "reset full should restore all games"

    sc_filter remove $base $filterName 1
    assertListEq [list [expr {$total - 1}] $total [expr {$total - 1}]] [sc_filter sizes $base $filterName] "remove should exclude one game"

    assertListEq [list $total $total $total] [sc_filter sizes $base tree] "tree filter should start full"
    sc_filter reset $base tree empty
    assertListEq [list 0 $total 0] [sc_filter sizes $base tree] "tree filter should be mutable through sc_filter"
    sc_filter reset $base tree full
    assertListEq [list $total $total $total] [sc_filter sizes $base tree] "tree filter should restore to full"

    assertEq 250 [sc_tree cacheinfo $base] "tree cache should use the default size"
    sc_tree cachesize $base 300
    assertEq 300 [sc_tree cacheinfo $base] "tree cache should grow when resized larger"
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

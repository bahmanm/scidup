
####################
# Recent files list:

set recentFiles(limit) 10   ;# Maximum number of recent files to remember.
set recentFiles(menu)   9   ;# Maximum number of files to show in File menu.
set recentFiles(extra)  9   ;# Maximum number of files to show in extra menu.
set recentFiles(data)  {}   ;# List of recently used files.

catch {source -encoding utf-8 [scidConfigFile recentfiles]}

namespace eval ::recentFiles {}

################################################################################
# ::recentFiles::save
#   Persists the recent-files list to disk.
# Visibility:
#   Public.
# Inputs:
#   - reportError: Boolean. If true, shows a warning dialog on write failure.
# Returns:
#   - None.
# Side effects:
#   - Writes `[scidConfigFile recentfiles]`.
#   - May persist `::recentSort` if it exists.
#   - Calls `::file::autoLoadBases.save`.
#   - May show a `tk_messageBox` on failure.
################################################################################
proc ::recentFiles::save {{reportError 0}} {
  global recentFiles
  set f {}
  set filename [scidConfigFile recentfiles]
  if  {[catch {open $filename w} f]} {
    if {$reportError} {
      tk_messageBox -title [tr ScidUp] -type ok -icon warning \
          -message "Unable to write file: $filename\n$f"
    }
    return
  }
  fconfigure $f -encoding utf-8
  puts $f "# ScidUp $::scidReleaseVersion recent files list"
  puts $f ""
  foreach i {limit menu extra data} {
    puts $f "set recentFiles($i) [list [set recentFiles($i)]]"
    puts $f ""
  }
  if {[info exists ::recentSort]}  {
    puts $f "set ::recentSort [list $::recentSort]"
  }
  ::file::autoLoadBases.save $f
  close $f
}

################################################################################
# ::recentFiles::add
#   Adds a file to the front of the recent-files list.
# Visibility:
#   Public.
# Inputs:
#   - fname: File path to add (ignored if empty).
# Returns:
#   - None.
# Side effects:
#   - Updates `recentFiles(data)`.
################################################################################
proc ::recentFiles::add {fname} {
  global recentFiles
  
  if {$fname == "" } { return }
  
  set rlist $recentFiles(data)
  
  # Remove file to be added from its current place in the
  # list, if it is there:
  while {1} {
    set idx [lsearch -exact $rlist $fname]
    if {$idx < 0} { break }
    set rlist [lreplace $rlist $idx $idx]
  }
  
  # Insert the current file at the start of the list:
  set rlist [linsert $rlist 0 $fname]
  
  # Trim the list if necessary:
  if {[llength $rlist] > $recentFiles(limit)} {
    set rlist [lrange $rlist 0 [expr {$recentFiles(limit) - 1}]]
  }
  
  set recentFiles(data) $rlist
  # ::recentFiles::save
}

################################################################################
# ::recentFiles::load
#   Loads a recent file by delegating to `::file::Open`.
# Visibility:
#   Public.
# Inputs:
#   - fname: File path to open.
# Returns:
#   - Integer: the return value of `::file::Open`.
# Side effects:
#   - Calls `::file::Open`, which is responsible for any open-or-switch behaviour.
################################################################################
proc ::recentFiles::load {fname} {
  ::file::Open $fname
}

################################################################################
# ::recentFiles::treeshow
#   Populates a menu with recent databases, opening each as a tree view.
# Visibility:
#   Public.
# Inputs:
#   - menu: Tk menu widget path to populate.
# Returns:
#   - None.
# Side effects:
#   - Clears and inserts commands into the given menu.
################################################################################
proc ::recentFiles::treeshow {menu} {
  global recentFiles
  set rlist $recentFiles(data)
  $menu delete 0 end
  set nfiles [llength $rlist]
  if {$nfiles > $recentFiles(limit)} { set nfiles $recentFiles(limit) }
  
  for {set i 0} {$i<$nfiles} {incr i} {
    set name [lindex $rlist $i]
    $menu add command -label "$name" -command [list ::file::openBaseAsTree $name]
  }
}

################################################################################
# ::recentFiles::show
#   Inserts recent files into a menu at a given index, optionally creating an
#   overflow submenu.
# Visibility:
#   Public.
# Inputs:
#   - menu: Tk menu widget path to insert into.
#   - idx: Menu index at which to start inserting.
# Returns:
#   - Integer: number of entries inserted into `menu` (including the "..." cascade
#     entry, if created; excludes entries inserted into `$menu.recentFiles`).
# Side effects:
#   - Inserts entries into `menu` and may create/destroy `$menu.recentFiles`.
#   - Populates `::helpMessage` for the inserted menu entries.
################################################################################
proc ::recentFiles::show {menu idx} {
  global recentFiles
  set rlist $recentFiles(data)
  set nfiles [llength $rlist]
  set nExtraFiles [expr {$nfiles - $recentFiles(menu)} ]
  if {$nfiles > $recentFiles(menu)} { set nfiles $recentFiles(menu) }
  if {$nExtraFiles > $recentFiles(extra)} {
    set nExtraFiles $recentFiles(extra)
  }
  if {$nExtraFiles < 0} { set nExtraFiles 0 }
  
  # Add menu commands for the most recent files:
  
  for {set i 0} {$i < $nfiles} {incr i} {
    set fname [lindex $rlist $i]
    set mname [::recentFiles::menuname $fname]
    set text [file tail $fname]
    set num [expr {$i + 1} ]
    set underline -1
    if {$num <= 9} { set underline 0 }
    if {$num == 10} { set underline 1 }
    $menu insert $idx command -label "$num: $mname" -underline $underline \
        -command [list ::recentFiles::load $fname]
    set ::helpMessage($menu,$idx) "  [file nativename $fname]"
    incr idx
  }
  
  # If no extra submenu of recent files is needed, return now:
  if {$nExtraFiles <= 0} { return $nfiles }
  
  # Now add the extra submenu of files:
  catch {destroy $menu.recentFiles}
  menu $menu.recentFiles
  $menu insert $idx cascade -label "..." -underline 0 -menu $menu.recentFiles
  set i $nfiles
  for {set extra 0} {$extra < $nExtraFiles} {incr extra} {
    set fname [lindex $rlist $i]
    incr i
    set mname [::recentFiles::menuname $fname]
    set text [file tail $fname]
    set num [expr {$extra + 1} ]
    set underline -1
    if {$num <= 9} { set underline 0 }
    if {$num == 10} { set underline 1 }
    $menu.recentFiles add command -label "$num: $mname" -underline $underline \
        -command [list ::recentFiles::load $fname]
    set ::helpMessage($menu.recentFiles,$extra) "  $fname"
  }
  return [expr {$nfiles + 1} ]
}

################################################################################
# ::recentFiles::menuname
#   Formats a filename for display in a menu (may shorten long paths).
# Visibility:
#   Private.
# Inputs:
#   - fname: File path.
# Returns:
#   - String: menu-friendly filename.
# Side effects:
#   - None.
################################################################################
proc ::recentFiles::menuname {fname} {
  set nativeName [file nativename $fname]
  if {[string length $nativeName] < 25} { return $nativeName }

  # Generate a menu name "..../path/filename" for the file.
  set mname [file tail $fname]
  set dir [file dirname $fname]
  while {1} {
    set tail [file join [file tail $dir] $mname]
    set dir [file dirname $dir]
    if {[string length $tail] > 20} { break }
    set mname $tail
  }

  set mname [file join .... $mname]
  set mname [file nativename $mname]
  return $mname
}

################################################################################
# ::recentFiles::configure
#   Populates a container with preference widgets for recent-files menu sizing.
# Visibility:
#   Public.
# Inputs:
#   - w: Parent widget path to populate.
# Returns:
#   - None.
# Side effects:
#   - Creates and grids child widgets under `w`.
#   - Binds widgets to `recentFiles(menu)` and `recentFiles(extra)`.
################################################################################
proc ::recentFiles::configure { w } {
  global recentFiles

  set tmpcombo {}
  for {set x 1} {$x <= 10} {incr x} {
      lappend tmpcombo $x
  }
  ttk::label $w.lmenu -text $::tr(RecentFilesMenu)
  ttk::label $w.lextra -text $::tr(RecentFilesExtra)
  ttk::combobox $w.menu -textvariable recentFiles(menu) -width 2 -values $tmpcombo -justify right -state readonly
  ttk::combobox $w.extra -textvariable recentFiles(extra) -width 2 -values $tmpcombo -justify right -state readonly
  grid $w.lmenu -row 0 -column 0 -sticky w
  grid $w.menu -row 0 -column 1 -sticky w -pady 5 -padx 5
  grid $w.lextra -row 1 -column 0 -sticky w
  grid $w.extra -row 1 -column 1 -sticky w -pady 5 -padx 5
}

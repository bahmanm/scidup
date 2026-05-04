#!/bin/sh

# Scid (Shane's Chess Information Database)
#
# Copyright (C) 1999-2004 Shane Hudson
# Copyright (C) 2006-2009 Pascal Georges
# Copyright (C) 2008-2011 Alexander Wagner
# Copyright (C) 2013-2015 Fulvio Benini
#
# Scid is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation.

#
# The following few comments are only for Unix versions of Scid:
#

# The "\" at the end of the comment line below is necessary! It means
#   that the "exec" line is a comment to Tcl/Tk, but not to /bin/sh.
# The next line restarts using scidup: \
exec `dirname $0`/../../../bin/scidup "$0" "$@"

# The above launches scidup from a location relative to this script.
# Alternatively, you can change the top line of this startup script
# to start scidup directly from a specific location, e.g.:

# For the above to work, scidup must be in a directory in your PATH.
# Alternatively, you can set the first line to start scidup directly
# by specifying the full name of scidup, e.g.:
# #!/home/myname/bin/scidup

############################################################

source [file join [file dirname [info script]] scidup dirs.tcl]

package require Tk  9
set useLocalTooltip [catch {package require tooltip 2.0}]

set scidReleaseVersion [sc_info release version]
set scidReleaseDate [sc_info release date]


# Helper function for issuing debug messages:
# trace add execution some_fn {enter leave} trace_log
# trace add variable some_var {read write array unset} trace_log
################################################################################
# trace_log
#   Emits trace diagnostics for commands and variables.
# Visibility:
#   Public.
# Inputs:
#   - args: Arguments supplied by Tcl's trace subsystem.
# Returns:
#   - None.
# Side effects:
#   - Writes a formatted message to stderr.
################################################################################
proc trace_log {args} {
  set bt "::"
  catch {set bt [info level -1]}
  if {[lindex $bt 0] eq "trace_log"} { return }

  set msg "\[[clock format [clock seconds] -format {%H:%M:%S}]\]"
  set op [lindex $args end]
  if {$op in "read write array"} {
    lassign $args var_name elem
    upvar $var_name var
    if {[array exists var]} { set value $var($elem)} { set value $var}
    append msg "   $op: $value [list $args]"
  } else {
    append msg " $op $args"
  }
  puts stderr "$msg - bt: [list $bt]"
}

# Determine operating system platform: unix, windows or macos
#
set windowsOS 0
set unixOS 0
set macOS 0
if {[tk windowingsystem] == "aqua"} {
  set macOS 1
} elseif {$tcl_platform(platform) == "unix"} {
  set unixOS 1
} elseif {$tcl_platform(platform) == "windows"} {
  set windowsOS 1
}

################################################################################
# InitDirs
#   Initialises global directory paths and ensures required directories exist.
# Visibility:
#   Private.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Sets directory globals (e.g. scidExeDir/scidConfigDir/scidShareDir).
#   - Creates user config/data/log directories when missing.
################################################################################
proc InitDirs {} {
  global scidExeDir scidUserDir scidConfigDir scidDataDir scidLogDir scidShareDir scidImgDir scidTclDir
  global scidUpConfigRootDir
  global scidBooksDir scidBasesDir ecoFile
  global scidUpIsBundle scidUpBundleRoot scidUpBundleLibraryDir

  # scidExeDir: contains the directory of the Scid executable program.
  # Used to determine the location of various relative data directories.
  set scidExecutable [info nameofexecutable]
  if {[file type $scidExecutable] == "link"} {
    set scidExeDir [file dirname [file readlink $scidExecutable]]
    if {[file pathtype $scidExeDir] == "relative"} {
      set scidExeDir [file dirname [file join [file dirname $scidExecutable]\
        [file readlink $scidExecutable]]]
    }
  } else {
    set scidExeDir [file dirname $scidExecutable]
  }

  set scidUpBundleRoot [file normalize [file join $scidExeDir ".."]]
  set scidUpBundleLibraryDir [file join $scidUpBundleRoot "lib"]
  set scidUpIsBundle 0
  if {[file exists [file join $scidUpBundleLibraryDir "tcl9" "9.0" "init.tcl"]] && [file exists [file join $scidUpBundleLibraryDir "tk9.0" "tk.tcl"]]} {
    set scidUpIsBundle 1
  }

  # scidUserDir: location of user-specific Scid files.
  set scidUserDir [::scidup::dirs::configRoot $::tcl_platform(platform) $::tcl_platform(os) $scidExeDir]
  set scidUpConfigRootDir $scidUserDir

  # scidConfigDir, scidDataDir, scidLogDir:
  # Location of Scid configuration, data and log files.
  set scidConfigDir $scidUserDir
  set scidDataDir [file nativename [file join $scidUserDir "data"]]
  set scidLogDir [file nativename [file join $scidUserDir "log"]]

  # scidShareDir, scidImgDir, scidTclDir, scidBooksDir, scidBasesDir, ecoFile:
  # Location of Scid resources
  set scidShareDir [file normalize [file join $scidExeDir "../share/scidup"]]
  if {! [file isdirectory $::scidShareDir]} {
    set scidShareDir $::scidExeDir
  }
  set scidTclDir [file nativename [file join $scidShareDir "tcl"]]
  if {! [file isdirectory $scidTclDir]} {
    set scidTclDir [file dirname [info script]]
    set scidShareDir [file normalize "$scidTclDir/../"]
  }
  set scidImgDir [file nativename [file join $scidShareDir "images"]]

  #Default values, can be overwritten by file option
  set scidBooksDir [file nativename [file join $scidShareDir "books"]]
  set scidBasesDir [file nativename [file join $scidShareDir "bases"]]
  set ecoFile [file nativename [file join $scidShareDir "scid.eco"]]

  set ::scidEnginesDir [file normalize [file join $::scidExeDir "../engines"]]
  if {! [file isdirectory $::scidEnginesDir]} {
    set ::scidEnginesDir $::scidExeDir
  }

  # Create the config, data and log directories if they do not exist:

  ################################################################################
  # makeScidDir
  # Visibility:
  #   Private.
  # Inputs:
  #   - dir: Directory path to ensure exists.
  # Returns:
  #   - None.
  # Side effects:
  #   - Creates the directory when it does not already exist.
  ################################################################################
  proc makeScidDir {dir} {
    if {! [file isdirectory $dir]} {
      file mkdir $dir
    }
  }
  makeScidDir $scidConfigDir
  makeScidDir $scidDataDir
  makeScidDir $scidLogDir
}
InitDirs


################################################################################
# InitImg
#   Loads application images (icons, buttons, textures, and piece sets).
# Visibility:
#   Private.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Creates Tk images and sets related globals (e.g. boardStyles/textureSquare).
#   - May set the application icon via `wm iconphoto`.
################################################################################
proc InitImg {} {
  global scidImgDir boardStyle boardStyles textureSquare

  #Set app icon
  set scidIconFile [file nativename [file join $scidImgDir "icons" "scidup.png"]]
  if {[file readable $scidIconFile]} {
    wm iconphoto . -default [image create photo -file "$scidIconFile"]
  }

  #Load all icons/buttons/_filename_.gif
  set dname [file join $::scidImgDir icons buttons]
  foreach {fname} [glob -directory $dname *.gif] {
    set iname [string range [file tail $fname] 0 end-4]
    image create photo $iname -file $fname
  }

  #Load all icons/buttons/_filename_.png
  set dname [file join $::scidImgDir icons buttons]
  foreach {fname} [glob -directory $dname *.png] {
    set iname [string range [file tail $fname] 0 end-4]
    image create photo $iname -format png -file $fname
  }

  #Load all sets/boards/_filename_.gif
  set textureSquare {}
  set dname [file join $::scidImgDir sets boards]
  foreach {fname} [glob -directory $dname *.gif] {
    set iname [string range [file tail $fname] 0 end-4]
    image create photo $iname -file $fname
    if {[string range $iname end-1 end] == "-l"} {
      lappend textureSquare [string range $iname 0 end-2]
    }
  }

  #Search available piece sets
  set boardStyles {}
  set dname [file join $::scidImgDir sets pieces]
  foreach {piecetype} [glob -directory $dname *] {
    if {[file isdirectory $piecetype] == 1} {
      lappend boardStyles [file tail $piecetype]
    }
  }
}
if {[catch {InitImg}]} {
  tk_messageBox -type ok -icon error -title "ScidUp: Error" \
    -message "Cannot load images.\n$::errorCode\n\n$::errorInfo"
  exit
}

################################################################################
# InitTooltip
#   Initialises the tooltip implementation and its compatibility wrapper.
# Visibility:
#   Private.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - May source Scid's local tooltip implementation.
#   - Defines ::utils::tooltip::Set as a wrapper around tooltip::tooltip.
################################################################################
proc InitTooltip {} {
  if {$::useLocalTooltip} {
    source [file nativename [file join $::scidTclDir "utils/tklib_tooltip.tcl"]]
  }
  namespace eval ::utils::tooltip {

    ################################################################################
    # ::utils::tooltip::Set
    #   Registers or updates a tooltip for a widget.
    # Visibility:
    #   Public.
    # Inputs:
    #   - args: Arguments forwarded to `tooltip::tooltip`.
    # Returns:
    #   - None.
    # Side effects:
    #   - Delegates to `tooltip::tooltip`.
    ################################################################################
    proc Set {args} { tooltip::tooltip {*}$args }
  }
}
InitTooltip

#############################################################
#
# NAMESPACES
#
# The main Tcl/Tk namespaces used in the Scid application are
# initialized here, so that default values can be set up and
# altered when the user options file is loaded.
#
foreach ns {
  ::icon
  ::splash
  ::utils
  ::utils::date ::utils::font ::utils::history ::utils::pane ::utils::string
  ::utils::sound ::utils::validate ::utils::win
  ::file
  ::file::finder ::file::maint ::maint
  ::bookmarks
  ::edit
  ::game
  ::gbrowser
  ::search
  ::search::filter ::search::board ::search::header ::search::material
  ::windows
  ::windows::gamelist ::windows::stats ::tree ::tree::mask ::windows::tree
  ::windows::switcher ::windows::eco ::crosstab ::pgn ::book
  ::windows::commenteditor
  ::tools
  ::tools::analysis
  ::tools::graphs
  ::tools::graphs::filter ::tools::graphs::absfilter ::tools::graphs::rating ::tools::graphs::score
  ::tb ::optable
  ::board ::move
  ::uci ::reviewgame ::novag
  ::config ::docking
  ::pinfo
  ::unsafe
} {
  namespace eval $ns {}
}

################################################################################
# ::splash::add
# Visibility:
#   Private.
# Inputs:
#   - text: Status text to display.
# Returns:
#   - None.
# Side effects:
#   - Intended to update the splash screen (currently a no-op).
################################################################################
proc ::splash::add {text} {
#TODO: decide what to do with all the splash messages (delete?)
}

# Platform specific operations
if { $unixOS } {
  # adds a checkbox to show hidden files
  catch {tk_getOpenFile -with-invalid-argument}
  namespace eval ::tk::dialog::file {
    variable showHiddenBtn 1
    variable showHiddenVar 0
  }
}

# Mouse button mapping:
# - MB2 is middle, MB3 is right.
set ::MB2 2
set ::MB3 3

if { $macOS } {
  set ::COMMAND Command
} else {
  set ::COMMAND Control
}


####################################################
# safeSource() - source a file using a safe interpreter
# @filename:  the absolute path to the file to source (load and execute)
# @args:      pairs of varname value that are visible to the sourced code
#
# This function execute the code inside a safe tcl interpreter and override
# "set" to import the variables of the executed code in the ::unsafe namespace.
# Attention must be paid to not evaluate ::unsafe vars, for example:
# set ::unsafe::badcode {tk_messageBox -message executeme}
# eval $::unsafe::badcode
# after idle $::unsafe::badcode

################################################################################
# safeSource
#   Sources a file within a safe interpreter and imports assigned variables.
# Visibility:
#   Public.
# Inputs:
#   - filename: Absolute path of the file to source.
#   - args: Pairs of varName/value to expose to the sourced script.
# Returns:
#   - None.
# Side effects:
#   - Creates and caches ::safeInterp (a safe interpreter).
#   - Imports variables assigned by the script into ::unsafe::*.
################################################################################
proc safeSource {filename args} {
  if {![info exists ::safeInterp]} {
    set ::safeInterp [::safe::interpCreate]
    interp hide $::safeInterp set
    interp alias $::safeInterp set {} ::safeSet $::safeInterp
  }
  set f [file nativename "$filename"]
  set d [file dirname $f]
  set n [file tail $f]
  set vdir [::safe::interpAddToAccessPath $::safeInterp $d]
  interp alias $::safeInterp image {} ::safeImage $::safeInterp [list $vdir $d]
  foreach {varname value} $args {
    $::safeInterp eval [list set $varname $value]
  }
  $::safeInterp eval [list set vdir $vdir]
  $::safeInterp eval [list source [file join $vdir $n]]
  foreach {varname value} $args {
    $::safeInterp eval [list unset $varname]
  }
}
################################################################################
# safeSet
#   Implements the safe-interpreter `set` command and mirrors values into ::unsafe::.
# Visibility:
#   Private.
# Inputs:
#   - i: Interpreter handle.
#   - args: `set` argument list (name/value pairs).
# Returns:
#   - Result of the underlying `set` operation.
# Side effects:
#   - Writes variables into the ::unsafe namespace.
################################################################################
proc safeSet {i args} {
  #TODO: do not import local variables
  #if {[$::safeInterp eval info level] == 0}
  foreach {varname value} $args {
    set ::unsafe::$varname $value
  }
  interp invokehidden $i set {*}$args
}

# Use a ::safe::interp to evaluate a file containing ttk::style and image commands.
# The evaluated script can only read the files inside its directory or direct subdirectories.
# @param filename:  the absolute path to the file

# recursiv identify all subdirs
################################################################################
# safeAddSubDirsToAccessPath
#   Recursively adds subdirectories to a safe interpreter's access path.
# Visibility:
#   Private.
# Inputs:
#   - safeInterp: Safe interpreter handle.
#   - dir: Root directory whose subdirectories will be added.
# Returns:
#   - None.
# Side effects:
#   - Calls ::safe::interpAddToAccessPath for each discovered subdirectory.
################################################################################
proc safeAddSubDirsToAccessPath { safeInterp dir } {
  foreach subdir [glob -nocomplain -directory $dir -type d *] {
    ::safe::interpAddToAccessPath $safeInterp $subdir
    safeAddSubDirsToAccessPath $safeInterp $subdir
  }
}

################################################################################
# safeSourceStyle
#   Loads a style/theme script in a safe interpreter.
# Visibility:
#   Public.
# Inputs:
#   - filename: Absolute path to the theme/style script.
# Returns:
#   - None.
# Side effects:
#   - Executes the style script in a safe interpreter with restricted file access.
#   - Applies any resulting ttk::style/image changes to the current UI.
################################################################################
proc safeSourceStyle {filename} {
  set filename [file nativename "$filename"]
  set dir [file dirname $filename]

  set safeInterp [::safe::interpCreate]

  set vdir [::safe::interpAddToAccessPath $safeInterp $dir]
  safeAddSubDirsToAccessPath $safeInterp $dir

  interp alias $safeInterp pwd {} ::safePwd
  interp alias $safeInterp package {} ::safePackage $safeInterp
  interp alias $safeInterp image {} ::safeImage $safeInterp [list $vdir $dir]
  interp alias $safeInterp ttk::style {} ::safeStyle $safeInterp
  interp alias $safeInterp ::styleOption {} ::safeStyleOption $safeInterp

  $safeInterp eval [list set vdir $vdir]
  $safeInterp eval [list source [file join $vdir [file tail $filename]]]
  ::safe::interpDelete $safeInterp
}

################################################################################
# safePwd
# Visibility:
#   Private.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - None.
################################################################################
proc safePwd {} {}
################################################################################
# safePackage
#   Restricts `package` operations invoked by safe style scripts.
# Visibility:
#   Private.
# Inputs:
#   - interp: Safe interpreter handle.
#   - args: Arguments forwarded from the safe interpreter.
# Returns:
#   - None.
# Side effects:
#   - May call `package require/provide/vsatisfies` in the main interpreter.
################################################################################
proc safePackage { interp args } {
  set args [lassign $args command]
  catch {
    switch -- $command {
      "require" { package require {*}$args }
      "vsatisfies" { package vsatisfies {*}$args }
      "provide" { package provide {*}$args }
    }
  }
}

################################################################################
# safeImage
#   Maps image -file arguments from safe to real paths and delegates to `image`.
# Visibility:
#   Private.
# Inputs:
#   - interp: Safe interpreter handle.
#   - dir_map: Mapping list used for rewriting safe paths.
#   - args: Arguments forwarded to `image`.
# Returns:
#   - Result of the delegated `image` command.
# Side effects:
#   - Creates or modifies Tk images.
################################################################################
proc safeImage {interp dir_map args} {
  set filename [lsearch -exact $args -file]
  if {$filename != -1} {
    incr filename
    set real_filename [string map $dir_map [lindex $args $filename]]
    set args [lreplace $args $filename $filename $real_filename]
  }
  return [image {*}$args]
}

################################################################################
# safeStyleOption
#   Evaluates a styleOption request from a safe style script.
# Visibility:
#   Private.
# Inputs:
#   - interp: Safe interpreter handle.
#   - args: Arguments forwarded from the safe interpreter.
# Returns:
#   - None.
# Side effects:
#   - Delegates to ::styleOption.
################################################################################
proc safeStyleOption {interp args} {
    styleOption {*}$args
}

# Evaluate ttk::style commands invoked inside the restricted script.
# If the command includes a script (ttk::style theme settings or ttk::style theme create)
# it is evaluated using the safe interpreter.
################################################################################
# safeStyle
#   Evaluates ttk::style commands invoked from a safe style script.
# Visibility:
#   Private.
# Inputs:
#   - interp: Safe interpreter handle.
#   - args: Arguments forwarded from the safe interpreter.
# Returns:
#   - Result of the delegated ttk::style command.
# Side effects:
#   - May configure ttk styles and/or evaluate theme scripts.
################################################################################
proc safeStyle {interp args} {
  lassign $args theme settings themeName script
  if {$theme eq "theme"} {
    if { $settings eq "settings"} {
      set curr_theme [ttk::style theme use]
      ttk::style theme use $themeName
      $interp eval $script
      ttk::style theme use $curr_theme
      return
    }

    set script_i [lsearch -exact $args -settings]
    if {$script_i != -1} {
      set script_j [expr {$script_i + 1}]
      ttk::style {*}[lreplace $args $script_i $script_j]
      $interp eval [list ttk::style theme settings $themeName [lindex $args $script_j]]
      return
    }
  }

  return [ttk::style {*}$args]
}

####################################################
# Load default/saved values
source [file nativename [file join $::scidTclDir "options.tcl"]]

################################################################################
# calculateTreeviewRowHeight
#   Computes and applies row heights for Treeview widgets based on current fonts.
# Visibility:
#   Private.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Configures ttk::style row heights.
#   - Updates ::glistRowHeight.
################################################################################
proc calculateTreeviewRowHeight { } {
  set row_height [expr { round(1.1 * [font metrics font_Regular -linespace]) }]
  ttk::style configure Treeview -rowheight $row_height

  set ::glistRowHeight [expr { round(1.4 * [font metrics font_Small -linespace]) }]
  ttk::style configure Gamelist.Treeview -rowheight $::glistRowHeight
}

################################################################################
# updateFonts
#   Updates derived fonts (bold/italic/headers) when a base font changes.
# Visibility:
#   Private.
# Inputs:
#   - font_name: Base font name that has changed (e.g. font_Regular).
# Returns:
#   - None.
# Side effects:
#   - Configures derived fonts.
#   - Recalculates Treeview row heights.
################################################################################
proc updateFonts {font_name} {
  switch $font_name {
    {font_Regular} {
      set font [font actual $font_name -family]
      set fontsize [font actual $font_name -size]
      font configure font_Bold       -family $font -size $fontsize -weight bold
      font configure font_Italic     -family $font -size $fontsize -slant italic
      font configure font_BoldItalic -family $font -size $fontsize -weight bold -slant italic
      font configure font_H1 -family $font -size [expr {$fontsize + 8} ] -weight bold
      font configure font_H2 -family $font -size [expr {$fontsize + 6} ] -weight bold
      font configure font_H3 -family $font -size [expr {$fontsize + 4} ] -weight bold
      font configure font_H4 -family $font -size [expr {$fontsize + 2} ] -weight bold
      font configure font_H5 -family $font -size [expr {$fontsize + 0} ] -weight bold
    }
    {font_Small} {
      set font [font actual $font_name -family]
      set fontsize [font actual $font_name -size]
      font configure font_SmallBold -family $font -size $fontsize -weight bold
      font configure font_SmallItalic -family $font -size $fontsize -slant italic
    }
  }
  calculateTreeviewRowHeight
}

################################################################################
# createFonts
#   Creates Scid's named fonts and initialises them from ::fontOptions.
# Visibility:
#   Private.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Creates and configures Tk fonts (e.g. font_Regular, font_Bold).
#   - Updates ::utils::tooltip::font.
################################################################################
proc createFonts {} {
  font create font_Bold
  font create font_BoldItalic
  font create font_Italic
  font create font_H1
  font create font_H2
  font create font_H3
  font create font_H4
  font create font_H5

  font create font_SmallBold
  font create font_SmallItalic

  foreach {name value} [array get ::fontOptions] {
    lassign $value f sz w s
    if {$f ne ""} {
      font create font_$name -family $f -size $sz -weight $w -slant $s
    } else {
      font create font_$name {*}[font configure TkDefaultFont]
      if {$name eq "Small"} {
        font configure font_$name -size [expr {int([font actual font_$name -size] * 0.85)}]
      } elseif {$name eq "Tiny"} {
        font configure font_$name -size [expr {int([font actual font_$name -size] * 0.7)}]
      }
    }
    updateFonts font_$name
  }

  set ::utils::tooltip::font font_Small
}
createFonts

# Workaround: set the options of ttkEntry.c widgets that don't work with ttk::style
set ::themeOptions {}
################################################################################
# styleOption
#   Records theme-specific option overrides for later application.
# Visibility:
#   Public.
# Inputs:
#   - themeName: Theme identifier.
#   - pattern: Option database pattern.
#   - value: Value to apply.
# Returns:
#   - None.
# Side effects:
#   - Appends an entry to ::themeOptions.
################################################################################
proc styleOption {themeName pattern value} {
  lappend ::themeOptions [list $themeName $pattern $value]
}

# Load darktheme, must load here to have it in place if used
source -encoding utf-8 [file nativename [file join $::scidTclDir "darktheme.tcl"]]
# Load more theme
if { [file exists $::ThemePackageFile] } {
  catch { ::safeSourceStyle $::ThemePackageFile }
}

# The font for ttkEntry.c widgets cannot be set with ttk::style
option add *TCombobox*font font_Regular
option add *TEntry.font font_Regular
option add *TSpinbox.font font_Regular

# Set the menu options
# This options are used only when a menu is created. If the theme is changed,
# it is necessary to restart the program to show the new colors.
################################################################################
# configure_menus
#   Configures menu option database entries for the current theme.
# Visibility:
#   Private.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Adds option database entries affecting menu appearance.
################################################################################
proc configure_menus {} {
  option add *Menu*TearOff 0
  if {[llength $::fontOptions(Menu)] == 4} { option add *Menu*Font font_Menu }

  if {$::unixOS} {
    option add *Menu.background [ttk::style lookup . -background] startupFile
    option add *Menu.activeBackground [ttk::style lookup . -background active] startupFile
    option add *Menu.disabledBackground [ttk::style lookup . -background disabled] startupFile
    option add *Menu.foreground [ttk::style lookup . -foreground] startupFile
    option add *Menu.selectColor [ttk::style lookup . -foreground] startupFile
    option add *Menu.activeForeground [ttk::style lookup . -foreground active] startupFile
    option add *Menu.disabledForeground [ttk::style lookup . -foreground disabled] startupFile
  }
}

################################################################################
# configure_style
#   Configures ttk styles and applies theme-specific option overrides.
# Visibility:
#   Private.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Updates ttk::style configuration.
#   - Configures tooltip appearance.
#   - Adds option database entries for combobox popdown listboxes.
################################################################################
proc configure_style {} {
  # Use default font everywhere
  ttk::style configure . -font font_Regular
  ttk::style configure Heading -font font_Regular

  # Style definitions
  ttk::style configure Bold.TCheckbutton -font font_Bold
  ttk::style configure Small.TCheckbutton -font font_Small

  ttk::style configure Small.TButton -font font_Small
  ttk::style configure Bold.TButton -font font_Bold
  ttk::style configure Pad0.Small.TButton -padding 0

  ttk::style configure Small.TRadiobutton -font font_Small
  ttk::style configure Bold.TRadiobutton -font font_Bold
  ttk::style configure SmallBold.TRadiobutton -font font_SmallBold

  ttk::style configure pad0.TMenubutton -padding 0 -indicatorwidth 0 -indicatorheight 0  -font font_Small

  ttk::style configure fieldbg.TLabel -background [ttk::style lookup . -fieldbackground "" white]

  ttk::style configure Switch.Toolbutton -padding 0

  # Some themes (e.g. vista and xpnative) use custom field elements and ignore -fieldbackground
  if {[regexp {(Combobox|Entry|Spinbox)\.(field|background)} [ttk::style element names]]} {
    ttk::style configure Error.TCombobox -foreground #b80f0a
    ttk::style configure Error.TEntry -foreground #b80f0a
    ttk::style configure Error.TSpinbox -foreground #b80f0a
  } else {
    ttk::style configure Error.TCombobox -fieldbackground #b80f0a
    ttk::style configure Error.TEntry -fieldbackground #b80f0a
    ttk::style configure Error.TSpinbox -fieldbackground #b80f0a
  }

  calculateTreeviewRowHeight

  # The ttk::combobox popdown listbox cannot be configured using ttk::style
  option add *TCombobox*Listbox.background [ttk::style lookup . -fieldbackground "" white] startupFile
  option add *TCombobox*Listbox.foreground [ttk::style lookup . -foreground] startupFile
  option add *TCombobox*Listbox.selectBackground [ttk::style lookup . -selectbackground] startupFile
  option add *TCombobox*Listbox.selectForeground [ttk::style lookup . -selectforeground] startupFile

  # Configure tooltips appearance
  ::tooltip::tooltip configure \
    -background [ttk::style lookup . -fieldbackground "" white] \
    -foreground [ttk::style lookup . -foreground]

  # Add the theme's specific options
  foreach elem [lsearch -all -inline -exact -index 0 $::themeOptions [ttk::style theme use]] {
    option add [lindex $elem 1] [lindex $elem 2]
  }

  #Load light or dark icons (if the theme name contains "dark")
  set icons_dir "light"
  if {[string first "dark" [ttk::style theme use]] != -1} {
    set icons_dir "dark"
  }
  set dname [file join $::scidImgDir icons $icons_dir]
  foreach {fname} [glob -directory $dname *.png] {
    set iname [string range [file tail $fname] 0 end-4]
    image create photo ::icon::$iname -format png -file $fname
  }
}
bind . <<ThemeChanged>> { if {[string equal %W .]} { configure_style } }

catch { ttk::style theme use $::lookTheme }
configure_menus


# Uses the circle and full circle unicode characters to simulate a switch button.
# Based on a ttk::checkbutton, update -text to reflect its state.
# Example:
#     ttk::checkbutton widget_name -style Switch.Toolbutton \
#         -command [list ::update_switch_btn widget_name]
#     ::update_switch_btn widget_name initial_value
# Return the value of the variable associated with the widget.
################################################################################
# ::update_switch_btn
#   Updates a ttk::checkbutton to render as a switch (filled/empty circle).
# Visibility:
#   Public.
# Inputs:
#   - widget: Widget path of the ttk::checkbutton.
#   - set_value: Optional initial value to assign to the widget variable.
# Returns:
#   - Current value of the widget's associated variable.
# Side effects:
#   - May assign the associated variable.
#   - Configures the widget's -text.
################################################################################
proc ::update_switch_btn {widget {set_value ""}} {
  set varname [$widget cget -variable]
  if {$set_value ne ""} {
    set ::$varname $set_value
  }
  if {[$widget instate selected]} {
    set full_circle [expr {$::windowsOS ?"\u2B24":"\u25CF"}]
    $widget configure -text "       $full_circle"
  } else {
    $widget configure -text "\u25EF       "
  }
  return [set ::$varname]
}

################################################################################
# autoscrollText
#   Creates a text widget with themed styling and autoscroll bars.
# Visibility:
#   Public.
# Inputs:
#   - bars: Autoscroll bar configuration.
#   - frame: Frame path to create.
#   - widget: Text widget path to create.
#   - style: ttk style name used for applying theme colours.
# Returns:
#   - None.
# Side effects:
#   - Creates a ttk::frame and text widget.
#   - Configures tags and applies theme styles.
################################################################################
proc autoscrollText {bars frame widget style} {
  ttk::frame $frame
  text $widget -cursor arrow -state disabled -highlightthickness 0 -font font_Regular
  $widget tag configure header -font font_Bold
  applyThemeStyle $style $widget
  autoscrollBars $bars $frame $widget
}

# Create a text widget and apply to it the current ttk style.
# It also creates a tag "header" in the text widget.
################################################################################
# ttk_text
#   Creates a text widget and applies the current ttk theme style.
# Visibility:
#   Public.
# Inputs:
#   - pathName: Widget path.
#   - args: Optional text widget configuration arguments.
# Returns:
#   - The created text widget path.
# Side effects:
#   - Creates and configures a Tk text widget.
#   - Applies theme styles and creates a "header" tag.
################################################################################
proc ttk_text {pathName {args ""}} {
  set style Treeview
  if {[set idx [lsearch $args "-style"]] >=0} {
    set style [lindex $args [expr {$idx + 1}]]
    set args [lreplace $args $idx [expr {$idx + 1}]]
  }
  set res [text $pathName -cursor arrow -highlightthickness 0 -font font_Regular]
  if {[llength $args] > 0} {
    $pathName configure {*}$args
  }
  $pathName tag configure header -font font_Bold
  ::applyThemeStyle $style $pathName
  return $res
}

# Create a canvas and apply to it the current ttk style.
################################################################################
# ttk_canvas
#   Creates a canvas widget and applies the current ttk theme style.
# Visibility:
#   Public.
# Inputs:
#   - pathName: Widget path.
#   - args: Canvas configuration arguments.
# Returns:
#   - The created canvas widget path.
# Side effects:
#   - Creates and configures a Tk canvas widget.
################################################################################
proc ttk_canvas {pathName args} {
  set res [canvas $pathName {*}$args]
  ::applyThemeStyle Treeview $pathName
  return $res
}

# Create an item into a widget (i.e. a canvas) and apply to it the current ttk style.
# TODO: find a better way to do this and re-apply when <<ThemeChanged>>
################################################################################
# ttk_create
#   Creates an item in a widget and applies theme-derived defaults.
# Visibility:
#   Public.
# Inputs:
#   - pathName: Widget path (typically a canvas).
#   - type: Item type (e.g. rectangle, text).
#   - x: X coordinate.
#   - y: Y coordinate.
#   - args: Item creation arguments.
# Returns:
#   - The created item identifier.
# Side effects:
#   - Creates a widget item and may set default -fill colour.
################################################################################
proc ttk_create {pathName type x y args} {
  if {"-fill" ni $args} {
    lappend args "-fill"
    lappend args [ttk::style lookup Treeview -foreground "" black]
  }
  $pathName create $type $x $y {*}$args
}

# Apply the theme's background color to a widget
################################################################################
# applyThemeColor_background
#   Applies the theme background colour to a widget and re-applies on theme change.
# Visibility:
#   Public.
# Inputs:
#   - widget: Widget path.
# Returns:
#   - None.
# Side effects:
#   - Configures the widget background.
#   - Adds a <<ThemeChanged>> binding.
################################################################################
proc applyThemeColor_background { widget } {
  set bgcolor [ttk::style lookup . -background "" #d9d9d9]
  $widget configure -background $bgcolor
  bind $widget <<ThemeChanged>> [list ::applyThemeColor_background $widget]
}

# Apply a ttk style to a tk widget
################################################################################
# applyThemeStyle
#   Applies a ttk style's colours to a Tk widget and re-applies on theme change.
# Visibility:
#   Public.
# Inputs:
#   - style: ttk style name.
#   - widget: Tk widget path.
# Returns:
#   - None.
# Side effects:
#   - Configures widget options derived from ttk style.
#   - Adds a <<ThemeChanged>> binding.
################################################################################
proc applyThemeStyle {style widget} {
  set exclude [list "-font"]
  set options [ttk::style configure .]
  lappend options {*}[ttk::style configure $style]

  foreach {option value} $options {
    if {$option in $exclude} { continue }
    catch { $widget configure $option $value }
  }
  bind $widget <<ThemeChanged>> [list ::applyThemeStyle $style $widget]
}

image create photo flag_unknown -data {
      iVBORw0KGgoAAAANSUhEUgAAABgAAAAMCAYAAAB4MH11AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAC4jAAAuIw
      F4pT92AAAAB3RJTUUH4wQHCTMzcDliXAAAABJJREFUOMtjYBgFo2AUjIKBBwAEjAABIobxpQAAAABJRU5ErkJggg==
}

################################################################################
# getFlagImage
#   Returns an image handle for a country's flag.
# Visibility:
#   Public.
# Inputs:
#   - countryID: Country identifier.
#   - returnUnknowFlag: When "no", returns empty string if no flag is available.
# Returns:
#   - Image name to use for the flag (or empty string).
# Side effects:
#   - May load an image from disk and create it.
################################################################################
proc getFlagImage { countryID { returnUnknowFlag no } } {
  set cflag "flag_[string tolower [string range $countryID 0 2]]"
  # preset unkown flag (empty transparent image 24x12)
  set country flag_unknown
  if { $cflag eq [info commands $cflag] } {
    # flag exists, use it
    set country $cflag
  } else {
    # flag does not exist, try to load it
    set dname [file join $::scidImgDir icons flags $cflag.gif]
    if { [file exists $dname] } {
      image create photo $cflag -file $dname
      set country $cflag
    } elseif { $returnUnknowFlag == "no" } {
      #no flag is needed, return nothing
      set country ""
    }
  }
  return $country
}

# Set numeric format
sc_info decimal $::locale(numeric)

# Start in the clipbase, if no database is loaded at startup.
set ::clipbase_db [sc_info clipbase]
sc_base switch $::clipbase_db
set ::curr_db [sc_base current]


set tcl_files {
language.tcl
errors.tcl
utils.tcl
scidup/updates.tcl
utils/date.tcl
utils/font.tcl
utils/graph.tcl
utils/history.tcl
utils/pane.tcl
utils/sound.tcl
utils/string.tcl
utils/validate.tcl
utils/win.tcl
enginecfg.tcl
enginecomm.tcl
misc.tcl
htext.tcl
file.tcl
file/finder.tcl
file/bookmark.tcl
file/recent.tcl
file/spellchk.tcl
file/maint.tcl
edit.tcl
game.tcl
windows.tcl
windows/browser.tcl
windows/gamelist.tcl
windows/pgn.tcl
windows/preferences.tcl
windows/book.tcl
windows/comment.tcl
windows/eco.tcl
windows/engine.tcl
windows/stats.tcl
windows/tree.tcl
windows/crosstab.tcl
windows/pfinder.tcl
windows/tourney.tcl
windows/switcher.tcl
search/search.tcl
search/board.tcl
search/header.tcl
search/material.tcl
tools/import.tcl
tools/optable.tcl
tools/preport.tcl
tools/pinfo.tcl
tools/analysis.tcl
tools/graphs.tcl
tools/ptracker.tcl
help/help.tcl
help/tips.tcl
keyboard.tcl
menus.tcl
board.tcl
move.tcl
main.tcl
tools/uci.tcl
end.tcl
tools/reviewgame.tcl
tools/inputengine.tcl
tools/novag.tcl
}

foreach f $tcl_files {
  source -encoding utf-8 [file nativename [file join $::scidTclDir "$f"]]
}

###
### End of file: start.tcl

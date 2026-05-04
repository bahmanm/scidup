###
### misc.tcl: part of Scid.
### Copyright (C) 2001  Shane Hudson.
### Copyright (C) 2007  Pascal Georges
### Copyright (C) 2015  Fulvio Benini
###
### Miscellaneous routines called by other Tcl functions

################################################################################
# ::vwaitTimed
#   Waits for a variable to be set, optionally timing out after a delay.
# Visibility:
#   Public.
# Inputs:
#   - `var`: Fully-qualified variable name to wait on (e.g. `::someVar`).
#   - `delay` (optional): Timeout in milliseconds (0 means no timeout).
#   - `warn` (optional): When `warnuser`, shows an error dialog on timeout.
# Returns:
#   - None.
# Side effects:
#   - Schedules/cancels an `after` timer when `delay` is non-zero.
#   - May show a `tk_messageBox` on timeout.
################################################################################
proc vwaitTimed { var {delay 0} {warn "warnuser"} } {

  proc trigger {var warn} {
    if {$warn == "warnuser"} {
      tk_messageBox -type ok -icon error -parent . -title "Protocol error" -message "vwait timeout for $var"
    }
    set $var 1
  }

  if { $delay != 0 } {
    set timerId [after $delay "trigger $var $warn"]
  }

  vwait $var

  if [info exists timerId] { after cancel $timerId }

}

## FROM TK 8.5.9
## ttk::bindMouseWheel $bindtag $command...
################################################################################
# ::bindMouseWheel
#   Adds basic mouse wheel bindings for a bind tag across supported window systems.
# Visibility:
#   Public.
# Inputs:
#   - `bindtag`: Bind tag to attach the mouse wheel bindings to.
#   - `callback`: Command prefix invoked with one extra argument:
#       - `-1` for upward scrolling
#       - `+1` for downward scrolling
# Returns:
#   - None.
# Side effects:
#   - Installs `bind` handlers on `$bindtag` for the current windowing system.
################################################################################
proc bindMouseWheel {bindtag callback} {
    switch -- [tk windowingsystem] {
	x11 {
	    bind $bindtag <ButtonPress-4> [list apply {{callback} {
	        {*}$callback -1
	        return -code break
	    } ::} $callback]
	    bind $bindtag <ButtonPress-5> [list apply {{callback} {
	        {*}$callback +1
	        return -code break
	    } ::} $callback]
	}
	win32 {
	    bind $bindtag <<MWheel>> [list apply {{callback} {
	        {*}$callback [expr {-(%d/120)}]
	        return -code break
	    } ::} $callback]
	}
	aqua {
	    bind $bindtag <MouseWheel> [list apply {{callback} {
	        {*}$callback [expr {-(%D)}]
	        return -code break
	    } ::} $callback]
	}
    }
}

################################################################################
# ::dialogbuttonframe
#   Creates and packs a standard button strip frame for dialogs.
# Visibility:
#   Public.
# Inputs:
#   - `frame`: Frame widget path to create.
#   - `buttonlist`: List of button descriptors; each element is
#     `{name arg...}` for `ttk::button`.
# Returns:
#   - None.
# Side effects:
#   - Creates a `ttk::frame` and one `ttk::button` per list element.
#   - Packs the buttons to the right with consistent padding.
################################################################################
proc dialogbuttonframe {frame buttonlist} {
  ttk::frame $frame
  set bnames {}
  set maxlength 0
  foreach buttonargs $buttonlist {
    set bname $frame.[lindex $buttonargs 0]
    set bargs [lrange $buttonargs 1 end]
    ttk::button $bname {*}$bargs
    set bnames [linsert $bnames 0 $bname]
    set length [string length [$bname cget -text]]
    if {$length > $maxlength} { set length $maxlength}
  }
  if {$maxlength < 7} { set maxlength 7 }
  foreach b $bnames {
    $b configure -width $maxlength -padx 4
    pack $b -side right -padx 4 -pady 4
  }
}

################################################################################
# ::packbuttons
# Visibility:
#   Public.
# Inputs:
#   - `side`: Pack side (e.g. `left` or `right`).
#   - `args`: Widget paths to pack.
# Returns:
#   - None.
# Side effects:
#   - Calls `pack` with standard padding.
################################################################################
proc packbuttons {side args} {
  pack {*}$args -side $side -padx 5 -pady 3
}
################################################################################
# ::packdlgbuttons
# Visibility:
#   Public.
# Inputs:
#   - `args`: Widget paths to pack.
# Returns:
#   - None.
# Side effects:
#   - Packs dialog buttons to the right with standard padding.
################################################################################
proc packdlgbuttons {args} {
  pack {*}$args -side right -padx 5 -pady "15 5"
}
################################################################################
# ::dialogbutton
#   Creates a dialog button with a minimum width derived from its label.
# Visibility:
#   Public.
# Inputs:
#   - `w`: Button widget path.
#   - `args`: Arguments forwarded to `ttk::button`.
# Returns:
#   - The result of `ttk::button`.
# Side effects:
#   - Creates/configures the button widget.
################################################################################
proc dialogbutton {w args} {
  set retval [ttk::button $w {*}$args] ;# -style TButton
  set length [string length [$w cget -text]]
  if {$length < 7} { set length 7 }
  $w configure -width $length
  return $retval
}

################################################################################
# ::dialogbuttonsmall
#   Creates a small dialog button with a minimum width derived from its label.
# Visibility:
#   Public.
# Inputs:
#   - `w`: Button widget path.
#   - `args`: Arguments forwarded to `ttk::button`.
#   - `style` (optional): ttk style name (default: `Small.TButton`).
# Returns:
#   - The result of `ttk::button`.
# Side effects:
#   - Creates/configures the button widget.
################################################################################
proc dialogbuttonsmall {w args {style "Small.TButton"} } {
  set retval [ttk::button $w -style $style {*}$args]
  set length [string length [$w cget -text]]
  if {$length < 7} { set length 7 }
  $w configure -width $length
  return $retval
}

################################################################################
# ::autoscrollframe
#   Creates a frame containing a widget with auto-hiding scrollbars.
# Visibility:
#   Public.
# Inputs:
#   - `args`: `[-bars none|x|y|both] frame type w ?configure-args...?`
# Returns:
#   - The frame widget path.
# Side effects:
#   - Creates/configures the frame and widget if they do not exist.
#   - Adds and manages scrollbars via `::autoscrollBars`.
################################################################################
proc autoscrollframe {args} {
  set bars both
  if {[lindex $args 0] == "-bars"} {
    set bars [lindex $args 1]
    if {$bars != "x" && $bars != "y" && $bars != "none" && $bars != "both"} {
      return -code error "Invalid parameter: -bars $bars"
    }
    set args [lrange $args 2 end]
  }
  if {[llength $args] < 3} {
    return -code error "Insufficient number of parameters"
  }
  set frame [lindex $args 0]
  set type [lindex $args 1]
  set w [lindex $args 2]
  set args [lrange $args 3 end]

  set retval $frame
  if {! [winfo exists $frame]} {
    ttk::frame $frame
    $frame configure -relief sunken -borderwidth 2
  }
  if {! [winfo exists $w]} {
    $type $w
    if {[llength $args] > 0} {
      $w configure {*}$args
    }
    $w configure -relief flat -borderwidth 0
  }

  autoscrollBars $bars $frame $w
  return $retval
}

################################################################################
# ::autoscrollBars
#   Adds auto-hiding scrollbars to a widget gridded in a frame.
# Visibility:
#   Public.
# Inputs:
#   - `bars`: One of `none`, `x`, `y`, `both`.
#   - `frame`: Parent frame widget path.
#   - `w`: Scrollable widget path to configure.
#   - `frame_row` (optional): Grid row to use for the widget.
# Returns:
#   - None.
# Side effects:
#   - Creates ttk scrollbars and configures `$w` x/y view commands.
#   - Initialises `_autoscroll(*)` state and binds mouse wheel scrolling.
################################################################################
proc autoscrollBars {bars frame w {frame_row 0}} {
  global _autoscroll

  grid $w -in $frame -row $frame_row -column 0 -sticky news
  grid rowconfigure $frame $frame_row -weight 1
  grid columnconfigure $frame 0 -weight 1

  if {$bars == "y"  ||  $bars == "both"} {
    ttk::scrollbar $frame.ybar -command [list $w yview] -takefocus 0
    $w configure -yscrollcommand [list _autoscroll $frame.ybar]
    grid $frame.ybar -row $frame_row -column 1 -sticky ns
    grid columnconfigure $frame 1 -weight 0
    set _autoscroll($frame.ybar) 1
    set _autoscroll(time:$frame.ybar) 0
    bindMouseWheel $w "_autoscrollMouseWheel $w $frame.ybar"
  }
  incr frame_row
  if {$bars == "x"  ||  $bars == "both"} {
    ttk::scrollbar $frame.xbar -command [list $w xview] -takefocus 0 -orient horizontal
    $w configure -xscrollcommand [list _autoscroll $frame.xbar]
    grid $frame.xbar -row $frame_row -column 0 -sticky we
    grid rowconfigure $frame $frame_row -weight 0
    set _autoscroll($frame.xbar) 1
    set _autoscroll(time:$frame.xbar) 0
  }
}

################################################################################
# ::_autoscrollMouseWheel
# Visibility:
#   Private.
# Inputs:
#   - `w`: Scrollable widget path.
#   - `bar`: Associated scrollbar widget path.
#   - `direction`: Scroll direction (-1 or +1).
# Returns:
#   - None.
# Side effects:
#   - Scrolls the widget when the scrollbar is currently visible/enabled.
################################################################################
proc _autoscrollMouseWheel {{w} {bar} {direction}} {
  if {$::_autoscroll($bar) == 0} return
  $w yview scroll $direction units
}

array set _autoscroll {}

# _autoscroll
#   This is the "set" command called for auto-scrollbars.
#   If the bar is shown but should not be, it is hidden.
#   If the bar is hidden but should be shown, it is redrawn.
#   Note that once a bar is shown, it will not be removed again for
#   at least a few milliseconds; this is to overcome problematic
#   interactions between the x and y scrollbars where hiding one
#   causes the other to be shown etc. This usually happens because
#   the stupid Tcl/Tk text widget doesn't handle scrollbars well.
#
################################################################################
# ::_autoscroll
#   y/x scrollcommand handler that hides/shows a scrollbar based on range.
# Visibility:
#   Private.
# Inputs:
#   - `bar`: Scrollbar widget path.
#   - `args`: Either `{min max}` or any args forwarded to `$bar set`.
# Returns:
#   - None.
# Side effects:
#   - Shows/hides the scrollbar via `grid configure` / `grid remove`.
#   - Updates `_autoscroll($bar)` and `_autoscroll(time:$bar)`.
################################################################################
proc _autoscroll {bar args} {
  global _autoscroll
  if {[llength $args] == 2} {
    set min [lindex $args 0]
    set max [lindex $args 1]
    if {$min > 0.0  ||  $max < 1.0} {
      if {! $_autoscroll($bar)} {
        grid configure $bar
        set _autoscroll($bar) 1
        set _autoscroll(time:$bar) [clock clicks -milli]
      }
    } else {
      if {[clock clicks -milli] > [expr {$_autoscroll(time:$bar) + 100}]} {
        grid remove $bar
        set _autoscroll($bar) 0
      }
    }
  }
  $bar set {*}$args
}

################################################################################
# ::_autoscrollMap
# Visibility:
#   Private.
# Inputs:
#   - `frame`: Frame widget path.
# Returns:
#   - None.
# Side effects:
#   - None (reserved hook; currently does not perform any action).
################################################################################
proc _autoscrollMap {frame} {
  # wm geometry [winfo toplevel $frame] [wm geometry [winfo toplevel $frame]]
}


# busyCursor, unbusyCursor:
#   Sets all cursors to watch (indicating busy) or back to their normal
#   setting again.

array set scid_busycursor {}
set scid_busycursorState 0

################################################################################
# ::doBusyCursor
#   Recursively applies or restores a “busy” cursor for a widget subtree.
# Visibility:
#   Private.
# Inputs:
#   - `w`: Root widget path.
#   - `flag`: When true, applies `watch`; otherwise restores the prior cursor.
# Returns:
#   - None.
# Side effects:
#   - Reads/stores previous cursor values in `scid_busycursor($w)`.
#   - Configures widget cursors for `$w` and its children.
################################################################################
proc doBusyCursor {w flag} {
  global scid_busycursor
  if {! [winfo exists $w]} { return }
  if {[winfo class $w] == "Menu"} { return }

  if {$flag} {
    if { [ catch { set scid_busycursor($w) [$w cget -cursor] } ] } {
      return
    }
    catch {$w configure -cursor watch}
  } else {
    catch {$w configure -cursor $scid_busycursor($w)} err
  }
  foreach i [winfo children $w] { doBusyCursor $i $flag }
}

################################################################################
# ::busyCursor
#   Enables or disables the busy cursor state for a widget subtree.
# Visibility:
#   Public.
# Inputs:
#   - `w`: Root widget path.
#   - `flag` (optional): When true, enables busy cursor; when false, disables.
# Returns:
#   - None.
# Side effects:
#   - Updates `scid_busycursorState` and calls `::doBusyCursor`.
################################################################################
proc busyCursor {w {flag 1}} {
  global scid_busycursor scid_busycursorState
  if {$scid_busycursorState == $flag} { return }
  set scid_busycursorState $flag
  doBusyCursor $w $flag
}

################################################################################
# ::unbusyCursor
# Visibility:
#   Public.
# Inputs:
#   - `w`: Root widget path.
# Returns:
#   - None.
# Side effects:
#   - Calls `::busyCursor $w 0`.
################################################################################
proc unbusyCursor {w} {busyCursor $w 0}


# addHorizontalRule, addVerticalRule
#   Add a horizontal/vertical rule frame to a window.
#   The optional parameters [x/y]padding and sunken allow the spacing and
#   appearance of the rule to be specified.
#
set horizRuleCounter 0
set vertRuleCounter 0

################################################################################
# ::addHorizontalRule
#   Adds a horizontal separator to a window.
# Visibility:
#   Public.
# Inputs:
#   - `w`: Parent widget path.
#   - `ypadding` (optional): Vertical padding (currently unused by implementation).
#   - `relief` (optional): Visual style hint (kept for compatibility).
#   - `height` (optional): Height hint (kept for compatibility).
# Returns:
#   - None.
# Side effects:
#   - Creates and packs a `ttk::separator` and increments `horizRuleCounter`.
################################################################################
proc addHorizontalRule {w {ypadding 5} {relief sunken} {height 2} } {
  global horizRuleCounter

  ttk::separator $w.line$horizRuleCounter -orient horizontal
  pack $w.line$horizRuleCounter -fill x ;# -pady $ypadding

  # set f [ ttk::frame $w.line$horizRuleCounter -height $height -borderwidth 2 -relief $relief ]
  # pack $f -fill x -pady $ypadding
  incr horizRuleCounter
}

################################################################################
# ::addVerticalRule
#   Adds a vertical separator to a window.
# Visibility:
#   Public.
# Inputs:
#   - `w`: Parent widget path.
#   - `xpadding` (optional): Horizontal padding (currently unused by implementation).
#   - `relief` (optional): Visual style hint (kept for compatibility).
# Returns:
#   - None.
# Side effects:
#   - Creates and packs a `ttk::separator` and increments `vertRuleCounter`.
################################################################################
proc addVerticalRule {w {xpadding 5} {relief sunken}} {
  global vertRuleCounter

  ttk::separator $w.line$vertRuleCounter -orient vertical
  pack $w.line$vertRuleCounter -fill y -side left ;# -padx $xpadding

  # set f [ ttk::frame $w.line$vertRuleCounter -width 2 -borderwidth 2 -relief $relief ]
  # pack $f -fill y -padx $xpadding -side left
  incr vertRuleCounter
}

# progressWindow:
#   Creates a window with a label, progress bar, and (if specified),
#   a cancel button and cancellation command.
#
################################################################################
# ::progressWindow
#   Creates a standard progress dialog with an optional cancel button.
# Visibility:
#   Public.
# Inputs:
#   - `title`: Window title string.
#   - `text`: Primary label text.
#   - `button` (optional): Cancel/close button label (empty disables the button).
#   - `cancelCmdPrefix` (optional): Command prefix invoked when cancelled.
# Returns:
#   - None.
# Side effects:
#   - Creates `.progressWin` and related widgets and grabs focus.
#   - Records/restores focus via `::progressWin_focus`.
#   - Calls `::progressBarSet` to initialise the progress canvas.
################################################################################
proc progressWindow { title text {button ""} {cancelCmdPrefix {progressBarCancel}} } {
  set w .progressWin
  if {[winfo exists $w]} { return }

  set ::progressWin_focus [focus]

  win::createDialog $w 6
  wm resizable $w 0 0
  wm title $w $title

  ttk::frame $w.f
  ttk::label $w.f.t -text $text
  ttk::frame $w.f.cmsg
  ttk_text $w.f.cmsg.text -width 70 -height 14 -wrap word
  autoscrollBars y $w.f.cmsg $w.f.cmsg.text
  canvas $w.f.c -width 400 -height 20 -bg white -relief solid -border 1 -highlightthickness 0
  $w.f.c create rectangle 0 0 0 0 -fill DodgerBlue3 -outline DodgerBlue3 -tags bar
  $w.f.c create text 395 10 -anchor e -font font_Regular -tags time -fill black -text "0:00 / 0:00"
  ttk::button $w.f.cancel -text $button -command [list {*}$cancelCmdPrefix]

  grid $w.f.t -row 0 -columnspan 2 -pady 4 -sticky news
  grid $w.f.cmsg -row 1 -columnspan 2 -pady 4 -sticky news
  grid $w.f.c -row 2 -column 0 -pady 4 -stick w
  grid $w.f.cancel -row 2 -column 1 -padx "10 0"
  grid $w.f -sticky news
  grid rowconfigure $w.f 1 -weight 1
  grid columnconfigure $w.f 0 -weight 1
  grid remove $w.f.cmsg
  if {$button == ""} { grid remove $w.f.cancel }

  # Set up geometry for middle of screen:
  set screenW [winfo screenwidth $w]
  set screenH [winfo screenheight $w]
  set x [expr {($screenW - 400) / 2}]
  set y [expr {($screenH - 40) / 2}]
  wm geometry $w +$x+$y
  grab $w

  progressBarSet $w.f.c 401 21
}

################################################################################
# ::progressBarSet
#   Initialises global progress canvas state for `::progressCallBack`.
# Visibility:
#   Public.
# Inputs:
#   - `canvasname`: Canvas widget path.
#   - `width`: Canvas width in pixels.
#   - `height`: Canvas height in pixels.
# Returns:
#   - None.
# Side effects:
#   - Sets `::progressCanvas(*)` fields and schedules initialisation clearing.
################################################################################
proc progressBarSet { canvasname width height } {
  update idletasks
  set ::progressCanvas(name) $canvasname
  set ::progressCanvas(w) $width
  set ::progressCanvas(h) $height
  set ::progressCanvas(cancel) 0
  set ::progressCanvas(init) 1
  set ::progressCanvas(time) [clock milliseconds]
  after idle { unset ::progressCanvas(init) }
}

################################################################################
# ::progressBarCancel
# Visibility:
#   Public.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Sets `::progressCanvas(cancel)` to request cancellation.
################################################################################
proc progressBarCancel { } {
  set ::progressCanvas(cancel) 1
}


################################################################################
# ::progressCallBack
#   Updates the progress bar and optional log area; breaks when cancelled.
# Visibility:
#   Public.
# Inputs:
#   - `done`: Either `"init"` or a fraction in `[0,1]`.
#   - `msg` (optional): Message line to append to the progress log (if present).
# Returns:
#   - On `"init"`, returns the initialisation flag value.
#   - Otherwise returns nothing meaningful.
# Side effects:
#   - Updates the progress bar canvas and time estimate.
#   - Calls `update` to process events.
#   - Returns `-code break` when cancelled or the window is closed.
################################################################################
proc progressCallBack {done {msg ""}} {
  if {$done == "init"} {
    if {[info exists ::progressCanvas(init)]} {
      return $::progressCanvas(init)
    }
    # No progress bar
    return -code break
  }

  if {! [winfo exists $::progressCanvas(name)] || $::progressCanvas(cancel)} {
    #Interrupted
    return -code break
  }

  set elapsed [expr { [clock milliseconds] - $::progressCanvas(time) }]
  if {$done != 0} {
    set estimated [expr { int($elapsed / double($done) / 1000) }]
    set elapsed [expr { $elapsed / 1000 }]
  } else {
    set elapsed [expr { $elapsed / 1000 }]
    set estimated $elapsed
  }

  set width [expr { int(double($::progressCanvas(w)) * double($done)) }]
  $::progressCanvas(name) coords bar 0 0 $width $::progressCanvas(h)

  set t [format "%d:%02d / %d:%02d" \
      [expr {$elapsed / 60}] [expr {$elapsed % 60}] \
      [expr {$estimated / 60}] [expr {$estimated % 60}]]
  $::progressCanvas(name) itemconfigure time -text $t

  if {$msg != ""} {
    catch {
      set widget "$::progressCanvas(name)msg"
      grid $widget
      append widget ".text"
      $widget insert end "$msg\n"
    }
  }

  update

  if {! [winfo exists $::progressCanvas(name)] || $::progressCanvas(cancel)} {
    #Interrupted
    return -code break
  }
}

################################################################################
# ::changeProgressWindow
# Visibility:
#   Public.
# Inputs:
#   - `newtext`: Replacement text for the progress window label.
# Returns:
#   - None.
# Side effects:
#   - Updates `.progressWin` label when the window exists.
################################################################################
proc changeProgressWindow {newtext} {
  set w .progressWin
  if {[winfo exists $w]} {
    $w.f.t configure -text $newtext
    update idletasks
  }
}

################################################################################
# ::updateProgressWindow
#   Updates the progress bar to reflect completion fraction.
# Visibility:
#   Public.
# Inputs:
#   - `done`: Units completed.
#   - `total`: Units total (0 means treat as complete).
# Returns:
#   - None.
# Side effects:
#   - Calls `::progressCallBack` when `.progressWin` exists.
################################################################################
proc updateProgressWindow {done total} {
  set w .progressWin
  if {! [winfo exists $w]} { return }
  if {$total != 0} {
    set done [expr { double($done) / double($total) }]
  } else {
    set done 1
  }
  ::progressCallBack $done
}

################################################################################
# ::closeProgressWindow
#   Closes the progress window (with an optional “show log then close” flow).
# Visibility:
#   Public.
# Inputs:
#   - `force` (optional): When true, closes immediately.
# Returns:
#   - None.
# Side effects:
#   - Destroys `.progressWin` and releases the grab, or switches into a
#     “close confirmation” state when a log is present.
#   - Restores focus to `::progressWin_focus` when possible.
################################################################################
proc closeProgressWindow {{force false}} {
  set w .progressWin
  if {! [winfo exists $w]} { return }

  if {!$force && [$w.f.cmsg.text index end] != "2.0" } {
    $w.f.cancel configure -text "$::tr(Close)"
    $w.f.cancel configure -command [list closeProgressWindow true]
    grid forget $w.f.c
    grid $w.f.cancel
    $w.f.cmsg.text configure -state disabled
    return
  }
  grab release $w
  destroy $w
  update idletasks
  catch {focus $::progressWin_focus}
}

################################################################################
# ::CreateSelectDBWidget
#   Creates a combobox for selecting an open database and binds it to a variable.
# Visibility:
#   Public.
# Inputs:
#   - `w` (optional): Parent widget path where `$w.lb` is created.
#   - `varname` (optional): Fully-qualified variable name to set on selection.
#   - `ref_base` (optional): Base ID to preselect (defaults to `sc_base current`).
#   - `readOnly` (optional): When false, filters out read-only bases.
# Returns:
#   - None.
# Side effects:
#   - Creates and grids `ttk::combobox $w.lb`.
#   - Binds `<<ComboboxSelected>>` to update `$varname`.
#   - Triggers an initial `<<ComboboxSelected>>` event.
################################################################################
proc CreateSelectDBWidget {{w} {varname} {ref_base ""} {readOnly 1}} {
  set listbases {}
  if {$ref_base == ""} { set ref_base [sc_base current] }
  set tr_database [tr Database]
  set tr_prefix_len [expr {[string length $tr_database] + 1}]
  set selected 0
  foreach i [sc_base list] {
      if {$readOnly || ![sc_base isReadOnly $i]} {
        set fname [::file::BaseName $i]
        if {$i == $ref_base} { set selected [llength $listbases] }
        lappend listbases "$tr_database $i: $fname"
      }
  }

  ttk::combobox $w.lb -values $listbases -state readonly
  grid $w.lb -sticky news
  grid columnconfigure $w 0 -weight 1

  bind $w.lb <<ComboboxSelected>> [list apply {{w varName prefixLen} {
    upvar #0 $varName var
    # The combobox value is of the form "<Database> <n>: <name>".
    # Use `scan` to extract the full (possibly multi-digit) base number.
    #
    # NOTE: This still relies on parsing the displayed string. A more robust
    # approach is to map the selected index to the base ID directly.
    scan [string range [$w get] $prefixLen end] %d var
  } ::} $w.lb $varname $tr_prefix_len]
  $w.lb current $selected
  event generate $w.lb <<ComboboxSelected>>
}

################################################################################
# ::storeEmtComment
#   Stores elapsed move time as a `[%emt ...]` tag in the current move comment.
# Visibility:
#   Public.
# Inputs:
#   - `h`: Hours component.
#   - `m`: Minutes component.
#   - `s`: Seconds component.
# Returns:
#   - None.
# Side effects:
#   - Reads/modifies the current move comment via `sc_pos getComment` /
#     `sc_pos setComment`.
################################################################################
proc storeEmtComment { h m s } {
    set time "[format "%d" $h]:[format "%02d" $m]:[format "%02d" $s]"

    #Replace %emt if present, otherwise prepend it
    if {[regsub {\[%emt\s*.*?\]} [sc_pos getComment] "\[%emt $time\]" comment]} {
      sc_pos setComment "$comment"
    } else {
      sc_pos setComment "\[%emt $time\]$comment"
    }
  }

################################################################################
# ::storeEvalComment
#   Stores an evaluation value as a `[%eval ...]` tag in the current move comment.
# Visibility:
#   Public.
# Inputs:
#   - `value`: Evaluation value to store (format is caller-defined).
# Returns:
#   - None.
# Side effects:
#   - Reads/modifies the current move comment via `sc_pos getComment` /
#     `sc_pos setComment`.
################################################################################
proc storeEvalComment { value } {
    #Replace %eval if present, otherwise prepend it
    if {[regsub {\[%eval\s*.*?\]} [sc_pos getComment] "\[%eval $value\]" comment]} {
      sc_pos setComment "$comment"
    } else {
      sc_pos setComment "\[%eval $value\]$comment"
    }
  }

################################################################################
# ::format_clock
#   Normalises a clock string by removing unnecessary leading zeros.
# Visibility:
#   Public.
# Inputs:
#   - `clk`: Clock string in `h:mm:ss` or `m:ss` style.
# Returns:
#   - A normalised string (e.g. `0:00:05` becomes `0:05`).
# Side effects:
#   - None.
################################################################################
proc format_clock {clk} {
    return "[string trimleft [string range $clk 0 end-4] {0:}][string range $clk end-3 end]"
}

################################################################################
# ::format_clock_from_seconds
#   Converts seconds to a human-readable clock string.
# Visibility:
#   Public.
# Inputs:
#   - `seconds`: Integer seconds (may be negative).
# Returns:
#   - A clock string in `h:mm:ss` or `m:ss` style (with a leading `-` when negative).
# Side effects:
#   - None.
################################################################################
proc format_clock_from_seconds {seconds} {
    set res ""
    if { $seconds < 0 } {
        set res "-"
        set seconds [expr {abs($seconds)}]
    }
    append res [format_clock [format "%d:%02d:%02d" \
        [expr {$seconds / 3600}] \
        [expr {($seconds / 60) % 60}] \
        [expr {$seconds % 60}] ]]
    return $res
}

################################################################################
# ::clock_to_seconds
#   Parses a clock string into seconds.
# Visibility:
#   Public.
# Inputs:
#   - `clk`: Clock string (`h:mm:ss` or `m:ss`) with optional leading `-`.
# Returns:
#   - Integer seconds, or empty string if parsing fails.
# Side effects:
#   - None.
################################################################################
proc clock_to_seconds {clk} {
    if {$clk eq ""} { return "" }

    set sign 1
    if {[string index $clk 0] eq "-"} {
        set sign -1
        set clk [string range $clk 1 end]
    }

    if {[regexp {^([0-9]+):([0-9]{2}):([0-9]{2})$} $clk -> h m s]} {
        # ok
    } elseif {[regexp {^([0-9]+):([0-9]{2})$} $clk -> m s]} {
        set h 0
    } else {
        return ""
    }

    return [expr {$sign * (3600 * $h + 60 * $m + $s)}]
}

################################################################################
# ::gameclock
#   A lightweight chess clock utility used by the main UI and PGN annotations.
################################################################################
namespace eval gameclock {
  array set data {}

  ################################################################################
  # ::gameclock::new
  #   Creates (optionally) and initialises a clock widget instance.
  # Visibility:
  #   Public.
  # Inputs:
  #   - `parent`: Parent widget path (empty string means “no widget”, data-only).
  #   - `n`: Clock slot/ID.
  #   - `size` (optional): Canvas size in pixels.
  #   - `showfall` (optional): When true, shows elapsed-overrun in red.
  # Returns:
  #   - None.
  # Side effects:
  #   - Creates and packs a `canvas` under `$parent` when `parent` is non-empty.
  #   - Initialises `::gameclock::data(*)` for the slot.
  #   - Calls `::gameclock::reset` and `::gameclock::draw`.
  ################################################################################
  proc new { parent n { size 100 } {showfall 0} } {
    global ::gameclock::data
    set data(showfallen$n) $showfall
    set data(id$n) ""
    if {$parent != ""} {
      set data(id$n) $parent.clock$n
      canvas $data(id$n) -height $size -width $size
      pack $data(id$n) -side top -anchor center
      for {set i 1} {$i<13} {incr i} {
        set a [expr {$i/6.*acos(-1)}]
        set x [expr { ($size/2 + (($size-15)/2)*sin($a) ) }]
        set y [expr { ($size/2 - (($size-15)/2)*cos($a) ) }]
        $data(id$n) create text $x $y -text $i -tag clock$n
      }
      bind $data(id$n) <Button-1> [list ::gameclock::toggleClock $n]
    }
    set data(fg$n) "black"
    set data(running$n) 0
    set data(digital$n) 1
    ::gameclock::reset $n
    ::gameclock::draw $n
  }

  ################################################################################
  # ::gameclock::draw
  #   Redraws the analogue/digital clock display and updates main-window fields.
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  # Returns:
  #   - None.
  # Side effects:
  #   - Updates `::gamePlayers(clockW)` / `::gamePlayers(clockB)` for slots 1/2.
  #   - Draws on the associated canvas (if it exists).
  ################################################################################
  proc draw { n } {
    global ::gameclock::data

    #TODO: Hack. For the moment we assume that:
    # -clock 1 is the white clock on the main board
    # -clock 2 is the black clock on the main board
    set sec $data(counter$n)
    set h [format "%d" [expr {abs($sec) / 60 / 60}] ]
    set m [format "%02d" [expr {(abs($sec) / 60) % 60}] ]
    set s [format "%02d" [expr {abs($sec) % 60}] ]
    if {$n == 1} { set ::gamePlayers(clockW) "$h:$m:$s" }
    if {$n == 2} { set ::gamePlayers(clockB) "$h:$m:$s" }

    if {! [winfo exists $data(id$n)]} { return }
    $data(id$n) delete aig$n

    set w [$data(id$n) cget -width ]
    set h [$data(id$n) cget -height ]
    set cx [expr {$w / 2 }]
    set cy [expr {$h / 2 }]
    if {$w < $h} {
      set size [expr {$w - 15 }]
    } else  {
      set size [expr {$h - 15 }]
    }

    if { $sec > 0 && $data(showfallen$n) } {
      set color "red"
    } else  {
      set color $::gameclock::data(fg$n)
    }

    if {$color == "white"} {set fg "black"} else {set fg "white"}

    foreach divisor {30 1800 21600} length "[expr {$size/2 * 0.8}] [expr {$size/2 * 0.7}] [expr {$size/2 * 0.4}]" \
        width {1 2 3} {
          set angle [expr {$sec * acos(-1) / $divisor}]
          set x [expr {$cx + $length * sin($angle)}]
          set y [expr {$cy - $length * cos($angle)}]
          $data(id$n) create line $cx $cy $x $y -width $width -tags aig$n -fill $color
        }
    # draw a digital clock
    if {$data(digital$n)} {
      set m [format "%02d" [expr {abs($sec) / 60}] ]
      set s [format "%02d" [expr {abs($sec) % 60}] ]
      $data(id$n) create text $cx [expr {$cy + $size/4 }] -text "$m:$s" -anchor center -fill $color -tag aig$n
    }
  }

  ################################################################################
  # ::gameclock::every
  #   Ticks the clock and reschedules itself.
  # Visibility:
  #   Private.
  # Inputs:
  #   - `ms`: Tick interval in milliseconds.
  #   - `body`: Command prefix to execute each tick.
  #   - `n`: Clock slot/ID.
  # Returns:
  #   - None.
  # Side effects:
  #   - Increments `::gameclock::data(counter$n)` and schedules `after`.
  ################################################################################
  proc every {ms body n} {
    incr ::gameclock::data(counter$n)
    {*}$body
    if {$::gameclock::data(id$n) == "" ||
        [winfo exists $::gameclock::data(id$n)]} {
      set ::gameclock::data(after$n) [after $ms [list ::gameclock::every $ms $body $n]]
    }
  }

  ################################################################################
  # ::gameclock::getSec
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  # Returns:
  #   - Seconds remaining/elapsed (sign convention as stored in `data(counter$n)`).
  # Side effects:
  #   - None.
  ################################################################################
  proc getSec { n } {
    return [expr {0 - $::gameclock::data(counter$n)}]
  }

  ################################################################################
  # ::gameclock::setSec
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  #   - `value`: Counter value to set.
  # Returns:
  #   - None.
  # Side effects:
  #   - Updates `data(counter$n)` and redraws the clock.
  ################################################################################
  proc setSec { n value } {
    set ::gameclock::data(counter$n) $value
    ::gameclock::draw $n
  }

  ################################################################################
  # ::gameclock::add
  #   Applies an increment (or decrement) to the clock counter and redraws.
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  #   - `value`: Seconds to add (sign is applied by the current counter convention).
  # Returns:
  #   - None.
  # Side effects:
  #   - Updates `data(counter$n)` and redraws the clock.
  ################################################################################
  proc add { n value } {
    set ::gameclock::data(counter$n) [expr {$::gameclock::data(counter$n) - $value }]
    ::gameclock::draw $n
  }


  ################################################################################
  # ::gameclock::reset
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  # Returns:
  #   - None.
  # Side effects:
  #   - Stops any running timer and resets the counter to 0.
  ################################################################################
  proc reset { n } {
    ::gameclock::stop $n
    set ::gameclock::data(counter$n) 0
  }

  ################################################################################
  # ::gameclock::start
  #   Starts ticking the clock once per second.
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  # Returns:
  #   - None.
  # Side effects:
  #   - Sets `data(running$n)` and schedules recurring `after` ticks.
  ################################################################################
  proc start { n } {
    if {$::gameclock::data(running$n)} { return }
    set ::gameclock::data(running$n) 1
    ::gameclock::every 1000 [list draw $n] $n
  }

  ################################################################################
  # ::gameclock::stop
  #   Stops ticking the clock.
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  # Returns:
  #   - `1` when a running clock was stopped, otherwise `0`.
  # Side effects:
  #   - Cancels any scheduled `after` tick and clears running state.
  ################################################################################
  proc stop { n } {
    if {! $::gameclock::data(running$n)} { return 0 }
    set ::gameclock::data(running$n) 0
    if {[info exists ::gameclock::data(after$n)]} {
      after cancel $::gameclock::data(after$n)
      unset ::gameclock::data(after$n)
    }
    return 1
  }

  ################################################################################
  # ::gameclock::storeTimeComment
  #   Stores clock time as a `[%clk ...]` tag in the current move comment.
  # Visibility:
  #   Public.
  # Inputs:
  #   - `color`: Clock slot/ID to read.
  # Returns:
  #   - None.
  # Side effects:
  #   - Reads/modifies the current move comment via `sc_pos getComment` /
  #     `sc_pos setComment`.
  ################################################################################
  proc storeTimeComment { color } {
    set sec [::gameclock::getSec $color]
    set h [format "%d" [expr {abs($sec) / 60 / 60}] ]
    set m [format "%02d" [expr {(abs($sec) / 60) % 60}] ]
    set s [format "%02d" [expr {abs($sec) % 60}] ]
    set time "$h:$m:$s"

    #Replace %clk if present, otherwise prepend it
    if {[regsub {\[%clk\s*.*?\]} [sc_pos getComment] "\[%clk $time\]" comment]} {
      sc_pos setComment "$comment"
    } else {
      sc_pos setComment "\[%clk $time\]$comment"
    }
  }

  ################################################################################
  # ::gameclock::toggleClock
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  # Returns:
  #   - None.
  # Side effects:
  #   - Starts or stops the clock depending on its current state.
  ################################################################################
  proc toggleClock { n } {
    if { $::gameclock::data(running$n) } {
      stop $n
    } else  {
      start $n
    }
  }

  ################################################################################
  # ::gameclock::setColor
  #   Applies a foreground/background theme to the clock widget.
  # Visibility:
  #   Public.
  # Inputs:
  #   - `n`: Clock slot/ID.
  #   - `color`: Colour scheme name (`white` or non-white treated as dark).
  # Returns:
  #   - None.
  # Side effects:
  #   - Configures the clock canvas and updates `data(fg$n)`.
  ################################################################################
  proc setColor { n color } {
    if {$color == "white"} {
      set fg "black"
      set bg "white"
    } else {
      set fg "white"
      set bg "black"
    }
    set ::gameclock::data(fg$n) $fg
    $::gameclock::data(id$n) configure -background $bg
    $::gameclock::data(id$n) itemconfigure clock$n -fill $fg
    $::gameclock::data(id$n) itemconfigure aig$n -fill $fg
  }

  ################################################################################
  # ::gameclock::isRunning
  # Visibility:
  #   Public.
  # Inputs:
  #   - None.
  # Returns:
  #   - `1` when any supported clock slot is running, otherwise `0`.
  # Side effects:
  #   - None.
  ################################################################################
  proc isRunning { } {
    global ::gameclock::data
    catch {
      if {$data(running1) || $data(running2)} { return 1 }
    }
    return 0
  }
}
################################################################################
# ::html
#   HTML export helpers for the current game or filter.
################################################################################
namespace eval html {
  set data {}
  set idx 0

  ################################################################################
  # ::html::exportCurrentFilter
  #   Exports the current filter as HTML (plus supporting assets and PGN).
  # Visibility:
  #   Public.
  # Inputs:
  #   - None.
  # Returns:
  #   - None.
  # Side effects:
  #   - Prompts for an output file path via `tk_getSaveFile`.
  #   - Copies assets into the destination directory.
  #   - Loads games, generates per-game HTML, and exports a `.pgn` file.
  #   - Uses `progressWindow` and `updateProgressWindow`.
  ################################################################################
  proc exportCurrentFilter {} {
    # Check that we have some games to export:
    if {[sc_filter count] == 0} {
      tk_messageBox -title "[tr ScidUp]: Filter empty" -type ok -icon info \
          -message "The filter contains no games."
      return
    }
    set ftype {
      { "HTML files" {".html" ".htm"} }
      { "All files" {"*"} }
    }
    set idir $::initialDir(html)
    set fName [tk_getSaveFile -initialdir $idir -filetypes $ftype -defaultextension ".html" -title "Create an HTML file"]
    if {$fName == ""} { return }
    if {[file extension $fName] != ".html" } {
      append fName ".html"
    }
    set prefix [file rootname [file tail $fName] ]
    set dirtarget [file dirname $fName]
    set sourcedir [file join $::scidShareDir html]
    catch {file copy -force [file join $sourcedir bitmaps] $dirtarget}
    catch {file copy -force [file join $sourcedir scid.js] $dirtarget}
    catch {file copy -force [file join $sourcedir scid.css] $dirtarget}
    # writeIndex "[file join $dirtarget $prefix].html" $prefix
    progressWindow [tr ScidUp] "Exporting games..."
    set savedGameNum [sc_game number]
    set gn [sc_filter first]
    set players {}
    set ::html::cancelHTML 0
    set total [sc_filter count]

    # build the list of matches
    set idx 1
    while {$gn != 0 && ! $::html::cancelHTML} {
      updateProgressWindow $idx $total
      sc_game load $gn
      set pl "[sc_game tags get White] - [sc_game tags get Black]"
      lappend players $pl
      set gn [sc_filter next]
      incr idx
    }

    set idx 1
    set gn [sc_filter first]
    while {$gn != 0 && ! $::html::cancelHTML} {
      updateProgressWindow $idx $total
      sc_game load $gn
      fillData
      set pl "[sc_game tags get White] - [sc_game tags get Black]"
      toHtml $::html::data $idx $dirtarget $prefix $players $pl [sc_game tags get "Event"] [sc_game tags get "ECO"] [sc_game info result] [sc_game tags get "Date"]
      set gn [sc_filter next]
      incr idx
    }

    closeProgressWindow
    exportPGN "[file join $dirtarget $prefix].pgn" "filter"
    sc_game load $savedGameNum
  }

  ################################################################################
  # ::html::sc_progressBar
  #   Cancels HTML export (used as a progress window cancel callback).
  # Visibility:
  #   Public.
  # Inputs:
  #   - None.
  # Returns:
  #   - None.
  # Side effects:
  #   - Sets `::html::cancelHTML`.
  ################################################################################
  proc sc_progressBar {} {
    set ::html::cancelHTML 1
  }

  ################################################################################
  # ::html::exportCurrentGame
  #   Exports the current game as HTML (plus supporting assets and PGN).
  # Visibility:
  #   Public.
  # Inputs:
  #   - None.
  # Returns:
  #   - None.
  # Side effects:
  #   - Prompts for an output file path via `tk_getSaveFile`.
  #   - Copies assets into the destination directory.
  #   - Exports the current game as HTML and as `.pgn`.
  ################################################################################
  proc exportCurrentGame {} {

    set ftype {
      { "HTML files" {".html" ".htm"} }
      { "All files" {"*"} }
    }
    set idir $::initialDir(html)
    set fName [tk_getSaveFile -initialdir $idir -filetypes $ftype -defaultextension ".html" -title "Create an HTML file"]
    if {[file extension $fName] != ".html" && [file extension $fName] != ".htm" } {
      append fName ".html"
    }
    if {$fName == ""} { return }
    set prefix [file rootname [file tail $fName] ]
    set dirtarget [file dirname $fName]
    set sourcedir [file join $::scidShareDir html]
    catch { file copy -force [file join $sourcedir bitmaps] $dirtarget }
    catch { file copy -force [file join $sourcedir scid.js] $dirtarget }
    catch { file copy -force [file join $sourcedir scid.css] $dirtarget }

    fillData
    set players [list "[sc_game tags get White] - [sc_game tags get Black]"]
    toHtml $::html::data -1 $dirtarget $prefix $players [lindex $players 0] \
        [sc_game tags get "Event"] [sc_game tags get "ECO"] \
        [sc_game info result] [sc_game tags get "Date"]
    exportPGN "[file join $dirtarget $prefix].pgn" "current"
  }

  # Dictionary mapping from special characters to their entities. (from tcllib)
  variable entities {
    \xa0 &nbsp; \xa1 &iexcl; \xa2 &cent; \xa3 &pound; \xa4 &curren;
    \xa5 &yen; \xa6 &brvbar; \xa7 &sect; \xa8 &uml; \xa9 &copy;
    \xaa &ordf; \xab &laquo; \xac &not; \xad &shy; \xae &reg;
    \xaf &macr; \xb0 &deg; \xb1 &plusmn; \xb2 &sup2; \xb3 &sup3;
    \xb4 &acute; \xb5 &micro; \xb6 &para; \xb7 &middot; \xb8 &cedil;
    \xb9 &sup1; \xba &ordm; \xbb &raquo; \xbc &frac14; \xbd &frac12;
    \xbe &frac34; \xbf &iquest; \xc0 &Agrave; \xc1 &Aacute; \xc2 &Acirc;
    \xc3 &Atilde; \xc4 &Auml; \xc5 &Aring; \xc6 &AElig; \xc7 &Ccedil;
    \xc8 &Egrave; \xc9 &Eacute; \xca &Ecirc; \xcb &Euml; \xcc &Igrave;
    \xcd &Iacute; \xce &Icirc; \xcf &Iuml; \xd0 &ETH; \xd1 &Ntilde;
    \xd2 &Ograve; \xd3 &Oacute; \xd4 &Ocirc; \xd5 &Otilde; \xd6 &Ouml;
    \xd7 &times; \xd8 &Oslash; \xd9 &Ugrave; \xda &Uacute; \xdb &Ucirc;
    \xdc &Uuml; \xdd &Yacute; \xde &THORN; \xdf &szlig; \xe0 &agrave;
    \xe1 &aacute; \xe2 &acirc; \xe3 &atilde; \xe4 &auml; \xe5 &aring;
    \xe6 &aelig; \xe7 &ccedil; \xe8 &egrave; \xe9 &eacute; \xea &ecirc;
    \xeb &euml; \xec &igrave; \xed &iacute; \xee &icirc; \xef &iuml;
    \xf0 &eth; \xf1 &ntilde; \xf2 &ograve; \xf3 &oacute; \xf4 &ocirc;
    \xf5 &otilde; \xf6 &ouml; \xf7 &divide; \xf8 &oslash; \xf9 &ugrave;
    \xfa &uacute; \xfb &ucirc; \xfc &uuml; \xfd &yacute; \xfe &thorn;
    \xff &yuml; \u192 &fnof; \u391 &Alpha; \u392 &Beta; \u393 &Gamma;
    \u394 &Delta; \u395 &Epsilon; \u396 &Zeta; \u397 &Eta; \u398 &Theta;
    \u399 &Iota; \u39A &Kappa; \u39B &Lambda; \u39C &Mu; \u39D &Nu;
    \u39E &Xi; \u39F &Omicron; \u3A0 &Pi; \u3A1 &Rho; \u3A3 &Sigma;
    \u3A4 &Tau; \u3A5 &Upsilon; \u3A6 &Phi; \u3A7 &Chi; \u3A8 &Psi;
    \u3A9 &Omega; \u3B1 &alpha; \u3B2 &beta; \u3B3 &gamma; \u3B4 &delta;
    \u3B5 &epsilon; \u3B6 &zeta; \u3B7 &eta; \u3B8 &theta; \u3B9 &iota;
    \u3BA &kappa; \u3BB &lambda; \u3BC &mu; \u3BD &nu; \u3BE &xi;
    \u3BF &omicron; \u3C0 &pi; \u3C1 &rho; \u3C2 &sigmaf; \u3C3 &sigma;
    \u3C4 &tau; \u3C5 &upsilon; \u3C6 &phi; \u3C7 &chi; \u3C8 &psi;
    \u3C9 &omega; \u3D1 &thetasym; \u3D2 &upsih; \u3D6 &piv;
    \u2022 &bull; \u2026 &hellip; \u2032 &prime; \u2033 &Prime;
    \u203E &oline; \u2044 &frasl; \u2118 &weierp; \u2111 &image;
    \u211C &real; \u2122 &trade; \u2135 &alefsym; \u2190 &larr;
    \u2191 &uarr; \u2192 &rarr; \u2193 &darr; \u2194 &harr; \u21B5 &crarr;
    \u21D0 &lArr; \u21D1 &uArr; \u21D2 &rArr; \u21D3 &dArr; \u21D4 &hArr;
    \u2200 &forall; \u2202 &part; \u2203 &exist; \u2205 &empty;
    \u2207 &nabla; \u2208 &isin; \u2209 &notin; \u220B &ni; \u220F &prod;
    \u2211 &sum; \u2212 &minus; \u2217 &lowast; \u221A &radic;
    \u221D &prop; \u221E &infin; \u2220 &ang; \u2227 &and; \u2228 &or;
    \u2229 &cap; \u222A &cup; \u222B &int; \u2234 &there4; \u223C &sim;
    \u2245 &cong; \u2248 &asymp; \u2260 &ne; \u2261 &equiv; \u2264 &le;
    \u2265 &ge; \u2282 &sub; \u2283 &sup; \u2284 &nsub; \u2286 &sube;
    \u2287 &supe; \u2295 &oplus; \u2297 &otimes; \u22A5 &perp;
    \u22C5 &sdot; \u2308 &lceil; \u2309 &rceil; \u230A &lfloor;
    \u230B &rfloor; \u2329 &lang; \u232A &rang; \u25CA &loz;
    \u2660 &spades; \u2663 &clubs; \u2665 &hearts; \u2666 &diams;
    \x22 &quot; \x26 &amp; \x3C &lt; \x3E &gt; \u152 &OElig;
    \u153 &oelig; \u160 &Scaron; \u161 &scaron; \u178 &Yuml;
    \u2C6 &circ; \u2DC &tilde; \u2002 &ensp; \u2003 &emsp; \u2009 &thinsp;
    \u200C &zwnj; \u200D &zwj; \u200E &lrm; \u200F &rlm; \u2013 &ndash;
    \u2014 &mdash; \u2018 &lsquo; \u2019 &rsquo; \u201A &sbquo;
    \u201C &ldquo; \u201D &rdquo; \u201E &bdquo; \u2020 &dagger;
    \u2021 &Dagger; \u2030 &permil; \u2039 &lsaquo; \u203A &rsaquo;
    \u20AC &euro;
  }
  ################################################################################
  # ::html::html_entities
  #   Replaces special characters with HTML entities.
  # Visibility:
  #   Private.
  # Inputs:
  #   - `s`: String to escape.
  # Returns:
  #   - Escaped HTML string.
  # Side effects:
  #   - None.
  ################################################################################
  proc html_entities {s} {
    variable entities
    return [string map $entities $s]
  }

  ################################################################################
  # ::html::toHtml
  #   Writes an HTML page for a game (or a multi-game export).
  # Visibility:
  #   Private.
  # Inputs:
  #   - `dt`: Data list as produced by `::html::fillData`.
  #   - `game`: Game index, or `-1` for single-game export.
  #   - `dirtarget`: Output directory path.
  #   - `prefix`: Output filename prefix.
  #   - `players` (optional): Full game list for navigation.
  #   - `this_players` (optional): Label for the current game.
  #   - `event` (optional), `eco` (optional), `result` (optional), `date` (optional).
  # Returns:
  #   - None.
  # Side effects:
  #   - Writes one `.html` file under `dirtarget`.
  ################################################################################
  proc toHtml { dt game dirtarget prefix {players ""} {this_players ""} {event ""} {eco "ECO"} {result "*"} {date ""} } {

    if { $game != -1 } {
      set f [open "[file join $dirtarget $prefix]_${game}.html" w]
    } else  {
      set f [open "[file join $dirtarget $prefix].html" w]
    }
    # header
    puts $f "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">"
    puts $f "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">"
    puts $f "<head>"
    puts $f "<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />"
    puts $f "<link rel=\"stylesheet\" type=\"text/css\" href=\"scid.css\" />"
    puts $f "<script src=\"scid.js\" type=\"text/javascript\"></script>"
    puts $f "<script type=\"text/javascript\">"
    puts $f "// <!\[CDATA\["
    puts $f "movesArray = new Array("
	    set dtLastIdx [expr {[llength $dt] - 1}]
	    for {set i 0} {$i<[llength $dt]} {incr i} {
	      array set elt [lindex $dt $i]
	      puts -nonewline $f "\"$elt(fen) $elt(prev) $elt(next)\""
	      if {$i < $dtLastIdx} { puts $f "," }
	    }
    puts $f ");"
    puts $f "var current = 0;"
    puts $f "var prefix = \"$prefix\";"
    puts $f "// \]\]>"
    puts $f "</script>"
    puts $f "<title>ScidUp</title>"
    puts $f "<meta content=\"ScidUp\" name=\"author\" />"
    puts $f "</head>"
    puts $f "<body onload=\"doinit()\" onkeydown=\"handlekey(event)\">"
    puts $f "<div id=\"framecontent\">"
    puts $f "<div class=\"innertube\">"
    # diagram
    puts $f "<div id=\"diagram\"><!-- diagram goes here --></div>"
    # navigation
    puts $f "<div id=\"nav\" style=\"text-align: center\"><!-- navigation goes here -->"
    puts $f "<form action=\"#\">"
    puts $f "<p>"
    puts $f "<input type='button' value=' &darr;&uarr; ' onclick='rotate()' /> <input type='button' value=' |&lt; ' onclick='jump(0)' /> <input type='button' value=' &lt; ' onclick='moveForward(0)' /> <input type='button' value=' &gt; ' onclick='moveForward(1)' /> <input type='button' value=' &gt;| ' onclick='jump(1)' /> "
    puts $f "</p><p>"
    # other games navigation
    puts $f "<select name=\"gameselect\" id=\"gameselect\" size=\"1\" onchange=\"gotogame()\">"
    set i 1
    foreach l $players {
      if { $game == $i } {
        puts $f "<option  selected=\"selected\">$i. [html_entities $l]</option>"
      } else  {
        puts $f "<option>$i. [html_entities $l]</option>"
      }
      incr i
    }
    puts $f "</select>"
    puts $f "</p><p>"
    puts $f "<input type=\"button\" value=\"&lt;--\" onclick=\"gotoprevgame()\" /> &nbsp; <input type=\"button\" value=\"--&gt;\" onclick=\"gotonextgame()\" />"
    puts $f "</p><p>"
    puts $f "<a href=\"${prefix}.pgn\">${prefix}.pgn</a>"
    puts $f "</p>"
    puts $f "</form>"
    puts $f "</div>"
    puts $f "</div>"
    puts $f "</div>"
    puts $f "<div id=\"maincontent\">"
    puts $f "<div class=\"innertube\">"
    puts $f "<div id=\"moves\"><!-- moves go here -->"
    # game header
    puts $f "<span class=\"hPlayers\"> [html_entities $this_players]</span>"
    puts $f "<span class=\"hEvent\"><br /> [html_entities $event]</span>"
    puts $f "<span class=\"hEvent\"><br />\[$date\]</span>"
    puts $f "<span class=\"hAnnot\"><br />\[$eco\]</span>"
    puts $f "<p>"
    # link moves
    set prevdepth 0
    set prevvarnumber 0
    for {set i 0} {$i<[llength $dt]} {incr i} {
      array set elt [lindex $dt $i]
      if {$elt(depth) == 0} {
        set class "V0"
      } elseif {$elt(depth) == 1} {
        set class "V1"
      } else {
        set class "V2"
      }
      if { $prevdepth == $elt(depth) && $prevvarnumber != $elt(var) } {
        puts $f "<span class=\"VC\">\]</span></div>"
        puts $f "<div class=\"var\"><span class=\"VC\">\[</span>"
      } else {
        while { $prevdepth > $elt(depth) } {
            puts $f "<span class=\"VC\">\]</span></div>"
            set prevdepth [expr {$prevdepth - 1}]
        }
        while { $prevdepth < $elt(depth) } {
            puts $f "<div class=\"var\"><span class=\"VC\">\[</span>"
            set prevdepth [expr {$prevdepth + 1}]
        }
      }
      set prevvarnumber $elt(var)
      # id = "mv1" not "id=1" now
      set nag [html_entities $elt(nag)]
      set comment [html_entities $elt(comment)]
      puts $f "<a href=\"javascript:gotoMove($elt(idx))\" id=\"mv$elt(idx)\" class=\"$class\">$elt(move)$nag</a>"
      if {$elt(diag)} {
        insertMiniDiag $elt(fen) $f
      }
      if {$comment != ""} {
        puts $f "<span class=\"VC\">$comment</span>"
      }
    }
    while { $prevdepth > 0 } {
        puts $f "<span class=\"VC\">\]</span></div>"
        set prevdepth [expr {$prevdepth - 1}]
    }

    puts $f "<br /><span class=\"VH\">$result</span>"
    puts $f "<p>"
    puts $f "<a href=\"https://github.com/bahmanm/scidup\" style=\"font-size: 0.8em\">Created with ScidUp</a>"
    puts $f "</div>"
    puts $f "</div>"
    puts $f "</div>"
    puts $f "</body>"
    puts $f "</html>"
    close $f
  }

  ################################################################################
  # ::html::colorSq
  #   Returns the CSS class name for a board square colour.
  # Visibility:
  #   Private.
  # Inputs:
  #   - `sq`: Square index (0-based).
  # Returns:
  #   - `"bs"` for black squares, `"ws"` for white squares.
  # Side effects:
  #   - None.
  ################################################################################
  proc colorSq {sq} {
    if { [expr {$sq % 2}] == 1 && [expr {int($sq / 8) %2 }] == 0 || [expr {$sq % 2}] == 0 && [expr {int($sq / 8) %2 }] == 1 } {
      return "bs"
    } else {
      return "ws"
    }
  }

  ################################################################################
  # ::html::piece2gif
  #   Maps a FEN piece character to a bitmap name prefix.
  # Visibility:
  #   Private.
  # Inputs:
  #   - `piece`: Single character piece designator or space.
  # Returns:
  #   - Bitmap name string (e.g. `wk`, `bp`, `sq`).
  # Side effects:
  #   - None.
  ################################################################################
  proc piece2gif {piece} {
    if {$piece == "K"} { return "wk" }
    if {$piece == "k"} { return "bk" }
    if {$piece == "Q"} { return "wq" }
    if {$piece == "q"} { return "bq" }
    if {$piece == "R"} { return "wr" }
    if {$piece == "r"} { return "br" }
    if {$piece == "B"} { return "wb" }
    if {$piece == "b"} { return "bb" }
    if {$piece == "N"} { return "wn" }
    if {$piece == "n"} { return "bn" }
    if {$piece == "P"} { return "wp" }
    if {$piece == "p"} { return "bp" }
    if {$piece == " "} { return "sq" }
  }

  ################################################################################
  # ::html::insertMiniDiag
  #   Writes a miniature board diagram as an HTML table for a FEN.
  # Visibility:
  #   Private.
  # Inputs:
  #   - `fen`: FEN board portion (piece placement).
  #   - `f`: File channel to write to.
  # Returns:
  #   - None.
  # Side effects:
  #   - Writes HTML to the given channel.
  ################################################################################
  proc insertMiniDiag {fen f} {

    set square 0
    set space " "
    puts $f "<table Border=0 CellSpacing=0 CellPadding=0><tr>"

    for {set i 0} {$i < [string length $fen]} {incr i} {
      set l [string range $fen $i $i ]
      set res [scan $l "%d" c]
      if {$res == 1} {
        if  { $c >= 1 && $c <= 8 } {
          for { set j 0} {$j < $c} {incr j} {
            puts $f "<td class=\"[colorSq $square]\"><img border=0 align=\"left\" src=\"bitmaps/mini/[piece2gif $space].gif\"></td>"
            incr square
          }
        }
      } elseif {$l == "/"}  {
        puts $f "</tr><tr>"
      } else  {
        puts $f "<td class=\"[colorSq $square]\"><img border=0 align=\"left\" src=\"bitmaps/mini/[piece2gif $l].gif\"></td>"
        incr square
      }
    }

    puts $f "</tr></table>"
  }

  ################################################################################
  # ::html::fillData
  #   Builds the move/variation navigation data for the current game.
  # Visibility:
  #   Private.
  # Inputs:
  #   - None.
  # Returns:
  #   - None.
  # Side effects:
  #   - Resets `::html::data` and `::html::idx` and walks the current game.
  ################################################################################
  proc fillData {} {
    set ::html::data {}
    set ::html::idx -1
    sc_move start
    parseGame
  }

  ################################################################################
  # ::html::parseGame
  #   Traverses the current game, recording mainline and variations into `data`.
  # Visibility:
  #   Private.
  # Inputs:
  #   - `prev` (optional): Previous element index sentinel; internal use.
  # Returns:
  #   - None.
  # Side effects:
  #   - Appends to `::html::data` via `::html::recordElt` and manages variation
  #     recursion via `sc_var`.
  ################################################################################
  proc parseGame { {prev -2} } {
    global ::html::data ::html::idx

    set already_written 0

    set dots 0

    while {1} {
      if { ! $already_written } {
        recordElt $dots $prev
        set dots 0
        set prev -2
      } else {
        set dots 1
      }
      set already_written 0

      # handle variants
      if {[sc_var count]>0} {
        # First write the move in the current line for which variations exist
        #
        if { ![sc_pos isAt vend]} {
          sc_move forward
          recordElt $dots $prev
          sc_move back
          set lastIdx $idx
          set already_written 1
        }
        for {set v 0} {$v<[sc_var count]} {incr v} {
          sc_var enter $v
          # in order to get the comment before first move
          sc_move back
          parseGame -1
          sc_var exit
        }
        #update the "next" token
        array set elt [lindex $data $lastIdx]
        set elt(next) [expr {$idx + 1}]
        lset data $lastIdx [array get elt]
        #update the "previous" token
        set prev $lastIdx
      }

      if {[sc_pos isAt vend]} { break }
      sc_move forward
    }
  }

  ################################################################################
  # ::html::recordElt
  #   Records a single move element into `::html::data`.
  # Visibility:
  #   Private.
  # Inputs:
  #   - `dots`: Whether to include ellipsis move formatting.
  #   - `prev` (optional): Previous element index sentinel; internal use.
  # Returns:
  #   - None.
  # Side effects:
  #   - Reads game state via `sc_pos`, `sc_move`, `sc_var`, and `sc_game`.
  #   - Appends an element (as an array serialisation) to `::html::data`.
  ################################################################################
  proc recordElt { dots {prev -2} } {
    global ::html::data ::html::idx

    array set elt {}

    incr idx
    set elt(idx) $idx
    set elt(fen) [lindex [split [sc_pos fen]] 0]
    if {$prev != -2} {
      set elt(prev) $prev
    } else  {
      set elt(prev) [expr {$idx-1}]
    }

    set nag [sc_pos getNags]
    if {$nag == "0"} { set nag "" }
    if {[string match "*D *" $nag] || [string match "*# *" $nag]} {
      set elt(diag) 1
    } else  {
      set elt(diag) 0
    }
    set nag [regsub -all "D " $nag "" ]
    set nag [regsub -all "# " $nag "" ]
    set elt(nag) $nag
    set comment [sc_pos getComment]
    set comment [regsub -all "\[\x5B\]%draw (.)+\[\x5D\]" $comment ""]
    set elt(comment) $comment
    set elt(depth) [sc_var level]
    set elt(var) [sc_var number]
    if {![sc_pos isAt vend]} {
      set elt(next) [expr {$idx +1 }]
    } else  {
      set elt(next) -1
    }

    set m [sc_game info previousMove]
    set mn [sc_pos moveNumber]

    set elt(move) ""
    if {[sc_pos side] == "black" && $m != ""} {
      set elt(move) "$mn.$m"
    } else {

      if {! [sc_pos isAt vstart] } {
        sc_move back
        set pnag [sc_pos getNags]
        if {$pnag == "0"} { set pnag "" }
        if {[string match "*D *" $pnag] || [string match "*# *" $pnag]} {
          set pdiag 1
        } else  {
          set pdiag 0
        }
        if {  [sc_pos isAt vstart] ||  [sc_pos getComment] != "" || $pdiag > 0 } {
          set dots 1
        }
        sc_move forward
      }

      if {$dots && $m != ""} {
        set elt(move) "[expr {$mn -1}]. ... $m"
      } else  {
        set elt(move) $m
      }

    }

    lappend ::html::data [array get elt]

  }

  ################################################################################
  # proc writeIndex {fn prefix} {
  # set f [open $fn w]
  # puts $f "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">"
  # puts $f "<html>"
  # puts $f "<head>"
  # puts $f "<meta content=\"text/html; charset=ISO-8859-1\" http-equiv=\"content-type\">"
  # puts $f "<title>Scid</title>"
  # puts $f "<meta content=\"Scid\" name=\"author\">"
  # puts $f "</head>"
  # puts $f "<frameset BORDER=\"0\" FRAMEBORDER=\"0\" FRAMESPACING=\"0\" COLS=\"380,*\">"
  # puts $f "<frameset BORDER=\"0\" FRAMEBORDER=\"0\" FRAMESPACING=\"0\" ROWS=\"380,*\">"
  # puts $f "<frame NAME=\"diagram\" SCROLLING=\"Auto\">"
  # puts $f "<frame NAME=\"nav\" SRC=\"${prefix}_nav.html\" SCROLLING=\"Auto\">"
  # puts $f "</frameset>"
  # puts $f "<frame NAME=\"moves\" SRC=\"${prefix}_1.html\" SCROLLING=\"Auto\">"
  # puts $f "</frameset>"
  # puts $f "</html>"
  # close $f
  # }
  ################################################################################
  # ::html::exportPGN
  #   Exports a selection of games to a PGN file.
  # Visibility:
  #   Private.
  # Inputs:
  #   - `fName`: Output PGN file path.
  #   - `selection`: Export selector (e.g. `filter` or `current`).
  # Returns:
  #   - None.
  # Side effects:
  #   - Calls `sc_base export` and may show a `progressWindow`.
  ################################################################################
  proc exportPGN { fName selection } {
    if {$selection == "filter"} {
      progressWindow [tr ScidUp] "Exporting games..." $::tr(Cancel)
    }
    sc_base export $selection "PGN" $fName -append 0 -starttext "" -endtext "" -comments 1 -variations 1 \
        -space 1 -symbols 1 -indentC 0 -indentV 0 -column 0 -noMarkCodes 1 -convertNullMoves 1
    if {$selection == "filter"} {
      closeProgressWindow
    }
  }

}
################################################################################
#
################################################################################

# end of misc.tcl

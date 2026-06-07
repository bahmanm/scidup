# Copyright (C) 2018-2019 Fulvio Benini
#
# This file is part of Scid (Shane's Chess Information Database).
#
# Scid is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation.
#
# Scid is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Scid.  If not, see <http://www.gnu.org/licenses/>.

namespace eval ::search {}

################################################################################
# ::search::Open
#   Creates a search window using a shared search framework.
# Visibility:
#   Public.
# Inputs:
#   - ref_base: Initial database selection value passed to `CreateSelectDBWidget`.
#     The selected database is stored in `::search::dbase_($w)`.
#   - ref_filter: Initial filter name for the search window.
#   - title: Translation key used for the window title (and window path suffix).
#   - create_subwnd: Callback that creates the search-specific widgets. It must
#     return an `options_cmd` command which supports the following call shapes:
#       - `$options_cmd <widgetPath>`: Returns a widget configuration list (used
#         to configure the presets menubutton).
#       - `$options_cmd reset`: Resets the search-specific option values.
#       - `$options_cmd`: Returns a list whose first element is the options list
#         to pass to `sc_filter search`, and whose optional second element is a
#         secondary options list for a follow-up "ignore colours" search.
# Returns:
#   - None.
# Side effects:
#   - Creates (and may destroy/recreate) the dialog `.wnd_$title`.
#   - Writes `::search::dbase_($w)` and `::search::filter_($w)`.
#   - Registers a variable trace and event bindings to keep the UI in sync.
#   - Calls `::search::refresh_` to populate filter size information.
################################################################################
proc ::search::Open {ref_base ref_filter title create_subwnd} {
	set w ".wnd_$title"
	if {[winfo exists $w]} { destroy $w }
	::win::createDialog $w
	::setTitle $w [::tr $title]

	grid [ttk::frame $w.refdb] -sticky news
	CreateSelectDBWidget "$w.refdb" "::search::dbase_($w)" "$ref_base"
	trace add variable ::search::dbase_($w) write ::search::use_dbfilter_
	set ::search::filter_($w) $ref_filter

	grid [ttk::frame $w.options] -sticky news
	set options_cmd [$create_subwnd $w.options]

	grid [ttk::labelframe $w.filterOp] -sticky news -pady 8
	if {![info exists ::search::filterOp_($w)]} {
		set ::search::filterOp_($w) reset
	}
	ttk::radiobutton $w.filterOp.and   -text [::tr FilterAnd]    -variable ::search::filterOp_($w) -value and
	ttk::radiobutton $w.filterOp.or    -text [::tr FilterOr]     -variable ::search::filterOp_($w) -value or
	ttk::radiobutton $w.filterOp.reset -text [::tr FilterIgnore] -variable ::search::filterOp_($w) -value reset
	grid $w.filterOp.and $w.filterOp.or $w.filterOp.reset -ipadx 8

	grid [ttk::frame $w.buttons] -sticky news
	ttk::menubutton $w.buttons.save -text [::tr Presets] -direction above
		$w.buttons.save configure {*}[$options_cmd $w.buttons.save]
		ttk::button $w.buttons.reset_values -text [::tr Defaults] \
			-command [list apply {{w options_cmd} {
				set ::search::filterOp_($w) reset
				{*}$options_cmd reset
			} ::} $w $options_cmd]
		ttk::button $w.buttons.search_new -text "[tr Search] ([tr GlistNewSort] [tr Filter])" \
			-command [list ::search::start_ 1 $w $options_cmd]
		ttk::button $w.buttons.search -text [::tr Search] \
			-command [list ::search::start_ 0 $w $options_cmd]
		grid $w.buttons.save $w.buttons.reset_values x $w.buttons.search_new $w.buttons.search -sticky w -padx "0 5"
	grid columnconfigure $w.buttons 2 -weight 1

	ttk::button $w.buttons.stop -text [::tr Stop] -command progressBarCancel
	canvas $w.progressbar -width 300 -height 20 -bg white -relief solid -border 1 -highlightthickness 0
	$w.progressbar create rectangle 0 0 0 0 -fill blue -outline blue -tags bar
	$w.progressbar create text 295 10 -anchor e -font font_Regular -tags time
	grid $w.buttons.stop -row 0 -column 0
	grid $w.progressbar -in $w.buttons -row 0 -column 1 -columnspan 4
	progressbar_ $w hide

	bind $w <Return> [list ${w}.buttons.search invoke]
	bind $w.buttons.search <Destroy> [list ::search::forget_window_ $w]
	bind $w <<NotifyFilter>> [list apply {{w} {
		lassign %d dbase filter
		if {[::search::tracks_filter_ $w $dbase $filter]} {
			::search::refresh_ $w
		}
	}} $w]

	::search::refresh_ $w
}

################################################################################
# ::search::CloseAll
# Visibility:
#   Public.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Destroys all active search windows tracked in `::search::dbase_`.
################################################################################
proc ::search::CloseAll {} {
	foreach {w} [array names ::search::dbase_] {
		destroy $w
	}
}

################################################################################
# ::search::forget_window_
#   Removes tracked state for a search window.
# Visibility:
#   Private.
# Inputs:
#   - w: Search window path.
# Returns:
#   - None.
# Side effects:
#   - Clears the tracked database for the window.
################################################################################
proc ::search::forget_window_ {w} {
	unset -nocomplain ::search::dbase_($w)
}

################################################################################
# ::search::tracks_filter_
#   Checks whether a search window currently tracks the given database/filter.
# Visibility:
#   Private.
# Inputs:
#   - w: Search window path.
#   - dbase: Database handle/slot identifier.
#   - filter: Filter name.
# Returns:
#   - 1 when the tracked state matches, 0 otherwise.
################################################################################
proc ::search::tracks_filter_ {w dbase filter} {
	if {![info exists ::search::dbase_($w)] || ![info exists ::search::filter_($w)]} {
		return 0
	}
	expr {$dbase eq $::search::dbase_($w) && $filter eq $::search::filter_($w)}
}

################################################################################
# ::search::DatabaseModified
#   Refreshes any open search windows that target the given database.
# Visibility:
#   Public.
# Inputs:
#   - dbase: Database handle/slot identifier.
# Returns:
#   - None.
# Side effects:
#   - Calls `::search::refresh_` for each matching search window.
################################################################################
proc ::search::DatabaseModified {dbase} {
	foreach {w w_base} [array get ::search::dbase_] {
		if {$dbase == $w_base} {
			::search::refresh_ $w
		}
	}
}

################################################################################
# ::search::refresh_
#   Updates the filter-operation label with the current filter/game counts.
# Visibility:
#   Private.
# Inputs:
#   - w: Search window path.
# Returns:
#   - None.
# Side effects:
#   - Calls `sc_filter sizes` and updates `$w.filterOp` label text.
#   - Destroys `w` if filter sizes cannot be retrieved.
################################################################################
proc ::search::refresh_ {w} {
	if {[catch {
		lassign [sc_filter sizes $::search::dbase_($w) $::search::filter_($w)] filterSz gameSz
	}]} {
		destroy $w
		return
	}
	set n_games [::windows::gamelist::formatFilterText $filterSz $gameSz]
	$w.filterOp configure -text "[::tr FilterOperation] ($n_games)"
}

################################################################################
# ::search::use_dbfilter_
#   Switches the active filter reference for a window to `dbfilter`.
# Visibility:
#   Private.
# Inputs:
#   - unused1: Trace callback argument (unused).
#   - w: Search window path.
#   - unused2: Trace callback argument (unused).
# Returns:
#   - None.
# Side effects:
#   - Sets `::search::filter_($w)` to `dbfilter`.
################################################################################
proc ::search::use_dbfilter_ { unused1 w {unused2 ""} } {
	set ::search::filter_($w) dbfilter
}

################################################################################
# ::search::progressbar_
#   Shows or hides the search progress bar and stop button.
# Visibility:
#   Private.
# Inputs:
#   - w: Search window path.
#   - show_hide: Either "show" or "hide".
# Returns:
#   - None.
# Side effects:
#   - Reconfigures the window layout (grid remove/add).
#   - Updates the progress bar via `progressBarSet`.
#   - Acquires/releases a grab on the stop button.
################################################################################
proc ::search::progressbar_ {w show_hide} {
	if {$show_hide eq "show"} {
		grid remove $w.buttons.save
		grid remove $w.buttons.reset_values
		grid remove $w.buttons.search_new
		grid remove $w.buttons.search
		grid $w.progressbar
		progressBarSet $w.progressbar 301 21
		grid $w.buttons.stop
		grab $w.buttons.stop
	} else {
		grab release $w.buttons.stop
		grid remove $w.buttons.stop
		grid remove $w.progressbar
		grid $w.buttons.save
		grid $w.buttons.reset_values
		grid $w.buttons.search_new
		grid $w.buttons.search
	}
}

################################################################################
# ::search::start_
#   Executes a search and updates the active filter for the search window.
# Visibility:
#   Private.
# Inputs:
#   - new_filter: When true, creates a new filter and opens a new game list.
#   - w: Search window path.
#   - options_cmd: Command that returns a list where the first element is the
#     search options list, and the optional second element is the
#     ignore-colours-hack options list.
# Returns:
#   - None.
# Side effects:
#   - Creates/composes filters via `sc_filter new` / `sc_filter compose`.
#   - Optionally copies the prior filter contents, depending on filter operation.
#   - Calls `::search::do_search_` (and optionally a second pass) with UI progress.
#   - Updates `::search::filter_($w)` and notifies listeners via `::notify::filter`.
#   - May open a new game list window when `new_filter` is true.
################################################################################
proc ::search::start_ {new_filter w options_cmd} {
	set dbase $::search::dbase_($w)
	set src_filter $::search::filter_($w)
	set src_op $::search::filterOp_($w)

	if {$new_filter} {
		set dest_filter [sc_filter new $dbase]
	} else {
		set dest_filter [sc_filter compose $dbase $src_filter ""]
	}
	if {$dest_filter ne $src_filter && $src_op ne "reset"} {
		sc_filter copy $dbase $dest_filter $src_filter
	}

	lassign [$options_cmd] options ignore_color_hack
	if {$ignore_color_hack ne ""} {
		set filter_hack [sc_filter new $dbase]
		sc_filter copy $dbase $filter_hack $dest_filter
	}

	set err [catch {::search::do_search_ $dbase $dest_filter $src_op $options "::search::progressbar_ $w show"}]
	::search::progressbar_ $w hide
	if {$err} {
		if {$::errorCode != $::ERROR::UserCancel} { ERROR::MessageBox }
	}

	if {!$err && $ignore_color_hack ne ""} {
		set err [catch {::search::do_search_ $dbase $filter_hack $src_op $ignore_color_hack "::search::progressbar_ $w show"}]
		::search::progressbar_ $w hide
		if {$err} {
			if {$::errorCode != $::ERROR::UserCancel} { ERROR::MessageBox }
		} else {
			sc_filter or $dbase $dest_filter $filter_hack
		}
	}
	if {$ignore_color_hack ne ""} {
		sc_filter release $dbase $filter_hack
	}

	set ::search::filter_($w) $dest_filter
	::notify::filter $dbase $dest_filter

	if {$new_filter} {
		::windows::gamelist::Open $dbase $dest_filter
	}
}

################################################################################
# ::search::do_search_
#   Applies a search to a filter, respecting the selected filter operation.
# Visibility:
#   Private.
# Inputs:
#   - dbase: Database handle/slot identifier.
#   - filter: Filter identifier to populate.
#   - filter_op: One of "reset", "and", or "or".
#   - options: List of options to pass to `sc_filter search`.
#   - reset_progressbar: Command to execute to reset/refresh the UI progress.
# Returns:
#   - None.
# Side effects:
#   - Mutates `filter` contents via `sc_filter reset/search/or/negate`.
#   - If `filter_op` is "or", creates and releases a temporary filter.
#   - Executes additional tag searches for any `-tag_pair` options.
################################################################################
proc ::search::do_search_ {dbase filter filter_op options reset_progressbar} {
	switch $filter_op {
		reset {
			sc_filter reset $dbase $filter full
		}
		and {
		}
		or {
			set or_filter [sc_filter new $dbase]
			sc_filter copy $dbase $or_filter $filter
			sc_filter negate $dbase $filter
		}
	}

	set tag_pairs [lsearch -all -inline -index 0 -exact $options "-tag_pair"]
	if {[llength tag_pairs] > 0} {
		set options [lsearch -all -inline -index 0 -not -exact $options "-tag_pair"]
	}

	{*}$reset_progressbar
	sc_filter search $dbase $filter {*}$options -filter AND
	foreach {elem} $tag_pairs {
		{*}$reset_progressbar
		lassign $elem -> tagName tagValue
		sc_filter search $dbase $filter tags $tagName $tagValue
	}

	if {[info exists or_filter]} {
		sc_filter or $dbase $filter $or_filter
		sc_filter release $dbase $or_filter
	}
}


################################################################################
# ::search::board
#   Opens the search window for the current board position.
# Visibility:
#   Public.
# Inputs:
#   - ref_base: Initial database selection value passed to `CreateSelectDBWidget`.
#   - ref_filter: Initial filter name for the search window.
# Returns:
#   - None.
# Side effects:
#   - Creates (and may destroy/recreate) the Board Search window.
################################################################################
proc ::search::board {{ref_base ""} {ref_filter "dbfilter"}} {
	::search::Open $ref_base $ref_filter BoardSearch ::search::boardCreateFrame
}

################################################################################
# ::search::boardCreateFrame
#   Creates the board-search specific widgets and returns the options command.
# Visibility:
#   Private.
# Inputs:
#   - w: Parent widget path for the board-search options area.
# Returns:
#   - options_cmd: A command name that implements the shared search framework
#     contract documented in `::search::Open`. For board search, the no-argument
#     form returns a 1-item list containing the board-search options list (i.e.
#     it omits the optional ignore-colours-hack second element).
# Side effects:
#   - Creates child ttk widgets under `w`.
#   - Initialises board-search option variables on first use.
################################################################################
proc ::search::boardCreateFrame {w} {
	if {![info exists ::search::boardOptType_]} {
		::search::boardOptions reset
	}

	ttk::labelframe $w.pos -text [::tr SearchType]
	grid $w.pos -sticky news -pady 6
	grid columnconfigure $w 0 -weight 1

	ttk::radiobutton $w.pos.exact -textvar ::tr(SearchBoardExact)  -variable ::search::boardOptType_ -value Exact
	ttk::radiobutton $w.pos.pawns -textvar ::tr(SearchBoardPawns)  -variable ::search::boardOptType_ -value Pawns
	ttk::radiobutton $w.pos.files -textvar ::tr(SearchBoardFiles)  -variable ::search::boardOptType_ -value Fyles
	ttk::radiobutton $w.pos.material -textvar ::tr(SearchBoardAny) -variable ::search::boardOptType_ -value Material
	grid $w.pos.exact -sticky w
	grid $w.pos.pawns -sticky w
	grid $w.pos.files -sticky w
	grid $w.pos.material -sticky w

	ttk::checkbutton $w.vars -text [::tr LookInVars] -variable ::search::boardOptInVars_ -onvalue 1 -offvalue 0
	grid $w.vars -sticky w

	ttk::checkbutton $w.flip -text [::tr IgnoreColors] -variable ::search::boardOptIgnoreCol_ -onvalue 1 -offvalue 0
	grid $w.flip -sticky w

	return "::search::boardOptions"
}

################################################################################
# ::search::boardOptions
#   Implements the board-search preset/options command contract.
# Visibility:
#   Private.
# Inputs:
#   - cmd: Optional control argument. Recognised values:
#       - "reset": Restores default board-search option values.
#       - <widgetPath>: When it begins with '.', returns a menu configuration list.
# Returns:
#   - When cmd is a widget path: Returns `{-state disabled}`.
#   - When cmd is "reset": Returns None.
#   - Otherwise: Returns a 1-item list containing the board-search options list.
# Side effects:
#   - May initialise or update `::search::boardOptType_`, `::search::boardOptInVars_`,
#     and `::search::boardOptIgnoreCol_`.
################################################################################
proc ::search::boardOptions {{cmd ""}} {
	if {[string index $cmd 0] eq "."} {
		return [list -state disabled]
	}

	if {$cmd eq "reset"} {
		set ::search::boardOptType_ Exact
		set ::search::boardOptInVars_ 0
		set ::search::boardOptIgnoreCol_ 0
		return
	}

	set options {board}
	lappend options $::search::boardOptType_
	lappend options $::search::boardOptInVars_
	lappend options $::search::boardOptIgnoreCol_
	return [list $options]
}

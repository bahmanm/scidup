### Menus.tcl: part of Scid.
### Copyright (C) 2001-2003 Shane Hudson.
### Copyright (C) 2015 Fulvio Benini


############################################################
### Main window menus:

menu .menu

## Mac Application menu has to be before any call to configure.
if { $macOS } {
  # Application menu:
  .menu add cascade -label [tr ScidUp] -menu .menu.apple
  menu .menu.apple

  set m .menu.apple

  $m add command -label HelpAbout -command helpAbout

  $m add separator

  ################################################################################
  # ::tk::mac::OpenDocument
  #   Opens one or more documents requested by macOS (typically via Finder).
  # Visibility:
  #   Public.
  # Inputs:
  #   - `args`: One or more file paths to open.
  # Returns:
  #   - None.
  # Side effects:
  #   - Calls `::file::Open` for each requested file.
  #   - Uses `::mac_open_queue` to serialise re-entrant calls.
  ################################################################################
  proc ::tk::mac::OpenDocument { args } {
    # The opening of big databases displays a progress bar that must process
    # the events to allow user interruption.
    # If another ::tk::mac::OpenDocument event is generated in the meantime,
    # this procedure is re-entered before the previous execution has finished.
    if {[info exists ::mac_open_queue]} {
      lappend ::mac_open_queue {*}$args
      return
    }
    set ::mac_open_queue $args
    while {[llength $::mac_open_queue]} {
      set files $::mac_open_queue
      set ::mac_open_queue {}
      foreach f $files {
        ::file::Open $f
      }
    }
    unset ::mac_open_queue
  }

  # To Quit (cmd-q)
  ################################################################################
  # ::tk::mac::Quit
  #   Exits Scid when macOS requests application termination (Cmd+Q).
  # Visibility:
  #   Public.
  # Inputs:
  #   - `args`: Unused (provided by the Tk/macOS hook signature).
  # Returns:
  #   - None.
  # Side effects:
  #   - Calls `::file::Exit`.
  ################################################################################
  proc ::tk::mac::Quit { args } { ::file::Exit }

  ## To get Help
  bind all <Command-?> {helpWindow Contents}
  bind all <Help> {helpWindow Contents}
}


### File menu:
set m .menu.file
menu $m -postcommand [list updateMenuStates $m]
.menu add cascade -label File -menu $m
$m add command -label FileNew -command ::file::New
$m add command -label FileOpen -acc "Ctrl+O" -command ::file::Open
$m add command -label FileFinder -acc "Ctrl+/" -command ::file::finder::Open
menu $m.bookmarks
$m add cascade -label FileBookmarks -menu $m.bookmarks
$m add separator
# naming is weird because the menus are moved from Tools to File menus
$m add command -label ToolsOpenBaseAsTree -command ::file::openBaseAsTree
menu $m.recenttrees
$m add cascade -label ToolsOpenRecentBaseAsTree -menu $m.recenttrees
$m add separator
set menuEndIdx [$m index end]
set ::menuFileRecentIdx [expr {$menuEndIdx + 1}]
$m add command -label FileExit -accelerator "Ctrl+Q" -command ::file::Exit


### Database menu:
set m .menu.db
menu $m -postcommand [list updateMenuStates $m]
.menu add cascade -label Database -menu $m
$m add command -label FileClose -acc "Ctrl+W" -command ::file::Close
$m add checkbutton -label LoadatStartup -variable ::autoLoadBases_currdb -command {
  if {[::file::autoLoadBases.remove $::curr_db] == -1} {
    ::file::autoLoadBases.add $::curr_db
  }
}
$m add separator
menu $m.copygames
  $m.copygames add command -label FileNew -command {
    set srcBase $::curr_db
    set destBase [::file::New]
    if {$destBase ne ""} {
      foreach {tag value} [sc_base extra $srcBase] {
        if {$tag ne "type"} {
          # Try to copy the database info (may not be supported by a database type like PGN)
          catch { sc_base extra $destBase $tag $value }
        }
      }
      ::windows::gamelist::CopyGames {} $srcBase $destBase all false
    }
  }
  $m.copygames add separator
  set copygamesEndIdx [$m.copygames index end]
  set ::menuDBCopyGamesIdx [expr {$copygamesEndIdx + 1}]
$m add cascade -label CopyAllGames -menu $m.copygames
menu $m.exportfilter
  $m.exportfilter add command -label ToolsExpFilterPGN \
      -command {exportGames filter PGN}
  $m.exportfilter add command -label ToolsExpFilterHTML \
      -command {exportGames filter HTML}
  $m.exportfilter add command -label ToolsExpFilterHTMLJS \
      -command {::html::exportCurrentFilter}
  $m.exportfilter add command -label ToolsExpFilterLaTeX \
      -command {exportGames filter LaTeX}
$m add cascade -label ToolsExpFilter -menu $m.exportfilter
menu $m.importfile
$m add cascade -label ToolsImportFile -menu $m.importfile
$m add separator
menu $m.utils
  $m.utils add checkbutton -label FileMaintWin -accelerator "Ctrl+M" -variable maintWin -command ::maint::OpenClose
  $m.utils add command -label FileMaintCompact -command compactDB
  $m.utils add command -label FileMaintClass -command classifyAllGames
  $m.utils add checkbutton -label FileMaintNameEditor -command nameEditor -variable nameEditorWin -accelerator "Ctrl+Shift+N"
  $m.utils add command -label StripTags -command stripTags
  $m.utils add separator
  $m.utils add command -label FileMaintDelete -state disabled -command markTwins
  $m.utils add command -label FileMaintTwin -command updateTwinChecker
  $m.utils add separator
  $m.utils add command -label Cleaner -command cleanerWin
$m add cascade -label FileMaint -menu $m.utils
menu $m.spell
  $m.spell add command -label FileMaintNamePlayer -command {openSpellCheckWin Player}
  $m.spell add command -label FileMaintNameEvent -command {openSpellCheckWin Event}
  $m.spell add command -label FileMaintNameSite -command {openSpellCheckWin Site}
  $m.spell add command -label FileMaintNameRound -command {openSpellCheckWin Round}
  $m.spell add command -label AddEloRatings -command {allocateRatings}
$m add cascade -label FileMaintName -menu $m.spell
$m add separator
set menuEndIdx [$m index end]
set ::menuDbSwitchIdx [expr {$menuEndIdx + 1}]


### Edit menu:
set m .menu.edit
menu $m
.menu add cascade -label Edit -menu $m
$m add command -label EditUndo -accelerator "Ctrl+z" -command { undoFeature undo }
$m add command -label EditRedo -accelerator "Ctrl+y" -command { undoFeature redo }
$m add separator
$m add command -label EditSetup -accelerator "S" -command setupBoard
$m add command -label EditCopyBoard -accelerator "Ctrl+Shift+C" -command copyFEN
$m add command -label EditPasteBoard -accelerator "Ctrl+Shift+V" -command pasteFEN
$m add command -label PgnFileCopy -command ::pgn::PgnClipboardCopy
$m add command -label EditPastePGN -command importClipboardGame
$m add separator
menu $m.strip
  $m.strip add command -label EditStripComments -command {::game::Strip comments}
  $m.strip add command -label EditStripVars -command {::game::Strip variations}
  $m.strip add command -label EditStripBegin -command {::game::TruncateBegin}
  $m.strip add command -label EditStripEnd -command {::game::Truncate}
$m add cascade -label EditStrip -menu $m.strip
$m add separator
$m add command -label EditReset -command ::windows::gamelist::ClearClipbase
$m add command -label EditCopy -command ::gameAddToClipbase
$m add command -label EditPaste -command {
  sc_clipbase paste
  ::notify::GameChanged
}


### Game menu:
set m .menu.game
menu $m -postcommand [list updateMenuStates $m]
.menu add cascade -label Game -menu $m
$m add command -label GameNew -accelerator "Ctrl+N" -command ::game::Clear
$m add command -label GameReload -command ::game::Reload
$m add separator
$m add command -label GameReplace -command gameReplace -accelerator "Ctrl+S"
$m add command -label GameAdd -command gameAdd  -accelerator "Ctrl+Shift+S"
menu $m.exportcurrent
  $m.exportcurrent add command -label ToolsExpCurrentPGN \
      -command {exportGames current PGN}
  $m.exportcurrent add command -label ToolsExpCurrentHTML \
      -command {exportGames current HTML}
  $m.exportcurrent add command -label ToolsExpCurrentHTMLJS \
      -command {::html::exportCurrentGame}
  $m.exportcurrent add command -label ToolsExpCurrentLaTeX \
      -command {exportGames current LaTeX}
$m add cascade -label ToolsExpCurrent -menu $m.exportcurrent
$m add separator
$m add command -label GameFirst -accelerator "Ctrl+Shift+Up" -command {::game::LoadNextPrev first}
$m add command -label GamePrev -accelerator "Ctrl+Up" -command {::game::LoadNextPrev previous}
$m add command -label GameNext -accelerator "Ctrl+Down" -command {::game::LoadNextPrev next}
$m add command -label GameLast -accelerator "Ctrl+Shift+Down" -command {::game::LoadNextPrev last}
$m add command -label GameRandom -command ::game::LoadRandom -accelerator "Ctrl+?"
$m add separator
$m add command -label GameDeepest -accelerator "Ctrl+Shift+D" -command {
  sc_move ply [sc_eco game ply]
  updateBoard
}
$m add command -label GameGotoMove -accelerator "Ctrl+U" -command ::game::GotoMoveNumber
$m add command -label GameNovelty -accelerator "Ctrl+Shift+Y" -command findNovelty


### Search menu:
set m .menu.search
menu $m
.menu add cascade -label Search -menu $m
$m add command -label SearchCurrent -command ::search::board -accelerator "Ctrl+Shift+B"
$m add command -label SearchHeader -command ::search::header -accelerator "Ctrl+Shift+H"
$m add command -label SearchMaterial -command ::search::material -accelerator "Ctrl+Shift+M"
$m add separator
$m add checkbutton -label WindowsPList -variable plistWin -command ::plist::toggle -accelerator "Ctrl+Shift+P"
$m add checkbutton -label WindowsTmt -variable tourneyWin -command ::tourney::toggle -accelerator "Ctrl+Shift+T"
$m add separator
$m add command -label SearchUsing -accel "Ctrl+Shift+U" -command ::search::usefile


### Windows menu:
set m .menu.windows
menu $m
.menu add cascade -label Windows -menu $m
$m add checkbutton -label WindowsComment -var ::windows::commenteditor::isOpen -command [list ::makeCommentWin toggle] -accelerator "Ctrl+E"
$m add checkbutton -label WindowsPGN -variable pgnWin -command ::pgn::OpenClose  -accelerator "Ctrl+P"
$m add checkbutton -label OptionsWindowsShowGameInfo -variable showGameInfo -command ::toggleGameInfo
$m add separator
$m add command -label WindowsGList -command ::windows::gamelist::Open  -accelerator "Ctrl+L"
$m add checkbutton -label WindowsSwitcher -variable baseWin -accelerator "Ctrl+D" -command ::windows::switcher::Open
$m add command -label ToolsCross -accelerator "Ctrl+Shift+X" -command ::crosstab::Open
$m add checkbutton -label WindowsECO -accelerator "Ctrl+Y" -variable ::windows::eco::isOpen -command {::windows::eco::OpenClose}
$m add checkbutton -label WindowsStats -variable ::windows::stats::isOpen -accelerator "Ctrl+I" -command ::windows::stats::Open
$m add checkbutton -label WindowsTree -variable treeWin -command ::tree::make -accelerator "Ctrl+T"
$m add checkbutton -label WindowsBook -variable ::book::isOpen -command ::book::open -accelerator "F6"
$m add command -label WindowsGraph -command ::tools::graphs::score::Refresh


### Tools menu:
set m .menu.tools
menu $m -postcommand [list updateMenuStates $m]
.menu add cascade -label Tools -menu $m
$m  add command -label ToolsConfigureEngines -command ::enginelist::choose
$m  add command -label ToolsStartEngine1 \
    -command [list ::enginewin::start 1] -accelerator "F2"
$m  add command -label ToolsStartEngine2 \
    -command [list ::enginewin::start 2] -accelerator "F3"
$m  add command -label ToolsAnalysis -command [list makeAnalysisWin 1]
$m  add command -label ToolsTrainReviewGame -command ::reviewgame::start
$m add separator
$m add checkbutton -label ToolsFilterGraph \
    -accelerator "Ctrl+Shift+G" -variable filterGraph -command tools::graphs::filter::Open
$m add checkbutton -label ToolsAbsFilterGraph \
    -accelerator "Ctrl+Shift+J" -variable absfilterGraph -command tools::graphs::absfilter::Open
$m add command -label ToolsOpReport \
    -accelerator "Ctrl+Shift+O" -command ::optable::makeReportWin
$m add command -label ToolsTracker \
    -accelerator "Ctrl+Shift+K" -command ::ptrack::make
$m add command -label ToolsBookTuning -command ::book::tuning
menu $m.hardware
  $m.hardware add command -label ToolsConnectHardwareConfigure -command ::ExtHardware::config
  $m.hardware add command -label ToolsConnectHardwareInputEngineConnect -command ::inputengine::connectdisconnect
  $m.hardware add command -label ToolsConnectHardwareNovagCitrineConnect -command ::novag::connect
$m add cascade -label ToolsConnectHardware -menu $m.hardware
$m add separator
menu $m.pinfo
  $m.pinfo add command -label GraphOptionsWhite -command { ::pinfo::playerInfo [sc_game info white] }
  $m.pinfo add command -label GraphOptionsBlack -command { ::pinfo::playerInfo [sc_game info black] }
$m add cascade -label ToolsPInfo -menu $m.pinfo
$m add command -label ToolsPlayerReport -command ::preport::preportDlg
$m add command -label ToolsRating -command {::tools::graphs::rating::Refresh both}


### Options menu:
set m .menu.options
menu $m
.menu add cascade -label Options -menu $m
menu $m.language
  foreach l $::languages {
      $m.language add radiobutton -label $::langName($l) \
          -underline $::langUnderline($l) -variable language -value $l \
          -command [list apply {{} { setLanguage; ::notify::PosChanged pgnonly }}]
  }
$m add cascade -label OptionsLanguage -menu $m.language
menu $m.theme -tearoff 1
set themeEndIdx [$m.theme index end]
set ::menuThemeListIdx [expr {$themeEndIdx + 1}]
$m add cascade -label OptionsTheme -menu $m.theme
menu $m.savelayout
menu $m.restorelayout
	    foreach i {"1 (default)" "2" "3"} slot {1 2 3} {
	      $m.savelayout add command -label $i -command [list ::docking::layout_save $slot]
	      $m.restorelayout add command -label $i -command [list ::docking::layout_restore $slot]
	    }
$m add command -label ConfigureScid -command { ::preferences::Open toggle }
$m add command -label OptionsResources -command ::preferences::resources
menu $m.export
  $m.export add command -label "PGN file text" -underline 0 -command [list setExportText PGN]
  $m.export add command -label "HTML file text" -underline 0 -command [list setExportText HTML]
  $m.export add command -label "LaTeX file text" -underline 0 -command [list setExportText LaTeX]
$m add cascade -label OptionsExport -menu $m.export
$m add separator
$m add checkbutton -label FullScreen -variable optionFullScreen \
  -command {
    set fullscreen [wm attributes . -fullscreen]
    wm attributes . -fullscreen [expr {!$fullscreen}]
  }
$m add checkbutton -label OptionsWindowsDock -variable windowsDock
$m add cascade -label OptionsWindowsSaveLayout -menu $m.savelayout
$m add cascade -label OptionsWindowsRestoreLayout -menu $m.restorelayout
$m add separator
$m add command -label OptionsSave -command options.write
$m add checkbutton -label OptionsAutoSave -variable optionsAutoSave \
    -command { if {!$::optionsAutoSave} { options.autoSaveHack } }


### Help menu:
set m .menu.helpmenu
menu $m
.menu add cascade -label Help -menu $m
set acc [expr {$macOS ? "Command-?" : "F1"}]
$m add command -label HelpContents -command {helpWindow Contents} -accelerator "$acc"
$m add command -label HelpIndex -command {helpWindow Index}
$m add command -label HelpGuide -command {helpWindow Guide}
$m add command -label HelpHints -command {helpWindow Hints}
$m add command -label HelpContact -command {helpWindow Author}
$m add separator
$m add command -label HelpTip -command ::tip::show
$m add separator
$m  add command -label HelpAbout -command helpAbout


##################################################
# Store menu labels for translations and help messages
set ::menuHelpMessage {}
################################################################################
# ::translateMenuLabels
#   Records menu labels for translation/help lookup and applies translations.
# Visibility:
#   Private.
# Inputs:
#   - `m`: Menu widget path whose entries should be processed.
# Returns:
#   - None.
# Side effects:
#   - Sets `::MenuLabels($m,$idx)` for non-separator, non-tearoff entries.
#   - Configures menu entry `-label` and `-underline` values.
#   - Installs a `<<MenuSelect>>` binding to update `::menuHelpMessage` and call
#     `updateStatusBar`.
#   - Recursively processes cascade submenus (except `.menu.options.language`).
################################################################################
proc translateMenuLabels {m} {
    bind $m <<MenuSelect>> {
        menuSelectionChanged_ %W [%W index active]
    }

    set n [$m index end]
    for {set i 0} {$n != "none" && $i <= $n} {incr i} {
        set type [$m type $i]
        if {$type != "separator" && $type != "tearoff"} {
            set lbl [$m entrycget $i -label]
            set ::MenuLabels($m,$i) $lbl
            set under 0
            catch { set under $::menuUnder($::language,$lbl) }
            $m entryconfig $i -label [tr $lbl] -underline $under
        }
        if {$type == "cascade"} {
            set submenu [$m entrycget $i -menu]
            if {$submenu ne ".menu.options.language"} {
                translateMenuLabels $submenu
            }
        }
    }
}

proc menuSelectionChanged_ {w idx} {
    if {$idx ne "" && $idx ne "none"} {
        # Tcl/Tk seems to generate strange window names for menus that
        # are configured to be a toplevel window main menu, e.g.
        # .menu.file get reported as ".#menu.#menu#file" and
        # .menu.file.utils is ".#menu.#menu#file.#menu#file#utils".
        # Convert paths with hashes to their true value before lookup.
        regsub -all "\#" [winfo name $w] . win
        set ::menuHelpMessage {}
        if {
            [info exists ::MenuLabels($win,$idx)] &&
            [info exists ::helpMessage($::language,$::MenuLabels($win,$idx))]
        } {
            set ::menuHelpMessage $::helpMessage($::language,$::MenuLabels($win,$idx))
        }
        updateStatusBar
    } elseif {$::menuHelpMessage ne ""} {
        set ::menuHelpMessage {}
        updateStatusBar
    }
}
################################################################################
# ::menuConfig
#   Issues a menu subcommand to an entry identified by its original label.
# Visibility:
#   Private.
# Inputs:
#   - `m`: Menu widget path.
#   - `label`: Original (untranslated) label to locate via `::MenuLabels`.
#   - `cmd`: Menu subcommand to invoke (e.g. `entryconfig`).
#   - `args`: Arguments to pass to the menu subcommand.
# Returns:
#   - None.
# Side effects:
#   - Invokes `$m $cmd $idx ...` for the first matching entry.
################################################################################
proc menuConfig {{m} {label} {cmd} args} {
    foreach {key lbl} [array get ::MenuLabels "$m*"] {
        if {$lbl eq $label} {
            set idx [lindex [split $key ","] 1]
            $m $cmd $idx {*}$args
            break
        }
    }
}
translateMenuLabels .menu
set fileExitHack [.menu.file index end]
set ::MenuLabels(.menu.file,end) $::MenuLabels(.menu.file,$fileExitHack)
array unset ::MenuLabels ".menu.file,$fileExitHack"


##################################################
# updateMenuStates:
#   Update all the menus, rechecking which state each item should be in.
#
################################################################################
# ::updateMenuStates
#   Updates dynamic menu entry state and contents for the requested top-level menu.
# Visibility:
#   Public.
# Inputs:
#   - `menuname` (optional): Menu widget path whose state should be updated.
# Returns:
#   - None.
# Side effects:
#   - For `.menu.file`: refreshes bookmarks and recent file menus.
#   - For `.menu.db`: updates `::autoLoadBases_currdb`.
#   - For `.menu.tools`: enables/disables training menu items based on
#     `::interactionHandler`.
#   - For `.menu.game`: enables/disables game navigation actions and toolbar
#     buttons based on filter/game state.
#   - For `.menu.options`: refreshes `::optionsFullScreen` from `wm attributes`.
################################################################################
proc updateMenuStates {{menuname}} {
  set m .menu
  switch -- $menuname {
  {.menu.file} {
      ::bookmarks::Refresh

      # update recent Tree list (open base as Tree)
      set ntreerecent [::recentFiles::treeshow .menu.file.recenttrees]

      # Remove and reinsert the Recent files list and Exit command:
      set idx2 [expr {[$m.file index end] -1}]
      $m.file delete $::menuFileRecentIdx $idx2
      set nrecent [::recentFiles::show $m.file $::menuFileRecentIdx]
      if {$nrecent > 0} {
        $m.file insert [expr {$::menuFileRecentIdx + $nrecent}] separator
      }
    }
  {.menu.db} {
      set ::autoLoadBases_currdb [expr {[::file::autoLoadBases.find $::curr_db] >= 0}]
    }
  {.menu.tools} {
      set st normal
      if {[info exists ::interactionHandler]} { set st disabled }
      menuConfig $m.tools ToolsTrainReviewGame entryconfig -state $st
    }
  {.menu.game} {
    set isReadOnly [sc_base isReadOnly $::curr_db]
    # Load first/last/random/game number buttons:
    set filtercount [sc_filter count]
    if {$filtercount == 0} {set state disabled} else {set state normal}
    $m.game entryconfig [tr GameFirst] -state $state
    $m.game entryconfig [tr GameLast] -state $state
    $m.game entryconfig [tr GameRandom] -state $state

    # Load previous button:
    if {[sc_filter previous]} {set state normal} else {set state disabled}
    $m.game entryconfig [tr GamePrev] -state $state
    .main.tb.gprev configure -state $state

    # Reload button:
    if {[sc_game number]} {set state normal} else {set state disabled}
    $m.game entryconfig [tr GameReload] -state $state

    # Load next button:
    if {[sc_filter next]} {set state normal} else {set state disabled}
    $m.game entryconfig [tr GameNext] -state $state
    .main.tb.gnext configure -state $state

    # Save add button:
    set state normal
    $m.game entryconfig [tr GameAdd] -state $state

    # Save replace button:
    set state normal
    if {[sc_game number] == 0  ||  $isReadOnly } {
      set state disabled
    }
    $m.game entryconfig [tr GameReplace] -state $state
  }
  {.menu.options} {
    set ::optionsFullScreen [wm attributes . -fullscreen]
  }
  }
}

################################################################################
# ::menuUpdateBases
#   Rebuilds menus whose entries depend on the currently open databases.
# Visibility:
#   Public.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Rebuilds database switch/copy/import submenus from `sc_base list`.
#   - Updates `::currentSlot` and configures menu item states based on:
#     - current database (`::curr_db`), clipbase (`::clipbase_db`)
#     - read-only status, compactability, and whether the database is empty.
################################################################################
proc menuUpdateBases {} {
  set ::currentSlot $::curr_db
  .menu.db delete $::menuDbSwitchIdx end
  .menu.db.copygames delete $::menuDBCopyGamesIdx end
  .menu.db.importfile delete 0 end

  foreach i [sc_base list] {
    set fname [::file::BaseName $i]
    set readonly [sc_base isReadOnly $i]

    .menu.db add radiobutton -variable currentSlot -value $i \
        -label "Base $i: $fname" \
        -underline 5 -accelerator "Ctrl+$i"\
        -command [list ::file::SwitchToBase $i]

    if {$i != $::curr_db && ! $readonly} {
        .menu.db.copygames add command -label "Base $i: $fname" \
            -command [list ::windows::gamelist::CopyGames {} $::curr_db $i all]
    }

    if {! $readonly} {
        .menu.db.importfile add command -label "into $i: $fname" \
            -command [list importPgnFile $i]
    }
  }

  #Current database
  set notClipbase [expr {$::curr_db != $::clipbase_db ? "normal" : "disabled"}]
  set canChange   [expr {![sc_base isReadOnly $::curr_db] ? "normal" : "disabled"}]
  set canCompact  [expr {[baseIsCompactable] ? "normal" : "disabled"}]
  set notEmpty    [expr {[sc_base numGames $::curr_db] != 0 ? "normal" : "disabled"}]

  menuConfig .menu.db FileClose entryconfig -state $notClipbase
  menuConfig .menu.db LoadatStartup entryconfig -state $notClipbase
  menuConfig .menu.db CopyAllGames entryconfig -state $notEmpty
  menuConfig .menu.db ToolsExpFilter entryconfig -state $notEmpty
  menuConfig .menu.db FileMaintName entryconfig -state $canChange
  menuConfig .menu.db.utils Cleaner          entryconfig -state $canChange
  menuConfig .menu.db.utils StripTags        entryconfig -state $canChange
  menuConfig .menu.db.utils FileMaintDelete  entryconfig -state $canChange
  menuConfig .menu.db.utils FileMaintClass   entryconfig -state $canChange
  menuConfig .menu.db.utils FileMaintTwin    entryconfig -state $canChange
  menuConfig .menu.db.utils FileMaintCompact entryconfig -state $canCompact
  menuConfig .menu.db.utils FileMaintNameEditor entryconfig -state $canChange
}

################################################################################
# ::menuUpdateThemes
#   Rebuilds the theme selection menu from the available ttk themes.
# Visibility:
#   Public.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Replaces theme entries under `.menu.options.theme` starting at
#     `::menuThemeListIdx`.
#   - Uses/updates `::lookTheme` and applies the selected theme via
#     `ttk::style theme use`.
################################################################################
proc menuUpdateThemes {} {
  set m .menu.options.theme
  $m delete $::menuThemeListIdx end
  foreach i [lsort [ttk::style theme names]] {
      $m add radiobutton -label "$i" -value $i -variable ::lookTheme \
      -command {ttk::style theme use $::lookTheme}
  }
}

##############################
# Multiple-language menu support functions.

# configMenuText:
#    Reconfigures the main window menus. Called when the language is changed.
#
################################################################################
# ::configMenuText
#   Configures a specific menu entry label/underline for a given language.
# Visibility:
#   Private.
# Inputs:
#   - `menu`: Menu widget path.
#   - `entry`: Entry index (integer).
#   - `tag`: Menu label tag key used in `::menuLabel`/`::menuUnder`.
#   - `lang`: Language key (e.g. `E`).
# Returns:
#   - None.
# Side effects:
#   - Configures `$menu entryconfig $entry -label ... -underline ...`.
################################################################################
proc configMenuText {menu entry tag lang} {
  global menuLabel menuUnder
  if {[info exists menuLabel($lang,$tag)] && [info exists menuUnder($lang,$tag)]} {
    $menu entryconfig $entry -label $menuLabel($lang,$tag) -underline $menuUnder($lang,$tag)
  } else {
    $menu entryconfig $entry -label $menuLabel(E,$tag) -underline $menuUnder(E,$tag)
  }
}

################################################################################
# ::setLanguageMenus
#   Applies the current language to all registered menus and refreshes dependants.
# Visibility:
#   Public.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Updates menu labels/underlines for all entries recorded in `::MenuLabels`.
#   - Calls: `::optable::ConfigMenus`, `::preport::ConfigMenus`,
#     `::tools::graphs::score::ConfigMenus`, and `::tools::graphs::rating::ConfigMenus`.
#   - Optionally reports duplicate underline characters when `::verifyMenus` is set.
################################################################################
proc setLanguageMenus {} {
  set lang $::language
  foreach {key lbl} [array get ::MenuLabels] {
      lassign [split $key ","] m idx
      if {![winfo exists $m]} { continue }
      set under 0
      catch { set under $::menuUnder($lang,$lbl) }
      $m entryconfig $idx -label [tr $lbl] -underline $under
  }

  ::optable::ConfigMenus
  ::preport::ConfigMenus
  ::tools::graphs::score::ConfigMenus
  ::tools::graphs::rating::ConfigMenus

  # Check for duplicate menu underline characters in this language:
  # set ::verifyMenus 1
  if {[info exists ::verifyMenus] && $::verifyMenus} {
    foreach m {file edit game search windows tools options help} {
      set list [checkMenuUnderline .menu.$m]
      if {[llength $list] > 0} {
        puts stderr "Menu $m has duplicate underline letters: $list"
      }
    }
  }
}

################################################################################
# ::checkMenuUnderline
#   Returns a list of duplicate underline characters within a menu.
# Visibility:
#   Private.
# Inputs:
#   - `menu`: Menu widget path.
# Returns:
#   - A list of lowercase characters that appear more than once as underlines.
# Side effects:
#   - None.
################################################################################
proc checkMenuUnderline {menu} {
  array set found {}
  set duplicates {}
  set last [$menu index last]
  for {set i [$menu cget -tearoff]} {$i <= $last} {incr i} {
    if {[string equal [$menu type $i] "separator"]} {
      continue
    }
    set char [string index [$menu entrycget $i -label] \
        [$menu entrycget $i -underline]]
    set char [string tolower $char]
    if {$char == ""} {
      continue
    }
    if {[info exists found($char)]} {
      lappend duplicates $char
    }
    set found($char) 1
  }
  return $duplicates
}

################################################################################
# ::configInformant
#   Builds the informant threshold configuration UI.
# Visibility:
#   Public.
# Inputs:
#   - `w`: Parent widget path to populate.
# Returns:
#   - None.
# Side effects:
#   - Creates ttk widgets under `$w.spinF` and binds spinboxes to `informant(*)`.
################################################################################
proc configInformant { w } {
  global informant

  ttk::frame $w.spinF
  set idx 0
  set row 0

  foreach i [lsort [array names informant]] {
    if {$i == "\"++-\""} { continue } ; # ignore old version: ++- from options.dat
    ttk::label $w.spinF.labelExpl$idx -text [ ::tr "Informant[ string trim $i "\""]" ]
    ttk::label $w.spinF.label$idx -text $i
     # Allow the configuration of "won game" up to "Mate found"
     if {$i == "\"+--\""} {
         ttk::spinbox $w.spinF.sp$idx -textvariable informant($i) -width 5 -from 0.0 -to 328.0 -increment 1.0 -validate all -validatecommand { regexp {^[0-9]\.[0-9]$} %P }
     } else {
         ttk::spinbox $w.spinF.sp$idx -textvariable informant($i) -width 5 -from 0.0 -to 9.9 -increment 0.1 -validate all -validatecommand { regexp {^[0-9]\.[0-9]$} %P }
     }
    grid $w.spinF.labelExpl$idx -row $row -column 0 -sticky w
    incr row
    grid $w.spinF.label$idx -row $row -column 0 -sticky w
    grid $w.spinF.sp$idx -row $row -column 1 -sticky w -padx "0 5" -pady "0 5"
    incr row
    incr idx
  }
  pack $w.spinF
}

################################################################################
# ::getBooksDir
#   Prompts for an opening books directory and updates an entry widget.
# Visibility:
#   Private.
# Inputs:
#   - `widget`: Entry widget path whose contents should be updated.
# Returns:
#   - None.
# Side effects:
#   - Shows a `tk_chooseDirectory` dialog.
#   - On selection, calls `::setBooksDir` and updates the entry widget.
################################################################################
proc getBooksDir { widget } {
  global scidBooksDir
  set dir [tk_chooseDirectory -initialdir $scidBooksDir -parent [winfo toplevel $widget] -mustexist 1]
  if {$dir != ""} {
      setBooksDir $dir
      $widget delete 0 end
      $widget insert end $dir
  }
}

################################################################################
# ::setBooksDir
# Visibility:
#   Private.
# Inputs:
#   - `dir`: Directory path to use as the opening books directory.
# Returns:
#   - None.
# Side effects:
#   - Updates the global `scidBooksDir`.
################################################################################
proc setBooksDir { dir } {
  global scidBooksDir
  set scidBooksDir $dir
}

################################################################################
# ::getTacticsBasesDir
#   Prompts for the tactics bases directory and updates an entry widget.
# Visibility:
#   Private.
# Inputs:
#   - `widget`: Entry widget path whose contents should be updated.
# Returns:
#   - None.
# Side effects:
#   - Shows a `tk_chooseDirectory` dialog.
#   - On selection, calls `::setTacticsBasesDir` and updates the entry widget.
################################################################################
proc getTacticsBasesDir { widget } {
  global scidBasesDir
  set dir [tk_chooseDirectory -initialdir $scidBasesDir -parent [winfo toplevel $widget] -mustexist 1]
  if {$dir != ""} {
      setTacticsBasesDir $dir
      $widget delete 0 end
      $widget insert end $dir
  }
}

################################################################################
# ::setTacticsBasesDir
# Visibility:
#   Private.
# Inputs:
#   - `dir`: Directory path to use as the tactics bases directory.
# Returns:
#   - None.
# Side effects:
#   - Updates the global `scidBasesDir`.
################################################################################
proc setTacticsBasesDir { dir } {
  global scidBasesDir
  set scidBasesDir $dir
}

################################################################################
# ::getPhotoDir
#   Prompts for the player photo directory and updates an entry widget.
# Visibility:
#   Private.
# Inputs:
#   - `widget`: Entry widget path whose contents should be updated.
# Returns:
#   - None.
# Side effects:
#   - Shows a `tk_chooseDirectory` dialog.
#   - On selection, calls `::setPhotoDir` and, when it reports success, updates
#     the entry widget.
################################################################################
proc getPhotoDir { widget } {
  set idir [pwd]
  if { [info exists ::scidPhotoDir] } { set idir $::scidPhotoDir }
  set dir [tk_chooseDirectory -initialdir $idir -parent [winfo toplevel $widget] -mustexist 1]
  if {$dir != ""} {
      if { [setPhotoDir $dir] } {
          $widget delete 0 end
          $widget insert end $dir
      }
  }
}

################################################################################
# ::setPhotoDir
#   Updates the player photo directory and reloads player photos.
# Visibility:
#   Public.
# Inputs:
#   - `dir`: Directory path containing player images.
# Returns:
#   - The number of images found (as reported by `loadPlayersPhoto`).
# Side effects:
#   - Updates `::scidPhotoDir` and registers it via `::options.store`.
#   - Calls `loadPlayersPhoto`.
#   - Shows a `tk_messageBox` with a summary of loaded images.
#   - Calls `::notify::GameChanged`.
################################################################################
proc setPhotoDir { dir } {
    set ::scidPhotoDir $dir
    ::options.store ::scidPhotoDir
    set n [loadPlayersPhoto]
    set ret [lindex $n 0]
    tk_messageBox -parent .resDialog -message "Found $ret images in [lindex $n 1] file(s)"
    ::notify::GameChanged
    return $ret
}

################################################################################
# ::getThemePkgFile
#   Prompts for a theme package index file and updates an entry widget.
# Visibility:
#   Private.
# Inputs:
#   - `widget`: Entry widget path whose contents should be updated.
# Returns:
#   - None.
# Side effects:
#   - Shows a `tk_getOpenFile` dialog.
#   - Calls `::readThemePkgFile` to load the selected file.
#   - On (reported) success, updates the entry widget contents.
################################################################################
proc getThemePkgFile { widget} {
  global initialDir
  set fullname [tk_getOpenFile -parent [winfo toplevel $widget] -title "Select a pkgIndex.tcl file for themes" -initialdir [file dirname $::ThemePackageFile] -initialfile $::ThemePackageFile \
	       -filetypes { {Theme "pkgIndex.tcl"} }]
  if { $fullname != "" && $fullname != $::ThemePackageFile && ! [readThemePkgFile $fullname] } {
      $widget delete 0 end
      $widget insert end $fullname
  }
}

################################################################################
# ::readThemePkgFile
#   Loads a theme package index file and refreshes the theme menu.
# Visibility:
#   Public.
# Inputs:
#   - `fullname`: Path to a theme `pkgIndex.tcl` file (may be empty).
# Returns:
#   - `0` on success or no-op, otherwise `1` on error.
# Side effects:
#   - May call `::safeSourceStyle` to load ttk themes.
#   - May call `::menuUpdateThemes` and update `::ThemePackageFile`.
#   - Shows a `tk_messageBox` describing how many new themes were found.
################################################################################
proc readThemePkgFile { fullname } {
    if {$fullname ne "" && $fullname != $::ThemePackageFile } {
        set count [llength [ttk::style theme names]]
        set ret [ catch { ::safeSourceStyle $fullname } ]
        set newCount [llength [ttk::style theme names]]
        set newthemes [expr {$newCount - $count}]
        if { $ret == 0 && $newthemes > 0  } {
            menuUpdateThemes
            set ::ThemePackageFile $fullname
        }
        tk_messageBox -parent .resDialog -message "$newthemes new theme(s) found."
    } else {
        set ::ThemePackageFile $fullname
        tk_messageBox -parent .resDialog -message "No new themes loaded."
        set ret 0
    }
    return $ret
}

################################################################################
# ::getECOFile
#   Prompts for an ECO file and updates an entry widget.
# Visibility:
#   Private.
# Inputs:
#   - `widget`: Entry widget path whose contents should be updated.
# Returns:
#   - None.
# Side effects:
#   - Shows a `tk_getOpenFile` dialog.
#   - Calls `::readECOFile` and, on success, updates the entry widget.
################################################################################
proc getECOFile { widget } {
  global ecoFile
  set ftype [list [list "[tr ScidUp] ECO files" [list ".eco"]]]
  set fullname [tk_getOpenFile -parent [winfo toplevel $widget] -initialdir [file dirname $ecoFile] -filetypes $ftype -title "Load ECO file"]
  if { [readECOFile $fullname] } {
      $widget delete 0 end
      $widget insert end $fullname
  }
}

################################################################################
# ::readECOFile
#   Loads an ECO file into Scid.
# Visibility:
#   Public.
# Inputs:
#   - `fullname`: Path to an ECO file (may be empty).
# Returns:
#   - `1` on successful load, otherwise `0`.
# Side effects:
#   - Calls `sc_eco read` when a filename is provided.
#   - Updates `ecoFile`.
#   - Shows `tk_messageBox` status/warning messages.
################################################################################
proc readECOFile { fullname } {
  global ecoFile
  if {[string compare $fullname ""]} {
    if {[catch {sc_eco read $fullname} result]} {
      tk_messageBox -title [tr ScidUp] -type ok -icon warning -message $result -parent .resDialog
    } else {
      set ecoFile $fullname
      tk_messageBox -title "[tr ScidUp]: ECO file loaded." -type ok -icon info -parent .resDialog \
          -message "ECO file $fullname loaded: $result positions.\n\nTo have this file automatically loaded when you start [tr ScidUp], select \"Save Options\" from the Options menu before exiting."
      return 1
    }
  } else {
      set ecoFile $fullname
  }
  return 0
}

################################################################################
# ::updateLocale
#   Applies the selected numeric locale formatting and refreshes dependent UI.
# Visibility:
#   Public.
# Inputs:
#   - None (uses `locale(numeric)`).
# Returns:
#   - None.
# Side effects:
#   - Calls `sc_info decimal`.
#   - Calls `::windows::gamelist::Refresh`.
#   - Calls `updateTitle`.
################################################################################
proc updateLocale {} {
  global locale
  sc_info decimal $locale(numeric)
  ::windows::gamelist::Refresh
  updateTitle
}

################################################################################
# ::chooseHighlightColor
#   Prompts for the “highlight last move” colour and refreshes the board.
# Visibility:
#   Public.
# Inputs:
#   - None.
# Returns:
#   - None.
# Side effects:
#   - Shows a `tk_chooseColor` dialog.
#   - Updates `::highlightLastMoveColor` when a colour is selected.
#   - Calls `updateBoard` when a colour is selected.
################################################################################
proc chooseHighlightColor {} {
  set col [ tk_chooseColor -initialcolor $::highlightLastMoveColor -title [tr ScidUp]]
  if { $col != "" } {
    set ::highlightLastMoveColor $col
    updateBoard
  }
}


### End of file: menus.tcl

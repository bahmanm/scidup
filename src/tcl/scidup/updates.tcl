################################################################################
# ScidUp updates support.
#   Provides facilities for checking whether a newer ScidUp release is available.
#
# Design:
#   - Network access is delegated to platform scripts using the host trust store.
#   - This module is responsible for:
#       * locating the platform script
#       * executing it
#       * strict parsing/validation of its stdout contract
#       * retry scheduling for transient failures
################################################################################

namespace eval ::scidup::updates {
  variable _nextRequestId 0
  array set _requests {}
}

################################################################################
# ::scidup::updates::defaultScriptPath
#   Returns the expected installed update-check script path for this platform.
# Visibility:
#   Public.
# Inputs:
#   - None.
# Returns:
#   - (string): Absolute path to the script expected to be installed, or empty
#     when it cannot be determined.
################################################################################
proc ::scidup::updates::defaultScriptPath {} {
  if {[info exists ::scidShareDir] && $::scidShareDir ne ""} {
    set shareDir $::scidShareDir
  } else {
    set exeDir [file dirname [info nameofexecutable]]
    set candidate [file normalize [file join $exeDir "../share/scidup"]]
    if {[file isdirectory $candidate]} {
      set shareDir $candidate
    } else {
      return ""
    }
  }

  if {$::tcl_platform(platform) eq "windows"} {
    return [file join $shareDir "tools" "check-newer-release.ps1"]
  }
  return [file join $shareDir "tools" "check-newer-release"]
}

################################################################################
# ::scidup::updates::checkNewerRelease
#   Checks whether a newer ScidUp release is available.
#
# Contract:
#   Invokes the platform script and expects a single TSV line on stdout:
#       kind<TAB>version<TAB>url
#
# Behaviour:
#   - Exit code 0: parses stdout and reports the result.
#   - Exit code 2: reports prerequisites missing.
#   - Any other non-zero: retries up to -maxAttempts with -retryDelayMs delay.
#
# Visibility:
#   Public.
# Inputs (options):
#   -onResult (required): Command prefix invoked as:
#       {*}$onResult $resultDict
#   -localVersion: Defaults to `[sc_info release version]`.
#   -scriptPath: Overrides the script path (useful for tests).
#   -maxAttempts: Default 5.
#   -retryDelayMs: Default 60000.
# Returns:
#   - (int): Request identifier (can be cancelled via `cancel`).
################################################################################
proc ::scidup::updates::checkNewerRelease {args} {
  array set opt {
    -onResult ""
    -localVersion ""
    -scriptPath ""
    -maxAttempts 5
    -retryDelayMs 60000
  }

  if {[llength $args] % 2 != 0} {
    return -code error "Expected an even number of arguments (option/value pairs)."
  }
  foreach {k v} $args {
    if {![info exists opt($k)]} {
      return -code error "Unknown option: $k"
    }
    set opt($k) $v
  }

  if {$opt(-onResult) eq ""} {
    return -code error "Missing required option: -onResult"
  }

  if {$opt(-localVersion) eq ""} {
    set opt(-localVersion) [sc_info release version]
  }
  if {$opt(-scriptPath) eq ""} {
    set opt(-scriptPath) [::scidup::updates::defaultScriptPath]
  }

  variable _nextRequestId
  variable _requests
  set requestId [incr _nextRequestId]

  set _requests($requestId,onResult) $opt(-onResult)
  set _requests($requestId,localVersion) $opt(-localVersion)
  set _requests($requestId,scriptPath) $opt(-scriptPath)
  set _requests($requestId,maxAttempts) $opt(-maxAttempts)
  set _requests($requestId,retryDelayMs) $opt(-retryDelayMs)
  set _requests($requestId,attempt) 0
  set _requests($requestId,buffer) ""
  set _requests($requestId,channel) ""
  set _requests($requestId,afterId) ""

  ::scidup::updates::_scheduleAttempt $requestId 0
  return $requestId
}

################################################################################
# ::scidup::updates::cancel
#   Cancels an in-flight update check request.
# Visibility:
#   Public.
# Inputs:
#   - requestId (int): Identifier returned by `checkNewerRelease`.
# Returns:
#   - None.
################################################################################
proc ::scidup::updates::cancel {requestId} {
  variable _requests
  if {![info exists _requests($requestId,onResult)]} {
    return
  }

  if {$_requests($requestId,afterId) ne ""} {
    after cancel $_requests($requestId,afterId)
  }
  if {$_requests($requestId,channel) ne ""} {
    catch {fileevent $_requests($requestId,channel) readable {}}
    catch {close $_requests($requestId,channel)}
  }

  array unset _requests "$requestId,*"
}

proc ::scidup::updates::_scheduleAttempt {requestId delayMs} {
  variable _requests
  if {![info exists _requests($requestId,onResult)]} {
    return
  }
  set _requests($requestId,afterId) [after $delayMs [list ::scidup::updates::_attempt $requestId]]
}

proc ::scidup::updates::_attempt {requestId} {
  variable _requests
  if {![info exists _requests($requestId,onResult)]} {
    return
  }

  incr _requests($requestId,attempt)
  set _requests($requestId,buffer) ""

  set scriptPath $_requests($requestId,scriptPath)
  if {$scriptPath eq "" || ![file exists $scriptPath]} {
    ::scidup::updates::_finish $requestId [dict create \
      status prerequisites_missing \
      message "Update check script not found: $scriptPath" \
      attempts $_requests($requestId,attempt)]
    return
  }

  if {$::tcl_platform(platform) eq "windows"} {
    set psPrefix [::scidup::updates::_findPowerShellExecutable]
    if {$psPrefix eq ""} {
      ::scidup::updates::_finish $requestId [dict create \
        status prerequisites_missing \
        message "PowerShell is required to check for updates." \
        attempts $_requests($requestId,attempt)]
      return
    }
    set tokens [concat $psPrefix [list -NoProfile -ExecutionPolicy Bypass -File $scriptPath $_requests($requestId,localVersion)]]
  } else {
    set tokens [list $scriptPath $_requests($requestId,localVersion)]
  }

  set channel [::scidup::updates::_openPipe $tokens]
  set _requests($requestId,channel) $channel
  fconfigure $channel -blocking 0 -buffering none -translation binary
  fileevent $channel readable [list ::scidup::updates::_onReadable $requestId $channel]
}

proc ::scidup::updates::_findPowerShellExecutable {} {
  # Returns a command prefix list suitable for `exec`/pipeline use.
  # On Windows, `auto_execok` may already return a multi-element command
  # prefix, so callers must treat the result as a list rather than a single
  # executable path string.
  # Prefer PATH lookup, then fall back to common system locations.
  foreach candidate {pwsh pwsh.exe powershell powershell.exe} {
    set exe [auto_execok $candidate]
    if {$exe ne ""} {
      return $exe
    }
  }

  set candidates {}
  if {[info exists ::env(ProgramFiles)]} {
    lappend candidates [file join $::env(ProgramFiles) PowerShell 7 pwsh.exe]
  }
  if {[info exists ::env(ProgramW6432)]} {
    lappend candidates [file join $::env(ProgramW6432) PowerShell 7 pwsh.exe]
  }
  if {[info exists ::env(SystemRoot)]} {
    lappend candidates [file join $::env(SystemRoot) System32 WindowsPowerShell v1.0 powershell.exe]
  }

  foreach exe $candidates {
    if {[file exists $exe]} {
      return [list $exe]
    }
  }
  return ""
}

proc ::scidup::updates::_openPipe {tokens} {
  set cmd "|"
  foreach t $tokens {
    append cmd " " [list $t]
  }
  # Merge stderr into stdout so transient failures can be diagnosed.
  append cmd " 2>@1"
  return [open $cmd r]
}

proc ::scidup::updates::_onReadable {requestId channel} {
  variable _requests
  if {![info exists _requests($requestId,onResult)]} {
    catch {fileevent $channel readable {}}
    catch {close $channel}
    return
  }

  set chunk [read $channel]
  append _requests($requestId,buffer) $chunk

  if {![eof $channel]} {
    return
  }

  fileevent $channel readable {}
  set _requests($requestId,channel) ""

  set exitCode 0
  set closeMessage ""
  # Tcl may suppress pipeline exit status errors when the channel is in
  # non-blocking mode. Switch back to blocking before closing so we can read
  # a reliable exit status from `close`.
  catch {fconfigure $channel -blocking 1}
  if {[catch {close $channel} closeMessage closeOpts]} {
    set exitCode [::scidup::updates::_exitCodeFromCloseOptions $closeOpts]
  }

  ::scidup::updates::_handleCompletion $requestId $exitCode $closeMessage $_requests($requestId,buffer)
}

proc ::scidup::updates::_exitCodeFromCloseOptions {closeOpts} {
  if {![dict exists $closeOpts -errorcode]} {
    return 1
  }
  set errorCode [dict get $closeOpts -errorcode]
  if {[llength $errorCode] >= 3 && [lindex $errorCode 0] eq "CHILDSTATUS"} {
    return [lindex $errorCode 2]
  }
  return 1
}

proc ::scidup::updates::_handleCompletion {requestId exitCode closeMessage output} {
  variable _requests
  if {![info exists _requests($requestId,onResult)]} {
    return
  }

  if {$exitCode == 0} {
    set parsed [catch {::scidup::updates::_parseScriptOutput $output} parsedResult parsedOpts]
    if {$parsed == 0} {
      ::scidup::updates::_finish $requestId [dict merge $parsedResult [dict create status ok attempts $_requests($requestId,attempt)]]
      return
    }

    set closeMessage [dict get $parsedOpts -errorinfo]
    set exitCode 1
  }

  if {$exitCode == 2} {
    ::scidup::updates::_finish $requestId [dict create \
      status prerequisites_missing \
      message [string trim $output] \
      attempts $_requests($requestId,attempt)]
    return
  }

  set attempt $_requests($requestId,attempt)
  set maxAttempts $_requests($requestId,maxAttempts)
  if {$attempt < $maxAttempts} {
    ::scidup::updates::_scheduleAttempt $requestId $_requests($requestId,retryDelayMs)
    return
  }

  set msg [string trim $output]
  if {$msg eq ""} {
    set msg $closeMessage
  }
  ::scidup::updates::_finish $requestId [dict create \
    status transient_failure \
    message $msg \
    attempts $attempt]
}

proc ::scidup::updates::_parseScriptOutput {output} {
  set text [string trimright $output "\r\n"]
  set lines [split $text "\n"]
  set nonEmpty {}
  foreach l $lines {
    if {[string trim $l] ne ""} {
      lappend nonEmpty $l
    }
  }
  if {[llength $nonEmpty] != 1} {
    return -code error "Expected exactly one non-empty output line, got [llength $nonEmpty]."
  }

  set line [lindex $nonEmpty 0]
  set fields [split $line "\t"]
  if {[llength $fields] != 3} {
    return -code error "Expected exactly three TSV fields, got [llength $fields]."
  }

  lassign $fields kind version url

  if {$kind ni {none release prerelease}} {
    return -code error "Unsupported kind: $kind"
  }

  if {$kind eq "none"} {
    if {$version ne "-" || $url ne "-"} {
      return -code error "Expected '-' placeholders for kind=none."
    }
    return [dict create kind $kind version $version url $url]
  }

  if {$kind eq "release"} {
    if {![string is integer -strict $version] || $version <= 0} {
      return -code error "Invalid release version: $version"
    }
  } else {
    if {![regexp {^[0-9]+-testing-[0-9]{4}-[0-9]{2}-[0-9]{2}$} $version]} {
      return -code error "Invalid prerelease version: $version"
    }
  }

  if {![regexp {^https://github\.com/} $url]} {
    return -code error "Invalid URL: $url"
  }

  return [dict create kind $kind version $version url $url]
}

proc ::scidup::updates::_finish {requestId resultDict} {
  variable _requests
  if {![info exists _requests($requestId,onResult)]} {
    return
  }

  set cb $_requests($requestId,onResult)
  array unset _requests "$requestId,*"

  catch {uplevel #0 [list {*}$cb $resultDict]}
}

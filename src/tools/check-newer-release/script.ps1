# ScidUp check-newer-release script (Windows).
# Outputs a single TSV line on stdout:
#   kind<TAB>version<TAB>url
# Exit codes:
#   0 = success (stdout is authoritative)
#   2 = prerequisites missing (cannot perform the check on this machine)
#   any other non-zero = transient failure

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($args.Count -lt 1 -or [string]::IsNullOrWhiteSpace($args[0])) {
  Write-Error "Missing local release version argument."
  exit 2
}

$localVersion = $args[0].Trim()

$repo = if ($env:SCIDUP_GITHUB_REPO) { $env:SCIDUP_GITHUB_REPO } else { "bahmanm/scidup" }
$releasesAtomUrl = "https://github.com/$repo/releases.atom"

function ReleaseUrl([string]$tag) {
  return "https://github.com/$repo/releases/tag/$tag"
}

try {
  try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  } catch {
    # Continue and let the request fail if TLS configuration is unsupported.
  }

  $client = New-Object System.Net.WebClient
  $client.Headers["User-Agent"] = "ScidUp check-newer-release"
  $content = $client.DownloadString($releasesAtomUrl)
} catch {
  Write-Error "Failed to fetch release feed: $releasesAtomUrl`n$($_.Exception.Message)"
  exit 1
}

if ([string]::IsNullOrWhiteSpace($content)) {
  Write-Error "Release feed was empty: $releasesAtomUrl"
  exit 1
}

try {
  [xml]$xml = $content
} catch {
  Write-Error "Failed to parse Atom XML."
  exit 1
}

$tags = New-Object System.Collections.Generic.HashSet[string]
foreach ($entry in $xml.feed.entry) {
  $id = [string]$entry.id
  if ($id -match 'Repository/[^/]+/(v[^<]+)$') {
    [void]$tags.Add($Matches[1])
  }
}

if ($tags.Count -eq 0) {
  Write-Error "Failed to discover any tags in the Atom feed."
  exit 1
}

function HasTag([string]$tag) {
  return $tags.Contains($tag)
}

if ($localVersion -match '^v(\d+)$') {
  $n = [int]$Matches[1]
  $nextN = $n + 1
  $nextTag = "v$nextN"
  if (HasTag $nextTag) {
    Write-Output ("release`t{0}`t{1}" -f $nextN, (ReleaseUrl $nextTag))
  } else {
    Write-Output "none`t-`t-"
  }
  exit 0
}

if ($localVersion -match '^v(\d+)-testing-(\d{4}-\d{2}-\d{2})$') {
  $n = [int]$Matches[1]
  $localDate = $Matches[2]

  $stableTag = "v$n"
  if (HasTag $stableTag) {
    Write-Output ("release`t{0}`t{1}" -f $n, (ReleaseUrl $stableTag))
    exit 0
  }

  $nextN = $n + 1
  $nextStableTag = "v$nextN"
  if (HasTag $nextStableTag) {
    Write-Output ("release`t{0}`t{1}" -f $nextN, (ReleaseUrl $nextStableTag))
    exit 0
  }

  $prefix = "v$n-testing-"
  $newestDate = $null
  foreach ($tag in $tags) {
    if ($tag.StartsWith($prefix)) {
      $datePart = $tag.Substring($prefix.Length)
      if ($datePart -match '^\d{4}-\d{2}-\d{2}$') {
        if ($null -eq $newestDate -or [string]::CompareOrdinal($datePart, $newestDate) -gt 0) {
          $newestDate = $datePart
        }
      }
    }
  }

  if ($null -ne $newestDate -and [string]::CompareOrdinal($newestDate, $localDate) -gt 0) {
    $candidateTag = "v$n-testing-$newestDate"
    Write-Output ("prerelease`t{0}-testing-{1}`t{2}" -f $n, $newestDate, (ReleaseUrl $candidateTag))
    exit 0
  }

  $nextPrefix = "v$nextN-testing-"
  $newestNextDate = $null
  foreach ($tag in $tags) {
    if ($tag.StartsWith($nextPrefix)) {
      $datePart = $tag.Substring($nextPrefix.Length)
      if ($datePart -match '^\d{4}-\d{2}-\d{2}$') {
        if ($null -eq $newestNextDate -or [string]::CompareOrdinal($datePart, $newestNextDate) -gt 0) {
          $newestNextDate = $datePart
        }
      }
    }
  }

  if ($null -ne $newestNextDate) {
    $candidateTag = "v$nextN-testing-$newestNextDate"
    Write-Output ("prerelease`t{0}-testing-{1}`t{2}" -f $nextN, $newestNextDate, (ReleaseUrl $candidateTag))
  } else {
    Write-Output "none`t-`t-"
  }
  exit 0
}

Write-Error "Unsupported local release version format: $localVersion"
exit 2

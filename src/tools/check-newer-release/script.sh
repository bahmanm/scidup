#!/usr/bin/env bash
#
# ScidUp check-newer-release script (Unix).
#
# Outputs a single TSV line on stdout:
#   kind<TAB>version<TAB>url
#
# Exit codes:
#   0 = success (stdout is authoritative)
#   2 = prerequisites missing (e.g. curl not available)
#   any other non-zero = transient failure

set -euo pipefail

if [ "$#" -lt 1 ] || [ -z "${1:-}" ]; then
  echo "Missing local release version argument." >&2
  exit 2
fi

local_version="$1"

REPO="${SCIDUP_GITHUB_REPO:-bahmanm/scidup}"
releases_atom_url="https://github.com/${REPO}/releases.atom"

if ! command -v curl >/dev/null 2>&1; then
  echo "curl is required to check for updates." >&2
  exit 2
fi

atom_xml="$(curl --fail --silent --show-error --location --max-time 15 "$releases_atom_url")" || {
  echo "Failed to fetch release feed: $releases_atom_url" >&2
  exit 1
}

# Extract tags from <id> elements of the Atom feed:
#   <id>tag:github.com,2008:Repository/.../v1-testing-2026-01-24</id>
tags="$(printf '%s\n' "$atom_xml" | sed -n 's@.*<id>tag:github.com,2008:Repository/[^/]*/\(v[^<]*\)</id>.*@\1@p')" || true

if [ -z "$tags" ]; then
  echo "Failed to parse tags from release feed." >&2
  exit 1
fi

release_url() {
  tag="$1"
  printf 'https://github.com/%s/releases/tag/%s' "$REPO" "$tag"
}

contains_tag() {
  needle="$1"
  printf '%s\n' "$tags" | grep -F -x -- "$needle" >/dev/null 2>&1
}

if printf '%s' "$local_version" | grep -Eq '^v[0-9]+$'; then
  n="$(printf '%s' "$local_version" | sed -n 's/^v\([0-9][0-9]*\)$/\1/p')"
  next_n=$((n + 1))
  next_tag="v${next_n}"

  if contains_tag "$next_tag"; then
    printf 'release\t%s\t%s\n' "$next_n" "$(release_url "$next_tag")"
  else
    printf 'none\t-\t-\n'
  fi
  exit 0
fi

if printf '%s' "$local_version" | grep -Eq '^v[0-9]+-testing-[0-9]{4}-[0-9]{2}-[0-9]{2}$'; then
  n="$(printf '%s' "$local_version" | sed -n 's/^v\([0-9][0-9]*\)-testing-[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$/\1/p')"
  local_date="$(printf '%s' "$local_version" | sed -n 's/^v[0-9][0-9]*-testing-\([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]\)$/\1/p')"

  stable_tag="v${n}"
  if contains_tag "$stable_tag"; then
    printf 'release\t%s\t%s\n' "$n" "$(release_url "$stable_tag")"
    exit 0
  fi

  # If there is a newer stable release, prefer it over prereleases.
  next_n=$((n + 1))
  next_stable_tag="v${next_n}"
  if contains_tag "$next_stable_tag"; then
    printf 'release\t%s\t%s\n' "$next_n" "$(release_url "$next_stable_tag")"
    exit 0
  fi

  newest_date="$(
    printf '%s\n' "$tags" \
      | sed -n "s/^v${n}-testing-\\([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]\\)$/\\1/p" \
      | sort \
      | tail -n 1
  )" || true

  if [ -n "$newest_date" ] && [ "$newest_date" \> "$local_date" ]; then
    candidate_tag="v${n}-testing-${newest_date}"
    printf 'prerelease\t%s-testing-%s\t%s\n' "$n" "$newest_date" "$(release_url "$candidate_tag")"
  else
    # If the next prerelease stream exists (e.g. local v0-testing but v1-testing
    # is published), treat it as newer.
    newest_next_date="$(
      printf '%s\n' "$tags" \
        | sed -n "s/^v${next_n}-testing-\\([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]\\)$/\\1/p" \
        | sort \
        | tail -n 1
    )" || true

    if [ -n "$newest_next_date" ]; then
      candidate_tag="v${next_n}-testing-${newest_next_date}"
      printf 'prerelease\t%s-testing-%s\t%s\n' "$next_n" "$newest_next_date" "$(release_url "$candidate_tag")"
    else
      printf 'none\t-\t-\n'
    fi
  fi
  exit 0
fi

echo "Unsupported local release version format: $local_version" >&2
exit 2

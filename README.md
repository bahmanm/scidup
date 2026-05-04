[![Build, Lint and Test](https://github.com/bahmanm/scidup/actions/workflows/ci.yml/badge.svg)](https://github.com/bahmanm/scidup/actions/workflows/ci.yml) ![Static Badge](https://img.shields.io/badge/license-gplv2-blue?style=flat&label=license) [![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fbahmanm%2Fscidup.svg?type=shield&issueType=security)](https://app.fossa.com/projects/git%2Bgithub.com%2Fbahmanm%2Fscidup?ref=badge_shield&issueType=security)

<p align="center">
  <img src="docs/assets/img/ScidUp-16x9.png" alt="ScidUp Logo - A Winged Pawn"/>
</p>

# ScidUp

Cross-Platform Chess Database and Analysis GUI. A fork of [the magnificent Scid project](https://sourceforge.net/projects/scid/).

⚠️⚠️⚠️ _It is in the pre-release stage._

## Aspirations

-  Be Lean: Drop features and code paths that are not used or are obsolete.
-  Modernise the codebase: Tests everywhere. Tcl/Tk 9. Re-organise the Tcl code.
-  Improve Installation and Packaging.
-  Enhance the feature-set with a special focus on ScidUp's inherited strengths:
   -  The fantastic database format.
   -  Analysis capability
   -  Engine management
 
## Non-Goals

-  Backward compatibility with Scid 5.x series. 

---

# Download and Run

1. Head over to the [Releases](https://github.com/bahmanm/scidup/releases) page.
2. Download the archive that matches your platform (Linux, macOS Apple Silicon, macOS Intel, Windows).
3. Extract the archive into a directory of your choice.
4. Read the included `README.txt` inside the extracted folder (platform-specific instructions).
   _TL;DR_
   - Linux/macOS: `./bin/scidup`
   - Windows: open `bin/` and double-click `scidup.exe`

---

# Programming Setup

ScidUp doesn't require a whole lot of dependencies and setup to get started hacking on it.

## Common

#### Prerequisites

- CMake 3.30+
- A C++ compiler toolchain
- Tcl/Tk 9 (ScidUp currently targets Tcl/Tk 9.0.3)

#### Configure and build

ScidUp uses CMake presets:

```sh
cmake --preset dev
cmake --build --preset dev
```

Run tests:

```sh
ctest --preset dev --output-on-failure
```

Or with disabled tests (don't do it unless you're debugging a build issue):

```sh
cmake --preset dev-no-checks
cmake --build --preset dev-no-checks
```

#### Pointing ScidUp at Tcl/Tk

`dev` / `dev-no-checks` do not hard-code a Tcl/Tk prefix.

If you have a dedicated Tcl/Tk prefix, the most reliable approach is to use `DEPS_INSTALL_PREFIX` (ScidUp will exclusively search it to avoid mixing Tcl/Tk installations from different providers):

```sh
export DEPS_INSTALL_PREFIX="/path/to/tcltk/prefix"
cmake --preset dev
```

Otherwise, if your Tcl/Tk is not in a default search path, pass a prefix explicitly:

```sh
cmake --preset dev -DCMAKE_PREFIX_PATH="/path/to/tcltk/prefix;$CMAKE_PREFIX_PATH"
```

## Ubuntu

#### Notes

- A full GUI build requires X11 development headers (for example `libx11-dev`).
- ScidUp expects Tcl/Tk 9.0.3. If your system Tcl/Tk does not match, install a dedicated Tcl/Tk prefix and set `DEPS_INSTALL_PREFIX`.

#### Building Tcl/Tk 9.0.3 (suggested)

```sh
sudo apt-get update
sudo apt-get install -y libx11-dev

set -euo pipefail

PREFIX="$HOME/scidup-deps/install"
BUILD="$HOME/scidup-deps/build"

mkdir -p "$PREFIX" "$BUILD"
cd "$BUILD"

curl -L http://prdownloads.sourceforge.net/tcl/tcl9.0.3-src.tar.gz | tar -xzf -
pushd tcl9.0.3/unix
./configure --prefix="$PREFIX" --enable-64bit --enable-zipfs
make install
popd

curl -L http://prdownloads.sourceforge.net/tcl/tk9.0.3-src.tar.gz | tar -xzf -
pushd tk9.0.3/unix
./configure --prefix="$PREFIX" --enable-64bit --enable-zipfs --with-tcl="$BUILD/tcl9.0.3/unix"
make install
popd

ln -sf "$PREFIX/bin/tclsh9.0" "$PREFIX/bin/tclsh"
ln -sf "$PREFIX/bin/wish9.0" "$PREFIX/bin/wish"
```

Then:

```sh
export DEPS_INSTALL_PREFIX="$HOME/scidup-deps/install"
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## macOS (Apple Silicon)

#### Notes

- ScidUp expects Tcl/Tk 9.0.3. If your system Tcl/Tk does not match, install a dedicated Tcl/Tk prefix and set `DEPS_INSTALL_PREFIX`.

#### Building Tcl/Tk 9.0.3 (suggested)

This mirrors the release pipeline’s approach (pinned version, local prefix):

```sh
set -euo pipefail

PREFIX="$HOME/scidup-deps/install"
BUILD="$HOME/scidup-deps/build"

mkdir -p "$PREFIX" "$BUILD"
cd "$BUILD"

curl -L http://prdownloads.sourceforge.net/tcl/tcl9.0.3-src.tar.gz | tar -xzf -
pushd tcl9.0.3/unix
./configure --prefix="$PREFIX" --enable-64bit --enable-zipfs
make install
popd

curl -L http://prdownloads.sourceforge.net/tcl/tk9.0.3-src.tar.gz | tar -xzf -
pushd tk9.0.3/unix
./configure --prefix="$PREFIX" --enable-64bit --enable-zipfs --enable-aqua --with-tcl="$BUILD/tcl9.0.3/unix"
make install
popd

ln -sf "$PREFIX/bin/tclsh9.0" "$PREFIX/bin/tclsh"
ln -sf "$PREFIX/bin/wish9.0" "$PREFIX/bin/wish"
```

Then:

```sh
export DEPS_INSTALL_PREFIX="$HOME/scidup-deps/install"
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

---

# License

ScidUp is distributed under the GNU GPL v2 (see `COPYING`). 
Unless stated otherwise, modifications and additions in this fork are © their respective 
contributors (see Git history) and licensed under the same terms.

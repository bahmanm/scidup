# libscidup-database

`libscidup-database` is a C++20 library for reading, writing, querying and
transforming chess databases.

The library provides a stateful database session facade over chess game data,
index metadata, name tables, filters, search helpers and storage codecs. Its
native focus is the SCID database family, with SCID5 represented as a logical
three-file database unit: `.si5`, `.sg5` and `.sn5`.

# 1. Documentation

- [Introduction](docs/introduction.md): concepts, history and architecture.
- [SCID5 File Format](docs/file-format-scid5.md): on-disk model, limits and
  encoding notes.
- [Recipes](docs/recipes.md): task-oriented examples.
- [API Surface](docs/api-surface.md): map of public headers and type roles.

# 2. Current Scope

The current library surface includes:

- database sessions through `scidBaseT`
- editable games through `Game`
- lightweight game cursors through `GameView`
- metadata through `IndexEntry` and `NameBase`
- result sets through `HFilter`
- query helpers for position, header and tree workflows
- codec-backed storage for memory, PGN, SCID4 and SCID5 databases

Concrete codec classes are implementation details. Use the session facade
unless you are deliberately working on the storage layer itself.

# 3. Build

From this directory:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target scidup_database scidup_tests_database_cpp
ctest --test-dir build --output-on-failure
```

The concrete CMake target is `scidup_database`; consumers should link the
namespaced alias `ScidUp::Database`.

# 4. Minimal Example

```cpp
#include "scidup/database/pgnparse.h"
#include "scidup/database/scidbase.h"

#include <iostream>
#include <string_view>

int main() {
    scidBaseT database;
    if (auto err = database.open("MEMORY", FMODE_Create, "Memory")) {
        std::cerr << "open failed: " << err << '\n';
        return 1;
    }

    constexpr std::string_view pgn =
        "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 1/2-1/2";

    Game game;
    PgnParseLog log;
    if (!pgnParseGame(pgn.data(), pgn.size(), game, log)) {
        std::cerr << log.log << '\n';
        return 1;
    }

    if (auto err = database.saveGame(game)) {
        std::cerr << "save failed: " << err << '\n';
        return 1;
    }

    std::cout << database.numGames() << '\n';
}
```

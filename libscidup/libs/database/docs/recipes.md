# Recipes

These examples show common tasks using the public library headers.

# 1. Create An In-Memory Database

```cpp
#include "scidup/database/scidbase.h"

scidBaseT database;
errorT err = database.open("MEMORY", FMODE_Create, "Memory");
if (err != OK) {
    // Handle the error.
}
```

Memory databases are useful for tests, temporary transformations and small
programmatic workflows.

# 2. Parse PGN And Save A Game

```cpp
#include "scidup/database/pgnparse.h"
#include "scidup/database/scidbase.h"

#include <string_view>

scidBaseT database;
auto err = database.open("MEMORY", FMODE_Create, "Memory");
if (err != OK) {
    return err;
}

constexpr std::string_view pgn =
    "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 1/2-1/2";

Game game;
PgnParseLog log;
if (!pgnParseGame(pgn.data(), pgn.size(), game, log)) {
    return ERROR_BadArg;
}

return database.saveGame(game);
```

Use `Game` when the game needs to be parsed, edited or saved.

# 3. Open A SCID5 Database

```cpp
#include "scidup/database/scidbase.h"

scidBaseT database;
errorT err = database.open("SCID5", FMODE_Both, "/path/to/database");
if (err != OK) {
    // Handle the error.
}
```

The filename is the database base path. The SCID5 codec resolves the `.si5`,
`.sg5` and `.sn5` members of the database unit.

Use `FMODE_ReadOnly` when the caller must not modify the database.

# 4. Iterate Game Metadata

```cpp
#include "scidup/database/scidbase.h"

for (gamenumT gnum = 0; gnum < database.numGames(); ++gnum) {
    const IndexEntry* entry = database.getIndexEntry(gnum);
    TagRoster tags = database.tagRoster(*entry);

    // entry contains IDs, dates, result, flags, ratings and storage offsets.
    // tags resolves the main PGN roster names through the session's NameBase.
}
```

`IndexEntry` is the fast path for metadata. It avoids decoding the full game.

# 5. Load A Full Editable Game

```cpp
#include "scidup/database/scidbase.h"

Game game;
if (database.loadGame(0, game) == OK) {
    game.MoveToEnd();
}
```

Use this path when you need the full mutable game tree, tags, comments or PGN
serialisation.

# 6. Inspect Moves With GameView

```cpp
#include "scidup/database/scidbase.h"

const IndexEntry* entry = database.getIndexEntry_bounds(0);
if (entry != nullptr) {
    GameView view = database.getGame(entry);
    FullMove firstMove = view.getMove(0);
}
```

`GameView` is a transient cursor over encoded game data. It is suitable for
read-only move inspection and search helpers.

# 7. Work With Filters

```cpp
#include "scidup/database/scidbase.h"

std::string filterId = database.newFilter();
HFilter filter = database.getFilter(filterId);

filter->clear();
filter.set(0, 1);

database.deleteFilter(filterId.c_str());
```

Filters are selections over game numbers. They are used by search, list and
bulk transformation APIs.

The built-in default database filter is available as `database.defaultFilter()`
or by name through `database.getFilter("dbfilter")`.

# 8. List Games In Sorted Order

```cpp
#include "scidup/database/scidbase.h"

HFilter filter = database.defaultFilter();
std::vector<gamenumT> rows(100);

size_t written =
    database.listGames("d-", 0, rows.size(), filter, rows.data());
rows.resize(written);
```

Sort criteria use compact field codes followed by `+` for ascending order or
`-` for descending order. For example, `d-` sorts by game date descending.
Creating a matching sort cache first can make repeated list operations faster.

# 9. Import PGN Into An Existing Database

```cpp
#include "scidup/database/scidbase.h"

#include <string>

std::string errors;
errorT err = database.importGames(
    ICodecDatabase::PGN,
    "/path/to/input.pgn",
    Progress(),
    errors);
```

The error string may contain parser diagnostics even when some games were
successfully imported.

# 10. Compact A Database

```cpp
#include "scidup/database/scidbase.h"

unsigned long long deleted = 0;
unsigned long long unused = 0;
unsigned long long sparse = 0;
unsigned long long badNameIds = 0;

if (database.getCompactStat(&deleted, &unused, &sparse, &badNameIds) == OK) {
    errorT err = database.compact(Progress());
}
```

Compaction rewrites the physical database files and is the correct way to
remove deleted or sparse native data.

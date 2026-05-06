# API Surface

This document maps the current public include tree. It is a guide to what each
header is for, not a generated symbol reference.

# 1. Primary Entry Point

Use `scidup/database/scidbase.h` first.

`scidBaseT` is the database session facade. It owns one open database session
and coordinates:

- opening and closing databases
- reading and saving games
- importing games
- filters
- flags
- statistics
- list and sort operations
- name and index transformations
- compaction

Prefer this facade over lower-level codec access.

Sort criteria accepted by `createSortCache()`, `listGames()` and
`sortedPosition()` are compact field codes followed by `+` or `-`. Common codes
include `d` for game date, `y` for year, `e` for event, `s` for site, `n` for
round, `w` for white, `b` for black, `o` for ECO, `r` for result, `m` for move
count, `W` for white Elo and `B` for black Elo.

# 2. Game Model

Use these headers when working with chess games and database-owned game
workflows:

- `game.h`: editable game model, PGN writing, tags, comments, variations and
  encoded game conversion
- `gameview.h`: lightweight read-only cursor over encoded moves
- `movetree.h`: move-tree storage for editable games

Use `Game` when you need mutation. Use `GameView` when you need fast read-only
inspection of encoded moves. Use `scidup/core/position.h` for standalone board
state, move generation and FEN/UCI position support.

# 3. Database Metadata

Use these headers when working with game lists, tags and names:

- `indexentry.h`: per-game metadata record
- `index.h`: collection of index entries
- `namebase.h`: mapping between name IDs and player/event/site/round strings
- `date.h`: compact date representation and helpers
- `tree.h`: opening-tree result statistics
- `matsig.h`: material signatures
- `stored.h`: stored-line helpers used by position search

Most consumers should retrieve metadata through `scidBaseT` instead of owning
`Index` or `NameBase` directly.

# 4. Filters And Query Helpers

Use these headers for result sets and searches:

- `hfilter.h`: filter storage and filtered iteration
- `searchpos.h`: position search over a database session
- `searchtournaments.h`: grouping and filtering games into tournaments

`HFilter` is a handle-like object over filter storage. Filters are owned either
by the database session or by caller-owned filter storage. Be careful not to use
an `HFilter` after the underlying filter has been deleted.

# 5. PGN Support

Use these headers for PGN parsing and encoding:

- `pgnparse.h`: parse PGN into `Game`
- `pgn_encode.h`: encode PGN output
- `pgn_lexer.h`: lower-level PGN tokenisation helpers

Most consumers should start with `pgnparse.h`.

# 6. Storage And Low-Level Helpers

These headers are public today because current consumers and tests need them,
but they should be treated as lower-level building blocks:

- `bytebuf.h`: transient byte buffer views and encoded tag helpers
- `filebuf.h`: file buffer helpers
- `containers.h`: custom containers used by game and database storage
- `common.h`: database file-format aliases, result constants and compatibility
  assertions
- `misc.h`: assorted utility functions

Board constants, primitive chess types, square helpers, attack tables, position
state, position hashing and shared status codes live in core headers:

- `scidup/core/primitives.h`: scalar aliases and primitive chess encodings
- `scidup/core/board.h`: board constants, piece helpers, square helpers and
  direction helpers
- `scidup/core/error.h`: shared `errorT` definitions
- `scidup/core/fullmove.h`: compact move representation with SAN-related
  information
- `scidup/core/hash.h`: Zobrist position hash helpers
- `scidup/core/movelist.h`: generated move records and fixed-capacity move lists
- `scidup/core/move_predicates.h`: move validation predicates
- `scidup/core/position.h`: board state, move generation and FEN/UCI position
  support
- `scidup/core/square_moves.h`: square movement lookup tables and helpers
- `scidup/core/square_collections.h`: square list and set helpers
- `scidup/core/attacks.h`: precomputed king and knight attack tables
- `scidup/core/dstring.h`: deprecated dynamic string helper still used by some
  board-output paths

These headers are useful when extending the library, but they are not the best
starting point for application code.

# 7. Codec Boundary

`codec.h` defines `ICodecDatabase`, the abstraction between a database session
and concrete storage. It exists so `scidBaseT` can work with memory, PGN, SCID4
and SCID5 representations through one interface.

The concrete codec headers are deliberately under `src/internal` and should not
be included by consumers. Open databases through `scidBaseT::open()` instead of
constructing codec implementations directly.

# 8. Current Stability Notes

The public include tree is broader than the ideal long-term consumer API. This
is partly because the library still exposes some compatibility and lower-level
types needed by existing workflows.

For new consumer code, prefer this order:

1. `scidbase.h`
2. `game.h`
3. `pgnparse.h`
4. `hfilter.h`
5. search/query headers as needed
6. lower-level helpers only when the higher-level facade is insufficient

# Introduction

`libscidup-database` is a C++ chess database library. It provides database
sessions, game storage, metadata indexing, name tables, filters, searches and
format codecs for applications that need direct access to chess database data.

# 1. Roots

The code descends from Scid, Shane's Chess Information Database, and later
modernisation work in ScidUp. That history matters because the library carries a
mature chess database model rather than a newly invented storage API.

The rest of this documentation treats `libscidup-database` as a library in its
own right. You do not need to use it through ScidUp.

# 2. Mental Model

The central abstraction is `scidBaseT`, a database session. A session owns the
durable state needed to work with one open database:

- an index of games
- a namebase for players, events, sites and rounds
- a storage codec
- filters and cached query state
- derived statistics and sort caches

Multiple `scidBaseT` objects may be open at the same time, and each object owns
one database session.

# 3. Layers

The library is organised around three layers:

1. Session facade
   `scidBaseT` is the normal entry point for consumers. It opens databases,
   loads games, saves games, manages filters, lists games, transforms metadata,
   compacts storage and exposes statistics.

2. Internal codec abstraction
   The session facade uses an internal codec boundary to read and write encoded
   game data, index entries and names without exposing concrete storage details
   as consumer-facing API.

3. Concrete codecs
   The concrete codecs implement memory, PGN, SCID4 and SCID5 storage. These
   classes live under `src/internal` and are not intended as consumer-facing API.

# 4. Important Types

`Game` is the editable game model. Use it when you need to parse PGN, modify
moves, edit tags, add comments or save a game.

`GameView` is a lighter cursor over encoded move data. Use it when you need to
inspect moves or search through a game without constructing a full editable
`Game`.

`ByteBuffer` is a transient view of encoded game data. Its lifetime depends on
the codec call that produced it.

`IndexEntry` stores per-game metadata: player IDs, event/site/round IDs, dates,
ratings, result, flags, ECO code, offset and encoded game length.

`NameBase` maps IDs to names for players, events, sites and rounds.

`HFilter` represents a selected set of games. Filters are used by searches,
bulk transformations, list operations and import/copy workflows.

# 5. Storage Formats

The native SCID5 database is not a single file. It is a logical unit made from:

- `.si5`: fixed-size index records
- `.sg5`: encoded game data
- `.sn5`: names and database metadata

Applications should treat those files as one database. Copying, deleting,
renaming or backing up only one member of the triplet corrupts the logical
database unit.

The library also contains support for memory databases, SCID4 databases and PGN
input. The capabilities differ by codec. For example, PGN is useful as an input
format, while memory databases are useful for tests, temporary datasets and
programmatic transformations.

# 6. State And Lifetime

The library is stateful. Opening a database populates the session's index,
namebase and codec state. Mutating operations go through the session so that
dependent caches and filters can be invalidated or resized consistently.

Durable state is owned by `scidBaseT` and its codec. Transient objects such as
`ByteBuffer` and `GameView` should be treated as views. Do not keep them after
the session or codec has performed another operation that may invalidate the
underlying buffer.

# 7. Error Handling

Most operations return `errorT`. `OK` means success; other values indicate
errors such as bad arguments, read-only access, unsupported codec features,
corrupt input, file I/O failure or user cancellation through a progress
callback.

Check return values. Some low-level helpers also use assertions to document
preconditions, so defensive callers should prefer bounds-checked entry points
such as `getIndexEntry_bounds()` when handling external input.

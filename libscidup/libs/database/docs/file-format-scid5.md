# SCID5 File Format

This document describes the SCID5 storage model used by
`libscidup-database`.

# 1. Database Unit

A SCID5 database is a logical three-file unit:

- `.si5`: index records
- `.sg5`: encoded game data
- `.sn5`: namebase and database metadata

All three files belong together. A database path should be understood as the
base name of this triplet rather than as one physical file.

# 2. Index File

The `.si5` file stores one fixed-size record per game. Each record is 56 bytes:

- 12 little-endian `uint32_t` words
- 8 bytes of home-pawn data

The fixed record size allows an existing game's index entry to be overwritten in
place.

Each record contains:

- comment, variation and NAG count ratings
- white, black, event, site and round name IDs
- standard chess versus Chess960 flag
- white and black Elo values
- game date and event date
- halfmove count
- game flags
- encoded game data offset and length
- stored-line code
- final material signature
- home-pawn data
- white and black rating types
- result
- ECO code

# 3. Namebase File

The `.sn5` file stores strings with an associated name type. The current name
types are:

- player
- event
- site
- round
- database information

Each entry is encoded as a varint followed by string bytes. The low three bits
of the varint store the name type; the remaining bits store the string length.

The namebase is append-oriented. When a game is modified and uses a new name,
that name is appended and the new ID is written into the corresponding index
entry.

Database information such as type, description, autoload and flag labels is
also stored in the namebase stream.

# 4. Game Data File

The `.sg5` file stores encoded game blobs. A game blob contains:

- extra PGN tag pairs not represented directly in the index
- an optional initial FEN
- moves
- comments
- Numeric Annotation Glyphs
- variation markers

Move data is compactly encoded. Normal moves use one byte where the high four
bits identify the moving piece index. The king always has index `0`, leaving
low special values available for null moves, castling, NAGs, comments,
variation start/end and end-of-game markers.

Trailing comment placeholders may be omitted when they are not needed.

# 5. Limits

The SCID5 codec currently encodes these practical limits:

- maximum games: about 4 billion
- maximum game data file size: 128 TB
- maximum encoded data for one game: 128 KB
- maximum unique player names: 268,435,456
- maximum unique event names: 268,435,456
- maximum unique round names: 2,147,483,648

Index records also impose field-specific limits, such as:

- Elo values up to 4000
- dates up to year 2047
- halfmove counts up to 1023
- tag names up to 240 characters
- tag values up to 255 characters

# 6. Deletion And Compaction

Game flags can mark games as deleted, but that does not necessarily remove data
from the `.sg5` file immediately. Compaction rewrites the database into a fresh
database unit and removes unused or sparse data.

Applications that need to reclaim disk space should call the session compaction
API rather than trying to edit the triplet directly.

# 7. Compatibility Notes

SCID5 stores names and game data in a compact representation designed for fast
database operations. It is not a PGN archive. PGN import/export is a separate
concern from the native database representation.

Consumers should use `scidBaseT` and the public library APIs to manipulate SCID5
databases. Direct modification of `.si5`, `.sg5` or `.sn5` files requires full
knowledge of the encoding and consistency rules.

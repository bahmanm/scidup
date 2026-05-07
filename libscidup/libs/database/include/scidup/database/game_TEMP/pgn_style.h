#pragma once

namespace scid::database {

enum gameFormatT {
	PGN_FORMAT_Plain = 0, // Plain regular PGN output
	PGN_FORMAT_HTML = 1,  // HTML format
	PGN_FORMAT_LaTeX = 2, // LaTeX (with chess12 package) format
	PGN_FORMAT_Color = 3  // PGN, with color tags <red> etc
};

} // namespace scid::database

#define PGN_STYLE_TAGS 1
#define PGN_STYLE_COMMENTS 2
#define PGN_STYLE_VARS 4
#define PGN_STYLE_INDENT_COMMENTS 8
#define PGN_STYLE_INDENT_VARS 16
#define PGN_STYLE_SYMBOLS 32
#define PGN_STYLE_SHORT_HEADER 64
#define PGN_STYLE_MOVENUM_SPACE 128
#define PGN_STYLE_COLUMN 256
#define PGN_STYLE_SCIDFLAGS 512
#define PGN_STYLE_STRIP_MARKS 1024
#define PGN_STYLE_NO_NULL_MOVES 2048
#define PGN_STYLE_UNICODE 4096

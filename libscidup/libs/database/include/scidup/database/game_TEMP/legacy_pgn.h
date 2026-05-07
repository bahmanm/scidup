#pragma once

#include "scidup/core/primitives.h"

#include <utility>

namespace scid::database {

void transPieces(char* s);
char transPiecesChar(char c);

// Piece letters translation
extern int language; // default to english
//  0 = en, 1 = fr, 2 = es, 3 = de
extern const char* langPieces[];

enum gameFormatT {
	PGN_FORMAT_Plain = 0, // Plain regular PGN output
	PGN_FORMAT_HTML = 1,  // HTML format
	PGN_FORMAT_LaTeX = 2, // LaTeX (with chess12 package) format
	PGN_FORMAT_Color = 3  // PGN, with color tags <red> etc
};

void game_printNag(byte nag, char* str, bool asSymbol, gameFormatT format);
byte game_parseNag(std::pair<const char*, const char*> strview);

} // namespace scid::database

#define PGN_STYLE_TAGS             1
#define PGN_STYLE_COMMENTS         2
#define PGN_STYLE_VARS             4
#define PGN_STYLE_INDENT_COMMENTS  8
#define PGN_STYLE_INDENT_VARS     16
#define PGN_STYLE_SYMBOLS         32   // e.g. "! +-" instead of "$2 $14"
#define PGN_STYLE_SHORT_HEADER    64
#define PGN_STYLE_MOVENUM_SPACE  128   // Space after move numbers.
#define PGN_STYLE_COLUMN         256   // Column style: one move per line.
#define PGN_STYLE_SCIDFLAGS      512
#define PGN_STYLE_STRIP_MARKS   1024   // Strip [%mark] and [%arrow] codes.
#define PGN_STYLE_NO_NULL_MOVES 2048   // Convert null moves to comments.
#define PGN_STYLE_UNICODE       4096   // Use U+2654..U+2659 for figurine


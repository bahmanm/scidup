#pragma once

#include "scidup/core/primitives.h"

namespace scidup::app {

// Legacy application-wide piece-letter translation used by Tcl/UI export
// compatibility. This is not part of plain PGN encoding.
void transPieces(char* s);
char transPiecesChar(char c);

// Piece letters translation
extern int language; // default to english
//  0 = en, 1 = fr, 2 = es, 3 = de
extern const char* langPieces[];

} // namespace scidup::app

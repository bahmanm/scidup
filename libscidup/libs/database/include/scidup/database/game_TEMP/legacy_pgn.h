#pragma once

#include "scidup/core/primitives.h"

namespace scid::database {

void transPieces(char* s);
char transPiecesChar(char c);

// Piece letters translation
extern int language; // default to english
//  0 = en, 1 = fr, 2 = es, 3 = de
extern const char* langPieces[];

} // namespace scid::database

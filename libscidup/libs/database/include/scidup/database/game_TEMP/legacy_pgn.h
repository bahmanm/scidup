#pragma once

#include "scidup/core/primitives.h"
#include "scidup/database/game_TEMP/pgn_style.h"

#include <utility>

namespace scid::database {

void transPieces(char* s);
char transPiecesChar(char c);

// Piece letters translation
extern int language; // default to english
//  0 = en, 1 = fr, 2 = es, 3 = de
extern const char* langPieces[];

void game_printNag(byte nag, char* str, bool asSymbol, gameFormatT format);
byte game_parseNag(std::pair<const char*, const char*> strview);

} // namespace scid::database

#pragma once

#include "scidup/core/board.h"

namespace scid::database {

// Pattern filter for material searches.
// It can specify, for example, a white pawn on the f-file, or a black bishop
// on f2 and white king on e1.
struct patternT {
	pieceT pieceMatch;
	rankT rankMatch;
	fyleT fyleMatch;
	byte flag; // 0 means this pattern must not occur.
};

enum gameExactMatchT {
	GAME_EXACT_MATCH_Exact = 0,
	GAME_EXACT_MATCH_Pawns,
	GAME_EXACT_MATCH_Fyles,
	GAME_EXACT_MATCH_Material
};

} // namespace scid::database

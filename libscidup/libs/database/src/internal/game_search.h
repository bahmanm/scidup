#pragma once

#include "scidup/core/board.h"

#include <cstddef>

namespace scid::database {

class ByteBuffer;
class Game;
class Position;

// Pattern filter for material searches.
// It can specify, for example, a white pawn on the f-file, or a black bishop
// on f2 and white king on e1.
struct patternT {
	pieceT pieceMatch;
	rankT rankMatch;
	fyleT fyleMatch;
	byte flag; // 0 means this pattern must not occur.
};

enum gameExactMatchT : int {
	GAME_EXACT_MATCH_Exact = 0,
	GAME_EXACT_MATCH_Pawns,
	GAME_EXACT_MATCH_Fyles,
	GAME_EXACT_MATCH_Material
};

namespace game_search {

bool materialMatch(Game& game, bool promotionsFlag, ByteBuffer& buf, byte* min,
                   byte* max, patternT* ptn, std::size_t ptnSize, int minPly,
                   int maxPly, int matchLength, bool oppBishops,
                   bool sameBishops, int minDiff, int maxDiff);
bool exactMatch(Game& game, Position* pos, ByteBuffer* buf,
                gameExactMatchT searchType);
bool varExactMatch(Game& game, Position* pos, gameExactMatchT searchType);

} // namespace game_search

} // namespace scid::database

#pragma once

#include "scidup/core/board.h"
#include "scidup/core/game.h"

#include <cstddef>

namespace scid::core {
class Position;
} // namespace scid::core

namespace scid::database {

class ByteBuffer;

// Pattern filter for material searches.
// It can specify, for example, a white pawn on the f-file, or a black bishop
// on f2 and white king on e1.
struct patternT {
	scid::core::pieceT pieceMatch;
	scid::core::rankT rankMatch;
	scid::core::fyleT fyleMatch;
	scid::core::byte flag; // 0 means this pattern must not occur.
};

enum gameExactMatchT : int {
	GAME_EXACT_MATCH_Exact = 0,
	GAME_EXACT_MATCH_Pawns,
	GAME_EXACT_MATCH_Fyles,
	GAME_EXACT_MATCH_Material
};

namespace game_search {

bool materialMatch(bool promotionsFlag, ByteBuffer& buf, scid::core::byte* min, scid::core::byte* max,
                   patternT* ptn, std::size_t ptnSize, int minPly, int maxPly,
                   int matchLength, bool oppBishops, bool sameBishops,
                   int minDiff, int maxDiff);
bool exactMatch(const scid::core::Game& game, scid::core::Position* pos, ByteBuffer* buf,
                gameExactMatchT searchType);
bool varExactMatch(const scid::core::Game& game, scid::core::Position* pos,
                   gameExactMatchT searchType);

} // namespace game_search

} // namespace scid::database

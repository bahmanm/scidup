#pragma once

#include "scidup/core/game.h"
#include "scidup/database/scidbase.h"

#include <cstddef>

namespace scid::core {
class Position;
} // namespace scid::core

namespace scid::database {

class ByteBuffer;

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

#pragma once

#include "scidup/database/game_TEMP/legacy_encode_options.h"

#include <utility>

namespace scid::database {

class Game;

namespace legacy_pgn {

std::pair<const char*, unsigned> encode(
    Game& game,
    LegacyGameEncodeOptions options = defaultLegacyGameEncodeOptions(),
    unsigned lineWidth = 0,
    bool newLineAtEnd = false,
    bool newLineToSpaces = true);

} // namespace legacy_pgn

} // namespace scid::database

#pragma once

#include "legacy_encode_options.h"
#include "scidup/core/game.h"

#include <utility>

namespace scid::database {

namespace legacy_pgn {

std::pair<const char*, unsigned> encode(
    const scid::core::Game& game,
    const char* scidFlags,
    LegacyGameEncodeOptions options = defaultLegacyGameEncodeOptions(),
    unsigned lineWidth = 0,
    bool newLineAtEnd = false,
    bool newLineToSpaces = true);

} // namespace legacy_pgn

} // namespace scid::database

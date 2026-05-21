#pragma once

#include "scidup_app_legacy_encode_options.h"
#include "scidup/core/game.h"

#include <utility>

namespace scidup::app {

namespace legacy_pgn {

std::pair<const char*, unsigned> encode(
    const scid::core::Game& game,
    const char* scidFlags,
    LegacyGameEncodeOptions options = defaultLegacyGameEncodeOptions(),
    unsigned lineWidth = 0,
    bool newLineAtEnd = false,
    bool newLineToSpaces = true);

} // namespace legacy_pgn

} // namespace scidup::app

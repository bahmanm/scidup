#pragma once

#include "scidup/core/game_cursor.h"

namespace scid::core::pgn {

bool nextLocation(GameCursor& cursor);
bool seekLocation(GameCursor& cursor, unsigned location);
unsigned locationOf(const GameCursor& cursor);
unsigned offsetOf(const GameCursor& cursor);

} // namespace scid::core::pgn

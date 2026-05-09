#pragma once

#include "scidup/database/common.h"

namespace scid::database {

class Game;

namespace game_state {

uint currentPly(const Game& game);
uint mainlineHalfMoveCount(const Game& game);

} // namespace game_state

} // namespace scid::database

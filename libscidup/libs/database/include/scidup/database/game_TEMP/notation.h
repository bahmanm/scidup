#pragma once

#include "scidup/database/common.h"

#include <string>

namespace scid::database {

class DString;
class Game;

namespace game_notation {

std::string currentPositionUci(const Game& game);
std::string nextMoveUci(const Game& game);
std::string nextSan(Game& game);
std::string previousSan(Game& game);
std::string previousMoveUci(const Game& game);
errorT writePartialMoveList(Game& game, DString& out, uint plyCount);

} // namespace game_notation

} // namespace scid::database

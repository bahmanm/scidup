#pragma once

#include <string>

namespace scid::database {

class Game;

namespace game_notation {

std::string currentPositionUci(const Game& game);
std::string nextMoveUci(const Game& game);
std::string nextSan(Game& game);
std::string previousSan(Game& game);
std::string previousMoveUci(const Game& game);

} // namespace game_notation

} // namespace scid::database

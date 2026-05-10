#pragma once

#include <string>

namespace scid::database {

class Game;

namespace game_notation {

std::string nextSan(Game& game);
std::string previousSan(Game& game);

} // namespace game_notation

} // namespace scid::database

#pragma once

namespace scid::core {
class Game;
}

namespace scid::database {

struct moveT;

namespace TEMP_movetext {

void syncCoreMovetext(scid::core::Game& coreGame, const moveT* firstMove);

} // namespace TEMP_movetext

} // namespace scid::database

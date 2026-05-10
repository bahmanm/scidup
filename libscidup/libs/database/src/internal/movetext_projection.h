#pragma once

namespace scid::core {
class Game;
class MovetextLocation;
}

namespace scid::database {

struct moveT;

namespace TEMP_movetext {

void syncCoreMovetext(scid::core::Game& coreGame, const moveT* firstMove);
void syncCoreMovetextAndLocation(scid::core::Game& coreGame,
                                 const moveT* firstMove,
                                 const moveT* currentMove,
                                 scid::core::MovetextLocation& location);

} // namespace TEMP_movetext

} // namespace scid::database

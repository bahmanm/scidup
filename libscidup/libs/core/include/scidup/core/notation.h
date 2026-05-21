#pragma once

#include "scidup/core/move.h"
#include "scidup/core/primitives.h"
#include <cstddef>
#include <optional>
#include <string>

namespace scid::core {
class Position;
} // namespace scid::core

namespace scid::core {
class Game;
class MovetextLocation;

namespace notation {

std::string currentPositionUci(const Game& game, MovetextLocation location);
std::string previousMoveUci(const Game& game, MovetextLocation location);
std::string nextMoveUci(const Game& game, MovetextLocation location);
std::string previousSan(const Game& game, MovetextLocation location);
std::string nextSan(const Game& game, MovetextLocation location);
std::string partialMoveList(const Game& game, std::size_t plyCount);

} // namespace notation
} // namespace scid::core

namespace scid::core {

inline constexpr std::size_t SAN_STRING_SIZE = 10;
using sanStringT = char[SAN_STRING_SIZE];

using sanFlagT = byte;
inline constexpr sanFlagT SAN_NO_CHECKTEST = 0;
inline constexpr sanFlagT SAN_CHECKTEST = 1;
inline constexpr sanFlagT SAN_MATETEST = 2;

inline constexpr std::size_t UCI_MOVE_STRING_SIZE = 6;

} // namespace scid::core

#pragma once

#include "scidup/core/primitives.h"
#include <cstddef>

namespace scid::database {

inline constexpr std::size_t SAN_STRING_SIZE = 10;
using sanStringT = char[SAN_STRING_SIZE];

using sanFlagT = byte;
inline constexpr sanFlagT SAN_NO_CHECKTEST = 0;
inline constexpr sanFlagT SAN_CHECKTEST = 1;
inline constexpr sanFlagT SAN_MATETEST = 2;

inline constexpr std::size_t UCI_MOVE_STRING_SIZE = 6;

} // namespace scid::database

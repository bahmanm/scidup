#pragma once

#include "scidup/core/primitives.h"
#include <cstdint>

namespace scid::database {

using gamenumT = scid::core::uint;
using idNumberT = std::uint32_t; // Should be idNameT

inline constexpr gamenumT INVALID_GAMEID = 0xffffffff;

} // namespace scid::database

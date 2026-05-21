#pragma once

#include "scidup/core/primitives.h"
#include <cstddef>

namespace scid::core {

using ratingT = ushort;
using ratingTypeT = byte;

inline constexpr ratingTypeT RATING_Elo = 0;
inline constexpr ratingTypeT RATING_Rating = 1;
inline constexpr ratingTypeT RATING_Rapid = 2;
inline constexpr ratingTypeT RATING_ICCF = 3;
inline constexpr ratingTypeT RATING_USCF = 4;
inline constexpr ratingTypeT RATING_DWZ = 5;
inline constexpr ratingTypeT RATING_BCF = 6;

inline constexpr std::size_t NUM_RATING_TYPES = 7;
inline constexpr const char* ratingTypeNames[NUM_RATING_TYPES + 1] = {
    "Elo", "Rating", "Rapid", "ICCF", "USCF", "DWZ", "ECF", nullptr};

} // namespace scid::core

#pragma once

#include <cstdint>

namespace scidup::eco {

using Code = std::uint16_t;
using String = char[6];

inline constexpr Code ECO_None = 0;

void toString(Code ecoCode, char* ecoStr, bool extensions = true);
inline void toBasicString(Code ecoCode, char* ecoStr) {
	toString(ecoCode, ecoStr, false);
}
inline void toExtendedString(Code ecoCode, char* ecoStr) {
	toString(ecoCode, ecoStr, true);
}

Code fromString(const char* ecoStr);
Code lastSubCode(Code ecoCode);
Code basicCode(Code ecoCode);
Code reduce(Code ecoCode);

} // namespace scidup::eco

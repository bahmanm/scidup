#pragma once

#include "scidup/database/common.h"

namespace scid::database::eco_code {

using String = char[6];

EcoCode fromString(const char* ecoStr);
void toString(EcoCode ecoCode, char* ecoStr, bool extensions = true);
EcoCode basicCode(EcoCode ecoCode);
EcoCode lastSubCode(EcoCode ecoCode);
EcoCode reduce(EcoCode ecoCode);

inline void toBasicString(EcoCode ecoCode, char* ecoStr) {
	toString(ecoCode, ecoStr, false);
}

inline void toExtendedString(EcoCode ecoCode, char* ecoStr) {
	toString(ecoCode, ecoStr, true);
}

} // namespace scid::database::eco_code

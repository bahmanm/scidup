#include "eco_code.h"

#include <cassert>
#include <cmath>

namespace scid::database::eco_code {

EcoCode fromString(const char* ecoStr) {
	EcoCode eco = ECO_CODE_NONE;

	if (*ecoStr >= 'A' && *ecoStr <= 'E') {
		eco = (*ecoStr - 'A') * 13100;
	} else if (*ecoStr >= 'a' && *ecoStr <= 'e') {
		eco = (*ecoStr - 'a') * 13100;
	} else {
		return ECO_CODE_NONE;
	}
	ecoStr++;
	if (!*ecoStr) {
		return eco + 1;
	}

	if (*ecoStr < '0' || *ecoStr > '9') {
		return ECO_CODE_NONE;
	}
	eco += (*ecoStr - '0') * 1310;
	ecoStr++;
	if (!*ecoStr) {
		return eco + 1;
	}

	if (*ecoStr < '0' || *ecoStr > '9') {
		return ECO_CODE_NONE;
	}
	eco += (*ecoStr - '0') * 131;
	ecoStr++;

	if (*ecoStr >= 'a' && *ecoStr <= 'z') {
		eco++;
		eco += (*ecoStr - 'a') * 5;
		ecoStr++;
		if (*ecoStr >= '1' && *ecoStr <= '4') {
			eco += *ecoStr - '0';
		}
	}
	return eco + 1;
}

void toString(EcoCode ecoCode, char* ecoStr, bool extensions) {
	char* s = ecoStr;
	if (ecoCode == ECO_CODE_NONE) {
		*s = 0;
		return;
	}
	ecoCode--;

	EcoCode basic = ecoCode / 131;
	*s++ = basic / 100 + 'A';
	*s++ = (basic % 100) / 10 + '0';
	*s++ = (basic % 10) + '0';

	if (extensions) {
		ecoCode = ecoCode % 131;
		if (ecoCode > 0) {
			ecoCode--;
			*s++ = (ecoCode / 5) + 'a';
			ecoCode = ecoCode % 5;
			if (ecoCode > 0) {
				*s++ = ecoCode + '0';
			}
		}
	}
	*s = 0;
}

EcoCode basicCode(EcoCode ecoCode) {
	if (ecoCode == ECO_CODE_NONE) {
		return ECO_CODE_NONE;
	}

	ecoCode--;
	ecoCode /= 131;
	ecoCode *= 131;
	return ecoCode + 1;
}

EcoCode reduce(EcoCode ecoCode) {
	assert(ecoCode != ECO_CODE_NONE);

	ecoCode--;
	EcoCode result = (ecoCode / 131) * 27;
	return result + static_cast<EcoCode>(std::ceil((ecoCode % 131) / 5.0));
}

EcoCode lastSubCode(EcoCode ecoCode) {
	if (ecoCode == ECO_CODE_NONE) {
		return ECO_CODE_NONE;
	}

	ecoCode--;
	if ((ecoCode % 131) == 0) {
		ecoCode += 126;
	}
	if (((ecoCode % 131) % 5) == 1) {
		ecoCode += 4;
	}
	return ecoCode + 1;
}

} // namespace scid::database::eco_code

#include "nag_format.h"

#include "scidup/core/nags.h"
#include "scidup/database/common.h"
#include "naglatex.h"

#include <cstdio>
#include <cstring>

namespace scid::database {

void game_printNag(byte nag, char* str, bool asSymbol, gameFormatT format) {
	ASSERT(str != NULL);

	if (nag == 0) {
		*str = 0;
		return;
	}

	if (asSymbol && format == PGN_FORMAT_LaTeX &&
	    nag >= (sizeof evalNagsLatex / sizeof(const char*))) {
		*str = 0;
		return;
	}

	if (asSymbol) {
		if (format == PGN_FORMAT_LaTeX) {
			strcpy(str, evalNagsLatex[nag]);
		} else if (format == PGN_FORMAT_HTML &&
		           nag == scid::core::NAG_Diagram) {
			strcpy(str, "<i>(D)</i>");
		} else {
			const auto text = scid::core::formatNag(nag, true);
			strcpy(str, text.c_str());
		}
		return;
	} else {
		std::snprintf(str, 10, "%s$%d",
		              format == PGN_FORMAT_LaTeX ? "\\" : "", nag);
	}
}

} // namespace scid::database

#include "scidup_app_nag_format.h"

#include "scidup_app_nag_latex.h"
#include "scid/core/nags.h"
#include "scid/database/common.h"

#include <cstdio>
#include <cstring>

namespace scidup::app {

void game_printNag(scid::core::Nag nag, char* str, bool asSymbol, gameFormatT format) {
	ASSERT(str != NULL);

	if (nag == scid::core::Nag::None) {
		*str = 0;
		return;
	}

	const auto value = scid::core::nagCode(nag);
	if (asSymbol && format == PGN_FORMAT_LaTeX &&
	    value >= (sizeof(evalNagsLatex) / sizeof(const char*))) {
		*str = 0;
		return;
	}

	if (asSymbol) {
		if (format == PGN_FORMAT_LaTeX) {
			strcpy(str, evalNagsLatex[value]);
		} else if (format == PGN_FORMAT_HTML &&
		           nag == scid::core::Nag::Diagram) {
			strcpy(str, "<i>(D)</i>");
		} else {
			const auto text = scid::core::nagToString(nag, true);
			strcpy(str, text.c_str());
		}
		return;
	} else {
		std::snprintf(str, 10, "%s$%u",
		              format == PGN_FORMAT_LaTeX ? "\\" : "", value);
	}
}

} // namespace scidup::app

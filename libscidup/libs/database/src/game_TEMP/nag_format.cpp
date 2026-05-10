#include "nag_format.h"

#include "scidup/core/nags.h"
#include "scidup/database/common.h"
#include "scidup/database/misc.h"
#include "naglatex.h"
#include "nagtext.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace scid::database {

void game_printNag(byte nag, char* str, bool asSymbol, gameFormatT format) {
	ASSERT(str != NULL);

	if (nag == 0) {
		*str = 0;
		return;
	}

	if (nag >= (sizeof evalNagsRegular / sizeof(const char*))) {
		if (format == PGN_FORMAT_LaTeX)
			*str = 0;
		else
			std::snprintf(str, 10, "$%u", nag);
		return;
	}

	if (asSymbol) {
		if (format == PGN_FORMAT_LaTeX) {
			strcpy(str, evalNagsLatex[nag]);
		} else {
			strcpy(str, evalNagsRegular[nag]);
		}
		if (nag == scid::core::NAG_Diagram) {
			if (format == PGN_FORMAT_LaTeX) {
				strcpy(str, evalNagsLatex[nag]);
			} else if (format == PGN_FORMAT_HTML) {
				strcpy(str, "<i>(D)</i>");
			} else {
				str[0] = 'D';
				str[1] = 0;
			}
		}
		return;
	} else {
		std::snprintf(str, 10, "%s$%d",
		              format == PGN_FORMAT_LaTeX ? "\\" : "", nag);
	}
}

byte game_parseNag(std::pair<const char*, const char*> strview) {
	auto slen = std::distance(strview.first, strview.second);
	if (slen == 0 || slen > 7)
		return 0;

	char strbuf[8] = {0};
	std::copy_n(strview.first, slen, strbuf);
	const char* str = strbuf;

	if (*str == '$') {
		str++;
		return (byte) strGetUnsigned(str);
	}
	if ((*str <= '9' && *str >= '0')) {
		return (byte) strGetUnsigned(str);
	}

	if (*str == '!') {
		str++;
		if (*str == 0) {
			return scid::core::NAG_GoodMove;
		}
		if (*str == '!') {
			return scid::core::NAG_ExcellentMove;
		}
		if (*str == '?') {
			return scid::core::NAG_InterestingMove;
		}
		return 0;
	}

	if (*str == '?') {
		str++;
		if (*str == 0) {
			return scid::core::NAG_PoorMove;
		}
		if (*str == '?') {
			return scid::core::NAG_Blunder;
		}
		if (*str == '!') {
			return scid::core::NAG_DubiousMove;
		}
		return 0;
	}

	if (*str == '+') {
		str++;
		if (*str == '=') {
			return scid::core::NAG_WhiteSlight;
		}
		if (*str == '-' && str[1] == 0) {
			return scid::core::NAG_WhiteDecisive;
		}
		if (*str == '>') {
			return scid::core::NAG_WithAttack;
		}
		if (*str == '/' && str[1] == '-') {
			return scid::core::NAG_WhiteClear;
		}
		if (*str == '/' && str[1] == '=') {
			return scid::core::NAG_WhiteSlight;
		}
		if (*str == '-' && str[1] == '-') {
			return scid::core::NAG_WhiteCrushing;
		}
		return 0;
	}

	if (*str == '=') {
		str++;
		if (*str == 0) {
			return scid::core::NAG_Equal;
		}
		if (*str == '+') {
			return scid::core::NAG_BlackSlight;
		}
		if (*str == '/' && str[1] == '+') {
			return scid::core::NAG_BlackSlight;
		}
		if (*str == '/' && str[1] == '&') {
			return scid::core::NAG_Compensation;
		}
		return 0;
	}

	if (*str == '-') {
		str++;
		if (*str == '+') {
			return scid::core::NAG_BlackDecisive;
		}
		if (*str == '>') {
			return scid::core::NAG_WithBlackAttack;
		}
		if (*str == '/' && str[1] == '+') {
			return scid::core::NAG_BlackClear;
		}
		if (*str == '-' && str[1] == '+') {
			return scid::core::NAG_BlackCrushing;
		}
		if (*str == '-' && str[1] == 0) {
			return scid::core::NAG_See;
		}
		return 0;
	}

	if (*str == '/') {
		str++;
		if (*str == 0) {
			return scid::core::NAG_Diagonal;
		}
		if (*str == '\\') {
			return scid::core::NAG_WithIdea;
		}
		return 0;
	}

	if (*str == 'R') {
		str++;
		if (*str == 0) {
			return scid::core::NAG_VariousMoves;
		}
		if (*str == 'R') {
			return scid::core::NAG_Comment;
		}
		return 0;
	}

	if (*str == 'z') {
		str++;
		if (*str == 'z') {
			return scid::core::NAG_BlackZugZwang;
		}
		return 0;
	}
	if (*str == 'Z') {
		str++;
		if (*str == 'Z') {
			return scid::core::NAG_ZugZwang;
		}
		return 0;
	}

	if (*str == 'B') {
		str++;
		if (*str == 'B') {
			return scid::core::NAG_BishopPair;
		}
		if (*str == 'b') {
			return scid::core::NAG_OppositeBishops;
		}
		return 0;
	}

	if (*str == 'o') {
		str++;
		if (*str == '-' && str[1] == 'o') {
			return scid::core::NAG_SeparatedPawns;
		}
		if (*str == 'o' && str[1] == 0) {
			return scid::core::NAG_UnitedPawns;
		}
		if (*str == '^' && str[1] == 0) {
			return scid::core::NAG_PassedPawn;
		}
		return 0;
	}

	if (*str == '(') {
		str++;
		if (*str == '_' && str[1] == ')') {
			return scid::core::NAG_BetterIs;
		}
		return 0;
	}

	if (*str == '[') {
		str++;
		if (*str == ']' && str[1] == 0) {
			return scid::core::NAG_OnlyMove;
		}
		if (*str == '+' && str[1] == ']') {
			return scid::core::NAG_SlightCentre;
		}
		if (*str == '+' && str[1] == '+' && str[2] == ']') {
			return scid::core::NAG_Centre;
		}
		return 0;
	}

	if (*str == '_') {
		str++;
		if (*str == '|' && str[1] == '_') {
			return scid::core::NAG_Ending;
		}
		if (*str == '|' && str[1] == 0) {
			return scid::core::NAG_Without;
		}
		return 0;
	}

	if (*str == '|') {
		str++;
		if (*str == '|') {
			return scid::core::NAG_Etc;
		}
		if (*str == '_') {
			return scid::core::NAG_With;
		}
		return 0;
	}

	if (*str == '>') {
		str++;
		if (*str == 0) {
			return scid::core::NAG_SlightKingSide;
		}
		if (*str == '>' && str[1] == 0) {
			return scid::core::NAG_ModerateKingSide;
		}
		if (*str == '>' && str[1] == '>') {
			return scid::core::NAG_KingSide;
		}
		return 0;
	}

	if (*str == '<') {
		str++;
		if (*str == 0) {
			return scid::core::NAG_SlightQueenSide;
		}
		if (*str == '<' && str[1] == 0) {
			return scid::core::NAG_ModerateQueenSide;
		}
		if (*str == '<' && str[1] == '<' && str[2] == 0) {
			return scid::core::NAG_QueenSide;
		}
		if (*str == '=' && str[1] == '>' && str[2] == 0) {
			return scid::core::NAG_File;
		}
		if (*str == '+' && str[1] == '>' && str[2] == 0) {
			return scid::core::NAG_SlightCounterPlay;
		}
		if (*str == '-' && str[1] == '>' && str[2] == 0) {
			return scid::core::NAG_BlackSlightCounterPlay;
		}
		if (*str == '+' && str[1] == '+' && str[2] == '>' &&
		    str[3] == 0) {
			return scid::core::NAG_CounterPlay;
		}
		if (*str == '-' && str[1] == '-' && str[2] == '>' &&
		    str[3] == 0) {
			return scid::core::NAG_BlackCounterPlay;
		}
		if (*str == '+' && str[1] == '+' && str[2] == '+' &&
		    str[3] == '>') {
			return scid::core::NAG_DecisiveCounterPlay;
		}
		if (*str == '-' && str[1] == '-' && str[2] == '-' &&
		    str[3] == '>') {
			return scid::core::NAG_BlackDecisiveCounterPlay;
		}
		return 0;
	}

	if (*str == '~' && *(str + 1) == '=') {
		return scid::core::NAG_Compensation;
	}

	if (*str == '~') {
		return scid::core::NAG_Unclear;
	}

	if (*str == 'x') {
		return scid::core::NAG_WeakPoint;
	}

	if (str[0] == 'N' && str[1] == 0) {
		return scid::core::NAG_Novelty;
	}

	if (str[0] == 'D' && str[1] == 0) {
		return scid::core::NAG_Diagram;
	}
	return 0;
}

} // namespace scid::database

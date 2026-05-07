#include "scidup/database/game_TEMP/legacy_pgn.h"

#include "scidup/database/common.h"
#include "scidup/database/game.h"
#include "scidup/database/misc.h"
#include "naglatex.h"
#include "nagtext.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace scid::database {

int language = 0; // default to english
//  0 = en,
//  1 = fr, 2 = es, 3 = de, 4 = it, 5 = ne, 6 = cz
//  7 = hu, 8 = no, 9 = sw, 10 = ca, 11 = fi, 12 = gr
//  TODO Piece translations for greek
const char* langPieces[] = {"",
                            "PPKRQDRTBFNC",
                            "PPKRQDRTBANC",
                            "PBKKQDRTBLNS",
                            "PPKRQDRTBANC",
                            "PpKKQDRTBLNP",
                            "PPKKQDRVBSNJ",
                            "PGKKQVRBBFNH",
                            "PBKKQDRTBLNS",
                            "PBKKQDRTBLNS",
                            "PPKRQDRTBANC",
                            "PSKKQDRTBLNR",
                            ""};

void transPieces(char* s) {
	if (language == 0)
		return;
	char* ptr = s;
	int i;

	while (*ptr) {
		if (*ptr >= 'A' && *ptr <= 'Z') {
			for (i = 0; i < 12; i += 2) {
				if (*ptr == langPieces[language][i]) {
					*ptr = langPieces[language][i + 1];
					break;
				}
			}
		}
		ptr++;
	}
}

char transPiecesChar(char c) {
	char ret = c;
	if (language == 0)
		return c;
	for (int i = 0; i < 12; i += 2) {
		if (c == langPieces[language][i]) {
			ret = langPieces[language][i + 1];
			break;
		}
	}
	return ret;
}

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
		if (nag == NAG_Diagram) {
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
			return NAG_GoodMove;
		}
		if (*str == '!') {
			return NAG_ExcellentMove;
		}
		if (*str == '?') {
			return NAG_InterestingMove;
		}
		return 0;
	}

	if (*str == '?') {
		str++;
		if (*str == 0) {
			return NAG_PoorMove;
		}
		if (*str == '?') {
			return NAG_Blunder;
		}
		if (*str == '!') {
			return NAG_DubiousMove;
		}
		return 0;
	}

	if (*str == '+') {
		str++;
		if (*str == '=') {
			return NAG_WhiteSlight;
		}
		if (*str == '-' && str[1] == 0) {
			return NAG_WhiteDecisive;
		}
		if (*str == '>') {
			return NAG_WithAttack;
		}
		if (*str == '/' && str[1] == '-') {
			return NAG_WhiteClear;
		}
		if (*str == '/' && str[1] == '=') {
			return NAG_WhiteSlight;
		}
		if (*str == '-' && str[1] == '-') {
			return NAG_WhiteCrushing;
		}
		return 0;
	}

	if (*str == '=') {
		str++;
		if (*str == 0) {
			return NAG_Equal;
		}
		if (*str == '+') {
			return NAG_BlackSlight;
		}
		if (*str == '/' && str[1] == '+') {
			return NAG_BlackSlight;
		}
		if (*str == '/' && str[1] == '&') {
			return NAG_Compensation;
		}
		return 0;
	}

	if (*str == '-') {
		str++;
		if (*str == '+') {
			return NAG_BlackDecisive;
		}
		if (*str == '>') {
			return NAG_WithBlackAttack;
		}
		if (*str == '/' && str[1] == '+') {
			return NAG_BlackClear;
		}
		if (*str == '-' && str[1] == '+') {
			return NAG_BlackCrushing;
		}
		if (*str == '-' && str[1] == 0) {
			return NAG_See;
		}
		return 0;
	}

	if (*str == '/') {
		str++;
		if (*str == 0) {
			return NAG_Diagonal;
		}
		if (*str == '\\') {
			return NAG_WithIdea;
		}
		return 0;
	}

	if (*str == 'R') {
		str++;
		if (*str == 0) {
			return NAG_VariousMoves;
		}
		if (*str == 'R') {
			return NAG_Comment;
		}
		return 0;
	}

	if (*str == 'z') {
		str++;
		if (*str == 'z') {
			return NAG_BlackZugZwang;
		}
		return 0;
	}
	if (*str == 'Z') {
		str++;
		if (*str == 'Z') {
			return NAG_ZugZwang;
		}
		return 0;
	}

	if (*str == 'B') {
		str++;
		if (*str == 'B') {
			return NAG_BishopPair;
		}
		if (*str == 'b') {
			return NAG_OppositeBishops;
		}
		return 0;
	}

	if (*str == 'o') {
		str++;
		if (*str == '-' && str[1] == 'o') {
			return NAG_SeparatedPawns;
		}
		if (*str == 'o' && str[1] == 0) {
			return NAG_UnitedPawns;
		}
		if (*str == '^' && str[1] == 0) {
			return NAG_PassedPawn;
		}
		return 0;
	}

	if (*str == '(') {
		str++;
		if (*str == '_' && str[1] == ')') {
			return NAG_BetterIs;
		}
		return 0;
	}

	if (*str == '[') {
		str++;
		if (*str == ']' && str[1] == 0) {
			return NAG_OnlyMove;
		}
		if (*str == '+' && str[1] == ']') {
			return NAG_SlightCentre;
		}
		if (*str == '+' && str[1] == '+' && str[2] == ']') {
			return NAG_Centre;
		}
		return 0;
	}

	if (*str == '_') {
		str++;
		if (*str == '|' && str[1] == '_') {
			return NAG_Ending;
		}
		if (*str == '|' && str[1] == 0) {
			return NAG_Without;
		}
		return 0;
	}

	if (*str == '|') {
		str++;
		if (*str == '|') {
			return NAG_Etc;
		}
		if (*str == '_') {
			return NAG_With;
		}
		return 0;
	}

	if (*str == '>') {
		str++;
		if (*str == 0) {
			return NAG_SlightKingSide;
		}
		if (*str == '>' && str[1] == 0) {
			return NAG_ModerateKingSide;
		}
		if (*str == '>' && str[1] == '>') {
			return NAG_KingSide;
		}
		return 0;
	}

	if (*str == '<') {
		str++;
		if (*str == 0) {
			return NAG_SlightQueenSide;
		}
		if (*str == '<' && str[1] == 0) {
			return NAG_ModerateQueenSide;
		}
		if (*str == '<' && str[1] == '<' && str[2] == 0) {
			return NAG_QueenSide;
		}
		if (*str == '=' && str[1] == '>' && str[2] == 0) {
			return NAG_File;
		}
		if (*str == '+' && str[1] == '>' && str[2] == 0) {
			return NAG_SlightCounterPlay;
		}
		if (*str == '-' && str[1] == '>' && str[2] == 0) {
			return NAG_BlackSlightCounterPlay;
		}
		if (*str == '+' && str[1] == '+' && str[2] == '>' &&
		    str[3] == 0) {
			return NAG_CounterPlay;
		}
		if (*str == '-' && str[1] == '-' && str[2] == '>' &&
		    str[3] == 0) {
			return NAG_BlackCounterPlay;
		}
		if (*str == '+' && str[1] == '+' && str[2] == '+' &&
		    str[3] == '>') {
			return NAG_DecisiveCounterPlay;
		}
		if (*str == '-' && str[1] == '-' && str[2] == '-' &&
		    str[3] == '>') {
			return NAG_BlackDecisiveCounterPlay;
		}
		return 0;
	}

	if (*str == '~' && *(str + 1) == '=') {
		return NAG_Compensation;
	}

	if (*str == '~') {
		return NAG_Unclear;
	}

	if (*str == 'x') {
		return NAG_WeakPoint;
	}

	if (str[0] == 'N' && str[1] == 0) {
		return NAG_Novelty;
	}

	if (str[0] == 'D' && str[1] == 0) {
		return NAG_Diagram;
	}
	return 0;
}

} // namespace scid::database

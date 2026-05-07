#include "scidup/database/game_TEMP/legacy_pgn.h"

#include "scidup/database/common.h"
#include "scidup/database/game.h"
#include "scidup/database/misc.h"
#include "scidup/core/dstring.h"
#include "scidup/core/notation.h"
#include "movetree.h"
#include "naglatex.h"
#include "nagtext.h"
#include "textbuf.h"

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

bool LegacyGameEncodeOptions::legacyFormatFromString(const char* str,
                                                     gameFormatT* fmt) {
	if (strIsCasePrefix(str, "Plain")) {
		*fmt = PGN_FORMAT_Plain;
	} else if (strIsCasePrefix(str, "PGN")) {
		*fmt = PGN_FORMAT_Plain;
	} else if (strIsCasePrefix(str, "HTML")) {
		*fmt = PGN_FORMAT_HTML;
	} else if (strIsCasePrefix(str, "LaTeX")) {
		*fmt = PGN_FORMAT_LaTeX;
	} else if (strIsCasePrefix(str, "Color")) {
		*fmt = PGN_FORMAT_Color;
	} else {
		return false;
	}
	return true;
}

void Game::ResetPgnStyle(void) {
	PgnStyle = 0;
}

void Game::ResetPgnStyle(uint flag) {
	PgnStyle = flag;
}

uint Game::GetPgnStyle() {
	return PgnStyle;
}

void Game::SetPgnStyle(uint mask, bool setting) {
	if (setting) {
		AddPgnStyle(mask);
	} else {
		RemovePgnStyle(mask);
	}
}

void Game::AddPgnStyle(uint mask) {
	PgnStyle |= mask;
}

void Game::RemovePgnStyle(uint mask) {
	PgnStyle &= ~mask;
}

void Game::SetPgnFormat(gameFormatT gf) {
	PgnFormat = gf;
}

// Compatibility wrapper for encode format parsing.
bool Game::PgnFormatFromString(const char* str, gameFormatT* fmt) {
	return LegacyGameEncodeOptions::legacyFormatFromString(str, fmt);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::SetPgnFormatFromString():
//      Sets the PgnFormat from the provided string.
//      Returns true if the PgnFormat was successfully set.
bool
Game::SetPgnFormatFromString (const char * str)
{
	return LegacyGameEncodeOptions::legacyFormatFromString(str, &PgnFormat);
}

bool Game::IsPlainFormat() {
	return LegacyGameEncodeOptions{PgnStyle, PgnFormat, HtmlStyle}
	    .isPlainFormat();
}

bool Game::IsHtmlFormat() {
	return LegacyGameEncodeOptions{PgnStyle, PgnFormat, HtmlStyle}
	    .isHtmlFormat();
}

bool Game::IsLatexFormat() {
	return LegacyGameEncodeOptions{PgnStyle, PgnFormat, HtmlStyle}
	    .isLatexFormat();
}

bool Game::IsColorFormat() {
	return LegacyGameEncodeOptions{PgnStyle, PgnFormat, HtmlStyle}
	    .isColorFormat();
}

void Game::SetHtmlStyle(uint style) {
	HtmlStyle = style;
}

uint Game::GetHtmlStyle() {
	return HtmlStyle;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// writeComment:
//    Called by WriteMoveList to write a single comment.
static void writeComment(TextBuffer* tb, const char* preStr,
                         const char* comment, const char* postStr,
                         bool colorFormat, uint numMovesPrinted) {
    const char* s = comment;
    if (s[0] != '\0') {

        if (colorFormat) {
            tb->PrintString ("<c_");
            tb->PrintInt (numMovesPrinted);
            tb->PrintChar ('>');
        }

        if (colorFormat) {
            // Translate "<", ">" in comments:
            tb->AddTranslation ('<', "<lt>");
            tb->AddTranslation ('>', "<gt>");
            // S.A any issues ?
            tb->NewlinesToSpaces (0);
            tb->PrintString (s);
            tb->ClearTranslation ('<');
            tb->ClearTranslation ('>');
        } else {
            tb->PrintString (preStr);
            tb->PrintString (s);
            tb->PrintString (postStr);
        }

        if (colorFormat) { tb->PrintString ("</c>"); }
    }
}

struct LegacyGamePgnEncoder {
	Game& game;
	TextBuffer* tb;
	LegacyGameEncodeOptions options;
	uint numMovesPrinted = 1;

	static std::pair<const char*, unsigned>
	encodeToPgnText(Game& game, LegacyGameEncodeOptions options,
	                uint lineWidth, bool newLineAtEnd,
	                bool newLineToSpaces);

	errorT encode();
	errorT writeMoveList(bool printMoveNum, bool inComment);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LegacyGamePgnEncoder::writeMoveList():
//      Write the moves, variations and comments in PGN notation.
//      Recursive; calls itself to write variations.
//
errorT LegacyGamePgnEncoder::writeMoveList(bool printMoveNum, bool inComment) {
    auto& CurrentMove = game.CurrentMove;
    auto& CurrentPos = game.CurrentPos;
    auto& FirstMove = game.FirstMove;
    auto& VarDepth = game.VarDepth;
    auto MoveBackup = [&] { return game.MoveBackup(); };
    auto MoveExitVariation = [&] { return game.MoveExitVariation(); };
    auto MoveForward = [&] { return game.MoveForward(); };
    auto MoveIntoVariation = [&](uint varNumber) {
        return game.MoveIntoVariation(varNumber);
    };

    sanStringT tempTrans;
    const char * preCommentStr = "{";
    const char * postCommentStr = "}";
    const char * startTable = "\n";
    const char * startColumn = "\t";
    const char * nextColumn = "\t";
    const char * endColumn = "\n";
    const char * endTable = "\n";
    bool printDiagrams = false;

    if (options.isHtmlFormat()) {
        preCommentStr = "";
        postCommentStr = "";
        startTable = "<table width=\"50%\">\n";
        startColumn = "<tr align=left>\n  <td width=\"15%\"><b>";
        nextColumn = "</b></td>\n  <td width=\"45%\" align=left><b>";
        endColumn = "</b></td>\n</tr>\n";
        endTable = "</table>\n";
        printDiagrams = true;
    }
    if (options.isLatexFormat()) {
        preCommentStr = "\\begin{nochess}{\\rm ";
        postCommentStr = "}\\end{nochess}";
        startTable = "\n\\begin{tabular}{p{1cm}p{2cm}p{2cm}}\n";
        startColumn = "";
        nextColumn = "&";
        endColumn = "\\\\\n";
        endTable = "\\end{tabular}\n\n";
        printDiagrams = true;
    }
    if (options.isColorFormat()) {
        startTable = "<br>";
        endColumn = "<br>";
    }

    if (options.isHtmlFormat()  &&  VarDepth == 0) { tb->PrintString ("<b>"); }
    if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
        tb->PrintString (startTable);
    }

    if (options.isPlainFormat()  &&  inComment) {
        preCommentStr = "";
        postCommentStr = "";
    }
    moveT * m = CurrentMove;

    // Print null moves:
    if ((options.style & PGN_STYLE_NO_NULL_MOVES) && !inComment &&
            options.isPlainFormat() && m->isNull()) {
        inComment = true;
        tb->PrintString(preCommentStr);
        preCommentStr = "";
        postCommentStr = "";
    }

    std::string strippedComment;
    // If this is a variation and it starts with a comment, print it:
    if ((VarDepth > 0 || CurrentMove->prev == FirstMove) &&
        (options.style & PGN_STYLE_COMMENTS)) {
        const char* comment = CurrentMove->prev->comment.c_str();
        if (*comment && (options.style & PGN_STYLE_STRIP_MARKS)) {
            strippedComment = comment;
            strTrimMarkCodes(strippedComment.data());
            comment = strippedComment.data();
        }
        if (*comment) {
            writeComment(tb, preCommentStr, comment, postCommentStr,
                         options.isColorFormat(), numMovesPrinted);
            tb->PrintSpace();
            if (!VarDepth) {
                tb->ClearTranslation ('\n');
                tb->NewLine();
                if (options.isColorFormat() || options.isLatexFormat()) {
                    tb->NewLine();
                }
            }
        }
    }

    while (CurrentMove->marker != END_MARKER) {
        moveT *m = CurrentMove;
        bool commentLine = false;

        if (m->san[0] == 0) {
            // If there is a next move we can skip the SAN_MATETEST
            CurrentPos->MakeSANString(
                &(m->moveData), m->san,
                (m->next->marker != END_MARKER) ? SAN_CHECKTEST : SAN_MATETEST);
        }

        bool printThisMove = true;
        if (m->isNull()) {
            // Null moves are not printed in LaTeX or HTML:
            if (options.isLatexFormat()  ||  options.isHtmlFormat()) {
                printThisMove = false;
                printMoveNum = true;
            }
            // If Plain PGN format, check whether to convert the
            // null move and remainder of the line to a comment:
            if ((options.style & PGN_STYLE_NO_NULL_MOVES)  &&  options.isPlainFormat()) {
                if (!inComment) {
                    // Enter inComment mode to convert rest of line
                    // to a comment:
                    inComment = true;
                    tb->PrintString(preCommentStr);
                    preCommentStr = "";
                    postCommentStr = "";
                }
                printThisMove = false;
                printMoveNum = true;
            }
        }
        int colWidth = 6;
        numMovesPrinted++;

        if (printThisMove) {
        // Print the move number and following dots if necessary:
        if (options.isColorFormat()) {
            tb->PrintString ("<m_");
            tb->PrintInt (numMovesPrinted);
            tb->PrintChar ('>');
        }
        if (printMoveNum  ||  (CurrentPos->GetToMove() == WHITE)) {
            if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
                tb->PrintString (startColumn);
                char temp [10];
                std::snprintf(temp, sizeof(temp), "%4u.", CurrentPos->GetFullMoveCount());
                tb->PrintString (temp);
                if (CurrentPos->GetToMove() == BLACK) {
                    tb->PauseTranslations();
                    tb->PrintString (nextColumn);
                    tb->PrintString ("...");
                    if (options.isPlainFormat()  ||  options.isColorFormat()) {
                        tb->PrintString ("        ");
                    }
                    tb->ResumeTranslations();
                }
            } else {
            if (options.style & PGN_STYLE_MOVENUM_SPACE) {
                tb->PrintInt(CurrentPos->GetFullMoveCount(), (CurrentPos->GetToMove() == WHITE ? "." : ". ..."));
                } else {
                    tb->PrintInt(CurrentPos->GetFullMoveCount(), (CurrentPos->GetToMove() == WHITE ? "." : "..."));
                }
                if (options.style & PGN_STYLE_MOVENUM_SPACE) {
                    if (options.isLatexFormat()) {
                        tb->PrintChar ('~');
                    } else {
                        tb->PrintChar (' ');
                    }
                }
            }
            printMoveNum = false;
        }

        // Now print the move: only regenerate the SAN string if necessary.

        if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
            tb->PauseTranslations();
            tb->PrintString (nextColumn);
            tb->ResumeTranslations();
        }
        if (options.isColorFormat() && (options.style & PGN_STYLE_UNICODE)) {
            char buf[100];
            char* q = buf;

            for (char const* p = m->san; *p; ++p) {
                ASSERT(q - buf < static_cast<std::ptrdiff_t>(sizeof(buf) - 4));

                switch (*p) {
                    case 'K':    q = std::copy_n("\xe2\x99\x94", 3, q); break;
                    case 'Q':    q = std::copy_n("\xe2\x99\x95", 3, q); break;
                    case 'R':    q = std::copy_n("\xe2\x99\x96", 3, q); break;
                    case 'B':    q = std::copy_n("\xe2\x99\x97", 3, q); break;
                    case 'N':    q = std::copy_n("\xe2\x99\x98", 3, q); break;
                    case 'P':    q = std::copy_n("\xe2\x99\x99", 3, q); break;
                    default:    *q++ = *p; break;
                }

            }
            *q = '\0';
            tb->PrintWord (buf);
        } else {
            // translate pieces
            strcpy(tempTrans, m->san);
            transPieces(tempTrans);
            //tb->PrintWord (m->san);
            tb->PrintWord (tempTrans);
        }
        colWidth -= (int) std::strlen(m->san);
        if (options.isColorFormat()) {
            tb->PrintString ("</m>");
        }
        }

        bool endedColumn = false;

        // Print NAGs and comments if the style indicates:

        if (options.style & PGN_STYLE_COMMENTS) {
            bool printDiagramHere = false;
            if (options.isColorFormat()  &&  m->nagCount > 0) {
                tb->PrintString ("<nag>");
            }
            for (uint i = 0; i < (uint) m->nagCount; i++) {
                char temp[20];
                game_printNag (m->nags[i], temp, options.style & PGN_STYLE_SYMBOLS,
                               options.legacyFormat);

                // Do not print a space before the Nag if it is the
                // first nag and starts with "!" or "?" -- those symbols
                // look better printed next to the move:

                if (i > 0  ||  (temp[0] != '!'  &&  temp[0] != '?')) {
                    tb->PrintSpace();
                    colWidth--;
                }
                if (printDiagrams  &&  m->nags[i] == NAG_Diagram) {
                    printDiagramHere = true;
                }
                tb->PrintWord (temp);
                colWidth -= (int) std::strlen(temp);

            }
            if (options.isColorFormat()  &&  m->nagCount > 0) {
                tb->PrintString ("</nag>");
            }
            tb->PrintSpace();
            colWidth--;
            if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
                if (options.isPlainFormat()  ||  options.isColorFormat()) {
                    while (colWidth-- > 0) { tb->PrintSpace(); }
                }
            }

            if (printDiagramHere) {
                if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
                    if (! endedColumn) {
                        if (CurrentPos->GetToMove() == WHITE) {
                            tb->PauseTranslations ();
                            tb->PrintString (nextColumn);
                            tb->ResumeTranslations ();
                        }
                        tb->PrintString (endColumn);
                        tb->PrintString (endTable);
                        endedColumn = true;
                    }
                }
                if (options.isHtmlFormat()  &&  VarDepth == 0) {
                    tb->PrintString ("</b>");
                }
                if (options.isLatexFormat()) {
                    // The commented-out code below will print diagrams
                    // in variations smaller than game diagrams:
                    //if (VarDepth == 0) {
                    //    tb->PrintString("\n\\font\\Chess=chess20\n");
                    //} else {
                    //    tb->PrintString("\n\\font\\Chess=chess10\n");
                    //}
                    tb->PrintString ("\n\\begin{diagram}\n");
                }
                MoveForward ();
                DString * dstr = new DString;
                if (options.isHtmlFormat()) {
                    CurrentPos->DumpHtmlBoard (dstr, options.htmlStyle, NULL);
                } else {
                    CurrentPos->DumpLatexBoard (dstr);
                }
                MoveBackup ();
                tb->PrintString (dstr->Data());
                delete dstr;
                if (options.isHtmlFormat()  &&  VarDepth == 0) {
                    tb->PrintString ("<b>");
                }
                if (options.isLatexFormat()) {
                    tb->PrintString ("\n\\end{diagram}\n");
                }
                printMoveNum = true;
            }

            const char* comment = m->comment.c_str();
            if (*comment && (options.style & PGN_STYLE_STRIP_MARKS)) {
                strippedComment = m->comment;
                strTrimMarkCodes(strippedComment.data());
                comment = strippedComment.data();
            }
            if (*comment) {
                if (!inComment && options.isPlainFormat()  &&
                    (options.style & PGN_STYLE_NO_NULL_MOVES)) {
                    // If this move has no variations, but the next move
                    // is a null move, enter inComment mode:
                    if (m->next->isNull()  &&
                          ((!(options.style & PGN_STYLE_VARS))  ||
                            (CurrentMove->next->numVariations == 0))) {
                        inComment = true;
                        tb->PrintString(preCommentStr);
                        preCommentStr = "";
                        postCommentStr = "";
                    }
                }

/* Code commented to remove extra lines
                if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
                       if (! endedColumn) {
                           if (CurrentPos->GetToMove() == WHITE) {
                               tb->PauseTranslations ();
                               tb->PrintString (nextColumn);
                               tb->ResumeTranslations ();
                           }
                           tb->PrintString (endColumn);
                           tb->PrintString (endTable);
                           endedColumn = true;
                       }
                }
*/
                if (options.isHtmlFormat()  &&  VarDepth == 0) {
                    tb->PrintString ("</b><dl><dd>");
                }
                if ((options.style & PGN_STYLE_INDENT_COMMENTS) && VarDepth == 0) {
                    if (options.isColorFormat()) {
                        tb->PrintString ("<br><ip1>");
                    } else {
                        tb->SetIndent (tb->GetIndent() + 4); tb->Indent();
                    }
                }

                writeComment(tb, preCommentStr, comment, postCommentStr,
                             options.isColorFormat(), numMovesPrinted);

                if ((options.style & PGN_STYLE_INDENT_COMMENTS) && VarDepth == 0) {
                    if (options.isColorFormat()) {
                        tb->PrintString ("</ip1><br>");
                        commentLine = true;
                    } else {
                        tb->SetIndent (tb->GetIndent() - 4); tb->Indent();
                    }
                } else {
                    tb->PrintSpace();
                }
                if (printDiagrams  &&  strIsPrefix ("#", comment)) {
                    if (options.isLatexFormat()) {
                        tb->PrintString ("\n\\begin{diagram}\n");
                    }
                    MoveForward ();
                    DString * dstr = new DString;
                    if (options.isHtmlFormat()) {
                        CurrentPos->DumpHtmlBoard (dstr, options.htmlStyle, NULL);
                    } else {
                        CurrentPos->DumpLatexBoard (dstr);
                    }
                    MoveBackup ();
                    tb->PrintString (dstr->Data());
                    if (options.isLatexFormat()) {
                        tb->PrintString ("\n\\end{diagram}\n");
                    }
                    delete dstr;
                }
                if (options.isHtmlFormat() && VarDepth == 0) {
                    tb->PrintString ("</dl><b>");
                }
                printMoveNum = true;
            }
        } else {
            tb->PrintSpace();
        }

        // Print any variations if the style indicates:
        if ((options.style & PGN_STYLE_VARS)  &&  (m->numVariations > 0)) {
            if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
                if (! endedColumn) {
                    if (CurrentPos->GetToMove() == WHITE) {
                        tb->PauseTranslations ();
                        tb->PrintString (nextColumn);
                        tb->ResumeTranslations ();
                    }
                    // Doesn't seem wanted!! S.A (see a few lines below)
                    // tb->PrintString (endColumn);
                    tb->PrintString (endTable);
                    endedColumn = true;
                }
            }
            if (options.isColorFormat()  &&  VarDepth == 0) { tb->PrintString ("<var>"); }
            // Doesn't indent first var in column mode properly
            // if including !(options.style & PGN_STYLE_COLUMN) here.
            // But as-is, depth 3 vars don't indent in COLUMN mode (bug)
            if ((options.style & PGN_STYLE_INDENT_VARS) && options.isColorFormat()) {
                if ( !commentLine ) {
                    tb->PrintString ("<br>");
                }
            }
            for (uint i=0; i < m->numVariations; i++) {
                if (options.style & PGN_STYLE_INDENT_VARS) {
                    if (options.isColorFormat()) {
                        if (VarDepth < 19) {
                            char tmp_str[16];
                            std::snprintf(tmp_str, sizeof(tmp_str), "<ip%u>", VarDepth + 1);
                            tb->PrintString(tmp_str);
                        }
                    } else {
                        tb->SetIndent (tb->GetIndent() + 4); tb->Indent();
                    }
                }
                if (options.isHtmlFormat()) {
                    if (VarDepth == 0) { tb->PrintString ("</b><dl><dd>"); }
                }
                if (options.isLatexFormat()  &&  VarDepth == 0) {
                    if (options.style & PGN_STYLE_INDENT_VARS) {
                        tb->PrintLine ("\\begin{variation}");
                    } else {
                        tb->PrintString ("{\\rm ");
                    }
                }
                if (options.isColorFormat()) { tb->PrintString ("<blue>"); }

                // Note tabs in column mode don't work after this VarDepth>1 for some reason
                // this VarDepth check is redundant i think
                if (!options.isLatexFormat()  ||  VarDepth != 0) {
                    tb->PrintChar ('(');
                }

                MoveIntoVariation (i);
                numMovesPrinted++;
                tb->PrintSpace();

                // Recursively print the variation:
                writeMoveList(true, inComment);

                MoveExitVariation();
                if (!options.isLatexFormat()  ||  VarDepth != 0) {
                    tb->PrintChar (')');
                }
                if (options.isColorFormat()) { tb->PrintString ("<blue>"); }
                if (options.isHtmlFormat()) {
                    if (VarDepth == 0) { tb->PrintString ("</dl><b>"); }
                }
                if (options.isLatexFormat()  &&  VarDepth == 0) {
                    if (options.style & PGN_STYLE_INDENT_VARS) {
                        tb->PrintLine ("\\end{variation}");
                    } else {
                        tb->PrintString ("}");
                    }
                }
                if (options.style & PGN_STYLE_INDENT_VARS) {
                    if (options.isColorFormat()) {
                        if (VarDepth < 19) {
                            char tmp_str[16];
                            std::snprintf(tmp_str, sizeof(tmp_str), "</ip%u><br>", VarDepth + 1);
                            tb->PrintString(tmp_str);
                        }
                    } else {
                        tb->SetIndent (tb->GetIndent() - 4); tb->Indent();
                    }
                } else { tb->PrintSpace(); }
                printMoveNum = true;
            }
            if (options.isColorFormat()  &&  VarDepth == 0) {
                tb->PrintString ("</var>");
            }
        }
        if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
            if (endedColumn) { tb->PrintString(startTable); }
            if (!endedColumn  &&  CurrentPos->GetToMove() == BLACK) {
                tb->PrintString (endColumn);
                endedColumn = true;
            }
        }
        MoveForward();
    }
    if (inComment) { tb->PrintString ("}"); }
    if (options.isHtmlFormat()  &&  VarDepth == 0) { tb->PrintString ("</b>"); }
    if ((options.style & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
        tb->PrintString(endTable);
    }
    return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LegacyGamePgnEncoder::encode():
//      Write a game in PGN to a textbuffer.
//
errorT LegacyGamePgnEncoder::encode() {
    auto& BlackElo = game.BlackElo;
    auto& BlackRatingType = game.BlackRatingType;
    auto& Date = game.Date;
    auto& EcoCode = game.EcoCode;
    auto& EventDate = game.EventDate;
    auto& Result = game.Result;
    auto& RoundStr = game.RoundStr;
    auto& ScidFlags = game.ScidFlags;
    auto& SiteStr = game.SiteStr;
    auto& StartPos = game.StartPos;
    auto& WhiteElo = game.WhiteElo;
    auto& WhiteRatingType = game.WhiteRatingType;
    auto& extraTags_ = game.extraTags_;
    auto FindExtraTag = [&](const char* tag) {
        return game.FindExtraTag(tag);
    };
    auto GetBlackStr = [&] { return game.GetBlackStr(); };
    auto GetEventStr = [&] { return game.GetEventStr(); };
    auto GetRoundStr = [&] { return game.GetRoundStr(); };
    auto GetSiteStr = [&] { return game.GetSiteStr(); };
    auto GetWhiteStr = [&] { return game.GetWhiteStr(); };
    auto MoveToStart = [&] { game.MoveToStart(); };

    char temp[256];
    char dateStr [20];
    const char * newline = "\n";
    tb->NewlinesToSpaces (false);
    if (options.isHtmlFormat()) { newline = "<br>\n"; }
    if (options.isLatexFormat()) {
        newline = "\\\\\n";
        tb->AddTranslation ('#', "\\#");
        tb->AddTranslation ('%', "\\%");
        tb->AddTranslation ('&', "\\&");
        tb->AddTranslation ('<', "$<$");
        tb->AddTranslation ('>', "$>$");
        tb->AddTranslation ('_', "\\_");
        // tb->AddTranslation ('[', "$[$");
        // tb->AddTranslation (']', "$]$");
    }
    if (options.isColorFormat()) {
        newline = "<br>";
    }

    if (options.style & PGN_STYLE_COLUMN) {
        options.style |= PGN_STYLE_INDENT_COMMENTS;
        options.style |= PGN_STYLE_INDENT_VARS;
    }

    // First: is there a pre-game comment? If so, print it:
//    if (FirstMove->comment != NULL && (options.style & PGN_STYLE_COMMENTS)
//        &&  ! strIsAllWhitespace (FirstMove->comment)) {
//        tb->AddTranslation ('\n', newline);
//        char * s = FirstMove->comment;
//        if (options.style & PGN_STYLE_STRIP_MARKS) {
//            s = strDuplicate (FirstMove->comment);
//            strTrimMarkCodes (s);
//        }
//        if (options.isColorFormat()) {
//            sprintf (temp, "<c_%u>", numMovesPrinted);
//            tb->PrintString (temp);
//            tb->AddTranslation ('<', "<lt>");
//            tb->AddTranslation ('>', "<gt>");
//            tb->PrintString (s);
//            tb->ClearTranslation ('<');
//            tb->ClearTranslation ('>');
//            tb->PrintLine ("</c>");
//        } else {
//            tb->PrintLine (s);
//        }
//        if (options.style & PGN_STYLE_STRIP_MARKS) { delete[] s; }
//        tb->ClearTranslation ('\n');
//        tb->NewLine();
//    }

    date_DecodeToString (Date, dateStr);
    if (options.isHtmlFormat()) { tb->PrintLine("<p><b>"); }
    if (options.isLatexFormat()) { tb->PrintLine ("{\\bf"); }

//    if (options.isColorFormat()) {
//        tb->AddTranslation ('<', "<lt>");
//        tb->AddTranslation ('>', "<gt>");
//    }

    if (options.style & PGN_STYLE_SHORT_HEADER) {
        // Print tags in short, 3-line format:

        //if (options.isHtmlFormat()) { tb->PrintString ("<font size=+1>"); }
        if (options.isLatexFormat()) { tb->PrintString ("$\\circ$ "); }
        if (options.legacyFormat==PGN_FORMAT_Color) {tb->PrintString ("<tag>"); }
        tb->PrintString (GetWhiteStr());
        if (WhiteElo > 0) {
            std::snprintf(temp, sizeof(temp), "  (%u)", WhiteElo);
            tb->PrintString (temp);
        }
        switch (options.legacyFormat) {
        case PGN_FORMAT_HTML:
            tb->PrintString (" &nbsp;&nbsp; -- &nbsp;&nbsp; ");
            break;
        case PGN_FORMAT_LaTeX:
            tb->PrintString (newline);
            tb->PrintString ("$\\bullet$ ");
            break;
        default:
            tb->PrintString ("   --   ");
            break;
        }
        tb->PrintString (GetBlackStr());
        if (BlackElo > 0) {
            std::snprintf(temp, sizeof(temp), "  (%u)", BlackElo);
            tb->PrintString (temp);
        }
        //if (options.isHtmlFormat()) { tb->PrintString ("</font>"); }
        tb->PrintString (newline);

        tb->PrintString (GetEventStr());
        if (!RoundStr.empty() && RoundStr != "?") {
            tb->PrintString (options.isHtmlFormat() ? " &nbsp;(" : " (");
            tb->PrintString (GetRoundStr());
            tb->PrintString (")");
        }
        tb->PrintString (options.isHtmlFormat() ? "&nbsp;&nbsp; " : "  ");
        if (options.isLatexFormat()) { tb->PrintString (newline); }
        if (!SiteStr.empty() && SiteStr != "?") {
            tb->PrintString (GetSiteStr());
            tb->PrintString (newline);
        }

        // Remove ".??" or ".??.??" from end of dateStr, then print it:
        if (dateStr[4] == '.'  &&  dateStr[5] == '?') { dateStr[4] = 0; }
        if (dateStr[7] == '.'  &&  dateStr[8] == '?') { dateStr[7] = 0; }
        tb->PrintString (dateStr);

        // Print ECO code:
        tb->PrintString (options.isHtmlFormat() ? " &nbsp; &nbsp; " : "  ");
        if (options.isLatexFormat()) { tb->PrintString ("\\hfill "); }
        tb->PrintString (RESULT_LONGSTR[Result]);
        if (EcoCode != 0) {
            tb->PrintString (options.isHtmlFormat() ? " &nbsp; &nbsp; " : "  ");
            if (options.isLatexFormat()) { tb->PrintString ("\\hfill "); }
            scidup::eco::String ecoStr;
            scidup::eco::toExtendedString(EcoCode, ecoStr);
            tb->PrintString (ecoStr);
        }
        auto annotator = FindExtraTag("Annotator");
        if (annotator != NULL) {
            std::snprintf(temp, sizeof(temp), " (%s)", annotator);
            tb->PrintString(temp);
        }

        tb->PrintString (newline);
        if (options.legacyFormat==PGN_FORMAT_Color) {tb->PrintString ("</tag>"); }

        // Print FEN if non-standard start:
        if (StartPos) {
            if (options.isLatexFormat()) {
                tb->PrintString ("\n\\begin{diagram}\n");
                DString dstr;
                StartPos->DumpLatexBoard (&dstr);
                tb->PrintString (dstr.Data());
                tb->PrintString ("\n\\end{diagram}\n");
            } else if (options.isHtmlFormat()) {
                DString dstr;
                StartPos->DumpHtmlBoard (&dstr, options.htmlStyle, NULL);
                tb->PrintString (dstr.Data());
            } else {
	                auto* out = std::copy_n("Position: ", 10, temp);
	                StartPos->PrintFEN(out, sizeof(temp) - 10);
	                std::strcat(temp, newline);
	                tb->PrintString (temp);
            }
        }
    } else {
        // Print tags in standard PGN format, one per line:
        // Note: we want no line-wrapping when printing PGN tags
        // so set it to a huge value for now:
        uint wrapColumn = tb->GetWrapColumn();
        tb->SetWrapColumn (99999);
        if (options.isColorFormat()) { tb->PrintString ("<tag>"); }
        std::snprintf(temp, sizeof(temp), "[Event \"%s\"]%s", GetEventStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Site \"%s\"]%s", GetSiteStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Date \"%s\"]%s", dateStr, newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Round \"%s\"]%s", GetRoundStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[White \"%s\"]%s", GetWhiteStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Black \"%s\"]%s", GetBlackStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Result \"%s\"]%s", RESULT_LONGSTR[Result], newline);
        tb->PrintString (temp);

        // Print all tags, not just the standard seven, if applicable:
            if (options.style & PGN_STYLE_TAGS) {
                if (WhiteElo > 0) {
                std::snprintf(temp, sizeof(temp), "[White%s \"%u\"]%s",
                         ratingTypeNames [WhiteRatingType], WhiteElo, newline);
                tb->PrintString (temp);
                }
                if (BlackElo > 0) {
                std::snprintf(temp, sizeof(temp), "[Black%s \"%u\"]%s",
                         ratingTypeNames [BlackRatingType], BlackElo, newline);
                tb->PrintString (temp);
                }
                if (EcoCode != 0) {
                scidup::eco::String ecoStr;
                scidup::eco::toExtendedString(EcoCode, ecoStr);
                std::snprintf(temp, sizeof(temp), "[ECO \"%s\"]%s", ecoStr, newline);
                tb->PrintString (temp);
                }
                if (EventDate != ZERO_DATE) {
                    char edateStr [20];
                    date_DecodeToString (EventDate, edateStr);
                std::snprintf(temp, sizeof(temp), "[EventDate \"%s\"]%s", edateStr, newline);
                tb->PrintString (temp);
                }

                if (options.style & PGN_STYLE_SCIDFLAGS  &&  *ScidFlags != 0) {
                std::snprintf(temp, sizeof(temp), "[ScidFlags \"%s\"]%s", ScidFlags, newline);
                tb->PrintString (temp);
                }

                // Now print other tags
                for (auto& e : extraTags_) {
                std::snprintf(temp, sizeof(temp), "[%s \"%s\"]%s", e.first.c_str(),
                        e.second.c_str(), newline);
                tb->PrintString(temp);
                }
            }
        // Finally, write the FEN tag if necessary:
        if (StartPos) {
            auto* out = std::copy_n("[FEN \"", 6, temp);
            StartPos->PrintFEN(out, sizeof(temp) - 6);
            auto it_end = std::copy_n("\"]", 2, temp + std::strlen(temp));
            std::strcpy(it_end, newline);
            tb->PrintString (temp);
        }
        if (options.isColorFormat()) { tb->PrintString ("</tag>"); }
        // Now restore the linewrap column:
        tb->SetWrapColumn (wrapColumn);
    }

//    if (options.isColorFormat()) {
//        tb->ClearTranslation ('<');
//        tb->ClearTranslation ('>');
//    }

    if (options.isHtmlFormat()) { tb->PrintLine("</b></p>"); }
    if (options.isLatexFormat()) {
        tb->PrintLine ("}\n\\begin{chess}{\\bf ");
    } else {
        tb->PrintString (newline);
    }

    MoveToStart();

    if (options.isHtmlFormat()) { tb->PrintString ("<p>"); }
    numMovesPrinted = 1;
    writeMoveList(true, false);
    if (options.isHtmlFormat()) { tb->PrintString ("<b>"); }
    if (options.isLatexFormat()) { tb->PrintString ("\n}\\end{chess}\n{\\bf "); }
    if (options.isColorFormat()) { tb->PrintString ("<tag>"); }
    tb->PrintWord (RESULT_LONGSTR [Result]);
    if (options.isLatexFormat()) {
        tb->PrintString ("}\n\\begin{center} \\hrule \\end{center}");
    }
    if (options.isHtmlFormat()) { tb->PrintString ("</b><hr></p>"); }
    if (options.isColorFormat()) { tb->PrintString ("</tag>"); }
    tb->NewLine();

    return OK;
}

std::pair<const char*, unsigned> LegacyGamePgnEncoder::encodeToPgnText(
    Game& game, LegacyGameEncodeOptions options, uint lineWidth,
    bool newLineAtEnd, bool newLineToSpaces) {
    static TextBuffer tbuf;

    auto location = game.currentLocation();
    tbuf.Empty();
    tbuf.SetWrapColumn(lineWidth ? lineWidth : tbuf.GetBufferSize());
    tbuf.NewlinesToSpaces(newLineToSpaces);
    LegacyGamePgnEncoder{game, &tbuf, options}.encode();
    if (newLineAtEnd) {
        tbuf.NewLine();
    }
    game.restoreLocation(location);

    return std::make_pair(tbuf.GetBuffer(), tbuf.GetByteCount());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::WriteToPGN():
//      Print the entire game.
//
std::pair<const char*, unsigned>
Game::WriteToPGN(uint lineWidth, bool NewLineAtEnd, bool newLineToSpaces)
{
    return LegacyGamePgnEncoder::encodeToPgnText(
        *this, {PgnStyle, PgnFormat, HtmlStyle}, lineWidth, NewLineAtEnd,
        newLineToSpaces);
}

} // namespace scid::database

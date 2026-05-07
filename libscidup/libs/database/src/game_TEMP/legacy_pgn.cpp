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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::PgnFormatFromString():
//      Converts a string to a gameFormatT, returning true on success
//      or false on error.
//      The string should be a case-insensitive unique prefix of
//      "plain" (or "pgn"), "HTML", "LaTeX" or "Color".
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

bool
Game::PgnFormatFromString (const char * str, gameFormatT * fmt)
{
    if (strIsCasePrefix (str, "Plain")) {
        *fmt = PGN_FORMAT_Plain;
    } else if (strIsCasePrefix (str, "PGN")) {
        *fmt = PGN_FORMAT_Plain;
    } else if (strIsCasePrefix (str, "HTML")) {
        *fmt = PGN_FORMAT_HTML;
    } else if (strIsCasePrefix (str, "LaTeX")) {
        *fmt = PGN_FORMAT_LaTeX;
    } else if (strIsCasePrefix (str, "Color")) {
        *fmt = PGN_FORMAT_Color;
    } else {
        return false;
    }
    return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::SetPgnFormatFromString():
//      Sets the PgnFormat from the provided string.
//      Returns true if the PgnFormat was successfully set.
bool
Game::SetPgnFormatFromString (const char * str)
{
    return PgnFormatFromString (str, &PgnFormat);
}

bool Game::IsPlainFormat() {
	return PgnFormat == PGN_FORMAT_Plain;
}

bool Game::IsHtmlFormat() {
	return PgnFormat == PGN_FORMAT_HTML;
}

bool Game::IsLatexFormat() {
	return PgnFormat == PGN_FORMAT_LaTeX;
}

bool Game::IsColorFormat() {
	return PgnFormat == PGN_FORMAT_Color;
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
void
Game::WriteComment (TextBuffer * tb, const char * preStr,
              const char * comment, const char * postStr)
{
    const char* s = comment;
    if (s[0] != '\0') {

        if (IsColorFormat()) {
            tb->PrintString ("<c_");
            tb->PrintInt (NumMovesPrinted);
            tb->PrintChar ('>');
        }

        if (IsColorFormat()) {
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

        if (IsColorFormat()) { tb->PrintString ("</c>"); }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::WriteMoveList():
//      Write the moves, variations and comments in PGN notation.
//      Recursive; calls itself to write variations.
//
errorT Game::WriteMoveList(TextBuffer* tb, moveT* oldCurrentMove,
                           bool printMoveNum, bool inComment) {
    sanStringT tempTrans;
    const char * preCommentStr = "{";
    const char * postCommentStr = "}";
    const char * startTable = "\n";
    const char * startColumn = "\t";
    const char * nextColumn = "\t";
    const char * endColumn = "\n";
    const char * endTable = "\n";
    bool printDiagrams = false;

    if (IsHtmlFormat()) {
        preCommentStr = "";
        postCommentStr = "";
        startTable = "<table width=\"50%\">\n";
        startColumn = "<tr align=left>\n  <td width=\"15%\"><b>";
        nextColumn = "</b></td>\n  <td width=\"45%\" align=left><b>";
        endColumn = "</b></td>\n</tr>\n";
        endTable = "</table>\n";
        printDiagrams = true;
    }
    if (IsLatexFormat()) {
        preCommentStr = "\\begin{nochess}{\\rm ";
        postCommentStr = "}\\end{nochess}";
        startTable = "\n\\begin{tabular}{p{1cm}p{2cm}p{2cm}}\n";
        startColumn = "";
        nextColumn = "&";
        endColumn = "\\\\\n";
        endTable = "\\end{tabular}\n\n";
        printDiagrams = true;
    }
    if (IsColorFormat()) {
        startTable = "<br>";
        endColumn = "<br>";
    }

    if (IsHtmlFormat()  &&  VarDepth == 0) { tb->PrintString ("<b>"); }
    if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
        tb->PrintString (startTable);
    }

    if (IsPlainFormat()  &&  inComment) {
        preCommentStr = "";
        postCommentStr = "";
    }
    moveT * m = CurrentMove;

    // Print null moves:
    if ((PgnStyle & PGN_STYLE_NO_NULL_MOVES) && !inComment &&
            IsPlainFormat() && m->isNull()) {
        inComment = true;
        tb->PrintString(preCommentStr);
        preCommentStr = "";
        postCommentStr = "";
    }

    std::string strippedComment;
    // If this is a variation and it starts with a comment, print it:
    if ((VarDepth > 0 || CurrentMove->prev == FirstMove) &&
        (PgnStyle & PGN_STYLE_COMMENTS)) {
        const char* comment = CurrentMove->prev->comment.c_str();
        if (*comment && (PgnStyle & PGN_STYLE_STRIP_MARKS)) {
            strippedComment = comment;
            strTrimMarkCodes(strippedComment.data());
            comment = strippedComment.data();
        }
        if (*comment) {
            WriteComment(tb, preCommentStr, comment, postCommentStr);
            tb->PrintSpace();
            if (!VarDepth) {
                tb->ClearTranslation ('\n');
                tb->NewLine();
                if (IsColorFormat() || IsLatexFormat()) {
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
            if (IsLatexFormat()  ||  IsHtmlFormat()) {
                printThisMove = false;
                printMoveNum = true;
            }
            // If Plain PGN format, check whether to convert the
            // null move and remainder of the line to a comment:
            if ((PgnStyle & PGN_STYLE_NO_NULL_MOVES)  &&  IsPlainFormat()) {
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
        NumMovesPrinted++;

        if (printThisMove) {
        // Print the move number and following dots if necessary:
        if (IsColorFormat()) {
            tb->PrintString ("<m_");
            tb->PrintInt (NumMovesPrinted);
            tb->PrintChar ('>');
        }
        if (printMoveNum  ||  (CurrentPos->GetToMove() == WHITE)) {
            if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
                tb->PrintString (startColumn);
                char temp [10];
                std::snprintf(temp, sizeof(temp), "%4u.", CurrentPos->GetFullMoveCount());
                tb->PrintString (temp);
                if (CurrentPos->GetToMove() == BLACK) {
                    tb->PauseTranslations();
                    tb->PrintString (nextColumn);
                    tb->PrintString ("...");
                    if (IsPlainFormat()  ||  IsColorFormat()) {
                        tb->PrintString ("        ");
                    }
                    tb->ResumeTranslations();
                }
            } else {
            if (PgnStyle & PGN_STYLE_MOVENUM_SPACE) {
                tb->PrintInt(CurrentPos->GetFullMoveCount(), (CurrentPos->GetToMove() == WHITE ? "." : ". ..."));
                } else {
                    tb->PrintInt(CurrentPos->GetFullMoveCount(), (CurrentPos->GetToMove() == WHITE ? "." : "..."));
                }
                if (PgnStyle & PGN_STYLE_MOVENUM_SPACE) {
                    if (IsLatexFormat()) {
                        tb->PrintChar ('~');
                    } else {
                        tb->PrintChar (' ');
                    }
                }
            }
            printMoveNum = false;
        }

        // Now print the move: only regenerate the SAN string if necessary.

        if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
            tb->PauseTranslations();
            tb->PrintString (nextColumn);
            tb->ResumeTranslations();
        }
        if (IsColorFormat() && (PgnStyle & PGN_STYLE_UNICODE)) {
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
        if (IsColorFormat()) {
            tb->PrintString ("</m>");
        }
        }

        bool endedColumn = false;

        // Print NAGs and comments if the style indicates:

        if (PgnStyle & PGN_STYLE_COMMENTS) {
            bool printDiagramHere = false;
            if (IsColorFormat()  &&  m->nagCount > 0) {
                tb->PrintString ("<nag>");
            }
            for (uint i = 0; i < (uint) m->nagCount; i++) {
                char temp[20];
                game_printNag (m->nags[i], temp, PgnStyle & PGN_STYLE_SYMBOLS,
                               PgnFormat);

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
            if (IsColorFormat()  &&  m->nagCount > 0) {
                tb->PrintString ("</nag>");
            }
            tb->PrintSpace();
            colWidth--;
            if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
                if (IsPlainFormat()  ||  IsColorFormat()) {
                    while (colWidth-- > 0) { tb->PrintSpace(); }
                }
            }

            if (printDiagramHere) {
                if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
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
                if (IsHtmlFormat()  &&  VarDepth == 0) {
                    tb->PrintString ("</b>");
                }
                if (IsLatexFormat()) {
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
                if (IsHtmlFormat()) {
                    CurrentPos->DumpHtmlBoard (dstr, HtmlStyle, NULL);
                } else {
                    CurrentPos->DumpLatexBoard (dstr);
                }
                MoveBackup ();
                tb->PrintString (dstr->Data());
                delete dstr;
                if (IsHtmlFormat()  &&  VarDepth == 0) {
                    tb->PrintString ("<b>");
                }
                if (IsLatexFormat()) {
                    tb->PrintString ("\n\\end{diagram}\n");
                }
                printMoveNum = true;
            }

            const char* comment = m->comment.c_str();
            if (*comment && (PgnStyle & PGN_STYLE_STRIP_MARKS)) {
                strippedComment = m->comment;
                strTrimMarkCodes(strippedComment.data());
                comment = strippedComment.data();
            }
            if (*comment) {
                if (!inComment && IsPlainFormat()  &&
                    (PgnStyle & PGN_STYLE_NO_NULL_MOVES)) {
                    // If this move has no variations, but the next move
                    // is a null move, enter inComment mode:
                    if (m->next->isNull()  &&
                          ((!(PgnStyle & PGN_STYLE_VARS))  ||
                            (CurrentMove->next->numVariations == 0))) {
                        inComment = true;
                        tb->PrintString(preCommentStr);
                        preCommentStr = "";
                        postCommentStr = "";
                    }
                }

/* Code commented to remove extra lines
                if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
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
                if (IsHtmlFormat()  &&  VarDepth == 0) {
                    tb->PrintString ("</b><dl><dd>");
                }
                if ((PgnStyle & PGN_STYLE_INDENT_COMMENTS) && VarDepth == 0) {
                    if (IsColorFormat()) {
                        tb->PrintString ("<br><ip1>");
                    } else {
                        tb->SetIndent (tb->GetIndent() + 4); tb->Indent();
                    }
                }

                WriteComment(tb, preCommentStr, comment, postCommentStr);

                if ((PgnStyle & PGN_STYLE_INDENT_COMMENTS) && VarDepth == 0) {
                    if (IsColorFormat()) {
                        tb->PrintString ("</ip1><br>");
                        commentLine = true;
                    } else {
                        tb->SetIndent (tb->GetIndent() - 4); tb->Indent();
                    }
                } else {
                    tb->PrintSpace();
                }
                if (printDiagrams  &&  strIsPrefix ("#", comment)) {
                    if (IsLatexFormat()) {
                        tb->PrintString ("\n\\begin{diagram}\n");
                    }
                    MoveForward ();
                    DString * dstr = new DString;
                    if (IsHtmlFormat()) {
                        CurrentPos->DumpHtmlBoard (dstr, HtmlStyle, NULL);
                    } else {
                        CurrentPos->DumpLatexBoard (dstr);
                    }
                    MoveBackup ();
                    tb->PrintString (dstr->Data());
                    if (IsLatexFormat()) {
                        tb->PrintString ("\n\\end{diagram}\n");
                    }
                    delete dstr;
                }
                if (IsHtmlFormat() && VarDepth == 0) {
                    tb->PrintString ("</dl><b>");
                }
                printMoveNum = true;
            }
        } else {
            tb->PrintSpace();
        }

        // Print any variations if the style indicates:
        if ((PgnStyle & PGN_STYLE_VARS)  &&  (m->numVariations > 0)) {
            if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
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
            if (IsColorFormat()  &&  VarDepth == 0) { tb->PrintString ("<var>"); }
            // Doesn't indent first var in column mode properly
            // if including !(PgnStyle & PGN_STYLE_COLUMN) here.
            // But as-is, depth 3 vars don't indent in COLUMN mode (bug)
            if ((PgnStyle & PGN_STYLE_INDENT_VARS) && IsColorFormat()) {
                if ( !commentLine ) {
                    tb->PrintString ("<br>");
                }
            }
            for (uint i=0; i < m->numVariations; i++) {
                if (PgnStyle & PGN_STYLE_INDENT_VARS) {
                    if (IsColorFormat()) {
                        if (VarDepth < 19) {
                            char tmp_str[16];
                            std::snprintf(tmp_str, sizeof(tmp_str), "<ip%u>", VarDepth + 1);
                            tb->PrintString(tmp_str);
                        }
                    } else {
                        tb->SetIndent (tb->GetIndent() + 4); tb->Indent();
                    }
                }
                if (IsHtmlFormat()) {
                    if (VarDepth == 0) { tb->PrintString ("</b><dl><dd>"); }
                }
                if (IsLatexFormat()  &&  VarDepth == 0) {
                    if (PgnStyle & PGN_STYLE_INDENT_VARS) {
                        tb->PrintLine ("\\begin{variation}");
                    } else {
                        tb->PrintString ("{\\rm ");
                    }
                }
                if (IsColorFormat()) { tb->PrintString ("<blue>"); }

                // Note tabs in column mode don't work after this VarDepth>1 for some reason
                // this VarDepth check is redundant i think
                if (!IsLatexFormat()  ||  VarDepth != 0) {
                    tb->PrintChar ('(');
                }

                MoveIntoVariation (i);
                NumMovesPrinted++;
                tb->PrintSpace();

                // Recursively print the variation:
                WriteMoveList (tb, oldCurrentMove, true, inComment);

                MoveExitVariation();
                if (!IsLatexFormat()  ||  VarDepth != 0) {
                    tb->PrintChar (')');
                }
                if (IsColorFormat()) { tb->PrintString ("<blue>"); }
                if (IsHtmlFormat()) {
                    if (VarDepth == 0) { tb->PrintString ("</dl><b>"); }
                }
                if (IsLatexFormat()  &&  VarDepth == 0) {
                    if (PgnStyle & PGN_STYLE_INDENT_VARS) {
                        tb->PrintLine ("\\end{variation}");
                    } else {
                        tb->PrintString ("}");
                    }
                }
                if (PgnStyle & PGN_STYLE_INDENT_VARS) {
                    if (IsColorFormat()) {
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
            if (IsColorFormat()  &&  VarDepth == 0) {
                tb->PrintString ("</var>");
            }
        }
        if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
            if (endedColumn) { tb->PrintString(startTable); }
            if (!endedColumn  &&  CurrentPos->GetToMove() == BLACK) {
                tb->PrintString (endColumn);
                endedColumn = true;
            }
        }
        MoveForward();
    }
    if (inComment) { tb->PrintString ("}"); }
    if (IsHtmlFormat()  &&  VarDepth == 0) { tb->PrintString ("</b>"); }
    if ((PgnStyle & PGN_STYLE_COLUMN)  &&  VarDepth == 0) {
        tb->PrintString(endTable);
    }
    return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::WritePGN():
//      Write a game in PGN to a textbuffer.
//
errorT Game::WritePGN(TextBuffer* tb) {
    char temp[256];
    char dateStr [20];
    const char * newline = "\n";
    tb->NewlinesToSpaces (false);
    if (IsHtmlFormat()) { newline = "<br>\n"; }
    if (IsLatexFormat()) {
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
    if (IsColorFormat()) {
        newline = "<br>";
    }

    if (PgnStyle & PGN_STYLE_COLUMN) {
        PgnStyle |= PGN_STYLE_INDENT_COMMENTS;
        PgnStyle |= PGN_STYLE_INDENT_VARS;
    }

    // First: is there a pre-game comment? If so, print it:
//    if (FirstMove->comment != NULL && (PgnStyle & PGN_STYLE_COMMENTS)
//        &&  ! strIsAllWhitespace (FirstMove->comment)) {
//        tb->AddTranslation ('\n', newline);
//        char * s = FirstMove->comment;
//        if (PgnStyle & PGN_STYLE_STRIP_MARKS) {
//            s = strDuplicate (FirstMove->comment);
//            strTrimMarkCodes (s);
//        }
//        if (IsColorFormat()) {
//            sprintf (temp, "<c_%u>", NumMovesPrinted);
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
//        if (PgnStyle & PGN_STYLE_STRIP_MARKS) { delete[] s; }
//        tb->ClearTranslation ('\n');
//        tb->NewLine();
//    }

    date_DecodeToString (Date, dateStr);
    if (IsHtmlFormat()) { tb->PrintLine("<p><b>"); }
    if (IsLatexFormat()) { tb->PrintLine ("{\\bf"); }

//    if (IsColorFormat()) {
//        tb->AddTranslation ('<', "<lt>");
//        tb->AddTranslation ('>', "<gt>");
//    }

    if (PgnStyle & PGN_STYLE_SHORT_HEADER) {
        // Print tags in short, 3-line format:

        //if (IsHtmlFormat()) { tb->PrintString ("<font size=+1>"); }
        if (IsLatexFormat()) { tb->PrintString ("$\\circ$ "); }
        if (PgnFormat==PGN_FORMAT_Color) {tb->PrintString ("<tag>"); }
        tb->PrintString (GetWhiteStr());
        if (WhiteElo > 0) {
            std::snprintf(temp, sizeof(temp), "  (%u)", WhiteElo);
            tb->PrintString (temp);
        }
        switch (PgnFormat) {
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
        //if (IsHtmlFormat()) { tb->PrintString ("</font>"); }
        tb->PrintString (newline);

        tb->PrintString (GetEventStr());
        if (!RoundStr.empty() && RoundStr != "?") {
            tb->PrintString (IsHtmlFormat() ? " &nbsp;(" : " (");
            tb->PrintString (GetRoundStr());
            tb->PrintString (")");
        }
        tb->PrintString (IsHtmlFormat() ? "&nbsp;&nbsp; " : "  ");
        if (IsLatexFormat()) { tb->PrintString (newline); }
        if (!SiteStr.empty() && SiteStr != "?") {
            tb->PrintString (GetSiteStr());
            tb->PrintString (newline);
        }

        // Remove ".??" or ".??.??" from end of dateStr, then print it:
        if (dateStr[4] == '.'  &&  dateStr[5] == '?') { dateStr[4] = 0; }
        if (dateStr[7] == '.'  &&  dateStr[8] == '?') { dateStr[7] = 0; }
        tb->PrintString (dateStr);

        // Print ECO code:
        tb->PrintString (IsHtmlFormat() ? " &nbsp; &nbsp; " : "  ");
        if (IsLatexFormat()) { tb->PrintString ("\\hfill "); }
        tb->PrintString (RESULT_LONGSTR[Result]);
        if (EcoCode != 0) {
            tb->PrintString (IsHtmlFormat() ? " &nbsp; &nbsp; " : "  ");
            if (IsLatexFormat()) { tb->PrintString ("\\hfill "); }
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
        if (PgnFormat==PGN_FORMAT_Color) {tb->PrintString ("</tag>"); }

        // Print FEN if non-standard start:
        if (StartPos) {
            if (IsLatexFormat()) {
                tb->PrintString ("\n\\begin{diagram}\n");
                DString dstr;
                StartPos->DumpLatexBoard (&dstr);
                tb->PrintString (dstr.Data());
                tb->PrintString ("\n\\end{diagram}\n");
            } else if (IsHtmlFormat()) {
                DString dstr;
                StartPos->DumpHtmlBoard (&dstr, HtmlStyle, NULL);
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
        if (IsColorFormat()) { tb->PrintString ("<tag>"); }
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
            if (PgnStyle & PGN_STYLE_TAGS) {
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

                if (PgnStyle & PGN_STYLE_SCIDFLAGS  &&  *ScidFlags != 0) {
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
        if (IsColorFormat()) { tb->PrintString ("</tag>"); }
        // Now restore the linewrap column:
        tb->SetWrapColumn (wrapColumn);
    }

//    if (IsColorFormat()) {
//        tb->ClearTranslation ('<');
//        tb->ClearTranslation ('>');
//    }

    if (IsHtmlFormat()) { tb->PrintLine("</b></p>"); }
    if (IsLatexFormat()) {
        tb->PrintLine ("}\n\\begin{chess}{\\bf ");
    } else {
        tb->PrintString (newline);
    }

    MoveToStart();

    if (IsHtmlFormat()) { tb->PrintString ("<p>"); }
    NumMovesPrinted = 1;
    WriteMoveList(tb, CurrentMove, true, false);
    if (IsHtmlFormat()) { tb->PrintString ("<b>"); }
    if (IsLatexFormat()) { tb->PrintString ("\n}\\end{chess}\n{\\bf "); }
    if (IsColorFormat()) { tb->PrintString ("<tag>"); }
    tb->PrintWord (RESULT_LONGSTR [Result]);
    if (IsLatexFormat()) {
        tb->PrintString ("}\n\\begin{center} \\hrule \\end{center}");
    }
    if (IsHtmlFormat()) { tb->PrintString ("</b><hr></p>"); }
    if (IsColorFormat()) { tb->PrintString ("</tag>"); }
    tb->NewLine();

    return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::WriteToPGN():
//      Print the entire game.
//
std::pair<const char*, unsigned>
Game::WriteToPGN(uint lineWidth, bool NewLineAtEnd, bool newLineToSpaces)
{
    static TextBuffer tbuf;

    auto location = currentLocation();
    tbuf.Empty();
    tbuf.SetWrapColumn(lineWidth ? lineWidth : tbuf.GetBufferSize());
    tbuf.NewlinesToSpaces(newLineToSpaces);
    WritePGN(&tbuf);
    if (NewLineAtEnd) tbuf.NewLine();
    restoreLocation(location);
    return std::make_pair(tbuf.GetBuffer(), tbuf.GetByteCount());
}

} // namespace scid::database

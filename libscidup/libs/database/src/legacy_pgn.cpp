#include "nag_format.h"
#include "legacy_pgn.h"
#include "piece_translation.h"
#include "scidup/core/game.h"
#include "scidup/database/common.h"
#include "scidup/database/misc.h"
#include "scidup/core/dstring.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/nags.h"
#include "scidup/core/notation.h"
#include "scidup/eco/code.h"
#include "textbuf.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace scid::database {

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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// writeComment:
//    Called by WriteMoveList to write a single comment.
static void writeComment(TextBuffer* tb, const char* preStr,
                         const char* comment, const char* postStr,
                         bool colorFormat, scid::core::uint numMovesPrinted) {
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

static std::string sanForMove(scid::core::Position& position,
                              scid::core::Move const& move,
                              scid::core::sanFlagT flag) {
    if (!move.san.empty()) {
        return move.san;
    }

    return position.makeSan(move.action, flag);
}

struct LegacyGamePgnEncoder {
	const scid::core::Game& game;
	const char* scidFlags;
	TextBuffer* tb;
	LegacyGameEncodeOptions options;
	scid::core::uint numMovesPrinted = 1;

	static std::pair<const char*, unsigned>
	encodeToPgnText(const scid::core::Game& game, const char* scidFlags,
	                LegacyGameEncodeOptions options, scid::core::uint lineWidth,
	                bool newLineAtEnd,
	                bool newLineToSpaces);

	scid::core::errorT encode();
	scid::core::errorT writeMoveList(bool printMoveNum, bool inComment, scid::core::uint depth,
	                     scid::core::GameCursor& cursor, scid::core::Position position);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LegacyGamePgnEncoder::writeMoveList():
//      Write the moves, variations and comments in PGN notation.
//      Recursive; calls itself to write variations.
//
scid::core::errorT LegacyGamePgnEncoder::writeMoveList(bool printMoveNum, bool inComment,
                                           scid::core::uint depth,
                                           scid::core::GameCursor& cursor,
                                           scid::core::Position position) {
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

    if (options.isHtmlFormat()  &&  depth == 0) { tb->PrintString ("<b>"); }
    if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
        tb->PrintString (startTable);
    }

    if (options.isPlainFormat()  &&  inComment) {
        preCommentStr = "";
        postCommentStr = "";
    }
    const auto* initialCoreMove = cursor.nextMove();

    // Print null moves:
    if ((options.style & PGN_STYLE_NO_NULL_MOVES) && !inComment &&
            options.isPlainFormat() && initialCoreMove &&
            initialCoreMove->action.isNull()) {
        inComment = true;
        tb->PrintString(preCommentStr);
        preCommentStr = "";
        postCommentStr = "";
    }

    std::string strippedComment;
    std::string lineStartCommentStorage;
    std::string_view lineStartComment;
    if (depth > 0) {
        if (auto variation = cursor.currentVariation()) {
            lineStartComment = variation->initialComment;
        }
    } else {
        lineStartComment = game.initialComment();
    }

    // If this is a variation and it starts with a comment, print it:
    if (!lineStartComment.empty() && (options.style & PGN_STYLE_COMMENTS)) {
        lineStartCommentStorage.assign(lineStartComment);
        const char* comment = lineStartCommentStorage.c_str();
        if (*comment && (options.style & PGN_STYLE_STRIP_MARKS)) {
            strippedComment = comment;
            strTrimMarkCodes(strippedComment.data());
            comment = strippedComment.data();
        }
        if (*comment) {
            writeComment(tb, preCommentStr, comment, postCommentStr,
                         options.isColorFormat(), numMovesPrinted);
            tb->PrintSpace();
            if (!depth) {
                tb->ClearTranslation ('\n');
                tb->NewLine();
                if (options.isColorFormat() || options.isLatexFormat()) {
                    tb->NewLine();
                }
            }
        }
    }

    while (!cursor.isAtLineEnd()) {
        const auto* coreMove = cursor.nextMove();
        if (!coreMove) {
            return scid::core::ERROR;
        }
        auto afterCoreMove = cursor;
        if (!afterCoreMove.next()) {
            return scid::core::ERROR;
        }
        const bool isNullMove = coreMove->action.isNull();
        const bool isLastMoveInLine = afterCoreMove.isAtLineEnd();
        const auto* nextCoreMove = afterCoreMove.nextMove();
        bool commentLine = false;

        auto positionAfterMove = position;
        if (positionAfterMove.applyMove(coreMove->action) != scid::core::OK) {
            return scid::core::ERROR;
        }
        const auto san = sanForMove(
            position, *coreMove,
            !isLastMoveInLine ? scid::core::SAN_CHECKTEST : scid::core::SAN_MATETEST);

        bool printThisMove = true;
        if (isNullMove) {
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
        if (printMoveNum  ||  (position.GetToMove() == scid::core::WHITE)) {
            if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
                tb->PrintString (startColumn);
                char temp [10];
                std::snprintf(temp, sizeof(temp), "%4u.", position.GetFullMoveCount());
                tb->PrintString (temp);
                if (position.GetToMove() == scid::core::BLACK) {
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
                tb->PrintInt(position.GetFullMoveCount(), (position.GetToMove() == scid::core::WHITE ? "." : ". ..."));
                } else {
                    tb->PrintInt(position.GetFullMoveCount(), (position.GetToMove() == scid::core::WHITE ? "." : "..."));
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

        if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
            tb->PauseTranslations();
            tb->PrintString (nextColumn);
            tb->ResumeTranslations();
        }
        if (options.isColorFormat() && (options.style & PGN_STYLE_UNICODE)) {
            char buf[100];
            char* q = buf;

            for (char const* p = san.c_str(); *p; ++p) {
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
            auto translatedSan = san;
            translatedSan.push_back('\0');
            transPieces(translatedSan.data());
            tb->PrintWord (translatedSan.data());
        }
        colWidth -= (int) san.size();
        if (options.isColorFormat()) {
            tb->PrintString ("</m>");
        }
        }

        bool endedColumn = false;

        // Print NAGs and comments if the style indicates:

        if (options.style & PGN_STYLE_COMMENTS) {
            bool printDiagramHere = false;
            auto const& nags = coreMove->metadata.nags;
            if (options.isColorFormat()  &&  !nags.empty()) {
                tb->PrintString ("<nag>");
            }
            for (scid::core::uint i = 0; i < (scid::core::uint) nags.size(); i++) {
                char temp[20];
                game_printNag (nags[i], temp, options.style & PGN_STYLE_SYMBOLS,
                               options.legacyFormat);

                // Do not print a space before the Nag if it is the
                // first nag and starts with "!" or "?" -- those symbols
                // look better printed next to the move:

                if (i > 0  ||  (temp[0] != '!'  &&  temp[0] != '?')) {
                    tb->PrintSpace();
                    colWidth--;
                }
                if (printDiagrams  &&  nags[i] == scid::core::NAG_Diagram) {
                    printDiagramHere = true;
                }
                tb->PrintWord (temp);
                colWidth -= (int) std::strlen(temp);

            }
            if (options.isColorFormat()  &&  !nags.empty()) {
                tb->PrintString ("</nag>");
            }
            tb->PrintSpace();
            colWidth--;
            if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
                if (options.isPlainFormat()  ||  options.isColorFormat()) {
                    while (colWidth-- > 0) { tb->PrintSpace(); }
                }
            }

            if (printDiagramHere) {
                if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
                    if (! endedColumn) {
                        if (position.GetToMove() == scid::core::WHITE) {
                            tb->PauseTranslations ();
                            tb->PrintString (nextColumn);
                            tb->ResumeTranslations ();
                        }
                        tb->PrintString (endColumn);
                        tb->PrintString (endTable);
                        endedColumn = true;
                    }
                }
                if (options.isHtmlFormat()  &&  depth == 0) {
                    tb->PrintString ("</b>");
                }
                if (options.isLatexFormat()) {
                    // The commented-out code below will print diagrams
                    // in variations smaller than game diagrams:
                    //if (depth == 0) {
                    //    tb->PrintString("\n\\font\\Chess=chess20\n");
                    //} else {
                    //    tb->PrintString("\n\\font\\Chess=chess10\n");
                    //}
                    tb->PrintString ("\n\\begin{diagram}\n");
                }
                scid::core::DString * dstr = new scid::core::DString;
                if (options.isHtmlFormat()) {
                    positionAfterMove.DumpHtmlBoard (dstr, options.htmlStyle, NULL);
                } else {
                    positionAfterMove.DumpLatexBoard (dstr);
                }
                tb->PrintString (dstr->Data());
                delete dstr;
                if (options.isHtmlFormat()  &&  depth == 0) {
                    tb->PrintString ("<b>");
                }
                if (options.isLatexFormat()) {
                    tb->PrintString ("\n\\end{diagram}\n");
                }
                printMoveNum = true;
            }

            const auto& moveComment = coreMove->metadata.comment;
            const char* comment = moveComment.c_str();
            if (*comment && (options.style & PGN_STYLE_STRIP_MARKS)) {
                strippedComment = moveComment;
                strTrimMarkCodes(strippedComment.data());
                comment = strippedComment.data();
            }
            if (*comment) {
                if (!inComment && options.isPlainFormat()  &&
                    (options.style & PGN_STYLE_NO_NULL_MOVES)) {
                    // If this move has no variations, but the next move
                    // is a null move, enter inComment mode:
                    if (nextCoreMove && nextCoreMove->action.isNull() &&
                          ((!(options.style & PGN_STYLE_VARS))  ||
                            nextCoreMove->childVariations.empty())) {
                        inComment = true;
                        tb->PrintString(preCommentStr);
                        preCommentStr = "";
                        postCommentStr = "";
                    }
                }

/* Code commented to remove extra lines
                if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
                       if (! endedColumn) {
                           if (position.GetToMove() == scid::core::WHITE) {
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
                if (options.isHtmlFormat()  &&  depth == 0) {
                    tb->PrintString ("</b><dl><dd>");
                }
                if ((options.style & PGN_STYLE_INDENT_COMMENTS) && depth == 0) {
                    if (options.isColorFormat()) {
                        tb->PrintString ("<br><ip1>");
                    } else {
                        tb->SetIndent (tb->GetIndent() + 4); tb->Indent();
                    }
                }

                writeComment(tb, preCommentStr, comment, postCommentStr,
                             options.isColorFormat(), numMovesPrinted);

                if ((options.style & PGN_STYLE_INDENT_COMMENTS) && depth == 0) {
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
                    scid::core::DString * dstr = new scid::core::DString;
                    if (options.isHtmlFormat()) {
                        positionAfterMove.DumpHtmlBoard (dstr, options.htmlStyle, NULL);
                    } else {
                        positionAfterMove.DumpLatexBoard (dstr);
                    }
                    tb->PrintString (dstr->Data());
                    if (options.isLatexFormat()) {
                        tb->PrintString ("\n\\end{diagram}\n");
                    }
                    delete dstr;
                }
                if (options.isHtmlFormat() && depth == 0) {
                    tb->PrintString ("</dl><b>");
                }
                printMoveNum = true;
            }
        } else {
            tb->PrintSpace();
        }

        // Print any variations if the style indicates:
        const auto variationCount =
            static_cast<scid::core::uint>(coreMove->childVariations.size());
        if ((options.style & PGN_STYLE_VARS)  &&  (variationCount > 0)) {
            if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
                if (! endedColumn) {
                    if (position.GetToMove() == scid::core::WHITE) {
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
            if (options.isColorFormat()  &&  depth == 0) { tb->PrintString ("<var>"); }
            // Doesn't indent first var in column mode properly
            // if including !(options.style & PGN_STYLE_COLUMN) here.
            // But as-is, depth 3 vars don't indent in COLUMN mode (bug)
            if ((options.style & PGN_STYLE_INDENT_VARS) && options.isColorFormat()) {
                if ( !commentLine ) {
                    tb->PrintString ("<br>");
                }
            }
            for (scid::core::uint i=0; i < variationCount; i++) {
                if (options.style & PGN_STYLE_INDENT_VARS) {
                    if (options.isColorFormat()) {
                        if (depth < 19) {
                            char tmp_str[16];
                            std::snprintf(tmp_str, sizeof(tmp_str), "<ip%u>", depth + 1);
                            tb->PrintString(tmp_str);
                        }
                    } else {
                        tb->SetIndent (tb->GetIndent() + 4); tb->Indent();
                    }
                }
                if (options.isHtmlFormat()) {
                    if (depth == 0) { tb->PrintString ("</b><dl><dd>"); }
                }
                if (options.isLatexFormat()  &&  depth == 0) {
                    if (options.style & PGN_STYLE_INDENT_VARS) {
                        tb->PrintLine ("\\begin{variation}");
                    } else {
                        tb->PrintString ("{\\rm ");
                    }
                }
                if (options.isColorFormat()) { tb->PrintString ("<blue>"); }

                // Note tabs in column mode don't work after depth > 1 for some reason.
                // This depth check is redundant, I think.
                if (!options.isLatexFormat()  ||  depth != 0) {
                    tb->PrintChar ('(');
                }

                if (!cursor.enterVariation(i)) {
                    return scid::core::ERROR;
                }
                numMovesPrinted++;
                tb->PrintSpace();

                // Recursively print the variation:
                const auto err =
                    writeMoveList(true, inComment, depth + 1, cursor, position);

                if (!cursor.exitVariation()) {
                    return scid::core::ERROR;
                }
                if (err != scid::core::OK) {
                    return err;
                }
                if (!options.isLatexFormat()  ||  depth != 0) {
                    tb->PrintChar (')');
                }
                if (options.isColorFormat()) { tb->PrintString ("<blue>"); }
                if (options.isHtmlFormat()) {
                    if (depth == 0) { tb->PrintString ("</dl><b>"); }
                }
                if (options.isLatexFormat()  &&  depth == 0) {
                    if (options.style & PGN_STYLE_INDENT_VARS) {
                        tb->PrintLine ("\\end{variation}");
                    } else {
                        tb->PrintString ("}");
                    }
                }
                if (options.style & PGN_STYLE_INDENT_VARS) {
                    if (options.isColorFormat()) {
                        if (depth < 19) {
                            char tmp_str[16];
                            std::snprintf(tmp_str, sizeof(tmp_str), "</ip%u><br>", depth + 1);
                            tb->PrintString(tmp_str);
                        }
                    } else {
                        tb->SetIndent (tb->GetIndent() - 4); tb->Indent();
                    }
                } else { tb->PrintSpace(); }
                printMoveNum = true;
            }
            if (options.isColorFormat()  &&  depth == 0) {
                tb->PrintString ("</var>");
            }
        }
        if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
            if (endedColumn) { tb->PrintString(startTable); }
            if (!endedColumn  &&  position.GetToMove() == scid::core::BLACK) {
                tb->PrintString (endColumn);
                endedColumn = true;
            }
        }
        if (!cursor.next()) {
            return scid::core::ERROR;
        }
        position = positionAfterMove;
    }
    if (inComment) { tb->PrintString ("}"); }
    if (options.isHtmlFormat()  &&  depth == 0) { tb->PrintString ("</b>"); }
    if ((options.style & PGN_STYLE_COLUMN)  &&  depth == 0) {
        tb->PrintString(endTable);
    }
    return scid::core::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LegacyGamePgnEncoder::encode():
//      Write a game in PGN to a textbuffer.
//
scid::core::errorT LegacyGamePgnEncoder::encode() {
    auto const& coreGame = game;
    const auto& BlackElo = coreGame.black().rating.value;
    const auto& BlackRatingType = coreGame.black().rating.type;
    const auto& Date = coreGame.date();
    const auto EcoCode = scidup::eco::fromString(coreGame.eco().c_str());
    const auto& EventDate = coreGame.eventDate();
    const auto& Result = coreGame.result();
    const auto& RoundStr = coreGame.round();
    const auto* scidFlags = this->scidFlags ? this->scidFlags : "";
    const auto& SiteStr = coreGame.site();
    auto* startPos = coreGame.startPosition();
    const auto& WhiteElo = coreGame.white().rating.value;
    const auto& WhiteRatingType = coreGame.white().rating.type;
    const auto& extraTags_ = coreGame.extraTags();
    auto findExtraTag = [&](const char* tag) {
        auto value = coreGame.findExtraTag(tag);
        return value ? value->c_str() : nullptr;
    };
    auto blackStr = [&] { return coreGame.black().name.c_str(); };
    auto eventStr = [&] { return coreGame.event().c_str(); };
    auto roundStr = [&] { return coreGame.round().c_str(); };
    auto siteStr = [&] { return coreGame.site().c_str(); };
    auto whiteStr = [&] { return coreGame.white().name.c_str(); };
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

    scid::core::date_DecodeToString (Date, dateStr);
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
        tb->PrintString (whiteStr());
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
        tb->PrintString (blackStr());
        if (BlackElo > 0) {
            std::snprintf(temp, sizeof(temp), "  (%u)", BlackElo);
            tb->PrintString (temp);
        }
        //if (options.isHtmlFormat()) { tb->PrintString ("</font>"); }
        tb->PrintString (newline);

        tb->PrintString (eventStr());
        if (!RoundStr.empty() && RoundStr != "?") {
            tb->PrintString (options.isHtmlFormat() ? " &nbsp;(" : " (");
            tb->PrintString (roundStr());
            tb->PrintString (")");
        }
        tb->PrintString (options.isHtmlFormat() ? "&nbsp;&nbsp; " : "  ");
        if (options.isLatexFormat()) { tb->PrintString (newline); }
        if (!SiteStr.empty() && SiteStr != "?") {
            tb->PrintString (siteStr());
            tb->PrintString (newline);
        }

        // Remove ".??" or ".??.??" from end of dateStr, then print it:
        if (dateStr[4] == '.'  &&  dateStr[5] == '?') { dateStr[4] = 0; }
        if (dateStr[7] == '.'  &&  dateStr[8] == '?') { dateStr[7] = 0; }
        tb->PrintString (dateStr);

        // Print ECO code:
        tb->PrintString (options.isHtmlFormat() ? " &nbsp; &nbsp; " : "  ");
        if (options.isLatexFormat()) { tb->PrintString ("\\hfill "); }
        tb->PrintString (scid::core::RESULT_LONGSTR[Result]);
        if (EcoCode != 0) {
            tb->PrintString (options.isHtmlFormat() ? " &nbsp; &nbsp; " : "  ");
            if (options.isLatexFormat()) { tb->PrintString ("\\hfill "); }
            scidup::eco::String ecoStr;
            scidup::eco::toExtendedString(EcoCode, ecoStr);
            tb->PrintString (ecoStr);
        }
        auto annotator = findExtraTag("Annotator");
        if (annotator != NULL) {
            std::snprintf(temp, sizeof(temp), " (%s)", annotator);
            tb->PrintString(temp);
        }

        tb->PrintString (newline);
        if (options.legacyFormat==PGN_FORMAT_Color) {tb->PrintString ("</tag>"); }

        // Print FEN if non-standard start:
        if (startPos) {
            if (options.isLatexFormat()) {
                tb->PrintString ("\n\\begin{diagram}\n");
                scid::core::DString dstr;
                scid::core::Position diagramPosition = *startPos;
                diagramPosition.DumpLatexBoard (&dstr);
                tb->PrintString (dstr.Data());
                tb->PrintString ("\n\\end{diagram}\n");
            } else if (options.isHtmlFormat()) {
                scid::core::DString dstr;
                scid::core::Position diagramPosition = *startPos;
                diagramPosition.DumpHtmlBoard (&dstr, options.htmlStyle, NULL);
                tb->PrintString (dstr.Data());
            } else {
	                auto* out = std::copy_n("scid::core::Position: ", 10, temp);
	                startPos->PrintFEN(out, sizeof(temp) - 10);
	                std::strcat(temp, newline);
	                tb->PrintString (temp);
            }
        }
    } else {
        // Print tags in standard PGN format, one per line:
        // Note: we want no line-wrapping when printing PGN tags
        // so set it to a huge value for now:
        scid::core::uint wrapColumn = tb->GetWrapColumn();
        tb->SetWrapColumn (99999);
        if (options.isColorFormat()) { tb->PrintString ("<tag>"); }
        std::snprintf(temp, sizeof(temp), "[Event \"%s\"]%s", eventStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Site \"%s\"]%s", siteStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Date \"%s\"]%s", dateStr, newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Round \"%s\"]%s", roundStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[White \"%s\"]%s", whiteStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Black \"%s\"]%s", blackStr(), newline);
        tb->PrintString (temp);
        std::snprintf(temp, sizeof(temp), "[Result \"%s\"]%s", scid::core::RESULT_LONGSTR[Result], newline);
        tb->PrintString (temp);

        // Print all tags, not just the standard seven, if applicable:
            if (options.style & PGN_STYLE_TAGS) {
                if (WhiteElo > 0) {
                std::snprintf(temp, sizeof(temp), "[White%s \"%u\"]%s",
                         scid::core::ratingTypeNames [WhiteRatingType], WhiteElo, newline);
                tb->PrintString (temp);
                }
                if (BlackElo > 0) {
                std::snprintf(temp, sizeof(temp), "[Black%s \"%u\"]%s",
                         scid::core::ratingTypeNames [BlackRatingType], BlackElo, newline);
                tb->PrintString (temp);
                }
                if (EcoCode != 0) {
                scidup::eco::String ecoStr;
                scidup::eco::toExtendedString(EcoCode, ecoStr);
                std::snprintf(temp, sizeof(temp), "[ECO \"%s\"]%s", ecoStr, newline);
                tb->PrintString (temp);
                }
                if (EventDate != scid::core::ZERO_DATE) {
                    char edateStr [20];
                    scid::core::date_DecodeToString (EventDate, edateStr);
                std::snprintf(temp, sizeof(temp), "[EventDate \"%s\"]%s", edateStr, newline);
                tb->PrintString (temp);
                }

                if (options.style & PGN_STYLE_SCIDFLAGS  &&  *scidFlags != 0) {
                std::snprintf(temp, sizeof(temp), "[ScidFlags \"%s\"]%s", scidFlags, newline);
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
        if (startPos) {
            auto* out = std::copy_n("[FEN \"", 6, temp);
            startPos->PrintFEN(out, sizeof(temp) - 6);
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

    if (options.isHtmlFormat()) { tb->PrintString ("<p>"); }
    numMovesPrinted = 1;
    scid::core::GameCursor cursor(coreGame);
    auto position = startPos ? *startPos : scid::core::Position::getStdStart();
    if (auto err = writeMoveList(true, false, 0, cursor, position)) {
        return err;
    }
    if (options.isHtmlFormat()) { tb->PrintString ("<b>"); }
    if (options.isLatexFormat()) { tb->PrintString ("\n}\\end{chess}\n{\\bf "); }
    if (options.isColorFormat()) { tb->PrintString ("<tag>"); }
    tb->PrintWord (scid::core::RESULT_LONGSTR [Result]);
    if (options.isLatexFormat()) {
        tb->PrintString ("}\n\\begin{center} \\hrule \\end{center}");
    }
    if (options.isHtmlFormat()) { tb->PrintString ("</b><hr></p>"); }
    if (options.isColorFormat()) { tb->PrintString ("</tag>"); }
    tb->NewLine();

    return scid::core::OK;
}

std::pair<const char*, unsigned> LegacyGamePgnEncoder::encodeToPgnText(
    const scid::core::Game& game, const char* scidFlags,
    LegacyGameEncodeOptions options, scid::core::uint lineWidth, bool newLineAtEnd,
    bool newLineToSpaces) {
    static TextBuffer tbuf;

    tbuf.Empty();
    tbuf.SetWrapColumn(lineWidth ? lineWidth : tbuf.GetBufferSize());
    tbuf.NewlinesToSpaces(newLineToSpaces);
    LegacyGamePgnEncoder{game, scidFlags, &tbuf, options}.encode();
    if (newLineAtEnd) {
        tbuf.NewLine();
    }

    return std::make_pair(tbuf.GetBuffer(), tbuf.GetByteCount());
}

std::pair<const char*, unsigned> legacy_pgn::encode(
    const scid::core::Game& game, const char* scidFlags,
    LegacyGameEncodeOptions options, unsigned lineWidth, bool newLineAtEnd,
    bool newLineToSpaces) {
    return LegacyGamePgnEncoder::encodeToPgnText(
        game, scidFlags, options, lineWidth, newLineAtEnd, newLineToSpaces);
}

} // namespace scid::database

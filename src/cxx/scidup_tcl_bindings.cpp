//////////////////////////////////////////////////////////////////////
//
//  FILE:       scidup_tcl_bindings.cpp
//              ScidUp Tcl/Tk bindings
//
//  Part of:    Scid (Shane's Chess Information Database)
//
//  Notice:     Copyright (c) 1999-2004 Shane Hudson.  All rights reserved.
//              Copyright (c) 2006-2007 Pascal Georges
//              Copyright (c) 2013-2014 Benini Fulvio
//
//  Scid is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation.
//
//  Scid is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Scid.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////



#include "crosstab.h"
#include "scidup/core/dstring.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_cursor.h"
#include "scidup/core/nags.h"
#include "scidup/core/notation.h"
#include "scidup/core/pgn/traversal.h"
#include "engine.h"
#include "scidup/database/game_id.h"
#include "scidup/database/game.h"
#include "optable.h"
#include "scidup/eco/book.h"
#include "game_search.h"
#include "game_storage.h"
#include "legacy_pgn.h"
#include "nag_format.h"
#include "piece_translation.h"
#include "polyglot.h"
#include "scidup/core/position.h"
#include "scidup/database/pgnparse.h"
#include "scidup/database/scidbase.h"
#include "scidup_app_editor.h"
#include "scidup_app_tree.h"
#include "scidup/database/searchpos.h"
#include <scidup/spelling/spelling.h>
#include "timer.h"
#include "dbasepool.h"
#include "ui.h"
#include "scidup_release.h"
#include <algorithm>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

//TODO: delete
#include "scidup_tcl_bindings.h"

#ifdef _WIN32
// Provide Tcl_ConsolePanic for embedding applications on Windows.
extern "C" TCL_NORETURN void Tcl_ConsolePanic(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputc('\n', stderr);
	fflush(stderr);
	abort();
}
#endif


//TODO: delete
extern scid::database::scidBaseT* db;
const int MAX_BASES = 9;
/////////////////


static scid::database::Game * scratchGame = NULL;      // "scratch" game for searches, etc.
static std::unique_ptr<scidup::eco::Book> ecoBook; // eco classification book.
static std::unique_ptr<scidup::spelling::SpellChecker> spellChk; // Name correction.
static OpTable * reports[2] = {NULL, NULL};

void scid_Exit(void*) {
	DBasePool::closeAll();
	if (scratchGame != NULL) delete scratchGame;
	spellChk.reset();
	for (size_t i = 0, n = sizeof(reports) / sizeof(reports[0]); i < n; i++) {
		if (reports[i] != NULL) delete reports[i];
	}
}

/*! \mainpage
 * Scid is an open source software released under the GPL licence.
 * The core database library is written in c++ and the GUI uses
 * <a href="http://core.tcl.tk"> the tcl/tk framework</a>.
 *
 * \section Tests
 * Link to <a href="../gcov/index.html">code coverage</a>
 */
int main(int argc, char* argv[]) {
	scratchGame = new scid::database::Game;
	DBasePool::init();

	return UI_Main(argc, argv, scid_Exit);
}





//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Global variables:
static const char * reportTypeName[2] = { "opening", "player" };
static const scid::database::uint REPORT_OPENING = 0;
static const scid::database::uint REPORT_PLAYER = 1;

static char decimalPointChar = '.';
static scid::database::uint htmlDiagStyle = 0;

static std::pair<std::string, std::string>
previousClockComments(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return {};

	auto moves = cursor.movesToCursor();
	std::pair<std::string, std::string> comments;
	if (moves.size() >= 2)
		comments.first = moves[moves.size() - 2]->metadata.comment;
	if (moves.size() >= 3)
		comments.second = moves[moves.size() - 3]->metadata.comment;
	return comments;
}

static std::vector<std::uint8_t>
previousMoveNags(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return {};

	auto move = cursor.previousMove();
	return move ? move->metadata.nags : std::vector<std::uint8_t>{};
}

static std::vector<std::uint8_t>
nextMoveNags(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return {};

	auto move = cursor.nextMove();
	return move ? move->metadata.nags : std::vector<std::uint8_t>{};
}

static std::string currentMoveComment(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return {};

	if (auto move = cursor.previousMove())
		return move->metadata.comment;
	if (auto variation = cursor.currentVariation())
		return variation->initialComment;
	return std::string(game.coreGame().movetext().initialComment);
}

static scid::database::uint variationCount(
    const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return 0;
	return static_cast<scid::database::uint>(cursor.variationCount());
}

static scid::database::uint variationLevel(
    const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return 0;
	return static_cast<scid::database::uint>(cursor.variationDepth());
}

static scid::database::uint variationNumber(
    const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return 0;
	return static_cast<scid::database::uint>(cursor.variationIndex());
}

static bool isAtVariationStart(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	return cursor.restore(game.coreLocation()) && cursor.isAtVariationStart();
}

static bool isAtVariationEnd(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	return cursor.restore(game.coreLocation()) && cursor.isAtVariationEnd();
}

static bool isAtStart(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	return cursor.restore(game.coreLocation()) && cursor.isAtGameStart();
}

static bool isAtEnd(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	return cursor.restore(game.coreLocation()) && cursor.isAtGameEnd();
}

static bool isAtEmptyVariation(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	return cursor.restore(game.coreLocation()) && cursor.isAtEmptyVariation();
}

static scid::database::uint currentPly(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return 0;
	return static_cast<scid::database::uint>(cursor.ply());
}

static std::optional<scid::database::Position>
currentPosition(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return std::nullopt;
	return cursor.currentPosition();
}

static std::optional<scid::database::simpleMoveT>
currentMove(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return std::nullopt;

	auto position = cursor.currentPosition();
	auto move = cursor.nextMove();
	if (!position || !move)
		return std::nullopt;

	return scid::core::notation::toSimpleMove(*position, move->action);
}

static std::optional<scid::core::MovetextLocation>
seekPgnLocation(const scid::database::Game& game, unsigned location) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!scid::core::pgn::seekLocation(cursor, location))
		return std::nullopt;
	return cursor.location();
}

static std::optional<unsigned>
pgnLocation(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return std::nullopt;
	return scid::core::pgn::locationOf(cursor);
}

static std::optional<unsigned>
pgnOffset(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return std::nullopt;
	return scid::core::pgn::offsetOf(cursor);
}

static bool setCurrentComment(scid::database::Game& game,
                              std::string_view comment) {
	scid::core::MovetextCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return false;
	return cursor.setComment(comment);
}

static bool clearCurrentNags(scid::database::Game& game) {
	scid::core::MovetextCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return false;
	cursor.clearPreviousMoveNags();
	return true;
}

static bool removeCurrentNag(scid::database::Game& game, bool moveNag) {
	scid::core::MovetextCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return false;
	return cursor.removePreviousMoveNag(moveNag);
}

static bool addCurrentNag(scid::database::Game& game, scid::database::byte nag) {
	scid::core::MovetextCursor cursor(game.coreGame());
	if (!cursor.restore(game.coreLocation()))
		return false;
	return cursor.addPreviousMoveNag(nag);
}

static bool stripMovetext(scid::database::Game& game, bool variations,
                          bool comments, bool nags) {
	if (variations) {
		scid::core::MovetextCursor cursor(game.coreGame());
		if (!cursor.restore(game.coreLocation()))
			return false;
		while (cursor.exitVariation()) {
		}
		game.restoreLocation(cursor.location());
	}

	game.coreGame().stripMovetext(variations, comments, nags);
	return true;
}

static void resetStartPosition(scid::database::Game& game,
                               const scid::database::Position& position) {
	game.coreGame().clearMovetext();
	game.coreGame().setStartPosition(position);
	game.restoreLocation(scid::core::MovetextLocation{});
}

static scid::database::errorT resetStartFen(scid::database::Game& game,
                                            const char* fen) {
	scid::database::Position position;
	if (auto err = position.ReadFromFEN(fen))
		return err;

	game.coreGame().clearMovetext();
	game.coreGame().setStartPosition(position);

	game.restoreLocation(scid::core::MovetextLocation{});
	return scid::database::OK;
}

//////////////////////////////////////////////////////////////////////
//
// Inline routines for setting Tcl result strings:
//

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setResult():
//    Inline function to set the Tcl interpreter result to a
//    constant string.
inline int
setResult (Tcl_Interp * ti, const char * str)
{
    Tcl_SetObjResult (ti, Tcl_NewStringObj(str, -1));
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setIntResult():
//    Inline function to set the Tcl interpreter result to a
//    signed integer value.
inline int
setIntResult (Tcl_Interp * ti, int i)
{
    Tcl_SetObjResult (ti, Tcl_NewWideIntObj(i));
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setUintResult():
//    Inline function to set the Tcl interpreter result to an
//    unsigned integer value.
inline int
setUintResult (Tcl_Interp * ti, scid::database::uint i)
{
    Tcl_SetObjResult (ti, Tcl_NewWideIntObj(i));
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AppendResult / AppendElement
//    Obj-based helpers for building results without the legacy string APIs.
//
//    These deliberately accept a trailing NULL for convenience (mirroring the
//    historic "append with NULL terminator" calling style) but do not rely on
//    varargs.
inline void
AppendResult (Tcl_Interp* ti)
{
    (void)ti;
}

template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline void
AppendResult (Tcl_Interp* ti, T)
{
    (void)ti;
}

template <typename... Rest>
inline void
AppendResult (Tcl_Interp* ti, const char* part, Rest... rest)
{
    if (part != nullptr) {
        Tcl_Obj* obj = Tcl_GetObjResult(ti);
        Tcl_AppendToObj(obj, part, -1);
    }
    AppendResult(ti, rest...);
}

inline void
AppendElement (Tcl_Interp* ti, const char* element)
{
    if (element == nullptr) {
        return;
    }

    Tcl_Obj* listObj = Tcl_GetObjResult(ti);
    if (Tcl_IsShared(listObj)) {
        listObj = Tcl_DuplicateObj(listObj);
        Tcl_SetObjResult(ti, listObj);
    }
    Tcl_ListObjAppendElement(ti, listObj, Tcl_NewStringObj(element, -1));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// appendUintResult:
//    Inline function to append the specified unsigned value to the
//    Tcl interpreter result.
inline int
appendUintResult (Tcl_Interp * ti, scid::database::uint i)
{
    char temp [20];
    std::snprintf(temp, sizeof(temp), "%u", i);
    AppendResult (ti, temp, NULL);
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// appendUintElement:
//    Inline function to append the specified unsigned value to the
//    Tcl interpreter list result.
inline scid::database::uint
appendUintElement (Tcl_Interp * ti, scid::database::uint i)
{
    char temp[20];
    std::snprintf(temp, sizeof(temp), "%u", i);
    AppendElement (ti, temp);
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setUintWidthResult():
//    Inline function to set the Tcl interpreter result to an
//    unsigned integer value, with zeroes to pad to the desired width.
inline int
setUintWidthResult (Tcl_Interp * ti, scid::database::uint i, scid::database::uint width)
{
    char temp [20];
    std::snprintf(temp, sizeof(temp), "%0*u", width, i);
    Tcl_SetObjResult (ti, Tcl_NewStringObj(temp, -1));
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// appendCharResult:
//    Inline function to append the specified character value to the
//    Tcl interpreter result.
inline int
appendCharResult (Tcl_Interp * ti, char ch)
{
    char tempStr [4];
    tempStr[0] = ch;
    tempStr[1] = 0;
    AppendResult (ti, tempStr, NULL);
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// translate:
//    Return the translation for a phrase.
//
inline const char *
translate (Tcl_Interp * ti, const char * name, const char * defaultText)
{
    Tcl_Obj* obj = Tcl_GetVar2Ex(ti, "tr", name, TCL_GLOBAL_ONLY);
    if (obj == nullptr) {
        return defaultText;
    }
    return Tcl_GetString(obj);
}

inline const char *
translate (Tcl_Interp * ti, const char * name)
{
    return translate (ti, name, name);
}

inline int errorResult (Tcl_Interp * ti, scid::database::errorT err, const char* errorMsg = 0) {
    if (errorMsg != 0) Tcl_SetObjResult (ti, Tcl_NewStringObj(errorMsg, -1));
    ASSERT(err != scid::database::OK);
    Tcl_SetObjErrorCode(ti, Tcl_NewWideIntObj(err));
    return TCL_ERROR;
}
inline int errorResult (Tcl_Interp * ti, const char* errorMsg) {
    return errorResult(ti, scid::database::ERROR_BadArg, errorMsg);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// InvalidCommand():
//    Given a Tcl Interpreter, a major command name (e.g. "sc_base") and
//    a null-terminated array of minor commands, this function sets
//    the interpreter's result to a useful error message listing the
//    available subcommands.
//    Returns TCL_ERROR, so caller can simply:
//        return InvalidCommand (...);
//    instead of:
//        InvalidCommand (...);
//        return TCL_ERROR;
int
InvalidCommand (Tcl_Interp * ti, const char * majorCmd,
                const char ** minorCmds)
{
    ASSERT (majorCmd != NULL);
    AppendResult (ti, "Invalid command: ", majorCmd,
                      " has the following minor commands:\n", NULL);
    while (*minorCmds != NULL) {
        AppendResult (ti, "   ", *minorCmds, "\n", NULL);
        minorCmds++;
    }
    return TCL_ERROR;
}


/************ End of Tcl result routines ***********/


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Standard error messages:
//
const char *
errMsgNotOpen (Tcl_Interp * ti)
{
    return translate (ti, "ErrNotOpen", "This is not an open database.");
}

const char *
errMsgSearchInterrupted (Tcl_Interp * ti)
{
    return translate (ti, "ErrSearchInterrupted",
                      "[Interrupted search; results are incomplete]");
}


/////////////////////////////////////////////////////////////////////
//  MISC functions
/////////////////////////////////////////////////////////////////////

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// str_is_prefix:
//    Provides a fast Tcl command "strIsPrefix" for checking if the
//    first string provided is a prefix of the second string, without
//    needing the standard slower [string match] or [string range]
//    routines.
int
str_is_prefix (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 3) {
        return errorResult (ti, "Usage: strIsPrefix <shortStr> <longStr>");
    }

    return UI_Result(ti, scid::database::OK, scid::database::strIsPrefix (argv[1], argv[2]));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// str_prefix_len:
//    Tcl command that returns the length of the common text at the start
//    of two strings.
int
str_prefix_len (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 3) {
        return errorResult (ti, "Usage: strPrefixLen <str> <str>");
    }

    return setUintResult (ti, scid::database::strPrefix (argv[1], argv[2]));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_base_inUse
//  Returns 1 if the database slot is in use; 0 otherwise.
int
sc_base_inUse (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto basePtr = db;
    if (argc > 2) {
        basePtr = DBasePool::getBase(scid::database::strGetUnsigned(argv[2]));
        if (basePtr == 0) return UI_Result(ti, scid::database::OK, false);
    }

    return UI_Result(ti, scid::database::OK, basePtr->isOpen());
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  exportGame:
//    Called by sc_base_export() to export a single game.
void
exportGame (scid::database::Game * g, FILE * exportFile,
            scid::database::LegacyGameEncodeOptions options)
{
    char old_language = scid::database::language;

    // Format-specific settings:
    switch (options.legacyFormat) {
    case scid::database::PGN_FORMAT_HTML:
    case scid::database::PGN_FORMAT_LaTeX:
        options.addStyle(PGN_STYLE_SHORT_HEADER);
        break;
    default:
        scid::database::language = 0;
        break;
    }

    options.htmlStyle = htmlDiagStyle;
    std::pair<const char*, unsigned> pgn = scid::database::legacy_pgn::encode(
        *g, options, 75, true,
        options.legacyFormat != scid::database::PGN_FORMAT_LaTeX);
    //size_t nWrited =
    fwrite(pgn.first, 1, pgn.second, exportFile);
    //TODO:
    //if (nWrited != db->tbuf->GetByteCount()) error
    scid::database::language = old_language;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_base_export:
//    Exports the current game or all filter games in the database
//    to a PGN, HTML or LaTeX file.
int
sc_base_export (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    bool exportFilter = false;
    bool appendToFile = false;
    scid::database::gameFormatT outputFormat = scid::database::PGN_FORMAT_Plain;
    const char * startText = "";
    const char * endText = "";
    const char * usage = "Usage: sc_base export current|filter PGN|HTML|LaTeX <pgn_filename> options...";
    scid::database::uint pgnStyle = PGN_STYLE_TAGS;

    const char * options[] = {
        "-append", "-starttext", "-endtext", "-comments", "-variations",
        "-spaces", "-symbols", "-indentComments", "-indentVariations",
        "-column", "-noMarkCodes", "-convertNullMoves", NULL
    };
    enum {
        OPT_APPEND, OPT_STARTTEXT, OPT_ENDTEXT, OPT_COMMENTS, OPT_VARIATIONS,
        OPT_SPACES, OPT_SYMBOLS, OPT_INDENTC, OPT_INDENTV,
        OPT_COLUMN, OPT_NOMARKS, OPT_CONVERTNULL
    };

    if (argc < 5) { return errorResult (ti, usage); }

    if (scid::database::strIsPrefix (argv[2], "current")) {
        exportFilter = false;
    } else if (scid::database::strIsPrefix (argv[2], "filter")) {
        exportFilter = true;
    } else {
        return errorResult (ti, usage);
    }

    if (! scid::database::LegacyGameEncodeOptions::legacyFormatFromString(
            argv[3], &outputFormat)) {
        return errorResult (ti, usage);
    }

    if (exportFilter  &&  !db->isOpen()) {
        return errorResult (ti, errMsgNotOpen(ti));
    }

    // Check for an even number of optional parameters:
    if ((argc % 2) != 1) { return errorResult (ti, usage); }

    // Parse all optional parameters:
    for (int arg = 5; arg < argc; arg += 2) {
        const char * value = argv[arg+1];
        bool flag = scid::database::strGetBoolean (value);
        int option = scid::database::strUniqueMatch (argv[arg], options);

        switch (option) {
        case OPT_APPEND:
            appendToFile = flag;
            break;

        case OPT_STARTTEXT:
            startText = value;
            break;

        case OPT_ENDTEXT:
            endText = value;
            break;

        case OPT_COMMENTS:
            if (flag) { pgnStyle |= PGN_STYLE_COMMENTS; }
            break;

        case OPT_VARIATIONS:
            if (flag) { pgnStyle |= PGN_STYLE_VARS; }
            break;

        case OPT_SPACES:
            if (flag) { pgnStyle |= PGN_STYLE_MOVENUM_SPACE; }
            break;

        case OPT_SYMBOLS:
            if (flag) { pgnStyle |= PGN_STYLE_SYMBOLS; }
            break;

        case OPT_INDENTC:
            if (flag) { pgnStyle |= PGN_STYLE_INDENT_COMMENTS; }
            break;

        case OPT_INDENTV:
            if (flag) { pgnStyle |= PGN_STYLE_INDENT_VARS; }
            break;

        case OPT_COLUMN:
            if (flag) { pgnStyle |= PGN_STYLE_COLUMN; }
            break;

        case OPT_NOMARKS:
            if (flag) { pgnStyle |= PGN_STYLE_STRIP_MARKS; }
            break;

        case OPT_CONVERTNULL:
            if (flag) { pgnStyle |= PGN_STYLE_NO_NULL_MOVES; }
            break;

        default:
            return InvalidCommand (ti, "sc_base export", options);
        }
    }

    const auto tcl_strings_are_utf8 =
        std::filesystem::path((const char8_t*)argv[4]).string();
    const auto exportFileName = tcl_strings_are_utf8.c_str();
    auto exportFile = fopen (exportFileName, (appendToFile ? "r+" : "w"));
    if (exportFile == NULL) {
        return errorResult (ti, "Error opening file for exporting games.");
    }
    // Write start text or find the place in the file to append games:
    if (appendToFile) {
        if (outputFormat == scid::database::PGN_FORMAT_Plain) {
            fseek (exportFile, 0, SEEK_END);
        } else {
            fseek (exportFile, 0, SEEK_SET);
            const char * endMarker = "";
            if (outputFormat == scid::database::PGN_FORMAT_HTML) {
                endMarker = "</body>";
            } else if (outputFormat == scid::database::PGN_FORMAT_LaTeX) {
                endMarker = "\\end{document}";
            }
            char line [1024];
            scid::database::uint pos = 0;
            while (1) {
                char* err = fgets(line, 1024, exportFile);
                if (err == 0 || feof(exportFile)) break;
                const char * s = scid::database::strTrimLeft (line, " ");
                if (scid::database::strIsCasePrefix (endMarker, s)) {
                    // We have seen the line to stop at, so break out
                    break;
                }
                pos = ftell (exportFile);
            }
            fseek (exportFile, pos, SEEK_SET);
        }
    } else {
        fputs (startText, exportFile);
    }

    if (!exportFilter) {
        auto editor = scidup::app::editor::gameSession(*db);
        exportGame (&editor.game(), exportFile, {pgnStyle, outputFormat, 0});
    } else { //TODO: remove this (duplicate of sc_filter export)
        scid::database::Progress progress = UI_CreateProgress(ti);
        scid::database::uint numSeen = 0;
        scid::database::uint numToExport = db->defaultFilterCount();
        scid::database::Game * g = scratchGame;
        for (scid::database::gamenumT i=0, n=db->numGames(); i < n; i++) {
            if (db->defaultFilterGet(i)) { // Export this game:
                if (++numSeen % 1024 == 0) {  // Update the percentage done bar:
                    if (!progress.report(numSeen, numToExport)) break;
                }

                // Print the game, skipping any corrupt games:
                const scid::database::IndexEntry* ie = db->getIndexEntry(i);
                if (ie->GetLength() == 0) { continue; }
                if (db->getGame(*ie, *g) != scid::database::OK) {
                    continue;
                }
                exportGame (g, exportFile, {pgnStyle, outputFormat, 0});
            }
        }
        progress.report(1, 1);
    }
    fputs (endText, exportFile);
    fclose (exportFile);
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_base_piecetrack:
//    Examines games in the filter of the current database and
//    returns a list of 64 integers indicating how frequently
//    the specified piece moves to each square.
int
sc_base_piecetrack (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    const char * usage =
        "Usage: sc_base piecetrack [-g|-t] <minMoves> <maxMoves> <startSquare ...>";

    if (argc < 5) {
        return errorResult (ti, usage);
    }

    // Check for optional mode parameter:
    bool timeOnSquareMode = false;
    int arg = 2;
    if (argv[arg][0] == '-') {
        if (argv[arg][1] == 'g'  && scid::database::strIsPrefix (argv[arg], "-games")) {
            timeOnSquareMode = false;
            arg++;
        } else if (argv[arg][1] == 't'  && scid::database::strIsPrefix (argv[arg], "-time")) {
            timeOnSquareMode = true;
            arg++;
        } else {
            return errorResult (ti, usage);
        }
    }

    // Read the two move-number parameters:
    scid::database::uint minPly = scid::database::strGetUnsigned(argv[arg]) * 2;
    arg++;
    scid::database::uint maxPly = scid::database::strGetUnsigned(argv[arg]) * 2;
    arg++;

    // Convert moves to plycounts, e.g. "5-10" -> "9-20"
    if (minPly < 2) { minPly=2; }
    if (maxPly < minPly) { maxPly = minPly; }
    minPly--;

    // Parse the variable number of tracked square arguments:
    scid::database::uint sqFreq[64] = {0};
    bool trackSquare[64] = { false };
    int nTrackSquares = 0;
    for (int a=arg; a < argc; a++) {
        scid::database::squareT sq = scid::database::strGetSquare (argv[a]);
        if (sq == scid::database::NULL_SQUARE) { return errorResult (ti, usage); }
        if (!trackSquare[sq]) {
            // Seen another starting square to track.
            trackSquare[sq] = true;
            nTrackSquares++;
        }
    }

    // If current base is unused, filter is empty, or no track
    // squares specified, then just return a zero-filled list:

    if (! db->isOpen()  ||  db->defaultFilterCount() == 0  ||  nTrackSquares == 0) {
        for (scid::database::uint i=0; i < 64; i++) { appendUintElement (ti, 0); }
        return TCL_OK;
    }

    // Examine every filter game and track the selected pieces:

    scid::database::Progress progress = UI_CreateProgress(ti);
    const auto filter = db->getFilter("dbfilter");
    const size_t filterCount = filter->size();
    size_t filterSeen = 0;
    for (const auto gnum : filter) {
        if (++filterSeen % 1024 == 0) {
            if (!progress.report(filterSeen, filterCount)) {
                return UI_Result(ti, scid::database::ERROR_UserCancel);
            }
        }

        const scid::database::IndexEntry* ie = db->getIndexEntry(gnum);

        // Skip games with non-standard start or no moves:
        if (ie->GetStartFlag()) { continue; }
        if (ie->GetLength() == 0) { continue; }

        // Skip games too short to be useful:
        if (ie->GetNumHalfMoves() < minPly) { continue; }

        // Set up piece tracking for this game:
        bool movedTo[64] = { false };
        bool track[64];
        int ntrack = nTrackSquares;
        for (scid::database::uint sq=0; sq < 64; sq++) { track[sq] = trackSquare[sq]; }

        // Process each game move until the maximum ply or end of
        // the game is reached:
        scid::database::uint plyCount = 0;
        db->getGame(ie).mainLine([&](auto move) {
            if (plyCount++ >= maxPly)
                return false;

            scid::database::squareT toSquare = move.getTo();
            scid::database::squareT fromSquare = move.getFrom();

            // Special hack for castling:
            if (move.isCastle()) {
                if (toSquare == scid::database::H1) {
                    if (track[scid::database::H1]) {
                        fromSquare = scid::database::H1;
                        toSquare = scid::database::F1;
                    } else {
                        toSquare = scid::database::G1;
                    }
                } else if (toSquare == scid::database::A1) {
                    if (track[scid::database::A1]) {
                        fromSquare = scid::database::A1;
                        toSquare = scid::database::D1;
                    } else {
                        toSquare = scid::database::C1;
                    }
                } else if (toSquare == scid::database::H8) {
                    if (track[scid::database::H8]) {
                        fromSquare = scid::database::H8;
                        toSquare = scid::database::F8;
                    } else {
                        toSquare = scid::database::G8;
                    }
                } else if (toSquare == scid::database::A8) {
                    if (track[scid::database::A8]) {
                        fromSquare = scid::database::A8;
                        toSquare = scid::database::D8;
                    } else {
                        toSquare = scid::database::C8;
                    }
                }
            }

            // TODO: Special hack for en-passant capture?

            if (track[toSquare]) {
                // A tracked piece has been captured:
                track[toSquare] = false;
                ntrack--;
                if (ntrack <= 0)
                    return false;

            } else if (track[fromSquare]) {
                // A tracked piece is moving:
                track[fromSquare] = false;
                track[toSquare] = true;
                if (plyCount >= minPly) {
                    // If not time-on-square mode, and this
                    // new target square has not been moved to
                    // already by a tracked piece in this game,
                    // increase its frequency now:
                    if (!timeOnSquareMode  && !movedTo[toSquare]) {
                        sqFreq[toSquare]++;
                    }
                    movedTo[toSquare] = true;
                }
            }

            if (timeOnSquareMode  &&  plyCount >= minPly) {
                // Time-on-square mode: find all tracked squares
                // (there are ntrack of them) and increment the
                // frequency of each.
                int nleft = ntrack;
                for (scid::database::uint i=0; i < 64; i++) {
                    if (track[i]) {
                        sqFreq[i]++;
                        nleft--;
                        // We can stop early when all tracked
                        // squares have been found:
                        if (nleft <= 0)
                            return false;
                    }
                }
            }

            return true;
        }); // while (plyCount < maxPly)
    } // foreach game

    progress.report(1, 1);

    // Now return the 64-integer list: if in time-on-square mode,
    // the value for each square is the number of plies when a
    // tracked piece was on it, so halve it to convert to moves:

    for (scid::database::uint i=0; i < 64; i++) {
        appendUintElement (ti, timeOnSquareMode ? sqFreq[i] / 2 : sqFreq[i]);
    }
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_base_duplicates:
//    Finds duplicate games and marks them deleted.
//    A pair of games are considered duplicates if the Event, Site,
//    White, Black, and Round values all match identically, and the
//    Date matches to within 2 days (that is, the same year, the same
//    month, and the days of month differ by 2 at most).
//
//    Furthermore, the moves of one game should, after truncating, be the
//    same as the moves of the other game, for them to be duplicates.

struct gNumListT {
    uint64_t hash;
    scid::database::uint gNumber;
    bool operator<(const gNumListT& a) const { return hash < a.hash; }
};

struct dupCriteriaT {
    bool exactNames;
    bool sameColors;
    bool sameEvent;
    bool sameSite;
    bool sameRound;
    bool sameResult;
    bool sameYear;
    bool sameMonth;
    bool sameDay;
    bool sameEcoCode;
    bool sameMoves;
};

bool
checkDuplicate (scid::database::scidBaseT * base,
                const scid::database::IndexEntry * ie1, const scid::database::IndexEntry * ie2,
                dupCriteriaT * cr)
{
    if (ie1->GetDeleteFlag()  ||  ie2->GetDeleteFlag()) { return false; }
    if (cr->sameEvent) {
        if (ie1->GetEvent() != ie2->GetEvent()) { return false; }
    }
    if (cr->sameSite) {
        if (ie1->GetSite() != ie2->GetSite()) { return false; }
    }
    if (cr->sameRound) {
        if (ie1->GetRound() != ie2->GetRound()) { return false; }
    }
    if (cr->sameYear) {
        if (ie1->GetYear() != ie2->GetYear()) { return false; }
    }
    if (cr->sameMonth) {
        if (ie1->GetMonth() != ie2->GetMonth()) { return false; }
    }
    if (cr->sameDay) {
        if (ie1->GetDay() != ie2->GetDay()) { return false; }
    }
    if (cr->sameResult) {
        if (ie1->GetResult() != ie2->GetResult()) { return false; }
    }
    if (cr->sameEcoCode) {
        scidup::eco::String a;
        scidup::eco::String b;
        scidup::eco::toBasicString(ie1->GetEcoCode(), a);
        scidup::eco::toBasicString(ie2->GetEcoCode(), b);
        if (a[0] != b[0]  ||  a[1] != b[1]  ||  a[2] != b[2]) { return false; }
    }

    // There are a lot of "place-holding" games in some database, that have
    // just one (usually wrong) move and a result, that are then replaced by
    // the full version of the game. Therefore, if we reach here and one
    // of the games (or both) have only one move or no moves, return true
    // as long as they have the same year, site and round:

    if (ie1->GetNumHalfMoves() <= 2  ||  ie2->GetNumHalfMoves() <= 2) {
        if (ie1->GetYear() == ie2->GetYear()  &&
            ie1->GetSite() == ie2->GetSite()  &&
            ie1->GetRound() == ie2->GetRound()) {
            return true;
        }
    }

    // Now check that the games contain the same moves, up to the length
    // of the shorter game:

    if (cr->sameMoves) {
        const scid::database::byte * hpData1 = ie1->GetHomePawnData();
        const scid::database::byte * hpData2 = ie2->GetHomePawnData();
        if (! scid::database::hpSig_Prefix (hpData1, hpData2)) { return false; }
        // Now we have to check the actual moves of the games:
        scid::database::uint length = std::min(ie1->GetNumHalfMoves(), ie2->GetNumHalfMoves());
        std::string a = base->getGame(ie1).getMoveSAN(0, length);
        std::string b = base->getGame(ie2).getMoveSAN(0, length);
        return (a == b);
    }
    return true;
}

UI_res_t sc_base_duplicates(scid::database::scidBaseT* dbase, UI_handle_t ti, int argc,
                            const char** argv) {
    dupCriteriaT criteria;
    criteria.exactNames  = false;
    criteria.sameColors  = true;
    criteria.sameEvent   = true;
    criteria.sameSite    = true;
    criteria.sameRound   = true;
    criteria.sameYear    = true;
    criteria.sameMonth   = true;
    criteria.sameDay     = false;
    criteria.sameResult  = false;
    criteria.sameEcoCode = false;
    criteria.sameMoves = true;

    bool skipShortGames = false;
    bool keepAllCommentedGames = true;
    bool keepAllGamesWithVars  = true;
    bool setFilterToDups = false;
    bool onlyFilterGames = false;

    // Deletion strategy: delete the shorter game, the game with the
    // smaller game number, or the game with the larger game number.
    enum deleteStrategyT { DELETE_SHORTER, DELETE_OLDER, DELETE_NEWER };
    deleteStrategyT deleteStrategy = DELETE_SHORTER;

    // Parse command options in pairs of arguments:

    const char * options[] = {
        "-players", "-colors", "-event", "-site", "-round", "-year",
        "-month", "-day", "-result", "-eco", "-moves", "-skipshort",
        "-comments", "-variations", "-setfilter", "-usefilter",
        "-delete",
        NULL
    };
    enum {
        OPT_PLAYERS, OPT_COLORS, OPT_EVENT, OPT_SITE, OPT_ROUND, OPT_YEAR,
        OPT_MONTH, OPT_DAY, OPT_RESULT, OPT_ECO, OPT_MOVES, OPT_SKIPSHORT,
        OPT_COMMENTS, OPT_VARIATIONS, OPT_SETFILTER, OPT_USEFILTER,
        OPT_DELETE
    };

    for (int arg = 3; arg < argc; arg += 2) {
        const char * optStr = argv[arg];
        const char * valueStr = argv[arg + 1];
        bool b = scid::database::strGetBoolean (valueStr);
        int index = scid::database::strUniqueMatch (optStr, options);
        switch (index) {
            case OPT_PLAYERS:     criteria.exactNames = b;   break;
            case OPT_COLORS:      criteria.sameColors = b;   break;
            case OPT_EVENT:       criteria.sameEvent = b;    break;
            case OPT_SITE:        criteria.sameSite = b;     break;
            case OPT_ROUND:       criteria.sameRound = b;    break;
            case OPT_YEAR:        criteria.sameYear = b;     break;
            case OPT_MONTH:       criteria.sameMonth = b;    break;
            case OPT_DAY:         criteria.sameDay = b;      break;
            case OPT_RESULT:      criteria.sameResult = b;   break;
            case OPT_ECO:         criteria.sameEcoCode = b;  break;
            case OPT_MOVES:       criteria.sameMoves = b;    break;
            case OPT_SKIPSHORT:   skipShortGames = b;        break;
            case OPT_COMMENTS:    keepAllCommentedGames = b; break;
            case OPT_VARIATIONS:  keepAllGamesWithVars = b;  break;
            case OPT_SETFILTER:   setFilterToDups = b;       break;
            case OPT_USEFILTER:   onlyFilterGames = b;       break;
            case OPT_DELETE:
                if (dbase->isReadOnly())
                    return UI_Result(ti, scid::database::ERROR_FileReadOnly);
                if (scid::database::strIsCasePrefix (valueStr, "shorter")) {
                    deleteStrategy = DELETE_SHORTER;
                } else if (scid::database::strIsCasePrefix (valueStr, "older")) {
                    deleteStrategy = DELETE_OLDER;
                } else if (scid::database::strIsCasePrefix (valueStr, "newer")) {
                    deleteStrategy = DELETE_NEWER;
                } else {
                    return UI_Result(ti, scid::database::ERROR_BadArg, "Invalid option.");
                }
                break;
            default:
                return UI_Result(ti, scid::database::ERROR_BadArg, "Invalid option.");
        }
    }
    const scid::database::gamenumT numGames = dbase->numGames();

    // Setup duplicates array:
    auto duplicates = std::make_unique<scid::database::gamenumT[]>(numGames);

    // We use a hashtable to limit duplicate game comparisons; each game
    // is only compared to others that hash to the same value.
    std::vector<gNumListT> hash(numGames);
    size_t n_hash = 0;
    const std::vector<uint32_t>& hashMap = (criteria.exactNames)
            ? std::vector<uint32_t>()
            : dbase->getNameBase()->generateHashMap(scid::database::NAME_PLAYER);
    for (scid::database::gamenumT i=0; i < numGames; i++) {
        const scid::database::IndexEntry* ie = dbase->getIndexEntry(i);
        if (! ie->GetDeleteFlag()  /* &&  !ie->GetStartFlag() */
            &&  (!skipShortGames  ||  ie->GetNumHalfMoves() >= 10)
            &&  (!onlyFilterGames  ||  dbase->defaultFilterGet(i) > 0)) {

            uint32_t wh = ie->GetWhite();
            uint32_t bl = ie->GetBlack();
            if (!criteria.exactNames) {
                wh = hashMap[wh];
                bl = hashMap[bl];
            }
            if (!criteria.sameColors && bl > wh) {
                std::swap(wh, bl);
            }
            gNumListT* node = &(hash[n_hash++]);
            node->hash = (uint64_t(wh) << 32) + bl;
            node->gNumber = i;
        }
    }
    hash.resize(n_hash);
    std::sort(hash.begin(), hash.end());

    scid::database::Filter tmp_filter(numGames);
    scid::database::HFilter filter = setFilterToDups ? dbase->getFilter("dbfilter")
                                     : scid::database::HFilter(&tmp_filter);
    filter.clear();
    scid::database::Progress progress = UI_CreateProgress(ti);
    // Now check same-hash games for duplicates:
    for (size_t i=0; i < n_hash; i++) {
        if ((i % 1024) == 0) {
            if (!progress.report(i, numGames)) break;
        }
        const gNumListT& head = hash[i];
        const scid::database::IndexEntry* ieHead = dbase->getIndexEntry(head.gNumber);

        for (size_t comp=i+1; comp < n_hash; comp++) {
            const gNumListT& compare = hash[comp];
            if (compare.hash != head.hash) break;

            const scid::database::IndexEntry* ieComp = dbase->getIndexEntry(compare.gNumber);

            if (checkDuplicate(dbase, ieHead, ieComp, &criteria)) {
                duplicates[head.gNumber] = compare.gNumber + 1;
                duplicates[compare.gNumber] = head.gNumber + 1;

                auto isImmune = [&](const scid::database::IndexEntry* ie) {
                    if (keepAllCommentedGames && ie->GetCommentsFlag())
                        return true;
                    return keepAllGamesWithVars && ie->GetVariationsFlag();
                };

                // Decide which game should get deleted:
                bool deleteHead = false;
                if (deleteStrategy == DELETE_OLDER) {
                    deleteHead = (head.gNumber < compare.gNumber);
                } else if (deleteStrategy == DELETE_NEWER) {
                    deleteHead = (head.gNumber > compare.gNumber);
                } else {
                    ASSERT(deleteStrategy == DELETE_SHORTER);
                    scid::database::uint a = ieHead->GetNumHalfMoves();
                    scid::database::uint b = ieComp->GetNumHalfMoves();
                    deleteHead = (a <= b);
                    if (a == b && isImmune(ieHead))
                        deleteHead = false;
                }

                scid::database::gamenumT gnumDelete = compare.gNumber;
                const scid::database::IndexEntry* ieDelete = ieComp;
                if (deleteHead) {
                    gnumDelete = head.gNumber;
                    ieDelete = ieHead;
                }
                // Delete whichever game is to be deleted:
                if (!isImmune(ieDelete)) {
                    filter->set(gnumDelete, 1);
                }
            }
        }
    }
    auto[err, nDel] = dbase->transformIndex(filter, {}, [](scid::database::IndexEntry& ie) {
        ie.SetDeleteFlag(true);
        return true;
    });
    dbase->setDuplicates(std::move(duplicates));
    progress.report(1, 1);
    return (err == scid::database::OK) ? UI_Result(ti, scid::database::OK, nDel) : UI_Result(ti, err);
}

//////////////////////////////////////////////////////////////////////
/// CLIPBASE functions
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_clipbase:
//    scid::database::Game clipbase functions.
//    Copies a game to, or pastes from, the clipbase database.
int
sc_clipbase (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto clipbase = DBasePool::getBase(DBasePool::getClipBase());
    ASSERT(clipbase);

    static const char * options [] = {
        "clear", "paste", NULL
    };
    enum {
        CLIP_CLEAR, CLIP_PASTE
    };
    int index = -1;

    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    switch (index) {
    case CLIP_CLEAR:
        clipbase->Close();
        DBasePool::clearClipBase();
        break;

    case CLIP_PASTE: // Paste the active clipbase game
        if (db != clipbase) {
            auto editor = scidup::app::editor::gameSession(*db);
            auto clipEditor = scidup::app::editor::gameSession(*clipbase);
            editor.replace(clipEditor.game().clone(), std::nullopt, true);
        }
        break;

    default:
        return InvalidCommand(ti, "sc_clipbase", options);
    }

    return UI_Result(ti, scid::database::OK);
}

//////////////////////////////////////////////////////////////////////
/// ECO Classification functions

// ecoTranslateT:
//    Structure for a linked list of ECO opening name translations.
//
struct ecoTranslateT {
    char   language;
    char * from;
    char * to;
    ecoTranslateT * next;
};

static ecoTranslateT * ecoTranslations = NULL;
void translateECO (Tcl_Interp * ti, const char * strFrom, scid::database::DString * dstrTo);

int
sc_eco (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    int index = -1;
    static const char * options [] = {
        "base", "game", "read", "reset", "summary",
        "translate", NULL
    };
    enum {
        ECO_BASE, ECO_GAME, ECO_READ, ECO_RESET, ECO_SUMMARY,
        ECO_TRANSLATE
    };

    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    switch (index) {
    case ECO_BASE:
        return sc_eco_base (cd, ti, argc, argv);

    case ECO_GAME:
        return sc_eco_game (cd, ti, argc, argv);

    case ECO_READ:
        return sc_eco_read (cd, ti, argc, argv);

    case ECO_RESET:
        if (ecoBook) { ecoBook = NULL; }
        break;

    case ECO_SUMMARY:
        return sc_eco_summary (cd, ti, argc, argv);

    case ECO_TRANSLATE:
        return sc_eco_translate (cd, ti, argc, argv);

    default:
        return InvalidCommand (ti, "sc_eco", options);
    }

    return TCL_OK;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_eco_base:
//    Reclassifies all games in the current base by ECO code.
//
//    The first parameter indicates if all games (not only those
//    with no existing ECO code) should be classified.
//       "0" or "nocode": only games with no ECO code.
//       "1" or "all": classify all games.
//       "date:yyyy.mm.dd": only games since date.
//    The second boolean parameter indicates if Scid-specific ECO
//    extensions (e.g. "B12j" instead of just "B12") should be used.
//
//    If the database is read-only, games can still be classified but
//    the results will not be scid::database::stored to the database file.
int
sc_eco_base (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 4) {
        return errorResult (ti, "Usage: sc_eco base <bool:all_games> <bool:extensions>");
    }
    if (!ecoBook) { return errorResult (ti, "No ECO Book is loaded."); }
    if (! db->isOpen()) return errorResult (ti, scid::database::ERROR_FileNotOpen);

    int option = -1;
    enum {ECO_NOCODE, ECO_ALL, ECO_DATE, ECO_FILTER};

    switch (argv[2][0]) {
    case '0':
    case 'n':
        option = ECO_NOCODE; break;
    case 'd':
        option = ECO_DATE; break;
    case 'f':
        option = ECO_FILTER; break;
    default:
        option = ECO_ALL; break;
    }

    bool extendedCodes = scid::database::strGetBoolean(argv[3]);
    scid::database::dateT startDate = scid::database::ZERO_DATE;
    if (option == ECO_DATE) {
        startDate = scid::database::date_EncodeFromString (&(argv[2][5]));
    }

    scid::database::scidBaseT& dbase = *db;
    auto entry_op = [&](scid::database::IndexEntry& ie) {
        if (ie.GetLength() == 0)
            return false;

        // Ignore games with existing ECO code if directed:
        if (option == ECO_NOCODE && ie.GetEcoCode() != 0)
            return false;

        // Ignore games before starting date if directed:
        if (option == ECO_DATE && ie.GetDate() < startDate)
            return false;

        auto bbuf = dbase.getGame(ie);
        scid::database::Position currentPosition;
        if (bbuf.decodeTags([](auto, auto) {}) != scid::database::OK)
            return false;

        const auto [errStartPos, fen] = bbuf.decodeStartBoard();
        if (errStartPos)
            return false;
        if (fen) {
            if (currentPosition.ReadFromFEN(fen) != scid::database::OK)
                return false;
        } else {
            currentPosition.StdStart();
        }

        scidup::eco::Code ecoCode = scidup::eco::ECO_None;
        for (;;) {
            if (currentPosition.TotalMaterial() < ecoBook->fewestPieces())
                break;

            const auto eco = ecoBook->findEco(currentPosition);
            if (eco != scidup::eco::ECO_None) {
                ecoCode = eco;
            }

            scid::database::simpleMoveT sm;
            if (scid::database::game_storage::decodeMainlineMove(
                    bbuf, currentPosition, sm) != scid::database::OK)
                break;

            currentPosition.DoSimpleMove(sm);
        }

        if (!extendedCodes) {
            ecoCode = scidup::eco::basicCode(ecoCode);
        }

        if (ie.GetEcoCode() != ecoCode) {
            ie.SetEcoCode(ecoCode);
            return true;
        }
        return false;
    };

    std::string filter =
        (option == ECO_FILTER) ? "dbfilter" : dbase.newFilter();
    auto hf = scidup::app::tree::resolveFilter(dbase, filter);
    auto changes = dbase.transformIndex(hf, UI_CreateProgress(ti), entry_op);
    if (option == ECO_FILTER)
        dbase.deleteFilter(filter.c_str());

    return UI_Result(ti, changes.first, changes.second);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_eco_game:
//    Returns ECO code for the current game. If the optional
//    parameter <ply> is passed, it returns the ply depth of the
//    deepest match instead of the ECO code.
int
sc_eco_game (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game& game = editor.game();
    scid::database::uint returnPly = 0;
    if (argc > 2) {
        if (argc == 3  &&  scid::database::strIsPrefix (argv[2], "ply")) {
            returnPly = 1;
        } else {
            return errorResult (ti, "Usage: sc_game eco [ply]");
        }
    }
    if (!ecoBook) { return TCL_OK; }

    auto location = game.coreLocation();
    {
        scid::core::GameCursor cursor(game.coreGame());
        cursor.toEnd();
        game.restoreLocation(cursor.location());
    }
    scidup::eco::Code ecoCode = scidup::eco::ECO_None;
    do {
        auto pos = currentPosition(game);
        if (!pos) { break; }
        ecoCode = ecoBook->findEco(*pos);
    } while (ecoCode == scidup::eco::ECO_None && game.previous() == scid::database::OK);

    auto ply = currentPly(game);
    game.restoreLocation(location);

    if (ecoCode == scidup::eco::ECO_None)
        return UI_Result(ti, scid::database::OK);

    if (returnPly)
        return UI_Result(ti, scid::database::OK, ply);

    scidup::eco::String extEco;
    scidup::eco::toExtendedString(ecoCode, extEco);
    return UI_Result(ti, scid::database::OK, extEco);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_eco_read:
//    Reads a book file for ECO classification.
//    Returns the book size (number of positions).
int
sc_eco_read (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc < 3) { return TCL_OK; }
    ecoBook = nullptr;
    auto book = scidup::eco::Book::load(argv[2]);
    if (book.first != scidup::eco::OK) {
        if (book.first == scidup::eco::ERROR_FileOpen) {
            AppendResult (ti, "Unable to open the ECO file:\n",
                              argv[2], NULL);
        } else {
            AppendResult (ti, "Unable to load the ECO file:\n",
                              argv[2], NULL);
        }
        return book.first;
    }
    ecoBook = std::make_unique<scidup::eco::Book>(std::move(book.second));
    return UI_Result(ti, scid::database::OK, ecoBook->size());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// formatEcoSummary:
//    Formats structured ECO book lines using the legacy text shape consumed by
//    sc_eco_summary's translation and hypertext conversion.
std::string
formatEcoSummary(const scidup::eco::Book& book, std::string_view prefix)
{
    auto res = std::string();
    auto prevCode = std::string_view();
    for (const auto& line : book.linesWithPrefix(prefix)) {
        if (prefix.size() < 3) {
            const auto common = std::mismatch(line.code.begin(), line.code.end(),
                                              prevCode.begin(), prevCode.end());
            const size_t nChars = std::distance(line.code.begin(), common.first);
            if (nChars > prefix.size()) {
                continue;
            }

            prevCode = line.code;
        }

        res.append(line.code);
        res.append(" [");
        res.append(line.name);
        res.append("]  ");
        res.append(line.moves);
        res.push_back('\n');
    }
    return res;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_eco_summary:
//    Returns a listing of positions for the specified ECO prefix,
//    in plain text or color (Scid hypertext) format.
int
sc_eco_summary (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    bool color = true;
    if (argc != 3  &&  argc != 4) {
        return errorResult (ti, "Usage: sc_eco summary <ECO-prefix> [<bool:color>]");
    }
    if (argc == 4) { color = scid::database::strGetBoolean (argv[3]); }
    if (!ecoBook) { return TCL_OK; }
    scid::database::DString * dstr = new scid::database::DString;
    scid::database::DString * temp = new scid::database::DString;
    bool inMoveList = false;
    translateECO(ti, formatEcoSummary(*ecoBook, argv[2]).c_str(), dstr);
    if (color) {
        scid::database::DString * oldstr = dstr;
        dstr = new scid::database::DString;
        const char * s = oldstr->Data();
        while (*s) {
            char ch = *s;
            switch (ch) {
            case '[':
                dstr->Append ("<tab>");
                dstr->AddChar (ch);
                break;
            case ']':
                dstr->AddChar (ch);
                dstr->Append ("<blue><run importMoveList {");
                inMoveList = true;
                temp->Clear();
                break;
            case '\n':
                if (inMoveList) {
                    dstr->Append ("}>", temp->Data());
                    inMoveList = false;
                }
                dstr->Append ("</run></blue></tab><br>");
                break;
            default:
                dstr->AddChar (ch);
                if (inMoveList) { temp->AddChar (scid::database::transPiecesChar(ch)); }//{ temp->AddChar (ch); }
            }
            s++;
        }
        delete oldstr;
    }
    AppendResult (ti, dstr->Data(), NULL);
    delete temp;
    delete dstr;
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_eco_translate:
//    Adds a new ECO openings phrase translation.
int
sc_eco_translate (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 5) {
        return errorResult (ti, "Usage: sc_eco translate <lang> <from> <to>");
    }

#ifdef WINCE
    ecoTranslateT * trans = (ecoTranslateT * )my_Tcl_Alloc( sizeof(ecoTranslateT));
#else
    ecoTranslateT * trans = new ecoTranslateT;
#endif
    trans->next = ecoTranslations;
    trans->language = argv[2][0];
    trans->from = scid::database::strDuplicate (argv[3]);
    trans->to = scid::database::strDuplicate (argv[4]);
    ecoTranslations = trans;
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// translateECO:
//    Translates an ECO opening name into the current scid::database::language.
//
void
translateECO (Tcl_Interp * ti, const char * strFrom, scid::database::DString * dstrTo)
{
    ecoTranslateT * trans = ecoTranslations;
    dstrTo->Clear();
    dstrTo->Append (strFrom);
    Tcl_Obj* langObj = Tcl_GetVar2Ex(ti, "language", nullptr, TCL_GLOBAL_ONLY);
    const char * language = (langObj != nullptr) ? Tcl_GetString(langObj) : nullptr;
    if (language == nullptr) { return; }
    char lang = language[0];
    while (trans != NULL) {
        if (trans->language == lang
            &&  scid::database::strContains (dstrTo->Data(), trans->from)) {
            // Translate this phrase in the string:
            char * temp = scid::database::strDuplicate (dstrTo->Data());
            dstrTo->Clear();
            char * in = temp;
            while (*in != 0) {
                if (scid::database::strIsPrefix (trans->from, in)) {
                    dstrTo->Append (trans->to);
                    in += scid::database::strLength (trans->from);
                } else {
                    dstrTo->AddChar (*in);
                    in++;
                }
            }
#ifdef WINCE
            my_Tcl_Free((char*) temp);
#else
            delete[] temp;
#endif
        }
        trans = trans->next;
    }
}

//////////////////////////////////////////////////////////////////////
///  FILTER functions
namespace {

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_filter_freq:
//    Returns a two-integer list showing how many filter games,
//    and how many total database games, meet the specified
//    date or mean rating range criteria.
//    Usage:
//        sc_filter freq baseId filterName date <startdate> [<endDate>]
//    or  sc_filter freq baseId filterName elo <lowerMeanElo> [<upperMeanElo>]
//Klimmek: or sc_filter freq baseId filterName moves <lowerhalfMove> <higherhalfMove>
//         add mode to count games with specified movenumber
//    where the final parameter defaults to the maximum allowed
//    date or Elo rating.
//    Note for rating queries: only the rating values in the game
//    are used; estimates from other games or the spelling file
//    will be ignored. Also, if one player has an Elo rating but
//    the other does not, the other rating will be assumed to be
//    same as the nonzero rating, up to a maximum of 2200.
int
sc_filter_freq (scid::database::scidBaseT* dbase, const scid::database::HFilter& filter, Tcl_Interp * ti, int argc, const char ** argv)
{
    const char * usage =
        "Usage: sc_filter freq baseId filterName date|elo|move <startDate|minElo|lowerhalfMove> [<endDate|maxElo|higherhalfMove>] [GuessElo]";

    bool eloMode = false;
    bool moveMode = false;
    bool guessElo = true;
    const char * options[] = { "date", "elo", "move", NULL };
    enum { OPT_DATE, OPT_ELO, OPT_MOVE };
    int option = -1;

    if (argc >= 6  &&  argc <= 8) {
        option = scid::database::strUniqueMatch (argv[4], options);
    }
    switch (option) {
        case OPT_DATE: eloMode = false; break;
        case OPT_ELO: eloMode = true; break;
        case OPT_MOVE: moveMode = true; break;
        default: return errorResult (ti, usage);
    }

    scid::database::dateT startDate = scid::database::date_EncodeFromString (argv[5]);
    scid::database::dateT endDate = (((scid::database::YEAR_MAX) << scid::database::YEAR_SHIFT) | ((12) << scid::database::MONTH_SHIFT) | (31));
    scid::database::uint minElo = scid::database::strGetUnsigned (argv[5]);
    scid::database::uint maxElo = scid::database::MAX_ELO;
    scid::database::uint maxMove, minMove;

    minMove = minElo;
    maxMove = minMove + 1;
    if (argc >= 7) {
        endDate = scid::database::date_EncodeFromString (argv[6]);
        maxMove = maxElo = scid::database::strGetUnsigned (argv[6]);
    }
    if (argc == 8) {
        guessElo = scid::database::strGetUnsigned (argv[7]);
    }
    if ( guessElo ) {
    // Double min/max Elos to avoid halving every mean Elo:
        minElo = minElo + minElo;
        maxElo = maxElo + maxElo + 1;
    }
    // Calculate frequencies in the specified date or rating range:
    scid::database::uint filteredCount = 0;
    scid::database::uint allCount = 0;

    if (eloMode) {
        for (scid::database::uint gnum=0, n = dbase->numGames(); gnum < n; gnum++) {
            const scid::database::IndexEntry* ie = dbase->getIndexEntry(gnum);
            if ( guessElo ) {
                scid::database::uint wElo = ie->GetWhiteElo();
                scid::database::uint bElo = ie->GetBlackElo();
                scid::database::uint bothElo = wElo + bElo;
                if (wElo == 0  &&  bElo != 0) {
                    bothElo += (bElo > 2200 ? 2200 : bElo);
                } else if (bElo == 0  &&  wElo != 0) {
                    bothElo += (wElo > 2200 ? 2200 : wElo);
                }
                if (bothElo >= minElo  &&  bothElo <= maxElo) {
                    allCount++;
                    if (filter.get(gnum) != 0) {
                        filteredCount++;
                    }
                }
            } else {
                //Klimmek: if lowest Elo in the Range: count them
                scid::database::uint mini = ie->GetWhiteElo();
                if ( mini > ie->GetBlackElo() ) mini = ie->GetBlackElo();
                if (mini < minElo  ||  mini >= maxElo)
                    continue;
                allCount++;
                if (filter.get(gnum) != 0) {
                    filteredCount++;
                }
            }
        }
    } else if ( moveMode ) {
        //Klimmek: count games with x Moves minMove=NumberHalfmove and maxMove Numberhalfmove+1
        for (scid::database::uint gnum=0, n = dbase->numGames(); gnum < n; gnum++) {
            const scid::database::IndexEntry* ie = dbase->getIndexEntry(gnum);
            scid::database::uint move = ie->GetNumHalfMoves();
            if (move >= minMove  &&  move <= maxMove) {
                allCount++;
                if (filter.get(gnum) != 0) {
                    filteredCount++;
                }
            }
        }
    }
    else { // datemode
        for (scid::database::uint gnum=0, n = dbase->numGames(); gnum < n; gnum++) {
            const scid::database::IndexEntry* ie = dbase->getIndexEntry(gnum);
            scid::database::dateT date = ie->GetDate();
            if (date >= startDate  &&  date <= endDate) {
                allCount++;
                if (filter.get(gnum) != 0) {
                    filteredCount++;
                }
            }
        }
    }
    appendUintElement (ti, filteredCount);
    appendUintElement (ti, allCount);
    return TCL_OK;
}

//TODO:
//This functions do not works because they do not specify the base, the filter and the sort criteria
//So for the moment we assume base=db, filter=dbFilter and sort=N+

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_filter_first:
//    Returns the game number of the first game in the filter,
//    or 0 if the filter is empty.
int
sc_filter_first(ClientData, Tcl_Interp * ti, int, const char**)
{
	for (scid::database::uint gnum=0; gnum < db->numGames(); gnum++) {
		if (db->defaultFilterGet(gnum) == 0) continue;
		return setUintResult (ti, gnum +1);
	}
	return setUintResult (ti, 0);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_filter_last:
//    Returns the game number of the last game in the filter,
//    or 0 if the filter is empty.
int
sc_filter_last(ClientData, Tcl_Interp * ti, int, const char**)
{
	long gnum = db->numGames();
	for (gnum--; gnum >= 0; gnum--) {
		if (db->defaultFilterGet(gnum) == 0) continue;
		return setUintResult (ti, gnum +1);
	}
	return setUintResult (ti, 0);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_filter_next:
//    Returns number of next game in the filter.
int
sc_filter_next(ClientData, Tcl_Interp * ti, int, const char**)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (db->isOpen()) {
        auto loadedGameId = editor.loadedGameId();
        scid::database::uint nextNumber = loadedGameId ? *loadedGameId + 1 : 0;
        while (nextNumber < db->numGames()) {
            if (db->defaultFilterGet(nextNumber) > 0) {
                return setUintResult (ti, nextNumber + 1);
            }
            nextNumber++;
        }
    }
    return setUintResult (ti, 0);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_filter_prev:
//    Returns number of previous game in the filter.
int
sc_filter_prev(ClientData, Tcl_Interp * ti, int, const char**)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (db->isOpen()) {
        int prevNumber = editor.loadedGameId()
                             ? static_cast<int>(*editor.loadedGameId()) - 1
                             : -1;
        while (prevNumber >= 0) {
            if (db->defaultFilterGet(prevNumber) > 0) {
                return setIntResult (ti, prevNumber + 1);
            }
            prevNumber--;
        }
    }
    return setUintResult (ti, 0);
}

//END TODO

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_filter_stats:
//    Returns statistics about the filter.
int
sc_filter_stats (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    enum {STATS_ALL, STATS_ELO, STATS_YEAR};

    if (argc < 2 || argc > 5) {
        return errorResult (ti, "Usage: sc_filter stats [all | elo <xx> | year <xx>]");
    }
    int statType = STATS_ALL;
    scid::database::uint min = 0;
    scid::database::uint max = 0;
    scid::database::uint inv_max = 0;
    if (argc > 2) {
        if (argv[2][0] == 'e') { statType = STATS_ELO; }
        if (argv[2][0] == 'y') { statType = STATS_YEAR; }
    }
    if (statType == STATS_ELO  ||  statType == STATS_YEAR) {
        if (argc < 4) {
            return errorResult (ti, "Incorrect number of parameters.");
        }
        min = scid::database::strGetUnsigned (argv[3]);
        max = scid::database::strGetUnsigned (argv[4]);
        //Klimmek: +10000 workaround to trigger max elo in filter function
        if ( max > 10000 ) {
            max -= 10000;
            inv_max = 1;
        }
    }
    scid::database::uint results[4] = {0, 0, 0, 0};
    scid::database::uint total = 0;
    const scid::database::HFilter filter = db->getFilter("dbfilter");
    for (scid::database::uint i=0, n = db->numGames(); i < n; i++) {
        const scid::database::IndexEntry* ie = db->getIndexEntry(i);
        if (filter.get(i)) {
            if ( max == 0 ) { //Old Statistic :
                if (statType == STATS_ELO  &&
                    (ie->GetWhiteElo() < min  ||  ie->GetBlackElo() < min)) {
                    continue;
                }
                if (statType == STATS_YEAR
                    &&  scid::database::date_GetYear(ie->GetDate()) < min) {
                    continue;
                }
            } else { //Klimmek:  new statistic: evaluation in intervals
                //count all games where player with highest Elo is in the specific range
                if (statType == STATS_ELO ) {
                    if (inv_max) {
                        scid::database::uint maxi = ie->GetWhiteElo();
                        if ( maxi < ie->GetBlackElo() ) maxi = ie->GetBlackElo();
                        if (maxi < min  ||  maxi >= max)
                            continue;
                    }
                    else {
                //count all games where player with lowest Elo is in the specific range
                        scid::database::uint mini = ie->GetWhiteElo();
                        if ( mini > ie->GetBlackElo() ) mini = ie->GetBlackElo();
                        if (mini < min  ||  mini >= max)
                            continue;
                    }
                }
                if (statType == STATS_YEAR
                    &&  ( scid::database::date_GetYear(ie->GetDate()) < min || scid::database::date_GetYear(ie->GetDate()) >= max) ) {
                    continue;
                }
            }
            results[ie->GetResult()]++;
            total++;
        }
    }
    char temp[80];
    scid::database::uint percentScore = results[scid::database::RESULT_White] * 2 + results[scid::database::RESULT_Draw] +
        results[scid::database::RESULT_None];
    percentScore = total ? percentScore * 500 / total : 0;
    std::snprintf(temp, sizeof(temp), "%7u %7u %7u %7u   %3u%c%u%%",
             total,
             results[scid::database::RESULT_White],
             results[scid::database::RESULT_Draw],
             results[scid::database::RESULT_Black],
             percentScore / 10, decimalPointChar, percentScore % 10);
    AppendResult (ti, temp, NULL);
    return TCL_OK;
}

} // namespace

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_filter: filter commands.  Valid minor commands:
//    count:     returns the number of games in the filter.
//    reset:     resets the filter so all games are included.
//    remove:    removes game number <x> from the filter.
//    stats:     prints filter statistics.
int
sc_filter_old(ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    int index = -1;
    static const char * options [] = {
        "count", "first", "frequency",
        "last", "negate", "next",
        "previous", "stats",
        "search", "release",
        "treestats", "export", "copy", "and", "or", "new", NULL
    };
    enum {
        FILTER_COUNT, FILTER_FIRST, FILTER_FREQ,
        FILTER_LAST, FILTER_NEGATE, FILTER_NEXT,
        FILTER_PREV, FILTER_STATS,
        FILTER_SEARCH, FILTER_RELEASE,
        FILTER_TREESTATS, FILTER_EXPORT, FILTER_COPY, FILTER_AND, FILTER_OR, FILTER_NEW
    };

    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    switch (index) {
    case FILTER_COUNT:
        if (argc == 2) {
            size_t res = db->getFilter("dbfilter")->size();
            return UI_Result(ti, scid::database::OK, res);
        }
        break;

    case FILTER_NEW:
        if (argc == 3) {
            if (auto dbase = DBasePool::getBase(scid::database::strGetUnsigned(argv[2])))
                return UI_Result(ti, scid::database::OK, dbase->newFilter());

            return UI_Result(ti, scid::database::ERROR_BadArg, "sc_filter: invalid baseId");
        }
        return UI_Result(ti, scid::database::ERROR_BadArg, "Usage: sc_filter new baseId");

    case FILTER_FIRST:
        return sc_filter_first (cd, ti, argc, argv);

    case FILTER_LAST:
        return sc_filter_last (cd, ti, argc, argv);

    case FILTER_NEXT:
        return sc_filter_next (cd, ti, argc, argv);

    case FILTER_PREV:
        return sc_filter_prev (cd, ti, argc, argv);

    case FILTER_STATS:
        return sc_filter_stats (cd, ti, argc, argv);

    }

    if (argc < 4) return errorResult (ti, "Usage: sc_filter <cmd> baseId filterName");
    auto dbase = DBasePool::getBase(scid::database::strGetUnsigned(argv[2]));
    if (dbase == NULL) return errorResult (ti, "sc_filter: invalid baseId");
    scid::database::HFilter filter = scidup::app::tree::resolveFilter(*dbase, argv[3]);
    if (filter == 0) return errorResult (ti, "sc_filter: invalid filterName");
    switch (index) {
    case FILTER_AND:
        if (argc == 5) {
            const scid::database::HFilter f = scidup::app::tree::resolveFilter(*dbase, argv[4]);
            if (f != 0) {
                for (scid::database::uint i=0, n = dbase->numGames(); i < n; i++) {
                    if (filter.get(i) != 0 && f.get(i) == 0) filter.set(i, 0);
                }
                return UI_Result(ti, scid::database::OK);
            }
        }
        return errorResult (ti, "Usage: sc_filter and baseId filterName filterAnd");

    case FILTER_OR:
        if (argc == 5) {
            const scid::database::HFilter f = scidup::app::tree::resolveFilter(*dbase, argv[4]);
            if (f != 0) {
                for (scid::database::uint i=0, n = dbase->numGames(); i < n; i++) {
                    if (filter.get(i) == 0) filter.set(i, f.get(i));
                }
                return UI_Result(ti, scid::database::OK);
            }
        }
        return errorResult (ti, "Usage: sc_filter or baseId filterName filterOr");

    case FILTER_COPY:
        if (argc == 5) {
            const scid::database::HFilter f = scidup::app::tree::resolveFilter(*dbase, argv[4]);
            if (f != 0) {
                for (scid::database::uint i=0, n = dbase->numGames(); i < n; i++) {
                    filter.set(i, f.get(i));
                }
                return UI_Result(ti, scid::database::OK);
            }
        }
        return errorResult (ti, "Usage: sc_filter copy baseId filterTo filterFrom");

    case FILTER_FREQ:
        return sc_filter_freq (dbase, filter, ti, argc, argv);

    case FILTER_NEGATE:
        for (scid::database::uint i=0, n = dbase->numGames(); i < n; i++) {
            filter.set(i, ! filter.get(i) );
        }
        return UI_Result(ti, scid::database::OK);

    case FILTER_COUNT:
        return UI_Result(ti, scid::database::OK, filter->size());

    case FILTER_RELEASE:
        dbase->deleteFilter(argv[3]);
        return TCL_OK;

    case FILTER_SEARCH:
        if (argc >= 5) {
            std::string_view subcmd = argv[4];
            if (subcmd == "header")
                return sc_search_header (cd, ti, dbase, filter, argc -3, argv +3);

            if (subcmd == "board") {
                if (argc == 5 || argc == 6) {
                    // sc_filter search baseId filterTo board [filterFrom]
                    bool useCache = (argc == 5);

                    auto editor = scidup::app::editor::gameSession(*db);
                    auto pos = currentPosition(editor.game());
                    if (!pos)
                        return UI_Result(ti, scid::database::ERROR, "Error reading position.");
                    if (useCache &&
                        scidup::app::tree::session(*dbase).cacheRestore(*pos))
                        return UI_Result(ti, scid::database::OK);

                    if (!scid::database::SearchPos(*pos).setFilter(
                            *dbase, filter, UI_CreateProgress(ti)))
                        return UI_Result(ti, scid::database::ERROR_UserCancel);

                    if (useCache)
                        scidup::app::tree::session(*dbase).cacheAdd(*pos);

                    return UI_Result(ti, scid::database::OK);
                }

                if (argc == 10) {
                    const char* args[6] = {argv[3], argv[4], argv[9],
                                           argv[5], argv[6], argv[7]};
                    return sc_search_board(ti, dbase, filter, 6, args);
                }
            }
        }
        return errorResult (ti, "Usage: sc_filter search baseId filterName <header|board|tags> [args]");

    case FILTER_TREESTATS: {
            const auto stats = dbase->getTreeStat(filter);
            UI_List res (stats.size());
            UI_List ginfo(9);
            for (auto const& node : stats) {
                ginfo.clear();
                ginfo.push_back(node.move ? node.move.getSAN() : "[end]");
                ginfo.push_back(node.freq[0]);
                ginfo.push_back(node.freq[scid::database::RESULT_White]);
                ginfo.push_back(node.freq[scid::database::RESULT_Draw]);
                ginfo.push_back(node.freq[scid::database::RESULT_Black]);
                ginfo.push_back(node.avgElo());
                ginfo.push_back(node.eloPerformance());
                ginfo.push_back(node.eloCount);
                ginfo.push_back(node.move.getColor() == scid::database::WHITE ? "W" : "B");
                res.push_back(ginfo);
            }
            return UI_Result(ti, scid::database::OK, res);
        }

    case FILTER_EXPORT:
        if (argc >= 7 && argc <=9) {
            const auto tcl_strings_are_utf8 =
                std::filesystem::path((const char8_t*)argv[5]).string();
            const auto exportFileName = tcl_strings_are_utf8.c_str();
            auto exportFile = fopen(exportFileName, "wb");
            if (exportFile == NULL) return errorResult (ti, "Error opening file for exporting games.");
            auto old_language = scid::database::language;
            scid::database::Game g;
            auto encodeOptions = scid::database::defaultLegacyGameEncodeOptions();
            if (scid::database::strCompare("LaTeX", argv[6]) == 0) {
                encodeOptions.legacyFormat = scid::database::PGN_FORMAT_LaTeX;
                encodeOptions.style = PGN_STYLE_TAGS | PGN_STYLE_COMMENTS |
                    PGN_STYLE_VARS | PGN_STYLE_SHORT_HEADER | PGN_STYLE_SYMBOLS |
                    PGN_STYLE_INDENT_VARS;
            } else { //Default to PGN
                encodeOptions.legacyFormat = scid::database::PGN_FORMAT_Plain;
                encodeOptions.style = PGN_STYLE_TAGS | PGN_STYLE_COMMENTS |
                    PGN_STYLE_VARS;
                scid::database::language = 0;
            }
            if (argc > 7) fprintf(exportFile, "%s", argv[7]);
            scid::database::Progress progress = UI_CreateProgress(ti);
            size_t count = filter->size();
            scid::database::gamenumT* idxList = new scid::database::gamenumT[count];
            count = dbase->listGames(argv[4], 0, count, filter, idxList);
            scid::database::errorT err = scid::database::OK;
                for (size_t i = 0; i < count; ++i) {
                    const scid::database::IndexEntry* ie = dbase->getIndexEntry(idxList[i]);
                    // Skip any corrupt games:
                    if (dbase->getGame(*ie, g) != scid::database::OK) continue;

                    std::pair<const char*, unsigned> pgn =
                        scid::database::legacy_pgn::encode(g, encodeOptions,
                                                           75, true);
                    if (pgn.second != fwrite(pgn.first, 1, pgn.second, exportFile)) {
                        err = scid::database::ERROR_FileWrite;
                        break;
                    }
                    if ((i % 1024 == 0) && !progress.report(i, count)) {
                        err = scid::database::ERROR_UserCancel;
                        break;
                    }
                }
            if (err == scid::database::OK && argc > 8)
                fprintf(exportFile, "%s", argv[8]);
            scid::database::language = old_language;
            fclose (exportFile);
            delete[] idxList;
            return UI_Result(ti, err);
        }
        return errorResult (ti, "Usage: sc_filter export baseId filterName sortCrit filename <PGN|LaTeX> [header] [footer]");

	}
    return InvalidCommand (ti, "sc_filter", options);
}

//////////////////////////////////////////////////////////////////////
///  GAME functions

int
sc_game (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * options [] = {
        "altered",    "crosstable", "eco",
        "find",       "firstMoves", "import",
        "info",        "load",      "merge",      "moves",
        "new",        "novelty",    "number",     "pgn",
        "pop",        "push",       "SANtoUCI",   "save",
        "startBoard", "strip",
        "tags",       "truncate",   "variant", "UCI_currentPos",
        "undo",       "undoAll",    "undoPoint",  "redo",       NULL
    };
    enum {
        GAME_ALTERED,    GAME_CROSSTABLE, GAME_ECO,
        GAME_FIND,       GAME_FIRSTMOVES, GAME_IMPORT,
        GAME_INFO,       GAME_LOAD,       GAME_MERGE,      GAME_MOVES,
        GAME_NEW,        GAME_NOVELTY,    GAME_NUMBER,     GAME_PGN,
        GAME_POP,        GAME_PUSH,       GAME_SANTOUCI,   GAME_SAVE,
        GAME_STARTBOARD, GAME_STRIP,
        GAME_TAGS,       GAME_TRUNCATE,   GAME_VARIANT, GAME_UCI_CURRENTPOS,
        GAME_UNDO,       GAME_UNDO_ALL,   GAME_UNDO_POINT, GAME_REDO
    };
    int index = -1;
    char old_language = 0;
    auto editor = scidup::app::editor::gameSession(*db);

    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options);}

    switch (index) {
    case GAME_ALTERED:
        return UI_Result(ti, scid::database::OK, editor.isDirty());

    case GAME_CROSSTABLE:
        return sc_game_crosstable (cd, ti, argc, argv);

    case GAME_ECO:  // "sc_game eco" is equivalent to "sc_eco game"
        return sc_eco_game (cd, ti, argc, argv);

    case GAME_FIND:
        return sc_game_find (cd, ti, argc, argv);

    case GAME_FIRSTMOVES:
        return sc_game_firstMoves (cd, ti, argc, argv);

    case GAME_IMPORT:
        return sc_game_import (cd, ti, argc, argv);

    case GAME_INFO:
        return sc_game_info (cd, ti, argc, argv);

    case GAME_LOAD:
        return sc_game_load (cd, ti, argc, argv);

    case GAME_MERGE:
        return sc_game_merge (cd, ti, argc, argv);

    case GAME_MOVES:
        return sc_game_moves (cd, ti, argc, argv);

    case GAME_NEW:
        editor.clearHistory();
        return sc_game_new (cd, ti, argc, argv);

    case GAME_NOVELTY:
        return sc_game_novelty (cd, ti, argc, argv);

    case GAME_NUMBER:
        return setUintResult(
            ti, editor.loadedGameId() ? *editor.loadedGameId() + 1 : 0);

    case GAME_PGN:
        return sc_game_pgn (cd, ti, argc, argv);

    case GAME_POP:
        return sc_game_pop (cd, ti, argc, argv);

    case GAME_PUSH:
        return sc_game_push (cd, ti, argc, argv);

    case GAME_SANTOUCI:
        if (argc == 3) {
            auto pos = currentPosition(editor.game());
            if (!pos)
                return UI_Result(ti, scid::database::ERROR, "Error reading position.");
            scid::database::simpleMoveT sm;
            auto end = argv[2] + std::strlen(argv[2]);
            if (auto err = pos->ParseMove(&sm, argv[2], end))
                return UI_Result(ti, err);

            char buf[scid::database::UCI_MOVE_STRING_SIZE] = {};
            sm.toLongNotation(buf);
            return UI_Result(ti, scid::database::OK, buf);
        }
        return errorResult(ti, "usage sc_game SANtoUCI move");

    case GAME_SAVE:
        return sc_game_save (cd, ti, argc, argv);

    case GAME_STARTBOARD:
        return sc_game_startBoard (cd, ti, argc, argv);

    case GAME_STRIP:
        return sc_game_strip (cd, ti, argc, argv);

    case GAME_TAGS:
        return sc_game_tags (cd, ti, argc, argv);

    case GAME_TRUNCATE:
        old_language = scid::database::language;
        scid::database::language = 0;
        if (argc > 2 && scid::database::strIsPrefix (argv[2], "-start")) {
            // "sc_game truncate -start" truncates the moves up to the
            // current position:
            editor.game().truncateStart();
        } else {
            // Remove moves from the current position to the end of the game.
            editor.game().truncate();
        }
        editor.setDirty();
        scid::database::language = old_language;
        break;

    case GAME_VARIANT:
        if (auto pos = currentPosition(editor.game())) {
            return UI_Result(ti, scid::database::OK,
                             pos->isChess960() ? "chess960" : "standard");
        }
        return UI_Result(ti, scid::database::ERROR, "Error reading position.");

    case GAME_UCI_CURRENTPOS:
        return UI_Result(ti, scid::database::OK,
                         scid::core::notation::currentPositionUci(
                             editor.game().coreGame(),
                             editor.game().coreLocation()));

    case GAME_UNDO:
        if (argc > 2 && scid::database::strCompare("size", argv[2]) == 0) {
            return UI_Result(ti, scid::database::OK, static_cast<scid::database::uint>(editor.undoSize()));
        }
        editor.undo();
        return UI_Result(ti, scid::database::OK);

    case GAME_UNDO_ALL:
        return UI_Result(ti, editor.undoAll());

    case GAME_UNDO_POINT:
        editor.storeUndoPoint();
        break;

    case GAME_REDO:
        if (argc > 2 && scid::database::strCompare("size", argv[2]) == 0) {
            return UI_Result(ti, scid::database::OK, static_cast<scid::database::uint>(editor.redoSize()));
        }
        editor.redo();
        return UI_Result(ti, scid::database::OK);

    default:
        return InvalidCommand (ti, "sc_game", options);
    }

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// isCrosstableGame:
//    Returns true if the game with the specified index entry
//    is considered a crosstable game. It must have the specified
//    Event and Site, and a Date within the specified range or
//    have the specified non-zero EventDate.
static inline bool
isCrosstableGame (const scid::database::IndexEntry* ie, scid::database::idNumberT siteID, scid::database::idNumberT eventID,
                  scid::database::dateT eventDate)
{
    if (ie->GetSite() != siteID  ||  ie->GetEvent() != eventID) {
        return false;
    }
    scid::database::dateT EventDate = ie->GetEventDate();
    if (eventDate != 0  && EventDate != 0 && EventDate != eventDate) {
        return false;
    }
    return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_crosstable:
//    Returns the crosstable for the current game.
int
sc_game_crosstable (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
#ifndef WINCE
    static const char * options [] = {
        "plain", "html", "hypertext", "latex", "filter", "count", NULL
    };
    enum {
        OPT_PLAIN, OPT_HTML, OPT_HYPERTEXT, OPT_LATEX, OPT_FILTER, OPT_COUNT
    };
    int option = -1;

    const char * usageMsg =
        "Usage: sc_game crosstable plain|html|hypertext|filter|count [name|rating|score|country] [allplay|swiss] [(+|-)(colors|countries|tallies|ratings|titles|groups|breaks|numcolumns)]";

    static const char * extraOptions [] = {
        "allplay", "knockout", "swiss", "auto",
        "name", "rating", "score", "country",
        "-ages", "+ages",               // Show player ages
        "-breaks", "+breaks",           // Show tiebreak scores
        "-colors", "+colors",           // Show game colors in Swiss table
        "-countries", "+countries",     // Show current countries
        "-flags", "+flags",             // Show flags
        "-tallies", "+tallies",
        "-ratings", "+ratings",         // Show Elo ratings
        "-titles", "+titles",           // Show FIDE titles
        "-groups", "+groups",           // Separate players into score groups
        "-deleted", "+deleted",         // Include deleted games in table
        "-numcolumns", "+numcolumns",   // All-play-all numbered columns
        "-gameNumber",
        "-threewin", "+threewin",       // Give 3 points for win, 1 for draw
        NULL
    };
    enum {
        EOPT_ALLPLAY, EOPT_KNOCKOUT, EOPT_SWISS, EOPT_AUTO,
        EOPT_SORT_NAME, EOPT_SORT_RATING, EOPT_SORT_SCORE, EOPT_SORT_COUNTRY,
        EOPT_AGES_OFF, EOPT_AGES_ON,
        EOPT_BREAKS_OFF, EOPT_BREAKS_ON,
        EOPT_COLORS_OFF, EOPT_COLORS_ON,
        EOPT_COUNTRIES_OFF, EOPT_COUNTRIES_ON,
        EOPT_FLAGS_OFF, EOPT_FLAGS_ON,
        EOPT_TALLIES_OFF, EOPT_TALLIES_ON,
        EOPT_RATINGS_OFF, EOPT_RATINGS_ON,
        EOPT_TITLES_OFF, EOPT_TITLES_ON,
        EOPT_GROUPS_OFF, EOPT_GROUPS_ON,
        EOPT_DELETED_OFF, EOPT_DELETED_ON,
        EOPT_NUMCOLUMNS_OFF, EOPT_NUMCOLUMNS_ON,
        EOPT_GNUMBER,
        EOPT_THREEWIN_OFF, EOPT_THREEWIN_ON
    };

    int sort = EOPT_SORT_SCORE;
    crosstableModeT mode = CROSSTABLE_AllPlayAll;
    bool showAges = true;
    bool showColors = true;
    bool showCountries = true;
    bool showFlags = true;
    bool showTallies = true;
    bool showRatings = true;
    bool showTitles = true;
    bool showBreaks = false;
    bool scoreGroups = false;
    bool useDeletedGames = false;
    bool numColumns = false;  // Numbers for columns in all-play-all table
    scid::database::uint numTableGames = 0;
    scid::database::uint gameNumber = 0;
    bool threewin = false;

    if (argc >= 3) { option = scid::database::strUniqueMatch (argv[2], options); }
    if (option < 0) { return errorResult (ti, usageMsg); }

    for (int arg=3; arg < argc; arg++) {
        int extraOption = scid::database::strUniqueMatch (argv[arg], extraOptions);
        switch (extraOption) {
            case EOPT_ALLPLAY:        mode = CROSSTABLE_AllPlayAll; break;
            case EOPT_KNOCKOUT:       mode = CROSSTABLE_Knockout;   break;
            case EOPT_SWISS:          mode = CROSSTABLE_Swiss;      break;
            case EOPT_AUTO:           mode = CROSSTABLE_Auto;       break;
            case EOPT_SORT_NAME:      sort = EOPT_SORT_NAME;   break;
            case EOPT_SORT_RATING:    sort = EOPT_SORT_RATING; break;
            case EOPT_SORT_SCORE:     sort = EOPT_SORT_SCORE;  break;
            case EOPT_SORT_COUNTRY:   sort = EOPT_SORT_COUNTRY;  break;
            case EOPT_AGES_OFF:       showAges = false;        break;
            case EOPT_AGES_ON:        showAges = true;         break;
            case EOPT_BREAKS_OFF:     showBreaks = false;      break;
            case EOPT_BREAKS_ON:      showBreaks = true;       break;
            case EOPT_COLORS_OFF:     showColors = false;      break;
            case EOPT_COLORS_ON:      showColors = true;       break;
            case EOPT_COUNTRIES_OFF:  showCountries = false;   break;
            case EOPT_COUNTRIES_ON:   showCountries = true;    break;
            case EOPT_FLAGS_OFF:      showFlags = false;       break;
            case EOPT_FLAGS_ON:       showFlags = true;        break;
            case EOPT_TALLIES_OFF:    showTallies = false;     break;
            case EOPT_TALLIES_ON:     showTallies = true;      break;
            case EOPT_RATINGS_OFF:    showRatings = false;     break;
            case EOPT_RATINGS_ON:     showRatings = true;      break;
            case EOPT_TITLES_OFF:     showTitles = false;      break;
            case EOPT_TITLES_ON:      showTitles = true;       break;
            case EOPT_GROUPS_OFF:     scoreGroups = false;     break;
            case EOPT_GROUPS_ON:      scoreGroups = true;      break;
            case EOPT_DELETED_OFF:    useDeletedGames = false; break;
            case EOPT_DELETED_ON:     useDeletedGames = true;  break;
            case EOPT_NUMCOLUMNS_OFF: numColumns = false;      break;
            case EOPT_NUMCOLUMNS_ON:  numColumns = true;       break;
            case EOPT_GNUMBER:
                // scid::database::Game number to print the crosstable for is
                // given in the next argument:
                if (arg+1 >= argc) { return errorResult (ti, usageMsg); }
                gameNumber = scid::database::strGetUnsigned (argv[arg+1]);
                arg++;
                break;
            case EOPT_THREEWIN_OFF:  threewin = false ; break;
            case EOPT_THREEWIN_ON:   threewin = true  ; break;
            default: return errorResult (ti, usageMsg);
        }
    }
    if (!db->isOpen()) { return TCL_OK; }

    const char * newlineStr = "";
    switch (option) {
        case OPT_PLAIN:     newlineStr = "\n";     break;
        case OPT_HTML:      newlineStr = "<br>\n"; break;
        case OPT_HYPERTEXT: newlineStr = "<br>";   break;
        case OPT_LATEX:     newlineStr = "\\\\\n"; break;
    }

    // Load crosstable game if necessary:
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game * g = &editor.game();
    if (gameNumber > 0) {
        g = scratchGame;
        g->clear();
        if (gameNumber > db->numGames()) {
            return setResult (ti, "Invalid game number");
        }
        const scid::database::IndexEntry* ie = db->getIndexEntry(gameNumber - 1);
        if (ie->GetLength() == 0) {
            return errorResult (ti, "Error: empty game file record.");
        }
        if (db->getGame(*ie, *g) != scid::database::OK) {
            return errorResult (ti, "Error reading game file.");
        }
    }

    scid::database::idNumberT eventId = 0, siteId = 0;
    if (db->getNameBase()->FindExactName(
            scid::database::NAME_EVENT, g->coreGame().event().c_str(),
            &eventId) != scid::database::OK) {
        return TCL_OK;
    }
    if (db->getNameBase()->FindExactName (
            scid::database::NAME_SITE, g->coreGame().site().c_str(), &siteId) != scid::database::OK) {
        return TCL_OK;
    }

    scid::database::dateT eventDate = g->coreGame().eventDate();
    scid::database::dateT firstSeenDate = g->coreGame().date();
    scid::database::dateT lastSeenDate = g->coreGame().date();

    Crosstable * ctable = new Crosstable;
    if (sort == EOPT_SORT_NAME) { ctable->SortByName(); }
    if (sort == EOPT_SORT_RATING) { ctable->SortByElo(); }
    if (sort == EOPT_SORT_COUNTRY) { ctable->SortByCountry(); }

    ctable->SetThreeWin(threewin);
    ctable->SetSwissColors (showColors);
    ctable->SetAges (showAges);
    ctable->SetCountries (showCountries);
    ctable->SetFlags (showFlags);
    ctable->SetTallies (showTallies);
    ctable->SetElos (showRatings);
    ctable->SetTitles (showTitles);
    ctable->SetTiebreaks (showBreaks);
    ctable->SetSeparateScoreGroups (scoreGroups);
    ctable->SetDecimalPointChar (decimalPointChar);
    ctable->SetNumberedColumns (numColumns);

    switch (option) {
        case OPT_PLAIN:     ctable->SetPlainOutput();     break;
        case OPT_HTML:      ctable->SetHtmlOutput();      break;
        case OPT_HYPERTEXT: ctable->SetHypertextOutput(); break;
        case OPT_LATEX:     ctable->SetLaTeXOutput();     break;
    }

    // Find all games that should be listed in the crosstable:
    const scidup::spelling::SpellChecker* spell = spellChk.get();
    bool tableFullMessage = false;
    for (scid::database::uint i=0, n = db->numGames(); i < n; i++) {
        const scid::database::IndexEntry* ie = db->getIndexEntry(i);
        if (ie->GetDeleteFlag()  &&  !useDeletedGames) { continue; }
        if (! isCrosstableGame (ie, siteId, eventId, eventDate)) {
            continue;
        }
        scid::database::idNumberT whiteId = ie->GetWhite();
        const char * whiteName = db->getNameBase()->GetName (scid::database::NAME_PLAYER, whiteId);
        scid::database::idNumberT blackId = ie->GetBlack();
        const char * blackName = db->getNameBase()->GetName (scid::database::NAME_PLAYER, blackId);

        // Ensure we have two different players:
        if (whiteId == blackId) { continue; }

        // If option is OPT_FILTER, adjust the filter and continue &&&
        if (option == OPT_FILTER) {
            db->defaultFilterSet(i, 1);
            continue;
        }

        // If option is OPT_COUNT, increment game count and continue:
        if (option == OPT_COUNT) {
            numTableGames++;
            continue;
        }

        // Add the two players to the crosstable:
        if (ctable->AddPlayer (whiteId, whiteName, ie->GetWhiteElo(), spell) != scid::database::OK  ||
            ctable->AddPlayer (blackId, blackName, ie->GetBlackElo(), spell) != scid::database::OK)
        {
            if (! tableFullMessage) {
                tableFullMessage = true;
                AppendResult (ti, "Warning: Player limit reached; table is incomplete\n\n", NULL);
            }
            continue;
        }

        scid::database::uint round = scid::database::strGetUnsigned (db->getNameBase()->GetName (scid::database::NAME_ROUND, ie->GetRound()));
        scid::database::dateT date = ie->GetDate();
        scid::database::resultT result = ie->GetResult();
        ctable->AddResult (i+1, whiteId, blackId, result, round, date);
        if (date < firstSeenDate) { firstSeenDate = date; }
        if (date > lastSeenDate) { lastSeenDate = date; }
    }

    if (option == OPT_COUNT) {
        // Just return a count of the number of tournament games:
        delete ctable;
        return setUintResult (ti, numTableGames);
    }
    if (option == OPT_FILTER) {
        delete ctable;
        return TCL_OK;
    }
    if (ctable->NumPlayers() < 2) {
        delete ctable;
        return setResult (ti, "No crosstable for this game.");
    }

    if (option == OPT_LATEX) {
        AppendResult (ti, "\\documentclass[10pt,a4paper]{article}\n\n",
                          "\\usepackage{a4wide}\n\n",
                          "\\begin{document}\n\n",
                          "\\setlength{\\parindent}{0cm}\n",
                          "\\setlength{\\parskip}{0.5ex}\n",
                          "\\small\n", NULL);
    }

    if (mode == CROSSTABLE_Auto) { mode = ctable->BestMode(); }

    // Limit all-play-all tables to 300 players:
    scid::database::uint apaLimit = 300;
    if (mode == CROSSTABLE_AllPlayAll  &&
            ctable->NumPlayers() > apaLimit  &&
            !tableFullMessage) {
        AppendResult (ti, "Warning: Too many players for all-play-all; try displaying as a swiss tournament.\n\n", NULL);
    }

    char stemp[1000];
    std::snprintf(stemp, sizeof(stemp), "%s%s%s, ",
                  g->coreGame().event().c_str(), newlineStr,
                  g->coreGame().site().c_str());
    AppendResult (ti, stemp, NULL);
    scid::database::date_DecodeToString (firstSeenDate, stemp);
    scid::database::strTrimDate (stemp);
    AppendResult (ti, stemp, NULL);
    if (lastSeenDate != firstSeenDate) {
        scid::database::date_DecodeToString (lastSeenDate, stemp);
        scid::database::strTrimDate (stemp);
        AppendResult (ti, " - ", stemp, NULL);
    }
    AppendResult (ti, newlineStr, NULL);

    scid::database::ratingT avgElo = ctable->AvgRating();
    if (avgElo > 0  &&  showRatings) {
        AppendResult (ti, translate (ti, "AverageRating", "Average Rating"),
                          ": ", NULL);
        appendUintResult (ti, avgElo);
        scid::database::uint category = ctable->FideCategory (avgElo);
        if (category > 0  &&  mode == CROSSTABLE_AllPlayAll) {
            std::snprintf(stemp, sizeof(stemp), "  (%s %u)",
                     translate (ti, "Category", "Category"), category);
            AppendResult (ti, stemp, NULL);
        }
        AppendResult (ti, newlineStr, NULL);
    }

    scid::database::DString * dstr = new scid::database::DString;
    if (mode != CROSSTABLE_AllPlayAll) { apaLimit = 0; }
    auto currentEditor = scidup::app::editor::gameSession(*db);
    ctable->PrintTable(
        dstr, mode, apaLimit,
        currentEditor.loadedGameId() ? *currentEditor.loadedGameId() + 1 : 0);

    AppendResult (ti, dstr->Data(), NULL);
    if (option == OPT_LATEX) {
        AppendResult (ti, "\n\\end{document}\n", NULL);
    }
    delete ctable;
    delete dstr;
#endif
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_find:
//    Returns the game number of the game in that current database
//    that best matches the specified number, player names, site,
//    round, year and result.
//    This command is used primarily to locate a bookmarked game in
//    a database where the number may be inaccurate due to database
//    sorting or compaction.
int
sc_game_find (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 9) {
        return errorResult (ti, "sc_game_find: Incorrect parameters");
    }

    scid::database::uint gnum = scid::database::strGetUnsigned (argv[2]);
    if (gnum == 0) { return setUintResult (ti, 0); }
    gnum--;
    const char * whiteStr = argv[3];
    const char * blackStr = argv[4];
    const char * siteStr = argv[5];
    const char * roundStr = argv[6];
    scid::database::uint year = scid::database::strGetUnsigned(argv[7]);
    scid::database::resultT result = scid::database::strGetResult (argv[8]);

    scid::database::idNumberT white, black, site, round;
    white = black = site = round = 0;
    db->getNameBase()->FindExactName (scid::database::NAME_PLAYER, whiteStr, &white);
    db->getNameBase()->FindExactName (scid::database::NAME_PLAYER, blackStr, &black);
    db->getNameBase()->FindExactName (scid::database::NAME_SITE, siteStr, &site);
    db->getNameBase()->FindExactName (scid::database::NAME_ROUND, roundStr, &round);

    // We give each game a "score" which is 1 for each matching field.
    // So the best possible score is 6.

    // First, check if the specified game number matches all fields:
    if (db->numGames() > gnum) {
        scid::database::uint score = 0;
        const scid::database::IndexEntry* ie = db->getIndexEntry(gnum);
        if (ie->GetWhite() == white) { score++; }
        if (ie->GetBlack() == black) { score++; }
        if (ie->GetSite() == site) { score++; }
        if (ie->GetRound() == round) { score++; }
        if (ie->GetYear() == year) { score++; }
        if (ie->GetResult() == result) { score++; }
        if (score == 6) { return setUintResult (ti, gnum+1); }
    }

    // Now look for the best matching game:
    scid::database::uint bestNum = 0;
    scid::database::uint bestScore = 0;

    for (scid::database::uint i=0, n = db->numGames(); i < n; i++) {
        scid::database::uint score = 0;
        const scid::database::IndexEntry* ie = db->getIndexEntry(i);
        if (ie->GetWhite() == white) { score++; }
        if (ie->GetBlack() == black) { score++; }
        if (ie->GetSite() == site) { score++; }
        if (ie->GetRound() == round) { score++; }
        if (ie->GetYear() == year) { score++; }
        if (ie->GetResult() == result) { score++; }
        // Update if the best score, favouring the specified game number
        // in the case of a tie:
        if (score > bestScore  ||  (score == bestScore  &&  gnum == i)) {
            bestScore = score;
            bestNum = i;
        }
        // Stop now if the best possible match is found:
        if (score == 6) { break; }
    }
    return setUintResult (ti, bestNum + 1);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_firstMoves:
//    get the first few moves of the specified game as  a text line.
//    E.g., "sc_game firstMoves 4" might return "1.e4 e5 2.Nf3 Nf6"
int
sc_game_firstMoves (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_game firstMoves <numMoves>");
    }
    if (!db->isOpen()) {
        return errorResult (ti, errMsgNotOpen(ti));
    }

    int plyCount = scid::database::strGetInteger (argv[2]);
    // Check plyCount is a reasonable value, or set it to current plycount.
    auto editor = scidup::app::editor::gameSession(*db);
    if (plyCount < 0)
        plyCount = currentPly(editor.game());
    if (plyCount == 0) plyCount = 1;

    return UI_Result(
        ti, scid::database::OK,
        scid::core::notation::partialMoveList(editor.game().coreGame(),
                                              plyCount));
}

int sc_game_import(ClientData, Tcl_Interp* ti, int argc, const char** argv) {
	if (argc != 3)
		return errorResult(ti, "Usage: sc_game import <pgn-text>");

	auto editor = scidup::app::editor::gameSession(*db);
	editor.setDirty();
	bool new_variation = false;
	if (editor.game().next() == scid::database::OK) {
		new_variation = (editor.game().addVariation() == scid::database::OK);
	}

	scid::database::PgnParseLog pgn;
	auto ok = scid::database::pgnParseGame(argv[2], std::strlen(argv[2]), editor.game(), pgn);

	if (new_variation && isAtEmptyVariation(editor.game())) {
		editor.game().deleteVariation();
	}

	if (!ok && pgn.log.empty())
		return UI_Result(ti, scid::database::OK, "No PGN text found.");

	if (pgn.log.empty())
		return UI_Result(ti, scid::database::OK,
		                 "PGN text imported with no errors or warnings.");

	return UI_Result(ti, scid::database::OK,
	                 "Errors/warnings importing PGN text:\n\n" + pgn.log);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_info:
//    Return the scid::database::Game Info string for the active game.
//    The returned text includes color codes.
int
sc_game_info (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game& g = editor.game();
    const auto loadedGameId = editor.loadedGameId();
    bool hideNextMove = false;
    bool showMaterialValue = false;
    bool showFEN = false;
    scid::database::uint commentWidth = 50;
    scid::database::uint commentHeight = 1;
    bool fullComment = false;
    char temp[1024];

    int arg = 2;
    while (arg < argc) {
        if  (scid::database::strIsPrefix (argv[arg], "-hideNextMove")) {
            if (arg+1 < argc) {
                arg++;
                hideNextMove = scid::database::strGetBoolean(argv[arg]);
            }
        } else if  (scid::database::strIsPrefix (argv[arg], "-materialValue")) {
            if (arg+1 < argc) {
                arg++;
                showMaterialValue = scid::database::strGetBoolean(argv[arg]);
            }
        } else if  (scid::database::strIsPrefix (argv[arg], "-fen")) {
            if (arg+1 < argc) {
                arg++;
                showFEN = scid::database::strGetBoolean(argv[arg]);
            }
        } else if  (scid::database::strIsPrefix (argv[arg], "-cfull")) {
            // Show full comment:
            if (arg+1 < argc) {
                arg++;
                fullComment = scid::database::strGetBoolean(argv[arg]);
                if (fullComment) {
                    commentWidth = 99999;
                    commentHeight = 99999;
                }
            }
        } else if  (scid::database::strIsPrefix (argv[arg], "-cwidth")) {
            if (arg+1 < argc) {
                arg++;
                commentWidth = scid::database::strGetBoolean(argv[arg]);
            }
        } else if  (scid::database::strIsPrefix (argv[arg], "-cheight")) {
            if (arg+1 < argc) {
                arg++;
                commentHeight = scid::database::strGetBoolean(argv[arg]);
            }
        } else if (scid::database::strIsPrefix (argv[arg], "white")) {
            AppendResult (ti, g.coreGame().white().name.c_str(), NULL);
            return TCL_OK;
        } else if (scid::database::strIsPrefix (argv[arg], "welo")) {
            return setIntResult (ti, g.coreGame().white().rating.value);
        } else if (scid::database::strIsPrefix (argv[arg], "black")) {
            AppendResult (ti, g.coreGame().black().name.c_str(), NULL);
            return TCL_OK;
        } else if (scid::database::strIsPrefix (argv[arg], "belo")) {
            return setIntResult (ti, g.coreGame().black().rating.value);
        } else if (scid::database::strIsPrefix (argv[arg], "event")) {
            AppendResult (ti, g.coreGame().event().c_str(), NULL);
            return TCL_OK;
        } else if (scid::database::strIsPrefix (argv[arg], "site")) {
            AppendResult (ti, g.coreGame().site().c_str(), NULL);
            return TCL_OK;
        } else if (scid::database::strIsPrefix (argv[arg], "round")) {
            AppendResult (ti, g.coreGame().round().c_str(), NULL);
            return TCL_OK;
        } else if (scid::database::strIsPrefix (argv[arg], "date")) {
            char dateStr [12];
            scid::database::date_DecodeToString (g.coreGame().date(), dateStr);
            AppendResult (ti, dateStr, NULL);
            return TCL_OK;
        } else if (scid::database::strIsPrefix (argv[arg], "year")) {
            return setUintResult (ti, scid::database::date_GetYear(g.coreGame().date()));
        } else if (scid::database::strIsPrefix (argv[arg], "result")) {
            return setResult (ti, scid::database::RESULT_STR[g.coreGame().result()]);
        } else if (scid::database::strIsPrefix (argv[arg], "nextMove")) {
            scid::database::strCopy(
                temp,
                scid::core::notation::nextSan(g.coreGame(), g.coreLocation())
                    .c_str());
            scid::database::transPieces(temp);
            AppendResult (ti, temp, NULL);
            return TCL_OK;
// nextMoveNT is the same as nextMove, except that the move is not translated
        } else if (scid::database::strIsPrefix (argv[arg], "nextMoveNT")) {
            AppendResult(
                ti,
                scid::core::notation::nextSan(g.coreGame(), g.coreLocation())
                    .c_str(),
                NULL);
            return TCL_OK;
// returns next move played in UCI format
        } else if (scid::database::strIsPrefix (argv[arg], "nextMoveUCI")) {
          AppendResult(
              ti,
              scid::core::notation::nextMoveUci(g.coreGame(),
                                                g.coreLocation())
                  .c_str(),
              NULL);
          return TCL_OK;
        } else if (scid::database::strIsPrefix (argv[arg], "previousMove")) {
            scid::database::strCopy(
                temp, scid::core::notation::previousSan(g.coreGame(),
                                                        g.coreLocation())
                          .c_str());
            scid::database::transPieces(temp);
            AppendResult (ti, temp, NULL);
            return TCL_OK;
// previousMoveNT is the same as previousMove, except that the move is not translated
        } else if (scid::database::strIsPrefix (argv[arg], "previousMoveNT")) {
            AppendResult(
                ti,
                scid::core::notation::previousSan(g.coreGame(),
                                                  g.coreLocation())
                    .c_str(),
                NULL);
            return TCL_OK;
// returns previous move played in UCI format
        } else if (scid::database::strIsPrefix (argv[arg], "previousMoveUCI")) {
            AppendResult(
                ti,
                scid::core::notation::previousMoveUci(g.coreGame(),
                                                      g.coreLocation())
                    .c_str(),
                NULL);
            return TCL_OK;
        } else if (scid::database::strIsPrefix (argv[arg], "duplicate")) {
            scid::database::uint dupGameNum = loadedGameId ? db->getDuplicates(*loadedGameId) : 0;
            return setUintResult (ti, dupGameNum);
        } else if (scid::database::strIsPrefix(argv[arg], "ECO")) {
            std::string str;
            if (ecoBook) {
                auto pos = currentPosition(g);
                if (pos) {
                    auto ecoStr = ecoBook->findEcoString(*pos);
                    if (!ecoStr.empty())
                        str.append(ecoStr);
                }
            }
            return UI_Result(ti, scid::database::OK, str);
        }
        arg++;
    }

    const char * gameStr = translate (ti, "game");
    std::snprintf(temp, sizeof(temp), "%c%s %u:  <pi %s>%s</pi>", toupper(gameStr[0]),
             gameStr + 1, loadedGameId ? *loadedGameId + 1 : 0,
             g.coreGame().white().name.c_str(), g.coreGame().white().name.c_str());
    if (auto whCountry = g.coreGame().findExtraTag("WhiteCountry"))
        std::snprintf(temp + std::strlen(temp), sizeof(temp) - std::strlen(temp),
                      " (%s)", whCountry->c_str());

    AppendResult (ti, temp, NULL);
    scid::database::ratingT elo = g.coreGame().white().rating.value;
    if (elo != 0) {
        std::snprintf(temp, sizeof(temp), " <red>%u</red>", elo);
        AppendResult (ti, temp, NULL);
    }
    std::snprintf(temp, sizeof(temp), "  --  <pi %s>%s</pi>",
             g.coreGame().black().name.c_str(), g.coreGame().black().name.c_str());
    if (auto blCountry = g.coreGame().findExtraTag("BlackCountry"))
        std::snprintf(temp + std::strlen(temp), sizeof(temp) - std::strlen(temp),
                      " (%s)", blCountry->c_str());

    AppendResult (ti, temp, NULL);
    elo = g.coreGame().black().rating.value;
    if (elo != 0) {
        std::snprintf(temp, sizeof(temp), " <red>%u</red>", elo);
        AppendResult (ti, temp, NULL);
    }

    if (hideNextMove) {
        std::snprintf(temp, sizeof(temp), "<br>(%s: %s)",
                 translate (ti, "Result"), translate (ti, "hidden"));
    } else {
        const auto moveCount = static_cast<unsigned>(
            (g.coreGame().mainlineHalfMoveCount() + 1) / 2);
        std::snprintf(temp, sizeof(temp), "<br>%s <red>(%u)</red>",
                 scid::database::RESULT_LONGSTR[g.coreGame().result()],
                 moveCount);
    }
    AppendResult (ti, temp, NULL);

    if (!g.coreGame().eco().empty()) {
        scidup::eco::String fullEcoStr;
        scid::database::strCopy(fullEcoStr, g.coreGame().eco().c_str());
        scidup::eco::String basicEcoStr;
        scid::database::strCopy (basicEcoStr, fullEcoStr);
        if (scid::database::strLength(basicEcoStr) >= 4) { basicEcoStr[3] = 0; }
        AppendResult (ti, "   <blue><run ::windows::eco::Refresh ",
                          basicEcoStr, ">", fullEcoStr,
                          "</run></blue>", NULL);
    }
    char dateStr[20];
    scid::database::date_DecodeToString (g.coreGame().date(), dateStr);
    scid::database::strTrimDate (dateStr);
    AppendResult (ti, "   <red>", dateStr, "</red>", NULL);

    if (loadedGameId) {
        // Check if this game is deleted or has other user-settable flags:
        const scid::database::IndexEntry* ie = db->getIndexEntry(*loadedGameId);
        if (ie->GetDeleteFlag()) {
            AppendResult (ti, "   <gray>(",
                              translate (ti, "deleted"), ")</gray>", NULL);
        }
        char userFlags[16];
        if (ie->GetFlagStr (userFlags, NULL) != 0) {
            // Print other flags set for this game:
            const char * flagStr = userFlags;
            // Skip over "D" for Deleted, as it is indicated above:
            if (*flagStr == 'D') { flagStr++; }
            if (*flagStr != 0) {
                AppendResult (ti, "   <gray>(",
                                  translate (ti, "flags", "flags"),
                                  ": ", flagStr, NULL);
                int flagCount = 0;
                while (*flagStr != 0) {
                    const char * flagName = NULL;
                    switch (*flagStr) {
                        case 'W': flagName = "WhiteOpFlag"; break;
                        case 'B': flagName = "BlackOpFlag"; break;
                        case 'M': flagName = "MiddlegameFlag"; break;
                        case 'E': flagName = "EndgameFlag"; break;
                        case 'N': flagName = "NoveltyFlag"; break;
                        case 'P': flagName = "PawnFlag"; break;
                        case 'T': flagName = "TacticsFlag"; break;
                        case 'Q': flagName = "QsideFlag"; break;
                        case 'K': flagName = "KsideFlag"; break;
                        case '!': flagName = "BrilliancyFlag"; break;
                        case '?': flagName = "BlunderFlag"; break;
                        case 'U': flagName = "UserFlag"; break;
                    }
                    if (flagName != NULL) {
                        AppendResult (ti, (flagCount > 0 ? ", " : " - "),
                                          translate (ti, flagName), NULL);
                    }
                    flagCount++;
                    flagStr++;
                }
                AppendResult (ti, ")</gray>", NULL);
            }
        }

        if (g.coreGame().findExtraTag("Bib") != nullptr) {
           AppendResult (ti, "  <red><run ::Bibliography::ShowRef>Bib</run></red>", NULL);
        }

        // Check if this game has a twin (duplicate):
        if (db->getDuplicates(*loadedGameId) != 0) {
            AppendResult (ti, "   <blue><run updateTwinChecker>(",
                              translate (ti, "twin"), ")</run></blue>", NULL);
        }
    }
    std::snprintf(temp, sizeof(temp),
                  "<br><gray><run ::crosstab::Open>%s:  %s</run> (%s)</gray><br>",
             g.coreGame().site().c_str(),
             g.coreGame().event().c_str(),
             g.coreGame().round().c_str());
    AppendResult (ti, temp, NULL);

    char san [20];
    char tempTrans[20];
    auto position = currentPosition(g);
    if (!position)
        return UI_Result(ti, scid::database::ERROR, "Error reading position.");
    scid::database::colorT toMove = position->GetToMove();
    scid::database::uint moveCount = position->GetFullMoveCount();
    scid::database::uint prevMoveCount = moveCount;
    if (toMove == scid::database::WHITE) { prevMoveCount--; }

    scid::database::strCopy(
        san, scid::core::notation::previousSan(g.coreGame(), g.coreLocation())
                 .c_str());
    strcpy(tempTrans, san);
    scid::database::transPieces(tempTrans);
    bool printNags = true;
    if (san[0] == 0) {
        scid::database::strCopy (temp, "(");
        scid::database::strAppend (temp, variationLevel(g) == 0 ?
                   translate (ti, "GameStart", "Start of game") :
                   translate (ti, "LineStart", "Start of line"));
        scid::database::strAppend (temp, ")");
        printNags = false;
    } else {
        std::snprintf(temp, sizeof(temp), "<run ::move::Back>%u.%s%s</run>",
	                 prevMoveCount, toMove==scid::database::WHITE ? ".." : "", tempTrans);//san);
        printNags = true;
    }
    AppendResult (ti, translate (ti, "LastMove", "Last move"), NULL);
    AppendResult (ti, ": <darkblue>", temp, "</darkblue>", NULL);
    auto nags = previousMoveNags(g);
    if (printNags  &&  !nags.empty()  &&  !hideNextMove) {
        AppendResult (ti, "<red>", NULL);
        for (scid::database::uint nagCount = 0 ; nagCount < nags.size(); nagCount++) {
            char nagstr[20];
            game_printNag (nags[nagCount], nagstr, true, scid::database::PGN_FORMAT_Plain);
            if (nagCount > 0  ||  (nagstr[0] != '!' && nagstr[0] != '?')) {
                AppendResult (ti, " ", NULL);
            }
            AppendResult (ti, nagstr, NULL);
        }
        AppendResult (ti, "</red>", NULL);
    }

    // Now print next move:

    scid::database::strCopy(
        san,
        scid::core::notation::nextSan(g.coreGame(), g.coreLocation()).c_str());
    strcpy(tempTrans, san);
    scid::database::transPieces(tempTrans);
    if (san[0] == 0) {
        scid::database::strCopy (temp, "(");
        scid::database::strAppend (temp, variationLevel(g) == 0 ?
                   translate (ti, "GameEnd", "End of game") :
                   translate (ti, "LineEnd", "End of line"));
        scid::database::strAppend (temp, ")");
        printNags = false;
    } else if (hideNextMove) {
        std::snprintf(temp, sizeof(temp), "%u.%s(", moveCount, toMove==scid::database::WHITE ? "" : "..");
        scid::database::strAppend (temp, translate (ti, "hidden"));
        scid::database::strAppend (temp, ")");
        printNags = false;
    } else {
        std::snprintf(temp, sizeof(temp), "<run ::move::Forward>%u.%s%s</run>",
	                 moveCount, toMove==scid::database::WHITE ? "" : "..", tempTrans);//san);
        printNags = true;
    }
    AppendResult (ti, "   ", translate (ti, "NextMove", "Next"), NULL);
    AppendResult (ti, ": <darkblue>", temp, "</darkblue>", NULL);
    nags = nextMoveNags(g);
    if (printNags  &&  !hideNextMove  &&  !nags.empty()) {
        AppendResult (ti, "<red>", NULL);
        for (scid::database::uint nagCount = 0 ; nagCount < nags.size(); nagCount++) {
            char nagstr[20];
            game_printNag (nags[nagCount], nagstr, true, scid::database::PGN_FORMAT_Plain);
            if (nagCount > 0  ||  (nagstr[0] != '!' && nagstr[0] != '?')) {
                AppendResult (ti, " ", NULL);
            }
            AppendResult (ti, nagstr, NULL);
        }
        AppendResult (ti, "</red>", NULL);
    }

    if (variationLevel(g) > 0) {
        AppendResult (ti, "   <green><run sc_var exit; updateBoard -animate>",
                          "(<lt>-Var)", "</run></green>", NULL);
    }

    if (showMaterialValue) {
        scid::database::uint mWhite = position->MaterialValue (scid::database::WHITE);
        scid::database::uint mBlack = position->MaterialValue (scid::database::BLACK);
        std::snprintf(temp, sizeof(temp), "    <gray>(%u-%u", mWhite, mBlack);
        AppendResult (ti, temp, NULL);
        if (mWhite > mBlack) {
            std::snprintf(temp, sizeof(temp), ":+%u", mWhite - mBlack);
            AppendResult (ti, temp, NULL);
        } else if (mBlack > mWhite) {
            std::snprintf(temp, sizeof(temp), ":-%u", mBlack - mWhite);
            AppendResult (ti, temp, NULL);
        }
        AppendResult (ti, ")</gray>", NULL);
    }

    // Print first few variations if there are any:

    scid::database::uint varCount = variationCount(g);
    if (!hideNextMove  &&  varCount > 0) {
        AppendResult (ti, "<br>", translate (ti, "Variations"), ":", NULL);
        for (scid::database::uint vnum = 0; vnum < varCount && vnum < 5; vnum++) {
            char s[20];
            g.enterVariation (vnum);
            scid::database::strCopy(
                s,
                scid::core::notation::nextSan(g.coreGame(), g.coreLocation())
                    .c_str());
            strcpy(tempTrans, s);
            scid::database::transPieces(tempTrans);
            std::snprintf(temp, sizeof(temp), "   <run sc_var enter %u; updateBoard -animate>v%u",
	                     vnum, vnum+1);
            AppendResult (ti, "<green>", temp, "</green>: ", NULL);
            if (s[0] == 0) {
                std::snprintf(temp, sizeof(temp), "<darkblue>(empty)</darkblue>");
            } else {
                std::snprintf(temp, sizeof(temp), "<darkblue>%u.%s%s</darkblue>",
	                         moveCount, toMove == scid::database::WHITE ? "" : "..", tempTrans);//s);
            }
            AppendResult (ti, temp, NULL);
            auto firstNags = nextMoveNags(g);
            if (!firstNags.empty() &&
                firstNags.front() >= scid::core::NAG_GoodMove &&
                firstNags.front() <= scid::core::NAG_DubiousMove) {
                game_printNag (firstNags.front(), s, true, scid::database::PGN_FORMAT_Plain);
                AppendResult (ti, "<red>", s, "</red>", NULL);
            }
            AppendResult (ti, "</run>", NULL);
            g.exitVariation ();
        }
    }

    // Check if this move has a comment:

    {
        auto comment = currentMoveComment(g);
        AppendResult (ti, "<br>", translate(ti, "Comment"),
                          " <green><run makeCommentWin>", NULL);
        char * str = scid::database::strDuplicate(comment.c_str());
        scid::database::strTrimMarkCodes (str);
        const char * s = str;
        scid::database::uint len;
        scid::database::uint lines = 0;
        // Add the first commentWidth characters of the comment, up to
        // the first commentHeight lines:
        for (len = 0; len < commentWidth; len++, s++) {
            char ch = *s;
            if (ch == 0) { break; }
            if (ch == '\n') {
                lines++;
                if (lines >= commentHeight) { break; }
                AppendResult (ti, "<br>", NULL);
            } else if (ch == '<') {
                AppendResult (ti, "<lt>", NULL);
            } else if (ch == '>') {
                AppendResult (ti, "<gt>", NULL);
            } else {
                appendCharResult (ti, ch);
            }
        }
        // Complete the current comment word and add "..." if necessary:
        if (len == commentWidth) {
            char ch = *s;
            while (ch != ' '  &&  ch != '\n'  &&  ch != 0) {
                appendCharResult (ti, ch);
                s++;
                ch = *s;
            }
            if (ch != 0) {
                AppendResult (ti, "...", NULL);
            }
        }
        AppendResult (ti, "</run></green>", NULL);
        delete[] str;
    }

    // Now check ECO book for the current position:
    if (ecoBook) {
        auto pos = currentPosition(g);
        auto ecoStr = pos ? ecoBook->findEcoString(*pos) : "";
        if (!ecoStr.empty()) {
            std::string ecoComment(ecoStr);
            scidup::eco::Code eco = scidup::eco::fromString(ecoComment.c_str());
            scidup::eco::String estr;
            scidup::eco::toExtendedString(eco, estr);
            scid::database::uint len = scid::database::strLength (estr);
            if (len >= 4) { estr[3] = 0; }
            scid::database::DString tempDStr;
            translateECO (ti, ecoComment.c_str(), &tempDStr);
            AppendResult (ti, "<br>ECO:  <blue><run ::windows::eco::Refresh ",
                              estr, ">", tempDStr.Data(),
                              "</run></blue>", NULL);
        }
    }
    if (showFEN) {
        char boardStr [200];
        position->PrintFEN(boardStr, sizeof(boardStr));
        AppendResult (ti, "<br><gray>", boardStr, "</gray>", NULL);
    }
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_load:
//    Takes a game number and loads the game
int
sc_game_load (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (!db->isOpen()) {
        return errorResult (ti, errMsgNotOpen(ti));
    }
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_game load <gameNumber>");
    }
    scid::database::uint gnum = scid::database::strGetUnsigned (argv[2]);

    // Check the game number is valid::
    if (gnum < 1  ||  gnum > db->numGames()) {
        return errorResult (ti, "Invalid game number.");
    }

    // We number games from 0 internally, so subtract one:
    gnum--;
    auto err = editor.load(gnum);
    if (err != scid::database::OK) {
        return errorResult (ti, "Sorry, this game appears to be corrupt.");
    }
    return scid::database::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_merge:
//    Merge the specified game into a variation from the current
//    game position.
int
sc_game_merge (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game& game = editor.game();
    const char * usage = "Usage: sc_game merge <baseNum> <gameNum> [<endPly>]";
    if (argc < 4  ||  argc > 5) { return errorResult (ti, usage); }

    const scid::database::scidBaseT* base = DBasePool::getBase(scid::database::strGetUnsigned(argv[2]));
    if (base == 0) return UI_Result(ti, scid::database::ERROR_FileNotOpen);

    scid::database::uint gnum = scid::database::strGetUnsigned (argv[3]);
    scid::database::uint endPly = 9999;     // Defaults to huge number for all moves.
    if (argc == 5) { endPly = scid::database::strGetUnsigned (argv[4]); }

    if (gnum < 1  ||  gnum > base->numGames()) {
        return errorResult (ti, "Invalid game number.");
    }
    // Number games from 0 internally:
    gnum--;

    // Check that the specified game can be merged:
    if (base == db && editor.matchesLoadedGame(gnum)) {
        return errorResult (ti, "This game cannot be merged into itself.");
    }
    if (isAtStart(game)  &&  isAtEnd(game)) {
        return errorResult (ti, "The current game has no moves.");
    }
    if (game.coreGame().hasNonStandardStart()) {
        return errorResult (ti, "The current game has a non-standard start position.");
    }

    // Load the merge game:

    const scid::database::IndexEntry* ie = base->getIndexEntry(gnum);
    auto bbuf = base->getGame(*ie);
    scid::database::Game * merge = scratchGame;
    if (scid::database::game_storage::decodeMovesOnly(*merge, bbuf) !=
        scid::database::OK) {
        return errorResult (ti, "Error decoding game.");
    }
    if (merge->coreGame().hasNonStandardStart()) {
        return errorResult (ti, "The merge game has a non-standard start position.");
    }

    // Set up an array of all the game positions in the merge game:
    scid::database::uint nMergePos = merge->coreGame().mainlineHalfMoveCount() + 1;
    typedef char compactBoardStr [36];
    compactBoardStr * mergeBoards = new compactBoardStr [nMergePos];
    merge->restoreLocation(scid::core::MovetextLocation{});
    for (scid::database::uint i=0; i < nMergePos; i++) {
        auto position = currentPosition(*merge);
        if (!position) {
            delete [] mergeBoards;
            return UI_Result(ti, scid::database::ERROR, "Error reading position.");
        }
        position->PrintCompactStr (mergeBoards[i]);
        merge->next();
    }

    // Now find the deepest position in the current game that occurs
    // in the merge game:
    game.restoreLocation(scid::core::MovetextLocation{});
    scid::database::uint matchPly = 0;
    scid::database::uint mergePly = 0;
    scid::database::uint ply = 0;
    bool done = false;
    while (!done) {
        if (game.next() != scid::database::OK) { done = true; }
        ply++;
        compactBoardStr currentBoard;
        auto position = currentPosition(game);
        if (!position) {
            delete [] mergeBoards;
            return UI_Result(ti, scid::database::ERROR, "Error reading position.");
        }
        position->PrintCompactStr (currentBoard);
        for (scid::database::uint n=0; n < nMergePos; n++) {
            if (scid::database::strEqual (currentBoard, mergeBoards[n])) {
                matchPly = ply;
                mergePly = n;
            }
        }
    }

    delete [] mergeBoards;

    // Now the games match at the locations matchPly in the current
    // game and mergePly in the merge game.
    // Create a new variation and add merge-game moves to it:
    {
        scid::core::GameCursor cursor(game.coreGame());
        if (!cursor.toPly(matchPly))
            cursor.toEnd();
        game.restoreLocation(cursor.location());
    }
    bool atLastMove = isAtEnd(game);
    std::optional<scid::database::simpleMoveT> sm;
    if (atLastMove) {
        // At end of game, so remember final game move for replicating
        // at the start of the variation:
        game.previous();
        sm = currentMove(game);
        ASSERT(sm);
        game.next();
    }
    game.next();
    game.addVariation();
    editor.setDirty();
    if (sm) {
        // We need to replicate the last move of the current game.
        game.addMove(*sm);
    }
    {
        scid::core::GameCursor cursor(merge->coreGame());
        if (!cursor.toPly(mergePly))
            cursor.toEnd();
        merge->restoreLocation(cursor.location());
    }
    ply = mergePly;
    while (ply < endPly) {
        auto mergeMove = currentMove(*merge);
        if (merge->next() != scid::database::OK) { break; }
        if (!mergeMove) { break; }
        if (game.addMove(*mergeMove) != scid::database::OK) { break; }
        ply++;
    }

    // Finally, add a comment describing the merge-game details:
    const auto tags = base->tagRoster(*ie);
    const auto welo = ie->GetWhiteElo();
    const auto belo = ie->GetBlackElo();
    auto dstr = scid::database::DString();
    dstr.Append(scid::database::RESULT_LONGSTR[ie->GetResult()]);
    const auto mergeHalfMoves = merge->coreGame().mainlineHalfMoveCount();
    if (ply < mergeHalfMoves) {
        dstr.Append("(", (mergeHalfMoves + 1) / 2, ")");
    }
    dstr.Append(" ", tags.white);
    if (welo > 0) {
        dstr.Append(" (", welo, ")");
    }
    dstr.Append(" - ");
    dstr.Append(tags.black);
    if (belo > 0) {
        dstr.Append(" (", belo, ")");
    }
    dstr.Append(" / ", tags.event);
    dstr.Append(" (", tags.round, ")");
    dstr.Append(", ", tags.site);
    dstr.Append(" ", ie->GetYear());
    if (!setCurrentComment(game, dstr.Data()))
        return UI_Result(ti, scid::database::ERROR, "Error updating comment.");

    // And exit the new variation:
    game.exitVariation();
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_moves:
//    Return a string of the moves reaching the current game position.
//    Optional arguments: "coord" for coordinate notation (1 move per line);
//    "nomoves" for standard algebraic without move numbers.
//    Default output is standard algebraic with move numbers.
int
sc_game_moves (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    bool sanFormat = true;
    bool printMoves = true;
    bool listFormat = false;
    const scid::database::uint MAXMOVES = 500;
    scid::database::sanStringT * moveStrings = new scid::database::sanStringT [MAXMOVES];
    scid::database::uint plyCount = 0;
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game * g = &editor.game();
    for (int arg = 2; arg < argc; arg++) {
        if (argv[arg][0] == 'c') { sanFormat = false; }
        if (argv[arg][0] == 'n') { printMoves = false; }
        if (argv[arg][0] == 'l') { printMoves = false; }
    }

    auto location = g->coreLocation();
    while (! isAtStart(*g)) {
        if (isAtVariationStart(*g)) {
            g->exitVariation();
            continue;
        }
        g->previous();
        auto sm = currentMove(*g);
        if (!sm) { break; }
        char * s = moveStrings[plyCount];
        if (sanFormat) {
            scid::database::strCopy(
                s,
                scid::core::notation::nextSan(g->coreGame(), g->coreLocation())
                    .c_str());
        } else {
            s = sm->toLongNotation(s);
            *s = 0;
        }
        plyCount++;
        if (plyCount == MAXMOVES) {
            // Too many moves, just give up:
            g->restoreLocation(location);
            delete[] moveStrings;
            return TCL_OK;
        }
    }
    g->restoreLocation(location);
    scid::database::uint count = 0;
    for (scid::database::uint i = plyCount; i > 0; i--, count++) {
        char move [20];
        if (sanFormat) {
	            move[0] = 0;
	            if (printMoves  &&  (count % 2 == 0)) {
	                std::snprintf(move, sizeof(move), "%u.", (count / 2) + 1);
	            }
	            scid::database::strAppend (move, moveStrings[i - 1]);
        } else {
            scid::database::strCopy (move, moveStrings [i - 1]);
        }
        if (listFormat) {
            AppendElement (ti, move);
        } else {
            AppendResult (ti, (count == 0 ? "" : " "), move, NULL);
        }
    }
    delete[] moveStrings;
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_new:
//    Clears the current game.
int
sc_game_new(ClientData, Tcl_Interp*, int, const char**)
{
    auto editor = scidup::app::editor::gameSession(*db);
    editor.resetToNewGame();
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_novelty:
//    Finds the first move in the current game (after the deepest
//    position found in the ECO book) that reaches a position not
//    found in the selected database. It then moves to that point
//    in the game and returns a text string of the move.
int
sc_game_novelty (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    const char * usage =
        "Usage: sc_game novelty [-older] base";

    bool olderGamesOnly = false;

    int baseArg = 2;
    if (argc >= baseArg
        &&  argv[baseArg][0] == '-'  &&  argv[baseArg][1] == 'o'
        &&  scid::database::strIsPrefix (argv[baseArg], "-older")) {
        olderGamesOnly = true;
        baseArg++;
    }
    if (argc < baseArg  ||  argc > baseArg+1) return errorResult(ti, usage);
    auto base = DBasePool::getBase(scid::database::strGetInteger (argv[baseArg]));
    if (base == 0) return UI_Result(ti, scid::database::ERROR_BadArg);

    // First, move to the deepest ECO position in the game.
    // This code is adapted from sc_eco_game().
    auto editor = scidup::app::editor::gameSession(*base);
    scid::database::Game* g = &editor.game();
    if (ecoBook) {
        while (g->next() == scid::database::OK) {}
        while (true) {
            auto pos = currentPosition(*g);
            if (!pos || !ecoBook->findEcoString(*pos).empty()) { break; }
            if (g->previous() != scid::database::OK) break;
        }
    }

    // Now keep doing an exact position search (ignoring the current
    // game) and skipping to the next game position whenever a match
    // is found, until a position not in any database game is reached:
    scid::database::Progress progress = UI_CreateProgress(ti);
    std::string filtername = base->newFilter();
    scid::database::HFilter filter = scidup::app::tree::resolveFilter(*base, filtername);
    scid::database::dateT currentDate = g->coreGame().date();
    while (g->next() == scid::database::OK) {
        auto pos = currentPosition(*g);
        if (!pos)
            return UI_Result(ti, scid::database::ERROR, "Error reading position.");
        scid::database::SearchPos(*pos).setFilter(*base, filter, scid::database::Progress());
        int count = 0;
        for (scid::database::uint i=0, n = base->numGames(); i < n; i++) {
            if (filter.get(i) == 0) continue;

            // Ignore newer games if requested:
            if (olderGamesOnly) {
                if (base->getIndexEntry(i)->GetDate() >= currentDate) continue;
            }
            if (count++ != 0) break;
        }

        if (count <= 1) { // Novelty found
            base->deleteFilter(filtername.c_str());
            return UI_Result(ti, scid::database::OK, currentPly(*g));
        }

        auto work_done = currentPly(*g) + 1;
        if (!progress.report(work_done, g->coreGame().mainlineHalfMoveCount())) {
            base->deleteFilter(filtername.c_str());
            return UI_Result(ti, scid::database::ERROR_UserCancel);
        }
    }

    base->deleteFilter(filtername.c_str());
    return UI_Result(ti, scid::database::OK, -1);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_pgn:
//    Returns the PGN representation of the game.
//    Optional args:
//      -format (plain|html|latex): output format. Default=plain.
//      -shortHeader (0|1): short, 3-line (non-PGN) header. Default=0.
//      -space (0|1): printing a space after move numbers. Default=0.
//      -tags (0|1): printing (nonstandard) tags. Default=1.
//      -comments (0|1): printing nags/comments. Default=1.
//      -variations (0|1): printing variations. Default=1.
//      -indentVars (0|1): indenting variations. Default=0.
//      -indentComments (0|1): indenting comments. Default=0.
//      -width (number): line length for wordwrap. Default=huge (99999),
//        to let a Tk text widget do its own line-breaking.
//      -base (number): Print the game from the numbered base.
//      -gameNumber (number): Print the numbered game instead of the
//        active game.
//      -unicode (0|1): use unicocde characters (e.g. U+2654 for king). Default=0.
int
sc_game_pgn (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * options [] = {
        "-column", "-comments", "-base", "-gameNumber", "-format",
        "-shortHeader", "-indentComments", "-indentVariations",
        "-symbols", "-tags", "-variations", "-width", "-space",
        "-markCodes", "-unicode",
        NULL
    };
    enum {
        OPT_COLUMN, OPT_COMMENTS, OPT_BASE, OPT_GAME_NUMBER, OPT_FORMAT,
        OPT_SHORT_HDR, OPT_INDENT_COMMENTS, OPT_INDENT_VARS,
        OPT_SYMBOLS, OPT_TAGS, OPT_VARS, OPT_WIDTH, OPT_SPACE,
        OPT_NOMARKS, OPT_UNICODE,
    };

    scid::database::scidBaseT* base = db;
    auto editor = scidup::app::editor::gameSession(*base);
    scid::database::Game * g = &editor.game();
    scid::database::uint lineWidth = 99999;
    auto encodeOptions = scid::database::defaultLegacyGameEncodeOptions();

    // Parse all the command options:
    // Note that every option takes a value so options/values always occur
    // in pairs, which simplifies the code.

    int thisArg = 2;
    while (thisArg < argc) {
        int index = scid::database::strUniqueMatch (argv[thisArg], options);
        if (index == -1) {
            AppendResult (ti, "Invalid option to sc_game pgn: ",
                              argv[thisArg], "; valid options are: ", NULL);
            for (const char ** s = options; *s != NULL; s++) {
                AppendResult (ti, *s, " ", NULL);
            }
            return TCL_ERROR;
        }

        // Check that our option has a value:
        if (thisArg+1 == argc) {
            AppendResult (ti, "Invalid option value: sc_game pgn ",
                              options[index], " requires a value.", NULL);
            return TCL_ERROR;
        }

        scid::database::uint value = scid::database::strGetUnsigned (argv[thisArg+1]);

        if (index == OPT_WIDTH) {
            lineWidth = value;

        } else if (index == OPT_BASE) {
            base = DBasePool::getBase(value);
            if (base == 0) return UI_Result(ti, scid::database::ERROR_FileNotOpen);
            editor = scidup::app::editor::gameSession(*base);
            g = &editor.game();

        } else if (index == OPT_GAME_NUMBER) {
            // Print the numbered game instead of the active game:

            g = scratchGame;
            g->clear();
            if (value < 1  ||  value > base->numGames()) {
                return setResult (ti, "Invalid game number");
            }
            const scid::database::IndexEntry* ie = base->getIndexEntry(value - 1);
            if (ie->GetLength() == 0) {
                return errorResult (ti, "Error: empty game file record.");
            }
            if (base->getGame(*ie, *g) != scid::database::OK) {
                return errorResult (ti, "Error reading game file.");
            }

        } else if (index == OPT_FORMAT) {
            // The option value should be "plain", "html" or "latex".
            scid::database::gameFormatT format;
            if (! scid::database::LegacyGameEncodeOptions::legacyFormatFromString(
                    argv[thisArg + 1], &format)) {
                return errorResult (ti, "Invalid -format option.");
            }
            encodeOptions.legacyFormat = format;

        } else {
            // The option is a boolean affecting pgn style:
            scid::database::uint bitmask = 0;
            switch (index) {
                case OPT_COLUMN:
                    bitmask = PGN_STYLE_COLUMN;        break;
                case OPT_COMMENTS:
                    bitmask = PGN_STYLE_COMMENTS;        break;
                case OPT_SYMBOLS:
                    bitmask = PGN_STYLE_SYMBOLS;         break;
                case OPT_TAGS:
                    bitmask = PGN_STYLE_TAGS;            break;
                case OPT_VARS:
                    bitmask = PGN_STYLE_VARS;            break;
                case OPT_SHORT_HDR:
                    bitmask = PGN_STYLE_SHORT_HEADER;    break;
                case OPT_SPACE:
                    bitmask = PGN_STYLE_MOVENUM_SPACE;   break;
                case OPT_INDENT_VARS:
                    bitmask = PGN_STYLE_INDENT_VARS;     break;
                case OPT_INDENT_COMMENTS:
                    bitmask = PGN_STYLE_INDENT_COMMENTS; break;
                case OPT_NOMARKS:
                    bitmask = PGN_STYLE_STRIP_MARKS;     break;
                case OPT_UNICODE:
                    bitmask = PGN_STYLE_UNICODE;         break;
                default: // unreachable!
                    return errorResult (ti, "Invalid option.");
            };
            if (bitmask > 0) {
                if (value) {
                    encodeOptions.addStyle(bitmask);
                } else {
                    encodeOptions.removeStyle(bitmask);
                }
            }
        }
        thisArg += 2;
    }

    std::pair<const char*, unsigned> pgnBuf =
        scid::database::legacy_pgn::encode(*g, encodeOptions, lineWidth);
    AppendResult (ti, pgnBuf.first, NULL);
    return TCL_OK;
}

//~~~~~~~ DEPRECATED ~~~~~~
// sc_game_pop:
//    Restores the last game saved with sc_game_push.
int
sc_game_pop(ClientData, Tcl_Interp*, int, const char**)
{
    auto editor = scidup::app::editor::gameSession(*db);
    editor.pop();
    return TCL_OK;
}

//~~~~~~~ DEPRECATED ~~~~~~
// sc_game_push:
//    Saves the current game and pushes a new empty game onto
//    the game state stack.
//    If the optional argument "copy" is present, the new game will be
//    a copy of the current game.
int
sc_game_push (ClientData, Tcl_Interp*, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    bool copy = false;

    if ( argc > 2 && !strcmp( argv[2], "copy" ) ) {
        copy = true;
    }
    else if ( argc > 2 && !strcmp( argv[2], "copyfast" ) ) {
        copy = true;
    }

    editor.push(copy);
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_save:
//    Saves the current game. If the parameter is 0, a NEW
//    game is added; otherwise, that game number is REPLACED.
int
sc_game_save (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    auto dbase = db;
    scid::database::Game& currGame = editor.game();
    if (argc == 4) {
        dbase = DBasePool::getBase(scid::database::strGetUnsigned(argv[3]));
        if (dbase == 0) return errorResult (ti, "Invalid database number.");
    } else if (argc != 3) {
        return errorResult (ti, "Usage: sc_game save <gameNumber> [targetbaseId]");
    }

    scid::database::gamenumT gnum = scid::database::strGetUnsigned(argv[2]);
    if (gnum == 0) {
        gnum = scid::database::INVALID_GAMEID;
    } else {
        gnum -= 1;
        const scid::database::IndexEntry* ieOld = dbase->getIndexEntry_bounds(gnum);
        if (ieOld == 0) return scid::database::ERROR_BadArg;
        // User-settable flags were scid::database::stored in currGame when the game
        // was loaded, but the user may have changed them.
        char buf[scid::database::IndexEntry::IDX_NUM_FLAGS + 1];
        ieOld->GetFlagStr(buf, "WBMENPTKQ!?U123456");
        currGame.setScidFlags(buf, std::strlen(buf));
    }
    auto location = currGame.coreLocation();
    scid::database::errorT res = dbase->saveGame(currGame, gnum);
    currGame.restoreLocation(location);
    if (res == scid::database::OK) {
        if (gnum == scid::database::INVALID_GAMEID && db == dbase) {
            // Saved new game, so set gameNumber to the saved game number:
            editor.setLoadedGameId(db->numGames() - 1);
        }
        editor.setDirty(false);
    }

    return UI_Result(ti, res);;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_startBoard:
//    Sets the starting position from a FEN string.
//    If there is no FEN string argument, a boolean value is
//    returned indicating whether the current game starts with
//    a setup position.
int
sc_game_startBoard (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (argc == 2) {
        return UI_Result(ti, scid::database::OK,
                         editor.game().coreGame().hasNonStandardStart());
    } else if (argc != 3) {
        return errorResult (ti, "Usage: sc_game startBoard <fenString>");
    }
    const char * str = argv[2];
    auto err = resetStartFen(editor.game(), str);
    if (err != scid::database::OK)
        return errorResult(ti, "Invalid FEN string.");

    editor.setDirty();
    return UI_Result(ti, scid::database::OK);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_strip:
//    Strips all comments, variations or annotations from a game.
int sc_game_strip(ClientData, Tcl_Interp* ti, int argc, const char** argv) {
	auto editor = scidup::app::editor::gameSession(*db);
	if (argc == 3 && !strcmp("variations", argv[2])) {
		if (!stripMovetext(editor.game(), true, false, false))
			return UI_Result(ti, scid::database::ERROR, "Error stripping game.");
	} else if (argc == 3 && !strcmp("comments", argv[2])) {
		if (!stripMovetext(editor.game(), false, true, true))
			return UI_Result(ti, scid::database::ERROR, "Error stripping game.");
	} else {
		return errorResult(ti, "Usage: sc_game strip [comments|variations]");
	}
	editor.setDirty();
	return UI_Result(ti, scid::database::OK);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    Returns summary information of the specified game:
//    its players, site, etc; or its moves; or all its boards
//    positions.
UI_res_t sc_base_gamesummary(const scid::database::scidBaseT& base, UI_handle_t ti, int argc,
                             const char** argv) {
	const char* usage = "Usage: sc_base gamesummary baseId gameNum";
	if (argc != 4)
		return UI_Result(ti, scid::database::ERROR_BadArg, usage);

	scid::database::Game* g = scratchGame;
	scid::database::gamenumT gnum = scid::database::strGetUnsigned(argv[3]);
	if (gnum > 0) {
		auto ie = base.getIndexEntry_bounds(gnum - 1);
		if (!ie || base.getGame(*ie, *scratchGame) != scid::database::OK) {
			return UI_Result(ti, scid::database::ERROR_BadArg, usage);
		}
	} else {
		auto editor =
		    scidup::app::editor::gameSession(const_cast<scid::database::scidBaseT&>(base));
		g = &editor.game();
	}

    UI_List res(3);

    // Return header summary if requested:
        scid::database::DString dstr;
        dstr.Append (g->coreGame().white().name.c_str());
        scid::database::ratingT elo = g->coreGame().white().rating.value;
        if (elo > 0) { dstr.Append (" (", elo, ")"); }
        dstr.Append ("  --  ", g->coreGame().black().name.c_str());
        elo = g->coreGame().black().rating.value;
        if (elo > 0) { dstr.Append (" (", elo, ")"); }
        dstr.Append ("\n", g->coreGame().event().c_str());
        const char * round = g->coreGame().round().c_str();
        if (! scid::database::strIsUnknownName(round)) {
            dstr.Append (" (", round, ")");
        }
        dstr.Append ("  ", g->coreGame().site().c_str(), "\n");
        char dateStr [20];
        scid::database::date_DecodeToString (g->coreGame().date(), dateStr);
        // Remove ".??" or ".??.??" from end of date:
        if (dateStr[4] == '.'  &&  dateStr[5] == '?') { dateStr[4] = 0; }
        if (dateStr[7] == '.'  &&  dateStr[8] == '?') { dateStr[7] = 0; }
        dstr.Append (dateStr, "  ");
        dstr.Append (scid::database::RESULT_LONGSTR[g->coreGame().result()]);
        if (!g->coreGame().eco().empty()) {
            dstr.Append ("  ", g->coreGame().eco().c_str());
        }
        res.push_back(dstr.Data());

    // Here, a list of the boards or moves is requested:
    const auto n_moves = g->coreGame().mainlineHalfMoveCount() + 1;
    UI_List boards(n_moves);
    UI_List moves(n_moves);
    auto location = g->coreLocation();
    g->restoreLocation(scid::core::MovetextLocation{});
    do {
            auto position = currentPosition(*g);
            if (!position)
                return UI_Result(ti, scid::database::ERROR, "Error reading position.");
            char boardStr[100];
            position->MakeLongStr (boardStr);
            boards.push_back(boardStr);

            scid::database::colorT toMove = position->GetToMove();
            scid::database::uint moveCount = position->GetFullMoveCount();
            char san [20];
            scid::database::strCopy(
                san,
                scid::core::notation::nextSan(g->coreGame(), g->coreLocation())
                    .c_str());
	            if (san[0] != 0) {
	                char temp[40];
	                if (toMove == scid::database::WHITE) {
	                    std::snprintf(temp, sizeof(temp), "%u.%s", moveCount, san);
	                } else {
	                    scid::database::strCopy (temp, san);
	                }
                auto nags = nextMoveNags(*g);
                if (!nags.empty()) {
                    for (scid::database::uint nagCount = 0 ; nagCount < nags.size(); nagCount++) {
                        char nagstr[20];
                        game_printNag (nags[nagCount], nagstr, true,
                                       scid::database::PGN_FORMAT_Plain);
                        if (nagCount > 0  ||
                              (nagstr[0] != '!' && nagstr[0] != '?')) {
                            scid::database::strAppend (temp, " ");
                        }
                        scid::database::strAppend (temp, nagstr);
                    }
                }
                moves.push_back(temp);
            } else {
                moves.push_back(scid::database::RESULT_LONGSTR[g->coreGame().result()]);
            }

    } while (g->next() == scid::database::OK);
    g->restoreLocation(location);

    res.push_back(boards);
    res.push_back(moves);
    return UI_Result(ti, scid::database::OK, res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_tags:
//   Get, set or reload the current game tags, or share them
//   with another game.
int
sc_game_tags (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    const char * options[] = {
        "get", "set", "reload", "share", NULL
    };
    enum { OPT_GET, OPT_SET, OPT_RELOAD, OPT_SHARE };

    int index = -1;
    if (argc >= 3) { index = scid::database::strUniqueMatch (argv[2], options); }

    switch (index) {
        case OPT_GET:    return sc_game_tags_get (cd, ti, argc, argv);
        case OPT_SET:
          return sc_game_tags_set (cd, ti, argc, argv);
        case OPT_RELOAD: return sc_game_tags_reload (cd, ti, argc, argv);
        case OPT_SHARE:  return sc_game_tags_share (cd, ti, argc, argv);
        default:         return InvalidCommand (ti, "sc_game tags", options);
    }
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_tags_get:
//    Gets a tag for the active game given its name.
//    Valid names are:  Event, Site, Date, Round, White, Black,
//       WhiteElo, BlackElo, ECO, Extra.
//    All except the last (Extra) return the tag value as a string.
//    For "Extra", the function returns all the extra tags as one long
//    string, in PGN format, one tag per line.
int
sc_game_tags_get (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{

    static const char * options [] = {
        "Event", "Site", "Date", "Year", "Month", "Day",
        "Round", "White", "Black", "Result", "WhiteElo",
        "BlackElo", "WhiteRType", "BlackRType", "ECO",
        "EDate", "EYear", "EMonth", "EDay", "Extra",
        NULL
    };
    enum {
        T_Event, T_Site, T_Date, T_Year, T_Month, T_Day,
        T_Round, T_White, T_Black, T_Result, T_WhiteElo,
        T_BlackElo, T_WhiteRType, T_BlackRType, T_ECO,
        T_EDate, T_EYear, T_EMonth, T_EDay, T_Extra
    };

    const char * usage = "Usage: sc_game tags get [-last] <tagName>";
    const char * tagName;
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game * g = &editor.game();

    if (argc < 4  ||  argc > 5) {
        return errorResult (ti, usage);
    }
    tagName = argv[3];
    if (argc == 5) {
        if (!scid::database::strEqual (argv[3], "-last")) { return errorResult (ti, usage); }
        tagName = argv[4];
        if (db->numGames() > 0) {
            g = scratchGame;
            const scid::database::IndexEntry* ie = db->getIndexEntry(db->numGames() - 1);
            if (db->getGame(*ie, *g) != scid::database::OK) {
                return errorResult (ti, "Error reading game file.");
            }
        }
    }
    const char * s;
    int index = scid::database::strExactMatch (tagName, options);

    switch (index) {
    case T_Event:
        s = g->coreGame().event().c_str();  if (!s) { s = "?"; }
        AppendResult (ti, s, NULL);
        break;

    case T_Site:
        s = g->coreGame().site().c_str();  if (!s) { s = "?"; }
        AppendResult (ti, s, NULL);
        break;

    case T_Date:
        {
            char dateStr[20];
            scid::database::date_DecodeToString (g->coreGame().date(), dateStr);
            AppendResult (ti, dateStr, NULL);
        }
        break;

    case T_Year:
        return setUintResult (ti, scid::database::date_GetYear(g->coreGame().date()));

    case T_Month:
        return setUintWidthResult (ti, scid::database::date_GetMonth(g->coreGame().date()), 2);

    case T_Day:
        return setUintWidthResult (ti, scid::database::date_GetDay(g->coreGame().date()), 2);

    case T_Round:
        s = g->coreGame().round().c_str();  if (!s) { s = "?"; }
        AppendResult (ti, s, NULL);
        break;

    case T_White:
        s = g->coreGame().white().name.c_str();  if (!s) { s = "?"; }
        AppendResult (ti, s, NULL);
        break;

    case T_Black:
        s = g->coreGame().black().name.c_str();  if (!s) { s = "?"; }
        AppendResult (ti, s, NULL);
        break;

    case T_Result:
        return UI_Result(
            ti, scid::database::OK,
            std::string(1, scid::database::RESULT_CHAR[g->coreGame().result()]));

    case T_WhiteElo:
        return setUintResult (ti, g->coreGame().white().rating.value);

    case T_BlackElo:
        return setUintResult (ti, g->coreGame().black().rating.value);

    case T_WhiteRType:
        return setResult (
            ti, scid::database::ratingTypeNames[g->coreGame().white().rating.type]);

    case T_BlackRType:
        return setResult (
            ti, scid::database::ratingTypeNames[g->coreGame().black().rating.type]);

    case T_ECO:
        {
            AppendResult (ti, g->coreGame().eco().c_str(), NULL);
            break;
        }

    case T_EDate:
        {
            char dateStr[20];
            scid::database::date_DecodeToString (g->coreGame().eventDate(), dateStr);
            AppendResult (ti, dateStr, NULL);
        }
        break;

    case T_EYear:
        return setUintResult (
            ti, scid::database::date_GetYear(g->coreGame().eventDate()));

    case T_EMonth:
        return setUintWidthResult (
            ti, scid::database::date_GetMonth(g->coreGame().eventDate()), 2);

    case T_EDay:
        return setUintWidthResult (
            ti, scid::database::date_GetDay(g->coreGame().eventDate()), 2);

    case T_Extra:
        for (auto& tag : g->coreGame().extraTags()) {
            AppendResult(ti, tag.first.c_str(), " \"", tag.second.c_str(), "\"\n", NULL);
        }
        break;

    default:  // Not a valid tag name.
        return InvalidCommand (ti, "sc_game tags get", options);
    }

    return TCL_OK;
}

static scid::database::uint strGetRatingType (const char * name) {
    scid::database::uint i = 0;
    while (scid::database::ratingTypeNames[i] != NULL) {
        if (scid::database::strEqual (name, scid::database::ratingTypeNames[i])) { return i; }
        i++;
    }
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_tags_set:
//    Set the standard tags for this game.
//    Args are: event, site, date, round, white, black, result,
//              whiteElo, whiteRatingType, blackElo, blackRatingType, Eco,
//              eventdate.
//    Last arg is the non-standard tags, a string of lines in the format:
//        [TagName "TagValue"]
int
sc_game_tags_set (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game& game = editor.game();
    const char * options[] = {
        "-event", "-site", "-date", "-round", "-white", "-black", "-result",
        "-whiteElo", "-whiteRatingType", "-blackElo", "-blackRatingType",
        "-eco", "-eventdate", "-extra",
        NULL
    };
    enum {
        T_EVENT, T_SITE, T_DATE, T_ROUND, T_WHITE, T_BLACK, T_RESULT,
        T_WHITE_ELO, T_WHITE_RTYPE, T_BLACK_ELO, T_BLACK_RTYPE,
        T_ECO, T_EVENTDATE, T_EXTRA
    };

    int arg = 3;
    if (((argc-arg) % 2) != 0) {
        return errorResult (ti, "Odd number of parameters.");
    }

    // Process each pair of parameters:
    while (arg+1 < argc) {
        int index = scid::database::strUniqueMatch (argv[arg], options);
        const char * value = argv[arg+1];
        arg += 2;

        switch (index) {
            case T_EVENT: game.coreGame().addTag("Event", value); break;
            case T_SITE: game.coreGame().addTag("Site", value); break;
            case T_DATE:
                game.coreGame().setDate(scid::database::date_EncodeFromString(value));
                break;
            case T_ROUND: game.coreGame().addTag("Round", value); break;
            case T_WHITE: game.coreGame().addTag("White", value); break;
            case T_BLACK: game.coreGame().addTag("Black", value); break;
            case T_RESULT: game.coreGame().setResult(scid::database::strGetResult(value)); break;
            case T_WHITE_ELO:
                {
                    auto rating = game.coreGame().white().rating;
                    rating.value = scid::database::strGetUnsigned(value);
                    game.coreGame().setWhiteRating(rating);
                    break;
                }
            case T_WHITE_RTYPE:
                {
                    auto rating = game.coreGame().white().rating;
                    rating.type = strGetRatingType(value);
                    game.coreGame().setWhiteRating(rating);
                    break;
                }
            case T_BLACK_ELO:
                {
                    auto rating = game.coreGame().black().rating;
                    rating.value = scid::database::strGetUnsigned(value);
                    game.coreGame().setBlackRating(rating);
                    break;
                }
            case T_BLACK_RTYPE:
                {
                    auto rating = game.coreGame().black().rating;
                    rating.type = strGetRatingType(value);
                    game.coreGame().setBlackRating(rating);
                    break;
                }
            case T_ECO:
                {
                    scidup::eco::String ecoStr;
                    scidup::eco::toExtendedString(scidup::eco::fromString(value), ecoStr);
                    game.coreGame().setEco(ecoStr);
                    break;
                }
            case T_EVENTDATE:
                game.coreGame().setEventDate(scid::database::date_EncodeFromString(value));
                break;
            case T_EXTRA:
                {
                    // Add all the nonstandard tags:
                    std::vector<std::string> extraTags;
                    for (auto const& tag : game.coreGame().extraTags()) {
                        extraTags.push_back(tag.first);
                    }
                    for (auto const& tag : extraTags) {
                        game.coreGame().removeExtraTag(tag);
                    }

                    Tcl_Obj* list = Tcl_NewStringObj(value, -1);
                    Tcl_IncrRefCount(list);

                    decltype(Tcl_GetCharLength(nullptr)) objc; // size type changed (Tcl_Size)
                    Tcl_Obj** objv;
                    // Usage :: sc_game tags set -extra [ list "Annotator \"boob [sc_pos moveNumber]\"" ]
                    if (Tcl_ListObjGetElements(ti, list, &objc, &objv) != TCL_OK) {
                        Tcl_DecrRefCount(list);
                        return errorResult(ti, "Error parsing extra tags.");
                    }
                    // Extract each tag-value pair and add it to the game:
                    for (auto it = objv, end = objv + objc; it != end; ++it) {
                        decltype(objc) n;
                        decltype(objv) v;
                        // We expect a pair. Invalid entries are ignored.
                        if (Tcl_ListObjGetElements(ti, *it, &n, &v) == TCL_OK && n == 2) {
                            game.coreGame().addTag(Tcl_GetString(v[0]), Tcl_GetString(v[1]));
                        }
                    }
                    Tcl_DecrRefCount(list);
                }
                break;
            default:
                return InvalidCommand (ti, "sc_game tags set", options);
        }
    }

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_tags_reload:
//    Reloads the tags (White, Black, Event,Site, etc) for a game.
//    Useful when a name that may occur in the current game has been
//    edited.
int
sc_game_tags_reload(ClientData, Tcl_Interp*, int, const char**)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (!db->isOpen()) { return TCL_OK; }
    const auto ie = editor.loadedIndexEntry();
    if (!ie) { return TCL_OK; }
    scid::database::game_storage::loadStandardTags(editor.game(), *ie,
                                                   db->tagRoster(*ie));
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_game_tags_share:
//    Shares tags between two games, updating one where the other
//    has more complete or better information.
//
//    This is mainly useful for combining the header information
//    of a pair of twins before deleting one of them. For example,
//    one may have a less complete date while the other may have
//    no ratings or an unknown ("?") round value.
//
//    If the subcommand parameter is "check", a list is returned
//    with a multiple of four elements, each set of four indicating
//    a game number, the tag that will be changed, the old value,
//    and the new value. If the parameter is "update", the changes
//    will be made and the empty string is returned.
int
sc_game_tags_share (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    const char * usage =
        "Usage: sc_game tags share [check|update] <gameNumber1> <gameNumber2>";
    if (argc != 6) { return errorResult (ti, usage); }
    bool updateMode = false;
    if (scid::database::strIsPrefix (argv[3], "check")) {
        updateMode = false;
    } else if (scid::database::strIsPrefix (argv[3], "update")) {
        updateMode = true;
    } else {
        return errorResult (ti, usage);
    }
    // Get the two game numbers, which should be different and non-zero.
    scid::database::uint gn1 = scid::database::strGetUnsigned (argv[4]);
    scid::database::uint gn2 = scid::database::strGetUnsigned (argv[5]);
    if (gn1 == 0) { return TCL_OK; }
    if (gn2 == 0) { return TCL_OK; }
    if (gn1 == gn2) { return TCL_OK; }
    if (gn1 > db->numGames()) { return TCL_OK; }
    if (gn2 > db->numGames()) { return TCL_OK; }

    // Do nothing if the base is not writable:
    if (!db->isOpen()  ||  db->isReadOnly()) { return TCL_OK; }

    // Make a local copy of each index entry:
    scid::database::IndexEntry ie1 = *(db->getIndexEntry(gn1 - 1));
    scid::database::IndexEntry ie2 = *(db->getIndexEntry(gn2 - 1));
    bool updated1 = false;
    bool updated2 = false;

    // Share dates if appropriate:
    char dateStr1 [16];
    char dateStr2 [16];
    scid::database::dateT date1 = ie1.GetDate();
    scid::database::dateT date2 = ie2.GetDate();
    scid::database::date_DecodeToString (date1, dateStr1);
    scid::database::date_DecodeToString (date2, dateStr2);
    scid::database::strTrimDate (dateStr1);
    scid::database::strTrimDate (dateStr2);
    if (date1 == 0) { *dateStr1 = 0; }
    if (date2 == 0) { *dateStr2 = 0; }
    // Check if one date is a prefix of the other:
    if (!scid::database::strEqual (dateStr1, dateStr2)  &&  scid::database::strIsPrefix (dateStr1, dateStr2)) {
        // Copy date grom game 2 to game 1:
        if (updateMode) {
            ie1.SetDate (date2);
            updated1 = true;
        } else {
            appendUintElement (ti, gn1);
            AppendElement (ti, "Date");
            AppendElement (ti, dateStr1);
            AppendElement (ti, dateStr2);
        }
    }
    if (!scid::database::strEqual (dateStr1, dateStr2)  &&  scid::database::strIsPrefix (dateStr2, dateStr1)) {
        // Copy date grom game 1 to game 2:
        if (updateMode) {
            ie2.SetDate (date1);
            updated2 = true;
        } else {
            appendUintElement (ti, gn2);
            AppendElement (ti, "Date");
            AppendElement (ti, dateStr2);
            AppendElement (ti, dateStr1);
        }
    }

    // Check if an event name can be updated:
    scid::database::idNumberT event1 = ie1.GetEvent();
    scid::database::idNumberT event2 = ie2.GetEvent();
    const char* eventStr1 = db->getNameBase()->GetName(scid::database::NAME_EVENT, event1);
    const char* eventStr2 = db->getNameBase()->GetName(scid::database::NAME_EVENT, event2);
    bool event1empty = scid::database::strEqual (eventStr1, "")  ||  scid::database::strEqual (eventStr1, "?");
    bool event2empty = scid::database::strEqual (eventStr2, "")  ||  scid::database::strEqual (eventStr2, "?");
    if (event1empty  && !event2empty) {
        // Copy event from event 2 to game 1:
        if (updateMode) {
            ie1.SetEvent (event2);
            updated1 = true;
        } else {
            appendUintElement (ti, gn1);
            AppendElement (ti, "Event");
            AppendElement (ti, eventStr1);
            AppendElement (ti, eventStr2);
        }
    }
    if (event2empty  && !event1empty) {
        // Copy event from game 1 to game 2:
        if (updateMode) {
            ie2.SetEvent (event1);
            updated2 = true;
        } else {
            appendUintElement (ti, gn2);
            AppendElement (ti, "Event");
            AppendElement (ti, eventStr2);
            AppendElement (ti, eventStr1);
        }
    }

    // Check if a round name can be updated:
    scid::database::idNumberT round1 = ie1.GetRound();
    scid::database::idNumberT round2 = ie2.GetRound();
    const char* roundStr1 = db->getNameBase()->GetName(scid::database::NAME_ROUND, round1);
    const char* roundStr2 = db->getNameBase()->GetName(scid::database::NAME_ROUND, round2);
    bool round1empty = scid::database::strEqual (roundStr1, "")  ||  scid::database::strEqual (roundStr1, "?");
    bool round2empty = scid::database::strEqual (roundStr2, "")  ||  scid::database::strEqual (roundStr2, "?");
    if (round1empty  && !round2empty) {
        // Copy round from game 2 to game 1:
        if (updateMode) {
            ie1.SetRound (round2);
            updated1 = true;
        } else {
            appendUintElement (ti, gn1);
            AppendElement (ti, "Round");
            AppendElement (ti, roundStr1);
            AppendElement (ti, roundStr2);
        }
    }
    if (round2empty  && !round1empty) {
        // Copy round from game 1 to game 2:
        if (updateMode) {
            ie2.SetRound (round1);
            updated2 = true;
        } else {
            appendUintElement (ti, gn2);
            AppendElement (ti, "Round");
            AppendElement (ti, roundStr2);
            AppendElement (ti, roundStr1);
        }
    }

    // Check if Elo ratings can be shared:
    scid::database::ratingT welo1 = ie1.GetWhiteElo();
    scid::database::ratingT belo1 = ie1.GetBlackElo();
    scid::database::ratingT welo2 = ie2.GetWhiteElo();
    scid::database::ratingT belo2 = ie2.GetBlackElo();
    if (welo1 == 0  &&  welo2 != 0) {
        // Copy White rating from game 2 to game 1:
        if (updateMode) {
            ie1.SetWhiteElo (welo2);
            updated1 = true;
        } else {
            appendUintElement (ti, gn1);
            AppendElement (ti, "WhiteElo");
            appendUintElement (ti, welo1);
            appendUintElement (ti, welo2);
        }
    }
    if (welo2 == 0  &&  welo1 != 0) {
        // Copy White rating from game 1 to game 2:
        if (updateMode) {
            ie2.SetWhiteElo (welo1);
            updated2 = true;
        } else {
            appendUintElement (ti, gn2);
            AppendElement (ti, "WhiteElo");
            appendUintElement (ti, welo2);
            appendUintElement (ti, welo1);
        }
    }
    if (belo1 == 0  &&  belo2 != 0) {
        // Copy Black rating from game 2 to game 1:
        if (updateMode) {
            ie1.SetBlackElo (belo2);
            updated1 = true;
        } else {
            appendUintElement (ti, gn1);
            AppendElement (ti, "BlackElo");
            appendUintElement (ti, belo1);
            appendUintElement (ti, belo2);
        }
    }
    if (belo2 == 0  &&  belo1 != 0) {
        // Copy Black rating from game 1 to game 2:
        if (updateMode) {
            ie2.SetBlackElo (belo1);
            updated2 = true;
        } else {
            appendUintElement (ti, gn2);
            AppendElement (ti, "BlackElo");
            appendUintElement (ti, belo2);
            appendUintElement (ti, belo1);
        }
    }

    if (!updateMode)
        return TCL_OK;

    auto duplicates = db->extractDuplicates();
    scid::database::errorT err1 = scid::database::OK;
    scid::database::errorT err2 = scid::database::OK;
    if (updated1) {
        scid::database::Game game;
        err1 = db->getGame(ie1, game);
        if (err1 == scid::database::OK) {
            err1 = db->saveGame(&game, gn1 - 1);
        }
    }
    if (updated2) {
        scid::database::Game game;
        err2 = db->getGame(ie2, game);
        if (err2 == scid::database::OK) {
            err2 = db->saveGame(&game, gn2 - 1);
        }
    }
    db->setDuplicates(std::move(duplicates));
    return UI_Result(ti, err1 != scid::database::OK ? err1 : err2);
}

//////////////////////////////////////////////////////////////////////
///  INFO functions
static bool
date_ValidString (const char * str)
{
    scid::database::uint maxValues[3] = { scid::database::YEAR_MAX, 12, 31 };

    // Check year, then month, then day:
    for (scid::database::uint i=0; i < 3; i++) {
        scid::database::uint maxValue = maxValues[i];
        bool seenQuestion, seenDigit, seenOther;
        seenQuestion = seenDigit = seenOther = false;
        const char * start = str;
        while (*str != 0  &&  *str != '.') {
            char ch = *str;
            if (ch >= '0'  &&  ch <= '9') {
                seenDigit = true;
            } else if (ch == '?') {
                seenQuestion = true;
            } else {
                seenOther = true;
            }
            str++;
        }
        // Here, we should have seen question marks or digits, not both:
        if (seenOther) { return false; }
        if (seenQuestion  &&  seenDigit) { return false; }
        if (seenDigit) {
            // Check that the value is not too large:
            scid::database::uint value = scid::database::strGetUnsigned (start);
            if (value > maxValue) { return false; }
        }
        if (*str == 0) { return true; } else { str++; }
    }
    return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_info:
//    General Scid Information commands.
int
sc_info (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * options [] = {
        "clipbase", "decimal", "priority",
        "html", "limit", "ratings",
        "suffix", "validDate", "release", "language", NULL
    };
    enum {
        INFO_CLIPBASE, INFO_DECIMAL, INFO_PRIORITY,
        INFO_HTML, INFO_LIMIT, INFO_RATINGS,
        INFO_SUFFIX, INFO_VALIDDATE, INFO_RELEASE, INFO_LANGUAGE
    };
    int index = -1;

    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    switch (index) {
    case INFO_CLIPBASE:
        return UI_Result(ti, scid::database::OK, DBasePool::getClipBase());

    case INFO_DECIMAL:
        if (argc >= 3) {
            decimalPointChar = argv[2][0];
        } else {
            return UI_Result(ti, scid::database::OK, std::string(1, decimalPointChar));
        }
        break;

    case INFO_PRIORITY:
        return sc_info_priority(cd, ti, argc, argv);

    case INFO_HTML:
        if (argc >= 3) {
            htmlDiagStyle = scid::database::strGetUnsigned (argv[2]);
        } else {
            return setUintResult (ti, htmlDiagStyle);
        }
        break;

    case INFO_LIMIT:
        return sc_info_limit (cd, ti, argc, argv);

    case INFO_RATINGS:   // List of all recognised rating types.
        {
            scid::database::uint i = 0;
            while (scid::database::ratingTypeNames[i] != NULL) {
                AppendElement (ti, (char *) scid::database::ratingTypeNames[i]);
                i++;
            }
        }
        break;

    case INFO_VALIDDATE:
        if (argc != 3) {
            return errorResult (ti, "Usage: sc_info validDate <datestring>");
        }
        return UI_Result(ti, scid::database::OK, date_ValidString (argv[2]));


    case INFO_RELEASE:
        if (argc != 3) {
            return errorResult (ti, "Usage: sc_info release <version|date|is_prerelease>");
        }
        if (scid::database::strIsPrefix (argv[2], "version")) {
            setResult (ti, scidup::release::kVersion);
        } else if (scid::database::strIsPrefix (argv[2], "date")) {
            setResult (ti, scidup::release::kDate);
        } else if (scid::database::strIsPrefix (argv[2], "is_prerelease")) {
            const char* version = scidup::release::kVersion;
            const int isPrerelease =
                (version != nullptr && version[0] != 0 && std::strstr(version, "-testing-") != nullptr) ? 1 : 0;
            return setIntResult(ti, isPrerelease);
        } else {
            return errorResult (ti, "Usage: sc_info release <version|date|is_prerelease>");
        }
        break;
    case INFO_LANGUAGE:
      if (argc != 3) {
        return errorResult (ti, "Usage: sc_info language <lang>");
      }
      if ( strcmp(argv[2], "en") == 0) {scid::database::language = 0;}
      if ( strcmp(argv[2], "fr") == 0) {scid::database::language = 1;}
      if ( strcmp(argv[2], "es") == 0) {scid::database::language = 2;}
      if ( strcmp(argv[2], "de") == 0) {scid::database::language = 3;}
      if ( strcmp(argv[2], "it") == 0) {scid::database::language = 4;}
      if ( strcmp(argv[2], "ne") == 0) {scid::database::language = 5;}
      if ( strcmp(argv[2], "cz") == 0) {scid::database::language = 6;}
      if ( strcmp(argv[2], "hu") == 0) {scid::database::language = 7;}
      if ( strcmp(argv[2], "no") == 0) {scid::database::language = 8;}
      if ( strcmp(argv[2], "sw") == 0) {scid::database::language = 9;}
      if ( strcmp(argv[2], "ca") == 0) {scid::database::language = 10;}
      if ( strcmp(argv[2], "fi") == 0) {scid::database::language = 11;}
      if ( strcmp(argv[2], "gr") == 0) {scid::database::language = 12;}

    break;
    default:
        return InvalidCommand (ti, "sc_info", options);
    };

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_info limit:
//    Limits that Scid imposes.
int
sc_info_limit (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * options [] = {
        "elo", "year", "bases", NULL
    };
    enum {
        LIM_ELO, LIM_YEAR, LIM_BASES
    };
    int index = -1;
    int result = 0;

    if (argc == 3) { index = scid::database::strUniqueMatch (argv[2], options); }

    switch (index) {
    case LIM_ELO:
        result = scid::database::MAX_ELO;
        break;

    case LIM_YEAR:
        result = scid::database::YEAR_MAX;
        break;

    case LIM_BASES:
        result = MAX_BASES;
        break;

    default:
        return UI_Result(ti, scid::database::ERROR_BadArg, "Usage: sc_info limit <elo|year|bases>");
    }

    return UI_Result(ti, scid::database::OK, result);
}

//////////////////////////////////////////////////////////////////////
//  MOVE functions

int
sc_move (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    static const char * options [] = {
        "add", "addSan", "back", "end", "endVar", "forward",
        "pgn", "ply", "start", NULL
    };
    enum {
        MOVE_ADD, MOVE_ADDSAN, MOVE_BACK, MOVE_END, MOVE_ENDVAR,
        MOVE_FORWARD, MOVE_PGN, MOVE_PLY, MOVE_START
    };
    int index = -1;

    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    switch (index) {
    case MOVE_ADD:
        return sc_move_add (cd, ti, argc, argv);

    case MOVE_ADDSAN:
        return sc_move_addSan (cd, ti, argc, argv);

    case MOVE_BACK:
        return sc_move_back (cd, ti, argc, argv);

    case MOVE_END:
        {
            scid::core::GameCursor cursor(editor.game().coreGame());
            cursor.toEnd();
            editor.game().restoreLocation(cursor.location());
        }
        break;

    case MOVE_ENDVAR:
        while (editor.game().next() == scid::database::OK) {
        }
        break;

    case MOVE_FORWARD:
        return sc_move_forward (cd, ti, argc, argv);

    case MOVE_PGN:
        return sc_move_pgn (cd, ti, argc, argv);

    case MOVE_PLY:
        if (argc == 3) {
            scid::core::GameCursor cursor(editor.game().coreGame());
            if (!cursor.toPly(scid::database::strGetUnsigned(argv[2])))
                cursor.toEnd();
            editor.game().restoreLocation(cursor.location());
            return UI_Result(ti, scid::database::OK);
        }
        return errorResult (ti, "Usage: sc_move ply <plynumber>");

    case MOVE_START:
        editor.game().restoreLocation(scid::core::MovetextLocation{});
        break;

    default:
        return InvalidCommand (ti, "sc_move", options);
    }

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_move_add: takes a move specified by three parameters
//      (square square promo) and adds it to the game.
int
sc_move_add (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);

    if (argc != 5) {
        return errorResult (ti, "Usage: sc_move add <sq> <sq> <promo>");
    }

    scid::database::uint sq1 = scid::database::strGetUnsigned (argv[2]);
    scid::database::uint sq2 = scid::database::strGetUnsigned (argv[3]);
    scid::database::uint promo = scid::database::strGetUnsigned (argv[4]);
    if (promo == 0) { promo = scid::database::EMPTY; }

    char s[scid::database::UCI_MOVE_STRING_SIZE];
    s[0] = scid::database::square_FyleChar (sq1);
    s[1] = scid::database::square_RankChar (sq1);
    s[2] = scid::database::square_FyleChar (sq2);
    s[3] = scid::database::square_RankChar (sq2);
    if (promo == scid::database::EMPTY) {
        s[4] = 0;
    } else {
        s[4] = scid::database::piece_Char(promo);
        s[5] = 0;
    }
    auto pos = currentPosition(editor.game());
    if (!pos)
        return errorResult(ti, "Error adding move.");

    scid::database::simpleMoveT sm;
    scid::database::errorT err = pos->ReadCoordMove(&sm, s, s[4] == 0 ? 4 : 5, true);
    if (err == scid::database::OK) {
        err = editor.game().addMove(sm);
        if (err == scid::database::OK) {
            editor.setDirty();
            return TCL_OK;
        }
    }
    return errorResult (ti, "Error adding move.");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_move_addSan:
//    Takes moves in regular SAN (e.g. "e4" or "Nbd2") or UCI (e.g. "e2e4")
//    and adds them to the game. The moves can be in one large string, separate
//    list elements, or a mixture of both. Move numbers are ignored
//    but variations/comments/annotations are parsed and added.
int sc_move_addSan(ClientData, Tcl_Interp* ti, int argc, const char** argv) {
	auto editor = scidup::app::editor::gameSession(*db);
	scid::database::PgnParseLog parser;
	for (int i = 2; i < argc; ++i) {
		editor.setDirty();
		if (!scid::database::pgnParseGame(argv[i], std::strlen(argv[i]), editor.game(), parser))
			return UI_Result(ti, scid::database::ERROR_InvalidMove, argv[i]);
	}
	return UI_Result(ti, scid::database::OK);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_move_back:
//    Moves back a specified number of moves (default = 1 move).
int
sc_move_back (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    int numMovesTakenBack = 0;
    int count = 1;
    if (argc > 2) {
        count = scid::database::strGetInteger (argv[2]);
        // if (count < 1) { count = 1; }
    }

    for (int i = 0; i < count; i++) {
        if (editor.game().previous() != scid::database::OK) { break; }
        numMovesTakenBack++;
    }

    setUintResult (ti, numMovesTakenBack);
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_move_forward:
//    Moves forward a specified number of moves (default = 1 move).
int
sc_move_forward (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    int numMovesMade = 0;
    int count = 1;
    if (argc > 2) {
        count = scid::database::strGetInteger (argv[2]);
        // Do we want to allow moving forward 0 moves? Yes, I think so.
        // if (count < 1) { count = 1; }
    }

    for (int i = 0; i < count; i++) {
        if (editor.game().next() != scid::database::OK) { break; }
        numMovesMade++;
    }

    setUintResult (ti, numMovesMade);
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_move_pgn:
//    Get or set the current board to the position closest to
//    the specified place in the PGN output (given as a scid::database::byte count
//    from the start of the output).
int
sc_move_pgn (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (argc == 2) {
        auto location = pgnLocation(editor.game());
        if (!location)
            return UI_Result(ti, scid::database::ERROR, "Error reading PGN location.");
        return UI_Result(ti, scid::database::OK, *location);
    }

    if (argc != 3) {
        return errorResult (ti, "Usage: sc_move pgn [offset]");
    }

    scid::database::uint offset = scid::database::strGetUnsigned (argv[2]);
    auto location = seekPgnLocation(editor.game(), offset);
    if (!location)
        return UI_Result(ti, scid::database::ERROR, "Error reading PGN location.");
    editor.game().restoreLocation(*location);
    return TCL_OK;
}



//////////////////////////////////////////////////////////////////////
//  POSITION functions

int
sc_pos (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game& g = editor.game();
    static const char * options [] = {
        "addNag", "analyze", "bestSquare", "board", "clearNags",
        "fen", "getComment", "getNags", "hash", "html",
        "isAt", "isCheck", "isLegal", "isPromotion",
        "moveNumber", "pgnOffset",
        "setComment", "side", "tex", "moves", "location",
        "attacks", "getPrevComment", "coordToSAN", NULL
    };
    enum {
        POS_ADDNAG, POS_ANALYZE, POS_BESTSQ, POS_BOARD, POS_CLEARNAGS,
        POS_FEN, POS_GETCOMMENT, POS_GETNAGS, POS_HASH, POS_HTML,
        POS_ISAT, POS_ISCHECK, POS_ISLEGAL, POS_ISPROMO,
        POS_MOVENUM, POS_PGNOFFSET,
        POS_SETCOMMENT, POS_SIDE, POS_TEX, POS_MOVES, LOCATION,
        POS_ATTACKS, POS_GETPREVCOMMENT, POS_COORDTOSAN
    };

    char boardStr[200];
    int index = -1;
    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    switch (index) {
    case POS_ADDNAG:
        return sc_pos_addNag (cd, ti, argc, argv);

    case POS_ANALYZE:
        return sc_pos_analyze (cd, ti, argc, argv);

    case POS_BESTSQ:
        return sc_pos_bestSquare (cd, ti, argc, argv);

    case POS_BOARD:
        if (argc == 2) {
            auto pos = currentPosition(g);
            if (!pos)
                return UI_Result(ti, scid::database::ERROR, "Error reading position.");
            pos->MakeLongStr(boardStr);
            return UI_Result(ti, scid::database::OK, boardStr);
        }
        if (argc == 4) {
            scid::database::Position pos;
            if (auto err = pos.ReadFromFENorUCI(argv[2]))
                return UI_Result(ti, err);

            auto game = scid::database::Game();
            resetStartPosition(game, pos);
            if (const auto len = std::strlen(argv[3])) {
                scid::database::PgnParseLog pgn;
                if (!scid::database::pgnParseGame(argv[3], len, game, pgn))
                    return UI_Result(ti, scid::database::ERROR_InvalidMove);
            }

            {
                scid::core::GameCursor cursor(game.coreGame());
                cursor.toEnd();
                game.restoreLocation(cursor.location());
            }
            auto finalPos = currentPosition(game);
            if (!finalPos)
                return UI_Result(ti, scid::database::ERROR, "Error reading position.");
            finalPos->MakeLongStr(boardStr);
            auto lastmove = scid::core::notation::previousMoveUci(
                game.coreGame(), game.coreLocation());
            UI_List result(2);
            result.push_back(boardStr);
            result.push_back(lastmove);
            return UI_Result(ti, scid::database::OK, result);
        }
        return UI_Result(ti, scid::database::ERROR_BadArg, "sc_pos board [startpos moves]");

    case POS_CLEARNAGS:
        if (!clearCurrentNags(g))
            return UI_Result(ti, scid::database::ERROR, "Error updating annotations.");
        editor.setDirty();
        break;

    case POS_FEN:
        if (auto pos = currentPosition(g)) {
            pos->PrintFEN(boardStr, sizeof(boardStr));
        } else {
            return UI_Result(ti, scid::database::ERROR, "Error reading position.");
        }
        AppendResult (ti, boardStr, NULL);
        break;

    case POS_GETCOMMENT:
        AppendResult (ti, currentMoveComment(g).c_str(), NULL);
        break;

    case POS_GETPREVCOMMENT: {
        auto comments = previousClockComments(g);
        UI_List res(2);
        res.push_back(comments.first.c_str());
        res.push_back(comments.second.c_str());
        return UI_Result(ti, scid::database::OK, res);
    }
    case POS_GETNAGS:
        return sc_pos_getNags (cd, ti, argc, argv);

    case POS_HASH:
        return sc_pos_hash (cd, ti, argc, argv);

    case POS_HTML:
        return sc_pos_html (cd, ti, argc, argv);

    case POS_ISAT:
        return sc_pos_isAt (cd, ti, argc, argv);

    case POS_ISCHECK:
        if (auto pos = currentPosition(g))
            return UI_Result(ti, scid::database::OK, pos->IsKingInCheck());
        return UI_Result(ti, scid::database::ERROR, "Error reading position.");

    case POS_ISLEGAL:
        return sc_pos_isLegal (cd, ti, argc, argv);

    case POS_ISPROMO:
        return sc_pos_isPromo (cd, ti, argc, argv);

    case POS_MOVES:
        return sc_pos_moves (ti, argc, argv);

    case POS_MOVENUM:
        // This used to return:
        //     (currentPly(db->game) + 2) / 2
        // but that value is wrong for games with non-standard
        // start positions. The correct value to return is:
        //     current position's GetFullMoveCount()
        if (auto pos = currentPosition(g))
            return setUintResult (ti, pos->GetFullMoveCount());
        return UI_Result(ti, scid::database::ERROR, "Error reading position.");

    case POS_PGNOFFSET:
        if (auto offset = pgnOffset(g))
            return setUintResult (ti, *offset);
        return UI_Result(ti, scid::database::ERROR, "Error reading PGN location.");

    case POS_SETCOMMENT:
        return sc_pos_setComment (cd, ti, argc, argv);

    case POS_SIDE:
        if (auto pos = currentPosition(g)) {
            setResult (ti, (pos->GetToMove() == scid::database::WHITE)
                       ? "white" : "black");
        } else {
            return UI_Result(ti, scid::database::ERROR, "Error reading position.");
        }
        break;

    case POS_TEX:
        {
            bool flip = false;
            if (argc > 2  &&  scid::database::strIsPrefix (argv[2], "flip")) { flip = true; }
            scid::database::DString * dstr = new scid::database::DString;
            auto pos = currentPosition(g);
            if (!pos) {
                delete dstr;
                return UI_Result(ti, scid::database::ERROR, "Error reading position.");
            }
            pos->DumpLatexBoard (dstr, flip);
            AppendResult (ti, dstr->Data(), NULL);
            delete dstr;
        }
        break;

    case LOCATION:
        return UI_Result(ti, scid::database::OK, currentPly(g));

    case POS_ATTACKS:
        {
            auto current = currentPosition(g);
            if (!current)
                return UI_Result(ti, scid::database::ERROR, "Error reading position.");
            scid::database::Position pos(*current);
            for (scid::database::colorT c = scid::database::WHITE; c <= scid::database::BLACK; c++) {
                for (scid::database::uint i = 0; i < pos.GetCount(c); i++) {
                    scid::database::squareT sq = pos.GetList(c)[i];
                    pos.SetToMove(scid::database::color_Flip(c));
                    int att = pos.TreeCalcAttacks(sq);
                    if (att) {
                      appendUintElement(ti, sq);
                      if (att > 1) AppendElement(ti, "green");
                      else if (att > 0) AppendElement(ti, "yellow");
                      else AppendElement(ti, "red");
                    }
                }
            }
        }
        break;

    case POS_COORDTOSAN: {
        if (argc != 4)
            return UI_Result(ti, scid::database::ERROR_BadArg,
                             "Usage: sc_pos coordToSAN position moves");

        scid::database::Position pos;
        if (auto err = pos.ReadFromFENorUCI(argv[2]))
            return UI_Result(ti, err);

        std::string sanMoves;
        auto res = pos.MakeCoordMoves(argv[3], std::strlen(argv[3]), &sanMoves);
        if (res != scid::database::OK) { // If MakeCoordMoves failed, try if scid::database::pgnParseGame works
            scid::database::PgnParseLog log;
            scid::database::Game game;
            if (pos.ReadFromFENorUCI(argv[2]) == scid::database::OK &&
                (resetStartPosition(game, pos), true) &&
                scid::database::pgnParseGame(argv[3], std::strlen(argv[3]), game, log) &&
                log.log.empty()) {
                std::string moves;
                for (auto const& move :
                     game.coreGame().movetext().mainline.moves) {
                    moves.push_back(' ');
                    moves.append(move.action.longNotation());
                }
                std::string str;
                if (scid::database::OK == pos.MakeCoordMoves(moves.data(), moves.size(), &str))
                    return UI_Result(ti, scid::database::OK, str);
            }
        }
        return UI_Result(ti, res, sanMoves);

        }

    default:
        return InvalidCommand (ti, "sc_pos", options);
    }

    return TCL_OK;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_addNag:
//    Adds a NAG (annotation symbol) for the current move.
int
sc_pos_addNag (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_pos addNag <nagvalue>");
    }
	const char * nagStr = argv[2];
	if( strcmp(nagStr, "X") == 0) {
		if (!removeCurrentNag(editor.game(), true))
			return UI_Result(ti, scid::database::ERROR, "Error updating annotations.");
	}
	else if( strcmp(nagStr, "Y") == 0) {
		if (!removeCurrentNag(editor.game(), false))
			return UI_Result(ti, scid::database::ERROR, "Error updating annotations.");
	}
	else
	{
		scid::database::byte nag = scid::database::game_parseNag({nagStr, nagStr + std::strlen(nagStr)});
		if (nag != 0) {
			if (!addCurrentNag(editor.game(), nag))
				return UI_Result(ti, scid::database::ERROR, "Error updating annotations.");
		}
		editor.setDirty();
	}
    return TCL_OK;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_analyze:
//    Analyzes the current position for the specified number of
//    milliseconds.
//    Returns a two-element list containing the score in centipawns
//    (from the perspective of the side to move) and the best move.
//    If there are no legal moves, the second element is the empty string.
int
sc_pos_analyze (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    const char * usage = "Usage: sc_pos analyze [<option> <value> ...]";

    scid::database::uint searchTime = 1000;   // Default = 1000 milliseconds
    scid::database::uint hashTableKB = 1024;  // Default: one-megabyte hash table.
    scid::database::uint pawnTableKB = 32;
    bool postMode = false;
    bool pruning = false;
    scid::database::uint mindepth = 4; // will not check time until this depth is reached
    scid::database::uint searchdepth = 0;

    static const char * options [] = {
        "-time", "-hashkb", "-pawnkb", "-post", "-pruning", "-mindepth", "-searchdepth", NULL
    };
    enum {
        OPT_TIME, OPT_HASH, OPT_PAWN, OPT_POST, OPT_PRUNING, OPT_MINDEPTH, OPT_SEARCHDEPTH
    };
    int arg = 2;
    while (arg+1 < argc) {
        const char * option = argv[arg];
        const char * value = argv[arg+1];
        arg += 2;
        int index = scid::database::strUniqueMatch (option, options);
        switch (index) {
            case OPT_TIME:     searchTime = scid::database::strGetUnsigned(value);  break;
            case OPT_HASH:     hashTableKB = scid::database::strGetUnsigned(value); break;
            case OPT_PAWN:     pawnTableKB = scid::database::strGetUnsigned(value); break;
            case OPT_POST:     postMode = scid::database::strGetBoolean(value);     break;
            case OPT_PRUNING:  pruning = scid::database::strGetBoolean(value);      break;
            case OPT_MINDEPTH: mindepth = scid::database::strGetUnsigned(value);    break;
            case OPT_SEARCHDEPTH: searchdepth = scid::database::strGetUnsigned(value);    break;
            default:
                return InvalidCommand (ti, "sc_pos analyze", options);
        }
    }
    if (arg != argc) { return errorResult (ti, usage); }

    auto pos = currentPosition(editor.game());
    if (!pos)
        return errorResult(ti, "Error reading position.");

    // Generate all legal moves:
    scid::database::MoveList mlist;
    pos->GenerateMoves(&mlist);

    // Start the engine:
    Engine * engine = new Engine();
    engine->SetSearchTime (searchTime);
    engine->SetHashTableKilobytes (hashTableKB);
    engine->SetPawnTableKilobytes (pawnTableKB);
    engine->SetMinDepthCheckTime(mindepth);
    if (searchdepth > 0)
      engine->SetSearchDepth(searchdepth);
    engine->SetPosition (&*pos);
    engine->SetPostMode (postMode);
    engine->SetPruning (pruning);
    int score = engine->Think (&mlist);
    delete engine;

    char moveStr[20];
    moveStr[0] = 0;
    if (mlist.Size() > 0) {
        pos->MakeSANString (mlist.Get(0), moveStr, scid::database::SAN_MATETEST);
    }
    UI_List res(2);
    res.push_back(score);
    res.push_back(moveStr);
    return UI_Result(ti, scid::database::OK, res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_bestSquare:
//    Takes a square and returns the best square that makes a move
//    with the given square. The square can be the from or to part of
//    a move. Used for smart move completion.
//    Returns -1 if no legal moves go to or from the square.
int
sc_pos_bestSquare (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_pos bestSquare <square>");
    }

    auto pos = currentPosition(editor.game());
    if (!pos)
        return errorResult(ti, "Error reading position.");

    // Try to read the square parameter as algebraic ("h8") or numeric (63):
    scid::database::squareT sq = scid::database::strGetSquare (argv[2]);
    if (sq == scid::database::NULL_SQUARE) {
      int sqInt = scid::database::strGetInteger (argv[2]);
      if (sqInt >= 0  &&  sqInt <= 63) { sq = sqInt; }
    }
    if (sq == scid::database::NULL_SQUARE) {
        return errorResult (ti, "Usage: sc_pos bestSquare <square>");
    }

    // Generate all legal moves:
    scid::database::MoveList mlist;
    pos->GenerateMoves(&mlist);

    // Restrict the list of legal moves to contain only those that
    // move to or from the specified square:
    auto end = std::remove_if(mlist.begin(), mlist.end(), [&](auto const& sm) {
        return sm.from != sq && sm.to != sq;
    });
    mlist.resize(std::distance(mlist.begin(), end));

    // If no matching legal moves, return -1:
    if (mlist.Size() == 0) {
        return setResult (ti, "-1");
    }

    if (mlist.Size() > 1) {
        // We have more than one move to choose from, so first check
        // the ECO openings book (if it is loaded) to see if any move
        // in the list reaches an ECO position. If so, select the move
        // reaching the largest ECO code as the best move. If no ECO
        // position is found, do a small chess engine search to find
        // the best move.

        scidup::eco::Code bestEco = scidup::eco::ECO_None;
        scidup::eco::Code secondBestEco = scidup::eco::ECO_None;
        if (ecoBook != NULL) {
            for (scid::database::uint i=0; i < mlist.Size(); i++) {
                pos->DoSimpleMove(*mlist.Get(i));
                scidup::eco::Code eco = ecoBook->findEco(*pos);
                pos->UndoSimpleMove (*mlist.Get(i));
                if (eco >= bestEco) {
                    secondBestEco = bestEco;
                    bestEco = eco;
                    std::rotate(mlist.begin(), mlist.begin() + i,
                                mlist.begin() + i + 1);
                }
            }
        }

        if (bestEco == scidup::eco::ECO_None  ||  bestEco == secondBestEco) {
            // No matching ECO position found, or a tie. So do a short
            // engine search to find the best move; 25 ms (= 1/40 s)
            // is enough to reach a few ply and select reasonable
            // moves but fast enough to seem almost instant. The
            // search promotes the best move to be first in the list.
            Engine * engine = new Engine();
            engine->SetSearchTime (25);    // Do a 25 millisecond search
            engine->SetPosition (&*pos);
            engine->Think (&mlist);
            delete engine;
        }
    }

    // Now the best move is the first in the list, either because it
    // is the only move, or it reaches the largest ECO code, or because
    // the chess engine search selected it.
    // Find the other square in the best move and return it:

    scid::database::simpleMoveT * sm = mlist.Get(0);
    ASSERT (sq == sm->from  ||  sq == sm->to);
    scid::database::squareT bestSq = sm->from;
    if (sm->from == sq) { bestSq = sm->to; }
    setUintResult (ti, bestSq);

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_getNags:
//    Get the NAGs for the current move.
int
sc_pos_getNags(ClientData, Tcl_Interp* ti, int, const char**)
{
    auto editor = scidup::app::editor::gameSession(*db);
    auto nags = previousMoveNags(editor.game());
    if (nags.empty()) {
        return setResult (ti, "0");
    }
    for (auto nag : nags) {
        char temp[20];
        game_printNag (nag, temp, true, scid::database::PGN_FORMAT_Plain);
        AppendResult (ti, temp, " ", NULL);
    }

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_hash:
//   Returns the 32-bit hash value of the current position.
int
sc_pos_hash (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    const char * usage = "Usage: sc_pos hash [full|pawn]";
    bool pawnHashOnly = false;
    if (argc > 3) { return errorResult (ti, usage); }
    if (argc == 3) {
        switch (argv[2][0]) {
            case 'f': pawnHashOnly = false; break;
            case 'p': pawnHashOnly = true;  break;
            default:  return errorResult (ti, usage);
        }
    }
    auto pos = currentPosition(editor.game());
    if (!pos)
        return errorResult(ti, "Error reading position.");

    scid::database::uint hash = pos->HashValue();
    if (pawnHashOnly) {
        hash = pos->PawnHashValue();
    }
    return setUintResult (ti, hash);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_html:
//    Returns an HTML table representation of the board.
//    There are two styles: 0 (the default), which has
//    40x40 squares and images in a "bitmaps" subdirectory;
//    and style 1 which has 36x35 squares and images in
//    a "bitmaps2" directory.
//    The directory can be overridden with the "-path" command.
int
sc_pos_html (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    const char * usage = "Usage: sc_pos html [-flip <boolean>] [-path <path>] [<style:0|1>]";
    scid::database::uint style = htmlDiagStyle;
    bool flip = false;
    int arg = 2;
    const char * path = NULL;

    if (argc > arg+1  && scid::database::strEqual (argv[arg], "-flip")) {
        flip = scid::database::strGetBoolean(argv[arg+1]);
        arg += 2;
    }
    if (argc > arg+1  && scid::database::strEqual (argv[arg], "-path")) {
        path = argv[arg+1];
        arg += 2;
    }
    if (argc < arg ||  argc > arg+1) {
        return errorResult (ti, usage);
    }
    if (argc == arg+1) { style = scid::database::strGetUnsigned (argv[arg]); }

    scid::database::DString * dstr = new scid::database::DString;
    auto pos = currentPosition(editor.game());
    if (!pos) {
        delete dstr;
        return errorResult(ti, "Error reading position.");
    }
    pos->DumpHtmlBoard (dstr, style, path, flip);
    AppendResult (ti, dstr->Data(), NULL);
    delete dstr;
    return TCL_OK;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_isAt: returns whether the position is at the
//   start or end of a move list, according to the arg value.
//   Valid arguments are: start, end, vstart and vend (or unique
//   abbreviations thereof).
int
sc_pos_isAt (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    static const char * options [] = {
        "start", "end", "vstart", "vend", NULL
    };
    enum {
        OPT_START, OPT_END, OPT_VSTART, OPT_VEND
    };
    int index = -1;
    if (argc == 3) { index = scid::database::strUniqueMatch(argv[2], options); }

    switch (index) {
    case OPT_START:
        return UI_Result(ti, scid::database::OK, isAtStart(editor.game()));

    case OPT_END:
        return UI_Result(ti, scid::database::OK, isAtEnd(editor.game()));

    case OPT_VSTART:
        return UI_Result(ti, scid::database::OK, isAtVariationStart(editor.game()));

    case OPT_VEND:
        return UI_Result(ti, scid::database::OK, isAtVariationEnd(editor.game()));

    default:
        return errorResult (ti, "Usage: sc_pos isAt start|end|vstart|vend");
    }
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_isPromo:
//    Takes two squares (from and to, in either order) and
//    returns true if they represent a pawn promotion move.
int
sc_pos_isPromo (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (argc != 4) {
        return errorResult (ti, "Usage: sc_pos isPromotion <square> <square>");
    }

    auto pos = currentPosition(editor.game());
    if (!pos)
        return errorResult(ti, "Error reading position.");

    int fromSq = scid::database::strGetInteger (argv[2]);
    int toSq = scid::database::strGetInteger (argv[3]);

    if (fromSq < scid::database::A1  ||  fromSq > scid::database::H8  ||  toSq < scid::database::A1  ||  toSq > scid::database::H8) {
        return errorResult (ti, "Usage: sc_pos isPromotion <square> <square>");
    }

    return UI_Result(ti, scid::database::OK, pos->IsPromoMove ((scid::database::squareT) fromSq, (scid::database::squareT) toSq));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_isLegal: returns true if the move between the two provided
//    squares (either could be the from square) is legal.
int
sc_pos_isLegal (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (argc != 4) {
        return errorResult (ti, "Usage: sc_pos isLegal <square> <square>");
    }

    int sq1 = scid::database::strGetInteger (argv[2]);
    int sq2 = scid::database::strGetInteger (argv[3]);
    if (sq1 < 0  ||  sq1 > 63  ||  sq2 < 0  ||  sq2 > 63) {
        return UI_Result(ti, scid::database::OK, false);
    }

    auto pos = currentPosition(editor.game());
    if (!pos)
        return errorResult(ti, "Error reading position.");

    bool legal = pos->IsLegalMove(sq1, sq2, scid::database::EMPTY) ||
                 pos->IsLegalMove(sq2, sq1, scid::database::EMPTY) ||
                 pos->IsLegalMove(sq1, sq2, scid::database::QUEEN) ||
                 pos->IsLegalMove(sq2, sq1, scid::database::QUEEN);
    return UI_Result(ti, scid::database::OK, legal);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_moves: Return the list of legal moves in SAN notation
//
UI_res_t sc_pos_moves(UI_handle_t ti, int argc, const char** argv) {
    auto editor = scidup::app::editor::gameSession(*db);
    const char* usage = "Usage: sc_pos moves [bIncludeLongNotation]";
    if (argc != 2 && argc != 3)
        return UI_Result(ti, scid::database::ERROR_BadArg, usage);

    bool coordMoves = argc == 3 && scid::database::strGetBoolean(argv[2]);
    auto pos = currentPosition(editor.game());
    if (!pos)
        return UI_Result(ti, scid::database::ERROR, "Error reading position.");

    scid::database::MoveList moves;
    pos->GenerateMoves(&moves);
    UI_List res(moves.Size() * (coordMoves ? 2 : 1));
    for (auto& sm : moves) {
        char buf[64];
        pos->MakeSANString(&sm, buf, scid::database::SAN_CHECKTEST);
        res.push_back(buf);
        if (coordMoves) {
            *sm.toLongNotation(buf) = '\0';
            res.push_back(buf);
        }
    }
    return UI_Result(ti, scid::database::OK, res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_pos_setComment:
//    Set the comment for the current move.
int
sc_pos_setComment (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_pos setComment <comment-text>");
    }
    const char * str = argv[2];
    auto oldComment = currentMoveComment(editor.game());

    if (str[0] == 0  || (isspace((char)str[0]) && str[1] == 0)) {
        // No comment: nullify comment if necessary:
        if (!setCurrentComment(editor.game(), {}))
            return UI_Result(ti, scid::database::ERROR, "Error updating comment.");
        editor.setDirty();
    } else {
        // Only set the comment if it has actually changed:
        if (!scid::database::strEqual (str, oldComment.c_str())) {
            if (!setCurrentComment(editor.game(), str))
                return UI_Result(ti, scid::database::ERROR, "Error updating comment.");
            editor.setDirty();
        }
    }
    return TCL_OK;
}


//////////////////////////////////////////////////////////////////////
//   NAME commands

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_correct:
//    Corrects specified names in the database.
int
sc_name_correct (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    scid::database::nameT nt = scid::database::NAME_INVALID;
    if (argc == 4) { nt = scid::database::NameBase::NameTypeFromString (argv[2]); }

    if (! scid::database::NameBase::IsValidNameType(nt)) {
        return errorResult (ti,
                "Usage: sc_name correct p|e|s|r <corrections>");
    }

    const char * str = argv[3];
    char oldName [512];
    char newName [512];
    char birth[128];
    char death[128];
    char line [512];
    scid::database::uint errorCount = 0;
    scid::database::uint correctionCount = 0;
    scid::database::uint badDateCount = 0;

    auto dbase = db;
    const scid::database::NameBase* nb = dbase->getNameBase();
    std::vector<scid::database::idNumberT> oldIDs;
    std::vector<std::string> newNames;
    struct Entry {
        scid::database::idNumberT new_id;
        scid::database::dateT birth;
        scid::database::dateT death;
    };
    std::unordered_map<scid::database::idNumberT, Entry> corrections;

    while (*str != 0) {
        scid::database::uint length = 0;
        while (*str != 0  &&  *str != '\n') {
            if (length < 511) { line[length++] = *str; }
            str++;
        }
        line[length] = 0;
        if (*str == '\n') { str++; }
        // Now parse oldName and newName out of line, if the
        // line starts with a double-quote:
        char * s = line;
        if (*s != '"') { continue; }
        birth[0] = 0;
        death[0] = 0;
        int dummyCount = 0;
        if (sscanf (line, "\"%[^\"]\" >> \"%[^\"]\" (%d)  %[0-9.?]--%[0-9.?]",
                    oldName, newName, &dummyCount, birth, death) < 2) {
            continue;
        }

        correctionCount++;

        // Find oldName in the scid::database::NameBase:
        scid::database::idNumberT oldID = 0;
        if (nb->FindExactName (nt, oldName, &oldID) != scid::database::OK) {
            errorCount++;
            continue;
        }

        oldIDs.emplace_back(oldID);
        newNames.emplace_back(newName);
        corrections[oldID] = {oldID, scid::database::date_EncodeFromString(birth),
                              scid::database::date_EncodeFromString(death)};
    }

    auto initNewIDs = [&](auto const& v_ids) {
        ASSERT(v_ids.size() == oldIDs.size());
        const auto size = std::min(v_ids.size(), oldIDs.size());
        for (size_t i = 0; i < size; ++i) {
            const auto oldID = oldIDs[i];
            corrections[oldID].new_id = v_ids[i];
        }
    };

    auto entry_op = [&](scid::database::idNumberT oldID, const scid::database::IndexEntry& ie) {
        auto it = corrections.find(oldID);
        if (it == corrections.end())
            return oldID;

        auto const& el = it->second;
        scid::database::dateT date = ie.GetDate();
        if (date != scid::database::ZERO_DATE) {
            if (date < el.birth || (el.death != scid::database::ZERO_DATE && date > el.death)) {
                ++badDateCount;
                return oldID;
            }
        }
        return el.new_id;
    };

    scid::database::Progress progress = UI_CreateProgress(ti);
    auto changes = dbase->transformNames(nt, dbase->getFilter("all"), progress,
                                         newNames, initNewIDs, entry_op);

    UI_List res(4);
    res.push_back(correctionCount);
    res.push_back(errorCount);
    res.push_back(changes.second);
    res.push_back(badDateCount);
    return UI_Result(ti, changes.first, res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_edit: edits a name in the scid::database::NameBase. This requires
//    writing the entire index file, since the ID number of
//    the edited name will change.
//    A rating, date or eventdate can also be edited.
//
//    1st arg: player|event|site|round|rating|date|edate
//    2nd arg: "all" / "filter" / "crosstable" (which games to edit)
//    3rd arg: name to edit.
//    4th arg: new name -- it might already exist in the namebase.
int
sc_name_edit (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    const char * usage = "Usage: sc_name edit <type> <oldName> <newName>";
    const char * options[] = {
        "player", "event", "site", "round", "rating",
        "date", "edate", NULL
    };
    enum {
        OPT_PLAYER, OPT_EVENT, OPT_SITE, OPT_ROUND, OPT_RATING,
        OPT_DATE, OPT_EVENTDATE
    };

    scid::database::scidBaseT* dbase = db;
    int option = -1;
    if (argc > 2) { option = scid::database::strUniqueMatch (argv[2], options); }

    scid::database::nameT nt = scid::database::NAME_PLAYER;
    switch (option) {
    case OPT_PLAYER:  nt = scid::database::NAME_PLAYER;  break;
    case OPT_EVENT:   nt = scid::database::NAME_EVENT;   break;
    case OPT_SITE:    nt = scid::database::NAME_SITE;    break;
    case OPT_ROUND:   nt = scid::database::NAME_ROUND;   break;
    case OPT_RATING:  break;
    case OPT_DATE:    break;
    case OPT_EVENTDATE:   break;
    default:
        return errorResult (ti, usage);
    }

    if (option == OPT_RATING) {
        if (argc != 7) { return errorResult (ti, usage); }
    } else {
        if (argc != 6) { return errorResult (ti, usage); }
    }

    enum { EDIT_ALL, EDIT_FILTER, EDIT_CTABLE };
    int editSelection = EDIT_ALL;
    switch (argv[3][0]) {
    case 'a': editSelection = EDIT_ALL; break;
    case 'f': editSelection = EDIT_FILTER; break;
    case 'c': editSelection = EDIT_CTABLE; break;
    default:
        return errorResult (ti, usage);
    }

    const char * oldName = argv[4];
    const char * newName = argv[5];
    scid::database::dateT oldDate = scid::database::ZERO_DATE;
    scid::database::dateT newDate = scid::database::ZERO_DATE;
    scid::database::ratingT newRating = 0;
    scid::database::byte newRatingType = 0;
    if (option == OPT_RATING) {
        newRating = scid::database::strGetUnsigned (argv[5]);
        newRatingType = strGetRatingType (argv[6]);
    }
    if (option == OPT_DATE  ||  option == OPT_EVENTDATE) {
        oldDate = scid::database::date_EncodeFromString (argv[4]);
        newDate = scid::database::date_EncodeFromString (argv[5]);
    }

    // Find the existing name in the namebase:
    scid::database::idNumberT newID = 0;
    scid::database::idNumberT oldID = 0;
    if (option != OPT_DATE  &&  option != OPT_EVENTDATE) {
        if (dbase->getNameBase()->FindExactName (nt, oldName, &oldID) != scid::database::OK)
            return UI_Result(ti, scid::database::OK, 0);
    }

    // Set up crosstable game criteria if necessary:
    scid::database::idNumberT eventId = 0, siteId = 0;
    scid::database::dateT eventDate = 0;
    if (editSelection == EDIT_CTABLE) {
        auto editor = scidup::app::editor::gameSession(*dbase);
        scid::database::Game* g = &editor.game();
        auto nb = dbase->getNameBase();
        if (nb->FindExactName(scid::database::NAME_EVENT,
                              g->coreGame().event().c_str(),
                              &eventId) != scid::database::OK)
            return UI_Result(ti, scid::database::OK, 0);
        if (nb->FindExactName(scid::database::NAME_SITE,
                              g->coreGame().site().c_str(), &siteId) != scid::database::OK)
            return UI_Result(ti, scid::database::OK, 0);
        eventDate = g->coreGame().eventDate();
    }
    auto inCTable = [&](const scid::database::IndexEntry& ie) {
        if (editSelection != EDIT_CTABLE)
            return true;
        return isCrosstableGame(&ie, siteId, eventId, eventDate);
    };

    std::string filter =
        (editSelection == EDIT_FILTER) ? "dbfilter" : dbase->newFilter();
    auto hf = scidup::app::tree::resolveFilter(*dbase, filter);
    auto prg = UI_CreateProgress(ti);
    std::pair<scid::database::errorT, size_t> changes;
    switch (option) {
    case OPT_DATE:
        changes = dbase->transformIndex(hf, prg, [&](scid::database::IndexEntry& ie) {
            if (ie.GetDate() == oldDate && inCTable(ie)) {
                ie.SetDate(newDate);
                return true;
            }
            return false;
        });
        break;
    case OPT_EVENTDATE:
        changes = dbase->transformIndex(hf, prg, [&](scid::database::IndexEntry& ie) {
            if (ie.GetEventDate() == oldDate && inCTable(ie)) {
                ie.SetEventDate(newDate);
                return true;
            }
            return false;
        });
        break;

    case OPT_RATING:
        changes = dbase->transformIndex(hf, prg, [&](scid::database::IndexEntry& ie) {
            bool bTB = inCTable(ie);
            bool bWH = (ie.GetWhite() == oldID);
            bool bBK = (ie.GetBlack() == oldID);
            if (bTB && (bWH || bBK)) {
                if (bWH) {
                    ie.SetWhiteElo(newRating);
                    ie.SetWhiteRatingType(newRatingType);
                }
                if (bBK) {
                    ie.SetBlackElo(newRating);
                    ie.SetBlackRatingType(newRatingType);
                }
                return true;
            }
            return false;
        });
        break;

    default:
        changes = dbase->transformNames(
            nt, hf, prg, std::vector<std::string>(1, newName),
            [&](const std::vector<scid::database::idNumberT>& v) {
                newID = v.front(); // store the newID
            },
            [&](scid::database::idNumberT id, const scid::database::IndexEntry& ie) {
                return (id == oldID && inCTable(ie)) ? newID : id;
            });
    }

    if (editSelection != EDIT_FILTER)
        dbase->deleteFilter(filter.c_str());

    return UI_Result(ti, changes.first, changes.second);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_retrievename:
//    Check for the right name in spellcheck and return it.
UI_res_t sc_name_retrievename (UI_handle_t ti, const scidup::spelling::SpellChecker& sp, int argc, const char ** argv)
{
    const char * usageStr = "Usage: sc_name retrievename <player>";
    if (argc != 3 ) { return errorResult (ti, usageStr); }
    std::vector<const char*> res = sp.find(scid::database::NAME_PLAYER, argv[argc-1]);
    return UI_Result(ti, scid::database::OK, (res.size() == 1) ? res[0] : "");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_elo:
//    Search for Elo-Ratings in spellcheck file.
static UI_res_t sc_name_elo(UI_handle_t ti, const scidup::spelling::SpellChecker& sp, int argc,
                            const char** argv) {
	auto usage = "Usage: sc_name elo [year] <player>";
	if (argc < 3 || argc > 4)
		return UI_Result(ti, scid::database::ERROR_BadArg, usage);

	const char* playerName = argv[argc - 1];
	scid::database::uint startYear = (argc == 4) ? scid::database::strGetUnsigned(argv[2]) : 1900;

	UI_List res(scid::database::YEAR_MAX * 12 * 2);
	if (auto vElo = sp.getPlayerElo(playerName)) {
		for (scid::database::uint year = startYear; year < scid::database::YEAR_MAX; year++) {
			for (scid::database::uint month = 1; month < 13; month++) {
					if (scid::database::ratingT elo = vElo->getElo((((year) << scid::database::YEAR_SHIFT) | ((month) << scid::database::MONTH_SHIFT) | (15)))) {
						char temp[500];
						std::snprintf(temp, sizeof(temp), "%4u.%02u", year,
						              (month - 1) * 100 / 12);
						res.push_back(temp);
						std::snprintf(temp, sizeof(temp), "%4u", elo);
						res.push_back(temp);
					}
			}
		}
	}
	return UI_Result(ti, scid::database::OK, res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_info:
//    Prints information given a player name. Reports on the players
//    success rate with white and black, common openings by ECO code,
int
sc_name_info (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    static char * lastPlayerName = NULL;
    const char * usageStr = "Usage: sc_name info [-htext] <player>";

    if (argc != 3  &&  argc != 4) { return errorResult (ti, usageStr); }

    bool htextOutput = false;
    bool setFilter = false;   // Set filter to games by this player
    bool setOpponent = false; // Set filter to games vs this opponent
    bool filter [scid::database::NUM_COLOR_TYPES][scid::database::NUM_RESULT_TYPES] =
    {
        { false, false, false, false },
        { false, false, false, false }
    };

    if (argc == 4) {
        const char * opt = argv[2];
        if (scid::database::strIsPrefix ("-h", opt)  &&  scid::database::strIsPrefix (opt, "-htext")) {
            htextOutput = true;
        } else if (opt[0] == '-'  &&  (opt[1] == 'f' || opt[1] == 'o')) {
            if (opt[1] == 'f') {
                setFilter = true;
            } else {
                setOpponent = true;
            }
            // Parse filter options: a = all, w = wins, d = draws, l = losses
            // for White, and capitalise those for Black.
            const char * fopt = opt + 2;
            while (*fopt != 0) {
                switch (*fopt) {
                case 'a':  // All White games:
                    filter [scid::database::WHITE][scid::database::RESULT_White] = true;
                    filter [scid::database::WHITE][scid::database::RESULT_Draw] = true;
                    filter [scid::database::WHITE][scid::database::RESULT_Black] = true;
                    filter [scid::database::WHITE][scid::database::RESULT_None] = true;
                    break;
                case 'A':  // All Black games:
                    filter [scid::database::BLACK][scid::database::RESULT_White] = true;
                    filter [scid::database::BLACK][scid::database::RESULT_Draw] = true;
                    filter [scid::database::BLACK][scid::database::RESULT_Black] = true;
                    filter [scid::database::BLACK][scid::database::RESULT_None] = true;
                    break;
                case 'w':  // White wins:
                    filter [scid::database::WHITE][scid::database::RESULT_White] = true;
                    break;
                case 'W':  // Black wins:
                    filter [scid::database::BLACK][scid::database::RESULT_White] = true;
                    break;
                case 'd':  // White draws:
                    filter [scid::database::WHITE][scid::database::RESULT_Draw] = true;
                    break;
                case 'D':  // Black draws:
                    filter [scid::database::BLACK][scid::database::RESULT_Draw] = true;
                    break;
                case 'l':  // White losses:
                    filter [scid::database::WHITE][scid::database::RESULT_Black] = true;
                    break;
                case 'L':  // Black losses:
                    filter [scid::database::BLACK][scid::database::RESULT_Black] = true;
                    break;
                default:
                    return errorResult (ti, usageStr);
                }
                fopt++;
            }
        } else {
            return errorResult (ti, usageStr);
        }
    }

    // Set up player name:
    const char * playerName = argv[argc-1];
    if (scid::database::strEqual (playerName, "")) {
        if (lastPlayerName != NULL) { playerName = lastPlayerName; }
    } else {
        if (lastPlayerName != NULL) { delete[] lastPlayerName; }
        lastPlayerName = scid::database::strDuplicate (playerName);
    }

    // Try to find player name in this database:
    scid::database::idNumberT id = 0;
    if (db->getNameBase()->FindExactName (scid::database::NAME_PLAYER, playerName, &id) != scid::database::OK) {
        AppendResult (ti, "The name \"", playerName,
                          "\" does not exist in this database.", NULL);
        return TCL_OK;
    }

    // Try to find opponent in this database:
    scid::database::idNumberT opponentId = 0;
    const char * opponent = NULL;
    auto editor = scidup::app::editor::gameSession(*db);
    if (scid::database::strEqual (
            playerName, editor.game().coreGame().white().name.c_str())) {
        opponent = editor.game().coreGame().black().name.c_str();
    } else if (scid::database::strEqual (
                   playerName, editor.game().coreGame().black().name.c_str())) {
        opponent = editor.game().coreGame().white().name.c_str();
    }

    if (opponent != NULL) {
        if (db->getNameBase()->FindExactName (scid::database::NAME_PLAYER, opponent, &opponentId) != scid::database::OK) {
            opponent = NULL;
        }
    }

    enum {STATS_ALL = 0, STATS_FILTER, STATS_OPP};

    scid::database::uint whitescore [3][scid::database::NUM_RESULT_TYPES];
    scid::database::uint blackscore [3][scid::database::NUM_RESULT_TYPES];
    scid::database::uint bothscore [3][scid::database::NUM_RESULT_TYPES];
    scid::database::uint whitecount[3] = {0};
    scid::database::uint blackcount[3] = {0};
    scid::database::uint totalcount[3] = {0};
    for (scid::database::uint stat=STATS_ALL; stat <= STATS_OPP; stat++) {
        for (scid::database::resultT r = 0; r < scid::database::NUM_RESULT_TYPES; r++) {
            whitescore[stat][r] = 0;
            blackscore[stat][r] = 0;
            bothscore[stat][r] = 0;
        }
    }

    scid::database::uint ecoCount [scid::database::NUM_COLOR_TYPES] [50];
    scid::database::uint ecoScore [scid::database::NUM_COLOR_TYPES] [50];
    for (scid::database::uint ecoGroup=0; ecoGroup < 50; ecoGroup++) {
        ecoCount[scid::database::WHITE][ecoGroup] = ecoCount[scid::database::BLACK][ecoGroup] = 0;
        ecoScore[scid::database::WHITE][ecoGroup] = ecoScore[scid::database::BLACK][ecoGroup] = 0;
    }

    scid::database::dateT firstGameDate = scid::database::ZERO_DATE;
    scid::database::dateT lastGameDate = scid::database::ZERO_DATE;

    if (setFilter || setOpponent) db->defaultFilterFill(0);

    for (scid::database::uint i=0, n = db->numGames(); i < n; i++) {
        const scid::database::IndexEntry* ie = db->getIndexEntry(i);
        scidup::eco::Code ecoCode = ie->GetEcoCode();
        int ecoClass = -1;
        if (ecoCode != scidup::eco::ECO_None) {
            scidup::eco::String ecoStr;
            scidup::eco::toBasicString(ecoCode, ecoStr);
            if (ecoStr[0] != 0) {
                ecoClass = ((ecoStr[0] - 'A') * 10) + (ecoStr[1] - '0');
                if (ecoClass < 0  ||  ecoClass >= 50) { ecoClass = -1; }
            }
        }

        scid::database::resultT result = ie->GetResult();
        scid::database::idNumberT whiteId = ie->GetWhite();
        scid::database::idNumberT blackId = ie->GetBlack();
        scid::database::dateT date = scid::database::ZERO_DATE;

        // Track statistics as white and black:
        if (whiteId == id) {
            date = ie->GetDate();
            if (ecoClass >= 0) {
                ecoCount[scid::database::WHITE][ecoClass]++;
                ecoScore[scid::database::WHITE][ecoClass] += scid::database::RESULT_SCORE[result];
            }
            whitescore[STATS_ALL][result]++;
            bothscore[STATS_ALL][result]++;
            whitecount[STATS_ALL]++;
            totalcount[STATS_ALL]++;
            if (db->defaultFilterGet(i) > 0) {
                whitescore[STATS_FILTER][result]++;
                bothscore[STATS_FILTER][result]++;
                whitecount[STATS_FILTER]++;
                totalcount[STATS_FILTER]++;
            }
            if (opponent != NULL  &&  blackId == opponentId) {
                whitescore[STATS_OPP][result]++;
                bothscore[STATS_OPP][result]++;
                whitecount[STATS_OPP]++;
                totalcount[STATS_OPP]++;
                if (setOpponent  &&  filter[scid::database::WHITE][result]) {
                    db->defaultFilterSet(i, 1);
                }
            }
            if (setFilter  &&  filter[scid::database::WHITE][result]) {
                db->defaultFilterSet(i, 1);
            }
        } else if (blackId == id) {
            date = ie->GetDate();
            result = scid::database::RESULT_OPPOSITE[result];
            if (ecoClass >= 0) {
                ecoCount[scid::database::BLACK][ecoClass]++;
                ecoScore[scid::database::BLACK][ecoClass] += scid::database::RESULT_SCORE[result];
            }
            blackscore[STATS_ALL][result]++;
            bothscore[STATS_ALL][result]++;
            blackcount[STATS_ALL]++;
            totalcount[STATS_ALL]++;
            if (db->defaultFilterGet(i) > 0) {
                blackscore[STATS_FILTER][result]++;
                bothscore[STATS_FILTER][result]++;
                blackcount[STATS_FILTER]++;
                totalcount[STATS_FILTER]++;
            }
            if (opponent != NULL  &&  whiteId == opponentId) {
                blackscore[STATS_OPP][result]++;
                bothscore[STATS_OPP][result]++;
                blackcount[STATS_OPP]++;
                totalcount[STATS_OPP]++;
                if (setOpponent  &&  filter[scid::database::BLACK][result]) {
                    db->defaultFilterSet(i, 1);
                }
            }
            if (setFilter  &&  filter[scid::database::BLACK][result]) {
                db->defaultFilterSet(i, 1);
            }
        }

        // Keep track of first and last games by this player:
        if (date != scid::database::ZERO_DATE) {
            if (firstGameDate == scid::database::ZERO_DATE  ||  date < firstGameDate) {
                firstGameDate = date;
            }
            if (lastGameDate == scid::database::ZERO_DATE  ||  date > lastGameDate) {
                lastGameDate = date;
            }
        }
    }

    char temp [500];
    scid::database::uint score, percent;
    scid::database::colorT color;
    const char * newline = (htextOutput ? "<br>" : "\n");
    const char * startHeading = (htextOutput ? "<darkblue>" : "");
    const char * endHeading = (htextOutput ? "</darkblue>" : "");
    const char * startBold = (htextOutput ? "<b>" : "");
    const char * endBold = (htextOutput ? "</b>" : "");
    scid::database::uint wWidth = scid::database::strLength (translate (ti, "White:"));
    scid::database::uint bWidth = scid::database::strLength (translate (ti, "Black:"));
    scid::database::uint tWidth = scid::database::strLength (translate (ti, "Total:"));
    scid::database::uint wbtWidth = wWidth;
    if (bWidth > wbtWidth) { wbtWidth = bWidth; }
    if (tWidth > wbtWidth) { wbtWidth = tWidth; }
    const char * fmt = \
     "%s  %-*s %3u%c%02u%%   +%s%3u%s  =%s%3u%s  -%s%3u%s  %4u%c%c /%s%4u%s";
    scidup::spelling::SpellChecker* spChecker = spellChk.get();

    AppendResult (ti, startBold, playerName, endBold, newline, NULL);

    // Show title, country, etc if listed in player spellcheck file:
    if (spChecker != NULL) {
        const scidup::spelling::PlayerInfo* pInfo = spChecker->getPlayerInfo(playerName);
        if (pInfo) { AppendResult (ti, "  ", pInfo->getComment(), newline, NULL); }
    }
    std::snprintf(temp, sizeof(temp), "  %s%u%s %s (%s: %u)",
             htextOutput ? "<red><run sc_name info -faA {}; ::windows::stats::Refresh>" : "",
             totalcount[STATS_ALL],
             htextOutput ? "</run></red>" : "",
             (totalcount[STATS_ALL] == 1 ?
              translate (ti, "game") : translate (ti, "games")),
             translate (ti, "Filter"),
             totalcount[STATS_FILTER]);
    AppendResult (ti, temp, NULL);
    if (firstGameDate != scid::database::ZERO_DATE) {
        scid::database::date_DecodeToString (firstGameDate, temp);
        scid::database::strTrimDate (temp);
        AppendResult (ti, ", ", temp, NULL);
    }
    if (lastGameDate > firstGameDate) {
        scid::database::date_DecodeToString (lastGameDate, temp);
        scid::database::strTrimDate (temp);
        AppendResult (ti, "--", temp, NULL);
    }
    AppendResult (ti, newline, NULL);

    // Print biography if applicable:
    if (spChecker != NULL) {
        std::vector<const char*> bio;
        const scidup::spelling::PlayerInfo* pInfo = spChecker->getPlayerInfo(playerName, &bio);
        if (pInfo != 0) {
            for (size_t i=0, n=bio.size(); i < n; i++) {
                if (i == 0) {
                    AppendResult (ti, newline, startHeading,
                              translate (ti, "Biography"), ":",
                              endHeading, newline, NULL);
                }
                AppendResult (ti, "  ", bio[i], newline, NULL);
            }
        }
    }
    // Print stats for all games:

    scid::database::strCopy (temp, translate (ti, "PInfoAll"));
    if (! htextOutput) { scid::database::strTrimMarkup (temp); }
    AppendResult (ti, newline, startHeading, temp, ":",
                      endHeading, newline, NULL);

    score = percent = 0;
    if (whitecount[STATS_ALL] > 0) {
        score = whitescore[STATS_ALL][scid::database::RESULT_White] * 2
            + whitescore[STATS_ALL][scid::database::RESULT_Draw]
            + whitescore[STATS_ALL][scid::database::RESULT_None];
        percent = score * 5000 / whitecount[STATS_ALL];
    }
    std::snprintf(temp, sizeof(temp), fmt,
             htextOutput ? "<tt>" : "",
             wbtWidth,
             translate (ti, "White:"),
             percent / 100, decimalPointChar, percent % 100,
             htextOutput ? "<red><run sc_name info -fw {}; ::windows::stats::Refresh>" : "",
             whitescore[STATS_ALL][scid::database::RESULT_White],
             htextOutput ? "</run></red>" : "",
             htextOutput ? "<red><run sc_name info -fd {}; ::windows::stats::Refresh>" : "",
             whitescore[STATS_ALL][scid::database::RESULT_Draw],
             htextOutput ? "</run></red>" : "",
             htextOutput ? "<red><run sc_name info -fl {}; ::windows::stats::Refresh>" : "",
             whitescore[STATS_ALL][scid::database::RESULT_Black],
             htextOutput ? "</run></red>" : "",
             score / 2, decimalPointChar, score % 2 ? '5' : '0',
             htextOutput ? "<red><run sc_name info -fa {}; ::windows::stats::Refresh>" : "",
             whitecount[STATS_ALL],
             htextOutput ? "</run></red></tt>" : "");
    AppendResult (ti, temp, newline, NULL);

    score = percent = 0;
    if (blackcount[STATS_ALL] > 0) {
        score = blackscore[STATS_ALL][scid::database::RESULT_White] * 2
            + blackscore[STATS_ALL][scid::database::RESULT_Draw]
            + blackscore[STATS_ALL][scid::database::RESULT_None];
        percent = score * 5000 / blackcount[STATS_ALL];
    }
    std::snprintf(temp, sizeof(temp), fmt,
             htextOutput ? "<tt>" : "",
             wbtWidth,
             translate (ti, "Black:"),
             percent / 100, decimalPointChar, percent % 100,
             htextOutput ? "<red><run sc_name info -fW {}; ::windows::stats::Refresh>" : "",
             blackscore[STATS_ALL][scid::database::RESULT_White],
             htextOutput ? "</run></red>" : "",
             htextOutput ? "<red><run sc_name info -fD {}; ::windows::stats::Refresh>" : "",
             blackscore[STATS_ALL][scid::database::RESULT_Draw],
             htextOutput ? "</run></red>" : "",
             htextOutput ? "<red><run sc_name info -fL {}; ::windows::stats::Refresh>" : "",
             blackscore[STATS_ALL][scid::database::RESULT_Black],
             htextOutput ? "</run></red>" : "",
             score / 2, decimalPointChar, score % 2 ? '5' : '0',
             htextOutput ? "<red><run sc_name info -fA {}; ::windows::stats::Refresh>" : "",
             blackcount[STATS_ALL],
             htextOutput ? "</run></red></tt>" : "");
    AppendResult (ti, temp, newline, NULL);

    score = percent = 0;
    if (totalcount[STATS_ALL] > 0) {
        score = bothscore[STATS_ALL][scid::database::RESULT_White] * 2
            + bothscore[STATS_ALL][scid::database::RESULT_Draw]
            + bothscore[STATS_ALL][scid::database::RESULT_None];
        percent = score * 5000 / totalcount[STATS_ALL];
    }
    std::snprintf(temp, sizeof(temp), fmt,
             htextOutput ? "<tt>" : "",
             wbtWidth,
             translate (ti, "Total:"),
             percent / 100, decimalPointChar, percent % 100,
             htextOutput ? "<red><run sc_name info -fwW {}; ::windows::stats::Refresh>" : "",
             bothscore[STATS_ALL][scid::database::RESULT_White],
             htextOutput ? "</run></red>" : "",
             htextOutput ? "<red><run sc_name info -fdD {}; ::windows::stats::Refresh>" : "",
             bothscore[STATS_ALL][scid::database::RESULT_Draw],
             htextOutput ? "</run></red>" : "",
             htextOutput ? "<red><run sc_name info -flL {}; ::windows::stats::Refresh>" : "",
             bothscore[STATS_ALL][scid::database::RESULT_Black],
             htextOutput ? "</run></red>" : "",
             score / 2, decimalPointChar, score % 2 ? '5' : '0',
             htextOutput ? "<red><run sc_name info -faA {}; ::windows::stats::Refresh>" : "",
             totalcount[STATS_ALL],
             htextOutput ? "</run></red></tt>" : "");
    AppendResult (ti, temp, newline, NULL);

    // Now print stats for games in the filter:

    scid::database::strCopy (temp, translate (ti, "PInfoFilter"));
    if (! htextOutput) { scid::database::strTrimMarkup (temp); }
    AppendResult (ti, newline, startHeading, temp, ":",
                      endHeading, newline, NULL);
    score = percent = 0;
    if (whitecount[STATS_FILTER] > 0) {
        score = whitescore[STATS_FILTER][scid::database::RESULT_White] * 2
            + whitescore[STATS_FILTER][scid::database::RESULT_Draw]
            + whitescore[STATS_FILTER][scid::database::RESULT_None];
        percent = score * 5000 / whitecount[STATS_FILTER];
    }
    std::snprintf(temp, sizeof(temp), fmt,
             htextOutput ? "<tt>" : "",
             wbtWidth,
             translate (ti, "White:"),
             percent / 100, decimalPointChar, percent % 100,
             "", whitescore[STATS_FILTER][scid::database::RESULT_White], "",
             "", whitescore[STATS_FILTER][scid::database::RESULT_Draw], "",
             "", whitescore[STATS_FILTER][scid::database::RESULT_Black], "",
             score / 2, decimalPointChar, score % 2 ? '5' : '0',
             "", whitecount[STATS_FILTER],
             htextOutput ? "</tt>" : "");
    AppendResult (ti, temp, newline, NULL);

    score = percent = 0;
    if (blackcount[STATS_FILTER] > 0) {
        score = blackscore[STATS_FILTER][scid::database::RESULT_White] * 2
            + blackscore[STATS_FILTER][scid::database::RESULT_Draw]
            + blackscore[STATS_FILTER][scid::database::RESULT_None];
        percent = score * 5000 / blackcount[STATS_FILTER];
    }
    std::snprintf(temp, sizeof(temp), fmt,
             htextOutput ? "<tt>" : "",
             wbtWidth,
             translate (ti, "Black:"),
             percent / 100, decimalPointChar, percent % 100,
             "", blackscore[STATS_FILTER][scid::database::RESULT_White], "",
             "", blackscore[STATS_FILTER][scid::database::RESULT_Draw], "",
             "", blackscore[STATS_FILTER][scid::database::RESULT_Black], "",
             score / 2, decimalPointChar, score % 2 ? '5' : '0',
             "", blackcount[STATS_FILTER],
             htextOutput ? "</tt>" : "");
    AppendResult (ti, temp, newline, NULL);

    score = percent = 0;
    if (totalcount[STATS_FILTER] > 0) {
        score = bothscore[STATS_FILTER][scid::database::RESULT_White] * 2
            + bothscore[STATS_FILTER][scid::database::RESULT_Draw]
            + bothscore[STATS_FILTER][scid::database::RESULT_None];
        percent = score * 5000 / totalcount[STATS_FILTER];
    }
    std::snprintf(temp, sizeof(temp), fmt,
             htextOutput ? "<tt>" : "",
             wbtWidth,
             translate (ti, "Total:"),
             percent / 100, decimalPointChar, percent % 100,
             "", bothscore[STATS_FILTER][scid::database::RESULT_White], "",
             "", bothscore[STATS_FILTER][scid::database::RESULT_Draw], "",
             "", bothscore[STATS_FILTER][scid::database::RESULT_Black], "",
             score / 2, decimalPointChar, score % 2 ? '5' : '0',
             "", totalcount[STATS_FILTER],
             htextOutput ? "</tt>" : "");
    AppendResult (ti, temp, newline, NULL);

    // Now print stats for games against the current opponent:

    if (opponent != NULL) {
        AppendResult (ti, newline, startHeading,
                          translate (ti, "PInfoAgainst"), " ",
                          startBold, opponent, endBold, ":",
                          endHeading, newline, NULL);

        score = percent = 0;
        if (whitecount[STATS_OPP] > 0) {
            score = whitescore[STATS_OPP][scid::database::RESULT_White] * 2
            + whitescore[STATS_OPP][scid::database::RESULT_Draw]
            + whitescore[STATS_OPP][scid::database::RESULT_None];
            percent = score * 5000 / whitecount[STATS_OPP];
        }
        std::snprintf(temp, sizeof(temp), fmt,
                 htextOutput ? "<tt>" : "",
                 wbtWidth,
                 translate (ti, "White:"),
                 percent / 100, decimalPointChar, percent % 100,
                 htextOutput ? "<red><run sc_name info -ow {}; ::windows::stats::Refresh>" : "",
                 whitescore[STATS_OPP][scid::database::RESULT_White],
                 htextOutput ? "</run></red>" : "",
                 htextOutput ? "<red><run sc_name info -od {}; ::windows::stats::Refresh>" : "",
                 whitescore[STATS_OPP][scid::database::RESULT_Draw],
                 htextOutput ? "</run></red>" : "",
                 htextOutput ? "<red><run sc_name info -ol {}; ::windows::stats::Refresh>" : "",
                 whitescore[STATS_OPP][scid::database::RESULT_Black],
                 htextOutput ? "</run></red>" : "",
                 score / 2, decimalPointChar, score % 2 ? '5' : '0',
                 htextOutput ? "<red><run sc_name info -oa {}; ::windows::stats::Refresh>" : "",
                 whitecount[STATS_OPP],
                 htextOutput ? "</run></red></tt>" : "");
        AppendResult (ti, temp, newline, NULL);

        score = percent = 0;
        if (blackcount[STATS_OPP] > 0) {
            score = blackscore[STATS_OPP][scid::database::RESULT_White] * 2
                + blackscore[STATS_OPP][scid::database::RESULT_Draw]
                + blackscore[STATS_OPP][scid::database::RESULT_None];
            percent = score * 5000 / blackcount[STATS_OPP];
        }
        std::snprintf(temp, sizeof(temp), fmt,
                 htextOutput ? "<tt>" : "",
                 wbtWidth,
                 translate (ti, "Black:"),
                 percent / 100, decimalPointChar, percent % 100,
                 htextOutput ? "<red><run sc_name info -oW {}; ::windows::stats::Refresh>" : "",
                 blackscore[STATS_OPP][scid::database::RESULT_White],
                 htextOutput ? "</run></red>" : "",
                 htextOutput ? "<red><run sc_name info -oD {}; ::windows::stats::Refresh>" : "",
                 blackscore[STATS_OPP][scid::database::RESULT_Draw],
                 htextOutput ? "</run></red>" : "",
                 htextOutput ? "<red><run sc_name info -oL {}; ::windows::stats::Refresh>" : "",
                 blackscore[STATS_OPP][scid::database::RESULT_Black],
                 htextOutput ? "</run></red>" : "",
                 score / 2, decimalPointChar, score % 2 ? '5' : '0',
                 htextOutput ? "<red><run sc_name info -oA {}; ::windows::stats::Refresh>" : "",
                 blackcount[STATS_OPP],
                 htextOutput ? "</run></red></tt>" : "");
        AppendResult (ti, temp, newline, NULL);

        score = percent = 0;
        if (totalcount[STATS_OPP] > 0) {
            score = bothscore[STATS_OPP][scid::database::RESULT_White] * 2
                + bothscore[STATS_OPP][scid::database::RESULT_Draw]
                + bothscore[STATS_OPP][scid::database::RESULT_None];
            percent = score * 5000 / totalcount[STATS_OPP];
    }
        std::snprintf(temp, sizeof(temp), fmt,
                 htextOutput ? "<tt>" : "",
                 wbtWidth,
                 translate (ti, "Total:"),
                 percent / 100, decimalPointChar, percent % 100,
                 htextOutput ? "<red><run sc_name info -owW {}; ::windows::stats::Refresh>" : "",
                 bothscore[STATS_OPP][scid::database::RESULT_White],
                 htextOutput ? "</run></red>" : "",
                 htextOutput ? "<red><run sc_name info -odD {}; ::windows::stats::Refresh>" : "",
                 bothscore[STATS_OPP][scid::database::RESULT_Draw],
                 htextOutput ? "</run></red>" : "",
                 htextOutput ? "<red><run sc_name info -olL {}; ::windows::stats::Refresh>" : "",
                 bothscore[STATS_OPP][scid::database::RESULT_Black],
                 htextOutput ? "</run></red>" : "",
                 score / 2, decimalPointChar, score % 2 ? '5' : '0',
                 htextOutput ? "<red><run sc_name info -oaA {}; ::windows::stats::Refresh>" : "",
                 totalcount[STATS_OPP],
                 htextOutput ? "</run></red></tt>" : "");
        AppendResult (ti, temp, newline, NULL);
    }

    // Now print common openings played:

    for (color = scid::database::WHITE; color <= scid::database::BLACK; color++) {
        for (scid::database::uint count = 0; count < 6; count++) {
            int mostPlayedIdx = -1;
            scid::database::uint mostPlayed = 0;
            for (scid::database::uint i=0; i < 50; i++) {
                if (ecoCount[color][i] > mostPlayed) {
                    mostPlayedIdx = i;
                    mostPlayed = ecoCount[color][i];
                }
            }
            if (mostPlayed > 0) {
                if (count == 0) {
                    const char * s = (color == scid::database::WHITE ? "PInfoMostWhite" :
                                      "PInfoMostBlack");
                    AppendResult (ti, newline, startHeading,
                                      translate (ti, s), ":",
                                      endHeading, newline, NULL);
                } else if (count == 3) {
                    AppendResult (ti, newline, NULL);
                }
                AppendResult (ti, "   ", NULL);

                temp[0] = mostPlayedIdx / 10 + 'A';
                temp[1] = mostPlayedIdx % 10 + '0';
                temp[2] = 0;
                if (htextOutput) {
                    AppendResult (ti, "<blue><run ::windows::eco::Refresh ",
                                      temp, ">", NULL);
                }
                AppendResult (ti, temp, NULL);
                if (htextOutput) {
                    AppendResult (ti, "</run></blue>", NULL);
                }
                std::snprintf(temp, sizeof(temp), ":%3u (%u%%)", mostPlayed,
                         ecoScore[color][mostPlayedIdx] * 50 / mostPlayed);
                AppendResult (ti, temp, NULL);
                ecoCount[color][mostPlayedIdx] = 0;
            }
        }
    }

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_match: returns the first N matching names,
//    or fewer if there are not N matches, given a substring
//    to search for.
//    Output is a Tcl list, to be read in pairs: the first element of
//    each pair is the frequency, the second contains the name.
//
//    1st arg: "p" (player) / "e" (event) / "s" (site) / "r" (round)
//    2nd arg: prefix string to search for.
//    3rd arg: maximum number of matches to return.
//    Example: sc_nameMatch player "Speel" 10
int
sc_name_match (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    const char * usage = \
        "Usage: sc_name match [-elo] <nameType> <prefix> <maxMatches>";

    int arg = 2;
    int argsleft = argc - 2;
    bool eloMode = false;  // In elo mode, return player peak ratings.
    if (argsleft < 3) { return errorResult (ti, usage); }
    if (argv[arg][0] == '-'  &&  scid::database::strIsPrefix (argv[arg], "-elo")) {
        eloMode = true;
        arg++;
        argsleft--;
    }
    if (argsleft != 3) {
        return errorResult (ti, usage);
    }

    scid::database::nameT nt = scid::database::NameBase::NameTypeFromString (argv[arg++]);
    if (nt == scid::database::NAME_INVALID) {
        return errorResult (ti, usage);
    }

    const char * prefix = argv[arg++];
    scid::database::uint maxMatches = scid::database::strGetUnsigned (argv[arg++]);
    if (maxMatches == 0) { return TCL_OK; }
    auto matches = db->getNameBase()->getFirstMatches(nt, prefix, maxMatches);
    for (auto nameID : matches) {
        scid::database::uint freq = db->getNameFreq(nt, nameID);
        const char * str = db->getNameBase()->GetName (nt, nameID);
        appendUintElement (ti, freq);
        AppendElement (ti, str);
        if (nt == scid::database::NAME_PLAYER  &&  eloMode) {
            appendUintElement (ti, db->peakElo(nameID));
        }
    }
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_plist:
//   Returns a list of play data matching selected criteria.
struct PlayerActivity {
    scid::database::dateT firstDate;
    scid::database::dateT lastDate;

    PlayerActivity() : firstDate(scid::database::ZERO_DATE), lastDate(scid::database::ZERO_DATE) {}
    void addDate(scid::database::dateT date) {
        if (firstDate == scid::database::ZERO_DATE || date < firstDate) firstDate = date;
        if (date > lastDate) lastDate = date;
    }
};

class PListSort{
    scid::database::scidBaseT* dbase_;
    int sort_;
    const std::vector<PlayerActivity>& activity_;
    enum { SORT_ELO, SORT_GAMES, SORT_OLDEST, SORT_NEWEST, SORT_NAME };

public:
    PListSort(scid::database::scidBaseT* dbase, const std::vector<PlayerActivity>& activity, int sortOrder)
    : dbase_(dbase), sort_(sortOrder), activity_(activity) {
	}
    bool operator() (scid::database::idNumberT p1, scid::database::idNumberT p2)
    {
        int compare = 0;
        switch (sort_) {
        case SORT_ELO:
            compare = dbase_->peakElo(p2) - dbase_->peakElo(p1);
            break;
        case SORT_GAMES:
            compare = dbase_->getNameFreq(scid::database::NAME_PLAYER, p2) - dbase_->getNameFreq(scid::database::NAME_PLAYER, p1);
            break;
        case SORT_OLDEST:
             // Sort by oldest game year in ascending order:
            compare = scid::database::date_GetYear(activity_[p1].firstDate) - scid::database::date_GetYear(activity_[p2].firstDate);
            break;
        case SORT_NEWEST:
             // Sort by newest game date in descending order:
            compare = scid::database::date_GetYear(activity_[p2].lastDate) - scid::database::date_GetYear(activity_[p1].lastDate);
            break;
        }

        // If equal, resolve by comparing names, first case-insensitive and
        // then case-sensitively if still tied:
        if (compare == 0) {
            const scid::database::NameBase* nb = dbase_->getNameBase();
            const char* name1 = nb->GetName (scid::database::NAME_PLAYER, p1);
            const char* name2 = nb->GetName (scid::database::NAME_PLAYER, p2);
            compare = scid::database::strCaseCompare (name1, name2);
            if (compare == 0) { compare = scid::database::strCompare (name1, name2); }
        }
        return compare < 0;
    }
};

int
sc_name_plist (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    const char * usage = "Usage: sc_name plist [-<option> <value> ...]";

    auto dbase = db;
    const char * namePrefix = "";
    scid::database::uint minGames = 0;
    scid::database::uint maxGames = dbase->numGames();
    scid::database::uint minElo = 0;
    scid::database::uint maxElo = scid::database::MAX_ELO;
    size_t count = 10;

    static const char * options [] = {
        "-name", "-minElo", "-maxElo", "-minGames", "-maxGames",
        "-size", "-sort", NULL
    };
    enum {
        OPT_NAME, OPT_MINELO, OPT_MAXELO, OPT_MINGAMES, OPT_MAXGAMES,
        OPT_SIZE, OPT_SORT
    };

    // Valid sort types:
    static const char * sortModes [] = {
        "elo", "games", "oldest", "newest", "name", NULL
    };
    enum {
        SORT_ELO, SORT_GAMES, SORT_OLDEST, SORT_NEWEST, SORT_NAME
    };

    int sortMode = SORT_NAME;

    // Read parameters in pairs:
    int arg = 2;
    while (arg+1 < argc) {
        const char * option = argv[arg];
        const char * value = argv[arg+1];
        arg += 2;
        int index = scid::database::strUniqueMatch (option, options);
        switch (index) {
            case OPT_NAME:     namePrefix = value;                   break;
            case OPT_MINELO:   minElo = scid::database::strGetUnsigned (value);      break;
            case OPT_MAXELO:   maxElo = scid::database::strGetUnsigned (value);      break;
            case OPT_MINGAMES: minGames = scid::database::strGetUnsigned (value);    break;
            case OPT_MAXGAMES: maxGames = scid::database::strGetUnsigned (value);    break;
            case OPT_SIZE:     count = scid::database::strGetUnsigned (value); break;
            case OPT_SORT:
               sortMode = scid::database::strUniqueMatch (value, sortModes);
               break;
            default:
                return InvalidCommand (ti, "sc_name plist", options);
        }
    }

    if (arg != argc) { return errorResult (ti, usage); }
    if (sortMode == -1) return InvalidCommand (ti, "sc_name plist -sort", sortModes);


    const scid::database::NameBase* nb = dbase->getNameBase();
    scid::database::idNumberT nPlayers = nb->GetNumNames(scid::database::NAME_PLAYER);

    std::vector<scid::database::idNumberT> plist;
    for (scid::database::idNumberT id = 0; id < nPlayers; id++) {
        const char * name = nb->GetName (scid::database::NAME_PLAYER, id);
        scid::database::uint nGames = dbase->getNameFreq(scid::database::NAME_PLAYER, id);
        scid::database::ratingT elo = dbase->peakElo(id);
        if (nGames < minGames  ||  nGames > maxGames) { continue; }
        if (elo < minElo  ||  elo > maxElo) { continue; }
        if (! scid::database::strIsCasePrefix (namePrefix, name)) { continue; }
        plist.push_back(id);
    }

    std::vector<PlayerActivity> activity(nPlayers);
    for (scid::database::gamenumT gnum=0, n = dbase->numGames(); gnum < n; gnum++) {
        const scid::database::IndexEntry* ie = dbase->getIndexEntry(gnum);
        scid::database::dateT date = ie->GetDate();
        if (scid::database::date_GetYear(date) > 0) {
            activity[ie->GetWhite()].addDate(date);
            activity[ie->GetBlack()].addDate(date);
        }
    }

    count = std::min(count, plist.size());
    std::partial_sort(plist.begin(), plist.begin() + count, plist.end(), PListSort(dbase, activity, sortMode));

    UI_List res(count);
    UI_List info(5);
    for (size_t i=0; i < count; i++) {
        scid::database::idNumberT id = plist[i];
        info.clear();
        info.push_back(dbase->getNameFreq(scid::database::NAME_PLAYER, id));
        info.push_back(scid::database::date_GetYear(activity[id].firstDate));
        info.push_back(scid::database::date_GetYear(activity[id].lastDate));
        info.push_back(dbase->peakElo(id));
        info.push_back(nb->GetName(scid::database::NAME_PLAYER, id));
        res.push_back(info);
    }

    return UI_Result(ti, scid::database::OK, res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_ratings:
//   Scan the current database for games with unrated players who
//   have Elo rating information in the spellcheck file, and fill
//   in the missing Elo ratings.
//
//   Boolean options:
//      -nomonth (default=true): indicates whether games with no month
//           should still have ratings allocated, assuming the month
//           to be January.
//      -update (default=true): indicates whether the database should
//           be updated; if it is false, no actual changes are made.
//      -debug (default=false): for debugging; it dumps all detected
//           rating changes, one per line, to stdout.
//      -test (default=false): tests whether a sspelling file with
//           rating info is loaded and can be used on this database.
//      -overwrite (default=false): if true, existing ratings can be
//           changed.
//
//   Returns a two-integer list: the number of changed ratings, and
//   the number of changed games.
UI_res_t sc_name_ratings (UI_handle_t ti, scid::database::scidBaseT& dbase, const scidup::spelling::SpellChecker& sp, int argc, const char ** argv)
{
    const char * options[] = {
        "-update", "-test", "-change", "-filter" };
    enum {
        OPT_UPDATE, OPT_TEST, OPT_CHANGE, OPT_FILTER
    };

    bool updateIndexFile = true;
    bool testOnly = false;
    bool overwrite = false;
    bool filterOnly = false;

    int arg = 2;
    while (arg+1 < argc) {
        int option = scid::database::strUniqueMatch (argv[arg], options);
        bool value = scid::database::strGetBoolean (argv[arg+1]);
        switch (option) {
            case OPT_UPDATE:    updateIndexFile = value;    break;
            case OPT_TEST:      testOnly = value;           break;
            case OPT_CHANGE:    overwrite = value;          break;
            case OPT_FILTER:    filterOnly = value;         break;
            default: return InvalidCommand (ti, "sc_name ratings", options);
        }
        arg += 2;
    }

    if (! sp.hasEloData()) {
        return UI_Result(ti, scid::database::ERROR, "The current spellcheck file does not have "
                          "Elo rating information.\n\n"
                          "To use this function, you should load "
                          "\"ratings.ssp\" (available from the Scid website) "
                          "as your spellcheck file first.");
    }

    if (testOnly) { return UI_Result(ti, scid::database::OK); }

    scid::database::uint numChangedRatings = 0;
    scid::database::uint numChangedGames = 0;
    const scid::database::NameBase* nb = dbase.getNameBase();
    std::vector<bool> cached(nb->GetNumNames(scid::database::NAME_PLAYER), false);
    std::vector<const scidup::spelling::PlayerElo*> vElo(nb->namebase_size(scid::database::NAME_PLAYER), NULL);

    auto getElo = [&](scid::database::idNumberT id, scid::database::dateT date) {
        if (!cached[id]) {
            cached[id] = true;
            vElo[id] = sp.getPlayerElo(nb->GetName(scid::database::NAME_PLAYER, id));
        }
        return (vElo[id]) ? vElo[id]->getElo(date) : 0;
    };

    auto entry_op = [&](scid::database::IndexEntry& ie) {
        scid::database::dateT date = ie.GetDate();
        scid::database::ratingT eloWhite = (!overwrite && ie.GetWhiteElo() != 0)
                            ? 0
                            : getElo(ie.GetWhite(), date);
        scid::database::ratingT eloBlack = (!overwrite && ie.GetBlackElo() != 0)
                            ? 0
                            : getElo(ie.GetBlack(), date);
        unsigned nChanges = (eloWhite != 0) ? 1 : 0;
        nChanges += (eloBlack != 0) ? 1 : 0;
        if (nChanges) {
            numChangedRatings += nChanges;
            numChangedGames++;
            if (updateIndexFile) {
                if (eloWhite != 0) {
                    ie.SetWhiteElo(eloWhite);
                    ie.SetWhiteRatingType(scid::database::RATING_Elo);
                }
                if (eloBlack != 0) {
                    ie.SetBlackElo(eloBlack);
                    ie.SetBlackRatingType(scid::database::RATING_Elo);
                }
                return true;
            }
        }
        return false;
    };

    std::string filter = (filterOnly) ? "dbfilter" : dbase.newFilter();
    auto hf = scidup::app::tree::resolveFilter(dbase, filter);
    auto changes = dbase.transformIndex(hf, UI_CreateProgress(ti), entry_op);
    if (!filterOnly)
        dbase.deleteFilter(filter.c_str());

    UI_List res(2);
    res.push_back(numChangedRatings);
    res.push_back(numChangedGames);
    return UI_Result(ti, changes.first, res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_read:
//   Reads a Scid name spelling file into memory, and returns a list of
//   four integers: the number of player, event, site and round names in
//   the file.
//   If there is no filename argument, sc_name_read just returns the same
//   list for the current spellchecker status without reading a new file.
int
sc_name_read (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc > 5) {
        return UI_Result(ti, scid::database::ERROR_BadArg, "Usage: sc_name read <spellcheck-file>");
    }

    if (argc > 2) {
        const char * filename = argv[2];
        scid::database::Progress progress = UI_CreateProgress(ti);
        std::pair<scid::database::errorT, std::unique_ptr<scidup::spelling::SpellChecker>> newSpell =
            scidup::spelling::SpellChecker::create(filename, progress);
        if (newSpell.first != scid::database::OK) {
            return UI_Result(ti, newSpell.first, "Error reading name spellcheck file.");
        }
        spellChk = std::move(newSpell.second);
        progress.report(1, 1);
    }

    UI_List res(scid::database::NUM_NAME_TYPES);
    for (scid::database::nameT i = 0; i < scid::database::NUM_NAME_TYPES; i++) {
        size_t n = (spellChk == NULL) ? 0 : spellChk->numCorrectNames(i);
        res.push_back(n);
    }
    return UI_Result(ti, scid::database::OK, res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_name_spellcheck:
//   Scan the current database for spelling corrections.
UI_res_t sc_name_spellcheck (UI_handle_t ti, scid::database::scidBaseT& dbase, const scidup::spelling::SpellChecker& sp, int argc, const char ** argv)
{
    scid::database::nameT nt = scid::database::NAME_INVALID;
    scid::database::uint maxCorrections = 20000;
    bool doSurnames = false;
    bool ambiguous = true;
    const char * usage = "Usage: sc_name spellcheck [-max <integer>] [-surnames <boolean>] [-ambiguous <boolean>] players|events|sites|rounds";

    const char * options[] = {
        "-max", "-surnames", "-ambiguous", NULL
    };
    enum {
        OPT_MAX, OPT_SURNAMES, OPT_AMBIGUOUS
    };

    int arg = 2;
    while (arg+1 < argc) {
        const char * option = argv[arg];
        const char * value = argv[arg+1];
        arg += 2;
        int index = -1;
        if (option[0] == '-') { index = scid::database::strUniqueMatch (option, options); }

        switch (index) {
        case OPT_MAX:
            maxCorrections = scid::database::strGetUnsigned (value);
            if ( maxCorrections == 0 ) {
                maxCorrections = (scid::database::uint)-1;
            }
            break;
        case OPT_SURNAMES:
            doSurnames = scid::database::strGetBoolean (value);
            break;
        case OPT_AMBIGUOUS:
            ambiguous = scid::database::strGetBoolean (value);
            break;
        default:
            return InvalidCommand (ti, "sc_name spellcheck", options);
        }
    }
    if (arg+1 != argc) { return errorResult (ti, usage); }
    nt = scid::database::NameBase::NameTypeFromString (argv[arg]);

    if (! scid::database::NameBase::IsValidNameType (nt)) {
        return errorResult (ti, usage);
    }

    const scid::database::NameBase* nb = dbase.getNameBase();
    std::vector<std::string> tmpRes;
    char tempStr[1024];
    scid::database::uint correctionCount = 0;

    scid::database::Progress progress = UI_CreateProgress(ti);
    // Check every name of the specified type:
    for (scid::database::idNumberT id=0, n=nb->GetNumNames(nt); id < n; id++) {
        if (correctionCount >= maxCorrections) break;

        if ((id % 1000) == 0) {  // Update the percentage done bar:
            if (!progress.report(id, n)) break;
        }

        scid::database::uint frequency = db->getNameFreq(nt, id);
        // Do not bother trying to correct unused names:
        if (frequency == 0) continue;

        const char* origName = nb->GetName(nt, id);
        // If requested ignore surnames
        if (nt == scid::database::NAME_PLAYER  &&  !doSurnames  && scid::database::strIsSurnameOnly (origName)) continue;

        // First, check for a general prefix or suffix correction:
        std::string name = origName;
        size_t nGenCorrections = sp.getGeneralCorrections(nt).normalize(&name);

        // If spellchecking names, remove any country code like " (USA)"
        // in parentheses at the end of the name:
        if (nt == scid::database::NAME_PLAYER) {
            size_t country = name.rfind(" (");
            if (country != std::string::npos && (country + 6) == name.length()) {
                if (*(name.rbegin()) == ')') name.erase(country);
            }
        }

        std::vector<const char*> corrections = sp.find(nt, name.c_str());
        // If requested ignore ambiguous corrections
        if (!ambiguous && corrections.size() > 1) continue;

        if (nGenCorrections != 0 && corrections.size() == 0) {
            corrections.push_back(name.c_str());
        }

        std::string correctCmd;
        const char* strAmbiguous = corrections.size() > 1 ? "Ambiguous: " : "";
        for (size_t i=0; i < corrections.size(); i++) {
            if (strcmp(origName, corrections[i]) == 0) {
                if (corrections.size() != 1) {
                    correctCmd.append("ERROR: " + name);
                }
                continue;
            }
            if (i==0) correctionCount++;

            std::snprintf(tempStr, sizeof(tempStr), "%s\"%s\"\t>> \"%s\" (%u)",
                              strAmbiguous,
                              origName,
                              corrections[i],
                              frequency);
            correctCmd += tempStr;

            if (nt == scid::database::NAME_PLAYER) { // Look for a player birthdate:
                const scidup::spelling::PlayerInfo* pInfo = sp.getPlayerInfo(corrections[i]);
                scid::database::dateT birthdate = pInfo->getBirthdate();
                scid::database::dateT deathdate = pInfo->getDeathdate();
                if (birthdate != scid::database::ZERO_DATE  ||  deathdate != scid::database::ZERO_DATE) {
                    correctCmd += "  ";
                    if (birthdate != scid::database::ZERO_DATE) {
                        scid::database::date_DecodeToString (birthdate, tempStr);
                        correctCmd += tempStr;
                    }
                    correctCmd += "--";
                    if (deathdate != scid::database::ZERO_DATE) {
                        scid::database::date_DecodeToString (deathdate, tempStr);
                        correctCmd += tempStr;
                    }
                }
            }
            correctCmd += "\n";
        }

        if (!correctCmd.empty()) tmpRes.push_back(correctCmd);
    }

    std::sort(tmpRes.begin(), tmpRes.end());

    progress.report(1,1);

    // Now generate the return message:
    static const char* NAME_TYPE_STRING[] = {"player", "event", "site", "round"};
    std::snprintf(tempStr, sizeof(tempStr), "Scid found %u %s name correction%s.\n",
             correctionCount, NAME_TYPE_STRING[nt],
             scid::database::strPlural (correctionCount));
    std::string res = tempStr;
    res +=
        "Edit the list to remove any corrections you do not want.\n"
        "Only lines of the form:\n"
        "   \"Old Name\" >> \"New Name\"\n"
        "(with no spaces before the \"Old Name\") are processed.\n"
        "You can discard a correction you do not want by deleting\n"
        "its line, or simply by adding a space or any other character\n"
        "at the start of the line.\n";
    if (nt == scid::database::NAME_PLAYER  &&  ! doSurnames) {
        res +=
            "Note: player names with a surname only, such as \"Kramnik\",\n"
            "have not been corrected, since such corrections are often\n"
            "wrong. You can choose to also show surname-only corrections\n"
            "using the button below.\n";
    }
    res += "\n";
    std::vector<std::string>::const_iterator it = tmpRes.begin();
    std::vector<std::string>::const_iterator it_Ambiguous =
        std::lower_bound(tmpRes.begin(), tmpRes.end(), "Ambig");
    for (;it != it_Ambiguous; it++) {
        res += *it;
    }
    for (;it != tmpRes.end(); it++) {
        res += "\n";
        res += *it;
    }
    return UI_Result(ti, scid::database::OK, res);
}

UI_res_t sc_name(UI_extra_t cd, UI_handle_t ti, int argc, const char** argv) {
	static const char * options [] = {
        "correct", "edit", "info", "match", "plist",
        "ratings", "read", "spellcheck", "retrievename", "elo",
        NULL
    };
    enum {
        OPT_CORRECT, OPT_EDIT, OPT_INFO, OPT_MATCH, OPT_PLIST,
        OPT_RATINGS, OPT_READ, OPT_SPELLCHECK, OPT_RETRIEVENAME, OPT_ELO
    };

    int index = -1;
    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    if (!db->isOpen()) {
        return errorResult (ti, scid::database::ERROR_FileNotOpen, errMsgNotOpen(ti));
    }

    switch (index) {
    case OPT_INFO:
        return sc_name_info (cd, ti, argc, argv);

    case OPT_MATCH:
        return sc_name_match (cd, ti, argc, argv);

    case OPT_PLIST:
        return sc_name_plist (cd, ti, argc, argv);

    case OPT_READ:
        return sc_name_read (cd, ti, argc, argv);
    }

    if (db->isReadOnly() && index != OPT_RETRIEVENAME) {
        return errorResult (ti, scid::database::ERROR_FileReadOnly);
    }

    switch (index) {
    case OPT_CORRECT:
        return sc_name_correct (cd, ti, argc, argv);

    case OPT_EDIT:
        return sc_name_edit (cd, ti, argc, argv);
    };

    if (spellChk == NULL) {
        return UI_Result(ti, scid::database::ERROR,
            "A spellcheck file has not been loaded.\n\n"
            "You can load one from the Options menu.");
    }

    switch (index) {
    case OPT_RATINGS:
        return sc_name_ratings(ti, *db, *spellChk, argc, argv);

    case OPT_RETRIEVENAME:
        return sc_name_retrievename(ti, *spellChk, argc, argv);

    case OPT_SPELLCHECK:
        return sc_name_spellcheck(ti, *db, *spellChk, argc, argv);

    case OPT_ELO:
        return sc_name_elo (ti, *spellChk, argc, argv);

    default:
        return InvalidCommand (ti, "sc_name", options);
    }

    return TCL_OK;
}

//////////////////////////////////////////////////////////////////////
//  OPENING/PLAYER REPORT functions

static scid::database::uint
avgGameLength (scid::database::resultT result)
{
    scid::database::uint sum = 0;
    scid::database::uint count = 0;
    for (scid::database::gamenumT i=0, n = db->numGames(); i < n; i++) {
        const scid::database::IndexEntry* ie = db->getIndexEntry(i);
        if (result == ie->GetResult()) {
            count++;
            sum += ((ie->GetNumHalfMoves() + 1) / 2);
        }
    }
    if (count == 0) { return 0; }
    return (sum + (count/2)) / count;
}

int
sc_report (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * options [] = {
        "avgLength", "best", "counts", "create", "eco", "elo",
        "endmaterial", "format", "frequency", "line", "max", "moveOrders",
        "notes", "players", "print", "score", "select", "themes", NULL
    };
    enum {
        OPT_AVGLENGTH, OPT_BEST, OPT_COUNTS, OPT_CREATE, OPT_ECO, OPT_ELO,
        OPT_ENDMAT, OPT_FORMAT, OPT_FREQ, OPT_LINE, OPT_MAX, OPT_MOVEORDERS,
        OPT_NOTES, OPT_PLAYERS, OPT_PRINT, OPT_SCORE, OPT_SELECT, OPT_THEMES
    };

    static const char * usage =
        "Usage: sc_report opening|player <command> [<options...>]";
    OpTable * report = NULL;
    if (argc < 2) {
        return errorResult (ti, usage);
    }
    switch (argv[1][0]) {
        case 'O': case 'o':  report = reports[REPORT_OPENING]; break;
        case 'P': case 'p':  report = reports[REPORT_PLAYER]; break;
        default:
            return errorResult (ti, usage);
    }

    scid::database::DString * dstr = NULL;
    int index = scid::database::strUniqueMatch (argv[2], options);

    if (! db->isOpen()) {
        return errorResult (ti, errMsgNotOpen(ti));
    }
    if (index >= 0  &&  index != OPT_CREATE  &&  report == NULL) {
        return errorResult (ti, "No report has been created yet.");
    }

    auto const& stats = db->getStats();
    switch (index) {
    case OPT_AVGLENGTH:
        if (argc != 4) {
            return errorResult (ti, "Usage: sc_report player|opening avgLength 1|=|0|*");
        } else {
            scid::database::resultT result = scid::database::strGetResult (argv[3]);
            appendUintElement (ti, report->AvgLength (result));
            appendUintElement (ti, avgGameLength (result));
        }
        break;

    case OPT_BEST:
        if (argc != 5) {
            return errorResult (ti, "Usage: sc_report opening|player best w|b|a|o|n <count>");
        }
        dstr = new scid::database::DString;
        report->BestGames (dstr, scid::database::strGetUnsigned(argv[4]), argv[3]);
        AppendResult (ti, dstr->Data(), NULL);
        break;

    case OPT_COUNTS:
        appendUintElement (ti, report->GetTotalCount());
        appendUintElement (ti, report->GetTheoryCount());
        break;

    case OPT_CREATE:
        return sc_report_create (cd, ti, argc, argv);

    case OPT_ECO:
        if (argc > 3) {
            dstr = new scid::database::DString();
            report->TopEcoCodes (dstr, scid::database::strGetUnsigned(argv[3]));
            AppendResult (ti, dstr->Data(), NULL);
        } else {
            AppendResult (ti, report->GetEco(), NULL);
        }
        break;

    case OPT_ELO:
        if (argc != 4) {
            return errorResult (ti, "Usage: sc_report opening|player elo white|black");
        } else {
            scid::database::colorT color = scid::database::WHITE;
            scid::database::uint count = 0;
            scid::database::uint pct = 0;
            scid::database::uint perf = 0;
            if (argv[3][0] == 'B'  ||  argv[3][0] == 'b') { color = scid::database::BLACK; }
            scid::database::uint avg = report->AvgElo (color, &count, &pct, &perf);
            appendUintElement (ti, avg);
            appendUintElement (ti, count);
            appendUintElement (ti, pct);
            appendUintElement (ti, perf);
        }
        break;

    case OPT_ENDMAT:
        dstr = new scid::database::DString;
        report->EndMaterialReport (dstr,
                       translate (ti, "OprepReportGames", "Report games"),
                       translate (ti, "OprepAllGames", "All games"));
        AppendResult (ti, dstr->Data(), NULL);
        break;

    case OPT_FORMAT:
        if (argc != 4) {
            return errorResult (ti, "Usage: sc_report opening|player format latex|html|text|ctext");
        }
        report->SetFormat (argv[3]);
        break;

    case OPT_FREQ:
        if (argc != 4) {
            return errorResult (ti, "Usage: sc_report opening|player frequency 1|=|0|*");
        } else {
            scid::database::resultT result = scid::database::strGetResult (argv[3]);
            appendUintElement (ti, report->PercentFreq (result));
            scid::database::uint freq = stats.nResults[result] * 1000;
            freq = freq / db->numGames();
            appendUintElement (ti, freq);
        }
        break;

    case OPT_LINE:
        dstr = new scid::database::DString;
        report->PrintStemLine (dstr);
        AppendResult (ti, dstr->Data(), NULL);
        break;

    case OPT_MAX:
        if (argc == 4  &&  argv[3][0] == 'g') {
            return setUintResult (ti, OPTABLE_MAX_TABLE_LINES);
        } else if (argc == 4  &&  argv[3][0] == 'r') {
            return setUintResult (ti, OPTABLE_MAX_ROWS);
        }
        return errorResult (ti, "Usage: sc_report opening|player max games|rows");

    case OPT_MOVEORDERS:
        if (argc != 4) {
            return errorResult (ti, "Usage: sc_report opening|player moveOrders <count>");
        }
        dstr = new scid::database::DString;
        report->PopularMoveOrders (dstr, scid::database::strGetUnsigned(argv[3]));
        AppendResult (ti, dstr->Data(), NULL);
        break;

    case OPT_NOTES:
        if (argc < 4  ||  argc > 5) {
            return errorResult (ti, "Usage: sc_report opening|player notes <0|1> [numrows]");
        }
        report->ClearNotes ();
        if (scid::database::strGetBoolean (argv[3])  &&  report->GetNumLines() > 0) {
            report->GuessNumRows ();
            if (argc > 4) {
                scid::database::uint nrows = scid::database::strGetUnsigned (argv[4]);
                if (nrows > 0) { report->SetNumRows (nrows); }
            }
            dstr = new scid::database::DString;
            // Print the table just to set up notes, but there is
            // no need to return the result:
            report->PrintTable (dstr, "", "");
        }
        break;

    case OPT_PLAYERS:
        if (argc != 5) {
            return errorResult (ti, "Usage: sc_report opening|player players w|b <count>");
        } else {
            scid::database::colorT color = scid::database::WHITE;
            if (argv[3][0] == 'B'  ||  argv[3][0] == 'b') { color = scid::database::BLACK; }
            dstr = new scid::database::DString;
            report->TopPlayers (dstr, color, scid::database::strGetUnsigned(argv[4]));
            AppendResult (ti, dstr->Data(), NULL);
        }
        break;

    case OPT_PRINT:
        if (argc < 3  ||  argc > 6) {
            return errorResult (ti, "Usage: sc_report opening|players print [numrows] [title] [comment]");
        }
        report->GuessNumRows ();
        if (argc > 3) {
            scid::database::uint nrows = scid::database::strGetUnsigned (argv[3]);
            if (nrows > 0) { report->SetNumRows (nrows); }
        }
        dstr = new scid::database::DString;
        report->PrintTable (dstr, argc > 4 ? argv[4] : "",
                             argc > 5 ? argv[5] : "");
        AppendResult (ti, dstr->Data(), NULL);
        break;

    case OPT_SCORE:
        appendUintElement (ti, report->PercentScore());
        {
            scid::database::uint percent = stats.nResults[scid::database::RESULT_White] * 2;
            percent += stats.nResults[scid::database::RESULT_Draw];
            percent = percent * 500;
            scid::database::uint sum = (stats.nResults[scid::database::RESULT_White] +
                                 stats.nResults[scid::database::RESULT_Draw] +
                                 stats.nResults[scid::database::RESULT_Black]);
            if (sum != 0)
            	percent = percent / sum;
            	else
            	percent = 0;
            appendUintElement (ti, percent);
        }
        break;

    case OPT_SELECT:
        return sc_report_select (cd, ti, argc, argv);

    case OPT_THEMES:
        dstr = new scid::database::DString;
        report->ThemeReport (dstr, argc - 3, (const char **) argv + 3);
        AppendResult (ti, dstr->Data(), NULL);
        break;

    default:
        return InvalidCommand (ti, "sc_report", options);
    }

    if (dstr != NULL) { delete dstr; }
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_report_create:
//    Creates a new opening table.
//    NOTE: It assumes the filter contains the results
//    of a tree search for the current position, so
//    the Tcl code that calls this need to ensure that
//    is done first.
int
sc_report_create (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    scid::database::uint maxThemeMoveNumber = 20;
    scid::database::uint maxExtraMoves = 1;
    scid::database::uint maxLines = OPTABLE_MAX_TABLE_LINES;
    static const char * usage =
        "Usage: sc_report opening|player create [maxExtraMoves] [maxLines] [excludeMove]";

    scid::database::uint reportType = 0;
    if (argc < 2) {
        return errorResult (ti, usage);
    }

    switch (argv[1][0]) {
        case 'O': case 'o':  reportType = REPORT_OPENING; break;
        case 'P': case 'p':  reportType = REPORT_PLAYER; break;
        default:
            return errorResult (ti, usage);
    }

    if (argc > 3) {
        maxExtraMoves = scid::database::strGetUnsigned (argv[3]);
    }
    if (argc > 4) {
        maxLines = scid::database::strGetUnsigned (argv[4]);
        if (maxLines > OPTABLE_MAX_TABLE_LINES) {
            maxLines = OPTABLE_MAX_TABLE_LINES;
        }
        if (maxLines == 0) { maxLines = 1; }
    }
    const char * excludeMove = "";
    if (argc > 5) { excludeMove = argv[5]; }
    if (excludeMove[0] == '-') { excludeMove = ""; }

    if (reports[reportType] != NULL) {
        delete reports[reportType];
    }
    auto editor = scidup::app::editor::gameSession(*db);
    OpTable* report =
        new OpTable(reportTypeName[reportType], &editor.game(), ecoBook.get());
    reports[reportType] = report;
    report->SetMaxTableLines (maxLines);
    report->SetExcludeMove (excludeMove);
    report->SetDecimalChar (decimalPointChar);
    report->SetMaxThemeMoveNumber (maxThemeMoveNumber);

    scid::database::Progress progress = UI_CreateProgress(ti);

    for (scid::database::uint gnum=0, n = db->numGames(); gnum < n; gnum++) {
        if ((gnum % 2000) == 0) {  // Update the percentage done bar:
            if (!progress.report(gnum, n)) break;
        }

        scid::database::byte ply = db->defaultFilterGet(gnum);
        const scid::database::IndexEntry* ie = db->getIndexEntry(gnum);
        if (ply != 0) {
            if (db->getGame(*ie, *scratchGame) != scid::database::OK) {
                return errorResult (ti, "Error reading game file.");
            }
            {
                scid::core::GameCursor cursor(scratchGame->coreGame());
                if (!cursor.toPly(ply - 1))
                    cursor.toEnd();
                scratchGame->restoreLocation(cursor.location());
            }
            if (isAtEnd(*scratchGame)) ply = 0;
            if (ply != 0) {
                scid::database::uint moveOrderID = report->AddMoveOrder (scratchGame);
                OpLine * line = new OpLine (scratchGame, ie, gnum+1,
                                            maxExtraMoves, maxThemeMoveNumber);
                if (report->Add (line)) {
                    line->SetMoveOrderID (moveOrderID);
                } else {
                    delete line;
                }
            }
        }
        report->AddEndMaterial (ie->GetFinalMatSig(), (ply != 0));
    }
    progress.report(1,1);

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_report_select:
//    Restricts the filter to only contain games
//    in the opening report matching the specified
//    opening/endgame theme or note number.
int
sc_report_select (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * usage =
        "Usage: sc_report opening|player select <op|eg|note> <number>";
    if (argc != 5) {
        return errorResult (ti, usage);
    }
    OpTable * report = NULL;
    switch (argv[1][0]) {
        case 'O': case 'o':  report = reports[REPORT_OPENING]; break;
        case 'P': case 'p':  report = reports[REPORT_PLAYER]; break;
        default:
            return errorResult (ti, usage);
    }

    char type = tolower (argv[3][0]);
    scid::database::uint number = scid::database::strGetUnsigned (argv[4]);

    scid::database::uint * matches = report->SelectGames (type, number);
    scid::database::uint * match = matches;
    db->defaultFilterFill(0);
    while (*match != 0) {
        scid::database::uint gnum = *match - 1;
        match++;
        scid::database::uint ply = *match + 1;
        match++;
        db->defaultFilterSet(gnum, ply);
    }
    delete[] matches;

    return TCL_OK;
}


//////////////////////////////////////////////////////////////////////
//  SEARCH and TREE functions

int
sc_tree (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * options [] = {
        "stats", "cachesize", "cacheinfo", NULL
    };
    enum {
        TREE_STATS, TREE_CACHESIZE, TREE_CACHEINFO
    };

    int index = -1;
    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    switch (index) {
    case TREE_STATS:
        return sc_tree_stats(cd, ti, argc, argv);

    case TREE_CACHESIZE:
        return sc_tree_cachesize (cd, ti, argc, argv);

    case TREE_CACHEINFO:
        return sc_tree_cacheinfo (cd, ti, argc, argv);

    default:
        return InvalidCommand (ti, "sc_tree", options);
    }

    return TCL_OK;
}

// @returns the tree stats of the specified filter
int
sc_tree_stats (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * usage =
      "Usage: sc_tree stats baseId filterId [<0|1>] [alpha|eco|frequency|score]";

    // Sort options: these should match the moveSortE enumerated type.
    static const char * sortOptions[] = {
        "alpha", "eco", "frequency", "score", NULL
    };
    // Enumeration of possible move-sorting methods for tree mode:
    enum moveSortE { SORT_ALPHA, SORT_ECO, SORT_FREQUENCY, SORT_SCORE };

    if (argc < 4)
        return UI_Result(ti, scid::database::ERROR_BadArg, usage);

    scid::database::scidBaseT* base = DBasePool::getBase(scid::database::strGetUnsigned(argv[2]));
    if (!base)
        return UI_Result(ti, scid::database::ERROR_BadArg, usage);

    scid::database::HFilter filter = scidup::app::tree::resolveFilter(*base, argv[3]);
    if (filter == nullptr)
        return UI_Result(ti, scid::database::ERROR_BadArg, usage);

    bool hideMoves = (argc > 4) ? scid::database::strGetBoolean(argv[4]) : false;
    int sortMethod = (argc > 5) ? scid::database::strUniqueMatch(argv[5], sortOptions)
                                : SORT_FREQUENCY;
    if (sortMethod < 0)
        return UI_Result(ti, scid::database::ERROR_BadArg, usage);

    auto editor = scidup::app::editor::gameSession(*db);
    auto current = currentPosition(editor.game());
    if (!current)
        return UI_Result(ti, scid::database::ERROR, "Error reading position.");
    scid::database::Position searchPos = *current;
    auto tree = base->getTreeStat(filter);

    auto calc_eco = [&](auto const& move) {
        scidup::eco::Code eco = scidup::eco::ECO_None;
        if (ecoBook && move) {
            scid::database::simpleMoveT sm;
            if (move.isCastle()) {
                auto side = move.getTo() > move.getFrom() ? scid::database::KING : scid::database::QUEEN;
                searchPos.makeMove(move.getFrom(), move.getFrom(), side, sm);
            } else {
                auto promo = move.isPromo() ? move.getPromo() : scid::database::INVALID_PIECE;
                searchPos.makeMove(move.getFrom(), move.getTo(), promo, sm);
            }
            searchPos.DoSimpleMove(sm);
            eco = ecoBook->findEco(searchPos);
            searchPos.UndoSimpleMove(sm);
        }
        return eco;
    };

    scid::database::sanStringT tempTrans;
    auto calc_san = [&](auto const& move) {
        strcpy(tempTrans, move ? move.getSAN().c_str() : "[end]");
        scid::database::transPieces(tempTrans);
    };

    if (sortMethod == SORT_ALPHA) { // icase alphabetical order
        std::sort(tree.begin(), tree.end(), [&](auto const& a, auto const& b) {
            calc_san(a.move);
            std::string temp = tempTrans;
            calc_san(b.move);
            return temp.compare(tempTrans) < 0;
        });

    } else if (sortMethod == SORT_ECO) { // Order by eco code
        std::sort(tree.begin(), tree.end(), [&](auto const& a, auto const& b) {
            return calc_eco(a.move) < calc_eco(b.move);
        });

    } else if (sortMethod == SORT_SCORE) { // Order by success
        if (searchPos.GetToMove() == scid::database::WHITE) {
            std::sort(tree.begin(), tree.end(),
                      [&](auto const& a, auto const& b) {
                          return a.score() > b.score();
                      });
        } else {
            std::sort(tree.begin(), tree.end(),
                      [&](auto const& a, auto const& b) {
                          return a.score() < b.score();
                      });
        }
    }

    char temp[256];
    std::string output;
    const char * titleRow =
        "    Move   ECO       Frequency    Score  AvElo Perf AvYear %Draws";
    titleRow = translate (ti, "TreeTitleRow", titleRow);
    output.append(titleRow);

    auto format_output = [&](auto const& node, auto& dest) {
        const auto avgElo = static_cast<int>(node.avgElo());
        const auto avgYear = static_cast<int>(node.avgYear());
        const auto pctDraws = static_cast<int>(node.percDraws());
        const auto score = node.score();
        const auto perf = node.eloCount < 10
                              ? 0
                              : static_cast<int>(node.eloPerformance());

        std::snprintf(temp, sizeof(temp), "  %3d%c%1d%%", score / 10,
                      decimalPointChar, score % 10);
        dest.append(temp);
        if (avgElo == 0) {
            dest.append("      ");
        } else {
            std::snprintf(temp, sizeof(temp), "  %4d", avgElo);
            dest.append(temp);
        }
        if (perf == 0) {
            dest.append("      ");
        } else {
            std::snprintf(temp, sizeof(temp), "  %4d", perf);
            dest.append(temp);
        }
        if (avgYear == 0) {
            dest.append("      ");
        } else {
            std::snprintf(temp, sizeof(temp), "  %4d", avgYear);
            dest.append(temp);
        }
        std::snprintf(temp, sizeof(temp), "  %3d%%", pctDraws);
        dest.append(temp);
    };

    if (tree.size() > 0) {
        scid::database::TreeNode totals({});
        for (auto const& node : tree) { // reduce
            totals.freq[0] += node.freq[0];
            totals.freq[scid::database::RESULT_White] += node.freq[scid::database::RESULT_White];
            totals.freq[scid::database::RESULT_Black] += node.freq[scid::database::RESULT_Black];
            totals.freq[scid::database::RESULT_Draw] += node.freq[scid::database::RESULT_Draw];
            totals.eloWhiteSum += node.eloWhiteSum;
            totals.eloBlackSum += node.eloBlackSum;
            totals.eloCount += node.eloCount;
            totals.yearSum += node.yearSum;
            totals.yearCount += node.yearCount;
        }

        // Now we print the list into the return string:
        unsigned count = 0;
        for (auto const& node : tree) {
            calc_san(node.move);
            scidup::eco::Code eco = calc_eco(node.move);
            scidup::eco::String ecoStr;
            scidup::eco::toExtendedString(eco, ecoStr);
            auto freq = long(1000ll * node.freq[0] / totals.freq[0]);
            std::snprintf(temp, sizeof(temp), "\n%2u: %-6s %-5s %7u:%3ld%c%1ld%%",
                     ++count,
                     hideMoves ? "---" : tempTrans,//node->san,
                     hideMoves ? "" : ecoStr,
                     node.freq[0],
                     freq / 10,
                     decimalPointChar,
                     freq % 10);
            output.append(temp);
            format_output(node, output);
        }

        // Print a totals line as well, if there are any moves in the tree:
        const char * totalString = translate (ti, "TreeTotal:", "TOTAL:");
        output.append("\n_______________________________________________________________\n");
        std::snprintf(temp, sizeof(temp), "%-12s     %7u:100%c0%%",
                 totalString, totals.freq[0], decimalPointChar);
        output.append(temp);
        format_output(totals, output);
        output.append("\n");
    }
    return UI_Result(ti, scid::database::OK, output);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_tree_cachesize:
//    set cache size
int
sc_tree_cachesize (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
  if (argc != 4) {
    return errorResult (ti, "Usage: sc_tree cachesize <base> <size>");
  }
  auto base = DBasePool::getBase(scid::database::strGetInteger(argv[2]));
  if (base) scidup::app::tree::session(*base).cacheResize(scid::database::strGetUnsigned(argv[3]));
  return TCL_OK;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_tree_cacheinfo:
//    returns a list of 2 values : used slots and max cache size
int
sc_tree_cacheinfo (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
  if (argc != 3) {
    return errorResult (ti, "Usage: sc_tree cacheinfo <base>");
  }
  auto base = DBasePool::getBase(scid::database::strGetInteger(argv[2]));
  if (base)
      return UI_Result(ti, scid::database::OK, scidup::app::tree::session(*base).cacheSize());

  return UI_Result(ti, scid::database::OK, 0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_search:
//    Search function interface.
int
sc_search (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * options [] = {
        "board", "header", "material", NULL
    };
    enum { OPT_BOARD, OPT_HEADER, OPT_MATERIAL };

    int index = -1;
    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }
    int ret = TCL_OK;

    if (!db->isOpen()) {
        return errorResult (ti, errMsgNotOpen(ti));
    }

    switch (index) {
    case OPT_BOARD:
        ret = sc_search_board (ti, db, db->getFilter("dbfilter"), argc, argv);
        break;

    case OPT_HEADER: {
        scid::database::HFilter filter = db->getFilter("dbfilter");
        ret = sc_search_header (cd, ti, db, filter, argc, argv);
        break;
    }

    case OPT_MATERIAL:
        ret = sc_search_material (cd, ti, argc, argv);
        break;

    default:
        return InvalidCommand (ti, "sc_search", options);
    }

    return ret;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_search_board:
//    Searches for exact match for the current position.
//    if <base> is present, search for current position in base <base>,
//    and sets <base> filter accordingly
int sc_search_board(Tcl_Interp* ti, const scid::database::scidBaseT* dbase, scid::database::HFilter filter,
                    int argc, const char** argv) {
	ASSERT(dbase != nullptr && filter != nullptr);

	const char* usageStr =
	    "Usage: sc_search board <filterOp> <searchType> <searchInVars> <flip>";

	if (argc != 6)
		return errorResult(ti, usageStr);

    scid::database::filterOpT filterOp = scid::database::strGetFilterOp (argv[2]);

    bool useHpSigSpeedup = false;
    scid::database::gameExactMatchT searchType = scid::database::GAME_EXACT_MATCH_Exact;

    switch (argv[3][0]) {
    case 'E':
        searchType = scid::database::GAME_EXACT_MATCH_Exact;
        useHpSigSpeedup = true;
        break;
    case 'P':
        searchType = scid::database::GAME_EXACT_MATCH_Pawns;
        useHpSigSpeedup = true;
        break;
    case 'F':
        searchType = scid::database::GAME_EXACT_MATCH_Fyles;
        break;
    case 'M':
        searchType = scid::database::GAME_EXACT_MATCH_Material;
        break;
    default:
        return errorResult (ti, usageStr);
    }

    bool searchInVars = scid::database::strGetBoolean (argv[4]);
    bool flip = false;
    flip = scid::database::strGetBoolean (argv[5]);

    auto editor = scidup::app::editor::gameSession(*db);
    auto position = currentPosition(editor.game());
    if (!position)
        return UI_Result(ti, scid::database::ERROR, "Error reading position.");
    auto* pos = &*position;

    scid::database::Progress progress = UI_CreateProgress(ti);
    Timer timer;  // Start timing this search.
    scid::database::Position * posFlip =  NULL;
    scid::database::matSigT msig = scid::database::matsig_Make (pos->GetMaterial());
    scid::database::matSigT msigFlip = 0;
    scid::database::uint hpSig = pos->GetHPSig();
    scid::database::uint hpSigFlip = 0;

    if (flip) {
        posFlip = new scid::database::Position;
        posFlip->Clear();
        for (auto sq = scid::database::A1; sq <= scid::database::H8; ++sq) {
            const auto piece = pos->GetPiece(sq);
            if (piece != scid::database::EMPTY) {
                const auto sq_flip = scid::database::square_Relative(scid::database::BLACK, sq);
                posFlip->AddPiece(scid::database::PIECE_FLIP[piece], sq_flip);
            }
        }
        posFlip->SetToMove(scid::database::color_Flip(pos->GetToMove()));
        posFlip->SetEPTarget(pos->GetEPTarget());
        //NOTE: the search ignores the castling flags
        hpSigFlip = posFlip->GetHPSig();
        msigFlip = scid::database::matsig_Make (posFlip->GetMaterial());
    }

    // If filter operation is to reset the filter, reset it:
    if (filterOp == scid::database::FILTEROP_RESET) {
        filter->includeAll();
        filterOp = scid::database::FILTEROP_AND;
    }
    size_t startFilterCount = filter->size();

    // Here is the loop that searches on each game:
    scid::database::Game tmpGame;
    scid::database::Game* g = &tmpGame;
    scid::database::gamenumT gameNum = 0;
    for (scid::database::gamenumT n = dbase->numGames(); gameNum < n; gameNum++) {
        if ((gameNum % 5000) == 0) {  // Update the percentage done bar:
            if (!progress.report(gameNum, n)) break;
        }
        // First, apply the filter operation:
        if (filterOp == scid::database::FILTEROP_AND) {  // Skip any games not in the filter:
            if (filter.get(gameNum) == 0) {
                continue;
            }
        } else /* filterOp==FILTEROP_OR*/ { // Skip any games in the filter:
            if (filter.get(gameNum) != 0) {
                continue;
            } else {
                // scid::database::OK, this game is NOT in the filter.
                // Add it so filterCounts are kept up to date:
                filter.set (gameNum, 1);
            }
        }

        const scid::database::IndexEntry* ie = dbase->getIndexEntry(gameNum);
        if (ie->GetLength() == 0) {
            // Skip games with no gamefile record:
            filter.set (gameNum, 0);
            continue;
        }

        // Set "useVars" to true only if the search specified searching
        // in variations, AND this game has variations:
        bool useVars = searchInVars && ie->GetVariationsFlag();

        bool possibleMatch = true;
        bool possibleFlippedMatch = flip;

        // Apply speedups if we are not searching in variations:
        if (! useVars) {
            if (! ie->GetStartFlag()) {
                // Speedups that only apply to standard start games:
                if (useHpSigSpeedup  &&  hpSig != 0xFFFF) {
                    const scid::database::byte * hpData = ie->GetHomePawnData();
                    if (! scid::database::hpSig_PossibleMatch (hpSig, hpData)) {
                        possibleMatch = false;
                    }
                    if (possibleFlippedMatch) {
                        if (! scid::database::hpSig_PossibleMatch (hpSigFlip, hpData)) {
                            possibleFlippedMatch = false;
                        }
                    }
                }
            }

            // If this game has no promotions, check the material of its final
            // position, since the searched position might be unreachable:
            if (possibleMatch) {
                if (!scid::database::matsig_isReachable (msig, ie->GetFinalMatSig(),
                                         ie->GetPromotionsFlag(),
                                         ie->GetUnderPromoFlag())) {
                        possibleMatch = false;
                    }
            }
            if (possibleFlippedMatch) {
                if (!scid::database::matsig_isReachable (msigFlip, ie->GetFinalMatSig(),
                                         ie->GetPromotionsFlag(),
                                         ie->GetUnderPromoFlag())) {
                        possibleFlippedMatch = false;
                    }
            }
        }

        if (!possibleMatch  &&  !possibleFlippedMatch) {
            filter.set (gameNum, 0);
            continue;
        }

        // At this point, the game needs to be loaded:
        auto bbuf = dbase->getGame(*ie);
        if (!bbuf) {
            return errorResult (ti, "Error reading game file.");
        }
        scid::database::uint ply = 0;
        if (useVars) {
            scid::database::game_storage::decodeMovesOnly(*g, bbuf);
            // Try matching the game without variations first:
            if (ply == 0  &&  possibleMatch) {
                if (scid::database::game_search::exactMatch(
                        *g, pos, nullptr, searchType)) {
                    ply = currentPly(*g) + 1;
                }
            }
            if (ply == 0  &&  possibleFlippedMatch) {
                if (scid::database::game_search::exactMatch(
                        *g, posFlip, nullptr, searchType)) {
                    ply = currentPly(*g) + 1;
                }
            }
            if (ply == 0  &&  possibleMatch) {
                g->restoreLocation(scid::core::MovetextLocation{});
                if (scid::database::game_search::varExactMatch(
                        *g, pos, searchType)) {
                    ply = currentPly(*g) + 1;
                }
            }
            if (ply == 0  &&  possibleFlippedMatch) {
                g->restoreLocation(scid::core::MovetextLocation{});
                if (scid::database::game_search::varExactMatch(
                        *g, posFlip, searchType)) {
                    ply = currentPly(*g) + 1;
                }
            }
        } else {
            // No searching in variations:
            if (possibleMatch) {
                auto bbuf_clone = bbuf;
                if (scid::database::game_search::exactMatch(
                        *g, pos, &bbuf_clone, searchType)) {
                    // Set its auto-load move number to the matching move:
                    ply = currentPly(*g) + 1;
                }
            }
            if (ply == 0  &&  possibleFlippedMatch) {
                if (scid::database::game_search::exactMatch(
                        *g, posFlip, &bbuf, searchType)) {
                    ply = currentPly(*g) + 1;
                }
            }
        }
        if (ply > 255) { ply = 255; }
        filter.set (gameNum, ply);
    }

    progress.report(1,1);
    if (flip) { delete posFlip; }

    // Now print statistics and time for the search:
    char temp[200];
    int centisecs = timer.CentiSecs();
    if (gameNum != dbase->numGames()) {
        AppendResult (ti, errMsgSearchInterrupted(ti), "  ", NULL);
    }
    std::snprintf(temp, sizeof(temp), "%lu / %lu  (%d%c%02d s)",
             static_cast<unsigned long>(filter->size()),
             static_cast<unsigned long>(startFilterCount),
             centisecs / 100, decimalPointChar, centisecs % 100);
    AppendResult (ti, temp, NULL);
    return TCL_OK;
}

void
flipPattern (scid::database::patternT * patt)
{
    if (patt->rankMatch != scid::database::NO_RANK) {
        patt->rankMatch = (scid::database::RANK_1 + scid::database::RANK_8) - patt->rankMatch;
    }
    patt->pieceMatch = scid::database::PIECE_FLIP[patt->pieceMatch];
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// parsePattern:
//    Called by sc_search_material to extract the details of
//    a pattern parameter (e.g "no wp c ?" for no White pawn on
//    the d file).
scid::database::errorT
parsePattern (const char * str, scid::database::patternT * patt)
{
    ASSERT (str != NULL  &&  patt != NULL);

    // Set up pointers to the four whitespace-separated pattern
    // parameter values in the string:
    str = scid::database::strFirstWord (str);
    const char * flagStr = str;
    str = scid::database::strNextWord (str);
    const char * colorPieceStr = str;
    str = scid::database::strNextWord (str);
    const char * fyleStr = str;
    str = scid::database::strNextWord (str);
    const char * rankStr = str;

    // Parse the color parameter: "w", "b", or "?" for no pattern.
    if (*colorPieceStr == '?') {
        // Empty pattern:
        patt->pieceMatch = scid::database::EMPTY;
        return scid::database::OK;
    }

    scid::database::colorT color = scid::database::WHITE;
    switch (tolower(*colorPieceStr)) {
        case 'w': color = scid::database::WHITE; break;
        case 'b': color = scid::database::BLACK; break;
        default: return scid::database::ERROR;
    }

    // Parse the piece type parameter for this pattern:
    scid::database::pieceT p = scid::database::EMPTY;
    switch (tolower(colorPieceStr[1])) {
        case 'k': p = scid::database::KING; break;
        case 'q': p = scid::database::QUEEN; break;
        case 'r': p = scid::database::ROOK; break;
        case 'b': p = scid::database::BISHOP; break;
        case 'n': p = scid::database::KNIGHT; break;
        case 'p': p = scid::database::PAWN; break;
        default: return scid::database::ERROR;
    }

    patt->pieceMatch = scid::database::piece_Make (color, p);
    patt->flag = scid::database::strGetBoolean (flagStr);

    // Parse the fyle parameter for this pattern:
    char ch = *fyleStr;
    if (ch == '?') {
        patt->fyleMatch = scid::database::NO_FYLE;
    } else if (ch >= 'a'  &&  ch <= 'h') {
        patt->fyleMatch = scid::database::A_FYLE + (ch - 'a');
    } else {
        return scid::database::ERROR;
    }

    // Parse the rank parameter for this pattern:
    ch = *rankStr;
    if (ch == '?') {
        patt->rankMatch = scid::database::NO_RANK;
    } else if (ch >= '1'  &&  ch <= '8') {
        patt->rankMatch = scid::database::RANK_1 + (ch - '1');
    } else {
        return scid::database::ERROR;
    }
    return scid::database::OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_search_material:
//    Searches by material and/or pattern.
int
sc_search_material (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (! db->isOpen()) {
        return errorResult (ti, "Not an open database.");
    }

    scid::database::uint minMoves = 0;
    scid::database::uint minPly = 0;
    scid::database::uint maxPly = 999;
    scid::database::uint matchLength = 1;
    scid::database::byte min[scid::database::MAX_PIECE_TYPES] = {0};
    scid::database::byte minFlipped[scid::database::MAX_PIECE_TYPES] = {0};
    scid::database::byte max[scid::database::MAX_PIECE_TYPES] = {0};
    scid::database::byte maxFlipped[scid::database::MAX_PIECE_TYPES] = {0};
    max[scid::database::WM] = max[scid::database::BM] = 9;
    int matDiff[2];
    matDiff[0] = -40;
    matDiff[1] = 40;
    scid::database::filterOpT filterOp = scid::database::FILTEROP_RESET;
    bool flip = false;
    bool oppBishops = true;
    bool sameBishops = true;
    scid::database::uint hpExcludeMask = scid::database::HPSIG_Empty;
    scid::database::uint hpExMaskFlip = scid::database::HPSIG_Empty;
    std::vector<scid::database::patternT> patt;
    std::vector<scid::database::patternT> flippedPatt;
    scid::database::patternT tempPatt;

    const char * options[] = {
        "wq", "bq", "wr", "br", "wb", "bb", "wn", "bn",
        "wp", "bp", "wm", "bm", "flip", "filter", "range",
        "length", "bishops", "diff", "pattern", NULL
    };
    enum {
        OPT_WQ, OPT_BQ, OPT_WR, OPT_BR, OPT_WB, OPT_BB, OPT_WN, OPT_BN,
        OPT_WP, OPT_BP, OPT_WM, OPT_BM, OPT_FLIP, OPT_FILTER, OPT_RANGE,
        OPT_LENGTH, OPT_BISHOPS, OPT_DIFF, OPT_PATTERN
    };

    int arg = 2;
    while (arg+1 < argc) {
        const char * option = argv[arg];
        const char * value = argv[arg+1];
        arg += 2;
        int index = -1;
        if (option[0] == '-') {
            index = scid::database::strUniqueMatch (&(option[1]), options);
        }
        scid::database::uint counts [2] = {0, 0};
        if (index >= OPT_WQ  &&  index <= OPT_BM) {
            scid::database::strGetUnsigneds (value, counts, 2);
        }

        switch (index) {

        case OPT_WQ:  min[scid::database::WQ] = counts[0];  max[scid::database::WQ] = counts[1];  break;
        case OPT_BQ:  min[scid::database::BQ] = counts[0];  max[scid::database::BQ] = counts[1];  break;
        case OPT_WR:  min[scid::database::WR] = counts[0];  max[scid::database::WR] = counts[1];  break;
        case OPT_BR:  min[scid::database::BR] = counts[0];  max[scid::database::BR] = counts[1];  break;
        case OPT_WB:  min[scid::database::WB] = counts[0];  max[scid::database::WB] = counts[1];  break;
        case OPT_BB:  min[scid::database::BB] = counts[0];  max[scid::database::BB] = counts[1];  break;
        case OPT_WN:  min[scid::database::WN] = counts[0];  max[scid::database::WN] = counts[1];  break;
        case OPT_BN:  min[scid::database::BN] = counts[0];  max[scid::database::BN] = counts[1];  break;
        case OPT_WP:  min[scid::database::WP] = counts[0];  max[scid::database::WP] = counts[1];  break;
        case OPT_BP:  min[scid::database::BP] = counts[0];  max[scid::database::BP] = counts[1];  break;
        case OPT_WM:  min[scid::database::WM] = counts[0];  max[scid::database::WM] = counts[1];  break;
        case OPT_BM:  min[scid::database::BM] = counts[0];  max[scid::database::BM] = counts[1];  break;

        case OPT_FLIP:
            flip = scid::database::strGetBoolean (value);
            break;

        case OPT_FILTER:
            filterOp = scid::database::strGetFilterOp (value);
            break;

        case OPT_RANGE:
            scid::database::strGetUnsigneds (value, counts, 2);
            minPly = counts[0];  maxPly = counts[1];
            break;

        case OPT_LENGTH:
            matchLength = scid::database::strGetUnsigned (value);
            if (matchLength < 1) { matchLength = 1; }
            break;

        case OPT_BISHOPS:
            switch (toupper(value[0])) {
                case 'S': oppBishops = false; sameBishops = true;  break;
                case 'O': oppBishops = true;  sameBishops = false; break;
                default:  oppBishops = true;  sameBishops = true;  break;
            }
            break;

        case OPT_DIFF:
            scid::database::strGetIntegers (value, matDiff, 2);
            break;

        case OPT_PATTERN:
            if (parsePattern (value, &tempPatt) != scid::database::OK) {
                AppendResult (ti, "Invalid pattern: ", value, NULL);
                return TCL_ERROR;
            }
            // Only add to lists if a pattern was specified:
            if (tempPatt.pieceMatch == scid::database::EMPTY) { break; }
            // Update home-pawn exclude masks if appropriate:
            if (!tempPatt.flag
                &&  scid::database::piece_Type(tempPatt.pieceMatch) == scid::database::PAWN
                &&  tempPatt.rankMatch == scid::database::NO_RANK
                &&  tempPatt.fyleMatch != scid::database::NO_FYLE) {
                scid::database::colorT color = scid::database::piece_Color (tempPatt.pieceMatch);
                scid::database::colorT flipColor = (color == scid::database::WHITE ? scid::database::BLACK : scid::database::WHITE);
                scid::database::fyleT fyle = tempPatt.fyleMatch;
                hpExcludeMask = scid::database::hpSig_AddPawn (hpExcludeMask, color, fyle);
                hpExMaskFlip = scid::database::hpSig_AddPawn (hpExMaskFlip, flipColor, fyle);
            }
            // Add the pattern and its flipped equivalent:
            // TODO: why not push_back() ?
            patt.insert(patt.begin(), tempPatt);
            flipPattern (&tempPatt);
            flippedPatt.insert(flippedPatt.begin(), tempPatt);
            break;

        default:
            return InvalidCommand (ti, "sc_search material", options);
        }
    }
    if (arg != argc) { return errorResult (ti, "Odd number of parameters."); }

    // Sanity check of values:
    if (max[scid::database::WQ] < min[scid::database::WQ]) { max[scid::database::WQ] = min[scid::database::WQ]; }
    if (max[scid::database::BQ] < min[scid::database::BQ]) { max[scid::database::BQ] = min[scid::database::BQ]; }
    if (max[scid::database::WR] < min[scid::database::WR]) { max[scid::database::WR] = min[scid::database::WR]; }
    if (max[scid::database::BR] < min[scid::database::BR]) { max[scid::database::BR] = min[scid::database::BR]; }
    if (max[scid::database::WB] < min[scid::database::WB]) { max[scid::database::WB] = min[scid::database::WB]; }
    if (max[scid::database::BB] < min[scid::database::BB]) { max[scid::database::BB] = min[scid::database::BB]; }
    if (max[scid::database::WN] < min[scid::database::WN]) { max[scid::database::WN] = min[scid::database::WN]; }
    if (max[scid::database::BN] < min[scid::database::BN]) { max[scid::database::BN] = min[scid::database::BN]; }
    if (max[scid::database::WP] < min[scid::database::WP]) { max[scid::database::WP] = min[scid::database::WP]; }
    if (max[scid::database::BP] < min[scid::database::BP]) { max[scid::database::BP] = min[scid::database::BP]; }
    // Minor piece range should be at least the sum of the Bishop
    // and Knight minimums, and at most the sum of the maximums:
    if (min[scid::database::WM] < min[scid::database::WB]+min[scid::database::WN]) { min[scid::database::WM] = min[scid::database::WB] + min[scid::database::WN]; }
    if (min[scid::database::BM] < max[scid::database::BB]+min[scid::database::BN]) { min[scid::database::BM] = min[scid::database::BB] + min[scid::database::BN]; }
    if (max[scid::database::WM] > max[scid::database::WB]+max[scid::database::WN]) { max[scid::database::WM] = max[scid::database::WB] + max[scid::database::WN]; }
    if (max[scid::database::BM] > max[scid::database::BB]+max[scid::database::BN]) { max[scid::database::BM] = max[scid::database::BB] + max[scid::database::BN]; }

    // Swap material difference range values if necessary:
    if (matDiff[0] > matDiff[1]) {
        int temp = matDiff[0]; matDiff[0] = matDiff[1]; matDiff[1] = temp;
    }

    // Set up flipped piece counts if necessary:
    if (flip) {
        minFlipped[scid::database::WQ] = min[scid::database::BQ];  maxFlipped[scid::database::WQ] = max[scid::database::BQ];
        minFlipped[scid::database::WR] = min[scid::database::BR];  maxFlipped[scid::database::WR] = max[scid::database::BR];
        minFlipped[scid::database::WB] = min[scid::database::BB];  maxFlipped[scid::database::WB] = max[scid::database::BB];
        minFlipped[scid::database::WN] = min[scid::database::BN];  maxFlipped[scid::database::WN] = max[scid::database::BN];
        minFlipped[scid::database::WP] = min[scid::database::BP];  maxFlipped[scid::database::WP] = max[scid::database::BP];
        minFlipped[scid::database::WM] = min[scid::database::BM];  maxFlipped[scid::database::WM] = max[scid::database::BM];
        minFlipped[scid::database::BQ] = min[scid::database::WQ];  maxFlipped[scid::database::BQ] = max[scid::database::WQ];
        minFlipped[scid::database::BR] = min[scid::database::WR];  maxFlipped[scid::database::BR] = max[scid::database::WR];
        minFlipped[scid::database::BB] = min[scid::database::WB];  maxFlipped[scid::database::BB] = max[scid::database::WB];
        minFlipped[scid::database::BN] = min[scid::database::WN];  maxFlipped[scid::database::BN] = max[scid::database::WN];
        minFlipped[scid::database::BP] = min[scid::database::WP];  maxFlipped[scid::database::BP] = max[scid::database::WP];
        minFlipped[scid::database::BM] = min[scid::database::WM];  maxFlipped[scid::database::BM] = max[scid::database::WM];
    }

    // Convert move numbers to halfmoves (ply counts):
    minMoves = minPly;
    minPly = minPly * 2 - 1;
    maxPly = maxPly * 2;

    // Set up the material Sig: it is the signature of the MAXIMUMs.
    scid::database::matSigT msig, msigFlipped;
    int checkMsig = 1;
    if (max[scid::database::WQ] > 3  ||  max[scid::database::BQ] > 3  ||  max[scid::database::WR] > 3 ||  max[scid::database::BR] > 3 ||
        max[scid::database::WB] > 3  ||  max[scid::database::BB] > 3  ||  max[scid::database::WN] > 3 ||  max[scid::database::BN] > 3) {
        // It is an unusual search, we cannot use material sig!
        checkMsig = 0;
    }
    msig = scid::database::matsig_Make (max);
    msigFlipped = MATSIG_FlipColor(msig);

    scid::database::Progress progress = UI_CreateProgress(ti);
    Timer timer;  // Start timing this search.

    char temp [250];
    scid::database::Game * g = scratchGame;
    scid::database::HFilter filter = db->getFilter("dbfilter");

    // If filter operation is to reset the filter, reset it:
    if (filterOp == scid::database::FILTEROP_RESET) {
        filter->includeAll();
        filterOp = scid::database::FILTEROP_AND;
    }
    size_t startFilterCount = filter->size();

    // Here is the loop that searches on each game:
    scid::database::gamenumT gameNum = 0, n = db->numGames();
    for (; gameNum < n; gameNum++) {
        if ((gameNum % 1000) == 0) {  // Update the percentage done bar:
            if (!progress.report(gameNum, n)) break;
        }
        // First, apply the filter operation:
        if (filterOp == scid::database::FILTEROP_AND) {  // Skip any games not in the filter:
            if (filter.get(gameNum) == 0) {
                continue;
            }
        } else /* filterOp == FILTEROP_OR*/ { // Skip any games in the filter:
            if (filter.get(gameNum) != 0) {
                continue;
            }
            // scid::database::OK, this game is NOT in the filter.
            // Add it so filterCounts are kept up to date:
            filter.set (gameNum, 1);
        }

        const scid::database::IndexEntry* ie = db->getIndexEntry(gameNum);
        if (ie->GetLength() == 0) {  // Skip games with no gamefile record
            filter.set (gameNum, 0);
            continue;
        }

        if (ie->GetNumHalfMoves() < minMoves  &&  ! ie->GetStartFlag()) {
            // Skip games without enough moves to match, if they
            // have the standard starting position:
            filter.set (gameNum, 0);
            continue;
        }

        bool possibleMatch = true;
        bool possibleFlippedMatch = flip;

        // First, eliminate games that cannot match from their final
        // material signature:
        if (checkMsig  &&  !scid::database::matsig_isReachable (msig, ie->GetFinalMatSig(),
                                                ie->GetPromotionsFlag(),
                                                ie->GetUnderPromoFlag()))
        {
            possibleMatch = false;
        }
        if (flip  &&  checkMsig
                &&  !scid::database::matsig_isReachable (msigFlipped, ie->GetFinalMatSig(),
                                         ie->GetPromotionsFlag(),
                                         ie->GetUnderPromoFlag()))
        {
            possibleFlippedMatch = false;
        }

        // If the game has a final home pawn that cannot appear in the
        // patterns, exclude it. For example, a White IQP search has no
        // white c or e pawns, so any game that ends with a c2 or e2 pawn
        // at home need not be loaded:

        if (possibleMatch  &&  hpExcludeMask != scid::database::HPSIG_Empty) {
            scid::database::uint gameFinalHP = scid::database::hpSig_Final (ie->GetHomePawnData());
            // If any bit is set in both, this game cannot match:
            if ((gameFinalHP & hpExcludeMask) != 0) {
                possibleMatch = false;
            }
        }
        if (possibleFlippedMatch  &&  hpExMaskFlip != scid::database::HPSIG_Empty) {
            scid::database::uint gameFinalHP = scid::database::hpSig_Final (ie->GetHomePawnData());
            // If any bit is set in both, this game cannot match:
            if ((gameFinalHP & hpExMaskFlip) != 0) {
                possibleFlippedMatch = false;
            }
        }

        if (!possibleMatch  &&  !possibleFlippedMatch) {
            filter.set (gameNum, 0);
            continue;
        }

        // Now, the game must be loaded and searched:
        auto bbuf = db->getGame(*ie);
        if (!bbuf) {
            continue;
        }

        bool result = false;
        if (possibleMatch) {
            auto bbuf_clone = bbuf;
            bool hasPromo = ie->GetPromotionsFlag() || ie->GetUnderPromoFlag();
            result = scid::database::game_search::materialMatch(
                *g, hasPromo, bbuf_clone, min, max, patt.data(), patt.size(),
                minPly, maxPly, matchLength, oppBishops, sameBishops,
                matDiff[0], matDiff[1]);
        }
        if (result == 0  &&  possibleFlippedMatch) {
            bool hasPromo = ie->GetPromotionsFlag() || ie->GetUnderPromoFlag();
            result = scid::database::game_search::materialMatch(
                *g, hasPromo, bbuf, minFlipped, maxFlipped, flippedPatt.data(),
                flippedPatt.size(), minPly, maxPly, matchLength, oppBishops,
                sameBishops, matDiff[0], matDiff[1]);
        }

        if (result) {
            // update the filter value to the current ply:
            scid::database::uint plyOfMatch = currentPly(*g) + 1 - matchLength;
            scid::database::byte b = (scid::database::byte) (plyOfMatch + 1);
            if (b == 0) { b = 1; }
            filter.set (gameNum, b);
        } else {
            // This game did NOT match:
            filter.set (gameNum, 0);
        }
    }

    progress.report(1,1);

    int centisecs = timer.CentiSecs();

    if (gameNum != n) {
        AppendResult (ti, errMsgSearchInterrupted(ti), "  ", NULL);
    }
    std::snprintf(temp, sizeof(temp), "%lu / %lu  (%d%c%02d s)",
             static_cast<unsigned long>(filter->size()),
             static_cast<unsigned long>(startFilterCount),
             centisecs / 100, decimalPointChar, centisecs % 100);
    AppendResult (ti, temp, NULL);
    return TCL_OK;
}


const scid::database::uint NUM_TITLES = 8;
enum {
    TITLE_GM, TITLE_IM, TITLE_FM,
    TITLE_WGM, TITLE_WIM, TITLE_WFM,
    TITLE_W, TITLE_NONE
};
const char * titleStr [NUM_TITLES] = {
    "gm", "im", "fm", "wgm", "wim", "wfm", "w", "none"
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// parseTitles:
//    Called from sc_search_header to parse a list
//    of player titles to be searched for. The provided
//    string should have some subset of the elements
//    gm, im, fm, wgm, wim, w and none, each separated
//    by whitespace. Example: "gm wgm" would indicate
//    to only search for games by a GM or WIM.
bool *
parseTitles (const char * str)
{
    bool * titles = new bool [NUM_TITLES];

    for (scid::database::uint t=0; t < NUM_TITLES; t++) { titles[t] = false; }

    str = scid::database::strFirstWord (str);
    while (*str != 0) {
        for (scid::database::uint i=0; i < NUM_TITLES; i++) {
            if (scid::database::strIsCasePrefix (titleStr[i], str)) {
                titles[i] = true;
                break;
            }
        }
        str = scid::database::strNextWord (str);
    }
    return titles;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_search_header:
//    Searches by header information.
int
sc_search_header (ClientData, Tcl_Interp * ti, scid::database::scidBaseT* base, scid::database::HFilter& filter, int argc, const char ** argv)
{
    ASSERT(argc >= 2);
    scid::database::Progress progress = UI_CreateProgress(ti);
    scid::database::errorT res = search_index(base, filter, argc -2, argv +2, progress);
    if (res != scid::database::OK) return UI_Result(ti, res);

    //TODO: the old options that follows do not work with FILTEROP_OR
    //      at the moment there is no tcl code that use them with FILTEROP_OR

	bool * wTitles = NULL;
    bool * bTitles = NULL;

    bool wToMove = true;
    bool bToMove = true;

	#if defined(TCL_MAJOR_VERSION) && TCL_MAJOR_VERSION >= 9
	using scid_tcl_size = Tcl_Size;
	#else
	using scid_tcl_size = int;
	#endif

    scid_tcl_size pgnTextCount = 0;
    const char ** sPgnText = NULL;

    const char * options[] = {
        "wtitles", "btitles", "toMove",
        "pgn", NULL
    };
    enum {
        OPT_WTITLES, OPT_BTITLES, OPT_TOMOVE,
        OPT_PGN
    };

    int arg = 2;
    while (arg+1 < argc) {
        const char * option = argv[arg];
        const char * value = argv[arg+1];
        arg += 2;
        int index = -1;
        if (option[0] == '-') {
            index = scid::database::strUniqueMatch (&(option[1]), options);
        }

        switch (index) {
        case OPT_WTITLES:
            delete[] wTitles;
            wTitles = parseTitles (value);
            break;

        case OPT_BTITLES:
            delete[] bTitles;
            bTitles = parseTitles (value);
            break;

        case OPT_TOMOVE:
            wToMove = false;
            if (scid::database::strFirstChar (value, 'w')  || scid::database::strFirstChar (value, 'W')) {
                wToMove = true;
            }
            bToMove = false;
            if (scid::database::strFirstChar (value, 'b')  || scid::database::strFirstChar (value, 'B')) {
                bToMove = true;
            }
            break;

        case OPT_PGN:
            if (Tcl_SplitList (ti, value, &pgnTextCount,
                               &sPgnText) != TCL_OK) {
                delete[] wTitles;
                delete[] bTitles;
                return TCL_ERROR;
            }
            break;

        }
    }

    // Set up White name matches array:
    std::vector<bool> mWhite;
    if (wTitles != NULL  &&  spellChk != NULL) {
        bool allTitlesOn = true;
        for (scid::database::uint t=0; t < NUM_TITLES; t++) {
            if (! wTitles[t]) { allTitlesOn = false; break; }
        }
        if (! allTitlesOn) {
            scid::database::idNumberT i;
            scid::database::idNumberT numNames = base->getNameBase()->GetNumNames(scid::database::NAME_PLAYER);
            mWhite.resize(numNames, true);
            for (i=0; i < numNames; i++) {
                const char * name = base->getNameBase()->GetName (scid::database::NAME_PLAYER, i);
                const scidup::spelling::PlayerInfo* pInfo = spellChk->getPlayerInfo(name);
                const char * title = (pInfo) ? pInfo->getTitle() : "";
                if ((!wTitles[TITLE_GM]  &&  scid::database::strEqual(title, "gm"))
                    || (!wTitles[TITLE_GM]  &&  scid::database::strEqual(title, "hgm"))
                    || (!wTitles[TITLE_IM]  &&  scid::database::strEqual(title, "im"))
                    || (!wTitles[TITLE_FM]  &&  scid::database::strEqual(title, "fm"))
                    || (!wTitles[TITLE_WGM]  &&  scid::database::strEqual(title, "wgm"))
                    || (!wTitles[TITLE_WIM]  &&  scid::database::strEqual(title, "wim"))
                    || (!wTitles[TITLE_WFM]  &&  scid::database::strEqual(title, "wfm"))
                    || (!wTitles[TITLE_W]  &&  scid::database::strEqual(title, "w"))
                    || (!wTitles[TITLE_NONE]  &&  scid::database::strEqual(title, ""))
                    || (!wTitles[TITLE_NONE]  &&  scid::database::strEqual(title, "cgm"))
                    || (!wTitles[TITLE_NONE]  &&  scid::database::strEqual(title, "cim"))) {
                    mWhite[i] = false;
                }
            }
        }
    }

    // Set up Black name matches array:
    std::vector<bool> mBlack;
    if (bTitles != NULL  &&  spellChk != NULL) {
        bool allTitlesOn = true;
        for (scid::database::uint t=0; t < NUM_TITLES; t++) {
            if (!bTitles[t]) { allTitlesOn = false; break; }
        }
        if (! allTitlesOn) {
            scid::database::idNumberT i;
            scid::database::idNumberT numNames = base->getNameBase()->GetNumNames(scid::database::NAME_PLAYER);
            mBlack.resize(numNames, true);
            for (i=0; i < numNames; i++) {
                const char * name = base->getNameBase()->GetName (scid::database::NAME_PLAYER, i);
                const scidup::spelling::PlayerInfo* pInfo = spellChk->getPlayerInfo(name);
                const char * title = (pInfo) ? pInfo->getTitle() : "";
                if ((!bTitles[TITLE_GM]  &&  scid::database::strEqual(title, "gm"))
                    || (!bTitles[TITLE_GM]  &&  scid::database::strEqual(title, "hgm"))
                    || (!bTitles[TITLE_IM]  &&  scid::database::strEqual(title, "im"))
                    || (!bTitles[TITLE_FM]  &&  scid::database::strEqual(title, "fm"))
                    || (!bTitles[TITLE_WGM]  &&  scid::database::strEqual(title, "wgm"))
                    || (!bTitles[TITLE_WIM]  &&  scid::database::strEqual(title, "wim"))
                    || (!bTitles[TITLE_WFM]  &&  scid::database::strEqual(title, "wfm"))
                    || (!bTitles[TITLE_W]  &&  scid::database::strEqual(title, "w"))
                    || (!bTitles[TITLE_NONE]  &&  scid::database::strEqual(title, ""))
                    || (!bTitles[TITLE_NONE]  &&  scid::database::strEqual(title, "cgm"))
                    || (!bTitles[TITLE_NONE]  &&  scid::database::strEqual(title, "cim"))) {
                    mBlack[i] = false;
                }
            }
        }
    }

    bool skipSearch = false;
    if (mWhite.empty() && mBlack.empty() &&
        wToMove == true && bToMove == true &&
        pgnTextCount == 0) {
        skipSearch = true;
    }

    // Here is the loop that searches on each game:
    scid::database::errorT result = scid::database::OK;
    if (!skipSearch)
    for (scid::database::uint i=0, n = base->numGames(); i < n; i++) {
        if ((i % 5000) == 0) {  // Update the percentage done bar:
            if (!progress.report(i,n)) {
                result = scid::database::ERROR_UserCancel;
                break;
            }
        }
        // Skip any games not in the filter:
        if (filter.get(i) == 0) continue;

        const scid::database::IndexEntry* ie = base->getIndexEntry(i);

		auto matchGameHeader = [&]() {
			//TODO: this trick does not work if there is a custom start position
			scid::database::uint halfmoves = ie->GetNumHalfMoves();
			if ((halfmoves % 2) == 0) { // This game ends with White to move
				if (!wToMove) {
					return false;
				}
			} else { // This game ends with Black to move
				if (!bToMove) {
					return false;
				}
			}

			// Last, we check the players
			if (!mWhite.empty() && !mWhite[ie->GetWhite()]) {
				return false;
			}
			if (!mBlack.empty() && !mBlack[ie->GetBlack()]) {
				return false;
			}

			// If we reach here, this game matches all criteria.
			return true;
		};
		bool match = matchGameHeader();

        // Now try to match the comment text if applicable:
        // Note that it is not worth using a faster staring search
        // algorithm like Boyer-Moore or Knuth-Morris-Pratt since
        // profiling showed most that most of the time is spent
        // generating the PGN representation of each game.

		if (match && pgnTextCount > 0) {
			if (base->getGame(*ie, *scratchGame) != scid::database::OK) {
				match = false;
			} else {
				const auto encodeOptions =
				    scid::database::LegacyGameEncodeOptions{
				        PGN_STYLE_TAGS | PGN_STYLE_COMMENTS |
				            PGN_STYLE_VARS | PGN_STYLE_SYMBOLS,
				        scid::database::PGN_FORMAT_Plain,
				        0,
				    };
				const char* buf =
				    scid::database::legacy_pgn::encode(*scratchGame,
				                                       encodeOptions)
				        .first;
					for (scid_tcl_size m = 0; m < pgnTextCount; m++) {
						if (match) {
							match = scid::database::strContains(buf, sPgnText[m]);
						}
					}
			}
		}

        if (match) {
            filter.set (i, 1);
        } else {
            // This game did NOT match:
            filter.set (i, 0);
        }
    }
    if (wTitles != NULL) { delete[] wTitles; }
    if (bTitles != NULL) { delete[] bTitles; }
    Tcl_Free ((char *) sPgnText);

    progress.report(1,1);

    return UI_Result(ti, result);;
}


//////////////////////////////////////////////////////////////////////
//  VARIATION creation/deletion/navigation functions.

int
sc_var (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game& game = editor.game();
    static const char * options [] = {
        "count", "number", "create", "delete", "enter", "exit", "first",
        "level", "list", "moveInto", "promote", NULL
    };
    enum {
        VAR_COUNT, VAR_NUMBER, VAR_CREATE, VAR_DELETE, VAR_ENTER, VAR_EXIT, VAR_FIRST,
        VAR_LEVEL, VAR_LIST, VAR_MOVEINTO, VAR_PROMOTE
    };
    int index = -1;

    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options); }

    switch (index) {
    case VAR_COUNT:
        return setUintResult (ti, variationCount(game));

    case VAR_NUMBER:
        return setUintResult (ti, variationNumber(game));

    case VAR_CREATE:
        if (! (isAtVariationStart(game)  &&  isAtVariationEnd(game))) {
            game.next();
            game.addVariation();
            editor.setDirty();
        }
        break;

    case VAR_DELETE:
        return sc_var_delete (cd, ti, argc, argv);

    case VAR_ENTER:
        return sc_var_enter (cd, ti, argc, argv);

    case VAR_EXIT:
        game.exitVariation();
        break;

    case VAR_FIRST:
        return sc_var_first (cd, ti, argc, argv);

    case VAR_LEVEL:
        return setUintResult (ti, variationLevel(game));

    case VAR_LIST:
        return sc_var_list (cd, ti, argc, argv);

    case VAR_MOVEINTO:
        return sc_var_enter (cd, ti, argc, argv);

    case VAR_PROMOTE:
        editor.setDirty();
        return UI_Result(ti, game.promoteVariationToMainline ());

    default:
        return InvalidCommand (ti, "sc_var", options);
    }

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_var_delete:
//    Deletes a specified variation.
int sc_var_delete(ClientData, Tcl_Interp* ti, int, const char**) {
	auto editor = scidup::app::editor::gameSession(*db);
	auto err = editor.game().deleteVariation();
	if (err != scid::database::ERROR_NoVariation)
		editor.setDirty();
	return UI_Result(ti, err);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_var_first:
//    Promotes the specified variation of the current to be the
//    first in the list.
int sc_var_first(ClientData, Tcl_Interp* ti, int, const char**) {
	auto editor = scidup::app::editor::gameSession(*db);
	auto err = editor.game().promoteVariationToFirst();
	if (err != scid::database::ERROR_NoVariation)
		editor.setDirty();
	return UI_Result(ti, err);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_var_list:
//    Returns a Tcl list of the variations for the current move.
int
sc_var_list (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game& game = editor.game();
    bool uci = (argc > 2) && ! scid::database::strCompare("UCI", argv[2]);
    scid::database::uint varCount = variationCount(game);
    char s[100];
    for (scid::database::uint varNumber = 0; varNumber < varCount; varNumber++) {
        game.enterVariation (varNumber);
        if (uci) {
            scid::database::strCopy(
                s, scid::core::notation::nextMoveUci(game.coreGame(),
                                                     game.coreLocation())
                       .c_str());
        } else {
            scid::database::strCopy(
                s, scid::core::notation::nextSan(game.coreGame(),
                                                 game.coreLocation())
                       .c_str());
        }
        // if (s[0] == 0) { scid::database::strCopy (s, "(empty)"); }
        AppendElement (ti, s);
        game.exitVariation ();
    }
    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_var_enter:
//    Moves into a specified variation.
int
sc_var_enter (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    auto editor = scidup::app::editor::gameSession(*db);
    scid::database::Game& game = editor.game();
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_var enter <number>");
    }

    scid::database::uint varNumber = scid::database::strGetUnsigned (argv[2]);
    if (varNumber >= variationCount(game)) {
        return errorResult (ti, "No such variation!");
    }

    game.enterVariation (varNumber);
    // Should moving into a variation also automatically play
    // the first variation move? Maybe it should depend on
    // whether there is a comment before the first move.
    // Uncomment the following line to auto-play the first move:
    game.next();

    return TCL_OK;
}

//////////////////////////////////////////////////////////////////////
///  BOOK functions

int
sc_book (ClientData cd, Tcl_Interp * ti, int argc, const char ** argv)
{
    static const char * options [] = {
        "load", "close", "moves", "positions", "movesupdate", "update", NULL
    };
    enum {
        BOOK_LOAD,    BOOK_CLOSE, BOOK_MOVE, BOOK_POSITIONS, BOOK_MOVES_UPDATE, BOOK_UPDATE,
    };
    int index = -1;

    if (argc > 1) { index = scid::database::strUniqueMatch (argv[1], options);}

    switch (index) {
    case BOOK_LOAD:
        return sc_book_load (cd, ti, argc, argv);

    case BOOK_CLOSE:
        return sc_book_close (cd, ti, argc, argv);

    case BOOK_MOVE:
        return sc_book_moves (cd, ti, argc, argv);

    case BOOK_POSITIONS:
        return sc_book_positions (cd, ti, argc, argv);

    case BOOK_UPDATE:
        return sc_book_update (cd, ti, argc, argv);

    case BOOK_MOVES_UPDATE:
        return sc_book_movesupdate (cd, ti, argc, argv);

    default:
        return InvalidCommand (ti, "sc_book", options);
    }

    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_book_load:
//    Opens and loads a .bin book (fruit format)
int
sc_book_load (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 4) {
        return errorResult (ti, "Usage: sc_book load bookfile slot");
    }

    scid::database::uint slot = scid::database::strGetUnsigned (argv[3]);

	 int bookstate = polyglot_open(argv[2], slot);

    if (bookstate == -1 ) {
			return errorResult (ti, "Unable to load book");
		}
    if (bookstate  >  0 ) {
		   // state == 1: book is read only
			return setIntResult (ti, bookstate);
	 }
    return TCL_OK;

//--//    if (polyglot_open(argv[2], slot) == -1 ) {
//--//			return errorResult (ti, "Unable to load book");
//--//		}
//--//    return TCL_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_book_close:
//    Closes the previously loaded book
int
sc_book_close (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_book close slot");
    }
    scid::database::uint slot = scid::database::strGetUnsigned (argv[2]);
    if (polyglot_close(slot) == -1 ) {
			return errorResult (ti, "Error closing book");
		}
    return TCL_OK;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_book_moves:
//    returns a list of all moves contained in opened book and their probability in a TCL list
int
sc_book_moves (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_book moves slot");
    }
    scid::database::uint slot = scid::database::strGetUnsigned (argv[2]);
    char boardStr[100];
    auto editor = scidup::app::editor::gameSession(*db);
    auto position = currentPosition(editor.game());
    if (!position)
        return UI_Result(ti, scid::database::ERROR, "Error reading position.");
    position->PrintFEN(boardStr, sizeof(boardStr));

    char moves[1024] = {};
    auto extra_info = polyglot_moves(moves, boardStr, slot);
    UI_List extra_list(extra_info.size());
    for (auto [score, depth, engine_name_idx] : extra_info) {
        UI_List entry(3);
        entry.push_back(score);
        entry.push_back(depth);
        entry.push_back(engine_name_idx);
        extra_list.push_back(entry);
    }
    UI_List res(2);
    res.push_back(moves);
    res.push_back(extra_list);
    return UI_Result(ti, scid::database::OK, res);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_positions:
//    returns a TCL list of moves to a position in the book
int
sc_book_positions (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
			char moves[200] = "";
			char boardStr[100];
    if (argc != 3) {
        return errorResult (ti, "Usage: sc_book positions slot");
    }
	    scid::database::uint slot = scid::database::strGetUnsigned (argv[2]);
			auto editor = scidup::app::editor::gameSession(*db);
			auto position = currentPosition(editor.game());
			if (!position)
				return UI_Result(ti, scid::database::ERROR, "Error reading position.");
			position->PrintFEN(boardStr, sizeof(boardStr));
			polyglot_positions(moves, (const char *) boardStr, slot);
    AppendResult (ti, moves, NULL);
    return TCL_OK;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_book_update:
//    updates the opened book with probability values
int
sc_book_update (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 4) {
        return errorResult (ti, "Usage: sc_book update <probs> slot");
    }
    scid::database::uint slot = scid::database::strGetUnsigned (argv[3]);
		scid_book_update( (char*) argv[2], slot );
    return TCL_OK;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// sc_book_movesupdate:
//    updates the opened book with moves and probability values
int
sc_book_movesupdate (ClientData, Tcl_Interp * ti, int argc, const char ** argv)
{
    if (argc != 6) {
        return errorResult (ti, "Usage: sc_book movesupdate <moves> <probs> slot tempfile");
    }
    scid::database::uint slot = scid::database::strGetUnsigned (argv[4]);
    scid_book_movesupdate( (char*) argv[2], (char*) argv[3], slot, (char*) argv[5] );
    return TCL_OK;
}
//////////////////////////////////////////////////////////////////////
/// END of scidup_tcl_bindings.cpp
//////////////////////////////////////////////////////////////////////

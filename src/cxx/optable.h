//////////////////////////////////////////////////////////////////////
//
//  FILE:       optable.h
//              OpTable class (for opening reports and theory tables)
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    3.5
//
//  Notice:     Copyright (c) 2001-2003  Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//
//////////////////////////////////////////////////////////////////////

#ifndef SCID_OPTABLE_H
#define SCID_OPTABLE_H

#include "scidup/core/dstring.h"
#include "scidup/core/game_result.h"
#include "scidup/core/movetext_location.h"
#include "scidup/core/notation.h"
#include "scidup/database/common.h"
#include "scidup/database/game_id.h"
#include "scidup/database/game_info.h"
#include "scidup/database/misc.h"
#include "scidup/eco/code.h"
#include <string>

namespace scidup::eco {
class Book;
}

const scid::core::uint OPTABLE_COLUMNS = 8;
const scid::core::uint OPTABLE_MIN_ROWS = 5;
const scid::core::uint OPTABLE_MAX_ROWS = 20;
const scid::core::uint OPTABLE_DEFAULT_ROWS = 10;
const scid::core::uint OPTABLE_MAX_EXTRA_MOVES = 10;
const scid::core::uint OPLINE_MOVES = (OPTABLE_COLUMNS + OPTABLE_MAX_EXTRA_MOVES) * 2;
const scid::core::uint OPTABLE_MAX_LINES = 2000;
const scid::core::uint OPTABLE_MAX_TABLE_LINES = 5000;//500;
const scid::core::uint OPTABLE_MAX_STARTLINE = 100;

const scid::core::uint OPTABLE_Text  = 0;
const scid::core::uint OPTABLE_HTML  = 1;
const scid::core::uint OPTABLE_LaTeX = 2;
const scid::core::uint OPTABLE_CText = 3;    // Color hypertext.
const scid::core::uint OPTABLE_Compact = 4;  // For more compact moves in table.

// Positional themes
const scid::core::uint NUM_POSTHEMES = 10;
const scid::core::uint POSTHEME_CastSame  = 0;
const scid::core::uint POSTHEME_CastOpp   = 1;
const scid::core::uint POSTHEME_QueenSwap = 2;
const scid::core::uint POSTHEME_OneBPair  = 3;
const scid::core::uint POSTHEME_Kstorm    = 4;
const scid::core::uint POSTHEME_WIQP      = 5;
const scid::core::uint POSTHEME_BIQP      = 6;
const scid::core::uint POSTHEME_WAdvPawn  = 7;
const scid::core::uint POSTHEME_BAdvPawn  = 8;
const scid::core::uint POSTHEME_OpenFyle  = 9;
const scid::core::uint POSTHEME_THRESHOLD = 4;  // Theme must occur this many times.

const scid::core::uint NUM_EGTHEMES = 8;
const scid::core::uint EGTHEME_P = 0;
const scid::core::uint EGTHEME_M = 1;
const scid::core::uint EGTHEME_R = 2;
const scid::core::uint EGTHEME_RM = 3;
const scid::core::uint EGTHEME_Q = 4;
const scid::core::uint EGTHEME_QM = 5;
const scid::core::uint EGTHEME_QR = 6;
const scid::core::uint EGTHEME_QRM = 7;

const scid::core::uint OPTABLE_Line = 0;
const scid::core::uint OPTABLE_All = 1;


struct moveOrderT {
    scid::core::uint id;       // Move Order id number
    scid::core::uint count;    // Number of times this order has occurred
    char * moves;  // String containing the moves in SAN notation
};

class OpLine
{
  friend class OpTable;
  private:
    char *      White;
    char *      Black;
    char *      Site;
    scid::database::gamenumT    GameNumber;
    scid::database::idNumberT   WhiteID;
    scid::database::idNumberT   BlackID;
    scid::core::ratingT        WhiteElo;   // Actual White rating (no estimate)
    scid::core::ratingT        BlackElo;   // Actual Black rating
    scid::core::ratingT        AvgElo;     // Average Elo (using actual or estimates)
    scid::core::dateT       Date;
    scid::core::resultT     Result;
    scid::core::uint        Length;
    scid::core::uint        NumMoves;
    bool        ShortGame;     // True if all game ends early enough that
                               // this line contains all its moves.
    scidup::eco::Code           EcoCode;
    scid::core::uint        MoveOrderID;
    scid::core::sanStringT  Move [OPLINE_MOVES];
    scid::core::uint        NoteMoveNum;  // If a note, at what move does it start?
    scid::core::uint        NoteNumber;   // If a note, this stores its footnote number.
    OpLine *    Next;         // Linked list used for sorting and footnotes.
    bool        Selected;     // For selecting lines by some criteria.
    scid::core::uint        StartPly;

    scid::core::uint        Theme [NUM_POSTHEMES];
    scid::core::uint        EgTheme;

    void Init (void);
    void Init (scid::core::Game * g, scid::core::MovetextLocation location,
               const scid::database::GameInfo& info, scid::database::gamenumT gameNum,
               scid::core::uint maxExtraMoves, scid::core::uint maxThemeMoveNumber);
    void Destroy (void);

  public:
    OpLine () { Init(); }
    OpLine (scid::core::Game * g, scid::core::MovetextLocation location,
            const scid::database::GameInfo& info, scid::database::gamenumT gnum,
            scid::core::uint max, scid::core::uint tm) {
        Init (g, location, info, gnum, max, tm);
    }
    ~OpLine() { Destroy(); }
    void SetPositionalThemes (scid::core::Position * pos);
    void Insert (OpLine * subline);
    void SetMoveOrderID (scid::core::uint id) { MoveOrderID = id; }
    scid::core::uint CommonLength (OpLine * line);
    static void PrintMove (scid::core::DString * dstr, const char * move, scid::core::uint format);
    void PrintNote (scid::core::DString * dstr, scid::core::uint movenum, scid::core::uint start, scid::core::uint format);
    void PrintSummary (scid::core::DString * dstr, scid::core::uint format, bool fullDate, bool nmoves);

    const char * GetMove (scid::core::uint depth) { return Move[depth]; }
};


class OpTable
{
  private:
    scid::core::uint        NumRows;
    scid::core::uint        TargetRows;
    scid::core::uint        NumLines;
    scid::core::uint        FilterCount;
    scid::core::uint        NumTableLines;
    scid::core::uint        MaxTableLines;
    scid::core::uint        MaxNoteLength;
    scid::core::uint        MaxThemeMoveNumber;
    scid::core::uint        NumNotes;
    scid::core::uint        Format;
    char *      Type;   // "opening" or "player" report
    bool        WTM;    // whether White is to move in the start position.
    scid::core::sanStringT  StartLine [OPTABLE_MAX_STARTLINE];
    scid::core::uint        StartLength;
    OpLine *    Line [OPTABLE_MAX_LINES];
    scid::core::uint        Results [scid::core::NUM_RESULT_TYPES];
    scid::core::uint        TheoryResults [scid::core::NUM_RESULT_TYPES];
    scid::core::uint        TheoryCount;
    std::string ECOstr_;
    scid::core::sanStringT  ExcludeMove;
    char        DecimalChar;

    // Statistics on material of final positions:
    scid::core::uint        EndgameCount [2][NUM_EGTHEMES];

    // Statistics on move orders to reach the start line:
    scid::core::uint        NumMoveOrders;
    moveOrderT  MoveOrder [OPTABLE_MAX_LINES];

    // Statistics on themes:
    scid::core::uint        ThemeCount [NUM_POSTHEMES];

    // Arrays for making rows out of the lines:
    OpLine *    Row [OPTABLE_MAX_TABLE_LINES];
    scid::core::uint        NLines [OPTABLE_MAX_TABLE_LINES];
    scid::core::uint        RowScore [OPTABLE_MAX_TABLE_LINES];

    void SelectTableLines (void);
    void SortTableLines (OpLine ** lines, scid::core::uint nlines, scid::core::uint depth);
    bool IsRowMergable (scid::core::uint rownum);
    void MergeRow (scid::core::uint rownum);
    bool HasNotes (OpLine * line, scid::core::uint movenum);
    scid::core::uint NoteCount (scid::core::uint note);
    scid::core::uint NoteScore (scid::core::uint note);
    void PrintNotes (scid::core::DString * dstr, scid::core::uint format);

  public:
    OpTable (const char * type, scid::core::Game * g,
             scid::core::MovetextLocation location, scidup::eco::Book * ecoBook) {
        Init (type, g, location, ecoBook);
    }
    OpTable (const char * type, scid::core::Game * g,
             scid::core::MovetextLocation location) {
        Init (type, g, location, NULL);
    }
    ~OpTable() { Clear();  delete[] Type; }
    void Init (const char * type, scid::core::Game * g,
               scid::core::MovetextLocation location, scidup::eco::Book * ecoBook);
    void Clear ();
    void ClearNotes ();
    void SetFormat (const char * str);
    void SetDecimalChar (char c) { DecimalChar = c; }

    scid::core::uint GetTotalCount() { return FilterCount; }
    scid::core::uint GetTheoryCount() { return TheoryCount; }

    void   SetExcludeMove (const char * s) {
        scid::database::strCopy (ExcludeMove, s);
        scid::database::strStrip (ExcludeMove, '-');
        scid::database::strStrip (ExcludeMove, '=');
    }
    const char* GetEco() const { return ECOstr_.c_str(); }
    void   SetNumRows (scid::core::uint nrows) { TargetRows = nrows; }
    void   GuessNumRows (void);
    void   SetMaxTableLines (scid::core::uint nlines) {
        if (nlines <= OPTABLE_MAX_TABLE_LINES) {
            MaxTableLines = nlines;
        }
    }
    scid::core::uint   GetMaxTableLines (void) { return MaxTableLines; }
    void   SetMaxExtraMoves (scid::core::uint nmoves) {
        MaxNoteLength = (OPTABLE_COLUMNS + nmoves) * 2;
    }
    scid::core::uint   GetMaxExtraMoves (void) {
        return (MaxNoteLength / 2) - OPTABLE_COLUMNS;
    }
    scid::core::uint   GetNumLines (void) { return NumLines; }
    void   SetMaxThemeMoveNumber (scid::core::uint x) { MaxThemeMoveNumber = x; }
    bool   Add (OpLine * line);
    scid::core::uint   PercentScore (void);
    scid::core::uint   TheoryPercent (void);
    scid::core::uint   TheoryScore (void);
    scid::core::uint   PercentFreq (scid::core::resultT result);
    scid::core::uint   AvgLength (scid::core::resultT result);
    scid::core::uint   AvgElo (scid::core::colorT color, scid::core::uint *count, scid::core::uint *oppScore, scid::core::uint *oppPerf);
    void   BestGames (scid::core::DString * dstr, scid::core::uint count, const char * rtype);
    void   TopPlayers (scid::core::DString * dstr, scid::core::colorT c, scid::core::uint count);
    void   TopEcoCodes (scid::core::DString * dstr, scid::core::uint count);
    void   PrintStemLine (scid::core::DString * dstr, scid::core::uint format, bool exclude);
    void   PrintStemLine (scid::core::DString * dstr) { PrintStemLine (dstr, Format, false); }
    void   MakeRows (void);
#ifdef WINCE
    void   DumpLines (/*FILE **/Tcl_Channel fp);
#else
    void   DumpLines (FILE * fp);
#endif
    void   PrintTable (scid::core::DString * dstr, const char *title, const char *comment);
    void   PrintLaTeX (scid::core::DString * dstr,const char *title, const char *comment);
    void   PrintHTML (scid::core::DString * str, const char *title, const char *comment);
    void   PrintText (scid::core::DString * str, const char *title, const char *comment,
                      bool htext);
    static scid::core::uint FormatFromStr (const char * str);
    scid::core::uint   AddMoveOrder (scid::core::Game * g,
                                         scid::core::MovetextLocation location);
    void   PopularMoveOrders (scid::core::DString * dstr, scid::core::uint count);
    void   ThemeReport (scid::core::DString * dstr, scid::core::uint argc, const char ** argv);
    void   AddEndMaterial (scid::database::matSigT ms, bool inFilter);
    void   EndMaterialReport (scid::core::DString * dstr, const char * repGames,
                              const char * allGames);
    scid::core::uint * SelectGames (char type, scid::core::uint number);
};

#endif // SCID_OPTABLE_H

//////////////////////////////////////////////////////////////////////
// optable.h
//////////////////////////////////////////////////////////////////////

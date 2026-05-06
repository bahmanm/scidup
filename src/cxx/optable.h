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

#include "scidup/core/game_result.h"
#include "scidup/core/notation.h"
#include "scidup/database/common.h"
#include "scidup/database/game.h"
#include "scidup/database/indexentry.h"
#include "scidup/eco/code.h"
#include <string>

namespace scidup::eco {
class Book;
}

const scid::database::uint OPTABLE_COLUMNS = 8;
const scid::database::uint OPTABLE_MIN_ROWS = 5;
const scid::database::uint OPTABLE_MAX_ROWS = 20;
const scid::database::uint OPTABLE_DEFAULT_ROWS = 10;
const scid::database::uint OPTABLE_MAX_EXTRA_MOVES = 10;
const scid::database::uint OPLINE_MOVES = (OPTABLE_COLUMNS + OPTABLE_MAX_EXTRA_MOVES) * 2;
const scid::database::uint OPTABLE_MAX_LINES = 2000;
const scid::database::uint OPTABLE_MAX_TABLE_LINES = 5000;//500;
const scid::database::uint OPTABLE_MAX_STARTLINE = 100;

const scid::database::uint OPTABLE_Text  = 0;
const scid::database::uint OPTABLE_HTML  = 1;
const scid::database::uint OPTABLE_LaTeX = 2;
const scid::database::uint OPTABLE_CText = 3;    // Color hypertext.
const scid::database::uint OPTABLE_Compact = 4;  // For more compact moves in table.

// Positional themes
const scid::database::uint NUM_POSTHEMES = 10;
const scid::database::uint POSTHEME_CastSame  = 0;
const scid::database::uint POSTHEME_CastOpp   = 1;
const scid::database::uint POSTHEME_QueenSwap = 2;
const scid::database::uint POSTHEME_OneBPair  = 3;
const scid::database::uint POSTHEME_Kstorm    = 4;
const scid::database::uint POSTHEME_WIQP      = 5;
const scid::database::uint POSTHEME_BIQP      = 6;
const scid::database::uint POSTHEME_WAdvPawn  = 7;
const scid::database::uint POSTHEME_BAdvPawn  = 8;
const scid::database::uint POSTHEME_OpenFyle  = 9;
const scid::database::uint POSTHEME_THRESHOLD = 4;  // Theme must occur this many times.

const scid::database::uint NUM_EGTHEMES = 8;
const scid::database::uint EGTHEME_P = 0;
const scid::database::uint EGTHEME_M = 1;
const scid::database::uint EGTHEME_R = 2;
const scid::database::uint EGTHEME_RM = 3;
const scid::database::uint EGTHEME_Q = 4;
const scid::database::uint EGTHEME_QM = 5;
const scid::database::uint EGTHEME_QR = 6;
const scid::database::uint EGTHEME_QRM = 7;

const scid::database::uint OPTABLE_Line = 0;
const scid::database::uint OPTABLE_All = 1;


struct moveOrderT {
    scid::database::uint id;       // Move Order id number
    scid::database::uint count;    // Number of times this order has occurred
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
    scid::database::ratingT        WhiteElo;   // Actual White rating (no estimate)
    scid::database::ratingT        BlackElo;   // Actual Black rating
    scid::database::ratingT        AvgElo;     // Average Elo (using actual or estimates)
    scid::database::dateT       Date;
    scid::database::resultT     Result;
    scid::database::uint        Length;
    scid::database::uint        NumMoves;
    bool        ShortGame;     // True if all game ends early enough that
                               // this line contains all its moves.
    scidup::eco::Code           EcoCode;
    scid::database::uint        MoveOrderID;
    scid::database::sanStringT  Move [OPLINE_MOVES];
    scid::database::uint        NoteMoveNum;  // If a note, at what move does it start?
    scid::database::uint        NoteNumber;   // If a note, this stores its footnote number.
    OpLine *    Next;         // Linked list used for sorting and footnotes.
    bool        Selected;     // For selecting lines by some criteria.
    scid::database::uint        StartPly;

    scid::database::uint        Theme [NUM_POSTHEMES];
    scid::database::uint        EgTheme;

    void Init (void);
    void Init (scid::database::Game * g, const scid::database::IndexEntry * ie, scid::database::gamenumT gameNum,
               scid::database::uint maxExtraMoves, scid::database::uint maxThemeMoveNumber);
    void Destroy (void);

  public:
    OpLine () { Init(); }
    OpLine (scid::database::Game * g, const scid::database::IndexEntry * ie, scid::database::gamenumT gnum, scid::database::uint max, scid::database::uint tm) {
        Init (g, ie, gnum, max, tm);
    }
    ~OpLine() { Destroy(); }
    void SetPositionalThemes (scid::database::Position * pos);
    void Insert (OpLine * subline);
    void SetMoveOrderID (scid::database::uint id) { MoveOrderID = id; }
    scid::database::uint CommonLength (OpLine * line);
    static void PrintMove (scid::database::DString * dstr, const char * move, scid::database::uint format);
    void PrintNote (scid::database::DString * dstr, scid::database::uint movenum, scid::database::uint start, scid::database::uint format);
    void PrintSummary (scid::database::DString * dstr, scid::database::uint format, bool fullDate, bool nmoves);

    const char * GetMove (scid::database::uint depth) { return Move[depth]; }
};


class OpTable
{
  private:
    scid::database::uint        NumRows;
    scid::database::uint        TargetRows;
    scid::database::uint        NumLines;
    scid::database::uint        FilterCount;
    scid::database::uint        NumTableLines;
    scid::database::uint        MaxTableLines;
    scid::database::uint        MaxNoteLength;
    scid::database::uint        MaxThemeMoveNumber;
    scid::database::uint        NumNotes;
    scid::database::uint        Format;
    char *      Type;   // "opening" or "player" report
    bool        WTM;    // whether White is to move in the start position.
    scid::database::sanStringT  StartLine [OPTABLE_MAX_STARTLINE];
    scid::database::uint        StartLength;
    OpLine *    Line [OPTABLE_MAX_LINES];
    scid::database::uint        Results [scid::database::NUM_RESULT_TYPES];
    scid::database::uint        TheoryResults [scid::database::NUM_RESULT_TYPES];
    scid::database::uint        TheoryCount;
    std::string ECOstr_;
    scid::database::sanStringT  ExcludeMove;
    char        DecimalChar;

    // Statistics on material of final positions:
    scid::database::uint        EndgameCount [2][NUM_EGTHEMES];

    // Statistics on move orders to reach the start line:
    scid::database::uint        NumMoveOrders;
    moveOrderT  MoveOrder [OPTABLE_MAX_LINES];

    // Statistics on themes:
    scid::database::uint        ThemeCount [NUM_POSTHEMES];

    // Arrays for making rows out of the lines:
    OpLine *    Row [OPTABLE_MAX_TABLE_LINES];
    scid::database::uint        NLines [OPTABLE_MAX_TABLE_LINES];
    scid::database::uint        RowScore [OPTABLE_MAX_TABLE_LINES];

    void SelectTableLines (void);
    void SortTableLines (OpLine ** lines, scid::database::uint nlines, scid::database::uint depth);
    bool IsRowMergable (scid::database::uint rownum);
    void MergeRow (scid::database::uint rownum);
    bool HasNotes (OpLine * line, scid::database::uint movenum);
    scid::database::uint NoteCount (scid::database::uint note);
    scid::database::uint NoteScore (scid::database::uint note);
    void PrintNotes (scid::database::DString * dstr, scid::database::uint format);

  public:
    OpTable (const char * type, scid::database::Game * g, scidup::eco::Book * ecoBook) {
        Init (type, g, ecoBook);
    }
    OpTable (const char * type, scid::database::Game * g) { Init (type, g, NULL); }
    ~OpTable() { Clear();  delete[] Type; }
    void Init (const char * type, scid::database::Game * g, scidup::eco::Book * ecoBook);
    void Clear ();
    void ClearNotes ();
    void SetFormat (const char * str);
    void SetDecimalChar (char c) { DecimalChar = c; }

    scid::database::uint GetTotalCount() { return FilterCount; }
    scid::database::uint GetTheoryCount() { return TheoryCount; }

    void   SetExcludeMove (const char * s) {
        scid::database::strCopy (ExcludeMove, s);
        scid::database::strStrip (ExcludeMove, '-');
        scid::database::strStrip (ExcludeMove, '=');
    }
    const char* GetEco() const { return ECOstr_.c_str(); }
    void   SetNumRows (scid::database::uint nrows) { TargetRows = nrows; }
    void   GuessNumRows (void);
    void   SetMaxTableLines (scid::database::uint nlines) {
        if (nlines <= OPTABLE_MAX_TABLE_LINES) {
            MaxTableLines = nlines;
        }
    }
    scid::database::uint   GetMaxTableLines (void) { return MaxTableLines; }
    void   SetMaxExtraMoves (scid::database::uint nmoves) {
        MaxNoteLength = (OPTABLE_COLUMNS + nmoves) * 2;
    }
    scid::database::uint   GetMaxExtraMoves (void) {
        return (MaxNoteLength / 2) - OPTABLE_COLUMNS;
    }
    scid::database::uint   GetNumLines (void) { return NumLines; }
    void   SetMaxThemeMoveNumber (scid::database::uint x) { MaxThemeMoveNumber = x; }
    bool   Add (OpLine * line);
    scid::database::uint   PercentScore (void);
    scid::database::uint   TheoryPercent (void);
    scid::database::uint   TheoryScore (void);
    scid::database::uint   PercentFreq (scid::database::resultT result);
    scid::database::uint   AvgLength (scid::database::resultT result);
    scid::database::uint   AvgElo (scid::database::colorT color, scid::database::uint *count, scid::database::uint *oppScore, scid::database::uint *oppPerf);
    void   BestGames (scid::database::DString * dstr, scid::database::uint count, const char * rtype);
    void   TopPlayers (scid::database::DString * dstr, scid::database::colorT c, scid::database::uint count);
    void   TopEcoCodes (scid::database::DString * dstr, scid::database::uint count);
    void   PrintStemLine (scid::database::DString * dstr, scid::database::uint format, bool exclude);
    void   PrintStemLine (scid::database::DString * dstr) { PrintStemLine (dstr, Format, false); }
    void   MakeRows (void);
#ifdef WINCE
    void   DumpLines (/*FILE **/Tcl_Channel fp);
#else
    void   DumpLines (FILE * fp);
#endif
    void   PrintTable (scid::database::DString * dstr, const char *title, const char *comment);
    void   PrintLaTeX (scid::database::DString * dstr,const char *title, const char *comment);
    void   PrintHTML (scid::database::DString * str, const char *title, const char *comment);
    void   PrintText (scid::database::DString * str, const char *title, const char *comment,
                      bool htext);
    static scid::database::uint FormatFromStr (const char * str);
    scid::database::uint   AddMoveOrder (scid::database::Game * g);
    void   PopularMoveOrders (scid::database::DString * dstr, scid::database::uint count);
    void   ThemeReport (scid::database::DString * dstr, scid::database::uint argc, const char ** argv);
    void   AddEndMaterial (scid::database::matSigT ms, bool inFilter);
    void   EndMaterialReport (scid::database::DString * dstr, const char * repGames,
                              const char * allGames);
    scid::database::uint * SelectGames (char type, scid::database::uint number);
};

#endif // SCID_OPTABLE_H

//////////////////////////////////////////////////////////////////////
// optable.h
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//  FILE:       crosstab.h
//              Crosstable class
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    3.6
//
//  Notice:     Copyright (c) 2000-2004 Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//
//////////////////////////////////////////////////////////////////////

#ifndef WINCE

#ifndef SCID_CROSSTAB_H
#define SCID_CROSSTAB_H

#include "scidup/database/common.h"
#include "scidup/core/date.h"
#include "scidup/core/dstring.h"
#include <scidup/spelling/spelling.h>

const scid::database::uint CROSSTABLE_MaxPlayers = 500;  // Max. number of players.
const scid::database::uint CROSSTABLE_MaxRounds =   60;  // Max. number of Swiss event rounds.

struct clashT
{
    scid::database::resultT  result;
    scid::database::uint     gameNum;
    scid::database::uint     opponent;  // For swiss crosstables.
    scid::database::colorT   color;
    scid::database::uint     round;
    clashT * next;
};

enum crosstableSortT
{
    CROSSTABLE_SortName, CROSSTABLE_SortElo, CROSSTABLE_SortScore, CROSSTABLE_SortCountry
};

enum crosstableOutputT
{
    CROSSTABLE_Plain, CROSSTABLE_Hypertext, CROSSTABLE_Html, CROSSTABLE_LaTeX
};

enum crosstableModeT
{
    CROSSTABLE_AllPlayAll, CROSSTABLE_Swiss, CROSSTABLE_Knockout,
    CROSSTABLE_Auto
};

struct playerDataT
{
    scid::database::idNumberT   id;
    char *      name;
    scid::database::eloT        elo;
    scid::database::uint        score;   // Score, stored as 2 pts per win, 1 pt per draw.
    scid::database::uint        n_won;
    scid::database::uint        n_draw;
    scid::database::uint        n_loss;
    scid::database::uint        gameCount;
    scid::database::uint        tiebreak; // Sonneborn-Berger tiebreak for all-play-all,
                          // or Bucholz tiebreak for Swiss.
    scid::database::uint        oppEloCount;
    scid::database::uint        oppEloTotal;
    scid::database::uint        oppEloScore;  // score against Elo opponents
    clashT *    firstClash [CROSSTABLE_MaxPlayers];
    clashT *    lastClash [CROSSTABLE_MaxPlayers];
    scid::database::uint        clashCount[CROSSTABLE_MaxPlayers];
    clashT *    roundClash [CROSSTABLE_MaxRounds];
    char        title [8];
    char        country [8];
    scid::database::dateT       birthdate;
    int         ageInYears;
    bool        printed;
};


class Crosstable
{
  private:

    scid::database::uint         PlayerCount;
    scid::database::uint         GameCount;
    scid::database::uint         MaxClashes;  // Maximum games between any two players
    scid::database::uint         MaxRound;
    scid::database::uint         ResultCount [scid::database::NUM_RESULT_TYPES];
    scid::database::dateT        FirstDate;

    bool         ShowTitles;
    bool         ShowElos;
    bool         ShowCountries;
    bool         ShowFlags;
    bool         ShowTallies;
    bool         ShowAges;
    bool         ShowTiebreaks;
    bool         SwissColors;  // If true, show colors in Swiss tables.
    bool         SeparateScoreGroups;
    char         DecimalPointChar;
    bool         APAColumnNums;  // If true, print numbers instead of partial
                                 // names above all-play-all table columns.

    crosstableOutputT OutputFormat;
    crosstableSortT SortOption;
    bool         ThreeWin;

    playerDataT * PlayerData [CROSSTABLE_MaxPlayers];

    // The following fields are set in PrintTable() and then used in
    // each of the private Print* methods:
    bool         PrintTitles;
    bool         PrintRatings;
    bool         PrintCountries;
    bool         PrintFlags;
    bool         PrintTallies;
    bool         PrintAges;
    bool         PrintTiebreaks;
    const char * StartTable;
    const char * EndTable;
    const char * StartRow;
    const char * EndRow;
    const char * NewLine;
    const char * BlankRowLine;
    const char * StartCol;
    const char * EndCol;
    const char * StartRightCol;
    const char * EndRightCol;
    const char * StartBoldCol;
    const char * EndBoldCol;
    scid::database::uint         LongestNameLen;
    scid::database::uint         LineWidth;
    scid::database::uint         PlayerNumWidth;
    scid::database::uint         SortedIndex [CROSSTABLE_MaxPlayers];
    scid::database::uint         InvertedIndex [CROSSTABLE_MaxPlayers];
    scid::database::uint         CurrentGame;

    void   Tiebreaks (crosstableModeT mode);

    void   PrintDashesLine (scid::database::DString * dstr);
    void   PrintPlayer (scid::database::DString * dstr, playerDataT * pdata);
    void   PrintPerformance (scid::database::DString * dstr, playerDataT * pdata);
    void   PrintScorePercentage (scid::database::DString * dstr, playerDataT * pdata);
    void   PrintAllPlayAll (scid::database::DString * dstr, scid::database::uint playerLimit);
    void   PrintKnockout (scid::database::DString * dstr, scid::database::uint playerLimit);
    void   PrintSwiss (scid::database::DString * dstr, scid::database::uint playerLimit);

    void   Init();
    void   Destroy();

  public:
    Crosstable() { Init(); }
    ~Crosstable() { Destroy(); }

    void   SetOutputFormat (crosstableOutputT opt) { OutputFormat = opt; }
    void   SetPlainOutput()     { OutputFormat = CROSSTABLE_Plain; }
    void   SetHtmlOutput()      { OutputFormat = CROSSTABLE_Html; }
    void   SetHypertextOutput() { OutputFormat = CROSSTABLE_Hypertext; }
    void   SetLaTeXOutput()     { OutputFormat = CROSSTABLE_LaTeX; }

    void   SetSortOption (crosstableSortT option) { SortOption = option; }
    void   SetThreeWin   (bool threewin) { ThreeWin = threewin; }

    void   SortByName()  { SortOption = CROSSTABLE_SortName; }
    void   SortByElo()   { SortOption = CROSSTABLE_SortElo; }
    void   SortByScore() { SortOption = CROSSTABLE_SortScore; }
    void   SortByCountry() { SortOption = CROSSTABLE_SortCountry; }

    void   SetAges (bool b) {ShowAges = b; }
    void   SetTitles (bool b) { ShowTitles = b; }
    void   SetElos (bool b) { ShowElos = b; }
    void   SetCountries (bool b) { ShowCountries = b; }
    void   SetFlags (bool b) { ShowFlags = b; }
    void   SetTallies (bool b) { ShowTallies = b; }
    void   SetTiebreaks (bool b) { ShowTiebreaks = b; }
    void   SetSwissColors (bool b) { SwissColors = b; }
    void   SetSeparateScoreGroups (bool b) { SeparateScoreGroups = b; }
    void   SetDecimalPointChar (char ch) { DecimalPointChar = ch; }
    void   SetNumberedColumns (bool b) { APAColumnNums = b; }

    scid::database::uint   NumPlayers() { return PlayerCount; }
    scid::database::errorT AddPlayer (scid::database::idNumberT id, const char * name, scid::database::eloT elo,
                      const scidup::spelling::SpellChecker*);
    scid::database::errorT AddResult (scid::database::uint gameNumber, scid::database::idNumberT white, scid::database::idNumberT black,
                      scid::database::resultT result, scid::database::uint round, scid::database::dateT date);

    crosstableModeT BestMode (void);
    scid::database::eloT   AvgRating();
    void   PrintTable (scid::database::DString * dstr, crosstableModeT mode, scid::database::uint playerLimit, int currentGame);

    static scid::database::uint Performance (scid::database::uint oppAvg, scid::database::uint percentage);
    static scid::database::uint FideCategory (scid::database::eloT rating);
    static scid::database::eloT OpponentElo (scid::database::eloT player, scid::database::eloT opponent);
    static int RatingChange (scid::database::eloT player, scid::database::uint oppAvg, scid::database::uint percentage, 
                             scid::database::uint count);
};

#endif  // #ifndef SCID_CROSSTAB_H

#endif // WINCE

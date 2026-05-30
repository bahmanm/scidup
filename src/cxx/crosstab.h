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

#include "scid/core/date.h"
#include "scid/core/dstring.h"
#include "scid/core/game_result.h"
#include "scid/database/common.h"
#include <scid/spelling/spelling.h>

const scid::core::uint CROSSTABLE_MaxPlayers = 500;  // Max. number of players.
const scid::core::uint CROSSTABLE_MaxRounds =   60;  // Max. number of Swiss event rounds.

struct clashT
{
    scid::core::resultT  result;
    scid::core::uint     gameNum;
    scid::core::uint     opponent;  // For swiss crosstables.
    scid::core::colorT   color;
    scid::core::uint     round;
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
    scid::core::ratingT        elo;
    scid::core::uint        score;   // Score, stored as 2 pts per win, 1 pt per draw.
    scid::core::uint        n_won;
    scid::core::uint        n_draw;
    scid::core::uint        n_loss;
    scid::core::uint        gameCount;
    scid::core::uint        tiebreak; // Sonneborn-Berger tiebreak for all-play-all,
                          // or Bucholz tiebreak for Swiss.
    scid::core::uint        oppEloCount;
    scid::core::uint        oppEloTotal;
    scid::core::uint        oppEloScore;  // score against Elo opponents
    clashT *    firstClash [CROSSTABLE_MaxPlayers];
    clashT *    lastClash [CROSSTABLE_MaxPlayers];
    scid::core::uint        clashCount[CROSSTABLE_MaxPlayers];
    clashT *    roundClash [CROSSTABLE_MaxRounds];
    char        title [8];
    char        country [8];
    scid::core::dateT       birthdate;
    int         ageInYears;
    bool        printed;
};


class Crosstable
{
  private:

    scid::core::uint         PlayerCount;
    scid::core::uint         GameCount;
    scid::core::uint         MaxClashes;  // Maximum games between any two players
    scid::core::uint         MaxRound;
    scid::core::uint         ResultCount [scid::core::NUM_RESULT_TYPES];
    scid::core::dateT        FirstDate;

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
    scid::core::uint         LongestNameLen;
    scid::core::uint         LineWidth;
    scid::core::uint         PlayerNumWidth;
    scid::core::uint         SortedIndex [CROSSTABLE_MaxPlayers];
    scid::core::uint         InvertedIndex [CROSSTABLE_MaxPlayers];
    scid::core::uint         CurrentGame;

    void   Tiebreaks (crosstableModeT mode);

    void   PrintDashesLine (scid::core::DString * dstr);
    void   PrintPlayer (scid::core::DString * dstr, playerDataT * pdata);
    void   PrintPerformance (scid::core::DString * dstr, playerDataT * pdata);
    void   PrintScorePercentage (scid::core::DString * dstr, playerDataT * pdata);
    void   PrintAllPlayAll (scid::core::DString * dstr, scid::core::uint playerLimit);
    void   PrintKnockout (scid::core::DString * dstr, scid::core::uint playerLimit);
    void   PrintSwiss (scid::core::DString * dstr, scid::core::uint playerLimit);

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

    scid::core::uint   NumPlayers() { return PlayerCount; }
    scid::core::errorT AddPlayer (scid::database::idNumberT id, const char * name, scid::core::ratingT elo,
                      const scid::spelling::SpellChecker*);
    scid::core::errorT AddResult (scid::core::uint gameNumber, scid::database::idNumberT white, scid::database::idNumberT black,
                      scid::core::resultT result, scid::core::uint round, scid::core::dateT date);

    crosstableModeT BestMode (void);
    scid::core::ratingT   AvgRating();
    void   PrintTable (scid::core::DString * dstr, crosstableModeT mode, scid::core::uint playerLimit, int currentGame);

    static scid::core::uint Performance (scid::core::uint oppAvg, scid::core::uint percentage);
    static scid::core::uint FideCategory (scid::core::ratingT rating);
    static scid::core::ratingT OpponentElo (scid::core::ratingT player, scid::core::ratingT opponent);
    static int RatingChange (scid::core::ratingT player, scid::core::uint oppAvg, scid::core::uint percentage,
                             scid::core::uint count);
};

#endif  // #ifndef SCID_CROSSTAB_H

#endif // WINCE

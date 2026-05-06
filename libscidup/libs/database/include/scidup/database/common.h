//////////////////////////////////////////////////////////////////////
//
//  FILE:       common.h
//              Common macros, structures and constants.
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    3.6.6
//
//  Notice:     Copyright (c) 2000-2004  Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//              Copyright (c) 2006-2007 Pascal Georges
//
//////////////////////////////////////////////////////////////////////

#ifndef SCID_COMMON_H
#define SCID_COMMON_H

#include "scidup/core/board.h"
#include "scidup/core/error.h"
#include <assert.h>
#include <cstddef>
#define ASSERT(f) assert(f)


// Version: div by 100 for major number, modulo 100 for minor number
// so 109 = 1.9, 110 = 1.10, etc.
namespace scid::database {

typedef unsigned short versionT;
const versionT SCID_VERSION = 400;     // Current file format version = 4.0


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TYPE DEFINITIONS

typedef char    sanStringT [ 10];   // SAN Move Notation

enum fileModeT {
    FMODE_None = 0,
    FMODE_ReadOnly,
    FMODE_WriteOnly,
    FMODE_Both,
    FMODE_Create
};


//  Game Information types

typedef uint            gamenumT;
typedef ushort          eloT;
typedef ushort          ecoT;
typedef char            ecoStringT [6];   /* "A00j1" */

const ecoT ECO_None = 0;

// Rating types:

const byte RATING_Elo = 0;
const byte RATING_Rating = 1;
const byte RATING_Rapid = 2;
const byte RATING_ICCF = 3;
const byte RATING_USCF = 4;
const byte RATING_DWZ = 5;
const byte RATING_BCF = 6;

extern const char * ratingTypeNames [8];   // Defined in game.cpp



// Result Type

const uint NUM_RESULT_TYPES = 4;
typedef byte    resultT;
const resultT
    RESULT_None  = 0,
    RESULT_White = 1,
    RESULT_Black = 2,
    RESULT_Draw  = 3;

const uint RESULT_SCORE[4] = { 1, 2, 0, 1 };
const char RESULT_CHAR [4]       = { '*',  '1',    '0',    '='       };
const char RESULT_STR [4][4]     = { "*",  "1-0",  "0-1",  "=-="     };
const char RESULT_LONGSTR [4][8] = { "*",  "1-0",  "0-1",  "1/2-1/2" };
const resultT RESULT_OPPOSITE [4] = {
    RESULT_None, RESULT_Black, RESULT_White, RESULT_Draw
};

} // namespace scid::database
#endif  // #ifdef SCID_COMMON_H

//////////////////////////////////////////////////////////////////////
//  EOF:  common.h
//////////////////////////////////////////////////////////////////////

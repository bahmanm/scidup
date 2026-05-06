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
#include "scidup/core/rating.h"
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



} // namespace scid::database
#endif  // #ifdef SCID_COMMON_H

//////////////////////////////////////////////////////////////////////
//  EOF:  common.h
//////////////////////////////////////////////////////////////////////

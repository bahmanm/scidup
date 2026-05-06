/*
* Copyright (c) 1999-2002  Shane Hudson
* Copyright (c) 2006-2009  Pascal Georges
* Copyright (C) 2014-2016  Fulvio Benini

* This file is part of Scid (Shane's Chess Information Database).
*
* Scid is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation.
*
* Scid is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Scid.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SCID_INDEX_H
#define SCID_INDEX_H

#include "scidup/database/common.h"
#include "scidup/database/indexentry.h"
#include <memory>


//////////////////////////////////////////////////////////////////////
//  Index:  Class Definition

namespace scid::database {

class Index
{
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    Index();
    ~Index();
    Index(const Index&) = delete;
    Index& operator=(const Index&) = delete;

    void Close();

    const IndexEntry* GetEntry (gamenumT g) const;

    /**
     * GetBadNameIdCount() - return the number of invalid name handles.
     *
     * To save space, avoiding duplicates, the index keep handles
     * to strings stored in the namebase file.
     * If one of the two files is corrupted, the index may have
     * handles to strings that do not exists.
     * This functions returns the number of invalid name handles.
     */
    int GetBadNameIdCount() const;

    void setBadNameIdCount(int count);

    /**
     * Header getter functions
     */
    gamenumT GetNumGames() const;

    void addEntry(const IndexEntry& ie);

    void replaceEntry(const IndexEntry& ie, gamenumT replaced);

private:
    void Init();
};



} // namespace scid::database
#endif  // #ifdef SCID_INDEX_H

//////////////////////////////////////////////////////////////////////
//  EOF: index.h
//////////////////////////////////////////////////////////////////////

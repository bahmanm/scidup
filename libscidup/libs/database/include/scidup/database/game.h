//////////////////////////////////////////////////////////////////////
//
//  FILE:       game.h
//              Game class for Scid.
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    3.5
//
//  Notice:     Copyright (c) 2000-2003 Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//
//////////////////////////////////////////////////////////////////////


#ifndef SCID_GAME_H
#define SCID_GAME_H

#include "scidup/core/game.h"
#include "scidup/core/game_result.h"
#include "scidup/core/movetext_location.h"
#include "scidup/database/common.h"
#include "scidup/core/date.h"
#include "scidup/eco/code.h"
#include "scidup/core/nags.h"
#include "scidup/database/indexentry.h"
#include "scidup/database/namebase.h"
#include "scidup/core/position.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace scid::database {

class ByteBuffer;
class Game;
class TextBuffer;
struct patternT;
enum gameExactMatchT : int;

namespace game_storage {
std::pair<IndexEntry, TagRoster> encode(const Game& game,
                                        std::vector<byte>& dest);
void loadStandardTags(Game& game, IndexEntry const& ie, TagRoster const& tags);
errorT decode(Game& game, IndexEntry const& ie, TagRoster const& tags,
              ByteBuffer buf);
errorT decodeMovesOnly(Game& game, ByteBuffer& buf);
} // namespace game_storage

//////////////////////////////////////////////////////////////////////
//  Game:  Class Definition

class Game {
    // Header data: tag pairs
    scid::core::Game coreGame_;
    // TODO [Game]: Keep Scid flags out of the core metadata model until there
    // is a domain reason for them outside database/app compatibility.
    char        scidFlags_[22];

    // Position and moves
    scid::core::MovetextLocation coreLocation_;

private:
    Game(const Game&);
    bool setCoreLocation(scid::core::MovetextLocation location);
    friend std::pair<IndexEntry, TagRoster> game_storage::encode(
        const Game& game, std::vector<byte>& dest);
    friend errorT game_storage::decode(Game& game, IndexEntry const& ie,
                                       TagRoster const& tags, ByteBuffer buf);
    friend errorT game_storage::decodeMovesOnly(Game& game, ByteBuffer& buf);
public:
    Game();
    ~Game();
    scid::core::Game& coreGame();
    const scid::core::Game& coreGame() const;
    scid::core::MovetextLocation coreLocation() const;
    void clear();

    void setScidFlags(const char* s, size_t len);
    const char* scidFlags() const;

    void restoreLocation(scid::core::MovetextLocation location);

    Game* clone();
};

} // namespace scid::database
#endif  // #ifndef SCID_GAME_H

//////////////////////////////////////////////////////////////////////
//  EOF:    game.h
//////////////////////////////////////////////////////////////////////

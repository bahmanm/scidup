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
#include "scidup/database/common.h"
#include "scidup/core/date.h"
#include "scidup/eco/code.h"
#include "scidup/core/nags.h"
#include "scidup/database/game_TEMP/search.h"
#include "scidup/database/indexentry.h"
#include "scidup/database/namebase.h"
#include "scidup/core/position.h"
#include <forward_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace scid::database {

class ByteBuffer;
class Game;
class TextBuffer;
struct moveT;
enum markerT : byte;

namespace game_storage {
std::pair<IndexEntry, TagRoster> encode(const Game& game,
                                        std::vector<byte>& dest);
void loadStandardTags(Game& game, IndexEntry const& ie, TagRoster const& tags);
errorT decode(Game& game, IndexEntry const& ie, TagRoster const& tags,
              ByteBuffer buf);
errorT decodeMovesOnly(Game& game, ByteBuffer& buf);
errorT decodeSkipTags(Game& game, ByteBuffer* buf);
errorT decodeNextMove(Game& game, ByteBuffer* buf, simpleMoveT& sm);
} // namespace game_storage

namespace game_notation {
std::string currentPositionUci(const Game& game);
std::string nextMoveUci(const Game& game);
std::string nextSan(Game& game);
std::string previousSan(Game& game);
std::string previousMoveUci(const Game& game);
} // namespace game_notation

//////////////////////////////////////////////////////////////////////
//  Game:  Class Definition

class Game {
    // Header data: tag pairs
    scid::core::Game coreGame_;
    // TODO [Game]: Keep Scid flags out of the core metadata model until there
    // is a domain reason for them outside database/app compatibility.
    char        ScidFlags[22];

    // Position and moves
    byte        moveChunkUsed_;
    std::forward_list<std::unique_ptr<moveT[]> > moveChunks_;
    std::unique_ptr<Position> CurrentPos{new Position};
    moveT*      FirstMove;
    moveT*      CurrentMove;
    uint        VarDepth;     // Current variation depth.
    ushort      NumHalfMoves; // Total half moves in the main line.

private:
    Game(const Game&);
    moveT* allocMove();
    moveT* NewMove(markerT marker);
    void ClearMoves();
    errorT DecodeVariation(ByteBuffer& buf, std::vector<moveT*>& comment_marks);
    static errorT decodeMove(ByteBuffer* buf, simpleMoveT* sm, byte val,
                             const Position* pos);
    // TODO [Game]: Move these database storage-codec operations out of Game
    // once the database wrapper around the future core Game exists.
    void LoadStandardTags(IndexEntry const& ie, TagRoster const& tags);
    std::pair<IndexEntry, TagRoster> Encode(std::vector<byte>& dest) const;
    errorT DecodeSkipTags(ByteBuffer* buf);
    errorT DecodeNextMove(ByteBuffer* buf, simpleMoveT& sm);
    errorT Decode(IndexEntry const& ie, TagRoster const& tags, ByteBuffer buf);
    errorT DecodeMovesOnly(ByteBuffer& buf);
    // TODO [Game]: Move these database search operations out of Game once the
    // database wrapper around the future core Game exists.
    bool MaterialMatch(bool PromotionsFlag, ByteBuffer& buf, byte* min,
                       byte* max, patternT* ptn, size_t ptn_size, int minPly,
                       int maxPly, int matchLength, bool oppBishops,
                       bool sameBishops, int minDiff, int maxDiff);
    bool ExactMatch(Position* pos, ByteBuffer* buf, gameExactMatchT searchType);
    bool VarExactMatch(Position* searchPos, gameExactMatchT searchType);
    void TEMP_syncCoreMovetext();

    friend std::pair<IndexEntry, TagRoster> game_storage::encode(
        const Game& game, std::vector<byte>& dest);
    friend void game_storage::loadStandardTags(Game& game,
                                               IndexEntry const& ie,
                                               TagRoster const& tags);
    friend errorT game_storage::decode(Game& game, IndexEntry const& ie,
                                       TagRoster const& tags, ByteBuffer buf);
    friend errorT game_storage::decodeMovesOnly(Game& game, ByteBuffer& buf);
    friend errorT game_storage::decodeSkipTags(Game& game, ByteBuffer* buf);
    friend errorT game_storage::decodeNextMove(Game& game, ByteBuffer* buf,
                                               simpleMoveT& sm);
    friend bool game_search::materialMatch(
        Game& game, bool promotionsFlag, ByteBuffer& buf, byte* min, byte* max,
        patternT* ptn, std::size_t ptnSize, int minPly, int maxPly,
        int matchLength, bool oppBishops, bool sameBishops, int minDiff,
        int maxDiff);
    friend bool game_search::exactMatch(Game& game, Position* pos,
                                        ByteBuffer* buf,
                                        gameExactMatchT searchType);
    friend bool game_search::varExactMatch(Game& game, Position* pos,
                                           gameExactMatchT searchType);
    friend struct LegacyGamePgnEncoder;
    friend std::string game_notation::currentPositionUci(const Game& game);
    friend std::string game_notation::nextMoveUci(const Game& game);
    friend std::string game_notation::nextSan(Game& game);
    friend std::string game_notation::previousSan(Game& game);
    friend std::string game_notation::previousMoveUci(const Game& game);

    /**
     * Contains the information of the current position in the game, so that
     * after an operation that alters the location, it can be restored.
     */
    struct GameSavedPos {
        Position pos;
        moveT* move;
        uint varDepth;
    };

public:
    Game();
    ~Game();
    scid::core::Game& coreGame();
    const scid::core::Game& coreGame() const;
    void Clear();
    void strip(bool variations, bool comments, bool NAGs);

    /// Setup the start position from a FEN string and remove all the moves.
    /// If the FEN is invalid the game is not changed.
    errorT SetStartFen(const char* fenStr);

    /// Set a new start position and remove all the moves.
    void SetStartPos(Position const& pos);

    void SetScidFlags(const char* s, size_t len);

    //////////////////////////////////////////////////////////////
    // Functions to add or delete moves:
    //
    errorT addMove(simpleMoveT const& sm);
    errorT addVariation();
    errorT deleteVariation();
    errorT promoteVariationToFirst();
    errorT promoteVariationToMainline();
    void truncate();
    void truncateStart();

    //////////////////////////////////////////////////////////////
    // Functions that move the current location (only CurrentPos,
    // CurrentMove and VarDepth are modified by these functions):
    //
    errorT next();
    errorT previous();
    errorT enterVariation(uint varNumber);
    errorT exitVariation();
    // TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
    // instead of keeping it on the generic Game cursor surface.
    errorT MoveForwardInPGN();
    errorT MoveToLocationInPGN(unsigned stopLocation);
    void toStart();
    /// Move to the end of the main line.
    void toEnd();
    void toPly(int hmNumber);
    GameSavedPos currentLocation() const;
    void restoreLocation(const GameSavedPos& savedPos);

    //////////////////////////////////////////////////////////////
    // Functions that get information about the current location.
    //
    Position* currentPos();
    const Position* currentPos() const;
    simpleMoveT* currentMove();
    ushort currentPly() const;
    uint variationCount() const;

    // Each variation has a "level" and a "number".
    // - "level" is the number of times that is necessary to call
    //   exitVariation() to reach the main line.
    // - "number" is the ordered position in the list of variations for the
    // current root position (first variation is number 0).
    // The main line is 0,0.
    uint variationLevel() const;
    uint variationNumber() const;

    // TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
    // instead of keeping it on the generic Game cursor surface.
    unsigned GetLocationInPGN() const;
    unsigned GetPgnOffset() const;

    bool isAtVariationStart() const;
    bool isAtVariationEnd() const;
    bool isAtStart() const;
    bool isAtEnd() const;
    bool isAtEmptyVariation() const;

    //////////////////////////////////////////////////////////////
    // Functions that get/set information about the last/next move.
    // Notice: when location is at the start of the game or a variation,
    // infomation are stored into the START_MARKER.
    // TODO [Game]: Replace this compatibility surface with Move.metadata,
    // MoveAction notation helpers, and GameCursor traversal.
    //
    errorT AddNag(byte nag);
    errorT RemoveNag(bool isMoveNag);
    void ClearNags();
    byte* GetNags() const;
    byte* GetNextNags() const;

    /// Return the comments of the previous 2 moves (useful to compute clocks).
    /// If there are no previous moves, return an empty comment.
    std::pair<const char*, const char*> previousComments() const;
    const char* GetMoveComment() const;
    void SetMoveComment(const char* comment);

    Game* clone();
};

} // namespace scid::database
#endif  // #ifndef SCID_GAME_H

//////////////////////////////////////////////////////////////////////
//  EOF:    game.h
//////////////////////////////////////////////////////////////////////

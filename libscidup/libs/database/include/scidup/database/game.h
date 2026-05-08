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
#include "scidup/database/game_TEMP/legacy_encode_options.h"
#include "scidup/database/game_TEMP/search.h"
#include "scidup/database/indexentry.h"
#include "scidup/database/namebase.h"
#include "scidup/core/pgn/movetext.h"
#include "scidup/core/position.h"
#include <forward_list>
#include <functional>
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

//////////////////////////////////////////////////////////////////////
//  Game:  Class Definition

class Game {
    // Header data: tag pairs
    scid::core::Game coreGame_;
    scidup::eco::Code EcoCode;
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

    // TODO [Game]: Move legacy export/encode options out of Game. These are
    // compatibility settings for the legacy writer, not core Game state.
    uint        PgnStyle;        // see PGN_STYLE macros above.
    gameFormatT PgnFormat;       // see PGN_FORMAT macros above.
    uint        HtmlStyle;       // HTML diagram style, see DumpHtmlBoard method in position.cpp.

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
    std::string& find_or_create_tag(std::string_view tag);
    void TEMP_syncCoreMovetext();
    void viewTagPairsImpl(
        const std::function<void(const char*, const char*)>& visitor) const;

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
    const scid::core::Game& coreGame() const;
    void Clear();
    void strip(bool variations, bool comments, bool NAGs);

    bool HasNonStandardStart(char* outFEN = nullptr, size_t outFENLen = 0) const;

    // The last field of the initial FEN is the number of the full moves.
    // @return  2 * full move - 2; +1 if it is black to move.
    long long initialPlyCounter() const;

    /// Setup the start position from a FEN string and remove all the moves.
    /// If the FEN is invalid the game is not changed.
    errorT SetStartFen(const char* fenStr);

    /// Set a new start position and remove all the moves.
    void SetStartPos(Position const& pos);
    void SetStartPos(std::unique_ptr<Position> pos);

    void SetScidFlags(const char* s, size_t len);
    void SetScidFlags(const char* s);

    ushort GetNumHalfMoves();

    //////////////////////////////////////////////////////////////
    // Functions to add or delete moves:
    //
    errorT AddMove(simpleMoveT const& sm);
    errorT AddVariation();
    errorT DeleteVariation();
    errorT FirstVariation();
    errorT MainVariation();
    void Truncate();
    void TruncateStart();

    //////////////////////////////////////////////////////////////
    // Functions that move the current location (only CurrentPos,
    // CurrentMove and VarDepth are modified by these functions):
    //
    errorT MoveForward();
    errorT MoveBackup();
    errorT MoveIntoVariation(uint varNumber);
    errorT MoveExitVariation();
    // TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
    // instead of keeping it on the generic Game cursor surface.
    errorT MoveForwardInPGN();
    errorT MoveToLocationInPGN(unsigned stopLocation);
    void MoveToStart();
    /// Move to the end of the main line.
    void MoveToEnd();
    void MoveToPly(int hmNumber);
    GameSavedPos currentLocation() const;
    void restoreLocation(const GameSavedPos& savedPos);

    //////////////////////////////////////////////////////////////
    // Functions that get information about the current location.
    //
    const Position* currentPos() const;
    Position* GetCurrentPos(); // Deprecated, use the const version
    /// @return an "UCI position" string that leads to the current position
    std::string currentPosUCI() const;
    simpleMoveT* GetCurrentMove();
    ushort GetCurrentPly() const;
    uint GetNumVariations() const;

    // Each variation has a "level" and a "number".
    // - "level" is the number of times that is necessary to call
    //   MoveExitVariation() to reach the main line.
    // - "number" is the ordered position in the list of variations for the
    // current root position (first variation is number 0).
    // The main line is 0,0.
    uint GetVarLevel() const;
    uint GetVarNumber() const;

    // TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
    // instead of keeping it on the generic Game cursor surface.
    unsigned GetLocationInPGN() const;
    unsigned GetPgnOffset() const;

    bool AtVarStart() const;
    bool AtVarEnd() const;
    bool AtStart() const;
    bool AtEnd() const;
    bool AtEmptyVar() const;

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
    std::string& accessMoveComment();
    void SetMoveComment(const char* comment);

    const char* GetNextSAN();
    void GetSAN(char* str);
    void GetPrevSAN(char* str);
    void GetPrevMoveUCI(char* str) const;
    void GetNextMoveUCI(char* str);

    void viewMainlineMoves(
        const std::function<void(const simpleMoveT&)>& visitor) const;
    void viewMovetext(
        const std::function<void(const scid::core::pgn::MovetextEntry&)>&
            visitor) const;

    //////////////////////////////////////////////////////////////
    // Functions that get/set the tag pairs:
    //

    // Invoke @e visitor for each existing tag pair
    template <typename TFunc> void viewTagPairs(TFunc visitor) const;

    // Add a tag.
    // For the tags that cannot be duplicated (like Event or White), the
    // previous value will be overwritten.
    std::string& addTag(std::string_view tag, std::string_view value);

    // Change the value of a tag (add the tag if it wasn't present).
    template <typename... Args>
    std::string& assignTagValue(std::string_view tag, Args&&... args) {
        return find_or_create_tag(tag).assign(std::forward<Args>(args)...);
    }

    const std::vector<std::pair<std::string, std::string>>& GetExtraTags() const;
    const char* FindExtraTag(const char* tag) const;
    void ClearExtraTags();
    void RemoveExtraTag(std::string_view tag);

    void     SetEventStr (const char * str);
    void     SetSiteStr  (const char * str);
    void     SetWhiteStr (const char * str);
    void     SetBlackStr (const char * str);
    void     SetRoundStr (const char * str);
    void     SetDate (dateT date);
    void     SetEventDate (dateT date);
    void     SetResult (resultT res);
    void     SetWhiteElo (ratingT elo);
    void     SetBlackElo (ratingT elo);
    void     SetWhiteRatingType (ratingTypeT b);
    void     SetBlackRatingType (ratingTypeT b);
    int setRating(colorT col, const char* ratingType, size_t ratingTypeLen,
                  std::pair<const char*, const char*> rating);
    void     SetEco (scidup::eco::Code eco);
    const char* GetEventStr () const;
    const char* GetSiteStr ()  const;
    const char* GetWhiteStr () const;
    const char* GetBlackStr () const;
    const char* GetRoundStr () const;
    dateT    GetDate ()        const;
    dateT    GetEventDate ()   const;
    resultT  GetResult ()      const;
    std::string_view GetResultStr() const;
    ratingT     GetWhiteElo ()    const;
    ratingT     GetBlackElo ()    const;
    ratingTypeT GetWhiteRatingType () const;
    ratingTypeT GetBlackRatingType () const;
    scidup::eco::Code GetEco() const;
    ratingT     GetAverageElo ();

    // TODO [Game]: Replace this legacy export/encode compatibility surface with
    // explicit encoder/exporter options outside the Game aggregate.
    // PGN conversion
    std::pair<const char*, unsigned> WriteToPGN (uint lineWidth = 0,
                                                 bool NewLineAtEnd = false,
                                                 bool newLineToSpaces = true);

    void      ResetPgnStyle (void);
    void      ResetPgnStyle (uint flag);

    uint      GetPgnStyle ();
    void      SetPgnStyle (uint mask, bool setting);
    void      AddPgnStyle (uint mask);
    void      RemovePgnStyle (uint mask);

    void      SetPgnFormat (gameFormatT gf);
    bool      SetPgnFormatFromString (const char * str);
    static bool PgnFormatFromString (const char * str, gameFormatT * fmt);
    bool      IsPlainFormat ();
    bool      IsHtmlFormat  ();
    bool      IsLatexFormat ();
    bool      IsColorFormat ();

    void      SetHtmlStyle (uint style);
    uint      GetHtmlStyle ();

    errorT    GetPartialMoveList (DString * str, uint plyCount);

    Game* clone();
};

template <typename TFunc> void Game::viewTagPairs(TFunc visitor) const {
	viewTagPairsImpl(visitor);
}

} // namespace scid::database
#endif  // #ifndef SCID_GAME_H

//////////////////////////////////////////////////////////////////////
//  EOF:    game.h
//////////////////////////////////////////////////////////////////////

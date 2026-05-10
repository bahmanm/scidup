#include "scidup/database/game.h"

#include "scidup/database/bytebuf.h"
#include "scidup/database/common.h"
#include "scidup/database/indexentry.h"
#include "scidup/database/matsig.h"
#include "scidup/database/namebase.h"
#include "scidup/core/movetext_cursor.h"
#include "game_storage.h"
#include "movetree.h"
#include "stored.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

namespace scid::database {

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::loadStandardTags():
//      Sets the standard tag values for this game, given an
//      index file entry and a namebase that stores the
//      player/site/event/round names.
// TODO [Game]: Keep IndexEntry/TagRoster hydration in the database storage
// boundary. The future core Game should not know about compact database
// metadata records.
//
void Game::loadStandardTags(IndexEntry const& ie, TagRoster const& tags) {
    coreGame_.setEvent(tags.event);
    coreGame_.setSite(tags.site);
    coreGame_.setWhiteName(tags.white);
    coreGame_.setBlackName(tags.black);
    coreGame_.setRound(tags.round);
    coreGame_.setDate(ie.GetDate());
    coreGame_.setEventDate(ie.GetEventDate());
    coreGame_.setWhiteRating({ie.GetWhiteElo(), ie.GetWhiteRatingType()});
    coreGame_.setBlackRating({ie.GetBlackElo(), ie.GetBlackRatingType()});
    coreGame_.setResult(ie.GetResult());
    scidup::eco::String ecoStr;
    scidup::eco::toExtendedString(ie.GetEcoCode(), ecoStr);
    coreGame_.setEco(ecoStr);
    ie.GetFlagStr(scidFlags_, NULL);
    if (!ie.isChessStd())
        coreGame_.findOrCreateTag("Variant").assign("Chess960");
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// encodeKing(): encoding of King moves.
//
static byte encodeKing(squareT from, int to) {
    // Valid King difference-from-old-square values are:
    // -9, -8, -7, -1, 1, 7, 8, 9, and -2 and 2 for castling.
    // To convert this to a val in the range [1-10], we add 9 and
    // then look up the val[] table.
    // Coded values 1-8 are one-square moves; 9 and 10 are Castling.
    static const byte val[] = {
    /* -9 -8 -7 -6 -5 -4 -3 -2 -1  0  1   2  3  4  5  6  7  8  9 */
        1, 2, 3, 0, 0, 0, 0, 9, 4, 0, 5, 10, 0, 0, 0, 0, 6, 7, 8
    };
    auto diff = to - from;
    static_assert(std::is_same_v<decltype(diff), int>);

    // If target square is the from square, it is the null move, which
    // is represented as a king move to its own square and is encoded
    // as the byte value zero.
    ASSERT((to == from && val[diff+9] == 0) || val[diff+9] != 0);

    // Verify we have a valid King move:
    ASSERT(diff >= -9 && diff <= 9);
    return val[diff + 9];
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// encodeKnight(): encoding Knight moves.
//
static byte encodeKnight(squareT from, squareT to) {
    // Valid Knight difference-from-old-square values are:
    // -17, -15, -10, -6, 6, 10, 15, 17.
    // To convert this to a value in the range [1-8], we add 17 to
    // the difference and then look up the val[] table.

    auto diff = to - from;
    static_assert(std::is_same_v<decltype(diff), int>);
    static const byte val[] = {
    /* -17 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1  0 */
        1,  0,  2,  0,  0,  0,  0,  3,  0, 0, 0, 4, 0, 0, 0, 0, 0, 0,

    /*  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 */
        0, 0, 0, 0, 0, 5, 0, 0, 0, 6, 0, 0, 0, 0, 7, 0, 8
    };

    // Verify we have a valid knight move:
    ASSERT (diff >= -17  &&  diff <= 17  &&  val[diff + 17] != 0);
    return val[diff + 17];
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// encodeRook(): encoding rook moves.
//
static byte encodeRook(squareT from, squareT to) {
    // Valid Rook moves are to same rank, OR to same fyle.
    // We encode the 8 squares on the same rank 0-8, and the 8
    // squares on the same fyle 9-15. This means that for any particular
    // rook move, two of the values in the range [0-15] will be
    // meaningless, as they will represent the from-square.

    // Check if the two squares share the same rank:
    if (square_Rank(from) == square_Rank(to)) {
        return square_Fyle(to);
    }
    return 8 + square_Rank(to);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// encodeBishop(): encoding Bishop moves.
//
static byte encodeBishop(squareT from, squareT to) {
    // We encode a Bishop move as the Fyle moved to, plus
    // a one-bit flag to indicate if the direction was
    // up-right/down-left or vice versa.

    auto rankdiff = square_Rank(to) - square_Rank(from);
    auto fylediff = square_Fyle(to) - square_Fyle(from);
    static_assert(std::is_same_v<decltype(rankdiff), int>);
    static_assert(std::is_same_v<decltype(fylediff), int>);

    // If (rankdiff * fylediff) is negative, it's up-left/down-right:
    if (rankdiff * fylediff < 0)
        return square_Fyle(to) + 8;

    return square_Fyle(to);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// encodeQueen(): encoding Queen moves.
//
static byte encodeQueen(squareT from, squareT to, byte& multibyte) {
    // We cannot fit all Queen moves in one byte, so Rooklike moves
    // are in one byte (encoded the same way as Rook moves),
    // while diagonal moves are in two bytes.
    if (square_Rank(from) == square_Rank(to)) // Rook-horizontal move:
        return square_Fyle(to);

    if (square_Fyle(from) == square_Fyle(to)) // Rook-vertical move:
        return 8 + square_Rank(to);

    // Diagonal move:
    ASSERT(std::abs(to / 8 - from / 8) == std::abs(to % 8 - from % 8));
    // First, we put a rook-horizontal move to the from square (which
    // is illegal of course) to indicate it is NOT a rooklike move:
    // Now we put the to-square in the next byte. We add a 64 to it
    // to make sure that it cannot clash with the Special tokens (which
    // are in the range 0 to 15, since they are special King moves).
    multibyte = to + 64;
    return square_Fyle(from);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// encodePawn(): encoding Pawn moves.
//
static byte encodePawn(squareT from, squareT to, pieceT promo) {
    // Pawn moves require a promotion encoding.
    // The pawn moves are:
    // 0 = capture-left,
    // 1 = forward,
    // 2 = capture-right (all no promotion);
    //    3/4/5 = 0/1/2 with Queen promo;
    //    6/7/8 = 0/1/2 with Rook promo;
    //  9/10/11 = 0/1/2 with Bishop promo;
    // 12/13/14 = 0/1/2 with Knight promo;
    // 15 = forward TWO squares.

    byte val;
    auto diff = to - from;
    static_assert(std::is_same_v<decltype(diff), int>);

    if (diff < 0) { diff = -diff; }
    if (diff == 16) { // Move forward two squares
        val = 15;
        ASSERT (promo == EMPTY);

    } else {
        if (diff == 7) { val = 0; }
        else if (diff == 8) { val = 1; }
        else {  // diff is 9:
            ASSERT (diff == 9);
            val = 2;
        }
        if (promo != EMPTY) {
            // Handle promotions.
            // sm->promote must be Queen=2,Rook=3, Bishop=4 or Knight=5.
            // We add 3 for Queen, 6 for Rook, 9 for Bishop, 12 for Knight.

            ASSERT (promo >= QUEEN  &&  promo <= KNIGHT);
            val += 3 * ((promo) - 1);
        }
    }
    return val;
}


// Special-move tokens:
// Since king-move values 1-10 are taken for actual King moves, only
// 11-15 (and zero) are available for non-move information.

#define ENCODE_NAG          11
#define ENCODE_COMMENT      12
#define ENCODE_START_MARKER 13
#define ENCODE_END_MARKER   14
#define ENCODE_END_GAME     15

#define ENCODE_FIRST        11
#define ENCODE_LAST         15

// The end-game and end-variation tokens could be the same single token,
// but having two different tokens allows for detecting corruption, since
// a game must end with the end-game token.


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// decodeMove():
//      Decode a move from a bytebuffer. Assumes the byte val is an
//      actual move, not the value of a "special" (non-move) token.
//      This function needs to be passed the bytebuffer because some
//      moves (only Queen diagonal moves) are encoded in two bytes, so
//      it may be necessary to read the next byte as well.
//
errorT Game::decodeMove(ByteBuffer* buf, simpleMoveT* sm, byte val,
                        const Position* pos) {
	return game_storage::decodeEncodedMove(*buf, val, *pos, *sm);
}

template <typename DestT>
void encodeMove(const simpleMoveT& sm, DestT& dest) {
	byte multibyte = 0;
	byte val;
	switch (piece_Type(sm.movingPiece)) {
	case KING:
		ASSERT(sm.pieceNum == 0); // Kings MUST be piece Number zero.
		val = encodeKing(sm.from,
		                 sm.isCastle() ? sm.from + sm.isCastle() : sm.to);
		break;
	case QUEEN:
		val = encodeQueen(sm.from, sm.to, multibyte);
		break;
	case ROOK:
		val = encodeRook(sm.from, sm.to);
		break;
	case BISHOP:
		val = encodeBishop(sm.from, sm.to);
		break;
	case KNIGHT:
		val = encodeKnight(sm.from, sm.to);
		break;
	default:
		ASSERT(PAWN == piece_Type(sm.movingPiece));
		val = encodePawn(sm.from, sm.to, sm.promote);
	}
	ASSERT(sm.pieceNum <= 15 && val <= 15);
	const auto encoded = static_cast<byte>(val | (sm.pieceNum << 4));
	dest.emplace_back(encoded);
	if (multibyte) { // Diagonal Queen moves are stored using two bytes.
		dest.emplace_back(multibyte);
	}
}

simpleMoveT toSimpleMove(Position& position,
                         scid::core::MoveAction const& action) {
	simpleMoveT move = {};
	if (action.isNull()) {
		position.makeMove(action.from, action.to, PAWN, move);
		return move;
	}
	if (action.castling) {
		position.makeMove(action.from, action.from,
		                  action.to > action.from ? KING : QUEEN, move);
		return move;
	}

	const auto notation = action.longNotation();
	if (position.ReadCoordMove(&move, notation.data(), notation.size(),
	                           false) == OK) {
		return move;
	}

	move.from = action.from;
	move.to = action.to;
	move.promote = action.promotion;
	position.fillMove(move);
	return move;
}

scid::core::MoveAction toMoveAction(simpleMoveT const& move) {
	return {move.from, move.to, move.promote, move.isCastle() != 0};
}

struct MovelistStats {
	unsigned variations = 0;
	unsigned nags = 0;
};

template <typename DestT>
void encodeMovelistLine(bool markComments,
                        scid::core::MoveSequence const& line,
                        Position& position,
                        DestT& dest,
                        MovelistStats& stats) {
	for (auto const& coreMove : line.moves) {
		auto move = toSimpleMove(position, coreMove.action);
		encodeMove(move, dest);

		for (auto nag : coreMove.metadata.nags) {
			dest.emplace_back(ENCODE_NAG);
			dest.emplace_back(nag);
			++stats.nags;
		}
		if (markComments && !coreMove.metadata.comment.empty())
			dest.emplace_back(ENCODE_COMMENT);

		for (auto const& variation : coreMove.childVariations) {
			++stats.variations;
			dest.emplace_back(ENCODE_START_MARKER);
			if (markComments && !variation.initialComment.empty())
				dest.emplace_back(ENCODE_COMMENT);

			auto variationPosition = position;
			encodeMovelistLine(markComments, variation.line, variationPosition,
			                   dest, stats);
			dest.emplace_back(ENCODE_END_MARKER);
		}

		position.DoSimpleMove(move);
	}
}

/// Encode the moves, the nags, the comment mark and the variations.
template <typename DestT>
std::pair<unsigned, unsigned> encodeMovelist(
    bool markComments,
    scid::core::Game const& game,
    DestT& dest) {
	// Check if there is a pre-game comment
	if (markComments && !game.initialComment().empty())
		dest.emplace_back(ENCODE_COMMENT);

	MovelistStats stats;
	auto position =
	    game.startPosition() ? *game.startPosition() : Position::getStdStart();
	encodeMovelistLine(markComments, game.movetext().mainline, position, dest,
	                   stats);
	dest.emplace_back(ENCODE_END_GAME);
	return {stats.variations, stats.nags};
}

/// Decodes the game moves
errorT Game::decodeVariation(ByteBuffer& buf,
                             std::vector<scid::core::MovetextLocation>&
                                 comment_marks) {
	struct VariationFrame {
		Position resumePosition;
		simpleMoveT resumeMove;
	};

	scid::core::MovetextCursor cursor(coreGame_);
	Position position = coreGame_.startPosition()
	                        ? *coreGame_.startPosition()
	                        : Position::getStdStart();
	std::vector<VariationFrame> variationStack;
	std::optional<simpleMoveT> previousMove;
	simpleMoveT sm;
	int varDepth = 0;
	for (;;) {
		auto [err, val] = buf.nextMove(
		    varDepth, [&](auto) { return true; },
		    [&] {
			    // Mark this comment as needing to be read
			    comment_marks.push_back(cursor.location());
		    },
		    [&](auto newVariation) {
			    if (newVariation) {
				    if (!previousMove || !cursor.previous())
					    return false;
				    variationStack.push_back({position, *previousMove});
				    position.UndoSimpleMove(*previousMove);
				    ++varDepth;
				    return cursor.addVariation() != nullptr;
			    }

			    if (variationStack.empty() || !cursor.exitVariation() ||
			        !cursor.next()) {
				    return false;
			    }
			    auto frame = variationStack.back();
			    variationStack.pop_back();
			    position = frame.resumePosition;
			    previousMove = frame.resumeMove;
			    --varDepth;
			    return true;
		    },
		    [&](auto nag) {
			    auto move = cursor.previousMove();
			    if (!move)
				    return false;
			    move->metadata.nags.push_back(nag);
			    return true;
		    });
		if (err)
			return (err == ERROR_EndOfMoveList) ? OK : err;

		auto errMove = decodeMove(&buf, &sm, val, &position);
		if (errMove)
			return errMove;
		cursor.addMove(toMoveAction(sm));
		position.DoSimpleMove(sm);
		previousMove = sm;
	}
}

struct CommentStats {
	unsigned comments = 0;
	unsigned empty = 0;
};

void countComment(std::string_view comment, CommentStats& stats) {
	if (comment.empty()) {
		++stats.empty;
	} else {
		++stats.comments;
	}
}

void countComments(scid::core::MoveSequence const& line,
                   CommentStats& stats) {
	for (auto const& move : line.moves) {
		countComment(move.metadata.comment, stats);
		for (auto const& variation : move.childVariations) {
			countComment(variation.initialComment, stats);
			countComments(variation.line, stats);
		}
	}
}

// Return the number of comments and true if comment marks are useful.
auto countComments(scid::core::Movetext const& movetext) {
	CommentStats stats;
	countComment(movetext.initialComment, stats);
	countComments(movetext.mainline, stats);
	return std::make_pair(stats.comments, stats.comments < stats.empty);
}

/**
 * The Comments section is composed by null-terminated strings. The comments are
 * stored in the order in which they will appear in the PGN notation:
 * {C1} 1.d4 {C2} (1.b4 {C3} 1...e5 {C4} (1...Na6 {C5}) 2.e4 {C6})
 * ({C7} 1.g4 {C8}) 1...d5 {C9}
 */
template <typename DestT>
void encodeComment(bool markComments, std::string_view comment, DestT& dest) {
	if (!comment.empty() || !markComments) {
		dest.insert(dest.end(), comment.begin(), comment.end());
		dest.emplace_back(0);
	}
}

template <typename DestT>
void encodeComments(bool markComments,
                    scid::core::MoveSequence const& line,
                    DestT& dest) {
	for (auto const& move : line.moves) {
		encodeComment(markComments, move.metadata.comment, dest);
		for (auto const& variation : move.childVariations) {
			encodeComment(markComments, variation.initialComment, dest);
			encodeComments(markComments, variation.line, dest);
		}
	}
}

template <typename DestT>
void encodeComments(bool markComments,
                    scid::core::Movetext const& movetext,
                    DestT& dest) {
	encodeComment(markComments, movetext.initialComment, dest);
	encodeComments(markComments, movetext.mainline, dest);
}


static bool nextPgnLocation(scid::core::MovetextCursor& cursor) {
	if (cursor.previousMove() &&
	    !cursor.previousMove()->childVariations.empty() &&
	    cursor.previous()) {
		return cursor.enterVariation(0);
	}

	while (!cursor.next()) {
		if (cursor.variationDepth() == 0)
			return false;

		auto variationIndex = cursor.variationIndex();
		if (!cursor.exitVariation())
			return false;
		if (cursor.enterVariation(variationIndex + 1))
			return true;
		if (!cursor.next())
			return false;
	}
	return true;
}

static void setCoreCommentAt(scid::core::Game& game,
                             scid::core::MovetextLocation location,
                             std::string_view comment) {
	scid::core::MovetextCursor cursor(game);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	ASSERT(restored);

	if (cursor.isAtLineStart()) {
		if (cursor.variationDepth() == 0) {
			game.setInitialComment(comment);
		} else {
			[[maybe_unused]] const bool updated =
			    cursor.setCurrentVariationInitialComment(comment);
			ASSERT(updated);
		}
		return;
	}

	auto* move = cursor.previousMove();
	ASSERT(move);
	move->metadata.comment.assign(comment.begin(), comment.end());
}

// Decodes the comments from @e buf and stores them into the marked core
// movetext locations. If the number of comments is greater than the number of
// marked locations, comments are added sequentially in PGN order after the last
// marked location. This way it is possible to not add encode marks for games
// that are almost fully commented.
template <typename SourceT>
static errorT decodeComments(
    SourceT& buf,
    scid::core::Game& game,
    std::vector<scid::core::MovetextLocation>& comment_marks) {
	scid::core::MovetextLocation firstCommentLocation;
	bool hasFirstCommentLocation = true;
	if (!comment_marks.empty()) {
		for (auto location : comment_marks) {
			if (auto str = buf.GetTerminatedString())
				setCoreCommentAt(game, location, str);
			else
				return ERROR_Decode;
		}

		scid::core::MovetextCursor cursor(game);
		[[maybe_unused]] const bool restored =
		    cursor.restore(comment_marks.back());
		ASSERT(restored);
		hasFirstCommentLocation = nextPgnLocation(cursor);
		if (hasFirstCommentLocation)
			firstCommentLocation = cursor.location();
	}

	while (auto str = buf.GetTerminatedString()) {
		if (!hasFirstCommentLocation)
			return ERROR_Decode;

		setCoreCommentAt(game, firstCommentLocation, str);
		scid::core::MovetextCursor cursor(game);
		[[maybe_unused]] const bool restored =
		    cursor.restore(firstCommentLocation);
		ASSERT(restored);
		hasFirstCommentLocation = nextPgnLocation(cursor);
		if (hasFirstCommentLocation)
			firstCommentLocation = cursor.location();
	}
	return OK;
}


/// Calculate the game's main line information:
/// - home pawn delta information
/// - promotions flags
/// - number of half moves
/// - final material signature
/// - stored line code
std::pair<bool, bool> mainlineInfo(const Position* customStart,
                                   scid::core::MoveSequence const& mainline,
                                   IndexEntry& dest) {
	ushort nHalfMoves = 0;
	bool PromoFlag = false;
	bool UnderPromosFlag = false;
	unsigned hpCount = 0;
	byte hpVal[8] = {};
	Position pos = customStart ? *customStart : Position::getStdStart();
	std::vector<simpleMoveT> moves;
	moves.reserve(mainline.moves.size());

	auto hpOld = HPSIG_StdStart; // All 16 pawns are on their home squares.
	for (auto const& coreMove : mainline.moves) {
		auto move = toSimpleMove(pos, coreMove.action);
		++nHalfMoves;

		if (move.promote != EMPTY) {
			PromoFlag = true;
			if (piece_Type(move.promote) != QUEEN) {
				UnderPromosFlag = true;
			}
		}

		pos.DoSimpleMove(move);
		moves.push_back(move);
		if (!customStart) {
			const auto hpNew = pos.GetHPSig();
			if (unsigned changed = hpOld - hpNew) {
				hpOld = hpNew;
				byte idxMovedPawn = 0; // __builtin_ctz(changed)
				while (changed >>= 1) {
					++idxMovedPawn;
				}
				assert(idxMovedPawn <= 0x0F);
				if ((hpCount & 1) == 0) // There are only 16 pawns, so we can
					idxMovedPawn <<= 4; // store two pawn moves in every byte
				hpVal[hpCount++ / 2] |= idxMovedPawn;
			}
		}
	}

	byte storedCode = 0;
	if (!customStart) {
		storedCode = StoredLine::classify([&](auto begin, auto end) {
			if (std::distance(begin, end) > nHalfMoves)
				return false;

			auto gameMove = moves.begin();
			for (; begin != end; ++begin) {
				if (begin->isCastle()) {
					auto side = begin->getTo() > begin->getFrom() ? 2 : -2;
					if (gameMove->isCastle() != side)
						return false;

				} else if (gameMove->from != begin->getFrom() ||
				           gameMove->to != begin->getTo()) {
					return false;
				}

				++gameMove;
			}
			return true;
		});
	}

	dest.SetHomePawnData(static_cast<byte>(hpCount), hpVal);
	dest.SetPromotionsFlag(PromoFlag);
	dest.SetUnderPromoFlag(UnderPromosFlag);
	dest.SetNumHalfMoves(nHalfMoves);
	dest.SetFinalMatSig(matsig_Make(pos.GetMaterial()));
	dest.SetStoredLineCode(storedCode);

	return {PromoFlag, UnderPromosFlag};
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::encode(): Encode the game to a buffer for disk storage.
//      If passed a NON-null IndexEntry pointer, it will fill in the
//      following fields of that index entry, which are computed as
//      the game is encoded:
//       -  result, ecoCode, whiteElo, blackElo
//       -  promotion flag
//       -  nMoves: the number of halfmoves
//       -  finalMatSig: the material signature of the final position.
//       -  homePawnData: the home pawn change list.
//
std::pair<IndexEntry, TagRoster> Game::encode(std::vector<byte>& dest) const {
    // TODO [Game]: Keep IndexEntry/TagRoster projection in the database storage
    // boundary. Core metadata should be projected here, not stored in database
    // codec types.
    auto tags = TagRoster();
    tags.event = coreGame_.event().c_str();
    tags.site = coreGame_.site().c_str();
    tags.white = coreGame_.white().name.c_str();
    tags.black = coreGame_.black().name.c_str();
    tags.round = coreGame_.round().c_str();

    auto ie = IndexEntry();
    // Set the fields in the IndexEntry:
    auto const& header = coreGame_.header();
    ie.SetDate(header.event.date);
    ie.SetEventDate(header.event.eventDate);
    ie.SetResult(header.result);
    ie.SetEcoCode(scidup::eco::fromString(header.eco.c_str()));
    ie.SetWhiteElo(header.white.rating.value);
    ie.SetBlackElo(header.black.rating.value);
    ie.SetWhiteRatingType(header.white.rating.type);
    ie.SetBlackRatingType(header.black.rating.type);
    if (coreGame_.hasNonStandardStart()) {
        ie.SetStartFlag(true);
        if (coreGame_.startPosition()->isChess960()) {
            ie.setChess960();
        }
    } else {
        ie.SetStartFlag(false);
    }
    ie.SetFlag(IndexEntry::StrToFlagMask(scidFlags_), true);

    const auto [promo, underPromo] =
        mainlineInfo(coreGame_.startPosition(), coreGame_.movetext().mainline,
                     ie);

    // First, encode info not already stored in the index
    // This will be the non-STR (non-"seven tag roster") PGN tags.
    encodeTags(coreGame_.extraTags(), dest);

	    // Encode the promotion flags and the start position
	    char FEN[256];
	    encodeStartBoard(promo, underPromo,
	                     coreGame_.hasNonStandardStart(FEN, sizeof(FEN))
	                         ? FEN
	                         : nullptr,
	                     dest);

    auto [commentCount, markComments] = countComments(coreGame_.movetext());

    // Compatibility: SCID4 requires the markers
    markComments = true;

    // Now the movelist:
    auto [varCount, nagCount] = encodeMovelist(markComments, coreGame_, dest);

    // Now do the comments
    encodeComments(markComments, coreGame_.movetext(), dest);

    ie.SetCommentCount(commentCount);
    ie.SetVariationCount(varCount);
    ie.SetNagCount(nagCount);

    return {ie, tags};
}

errorT Game::decodeMovesOnly(ByteBuffer& buf) {
	clear();
	if (errorT err = buf.decodeTags([](auto, auto) {}))
		return err;

	const auto [errStartPos, fen] = buf.decodeStartBoard();
	if (errStartPos)
		return errStartPos;
	if (fen) {
		if (errorT err = setStartFen(fen))
			return err;
	}

	std::vector<scid::core::MovetextLocation> comment_marks;
	auto err = decodeVariation(buf, comment_marks);
	TEMP_syncLegacyMovetextFromCore();
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::decode():
//      Decodes a game from its on-disk representation in a bytebuffer.
//      Decodes all the information: comments, variations, non-standard
//      tags, etc.
//
errorT Game::decode(IndexEntry const& ie, TagRoster const& tags, ByteBuffer buf) {
    clear();
    loadStandardTags(ie, tags);

    errorT err = buf.decodeTags([&](const auto& tag, const auto& value) {
        auto& dest = coreGame_.findOrCreateTag(tag);
        dest.assign(value.begin(), value.end());
    });
    if (err)
        return err;

    const auto [err_startpos, fen] = buf.decodeStartBoard();
    if (err_startpos)
        return err_startpos;

    if (fen)
        err = setStartFen(fen);

    std::vector<scid::core::MovetextLocation> comment_marks;
    if (err == OK)
        err = decodeVariation(buf, comment_marks);

    if (err != OK) {
        TEMP_syncLegacyMovetextFromCore();
        return err;
    }

    if (err == OK)
        err = decodeComments(buf, coreGame_, comment_marks);

    if (err == OK)
        TEMP_syncLegacyMovetextFromCore();

    return err;
}

std::pair<IndexEntry, TagRoster> game_storage::encode(
    const Game& game, std::vector<byte>& dest) {
	return game.encode(dest);
}

void game_storage::loadStandardTags(Game& game, IndexEntry const& ie,
                                    TagRoster const& tags) {
	game.loadStandardTags(ie, tags);
}

errorT game_storage::decode(Game& game, IndexEntry const& ie,
                            TagRoster const& tags, ByteBuffer buf) {
	return game.decode(ie, tags, buf);
}

errorT game_storage::decodeMovesOnly(Game& game, ByteBuffer& buf) {
	return game.decodeMovesOnly(buf);
}

errorT game_storage::decodeEncodedMove(ByteBuffer& buf, byte val,
                                       const Position& pos, simpleMoveT& sm) {
	const colorT toMove = pos.GetToMove();
	const squareT from = pos.GetList(toMove)[val >> 4];
	if (from > H8)
		return ERROR_Decode;

	const auto ptype = piece_Type(pos.GetPiece(from));
	const auto [to, promo] = buf.decodeMove(toMove, ptype, from, val);
	if (to < 0 || to > 63)
		return ERROR_Decode;

	if (to == from) {
		if (promo == INVALID_PIECE)
			return ERROR_Decode;

		if (promo != PAWN && !pos.canCastle<false>(promo == KING))
			return ERROR_Decode;
	} else {
		if (to == pos.GetKingSquare(WHITE) || to == pos.GetKingSquare(BLACK))
			return ERROR_Decode;
	}
	pos.makeMove(from, to, promo, sm);
	return OK;
}

errorT game_storage::decodeMainlineMove(ByteBuffer& buf, const Position& pos,
                                        simpleMoveT& sm) {
	auto [err, val] = buf.nextLineMove();
	if (err)
		return err;

	return decodeEncodedMove(buf, val, pos, sm);
}

} // namespace scid::database

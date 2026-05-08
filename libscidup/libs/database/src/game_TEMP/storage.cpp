#include "scidup/database/game.h"

#include "scidup/database/bytebuf.h"
#include "scidup/database/common.h"
#include "scidup/database/game_TEMP/storage.h"
#include "scidup/database/indexentry.h"
#include "scidup/database/matsig.h"
#include "scidup/database/namebase.h"
#include "movetree.h"
#include "stored.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <type_traits>
#include <vector>

namespace scid::database {

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::LoadStandardTags():
//      Sets the standard tag values for this game, given an
//      index file entry and a namebase that stores the
//      player/site/event/round names.
// TODO [Game]: Keep IndexEntry/TagRoster hydration in the database storage
// boundary. The future core Game should not know about compact database
// metadata records.
//
void Game::LoadStandardTags(IndexEntry const& ie, TagRoster const& tags) {
    SetEventStr(tags.event);
    SetSiteStr(tags.site);
    SetWhiteStr(tags.white);
    SetBlackStr(tags.black);
    SetRoundStr(tags.round);
    SetDate(ie.GetDate());
    SetEventDate(ie.GetEventDate());
    SetWhiteElo(ie.GetWhiteElo());
    SetBlackElo(ie.GetBlackElo());
    SetWhiteRatingType(ie.GetWhiteRatingType());
    SetBlackRatingType(ie.GetBlackRatingType());
    SetResult(ie.GetResult());
    SetEco(ie.GetEcoCode());
    ie.GetFlagStr(ScidFlags, NULL);
    if (!ie.isChessStd())
        assignTagValue("Variant", "Chess960");
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
	const colorT toMove = pos->GetToMove();
	const squareT from = pos->GetList(toMove)[val >> 4];
	if (from > H8)
		return ERROR_Decode;

	const auto ptype = piece_Type(pos->GetPiece(from));
	const auto [to, promo] = buf->decodeMove(toMove, ptype, from, val);
	if (to < 0 || to > 63)
		return ERROR_Decode;

	if (to == from) {
		if (promo == INVALID_PIECE)
			return ERROR_Decode;

		if (promo != PAWN && !pos->canCastle<false>(promo == KING))
			return ERROR_Decode;
	} else {
		if (to == pos->GetKingSquare(WHITE) || to == pos->GetKingSquare(BLACK))
			return ERROR_Decode;
	}
	pos->makeMove(from, to, promo, *sm);
	return OK;
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

/// Encode the moves, the nags, the comment mark and the variations.
template <typename MoveT, typename DestT>
std::pair<unsigned, unsigned> encodeMovelist(bool mark_comments, const MoveT* m,
                                             DestT& dest) {
	ASSERT(m && m->startMarker());

	// Check if there is a pre-game comment
	if (mark_comments && !m->comment.empty())
		dest.emplace_back(ENCODE_COMMENT);

	unsigned n_vars = 0;
	unsigned n_nags = 0;
	while ((m = m->nextMoveInPGN())) {
		if (m->startMarker()) {
			++n_vars;
			dest.emplace_back(ENCODE_START_MARKER);
			if (mark_comments && !m->comment.empty())
				dest.emplace_back(ENCODE_COMMENT);

		} else if (m->endMarker()) {
			if (m->nextMoveInPGN())
				dest.emplace_back(ENCODE_END_MARKER);

		} else {
			encodeMove(m->moveData, dest);

			for (int i = 0, n = m->nagCount; i < n; ++i) {
				dest.emplace_back(ENCODE_NAG);
				dest.emplace_back(m->nags[i]);
				++n_nags;
			}
			if (mark_comments && !m->comment.empty())
				dest.emplace_back(ENCODE_COMMENT);
		}
	}
	dest.emplace_back(ENCODE_END_GAME);
	return {n_vars, n_nags};
}

/// Decodes the game moves
errorT Game::DecodeVariation(ByteBuffer& buf,
                             std::vector<moveT*>& comment_marks) {
	simpleMoveT sm;
	for (;;) {
		auto [err, val] = buf.nextMove(
		    this->VarDepth, [&](auto) { return true; },
		    [&] {
			    // Mark this comment as needing to be read
			    comment_marks.push_back(this->CurrentMove->prev);
		    },
		    [&](auto newVariation) {
			    if (newVariation)
				    return AddVariation() == OK;

			    return (MoveExitVariation() == OK && MoveForward() == OK);
		    },
		    [&](auto nag) {
			    return this->AddNag(nag) == OK;
		    });
		if (err)
			return (err == ERROR_EndOfMoveList) ? OK : err;

		auto errMove = decodeMove(&buf, &sm, val, currentPos());
		if (!errMove)
			errMove = AddMove(sm);
		if (errMove)
			return errMove;
	}
}

// Return the number of comments and true if comment marks are useful
template <typename MoveT> auto countComments(const MoveT* m) {
	unsigned n_comments = 0;
	unsigned n_empty = 0;
	for (; m; m = m->nextMoveInPGN()) {
		if (m->endMarker())
			continue;

		if (m->comment.empty()) {
			++n_empty;
		} else {
			++n_comments;
		}
	}
	return std::make_pair(n_comments, n_comments < n_empty);
}

/**
 * The Comments section is composed by null-terminated strings. The comments are
 * stored in the order in which they will appear in the PGN notation:
 * {C1} 1.d4 {C2} (1.b4 {C3} 1...e5 {C4} (1...Na6 {C5}) 2.e4 {C6})
 * ({C7} 1.g4 {C8}) 1...d5 {C9}
 */
template <typename MoveT, typename DestT>
void encodeComments(bool mark_comments, const MoveT* m, DestT& dest) {
	for (; m; m = m->nextMoveInPGN()) {
		if (m->endMarker())
			continue;

		if (!m->comment.empty() || !mark_comments) {
			const auto len = m->comment.size() + 1; // Include the null char
			const auto data = m->comment.c_str();
			dest.insert(dest.end(), data, data + len);
		}
	}
}


// Decodes the comments from @e buf and stores them into the marked moves.
// If the number of comments is greater than the number of marked moves, they
// are added sequentially after the last move that has a comment.
// This way it is possible to not add encode marks for games that are almost
// fully commented.
template <typename SourceT, typename MoveT>
static errorT decodeComments(SourceT& buf, MoveT* first_move,
                             std::vector<MoveT*>& comment_marks) {
    if (!comment_marks.empty()) {
        for (auto m : comment_marks) {
            if (auto str = buf.GetTerminatedString())
                m->comment = str;
            else
                return ERROR_Decode;
        }
        first_move = comment_marks.back()->nextMoveInPGN();
    }
    while (auto str = buf.GetTerminatedString()) {
        if (!first_move)
            return ERROR_Decode;

        first_move->comment = str;
        first_move = first_move->nextMoveInPGN();
    }
    return OK;
}


/// Calculate the game's main line information:
/// - home pawn delta information
/// - promotions flags
/// - number of half moves
/// - final material signature
/// - stored line code
template <typename MoveT>
std::pair<bool, bool> mainlineInfo(const Position* customStart,
                                   const MoveT* firstMove, IndexEntry& dest) {
	ushort nHalfMoves = 0;
	bool PromoFlag = false;
	bool UnderPromosFlag = false;
	unsigned hpCount = 0;
	byte hpVal[8] = {};
	Position pos = customStart ? *customStart : Position::getStdStart();

	auto hpOld = HPSIG_StdStart; // All 16 pawns are on their home squares.
	for (auto move = firstMove; !move->endMarker(); move = move->next) {
		++nHalfMoves;

		if (move->moveData.promote != EMPTY) {
			PromoFlag = true;
			if (piece_Type(move->moveData.promote) != QUEEN) {
				UnderPromosFlag = true;
			}
		}

		pos.DoSimpleMove(move->moveData);
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

			const moveT* gameMove = firstMove;
			for (; begin != end; ++begin) {
				if (begin->isCastle()) {
					auto side = begin->getTo() > begin->getFrom() ? 2 : -2;
					if (gameMove->moveData.isCastle() != side)
						return false;

				} else if (gameMove->moveData.from != begin->getFrom() ||
				           gameMove->moveData.to != begin->getTo()) {
					return false;
				}

				gameMove = gameMove->next;
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
// Game::Encode(): Encode the game to a buffer for disk storage.
//      If passed a NON-null IndexEntry pointer, it will fill in the
//      following fields of that index entry, which are computed as
//      the game is encoded:
//       -  result, ecoCode, whiteElo, blackElo
//       -  promotion flag
//       -  nMoves: the number of halfmoves
//       -  finalMatSig: the material signature of the final position.
//       -  homePawnData: the home pawn change list.
//
std::pair<IndexEntry, TagRoster> Game::Encode(std::vector<byte>& dest) const {
    // TODO [Game]: Keep IndexEntry/TagRoster projection in the database storage
    // boundary. Core metadata should be projected here, not stored in database
    // codec types.
    auto tags = TagRoster();
    tags.event = GetEventStr();
    tags.site = GetSiteStr();
    tags.white = GetWhiteStr();
    tags.black = GetBlackStr();
    tags.round = GetRoundStr();

    auto ie = IndexEntry();
    // Set the fields in the IndexEntry:
    ie.SetDate(header_.event.date);
    ie.SetEventDate(header_.event.eventDate);
    ie.SetResult(header_.result);
    ie.SetEcoCode(EcoCode);
    ie.SetWhiteElo(header_.white.rating.value);
    ie.SetBlackElo(header_.black.rating.value);
    ie.SetWhiteRatingType(header_.white.rating.type);
    ie.SetBlackRatingType(header_.black.rating.type);
    if (HasNonStandardStart()) {
        ie.SetStartFlag(true);
        if (StartPos->isChess960()) {
            ie.setChess960();
        }
    } else {
        ie.SetStartFlag(false);
    }
    ie.SetFlag(IndexEntry::StrToFlagMask(ScidFlags), true);

    const auto [promo, underPromo] = mainlineInfo(StartPos.get(),
                                                  FirstMove->next, ie);

    // First, encode info not already stored in the index
    // This will be the non-STR (non-"seven tag roster") PGN tags.
    encodeTags(GetExtraTags(), dest);

	    // Encode the promotion flags and the start position
	    char FEN[256];
	    encodeStartBoard(promo, underPromo,
	                     HasNonStandardStart(FEN, sizeof(FEN)) ? FEN : nullptr, dest);

    auto [commentCount, markComments] = countComments(FirstMove);

    // Compatibility: SCID4 requires the markers
    markComments = true;

    // Now the movelist:
    auto [varCount, nagCount] = encodeMovelist(markComments, FirstMove, dest);

    // Now do the comments
    encodeComments(markComments, FirstMove, dest);

    ie.SetCommentCount(commentCount);
    ie.SetVariationCount(varCount);
    ie.SetNagCount(nagCount);

    return {ie, tags};
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::DecodeNextMove():
//      Decodes one more mainline move of the game from the bytebuffer.
//      Used in searches for speed, since it is usually possible to
//      determine if a game matches the search criteria without decoding
//      all of it.
//      If the game flag KeepDecodedMoves is true, the move decodes is
//      added normally. If it is false, only the current position is
//      updated and the list of moves is not updated -- this is done
//      in searches for speed.
//      Returns OK if a move was found, or ERROR_EndOfMoveList if all the
//      moves have been decoded. Returns ERROR_Game if some corruption was
//      detected.
//
errorT Game::DecodeNextMove(ByteBuffer* buf, simpleMoveT& sm) {
	ASSERT(buf != NULL);

	auto [err, val] = buf->nextLineMove();
	if (err)
		return err;

	return decodeMove(buf, &sm, val, currentPos());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::DecodeStart():
//      Decodes the starting information from the game's on-disk
//      representation in the bytebuffer. After this is called,
//      DecodeNextMove() can be called to decode each successive
//      mainline move.
//
errorT Game::DecodeSkipTags(ByteBuffer* buf) {
    ASSERT(buf != NULL);

    Clear();
    errorT err = buf->decodeTags([](auto, auto) {});
    if (err != OK)
        return err;

    const auto [err_startpos, fen] = buf->decodeStartBoard();
    if (err_startpos)
        return err_startpos;

    if (fen)
        return SetStartFen(fen);

    return OK;
}

errorT Game::DecodeMovesOnly(ByteBuffer& buf) {
	if (errorT err = DecodeSkipTags(&buf))
		return err;

	std::vector<moveT*> comment_marks;
	return DecodeVariation(buf, comment_marks);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::Decode():
//      Decodes a game from its on-disk representation in a bytebuffer.
//      Decodes all the information: comments, variations, non-standard
//      tags, etc.
//
errorT Game::Decode(IndexEntry const& ie, TagRoster const& tags, ByteBuffer buf) {
    Clear();
    LoadStandardTags(ie, tags);

    errorT err = buf.decodeTags([&](const auto& tag, const auto& value) {
        assignTagValue(tag, value);
    });
    if (err)
        return err;

    const auto [err_startpos, fen] = buf.decodeStartBoard();
    if (err_startpos)
        return err_startpos;

    if (fen)
        err = SetStartFen(fen);

    std::vector<moveT*> comment_marks;
    if (err == OK)
        err = DecodeVariation(buf, comment_marks);

    if (err == OK)
        err = decodeComments(buf, FirstMove, comment_marks);

    return err;
}

std::pair<IndexEntry, TagRoster> game_storage::encode(
    const Game& game, std::vector<byte>& dest) {
	return game.Encode(dest);
}

void game_storage::loadStandardTags(Game& game, IndexEntry const& ie,
                                    TagRoster const& tags) {
	game.LoadStandardTags(ie, tags);
}

errorT game_storage::decode(Game& game, IndexEntry const& ie,
                            TagRoster const& tags, ByteBuffer buf) {
	return game.Decode(ie, tags, buf);
}

errorT game_storage::decodeMovesOnly(Game& game, ByteBuffer& buf) {
	return game.DecodeMovesOnly(buf);
}

errorT game_storage::decodeSkipTags(Game& game, ByteBuffer* buf) {
	return game.DecodeSkipTags(buf);
}

errorT game_storage::decodeNextMove(Game& game, ByteBuffer* buf,
                                    simpleMoveT& sm) {
	return game.DecodeNextMove(buf, sm);
}

} // namespace scid::database

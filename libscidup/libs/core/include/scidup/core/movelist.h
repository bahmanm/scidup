/** @file
 * Generated move records and fixed-capacity move lists.
 */
#pragma once

#include "scidup/core/board.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

//////////////////////////////////////////////////////////////////////
//  MoveList:  Constants

namespace scid::core {

const uint  MAX_LEGAL_MOVES = 256;  // max. length of the moves list


///////////////////////////////////////////////////////////////////////////
//  MoveList:  Data Structures

// TODO [MoveAction]: This is a position-resolved reversible action record.
// It semantically belongs with Position, but currently lives here because
// MoveList stores generated actions and Position includes MoveList.
struct MoveAction
{
    squareT  from;
    squareT  to;
    pieceT   promote; // EMPTY if not a promotion, type (no color) otherwise
    pieceT   movingPiece : 7;
    pieceT   castling : 1;
    byte     pieceNum;
    byte     capturedNum;
    pieceT   capturedPiece;
    squareT  capturedSquare; // ONLY different to "to" field if this capture
                            //    is an en passant capture.
    byte     castleFlags;    // pre-move information
    squareT  epSquare;       // pre-move information
    ushort   oldHalfMoveClock;

	bool isNullMove() const {
		return from == to && from != NULL_SQUARE &&
		       piece_Type(movingPiece) == KING;
	}

	/// Returns:
	///  +2 for king side castle
	///  -2 for queen side castle
	///  0 (false) if it is not a castle moves.
	int isCastle() const {
		if (castling)
			return to > from ? 2 : -2;

		return 0;
	}

	/// Converts the move to UCI coordinate notation.
	/// @return a pointer one past the last char written.
	template <typename OutputIt> OutputIt toLongNotation(OutputIt dest) const {
		if (from == to) {
			// UCI standard for null move
			*dest++ = '0';
			*dest++ = '0';
			*dest++ = '0';
			*dest++ = '0';
		} else {
			*dest++ = square_FyleChar(from);
			*dest++ = square_RankChar(from);
			*dest++ = square_FyleChar(to);
			*dest++ = square_RankChar(to);
			if (promote != EMPTY) {
				constexpr const char promoChars[] = "  qrbn ";
				*dest++ = promoChars[piece_Type(promote)];
			}
		}
		return dest;
	}
};

struct ScoredMove : public MoveAction {
	std::int32_t score; // used for alpha/beta ordering.

	bool operator<(const ScoredMove& b) const {
		// Highest score first
		return score > b.score;
	}
};

// typedef std::vector<MoveAction> MoveList;
class MoveList {
	uint ListSize = 0;
	ScoredMove Moves[MAX_LEGAL_MOVES];

public:
	typedef ScoredMove* iterator;
	iterator begin() { return Moves; };
	iterator end() { return Moves + ListSize; }
	uint Size() { return ListSize; }
	void Clear() { ListSize = 0; }
	ScoredMove& emplace_back() {
		assert(ListSize < MAX_LEGAL_MOVES);
		ScoredMove& sm = Moves[ListSize++];
		sm = ScoredMove();
		return sm;
	}
	void resize(std::size_t count) {
		assert(count <= MAX_LEGAL_MOVES);
		ListSize = static_cast<uint>(count);
	}
	void push_back(const ScoredMove& sm) {
		assert(ListSize < MAX_LEGAL_MOVES);
		Moves[ListSize++] = sm;
	}
	ScoredMove* Get(std::size_t index) {
		assert(index < ListSize);
		return &(Moves[index]);
	}
};


} // namespace scid::core

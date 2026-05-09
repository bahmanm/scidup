#include "scidup/database/game.h"

#include "movetree.h"
#include "scidup/core/position.h"

#include <algorithm>
#include <utility>

namespace scid::database {

Game::~Game() = default;

constexpr int MOVE_CHUNKSIZE = 128;

Game::Game() {
	Clear();
}

const scid::core::Game& Game::coreGame() const {
	return coreGame_;
}

// TODO [Game]: Keep start-position lifecycle on the future core Game, but keep
// PGN/UCI/export projections of the starting position outside the aggregate.
bool Game::HasNonStandardStart(char* outFEN, size_t outFENLen) const {
	return coreGame_.hasNonStandardStart(outFEN, outFENLen);
}

errorT Game::SetStartFen(const char* fenStr) {
	Position pos;
	if (auto err = pos.ReadFromFEN(fenStr))
		return err;

	SetStartPos(pos);
	return OK;
}

void Game::SetStartPos(Position const& pos) {
	ClearMoves();
	coreGame_.setStartPosition(pos);
	*CurrentPos = pos;
}

// TODO [Game]: Keep Scid flags in database/app compatibility, not in the core
// metadata model.
void Game::SetScidFlags(const char* s, size_t len) {
	constexpr size_t size = sizeof(ScidFlags) / sizeof(*ScidFlags);
	std::fill_n(ScidFlags, size, 0);
	std::copy_n(s, std::min(size - 1, len), ScidFlags);
}

ushort Game::GetNumHalfMoves() {
	return NumHalfMoves;
}

moveT* Game::allocMove() {
	// TODO [Game]: Hide legacy moveT chunk allocation behind the future core
	// move-tree representation.
	if (moveChunkUsed_ == MOVE_CHUNKSIZE) {
		moveChunks_.emplace_front(new moveT[MOVE_CHUNKSIZE]);
		moveChunkUsed_ = 0;
	}
	return moveChunks_.front().get() + moveChunkUsed_++;
}

moveT* Game::NewMove(markerT marker) {
	moveT* res = allocMove();
	res->clear();
	res->marker = marker;
	return res;
}

Game::Game(const Game& obj) {
	// TODO [Game]: Revisit clone/copy after GameCursor exists. This currently
	// copies aggregate data and restores PGN-order cursor location in one
	// compatibility operation.
	coreGame_ = obj.coreGame_;
	EcoCode = obj.EcoCode;
	std::copy_n(obj.ScidFlags, sizeof(obj.ScidFlags), ScidFlags);

	NumHalfMoves = obj.NumHalfMoves;

	moveChunkUsed_ = MOVE_CHUNKSIZE;
	FirstMove = obj.FirstMove->cloneLine(nullptr,
	                                     [this]() { return allocMove(); });

	MoveToLocationInPGN(obj.GetLocationInPGN());
}

Game* Game::clone() {
	return new Game(*this);
}

void Game::strip(bool variations, bool comments, bool NAGs) {
	// TODO [Game]: Decide whether stripping belongs on core Game or a future
	// GameEditor. It mutates move-tree structure and Move.metadata together.
	while (variations && MoveExitVariation() == OK) {
	}

	for (auto& chunk : moveChunks_) {
		moveT* move = chunk.get();
		moveT* end = (chunk == moveChunks_.front()) ? move + moveChunkUsed_
		                                            : move + MOVE_CHUNKSIZE;
		for (; move != end; ++move) {
			if (variations) {
				move->numVariations = 0;
				move->varChild = nullptr;
			}
			if (comments)
				move->comment.clear();

			if (NAGs) {
				move->nagCount = 0;
				std::fill_n(move->nags, sizeof(move->nags), 0);
			}
		}
	}
	TEMP_syncCoreMovetext();
}

void Game::ClearMoves() {
	// TODO [Game]: Move move-tree reset and cursor initialization into the
	// future core Game implementation once move storage is no longer legacy
	// moveT chunks.
	if (moveChunks_.empty()) {
		moveChunkUsed_ = MOVE_CHUNKSIZE;
	} else {
		moveChunks_.erase_after(moveChunks_.begin(), moveChunks_.end());
		moveChunkUsed_ = 0;
	}
	coreGame_.clearStartPosition();
	coreGame_.clearMovetext();
	CurrentPos->StdStart();

	FirstMove = NewMove(START_MARKER);
	CurrentMove = NewMove(END_MARKER);
	FirstMove->setNext(CurrentMove);

	VarDepth = 0;
	NumHalfMoves = 0;
}

void Game::Clear() {
	// TODO [Game]: Split this reset across core Game metadata/moves and
	// database compatibility flags.
	coreGame_.clear();
	EcoCode = 0;
	ScidFlags[0] = 0;

	ClearMoves();
}

} // namespace scid::database

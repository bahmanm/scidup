#include "scidup/database/game.h"

#include "movetree.h"
#include "scidup/core/position.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

namespace scid::database {

Game::~Game() = default;

constexpr int MOVE_CHUNKSIZE = 128;

Game::Game() {
	Clear();
}

// TODO [Game]: Keep start-position lifecycle on the future core Game, but keep
// PGN/UCI/export projections of the starting position outside the aggregate.
bool Game::HasNonStandardStart(char* outFEN, size_t outFENLen) const {
	if (!StartPos)
		return false;
	if (outFEN && outFENLen)
		StartPos->PrintFEN(outFEN, outFENLen);
	return true;
}

long long Game::initialPlyCounter() const {
	return StartPos ? StartPos->GetPlyCounter() : 0;
}

errorT Game::SetStartFen(const char* fenStr) {
	auto pos = std::make_unique<Position>();
	if (auto err = pos->ReadFromFEN(fenStr))
		return err;

	SetStartPos(std::move(pos));
	return OK;
}

void Game::SetStartPos(Position const& pos) {
	return SetStartPos(std::make_unique<Position>(pos));
}

void Game::SetStartPos(std::unique_ptr<Position> pos) {
	ClearMoves();
	StartPos = std::move(pos);
	*CurrentPos = *StartPos;
}

// TODO [Game]: Keep Scid flags in database/app compatibility, not in the core
// metadata model.
void Game::SetScidFlags(const char* s, size_t len) {
	constexpr size_t size = sizeof(ScidFlags) / sizeof(*ScidFlags);
	std::fill_n(ScidFlags, size, 0);
	std::copy_n(s, std::min(size - 1, len), ScidFlags);
}

void Game::SetScidFlags(const char* s) {
	SetScidFlags(s, std::strlen(s));
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
	// copies aggregate data, legacy export state, and restores PGN-order cursor
	// location in one compatibility operation.
	header_ = obj.header_;
	EcoCode = obj.EcoCode;
	std::copy_n(obj.ScidFlags, sizeof(obj.ScidFlags), ScidFlags);

	if (obj.StartPos)
		StartPos = std::make_unique<Position>(*obj.StartPos);

	NumHalfMoves = obj.NumHalfMoves;
	PgnStyle = obj.PgnStyle;
	PgnFormat = obj.PgnFormat;
	HtmlStyle = obj.HtmlStyle;

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
	StartPos = nullptr;
	CurrentPos->StdStart();

	FirstMove = NewMove(START_MARKER);
	CurrentMove = NewMove(END_MARKER);
	FirstMove->setNext(CurrentMove);

	VarDepth = 0;
	NumHalfMoves = 0;
}

void Game::Clear() {
	// TODO [Game]: Split this reset across core Game metadata/moves, database
	// compatibility flags, and legacy export defaults.
	header_ = {};
	EcoCode = 0;
	ScidFlags[0] = 0;

	PgnStyle = PGN_STYLE_TAGS | PGN_STYLE_VARS | PGN_STYLE_COMMENTS;
	PgnFormat = PGN_FORMAT_Plain;
	HtmlStyle = 0;

	ClearMoves();
}

} // namespace scid::database

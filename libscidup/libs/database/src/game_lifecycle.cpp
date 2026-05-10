#include "scidup/database/game.h"

#include "movetree.h"
#include "scidup/core/position.h"

#include <algorithm>
#include <utility>

namespace scid::database {

Game::~Game() = default;

constexpr int MOVE_CHUNKSIZE = 128;

Game::Game() {
	clear();
}

scid::core::Game& Game::coreGame() {
	return coreGame_;
}

const scid::core::Game& Game::coreGame() const {
	return coreGame_;
}

scid::core::MovetextLocation Game::coreLocation() const {
	return coreLocation_;
}

errorT Game::setStartFen(const char* fenStr) {
	Position pos;
	if (auto err = pos.ReadFromFEN(fenStr))
		return err;

	setStartPosition(pos);
	return OK;
}

void Game::setStartPosition(Position const& pos) {
	clearMoves();
	coreGame_.setStartPosition(pos);
	*currentPos_ = pos;
}

// TODO [Game]: Keep Scid flags in database/app compatibility, not in the core
// metadata model.
void Game::setScidFlags(const char* s, size_t len) {
	constexpr size_t size = sizeof(scidFlags_) / sizeof(*scidFlags_);
	std::fill_n(scidFlags_, size, 0);
	std::copy_n(s, std::min(size - 1, len), scidFlags_);
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

moveT* Game::newMove(markerT marker) {
	moveT* res = allocMove();
	res->clear();
	res->marker = marker;
	return res;
}

Game::Game(const Game& obj) {
	coreGame_ = obj.coreGame_;
	coreLocation_ = obj.coreLocation_;
	std::copy_n(obj.scidFlags_, sizeof(obj.scidFlags_), scidFlags_);
	moveChunkUsed_ = MOVE_CHUNKSIZE;
	TEMP_syncLegacyMovetextFromCore();
}

Game* Game::clone() {
	return new Game(*this);
}

void Game::strip(bool variations, bool comments, bool NAGs) {
	// TODO [Game]: Decide whether stripping belongs on core Game or a future
	// MovetextCursor. It mutates move-tree structure and Move.metadata together.
	while (variations && exitVariation() == OK) {
	}

	coreGame_.stripMovetext(variations, comments, NAGs);
	TEMP_syncLegacyMovetextFromCore();
}

void Game::clearMoves() {
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
	currentPos_->StdStart();

	firstMove_ = newMove(START_MARKER);
	currentMove_ = newMove(END_MARKER);
	firstMove_->setNext(currentMove_);
	coreLocation_ = scid::core::MovetextLocation();

	varDepth_ = 0;
	numHalfMoves_ = 0;
}

void Game::clear() {
	// TODO [Game]: Split this reset across core Game metadata/moves and
	// database compatibility flags.
	coreGame_.clear();
	scidFlags_[0] = 0;

	clearMoves();
}

} // namespace scid::database

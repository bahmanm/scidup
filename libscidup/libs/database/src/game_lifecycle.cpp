#include "scidup/database/game.h"

#include "scidup/core/position.h"

#include <algorithm>
#include <utility>

namespace scid::database {

Game::~Game() = default;

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
	coreGame_.clearMovetext();
	coreGame_.setStartPosition(pos);
	coreLocation_ = scid::core::MovetextLocation();
	*currentPos_ = pos;
}

// TODO [Game]: Keep Scid flags in database/app compatibility, not in the core
// metadata model.
void Game::setScidFlags(const char* s, size_t len) {
	constexpr size_t size = sizeof(scidFlags_) / sizeof(*scidFlags_);
	std::fill_n(scidFlags_, size, 0);
	std::copy_n(s, std::min(size - 1, len), scidFlags_);
}

const char* Game::scidFlags() const {
	return scidFlags_;
}

Game::Game(const Game& obj) {
	coreGame_ = obj.coreGame_;
	coreLocation_ = obj.coreLocation_;
	std::copy_n(obj.scidFlags_, sizeof(obj.scidFlags_), scidFlags_);
	*currentPos_ = *obj.currentPos_;
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
}

void Game::clear() {
	// TODO [Game]: Split this reset across core Game metadata/moves and
	// database compatibility flags.
	coreGame_.clear();
	scidFlags_[0] = 0;
	coreLocation_ = scid::core::MovetextLocation();
	currentPos_->StdStart();
}

} // namespace scid::database

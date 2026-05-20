#include "scidup/database/game.h"

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

char* Game::scidFlagsData() {
	return scidFlags_;
}

size_t Game::scidFlagsCapacity() const {
	return sizeof(scidFlags_);
}

Game::Game(const Game& obj) {
	coreGame_ = obj.coreGame_;
	std::copy_n(obj.scidFlags_, sizeof(obj.scidFlags_), scidFlags_);
}

Game* Game::clone() {
	return new Game(*this);
}

void Game::clear() {
	// TODO [Game]: Split this reset across core Game metadata/moves and
	// database compatibility flags.
	coreGame_.clear();
	scidFlags_[0] = 0;
}

} // namespace scid::database

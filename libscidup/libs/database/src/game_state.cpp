#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"
#include "scidup/core/notation.h"

namespace scid::database {

Game::GameSavedPos Game::currentLocation() const {
	return {coreLocation_};
}

void Game::restoreLocation(const GameSavedPos& savedPos) {
	[[maybe_unused]] const bool restored = setCoreLocation(savedPos.coreLocation);
	ASSERT(restored);
}

bool Game::setCoreLocation(scid::core::MovetextLocation location) {
	scid::core::GameCursor cursor(coreGame_);
	if (!cursor.restore(location))
		return false;

	coreLocation_ = location;
	return true;
}

bool Game::refreshCurrentPosCache() const {
	scid::core::GameCursor cursor(coreGame_);
	if (!cursor.restore(coreLocation_))
		return false;

	auto position = cursor.currentPosition();
	if (!position)
		return false;

	*currentPos_ = *position;
	return true;
}

Position* Game::currentPos() {
	[[maybe_unused]] const bool refreshed = refreshCurrentPosCache();
	ASSERT(refreshed);
	return currentPos_.get();
}

const Position* Game::currentPos() const {
	[[maybe_unused]] const bool refreshed = refreshCurrentPosCache();
	ASSERT(refreshed);
	return currentPos_.get();
}

simpleMoveT* Game::currentMove() {
	scid::core::GameCursor cursor(coreGame_);
	[[maybe_unused]] const bool restored = cursor.restore(coreLocation_);
	ASSERT(restored);
	const auto* move = cursor.nextMove();
	if (!move)
		return nullptr;

	auto position = cursor.currentPosition();
	if (!position)
		return nullptr;

	auto simpleMove = scid::core::notation::toSimpleMove(*position, move->action);
	if (!simpleMove)
		return nullptr;

	currentMoveCache_ = *simpleMove;
	return &currentMoveCache_;
}

} // namespace scid::database

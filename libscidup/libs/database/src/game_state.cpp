#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"

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

} // namespace scid::database

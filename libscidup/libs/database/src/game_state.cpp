#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"

namespace scid::database {

void Game::restoreLocation(scid::core::MovetextLocation location) {
	[[maybe_unused]] const bool restored = setCoreLocation(location);
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

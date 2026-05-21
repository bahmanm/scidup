#include "scidup/core/pgn/traversal.h"

#include <cassert>

namespace scid::core::pgn {

bool nextLocation(GameCursor& cursor) {
	if (cursor.previousMove() &&
	    !cursor.previousMove()->childVariations.empty() &&
	    cursor.previous()) {
		return cursor.enterVariation(0);
	}

	while (!cursor.next()) {
		if (cursor.variationDepth() == 0)
			return false;

		auto variationIndex = cursor.variationIndex();
		if (!cursor.exitVariation())
			return false;
		if (cursor.enterVariation(variationIndex + 1))
			return true;
		[[maybe_unused]] const bool skippedParent = cursor.next();
		assert(skippedParent);
	}
	return true;
}

bool seekLocation(GameCursor& cursor, unsigned location) {
	cursor.toStart();
	for (unsigned loc = 1; loc < location; ++loc) {
		if (!nextLocation(cursor))
			return false;
	}
	return true;
}

unsigned locationOf(const GameCursor& cursor) {
	auto currentLocation = cursor.location();
	auto scan = cursor;
	scan.toStart();

	unsigned result = 1;
	if (scan.location() == currentLocation)
		return result;

	while (nextLocation(scan)) {
		++result;
		if (scan.location() == currentLocation)
			return result;
	}
	assert(false);
	return result;
}

unsigned offsetOf(const GameCursor& cursor) {
	auto scan = cursor;
	while (scan.isAtVariationStart() && scan.variationDepth() != 0) {
		[[maybe_unused]] const bool exited = scan.exitVariation();
		assert(exited);
	}
	return locationOf(scan);
}

} // namespace scid::core::pgn

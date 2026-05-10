#include "scidup/database/game.h"

#include "scidup/database/common.h"
#include "scidup/database/game_TEMP/notation.h"
#include "scidup/core/movetext_cursor.h"
#include "scidup/core/notation.h"
#include "scidup/core/position.h"
#include "movetext_projection.h"
#include "movetree.h"

#include <cstdio>

namespace scid::database {

namespace {

bool syncCoreMoveSan(scid::core::Game& coreGame,
                     scid::core::MovetextLocation location,
                     const moveT* legacyMove,
                     bool nextMove) {
	if (!legacyMove || legacyMove->startMarker() || legacyMove->endMarker())
		return false;

	scid::core::MovetextCursor cursor(coreGame);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	ASSERT(restored);

	return nextMove ? cursor.setNextMoveSan(legacyMove->san)
	                : cursor.setPreviousMoveSan(legacyMove->san);
}

void cacheLegacySan(char* dest, const std::string& san) {
	std::snprintf(dest, SAN_STRING_SIZE, "%s", san.c_str());
}

} // namespace

std::string game_notation::currentPositionUci(const Game& game) {
	return scid::core::notation::currentPositionUci(game.coreGame_,
	                                                game.coreLocation_);
}


std::string game_notation::nextSan(Game& game) {
	// TODO [Game]: Move SAN generation/caching to notation helpers and
	// Move.metadata once Move owns SAN and GameCursor owns the current position.
	ASSERT(!game.currentMove_->endMarker() || *game.currentMove_->san == '\0');

	if (!game.currentMove_->endMarker() && *game.currentMove_->san == '\0') {
		const auto san =
		    scid::core::notation::nextSan(game.coreGame_, game.coreLocation_);
		if (!san.empty()) {
			cacheLegacySan(game.currentMove_->san, san);
		} else {
			game.currentPos_->MakeSANString(
			    &game.currentMove_->moveData, game.currentMove_->san,
			    game.currentMove_->next->endMarker() ? SAN_MATETEST
			                                         : SAN_CHECKTEST);
		}
		if (!syncCoreMoveSan(game.coreGame_, game.coreLocation_,
		                     game.currentMove_, true))
			TEMP_movetext::syncCoreMovetextAndLocation(
			    game.coreGame_, game.firstMove_, game.currentMove_,
			    game.coreLocation_);
	}
	return game.currentMove_->san;
}

std::string game_notation::previousSan(Game& game) {
    // TODO [Game]: Move SAN generation/caching to notation helpers and
    // Move.metadata once Move owns SAN and GameCursor owns the current position.
    moveT * m = game.currentMove_->prev;
    if (m->startMarker()  ||  m->endMarker()) {
        return {};
    }
    if (m->san[0] == 0) {
		const auto san =
		    scid::core::notation::previousSan(game.coreGame_, game.coreLocation_);
		if (!san.empty()) {
			cacheLegacySan(m->san, san);
		} else {
			game.previous();
			game.currentPos_->MakeSANString(&(m->moveData), m->san,
			                                SAN_MATETEST);
			game.next();
		}
		if (!syncCoreMoveSan(game.coreGame_, game.coreLocation_, m, false))
			TEMP_movetext::syncCoreMovetextAndLocation(
			    game.coreGame_, game.firstMove_, game.currentMove_,
			    game.coreLocation_);
    }
    return m->san;
}

std::string game_notation::previousMoveUci(const Game& game) {
	return scid::core::notation::previousMoveUci(game.coreGame_,
	                                             game.coreLocation_);
}

std::string game_notation::nextMoveUci(const Game& game) {
	return scid::core::notation::nextMoveUci(game.coreGame_,
	                                         game.coreLocation_);
}

} // namespace scid::database

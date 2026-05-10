#include "scidup/database/game.h"

#include "scidup/database/common.h"
#include "scidup/database/game_TEMP/notation.h"
#include "scidup/core/dstring.h"
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

} // namespace

std::string game_notation::currentPositionUci(const Game& game) {
	return scid::core::notation::currentPositionUci(game.coreGame_,
	                                                game.coreLocation_);
}


errorT game_notation::writePartialMoveList(Game& game, DString& out,
                                           uint plyCount) {
    // TODO [Game]: Rebuild this UI compatibility helper on GameCursor plus SAN
    // notation once cursor traversal is no longer stored directly on Game.
    // First, copy the relevant data so we can leave the game state
    // unaltered:
    auto location = game.currentLocation();

    game.toStart();
    char temp [80];
    for (uint i=0; i < plyCount; i++) {
        if (game.isAtEnd()) {
            break;
        }
        const auto* pos = game.currentPos();
        if (i != 0) { out.Append (" "); }
        if (i == 0  ||  pos->GetToMove() == WHITE) {
            std::snprintf(temp, sizeof(temp), "%d%s", pos->GetFullMoveCount(),
                     (pos->GetToMove() == WHITE ? "." : "..."));
            out.Append (temp);
        }
        // add one space for indenting to work out right
        out.Append (" ");
        out.Append (game_notation::nextSan(game).c_str());
        game.next();
    }

    // Now reconstruct the original game state:
    game.restoreLocation(location);
    return OK;
}

std::string game_notation::nextSan(Game& game) {
	// TODO [Game]: Move SAN generation/caching to notation helpers and
	// Move.metadata once Move owns SAN and GameCursor owns the current position.
	ASSERT(!game.currentMove_->endMarker() || *game.currentMove_->san == '\0');

	if (!game.currentMove_->endMarker() && *game.currentMove_->san == '\0') {
		game.currentPos_->MakeSANString(
		    &game.currentMove_->moveData, game.currentMove_->san,
		    game.currentMove_->next->endMarker() ? SAN_MATETEST : SAN_CHECKTEST);
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
        game.previous();
		game.currentPos_->MakeSANString (&(m->moveData), m->san, SAN_MATETEST);
        game.next();
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

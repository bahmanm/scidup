#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"
#include "movetree.h"

namespace scid::database {
namespace {

scid::core::GameCursor currentCoreCursor(
    const scid::core::Game& coreGame,
    scid::core::MovetextLocation location) {
	scid::core::GameCursor cursor(coreGame);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	ASSERT(restored);
	return cursor;
}

} // namespace

Game::GameSavedPos Game::currentLocation() const {
	return {*currentPos_, currentMove_, varDepth_, coreLocation_};
}

void Game::restoreLocation(const GameSavedPos& savedPos) {
	*currentPos_ = savedPos.pos;
	currentMove_ = savedPos.move;
	varDepth_ = savedPos.varDepth;
	coreLocation_ = savedPos.coreLocation;
}

Position* Game::currentPos() {
	return currentPos_.get();
}

const Position* Game::currentPos() const {
	return currentPos_.get();
}

simpleMoveT* Game::currentMove() {
	// TODO [Game]: Keep this legacy pointer until callers stop depending on
	// position-filled simpleMoveT fields that are not present in MoveAction.
	return currentMove_->endMarker() ? nullptr : &currentMove_->moveData;
}

ushort Game::currentPly() const {
	return static_cast<ushort>(
	    currentCoreCursor(coreGame_, coreLocation_).ply());
}

bool Game::isAtVariationStart() const {
	return currentCoreCursor(coreGame_, coreLocation_).isAtVariationStart();
}

bool Game::isAtVariationEnd() const {
	return currentCoreCursor(coreGame_, coreLocation_).isAtVariationEnd();
}

bool Game::isAtStart() const {
	return currentCoreCursor(coreGame_, coreLocation_).isAtGameStart();
}

bool Game::isAtEnd() const {
	return currentCoreCursor(coreGame_, coreLocation_).isAtGameEnd();
}

bool Game::isAtEmptyVariation() const {
	return currentCoreCursor(coreGame_, coreLocation_).isAtEmptyVariation();
}

} // namespace scid::database

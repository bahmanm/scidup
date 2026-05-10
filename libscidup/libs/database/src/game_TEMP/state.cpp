#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"
#include "movetree.h"

namespace scid::database {
namespace {

bool moveCoreCursorToCurrentLocation(scid::core::MovetextLocation location,
                                     scid::core::GameCursor& cursor) {
	return cursor.restore(location);
}

} // namespace

Game::GameSavedPos Game::currentLocation() const {
	return {*currentPos_, currentMove_, varDepth_, coreLocation_};
}

void Game::restoreLocation(const GameSavedPos& savedPos) {
	*currentPos_ = savedPos.pos;
	currentMove_ = savedPos.move;
	varDepth_ = savedPos.varDepth;
	if (savedPos.coreLocation)
		coreLocation_ = *savedPos.coreLocation;
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
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return static_cast<ushort>(cursor.ply());

	auto ply = currentPos_->GetPlyCounter();
	auto startPos = coreGame_.startPosition();
	return startPos ? ply - startPos->GetPlyCounter() : ply;
}

uint Game::variationCount() const {
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return static_cast<uint>(cursor.variationCount());

	return currentMove_->numVariations;
}

uint Game::variationLevel() const {
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return static_cast<uint>(cursor.variationDepth());

	return varDepth_;
}

uint Game::variationNumber() const {
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return static_cast<uint>(cursor.variationIndex());

	if (varDepth_ != 0) {
		uint varNumber = 0;
		auto moves = currentMove_->getParent();
		for (auto parent = moves.first; parent; varNumber++) {
			parent = parent->varChild;
			if (parent == moves.second)
				return varNumber;
		}
	}
	return 0;
}

bool Game::isAtVariationStart() const {
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return cursor.isAtVariationStart();

	return currentMove_->prev->startMarker();
}

bool Game::isAtVariationEnd() const {
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return cursor.isAtVariationEnd();

	return currentMove_->endMarker();
}

bool Game::isAtStart() const {
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return cursor.isAtGameStart();

	return varDepth_ == 0 && isAtVariationStart();
}

bool Game::isAtEnd() const {
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return cursor.isAtGameEnd();

	return varDepth_ == 0 && isAtVariationEnd();
}

bool Game::isAtEmptyVariation() const {
	scid::core::GameCursor cursor(coreGame_);
	if (moveCoreCursorToCurrentLocation(coreLocation_, cursor))
		return cursor.isAtEmptyVariation();

	return varDepth_ != 0 && isAtVariationStart() && isAtVariationEnd();
}

} // namespace scid::database

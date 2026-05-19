#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"
#include "scidup/core/notation.h"

namespace scid::database {
namespace {

simpleMoveT toCompatibilityMove(Position& position,
                                scid::core::MoveAction const& action) {
	simpleMoveT move = {};
	if (action.isNull()) {
		position.makeMove(action.from, action.to, PAWN, move);
		return move;
	}
	if (action.castling) {
		position.makeMove(action.from, action.from,
		                  action.to > action.from ? KING : QUEEN, move);
		return move;
	}
	position.makeMove(action.from, action.to, action.promotion, move);
	return move;
}

} // namespace

Game::GameSavedPos Game::currentLocation() const {
	return {coreLocation_};
}

void Game::restoreLocation(const GameSavedPos& savedPos) {
	[[maybe_unused]] const bool restored = setCoreLocation(savedPos.coreLocation);
	ASSERT(restored);
}

bool Game::setCoreLocation(scid::core::MovetextLocation location) {
	Position position;
	if (auto startPosition = coreGame_.startPosition()) {
		position = *startPosition;
	} else {
		position.StdStart();
	}

	scid::core::GameCursor cursor(coreGame_);
	if (!cursor.restore(location))
		return false;

	for (const auto* move : cursor.movesToCursor()) {
		auto simpleMove =
		    scid::core::notation::toSimpleMove(position, move->action);
		if (!simpleMove)
			return false;
		position.DoSimpleMove(*simpleMove);
	}

	coreLocation_ = location;
	*currentPos_ = position;
	return true;
}

Position* Game::currentPos() {
	return currentPos_.get();
}

const Position* Game::currentPos() const {
	return currentPos_.get();
}

simpleMoveT* Game::currentMove() {
	scid::core::GameCursor cursor(coreGame_);
	[[maybe_unused]] const bool restored = cursor.restore(coreLocation_);
	ASSERT(restored);
	const auto* move = cursor.nextMove();
	if (!move)
		return nullptr;

	currentMoveCache_ = toCompatibilityMove(*currentPos_, move->action);
	return &currentMoveCache_;
}

} // namespace scid::database

#include "scidup/database/game.h"

#include "movetree.h"

namespace scid::database {

Game::GameSavedPos Game::currentLocation() const {
	return GameSavedPos{*CurrentPos, currentMove_, varDepth_};
}

void Game::restoreLocation(const GameSavedPos& savedPos) {
	*CurrentPos = savedPos.pos;
	currentMove_ = savedPos.move;
	varDepth_ = savedPos.varDepth;
}

Position* Game::currentPos() {
	return CurrentPos.get();
}

const Position* Game::currentPos() const {
	return CurrentPos.get();
}

simpleMoveT* Game::currentMove() {
	return currentMove_->endMarker() ? nullptr : &currentMove_->moveData;
}

ushort Game::currentPly() const {
	auto ply = CurrentPos->GetPlyCounter();
	auto startPos = coreGame_.startPosition();
	return startPos ? ply - startPos->GetPlyCounter() : ply;
}

uint Game::variationCount() const {
	return currentMove_->numVariations;
}

uint Game::variationLevel() const {
	return varDepth_;
}

uint Game::variationNumber() const {
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
	return currentMove_->prev->startMarker();
}

bool Game::isAtVariationEnd() const {
	return currentMove_->endMarker();
}

bool Game::isAtStart() const {
	return varDepth_ == 0 && isAtVariationStart();
}

bool Game::isAtEnd() const {
	return varDepth_ == 0 && isAtVariationEnd();
}

bool Game::isAtEmptyVariation() const {
	return varDepth_ != 0 && isAtVariationStart() && isAtVariationEnd();
}

} // namespace scid::database

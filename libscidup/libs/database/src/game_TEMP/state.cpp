#include "scidup/database/game.h"

#include "movetree.h"

namespace scid::database {

Game::GameSavedPos Game::currentLocation() const {
	return GameSavedPos{*CurrentPos, CurrentMove, VarDepth};
}

void Game::restoreLocation(const GameSavedPos& savedPos) {
	*CurrentPos = savedPos.pos;
	CurrentMove = savedPos.move;
	VarDepth = savedPos.varDepth;
}

Position* Game::currentPos() {
	return CurrentPos.get();
}

const Position* Game::currentPos() const {
	return CurrentPos.get();
}

simpleMoveT* Game::currentMove() {
	return CurrentMove->endMarker() ? nullptr : &CurrentMove->moveData;
}

ushort Game::currentPly() const {
	auto ply = CurrentPos->GetPlyCounter();
	auto startPos = coreGame_.startPosition();
	return startPos ? ply - startPos->GetPlyCounter() : ply;
}

uint Game::variationCount() const {
	return CurrentMove->numVariations;
}

uint Game::variationLevel() const {
	return VarDepth;
}

uint Game::variationNumber() const {
	if (VarDepth != 0) {
		uint varNumber = 0;
		auto moves = CurrentMove->getParent();
		for (auto parent = moves.first; parent; varNumber++) {
			parent = parent->varChild;
			if (parent == moves.second)
				return varNumber;
		}
	}
	return 0;
}

bool Game::AtVarStart() const {
	return CurrentMove->prev->startMarker();
}

bool Game::AtVarEnd() const {
	return CurrentMove->endMarker();
}

bool Game::AtStart() const {
	return VarDepth == 0 && AtVarStart();
}

bool Game::AtEnd() const {
	return VarDepth == 0 && AtVarEnd();
}

bool Game::AtEmptyVar() const {
	return VarDepth != 0 && AtVarStart() && AtVarEnd();
}

} // namespace scid::database

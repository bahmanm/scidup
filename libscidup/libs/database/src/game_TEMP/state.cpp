#include "scidup/database/game.h"
#include "scidup/database/game_TEMP/state.h"

#include "movetree.h"

namespace scid::database {

uint game_state::currentPly(const Game& game) {
	auto ply = game.currentPos()->GetPlyCounter();
	auto startPos = game.coreGame().startPosition();
	return startPos ? ply - startPos->GetPlyCounter() : ply;
}

uint game_state::mainlineHalfMoveCount(const Game& game) {
	return static_cast<uint>(game.coreGame().movetext().mainline.moves.size());
}

Game::GameSavedPos Game::currentLocation() const {
	return GameSavedPos{*CurrentPos, CurrentMove, VarDepth};
}

void Game::restoreLocation(const GameSavedPos& savedPos) {
	*CurrentPos = savedPos.pos;
	CurrentMove = savedPos.move;
	VarDepth = savedPos.varDepth;
}

const Position* Game::currentPos() const {
	return CurrentPos.get();
}

Position* Game::GetCurrentPos() {
	return CurrentPos.get();
}

simpleMoveT* Game::GetCurrentMove() {
	return CurrentMove->endMarker() ? nullptr : &CurrentMove->moveData;
}

ushort Game::GetCurrentPly() const {
	auto ply = CurrentPos->GetPlyCounter();
	auto startPos = coreGame_.startPosition();
	return startPos ? ply - startPos->GetPlyCounter() : ply;
}

uint Game::GetNumVariations() const {
	return CurrentMove->numVariations;
}

uint Game::GetVarLevel() const {
	return VarDepth;
}

uint Game::GetVarNumber() const {
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

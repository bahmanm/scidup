#include "scidup/database/game.h"

#include "movetree.h"

namespace scid::database {

simpleMoveT* Game::GetCurrentMove() {
	return CurrentMove->endMarker() ? nullptr : &CurrentMove->moveData;
}

uint Game::GetNumVariations() const {
	return CurrentMove->numVariations;
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

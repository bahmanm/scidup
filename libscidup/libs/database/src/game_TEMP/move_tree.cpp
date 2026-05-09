#include "scidup/database/game.h"

#include "scidup/database/common.h"
#include "scidup/core/position.h"
#include "movetree.h"

#include <memory>
#include <utility>

namespace scid::database {

///////////////////////////////////////////////////////////////////////////
// A "location" in the game is represented by a position (Game::CurrentPos), the
// next move to be played (Game::CurrentMove) and the number of parent variations
// (Game::varDepth_). Since CurrentMove is the next move to be played, some
// invariants must hold: it is never nullptr and it never points to a
// START_MARKER (it will point to a END_MARKER if there are no more moves). This
// also means that CurrentMove->prev is always valid: it will point to a
// previous move or to a START_MARKER.
// The following functions modify ONLY the current location of the game.

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move current position forward one move.
// Also update all the necessary fields in the simpleMove structure
// (CurrentMove->moveData) so it can be undone.
//
errorT Game::next(void) {
	if (CurrentMove->endMarker())
		return ERROR_EndOfMoveList;

	CurrentPos->DoSimpleMove(CurrentMove->moveData);
	CurrentMove = CurrentMove->next;

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::previous():
//      Backup one move.
//
errorT Game::previous(void) {
	if (CurrentMove->prev->startMarker())
		return ERROR_StartOfMoveList;

	CurrentMove = CurrentMove->prev;
	CurrentPos->UndoSimpleMove(CurrentMove->moveData);

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::enterVariation():
//      Move into a subvariation. Variations are numbered from 0.
errorT Game::enterVariation(uint varNumber) {
	for (auto subVar = CurrentMove; subVar->varChild; --varNumber) {
		subVar = subVar->varChild;
		if (varNumber == 0) {
			CurrentMove = subVar->next; // skip the START_MARKER
			++varDepth_;

			// Invariants
			ASSERT(CurrentMove && CurrentMove->prev);
			ASSERT(!CurrentMove->startMarker());
			return OK;
		}
	}
	return ERROR_NoVariation; // there is no such variation
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::exitVariation():
//      Move out of a variation, to the parent.
//
errorT Game::exitVariation(void) {
	if (varDepth_ == 0) // not in a variation!
		return ERROR_NoVariation;

	// Algorithm: go back previous moves as far as possible, then
	// go up to the parent of the variation.
	while (previous() == OK) {
	}
	CurrentMove = CurrentMove->getParent().first;
	--varDepth_;

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move to the beginning of the game.
//
void Game::toStart() {
	if (auto startPos = coreGame_.startPosition()) {
		*CurrentPos = *startPos;
	} else {
		CurrentPos->StdStart();
	}
	varDepth_ = 0;
	CurrentMove = FirstMove->next;

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
}

void Game::toEnd() {
	toStart();
	while (next() == OK) {
	}
}

void Game::toPly(int hmNumber) {
	toStart();
	for (int i = 0; i < hmNumber; ++i)
		next();
}

// TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
// instead of keeping it on the generic Game cursor surface.
errorT Game::nextPgn() {
	if (CurrentMove->prev->varChild && previous() == OK)
		return enterVariation(0);

	while (next() != OK) {
		if (varDepth_ == 0)
			return ERROR_EndOfMoveList;

		auto varnum = variationNumber();
		exitVariation();
		if (enterVariation(varnum + 1) == OK)
			return OK;

		next();
	}
	return OK;
}

// TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
// instead of keeping it on the generic Game cursor surface.
errorT Game::toPgnLocation(unsigned stopLocation) {
	toStart();
	for (unsigned loc = 1; loc < stopLocation; ++loc) {
		errorT err = nextPgn();
		if (err != OK)
			return err;
	}
	return OK;
}

// TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
// instead of keeping it on the generic Game cursor surface.
unsigned Game::pgnLocation() const {
	unsigned res = 1;
	const moveT* last_move = CurrentMove->prev;
	const moveT* move = FirstMove;
	for (; move != last_move; move = move->nextMoveInPGN()) {
		if (!move->endMarker())
			++res;
	}
	return res;
}

// TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
// instead of keeping it on the generic Game cursor surface.
unsigned Game::pgnOffset() const {
	unsigned res = 1;
	const moveT* last_move = CurrentMove->getPrevMove();
	if (last_move) {
		const moveT* move = FirstMove;
		for (; move != last_move; move = move->nextMoveInPGN()) {
			if (!move->endMarker())
				++res;
		}
	}
	return res;
}

///////////////////////////////////////////////////////////////////////////
// The following functions modify the moves graph in order to add or delete
// moves. Promoting variations also modifies the moves graph.

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::addMove():
//      Add a move at current position and do it.
//
errorT Game::addMove(simpleMoveT const& sm) {
	// We must be at the end of a game/variation to add a move:
	if (!CurrentMove->endMarker())
		truncate();

	CurrentMove->setNext(newMove(END_MARKER));
	CurrentMove->marker = NO_MARKER;
	CurrentMove->moveData = sm;
	if (varDepth_ == 0)
		++numHalfMoves_;

	auto err = next();
	if (err == OK)
		TEMP_syncCoreMovetext();
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::addVariation():
//      Add a variation for the current move.
//      Also moves into the variation.
errorT Game::addVariation() {
	auto err = previous();
	if (err != OK)
		return err;

	auto newVar = newMove(START_MARKER);
	newVar->setNext(newMove(END_MARKER));
	CurrentMove->appendChild(newVar);

	// Move into variation
	CurrentMove = newVar->next;
	++varDepth_;

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
	TEMP_syncCoreMovetext();
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::promoteVariationToFirst():
// Promotes the current variation to first variation.
errorT Game::promoteVariationToFirst() {
	auto parent = CurrentMove->getParent();
	auto root = parent.first;
	if (!root)
		return ERROR_NoVariation;

	root->detachChild(parent.second);
	root->insertChild(parent.second, 0);
	TEMP_syncCoreMovetext();
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::promoteVariationToMainline():
//    Like promoteVariationToFirst, but promotes the variation to the main line,
//    demoting the main line to be the first variation.
errorT Game::promoteVariationToMainline() {
	auto parent = CurrentMove->getParent();
	auto root = parent.first;
	if (!root)
		return ERROR_NoVariation;
	if (parent.second->next->endMarker()) // Do not promote empty variations
		return OK;

	// Make the current variation the first variation
	root->detachChild(parent.second);
	root->insertChild(parent.second, 0);

	// Swap the mainline with the current variation
	root->swapLine(*parent.second->next);

	ASSERT(varDepth_);
	if (--varDepth_ == 0) { // Recalculate mainline half-move count.
		const auto count_moves = [](auto move) {
			int res = 0;
			while (!move->endMarker()) {
				++res;
				move = move->next;
			}
			return res;
		};
		ASSERT(FirstMove->startMarker() && FirstMove->next);
		numHalfMoves_ = count_moves(FirstMove->next);
	}

	TEMP_syncCoreMovetext();
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::deleteVariation():
//      Deletes a variation. Variations are numbered from 0.
//      Note that for speed and simplicity, freed moves are not
//      added to the free list. This means that repeatedly adding and
//      deleting variations will waste memory until the game is cleared.
//
errorT Game::deleteVariation() {
	auto parent = CurrentMove->getParent();
	auto root = parent.first;
	if (!root || exitVariation() != OK)
		return ERROR_NoVariation;

	root->detachChild(parent.second);
	TEMP_syncCoreMovetext();
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::truncate():
//      Remove moves from the current move to the end of this line.
//      For speed and simplicity, moves and comments are not freed.
//      So repeatedly adding moves and truncating a game will waste
//      memory until the game is cleared.
void Game::truncate() {
	if (CurrentMove->endMarker())
		return;

	auto endMove = newMove(END_MARKER);
	CurrentMove->prev->setNext(endMove);

	CurrentMove = endMove;
	if (varDepth_ == 0)
		numHalfMoves_ = currentPly();
	TEMP_syncCoreMovetext();

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::truncateStart():
//      Remove all moves leading to the current position.
void Game::truncateStart() {
	    // It is necessary to rebuild the current position using ReadFromFEN()
	    // because the order of pieces is important when encoding to SCIDv4 format.
	    char tempStr[256];
	    CurrentPos->PrintFEN(tempStr, sizeof(tempStr));
	    auto pos = std::make_unique<Position>();
	    if (pos->ReadFromFEN(tempStr) != OK)
	        return;

    if (varDepth_ != 0 && promoteVariationToMainline() != OK)
		return;

    numHalfMoves_ -= currentPly();
    coreGame_.setStartPosition(*pos);
    *CurrentPos = *pos;
    FirstMove->setNext(CurrentMove);

    // Do all the moves to update moveData.pieceNum to the new start position.
    do {
        if (!CurrentMove->startMarker() && !CurrentMove->endMarker()) {
            CurrentPos->fillMove(CurrentMove->moveData);
        }
    } while (nextPgn() == OK);
    toStart();
    TEMP_syncCoreMovetext();
}

} // namespace scid::database

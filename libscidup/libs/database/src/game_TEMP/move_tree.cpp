#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_cursor.h"
#include "scidup/core/position.h"
#include "scidup/database/common.h"
#include "movetext_cursor_bridge.h"
#include "movetext_projection.h"
#include "movetree.h"

#include <memory>
#include <utility>

namespace scid::database {

namespace {

scid::core::MoveAction toCoreMoveAction(simpleMoveT const& sm) {
	return {sm.from, sm.to, sm.promote};
}

bool restoreCoreCursor(scid::core::MovetextCursor& cursor,
                       scid::core::MovetextLocation location) {
	return cursor.restore(location);
}

bool restoreCoreCursor(scid::core::GameCursor& cursor,
                       scid::core::MovetextLocation location) {
	return cursor.restore(location);
}

void TEMP_syncCoreMovetextAndLocation(scid::core::Game& coreGame,
                                      const moveT* firstMove,
                                      const moveT* currentMove,
                                      scid::core::MovetextLocation& location) {
	TEMP_movetext::syncCoreMovetext(coreGame, firstMove);
	scid::core::MovetextCursor cursor(coreGame);
	if (TEMP_movetext::moveCursorToLegacyLocation(cursor, firstMove,
	                                                currentMove))
		location = cursor.location();
}

} // namespace

///////////////////////////////////////////////////////////////////////////
// A "location" in the game is represented by a position (Game::currentPos_), the
// next move to be played (Game::currentMove_) and the number of parent variations
// (Game::varDepth_). Since currentMove_ is the next move to be played, some
// invariants must hold: it is never nullptr and it never points to a
// START_MARKER (it will point to a END_MARKER if there are no more moves). This
// also means that currentMove_->prev is always valid: it will point to a
// previous move or to a START_MARKER.
// The following functions modify ONLY the current location of the game.

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move current position forward one move.
// Also update all the necessary fields in the simpleMove structure
// (currentMove_->moveData) so it can be undone.
//
errorT Game::next(void) {
	if (currentMove_->endMarker())
		return ERROR_EndOfMoveList;

	scid::core::GameCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	currentPos_->DoSimpleMove(currentMove_->moveData);
	currentMove_ = currentMove_->next;
	if (coreCursorReady && coreCursor.next())
		coreLocation_ = coreCursor.location();

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::previous():
//      Backup one move.
//
errorT Game::previous(void) {
	if (currentMove_->prev->startMarker())
		return ERROR_StartOfMoveList;

	scid::core::GameCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	currentMove_ = currentMove_->prev;
	currentPos_->UndoSimpleMove(currentMove_->moveData);
	if (coreCursorReady && coreCursor.previous())
		coreLocation_ = coreCursor.location();

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::enterVariation():
//      Move into a subvariation. Variations are numbered from 0.
errorT Game::enterVariation(uint varNumber) {
	scid::core::GameCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);
	const auto requestedVariation = varNumber;

	for (auto subVar = currentMove_; subVar->varChild; --varNumber) {
		subVar = subVar->varChild;
		if (varNumber == 0) {
			currentMove_ = subVar->next; // skip the START_MARKER
			++varDepth_;
			if (coreCursorReady &&
			    coreCursor.enterVariation(requestedVariation))
				coreLocation_ = coreCursor.location();

			// Invariants
			ASSERT(currentMove_ && currentMove_->prev);
			ASSERT(!currentMove_->startMarker());
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

	scid::core::GameCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	// Algorithm: go back previous moves as far as possible, then
	// go up to the parent of the variation.
	while (previous() == OK) {
	}
	currentMove_ = currentMove_->getParent().first;
	--varDepth_;
	if (coreCursorReady && coreCursor.exitVariation())
		coreLocation_ = coreCursor.location();

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move to the beginning of the game.
//
void Game::toStart() {
	if (auto startPos = coreGame_.startPosition()) {
		*currentPos_ = *startPos;
	} else {
		currentPos_->StdStart();
	}
	varDepth_ = 0;
	currentMove_ = firstMove_->next;
	scid::core::GameCursor coreCursor(coreGame_);
	coreCursor.toStart();
	coreLocation_ = coreCursor.location();

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
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
	if (currentMove_->prev->varChild && previous() == OK)
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
	const moveT* last_move = currentMove_->prev;
	const moveT* move = firstMove_;
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
	const moveT* last_move = currentMove_->getPrevMove();
	if (last_move) {
		const moveT* move = firstMove_;
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
	if (!currentMove_->endMarker())
		truncate();

	scid::core::MovetextCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	currentMove_->setNext(newMove(END_MARKER));
	currentMove_->marker = NO_MARKER;
	currentMove_->moveData = sm;
	const bool mainlineMove = varDepth_ == 0;
	if (mainlineMove)
		++numHalfMoves_;

	auto err = next();
	if (err == OK) {
		if (coreCursorReady) {
			coreCursor.addMove(toCoreMoveAction(sm));
			coreLocation_ = coreCursor.location();
		} else {
			// TODO [Game]: Remove this fallback once coreLocation_ is the authoritative
			// cursor state and legacy moveT mapping is gone.
			TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
			                                 currentMove_, coreLocation_);
		}
	}
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

	scid::core::MovetextCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	auto newVar = newMove(START_MARKER);
	newVar->setNext(newMove(END_MARKER));
	currentMove_->appendChild(newVar);

	// Move into variation
	currentMove_ = newVar->next;
	++varDepth_;

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
	if (coreCursorReady) {
		if (coreCursor.addVariation())
			coreLocation_ = coreCursor.location();
		else
			TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
			                                 currentMove_, coreLocation_);
	} else {
		// TODO [Game]: Remove this fallback once coreLocation_ is the authoritative
		// cursor state and legacy moveT mapping is gone.
		TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
		                                 currentMove_, coreLocation_);
	}
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::promoteVariationToFirst():
// Promotes the current variation to first variation.
errorT Game::promoteVariationToFirst() {
	scid::core::MovetextCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	auto parent = currentMove_->getParent();
	auto root = parent.first;
	if (!root)
		return ERROR_NoVariation;

	root->detachChild(parent.second);
	root->insertChild(parent.second, 0);
	if (coreCursorReady) {
		if (coreCursor.promoteVariationToFirst())
			coreLocation_ = coreCursor.location();
		else
			TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
			                                 currentMove_, coreLocation_);
	} else {
		// TODO [Game]: Remove this fallback once coreLocation_ is the authoritative
		// cursor state and legacy moveT mapping is gone.
		TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
		                                 currentMove_, coreLocation_);
	}
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::promoteVariationToMainline():
//    Like promoteVariationToFirst, but promotes the variation to the main line,
//    demoting the main line to be the first variation.
errorT Game::promoteVariationToMainline() {
	scid::core::MovetextCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	auto parent = currentMove_->getParent();
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
		ASSERT(firstMove_->startMarker() && firstMove_->next);
		numHalfMoves_ = count_moves(firstMove_->next);
	}

	if (coreCursorReady) {
		if (coreCursor.promoteVariationToMainline())
			coreLocation_ = coreCursor.location();
		else
			TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
			                                 currentMove_, coreLocation_);
	} else {
		// TODO [Game]: Remove this fallback once coreLocation_ is the authoritative
		// cursor state and legacy moveT mapping is gone.
		TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
		                                 currentMove_, coreLocation_);
	}
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
	scid::core::MovetextCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	auto parent = currentMove_->getParent();
	auto root = parent.first;
	if (!root || exitVariation() != OK)
		return ERROR_NoVariation;

	root->detachChild(parent.second);
	if (coreCursorReady) {
		if (coreCursor.deleteVariation())
			coreLocation_ = coreCursor.location();
		else
			TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
			                                 currentMove_, coreLocation_);
	} else {
		// TODO [Game]: Remove this fallback once coreLocation_ is the authoritative
		// cursor state and legacy moveT mapping is gone.
		TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
		                                 currentMove_, coreLocation_);
	}
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::truncate():
//      Remove moves from the current move to the end of this line.
//      For speed and simplicity, moves and comments are not freed.
//      So repeatedly adding moves and truncating a game will waste
//      memory until the game is cleared.
void Game::truncate() {
	if (currentMove_->endMarker())
		return;

	scid::core::MovetextCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

	auto endMove = newMove(END_MARKER);
	currentMove_->prev->setNext(endMove);

	currentMove_ = endMove;
	if (varDepth_ == 0)
		numHalfMoves_ = currentPly();
	if (coreCursorReady) {
		coreCursor.truncate();
		coreLocation_ = coreCursor.location();
	} else {
		// TODO [Game]: Remove this fallback once coreLocation_ is the authoritative
		// cursor state and legacy moveT mapping is gone.
		TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
		                                 currentMove_, coreLocation_);
	}

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::truncateStart():
//      Remove all moves leading to the current position.
void Game::truncateStart() {
	    // It is necessary to rebuild the current position using ReadFromFEN()
	    // because the order of pieces is important when encoding to SCIDv4 format.
	    char tempStr[256];
	    currentPos_->PrintFEN(tempStr, sizeof(tempStr));
	    auto pos = std::make_unique<Position>();
	    if (pos->ReadFromFEN(tempStr) != OK)
	        return;

    if (varDepth_ != 0 && promoteVariationToMainline() != OK)
		return;

	scid::core::MovetextCursor coreCursor(coreGame_);
	const bool coreCursorReady = restoreCoreCursor(coreCursor, coreLocation_);

    numHalfMoves_ -= currentPly();
    coreGame_.setStartPosition(*pos);
    *currentPos_ = *pos;
    firstMove_->setNext(currentMove_);

    // Do all the moves to update moveData.pieceNum to the new start position.
    do {
        if (!currentMove_->startMarker() && !currentMove_->endMarker()) {
            currentPos_->fillMove(currentMove_->moveData);
        }
    } while (nextPgn() == OK);
    toStart();
	if (coreCursorReady) {
		coreCursor.truncateBeforeCursor();
		coreLocation_ = coreCursor.location();
	} else {
		// TODO [Game]: Remove this fallback once coreLocation_ is the authoritative
		// cursor state and legacy moveT mapping is gone.
		TEMP_syncCoreMovetextAndLocation(coreGame_, firstMove_,
		                                 currentMove_, coreLocation_);
	}
}

} // namespace scid::database

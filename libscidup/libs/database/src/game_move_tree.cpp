#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_cursor.h"
#include "scidup/core/position.h"
#include "scidup/database/common.h"
#include "movetree.h"

#include <utility>

namespace scid::database {

namespace {

scid::core::MoveAction toCoreMoveAction(simpleMoveT const& sm) {
	return {sm.from, sm.to, sm.promote, sm.isCastle() != 0};
}

void restoreCoreCursor(scid::core::MovetextCursor& cursor,
                       scid::core::MovetextLocation location) {
	[[maybe_unused]] const bool restored = cursor.restore(location);
	ASSERT(restored);
}

void restoreCoreCursor(scid::core::GameCursor& cursor,
                       scid::core::MovetextLocation location) {
	[[maybe_unused]] const bool restored = cursor.restore(location);
	ASSERT(restored);
}

bool nextPgnCore(scid::core::GameCursor& cursor) {
	if (cursor.previousMove() &&
	    !cursor.previousMove()->childVariations.empty() &&
	    cursor.previous()) {
		return cursor.enterVariation(0);
	}

	while (!cursor.next()) {
		if (cursor.variationDepth() == 0)
			return false;

		auto variationIndex = cursor.variationIndex();
		if (!cursor.exitVariation())
			return false;
		if (cursor.enterVariation(variationIndex + 1))
			return true;
		[[maybe_unused]] const bool skippedParent = cursor.next();
		ASSERT(skippedParent);
	}
	return true;
}

unsigned pgnLocationOf(const scid::core::Game& game,
                       scid::core::MovetextLocation location) {
	scid::core::GameCursor cursor(game);
	unsigned result = 1;
	if (cursor.location() == location)
		return result;

	while (nextPgnCore(cursor)) {
		++result;
		if (cursor.location() == location)
			return result;
	}
	ASSERT(false);
	return result;
}

unsigned pgnOffsetOf(const scid::core::Game& game,
                     scid::core::MovetextLocation location) {
	scid::core::GameCursor cursor(game);
	restoreCoreCursor(cursor, location);
	while (cursor.isAtVariationStart() && cursor.variationDepth() != 0) {
		[[maybe_unused]] const bool exited = cursor.exitVariation();
		ASSERT(exited);
	}
	return pgnLocationOf(game, cursor.location());
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
	scid::core::GameCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.next())
		return ERROR_EndOfMoveList;
	coreLocation_ = coreCursor.location();

	ASSERT(!currentMove_->endMarker());
	currentPos_->DoSimpleMove(currentMove_->moveData);
	currentMove_ = currentMove_->next;

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
	scid::core::GameCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.previous())
		return ERROR_StartOfMoveList;
	coreLocation_ = coreCursor.location();

	ASSERT(!currentMove_->prev->startMarker());
	currentMove_ = currentMove_->prev;
	currentPos_->UndoSimpleMove(currentMove_->moveData);

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
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.enterVariation(varNumber))
		return ERROR_NoVariation;
	coreLocation_ = coreCursor.location();

	for (auto subVar = currentMove_; subVar->varChild; --varNumber) {
		subVar = subVar->varChild;
		if (varNumber == 0) {
			currentMove_ = subVar->next; // skip the START_MARKER
			++varDepth_;

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
	scid::core::GameCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.exitVariation())
		return ERROR_NoVariation;
	coreLocation_ = coreCursor.location();

	// Algorithm: go back previous moves as far as possible, then
	// go up to the parent of the variation.
	while (!currentMove_->prev->startMarker()) {
		currentMove_ = currentMove_->prev;
		currentPos_->UndoSimpleMove(currentMove_->moveData);
	}
	currentMove_ = currentMove_->getParent().first;
	--varDepth_;

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move to the beginning of the game.
//
void Game::toStart() {
	scid::core::GameCursor coreCursor(coreGame_);
	coreCursor.toStart();
	coreLocation_ = coreCursor.location();

	if (auto startPos = coreGame_.startPosition()) {
		*currentPos_ = *startPos;
	} else {
		currentPos_->StdStart();
	}
	varDepth_ = 0;
	currentMove_ = firstMove_->next;

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
	scid::core::GameCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	if (coreCursor.previousMove() &&
	    !coreCursor.previousMove()->childVariations.empty() &&
	    coreCursor.previous()) {
		[[maybe_unused]] const bool entered = coreCursor.enterVariation(0);
		ASSERT(entered);
		auto err = previous();
		if (err == OK)
			err = enterVariation(0);
		ASSERT(coreLocation_ == coreCursor.location());
		return err;
	}

	while (!coreCursor.next()) {
		if (coreCursor.variationDepth() == 0)
			return ERROR_EndOfMoveList;

		auto varnum = static_cast<uint>(coreCursor.variationIndex());
		[[maybe_unused]] const bool exited = coreCursor.exitVariation();
		ASSERT(exited);
		auto err = exitVariation();
		ASSERT(err == OK);

		if (coreCursor.enterVariation(varnum + 1)) {
			err = enterVariation(varnum + 1);
			ASSERT(err == OK);
			ASSERT(coreLocation_ == coreCursor.location());
			return OK;
		}

		[[maybe_unused]] const bool skippedParent = coreCursor.next();
		ASSERT(skippedParent);
		err = next();
		ASSERT(err == OK);
	}

	auto err = next();
	ASSERT(err == OK);
	ASSERT(coreLocation_ == coreCursor.location());
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
	return pgnLocationOf(coreGame_, coreLocation_);
}

// TODO [Game]: Move PGN-order traversal to a PGN/export traversal adapter
// instead of keeping it on the generic Game cursor surface.
unsigned Game::pgnOffset() const {
	return pgnOffsetOf(coreGame_, coreLocation_);
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
	restoreCoreCursor(coreCursor, coreLocation_);
	coreCursor.addMove(toCoreMoveAction(sm));
	coreLocation_ = coreCursor.location();
	TEMP_syncLegacyMovetextFromCore();

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
	return OK;
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
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.addVariation())
		return ERROR_NoVariation;
	coreLocation_ = coreCursor.location();
	TEMP_syncLegacyMovetextFromCore();

	// Invariants
	ASSERT(currentMove_ && currentMove_->prev);
	ASSERT(!currentMove_->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::promoteVariationToFirst():
// Promotes the current variation to first variation.
errorT Game::promoteVariationToFirst() {
	scid::core::MovetextCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.promoteVariationToFirst())
		return ERROR_NoVariation;

	coreLocation_ = coreCursor.location();
	TEMP_syncLegacyMovetextFromCore();
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::promoteVariationToMainline():
//    Like promoteVariationToFirst, but promotes the variation to the main line,
//    demoting the main line to be the first variation.
errorT Game::promoteVariationToMainline() {
	scid::core::MovetextCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.promoteVariationToMainline())
		return ERROR_NoVariation;

	coreLocation_ = coreCursor.location();
	TEMP_syncLegacyMovetextFromCore();
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
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.deleteVariation())
		return ERROR_NoVariation;

	coreLocation_ = coreCursor.location();
	TEMP_syncLegacyMovetextFromCore();
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
	restoreCoreCursor(coreCursor, coreLocation_);
	coreCursor.truncate();
	coreLocation_ = coreCursor.location();
	TEMP_syncLegacyMovetextFromCore();

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
	    Position pos;
	    if (pos.ReadFromFEN(tempStr) != OK)
	        return;

    if (varDepth_ != 0 && promoteVariationToMainline() != OK)
		return;

	scid::core::MovetextCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);

    coreGame_.setStartPosition(pos);
	coreCursor.truncateBeforeCursor();
	coreLocation_ = coreCursor.location();
	TEMP_syncLegacyMovetextFromCore();
}

} // namespace scid::database

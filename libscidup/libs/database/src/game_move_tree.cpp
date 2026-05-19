#include "scidup/database/game.h"

#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_cursor.h"
#include "scidup/core/position.h"
#include "scidup/database/common.h"

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

} // namespace

///////////////////////////////////////////////////////////////////////////
// A "location" in the game is represented by the core MovetextLocation plus a
// temporary current-position cache for callers that have not moved to core
// traversal yet.
// The following functions modify ONLY the current location of the game.

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move current position forward one move.
// Keep the temporary legacy cursor cache in sync while deriving the position
// update from the core move action.
//
errorT Game::next(void) {
	scid::core::GameCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	if (!coreCursor.next())
		return ERROR_EndOfMoveList;
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
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
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
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
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
	return OK;
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
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move to the beginning of the game.
//
void Game::toStart() {
	scid::core::GameCursor coreCursor(coreGame_);
	coreCursor.toStart();
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
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

///////////////////////////////////////////////////////////////////////////
// The following functions modify the moves graph in order to add or delete
// moves. Promoting variations also modifies the moves graph.

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::addMove():
//      Add a move at current position and do it.
//
errorT Game::addMove(simpleMoveT const& sm) {
	// We must be at the end of a game/variation to add a move:
	scid::core::GameCursor readCursor(coreGame_);
	restoreCoreCursor(readCursor, coreLocation_);
	if (!readCursor.isAtLineEnd())
		truncate();

	scid::core::MovetextCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	coreCursor.addMove(toCoreMoveAction(sm));
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
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
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
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

	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
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

	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
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

	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::truncate():
//      Remove moves from the current move to the end of this line.
//      For speed and simplicity, moves and comments are not freed.
//      So repeatedly adding moves and truncating a game will waste
//      memory until the game is cleared.
void Game::truncate() {
	scid::core::GameCursor readCursor(coreGame_);
	restoreCoreCursor(readCursor, coreLocation_);
	if (readCursor.isAtLineEnd())
		return;

	scid::core::MovetextCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);
	coreCursor.truncate();
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::truncateStart():
//      Remove all moves leading to the current position.
void Game::truncateStart() {
	// It is necessary to rebuild the current position using ReadFromFEN()
	// because the order of pieces is important when encoding to SCIDv4 format.
	scid::core::GameCursor readCursor(coreGame_);
	restoreCoreCursor(readCursor, coreLocation_);
	auto currentPosition = readCursor.currentPosition();
	if (!currentPosition)
		return;

	char tempStr[256];
	currentPosition->PrintFEN(tempStr, sizeof(tempStr));
	Position pos;
	if (pos.ReadFromFEN(tempStr) != OK)
		return;

	if (readCursor.variationDepth() != 0 && promoteVariationToMainline() != OK)
		return;

	scid::core::MovetextCursor coreCursor(coreGame_);
	restoreCoreCursor(coreCursor, coreLocation_);

	coreGame_.setStartPosition(pos);
	coreCursor.truncateBeforeCursor();
	[[maybe_unused]] const bool restored = setCoreLocation(coreCursor.location());
	ASSERT(restored);
}

} // namespace scid::database

#include "scidup/database/game.h"

#include "scidup/core/movetext_cursor.h"
#include "scidup/database/common.h"
#include "movetext_projection.h"
#include "movetree.h"

namespace scid::database {

namespace {

bool syncCoreMoveMetadata(scid::core::Game& coreGame,
                          scid::core::MovetextLocation location,
                          const moveT* legacyMove) {
	if (!legacyMove || legacyMove->startMarker() || legacyMove->endMarker())
		return false;

	scid::core::MovetextCursor cursor(coreGame);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	ASSERT(restored);

	auto move = cursor.previousMove();
	if (!move)
		return false;

	move->metadata.comment = legacyMove->comment;
	move->metadata.nags.assign(legacyMove->nags,
	                           legacyMove->nags + legacyMove->nagCount);
	return true;
}

bool syncCoreComment(scid::core::Game& coreGame,
                     scid::core::MovetextLocation location,
                     const moveT* firstMove,
                     const moveT* legacyMove) {
	if (!legacyMove)
		return false;

	if (legacyMove == firstMove) {
		coreGame.setInitialComment(legacyMove->comment);
		return true;
	}

	if (!legacyMove->startMarker()) {
		return syncCoreMoveMetadata(coreGame, location, legacyMove);
	}

	scid::core::MovetextCursor cursor(coreGame);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	ASSERT(restored);

	auto variation = cursor.currentVariation();
	if (!variation)
		return false;

	variation->initialComment = legacyMove->comment;
	return true;
}

} // namespace

// TODO [Game]: Move NAG/comment storage behind Move.metadata once the core
// Move shape exists. These methods are compatibility accessors around legacy
// moveT fields at the current cursor location.
void Game::clearNags() {
	currentMove_->prev->nagCount = 0;
	currentMove_->prev->nags[0] = 0;
	if (!syncCoreMoveMetadata(coreGame_, coreLocation_, currentMove_->prev))
		TEMP_movetext::syncCoreMovetextAndLocation(
		    coreGame_, firstMove_, currentMove_, coreLocation_);
}

byte* Game::nags() const {
	return currentMove_->prev->nags;
}

byte* Game::nextNags() const {
	return currentMove_->nags;
}

std::pair<const char*, const char*> Game::previousComments() const {
	std::pair<const char*, const char*> res = {"", ""};
	auto move = currentMove_->getPrevMove();
	if (move)
		move = move->getPrevMove();
	if (move) {
		res.first = move->comment.c_str();
		move = move->getPrevMove();
	}
	if (move)
		res.second = move->comment.c_str();

	return res;
}

const char* Game::moveComment() const {
	return currentMove_->prev->comment.c_str();
}

errorT Game::addNag (byte nag) {
    moveT * m = currentMove_->prev;
    if (m->nagCount + 1 >= MAX_NAGS) { return ERROR_GameFull; }
    if (nag == 0) { /* Nags cannot be zero! */ return OK; }
	// If it is a move nag replace an existing
	if( nag >= 1 && nag <= 6)
		for( int i=0; i<m->nagCount; i++)
			if( m->nags[i] >= 1 && m->nags[i] <= 6)
			{
				m->nags[i] = nag;
				if (!syncCoreMoveMetadata(coreGame_, coreLocation_, m))
					TEMP_movetext::syncCoreMovetextAndLocation(
					    coreGame_, firstMove_, currentMove_, coreLocation_);
				return OK;
			}
	// If it is a position nag replace an existing
	if( nag >= 10 && nag <= 21)
		for( int i=0; i<m->nagCount; i++)
			if( m->nags[i] >= 10 && m->nags[i] <= 21)
			{
				m->nags[i] = nag;
				if (!syncCoreMoveMetadata(coreGame_, coreLocation_, m))
					TEMP_movetext::syncCoreMovetextAndLocation(
					    coreGame_, firstMove_, currentMove_, coreLocation_);
				return OK;
			}
	if( nag >= 1 && nag <= 6)
	{
		// Put Move Nags at the beginning
		for( int i=m->nagCount; i>0; i--)  m->nags[i] =  m->nags[i-1];
		m->nags[0] = nag;
	}
	else
		m->nags[m->nagCount] = nag;
	m->nagCount += 1;
	m->nags[m->nagCount] = 0;
	if (!syncCoreMoveMetadata(coreGame_, coreLocation_, m))
		TEMP_movetext::syncCoreMovetextAndLocation(
		    coreGame_, firstMove_, currentMove_, coreLocation_);
    return OK;
}

errorT Game::removeNag (bool isMoveNag) {
    moveT * m = currentMove_->prev;
	if( isMoveNag)
	{
		for( int i=0; i<m->nagCount; i++)
			if( m->nags[i] >= 1 && m->nags[i] <= 6)
			{
				m->nagCount -= 1;
				for( int j=i; j<m->nagCount; j++)  m->nags[j] =  m->nags[j+1];
				m->nags[m->nagCount] = 0;
				if (!syncCoreMoveMetadata(coreGame_, coreLocation_, m))
					TEMP_movetext::syncCoreMovetextAndLocation(
					    coreGame_, firstMove_, currentMove_, coreLocation_);
				return OK;
			}
	}
	else
	{
		for( int i=0; i<m->nagCount; i++)
			if( m->nags[i] >= 10 && m->nags[i] <= 21)
			{
				m->nagCount -= 1;
				for( int j=i; j<m->nagCount; j++)  m->nags[j] =  m->nags[j+1];
				m->nags[m->nagCount] = 0;
				if (!syncCoreMoveMetadata(coreGame_, coreLocation_, m))
					TEMP_movetext::syncCoreMovetextAndLocation(
					    coreGame_, firstMove_, currentMove_, coreLocation_);
				return OK;
			}
	}
    return OK;
}


void Game::setMoveComment(const char* comment) {
	ASSERT(currentMove_ != NULL && currentMove_->prev != NULL);
	moveT* m = currentMove_->prev;
	if (comment == NULL) {
		m->comment.clear();
	} else {
		m->comment = comment;
		// CommentsFlag = 1;
	}
	if (!syncCoreComment(coreGame_, coreLocation_, firstMove_, m))
		TEMP_movetext::syncCoreMovetextAndLocation(
		    coreGame_, firstMove_, currentMove_, coreLocation_);
}
} // namespace scid::database

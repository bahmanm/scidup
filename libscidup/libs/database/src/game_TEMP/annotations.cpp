#include "scidup/database/game.h"

#include "scidup/database/common.h"
#include "movetree.h"

namespace scid::database {

void Game::ClearNags() {
	CurrentMove->prev->nagCount = 0;
	CurrentMove->prev->nags[0] = 0;
}

byte* Game::GetNags() const {
	return CurrentMove->prev->nags;
}

byte* Game::GetNextNags() const {
	return CurrentMove->nags;
}

std::pair<const char*, const char*> Game::previousComments() const {
	std::pair<const char*, const char*> res = {"", ""};
	auto move = CurrentMove->getPrevMove();
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

const char* Game::GetMoveComment() const {
	return CurrentMove->prev->comment.c_str();
}

std::string& Game::accessMoveComment() {
	return CurrentMove->prev->comment;
}

void Game::viewMainlineMoves(
    const std::function<void(const simpleMoveT&)>& visitor) const {
	for (const auto* m = FirstMove; !m->endMarker(); m = m->Next()) {
		if (!m->startMarker()) {
			visitor(m->move());
		}
	}
}

void Game::viewMovetext(
    const std::function<void(const GameMoveView&)>& visitor) const {
	if (!FirstMove->comment.empty()) {
		visitor({GameMoveViewKind::InitialComment, {}, {},
		         FirstMove->comment, {}});
	}

	for (auto m = FirstMove; (m = m->nextMoveInPGN());) {
		if (m->startMarker()) {
			visitor({GameMoveViewKind::VariationStart, {}, {}, m->comment, {}});
		} else if (m->endMarker()) {
			if (m->nextMoveInPGN()) {
				visitor({GameMoveViewKind::VariationEnd, {}, {}, {}, {}});
			}
		} else {
			visitor({GameMoveViewKind::Move, m->moveData, m->san, m->comment,
			         {m->nags, m->nagCount}});
		}
	}
}

errorT Game::AddNag (byte nag) {
    moveT * m = CurrentMove->prev;
    if (m->nagCount + 1 >= MAX_NAGS) { return ERROR_GameFull; }
    if (nag == 0) { /* Nags cannot be zero! */ return OK; }
	// If it is a move nag replace an existing
	if( nag >= 1 && nag <= 6)
		for( int i=0; i<m->nagCount; i++)
			if( m->nags[i] >= 1 && m->nags[i] <= 6)
			{
				m->nags[i] = nag;
				return OK;
			}
	// If it is a position nag replace an existing
	if( nag >= 10 && nag <= 21)
		for( int i=0; i<m->nagCount; i++)
			if( m->nags[i] >= 10 && m->nags[i] <= 21)
			{
				m->nags[i] = nag;
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
    return OK;
}

errorT Game::RemoveNag (bool isMoveNag) {
    moveT * m = CurrentMove->prev;
	if( isMoveNag)
	{
		for( int i=0; i<m->nagCount; i++)
			if( m->nags[i] >= 1 && m->nags[i] <= 6)
			{
				m->nagCount -= 1;
				for( int j=i; j<m->nagCount; j++)  m->nags[j] =  m->nags[j+1];
				m->nags[m->nagCount] = 0;
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
				return OK;
			}
	}
    return OK;
}


void Game::SetMoveComment(const char* comment) {
	ASSERT(CurrentMove != NULL && CurrentMove->prev != NULL);
	moveT* m = CurrentMove->prev;
	if (comment == NULL) {
		m->comment.clear();
	} else {
		m->comment = comment;
		// CommentsFlag = 1;
	}
}
} // namespace scid::database

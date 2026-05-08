#include "scidup/database/game.h"

#include "scidup/database/common.h"
#include "movetree.h"

namespace scid::database {

namespace {

scid::core::MoveAction toMoveAction(simpleMoveT const& sm) {
	return {sm.from, sm.to, sm.promote};
}

void copyMoveData(moveT const& source, scid::core::Move& dest);

void copyLine(moveT const* source, scid::core::MoveSequence& dest) {
	for (auto move = source; move && !move->endMarker(); move = move->next) {
		if (move->startMarker())
			continue;

		auto& destMove = dest.moves.emplace_back();
		copyMoveData(*move, destMove);
	}
}

void copyVariations(moveT const& source, scid::core::Move& dest) {
	for (auto variation = source.varChild; variation;
	     variation = variation->varChild) {
		auto& destVariation = dest.childVariations.emplace_back();
		copyLine(variation->next, destVariation.line);
	}
}

void copyMoveData(moveT const& source, scid::core::Move& dest) {
	dest.action = toMoveAction(source.moveData);
	dest.san = source.san;
	dest.metadata.comment = source.comment;
	dest.metadata.nags.assign(source.nags, source.nags + source.nagCount);
	copyVariations(source, dest);
}

} // namespace

void Game::TEMP_syncCoreMovetext() {
	coreGame_.clearMovetext();
	coreGame_.setInitialComment(FirstMove->comment);
	for (auto move = FirstMove->next; move && !move->endMarker();
	     move = move->next) {
		if (move->startMarker())
			continue;

		auto& dest = coreGame_.appendMainlineMove(toMoveAction(move->moveData));
		copyMoveData(*move, dest);
	}
}

// TODO [Game]: Move NAG/comment storage behind Move.metadata once the core
// Move shape exists. These methods are compatibility accessors around legacy
// moveT fields at the current cursor location.
void Game::ClearNags() {
	CurrentMove->prev->nagCount = 0;
	CurrentMove->prev->nags[0] = 0;
	TEMP_syncCoreMovetext();
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
	// TODO [Game]: Rebuild this on core Game/GameCursor traversal and the
	// future MoveAction type instead of exposing legacy simpleMoveT directly.
	for (const auto* m = FirstMove; !m->endMarker(); m = m->Next()) {
		if (!m->startMarker()) {
			visitor(m->move());
		}
	}
}

void Game::viewMovetext(
    const std::function<void(const scid::core::pgn::MovetextEntry&)>&
        visitor) const {
	// TODO [Game]: Move PGN-shaped movetext traversal to a PGN/export adapter
	// once generic GameCursor traversal exists.
	using scid::core::pgn::MovetextEntryKind;

	if (!FirstMove->comment.empty()) {
		visitor({MovetextEntryKind::InitialComment, {}, FirstMove->comment, {}});
	}

	for (auto m = FirstMove; (m = m->nextMoveInPGN());) {
		if (m->startMarker()) {
			visitor({MovetextEntryKind::VariationStart, {}, m->comment, {}});
		} else if (m->endMarker()) {
			if (m->nextMoveInPGN()) {
				visitor({MovetextEntryKind::VariationEnd, {}, {}, {}});
			}
		} else {
			visitor({MovetextEntryKind::Move, m->san, m->comment,
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
				TEMP_syncCoreMovetext();
				return OK;
			}
	// If it is a position nag replace an existing
	if( nag >= 10 && nag <= 21)
		for( int i=0; i<m->nagCount; i++)
			if( m->nags[i] >= 10 && m->nags[i] <= 21)
			{
				m->nags[i] = nag;
				TEMP_syncCoreMovetext();
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
	TEMP_syncCoreMovetext();
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
				TEMP_syncCoreMovetext();
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
				TEMP_syncCoreMovetext();
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
	TEMP_syncCoreMovetext();
}
} // namespace scid::database

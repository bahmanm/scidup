//////////////////////////////////////////////////////////////////////
//
//  FILE:       game.cpp
//              Game class methods
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    3.5
//
//  Notice:     Copyright (c) 2000-2003  Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//
//////////////////////////////////////////////////////////////////////

#include "scidup/database/game.h"
#include "scidup/database/common.h"
#include "movetree.h"
#include "scidup/core/position.h"
#include <algorithm>
#include <cstring>

namespace scid::database {

void MoveChunkDeleter::operator()(moveT* ptr) const {
	delete[] ptr;
}

Game::~Game() = default;

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

//////////////////////////////////////////////////////////////////////
//  PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move allocation:
//      moves are allocated in chunks to save memory and for faster
//      performance.
//
constexpr int MOVE_CHUNKSIZE = 128;

moveT* Game::allocMove() {
	if (moveChunkUsed_ == MOVE_CHUNKSIZE) {
		moveChunks_.emplace_front(new moveT[MOVE_CHUNKSIZE]);
		moveChunkUsed_ = 0;
	}
	return moveChunks_.front().get() + moveChunkUsed_++;
}

moveT* Game::NewMove(markerT marker) {
	moveT* res = allocMove();
	res->clear();
	res->marker = marker;
	return res;
}

Game::Game(const Game& obj) {
	extraTags_ = obj.extraTags_;
	WhiteStr = obj.WhiteStr;
	BlackStr = obj.BlackStr;
	EventStr = obj.EventStr;
	SiteStr = obj.SiteStr;
	RoundStr = obj.RoundStr;
	Date = obj.Date;
	EventDate = obj.EventDate;
	EcoCode = obj.EcoCode;
	WhiteElo = obj.WhiteElo;
	BlackElo = obj.BlackElo;
	WhiteRatingType = obj.WhiteRatingType;
	BlackRatingType = obj.BlackRatingType;
	Result = obj.Result;
	std::copy_n(obj.ScidFlags, sizeof(obj.ScidFlags), ScidFlags);

	if (obj.StartPos)
		StartPos = std::make_unique<Position>(*obj.StartPos);

	NumHalfMoves = obj.NumHalfMoves;
	NumMovesPrinted = obj.NumMovesPrinted;
	PgnStyle = obj.PgnStyle;
	PgnFormat = obj.PgnFormat;
	HtmlStyle = obj.HtmlStyle;

	moveChunkUsed_ = MOVE_CHUNKSIZE;
	FirstMove = obj.FirstMove->cloneLine(nullptr,
	                                     [this]() { return allocMove(); });

	MoveToLocationInPGN(obj.GetLocationInPGN());
}
Game* Game::clone() { return new Game(*this); }

void Game::strip(bool variations, bool comments, bool NAGs) {
    while (variations && MoveExitVariation() == OK) { // Go to main line
    }

    for (auto& chunk : moveChunks_) {
        moveT* move = chunk.get();
        moveT* end = (chunk == moveChunks_.front()) ? move + moveChunkUsed_
                                                    : move + MOVE_CHUNKSIZE;
        for (; move != end; ++move) {
            if (variations) {
                move->numVariations = 0;
                move->varChild = nullptr;
            }
            if (comments)
                move->comment.clear();

            if (NAGs) {
                move->nagCount = 0;
                std::fill_n(move->nags, sizeof(move->nags), 0);
            }
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::ClearMoves(): clear all moves.
void Game::ClearMoves() {
    // Delete any chunks of moves except the first:
    if (moveChunks_.empty()) {
        moveChunkUsed_ = MOVE_CHUNKSIZE;
    } else {
        moveChunks_.erase_after(moveChunks_.begin(), moveChunks_.end());
        moveChunkUsed_ = 0;
    }
    StartPos = nullptr;
    CurrentPos->StdStart();

    // Initialize FirstMove: start and end of movelist markers
    FirstMove = NewMove(START_MARKER);
    CurrentMove = NewMove(END_MARKER);
    FirstMove->setNext(CurrentMove);

    VarDepth = 0;
    NumHalfMoves = 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::Clear():
//      Reset the game to its normal empty state.
//
void Game::Clear() {
	extraTags_.clear();
	WhiteStr.clear();
	BlackStr.clear();
	EventStr.clear();
	SiteStr.clear();
	RoundStr.clear();
	Date = ZERO_DATE;
	EventDate = ZERO_DATE;
	EcoCode = 0;
	WhiteElo = BlackElo = 0;
	WhiteRatingType = BlackRatingType = RATING_Elo;
	Result = RESULT_None;
	ScidFlags[0] = 0;

	NumMovesPrinted = 0;
	PgnStyle = PGN_STYLE_TAGS | PGN_STYLE_VARS | PGN_STYLE_COMMENTS;
	PgnFormat = PGN_FORMAT_Plain;
	HtmlStyle = 0;

	ClearMoves();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::FindExtraTag():
//   Finds and returns an extra PGN tag if it
//   exists, or NULL if it does not exist.
const char* Game::FindExtraTag(const char* tag) const {
    for (auto& e : extraTags_) {
        if (e.first == tag)
            return e.second.c_str();
    }
    return NULL;
}

std::string_view Game::GetResultStr() const {
	using namespace std::literals;
	static std::string_view res[] = {"*"sv, "1-0"sv, "0-1"sv, "1/2-1/2"sv};
	return res[Result];
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::SetMoveComment():
//      Sets the comment for a move. A comment before the game itself
//      is stored as a comment of FirstMove.
//
void
Game::SetMoveComment (const char * comment)
{
    ASSERT (CurrentMove != NULL  &&  CurrentMove->prev != NULL);
    moveT * m = CurrentMove->prev;
    if (comment == NULL) {
        m->comment.clear();
    } else {
        m->comment = comment;
        // CommentsFlag = 1;
    }
}

int Game::setRating(colorT col, const char* ratingType, size_t ratingTypeLen,
                    std::pair<const char*, const char*> rating) {
	auto begin = ratingTypeNames;
	const size_t ratingSz = 7;
	auto it = std::find_if(begin, begin + ratingSz, [&](auto rType) {
		return std::equal(ratingType, ratingType + ratingTypeLen, rType,
		                  rType + std::strlen(rType));
	});
	auto rType = static_cast<ratingTypeT>(std::distance(begin, it));
	if (rType >= ratingSz)
		return -1;

	int res = 1;
	auto elo = strGetUnsigned(std::string{rating.first, rating.second}.c_str());
	if (elo > MAX_ELO) {
		elo = 0;
		res = 0;
	}
	if (col == WHITE) {
		SetWhiteElo(static_cast<ratingT>(elo));
		SetWhiteRatingType(rType);
	} else {
		SetBlackElo(static_cast<ratingT>(elo));
		SetBlackRatingType(rType);
	}
	return res;
}

///////////////////////////////////////////////////////////////////////////
// A "location" in the game is represented by a position (Game::CurrentPos), the
// next move to be played (Game::CurrentMove) and the number of parent variations
// (Game::VarDepth). Since CurrentMove is the next move to be played, some
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
errorT Game::MoveForward(void) {
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
// Game::MoveBackup():
//      Backup one move.
//
errorT Game::MoveBackup(void) {
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
// Game::MoveIntoVariation():
//      Move into a subvariation. Variations are numbered from 0.
errorT Game::MoveIntoVariation(uint varNumber) {
	for (auto subVar = CurrentMove; subVar->varChild; --varNumber) {
		subVar = subVar->varChild;
		if (varNumber == 0) {
			CurrentMove = subVar->next; // skip the START_MARKER
			++VarDepth;

			// Invariants
			ASSERT(CurrentMove && CurrentMove->prev);
			ASSERT(!CurrentMove->startMarker());
			return OK;
		}
	}
	return ERROR_NoVariation; // there is no such variation
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::MoveExitVariation():
//      Move out of a variation, to the parent.
//
errorT Game::MoveExitVariation(void) {
	if (VarDepth == 0) // not in a variation!
		return ERROR_NoVariation;

	// Algorithm: go back previous moves as far as possible, then
	// go up to the parent of the variation.
	while (MoveBackup() == OK) {
	}
	CurrentMove = CurrentMove->getParent().first;
	--VarDepth;

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Move to the beginning of the game.
//
void Game::MoveToStart() {
	if (StartPos) {
		*CurrentPos = *StartPos;
	} else {
		CurrentPos->StdStart();
	}
	VarDepth = 0;
	CurrentMove = FirstMove->next;

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
}

void Game::MoveToEnd() {
	MoveToStart();
	while (MoveForward() == OK) {
	}
}

errorT Game::MoveForwardInPGN() {
	if (CurrentMove->prev->varChild && MoveBackup() == OK)
		return MoveIntoVariation(0);

	while (MoveForward() != OK) {
		if (VarDepth == 0)
			return ERROR_EndOfMoveList;

		auto varnum = GetVarNumber();
		MoveExitVariation();
		if (MoveIntoVariation(varnum + 1) == OK)
			return OK;

		MoveForward();
	}
	return OK;
}

errorT Game::MoveToLocationInPGN(unsigned stopLocation) {
	MoveToStart();
	for (unsigned loc = 1; loc < stopLocation; ++loc) {
		errorT err = MoveForwardInPGN();
		if (err != OK)
			return err;
	}
	return OK;
}

unsigned Game::GetLocationInPGN() const {
	unsigned res = 1;
	const moveT* last_move = CurrentMove->prev;
	const moveT* move = FirstMove;
	for (; move != last_move; move = move->nextMoveInPGN()) {
		if (!move->endMarker())
			++res;
	}
	return res;
}

unsigned Game::GetPgnOffset() const {
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
// Game::AddMove():
//      Add a move at current position and do it.
//
errorT Game::AddMove(simpleMoveT const& sm) {
	// We must be at the end of a game/variation to add a move:
	if (!CurrentMove->endMarker())
		Truncate();

	CurrentMove->setNext(NewMove(END_MARKER));
	CurrentMove->marker = NO_MARKER;
	CurrentMove->moveData = sm;
	if (VarDepth == 0)
		++NumHalfMoves;

	return MoveForward();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::AddVariation():
//      Add a variation for the current move.
//      Also moves into the variation.
errorT Game::AddVariation() {
	auto err = MoveBackup();
	if (err != OK)
		return err;

	auto newVar = NewMove(START_MARKER);
	newVar->setNext(NewMove(END_MARKER));
	CurrentMove->appendChild(newVar);

	// Move into variation
	CurrentMove = newVar->next;
	++VarDepth;

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::FirstVariation():
// Promotes the current variation to first variation.
errorT Game::FirstVariation() {
	auto parent = CurrentMove->getParent();
	auto root = parent.first;
	if (!root)
		return ERROR_NoVariation;

	root->detachChild(parent.second);
	root->insertChild(parent.second, 0);
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::MainVariation():
//    Like FirstVariation, but promotes the variation to the main line,
//    demoting the main line to be the first variation.
errorT Game::MainVariation() {
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

	ASSERT(VarDepth);
	if (--VarDepth == 0) { // Recalculate NumHalfMoves
		const auto count_moves = [](auto move) {
			int res = 0;
			while (!move->endMarker()) {
				++res;
				move = move->next;
			}
			return res;
		};
		ASSERT(FirstMove->startMarker() && FirstMove->next);
		NumHalfMoves = count_moves(FirstMove->next);
	}

	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::DeleteVariation():
//      Deletes a variation. Variations are numbered from 0.
//      Note that for speed and simplicity, freed moves are not
//      added to the free list. This means that repeatedly adding and
//      deleting variations will waste memory until the game is cleared.
//
errorT Game::DeleteVariation() {
	auto parent = CurrentMove->getParent();
	auto root = parent.first;
	if (!root || MoveExitVariation() != OK)
		return ERROR_NoVariation;

	root->detachChild(parent.second);
	return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::Truncate():
//      Truncate game at the current move.
//      For speed and simplicity, moves and comments are not freed.
//      So repeatedly adding moves and truncating a game will waste
//      memory until the game is cleared.
void Game::Truncate() {
	if (CurrentMove->endMarker())
		return;

	auto endMove = NewMove(END_MARKER);
	CurrentMove->prev->setNext(endMove);

	CurrentMove = endMove;
	if (VarDepth == 0)
		NumHalfMoves = GetCurrentPly();

	// Invariants
	ASSERT(CurrentMove && CurrentMove->prev);
	ASSERT(!CurrentMove->startMarker());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::TruncateStart():
//      Truncate all moves leading to current position.
void Game::TruncateStart() {
	    // It is necessary to rebuild the current position using ReadFromFEN()
	    // because the order of pieces is important when encoding to SCIDv4 format.
	    char tempStr[256];
	    CurrentPos->PrintFEN(tempStr, sizeof(tempStr));
	    auto pos = std::make_unique<Position>();
	    if (pos->ReadFromFEN(tempStr) != OK)
	        return;

    if (VarDepth != 0 && MainVariation() != OK)
		return;

    NumHalfMoves -= GetCurrentPly();
    StartPos = std::move(pos);
    *CurrentPos = *StartPos;
    FirstMove->setNext(CurrentMove);

    // Do all the moves to update moveData.pieceNum to the new StartPos
    do {
        if (!CurrentMove->startMarker() && !CurrentMove->endMarker()) {
            CurrentPos->fillMove(CurrentMove->moveData);
        }
    } while (MoveForwardInPGN() == OK);
    MoveToStart();
}

ratingT
Game::GetAverageElo () {
	auto white = WhiteElo;
	auto black = BlackElo;
	return (white == 0 || black == 0) ? 0 : (white + black) / 2;
}

//////////////////////////////////////////////////////////////////////
//  EOF:    game.cpp
//////////////////////////////////////////////////////////////////////

} // namespace scid::database

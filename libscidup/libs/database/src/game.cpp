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
#include "scidup/database/bytebuf.h"
#include "scidup/database/common.h"
#include "scidup/core/dstring.h"
#include "scidup/core/notation.h"
#include "movetree.h"
#include "scidup/core/position.h"
#include "textbuf.h"
#include <algorithm>
#include <cstdio>
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

std::string Game::currentPosUCI() const {
	std::string res = "position startpos moves";
	char FEN[256] = {};

	std::vector<const moveT*> moves;
	const moveT* move = CurrentMove;
	while ((move = move->getPrevMove())) {
		if (move->moveData.isNullMove()) {
			Position lastValidPos = *currentPos();
				for (const moveT* m : moves) {
					lastValidPos.UndoSimpleMove(m->moveData);
				}
				lastValidPos.PrintFEN(FEN, sizeof(FEN));
				break;
			}
		moves.emplace_back(move);
	}

		if (*FEN || HasNonStandardStart(FEN, sizeof(FEN))) {
			res.replace(9, 4, "fen ");
			res.replace(13, 4, FEN);
		}

	const auto allocSpeedup = res.size();
	res.resize(allocSpeedup + moves.size() * 6);
	auto it = res.data() + allocSpeedup;
	for (auto m = moves.crbegin(), end = moves.crend(); m != end; ++m) {
		*it++ = ' ';
		it = (*m)->moveData.toLongNotation(it);
	}
	res.resize(std::distance(res.data(), it)); // shrink
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

namespace {
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// calcHomePawnMask():
//      Computes the homePawn mask for a position.
//
int calcHomePawnMask (pieceT pawn, const pieceT* board)
{
    ASSERT (pawn == WP  ||  pawn == BP);
    const pieceT* bd = &(board[ (pawn == WP ? H2 : H7) ]);
    int result = 0;
    if (*bd == pawn) { result |= 128; }  bd--;   // H-fyle pawn
    if (*bd == pawn) { result |=  64; }  bd--;   // G-fyle pawn
    if (*bd == pawn) { result |=  32; }  bd--;   // F-fyle pawn
    if (*bd == pawn) { result |=  16; }  bd--;   // E-fyle pawn
    if (*bd == pawn) { result |=   8; }  bd--;   // D-fyle pawn
    if (*bd == pawn) { result |=   4; }  bd--;   // C-fyle pawn
    if (*bd == pawn) { result |=   2; }  bd--;   // B-fyle pawn
    if (*bd == pawn) { result |=   1; }          // A-fyle pawn
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// patternsMatch():
//      Used by Game::MaterialMatch() to test patterns.
//      Returns 1 if all the patterns in the list match, 0 otherwise.
//
int patternsMatch(const Position* pos, patternT* ptn, size_t ptn_size) {
    const pieceT* board = pos->GetBoard();
    for (auto ptn_end = ptn + ptn_size; ptn != ptn_end; ++ptn) {
        if (ptn->rankMatch == NO_RANK) {

            if (ptn->fyleMatch == NO_FYLE) { // Nothing to test!
            } else {  // Test this fyle:
                squareT sq = square_Make (ptn->fyleMatch, RANK_1);
                int found = 0;
                for (uint i=0; i < 8; i++, sq += 8) {
                    if (board[sq] == ptn->pieceMatch) { found = 1; break; }
                }
                if (found != ptn->flag) { return 0; }
            }

        } else { // rankMatch is a rank from 1 to 8:

            if (ptn->fyleMatch == NO_FYLE) { // Test the whole rank:
                int found = 0;
                squareT sq = square_Make (A_FYLE, ptn->rankMatch);
                for (uint i=0; i < 8; i++, sq++) {
                    if (board[sq] == ptn->pieceMatch) { found = 1; break; }
                }
                if (found != ptn->flag) { return 0; }
            } else {  // Just test one square:
                squareT sq = square_Make(ptn->fyleMatch, ptn->rankMatch);
                int found = 0;
                if (board[sq] == ptn->pieceMatch) { found = 1; }
                if (found != ptn->flag) { return 0; }
            }
        }
    }

    // If we reach here, all patterns matched:
    return 1;
}
} // end of anonymous namespace

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::MaterialMatch(): Material search test.
//      The parameters min and max should each be an array of 15
//      counts, to specify the maximum and minimum number of counts
//      of each type of piece.
//
bool Game::MaterialMatch(bool PromotionsFlag, ByteBuffer& buf, byte* min,
                         byte* max, patternT* patterns, size_t ptn_size,
                         int minPly, int maxPly, int matchLength,
                         bool oppBishops, bool sameBishops, int minDiff,
                         int maxDiff) {
    ASSERT (matchLength >= 1);

    int matchesNeeded = matchLength;
    int matDiff;
    uint plyCount = 0;
    errorT err = DecodeSkipTags(&buf);
    while (err == OK) {
        bool foundMatch = false;
        byte wMinor, bMinor;

        // If current pos has LESS than the minimum of pawns, this
        // game can never match so return false;
        if (CurrentPos->PieceCount(WP) < min[WP]) { return false; }
        if (CurrentPos->PieceCount(BP) < min[BP]) { return false; }

        // If not in the valid move range, go to the next move or return:
        if ((int)plyCount > maxPly) { return false; }
        if ((int)plyCount < minPly) { goto Next_Move; }

// For these comparisons, we really could only do half of them each move,
// according to which side just moved.
        // For non-pawns, the count could be increased by promotions:
        if (CurrentPos->PieceCount(WQ) < min[WQ]) { goto Check_Promotions; }
        if (CurrentPos->PieceCount(BQ) < min[BQ]) { goto Check_Promotions; }
        if (CurrentPos->PieceCount(WR) < min[WR]) { goto Check_Promotions; }
        if (CurrentPos->PieceCount(BR) < min[BR]) { goto Check_Promotions; }
        if (CurrentPos->PieceCount(WB) < min[WB]) { goto Check_Promotions; }
        if (CurrentPos->PieceCount(BB) < min[BB]) { goto Check_Promotions; }
        if (CurrentPos->PieceCount(WN) < min[WN]) { goto Check_Promotions; }
        if (CurrentPos->PieceCount(BN) < min[BN]) { goto Check_Promotions; }
        wMinor = CurrentPos->PieceCount(WB) + CurrentPos->PieceCount(WN);
        bMinor = CurrentPos->PieceCount(BB) + CurrentPos->PieceCount(BN);
        if (wMinor < min[WM]) { goto Check_Promotions; }
        if (bMinor < min[BM]) { goto Check_Promotions; }

        // Now test maximum counts:
        if (CurrentPos->PieceCount(WQ) > max[WQ]) { goto Next_Move; }
        if (CurrentPos->PieceCount(BQ) > max[BQ]) { goto Next_Move; }
        if (CurrentPos->PieceCount(WR) > max[WR]) { goto Next_Move; }
        if (CurrentPos->PieceCount(BR) > max[BR]) { goto Next_Move; }
        if (CurrentPos->PieceCount(WB) > max[WB]) { goto Next_Move; }
        if (CurrentPos->PieceCount(BB) > max[BB]) { goto Next_Move; }
        if (CurrentPos->PieceCount(WN) > max[WN]) { goto Next_Move; }
        if (CurrentPos->PieceCount(BN) > max[BN]) { goto Next_Move; }
        if (CurrentPos->PieceCount(WP) > max[WP]) { goto Next_Move; }
        if (CurrentPos->PieceCount(BP) > max[BP]) { goto Next_Move; }
        if (wMinor > max[WM]) { goto Next_Move; }
        if (bMinor > max[BM]) { goto Next_Move; }

        // If both sides have ONE bishop, we need to check if the search
        // was restricted to same-color or opposite-color bishops:
        if (CurrentPos->PieceCount(WB) == 1
                && CurrentPos->PieceCount(BB) == 1) {
            if (!oppBishops  ||  !sameBishops) { // Check the restriction:
                colorT whiteBishCol = NOCOLOR;
                colorT blackBishCol = NOCOLOR;

                // Search for the white and black bishop, to find their
                // square color:
                const pieceT* bd = CurrentPos->GetBoard();
                for (squareT sq = A1; sq <= H8; sq++) {
                    if (bd[sq] == WB) {
                        whiteBishCol = BOARD_SQUARECOLOR [sq];
                    } else if (bd[sq] == BB) {
                        blackBishCol = BOARD_SQUARECOLOR [sq];
                    }
                }
                // They should be valid colors:
                ASSERT (blackBishCol != NOCOLOR  &&  whiteBishCol != NOCOLOR);

                // If the square colors do not match the restriction,
                // then this game cannot match:
                if (oppBishops  &&  blackBishCol == whiteBishCol) {
                    return false;
                }
                if (sameBishops  &&  blackBishCol != whiteBishCol) {
                    return false;
                }
            }
        }

        // Now check if the material difference is in-range:
        matDiff = (int)CurrentPos->MaterialValue(WHITE) -
                  (int)CurrentPos->MaterialValue(BLACK);
        if (matDiff < minDiff  ||  matDiff > maxDiff) { goto Next_Move; }

        // At this point, the Material matches; do the patterns match?
        if (ptn_size == 0 || patternsMatch(currentPos(), patterns, ptn_size)) {
            foundMatch = true;
            matchesNeeded--;
            if (matchesNeeded <= 0) { return true; }
        }
        // No? well, keep trying...
        goto Next_Move;

      Check_Promotions:
        // We only continue if this game has promotion moves:
        if (! PromotionsFlag) { return false; }

      Next_Move:
        {
            simpleMoveT sm;
            err = DecodeNextMove(&buf, sm);
            if (err == OK) {
                CurrentPos->DoSimpleMove(sm);
            }
        }
        plyCount++;
        if (! foundMatch) { matchesNeeded = matchLength; }
    }

    // End of game reached, and no match:
    return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::ExactMatch():
//      Exact position search test.
//      If sm is not NULL, its from, to, promote etc will be filled with
//      the next move at the matching position, if there is one.
//      If neverMatch is non-NULL, the boolean it points to is set to
//      true if the game could never match even with extra moves.
//
bool
Game::ExactMatch (Position * searchPos, ByteBuffer * buf,
                  gameExactMatchT searchType)
{
    // If buf is NULL, the game is in memory. Otherwise, Decode only
    // the necessary moves:
    errorT err = OK;

    if (buf == NULL) {
        MoveToStart();
    } else {
        err = DecodeSkipTags(buf);
    }

    uint search_whiteHPawns = 0;
    uint search_blackHPawns = 0;
    bool check_pawnMaskWhite, check_pawnMaskBlack;
    bool doHomePawnChecks = false;

    uint wpawnFyle [8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint bpawnFyle [8] = {0, 0, 0, 0, 0, 0, 0, 0};;

    if (searchType == GAME_EXACT_MATCH_Fyles) {
        const pieceT* board = searchPos->GetBoard();
        uint fyle = 0;
        for (squareT sq = A1; sq <= H8; sq++, board++) {
            if (*board == WP) {
                wpawnFyle[fyle]++;
            } else if (*board == BP) {
                bpawnFyle[fyle]++;
            }
            fyle = (fyle + 1) & 7;
        }
    }

    if (searchType == GAME_EXACT_MATCH_Exact  ||
        searchType == GAME_EXACT_MATCH_Pawns) {
        doHomePawnChecks = true;
        search_whiteHPawns = calcHomePawnMask (WP, searchPos->GetBoard());
        search_blackHPawns = calcHomePawnMask (BP, searchPos->GetBoard());
    }
    check_pawnMaskWhite = check_pawnMaskBlack = false;

    while (err == OK) {
        const pieceT* currentBoard = CurrentPos->GetBoard();
        const pieceT* board = searchPos->GetBoard();
        const pieceT* b1 = currentBoard;
        const pieceT* b2 = board;

        // If NO_SPEEDUPS is defined, a slower search is done without
        // optimisations that detect insufficient material.
#ifndef NO_SPEEDUPS
        // Insufficient material optimisation:
        if (searchPos->GetCount(WHITE) > CurrentPos->GetCount(WHITE)  ||
            searchPos->GetCount(BLACK) > CurrentPos->GetCount(BLACK)) {
            return false;
        }
        // Insufficient pawns optimisation:
        if (searchPos->PieceCount(WP) > CurrentPos->PieceCount(WP)  ||
            searchPos->PieceCount(BP) > CurrentPos->PieceCount(BP)) {
            return false;
        }

        // HomePawn mask optimisation:
        // If current pos doesn't have a pawn on home rank where
        // the search pos has one, it can never match.
        // This happens when (current_xxHPawns & search_xxHPawns) is
        // not equal to search_xxHPawns.
        // We do not do this optimisation for a pawn files search,
        // because the exact pawn squares are not important there.
            if (check_pawnMaskWhite) {
                auto current_whiteHPawns = calcHomePawnMask (WP, currentBoard);
                if ((current_whiteHPawns & search_whiteHPawns)
                        != search_whiteHPawns) {
                    return false;
                }
                check_pawnMaskWhite = false;
            }
            if (check_pawnMaskBlack) {
                auto current_blackHPawns = calcHomePawnMask (BP, currentBoard);
                if ((current_blackHPawns & search_blackHPawns)
                        != search_blackHPawns) {
                    return false;
                }
                check_pawnMaskBlack = false;
            }
#endif  // #ifndef NO_SPEEDUPS
        bool found = true;

        // Not correct color: skip to next move
        if (searchPos->GetToMove() != CurrentPos->GetToMove()) {
            //skip++;
            goto Move_Forward;
        }

        // Extra material: skip to next move
        if (searchPos->GetCount(WHITE) < CurrentPos->GetCount(WHITE)  ||
            searchPos->GetCount(BLACK) < CurrentPos->GetCount(BLACK)) {
            //skip++;
            goto Move_Forward;
        }
        // Extra pawns/pieces: skip to next move
        if (searchPos->PieceCount(WP) != CurrentPos->PieceCount(WP)  ||
            searchPos->PieceCount(BP) != CurrentPos->PieceCount(BP)  ||
            searchPos->PieceCount(WN) != CurrentPos->PieceCount(WN)  ||
            searchPos->PieceCount(BN) != CurrentPos->PieceCount(BN)  ||
            searchPos->PieceCount(WB) != CurrentPos->PieceCount(WB)  ||
            searchPos->PieceCount(BB) != CurrentPos->PieceCount(BB)  ||
            searchPos->PieceCount(WR) != CurrentPos->PieceCount(WR)  ||
            searchPos->PieceCount(BR) != CurrentPos->PieceCount(BR)  ||
            searchPos->PieceCount(WQ) != CurrentPos->PieceCount(WQ)  ||
            searchPos->PieceCount(BQ) != CurrentPos->PieceCount(BQ)) {
            //skip++;
            goto Move_Forward;
        }

        // NOW, compare the actual boards piece-by-piece.
        if (searchType == GAME_EXACT_MATCH_Exact) {
            if (searchPos->HashValue() == CurrentPos->HashValue()) {
                for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2) { found = false; break; }
                }
            } else {
                found = false;
            }
        } else if (searchType == GAME_EXACT_MATCH_Pawns) {
            if (searchPos->PawnHashValue() == CurrentPos->PawnHashValue()) {
                for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2  &&  (*b1 == WP  ||  *b1 == BP)) {
                        found = false;
                        break;
                    }
                }
            } else {
                found = false;
            }
        } else if (searchType == GAME_EXACT_MATCH_Fyles) {
            for (fyleT f = A_FYLE; f <= H_FYLE; f++) {
                if (searchPos->FyleCount(WP,f) != CurrentPos->FyleCount(WP,f)
                      || searchPos->FyleCount(BP,f) != CurrentPos->FyleCount(BP,f)) {
                    found = false;
                    break;
                }
            }
        } else {
            // searchType == GAME_EXACT_Match_Material, so do nothing.
        }

        if (found) {
            return true;
        }

    Move_Forward:
        if (buf == NULL) {
            err = MoveForward();
        } else {
            simpleMoveT nextMove;
            err = DecodeNextMove(buf, nextMove);
            if (err == OK) {
                CurrentPos->DoSimpleMove(nextMove);
                if (doHomePawnChecks) {
                    rankT rTo = square_Rank (nextMove.to);
                    rankT rFrom = square_Rank (nextMove.from);
                    // We only re-check the home pawn masks when something moves
                    // to or from the 2nd/7th rank:
                    if (rTo == RANK_2  ||  rFrom == RANK_2) {
                        check_pawnMaskWhite = true;
                    }
                    if (rTo == RANK_7  ||  rFrom == RANK_7) {
                        check_pawnMaskBlack = true;
                    }
                }
            }
        }
    }
    return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::VarExactMatch():
//    Like ExactMatch(), but also searches in variations.
//    This is much slower than ExactMatch(), since it will
//    search every position until a match is found.
bool
Game::VarExactMatch (Position * searchPos, gameExactMatchT searchType)
{
    uint wpawnFyle [8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint bpawnFyle [8] = {0, 0, 0, 0, 0, 0, 0, 0};;

    if (searchType == GAME_EXACT_MATCH_Fyles) {
        const pieceT* board = searchPos->GetBoard();
        uint fyle = 0;
        for (squareT sq = A1; sq <= H8; sq++, board++) {
            if (*board == WP) {
                wpawnFyle[fyle]++;
            } else if (*board == BP) {
                bpawnFyle[fyle]++;
            }
            fyle = (fyle + 1) & 7;
        }
    }

    errorT err = OK;
    while (err == OK) {
        // Check if this position matches:
        bool match = false;
        if (searchPos->GetToMove() == CurrentPos->GetToMove()
            &&  searchPos->GetCount(WHITE) == CurrentPos->GetCount(WHITE)
            &&  searchPos->GetCount(BLACK) == CurrentPos->GetCount(BLACK)
            &&  searchPos->PieceCount(WP) == CurrentPos->PieceCount(WP)
            &&  searchPos->PieceCount(BP) == CurrentPos->PieceCount(BP)
            &&  searchPos->PieceCount(WN) == CurrentPos->PieceCount(WN)
            &&  searchPos->PieceCount(BN) == CurrentPos->PieceCount(BN)
            &&  searchPos->PieceCount(WB) == CurrentPos->PieceCount(WB)
            &&  searchPos->PieceCount(BB) == CurrentPos->PieceCount(BB)
            &&  searchPos->PieceCount(WR) == CurrentPos->PieceCount(WR)
            &&  searchPos->PieceCount(BR) == CurrentPos->PieceCount(BR)
            &&  searchPos->PieceCount(WQ) == CurrentPos->PieceCount(WQ)
            &&  searchPos->PieceCount(BQ) == CurrentPos->PieceCount(BQ)) {
            match = true;
            const pieceT* b1 = CurrentPos->GetBoard();
            const pieceT* b2 = searchPos->GetBoard();
            if (searchType == GAME_EXACT_MATCH_Pawns) {
                for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2  &&  (*b1 == WP  ||  *b1 == BP)) {
                        match = false; break;
                    }
                }
            } else if (searchType == GAME_EXACT_MATCH_Fyles) {
                uint wpf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                uint bpf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                uint fyle = 0;
                for (squareT sq = A1;  sq <= H8;  sq++, b1++) {
                    if (*b1 == WP) {
                        wpf[fyle]++;
                        if (wpf[fyle] > wpawnFyle[fyle]) { match = false; break; }
                    } else if (*b1 == BP) {
                        bpf[fyle]++;
                        if (bpf[fyle] > bpawnFyle[fyle]) { match = false; break; }
                    }
                    fyle = (fyle + 1) & 7;
                }
            } else if (searchType == GAME_EXACT_MATCH_Exact) {
                if (searchPos->HashValue() == CurrentPos->HashValue()) {
                    for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                        if (*b1 != *b2) { match = false; break; }
                    }
                } else {
                    match = false;
                }
            } else {
                // searchType == GAME_EXACT_MATCH_Material, so do nothing.
            }
        }
        if (match) { return true; }

        // Now try searching each variation in turn:
        for (uint i=0; i < CurrentMove->numVariations; i++) {
            MoveIntoVariation (i);
            match = VarExactMatch (searchPos, searchType);
            MoveExitVariation();
            if (match) { return true; }
        }
        // Continue down this variation:
        MoveForward();
        if (CurrentMove->marker == END_MARKER) {
            err = ERROR_EndOfMoveList;
        }
    }
    return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetPartialMoveList():
//      Write the first few moves of a game.
//
errorT
Game::GetPartialMoveList (DString * outStr, uint plyCount)
{
    // First, copy the relevant data so we can leave the game state
    // unaltered:
    auto location = currentLocation();

    MoveToStart();
    char temp [80];
    for (uint i=0; i < plyCount; i++) {
        if (CurrentMove->marker == END_MARKER) {
            break;
        }
        if (i != 0) { outStr->Append (" "); }
        if (i == 0  ||  CurrentPos->GetToMove() == WHITE) {
            std::snprintf(temp, sizeof(temp), "%d%s", CurrentPos->GetFullMoveCount(),
                     (CurrentPos->GetToMove() == WHITE ? "." : "..."));
            outStr->Append (temp);
        }
        moveT * m = CurrentMove;
        if (m->san[0] == 0) {
            CurrentPos->MakeSANString(&(m->moveData),
                                      m->san, SAN_CHECKTEST);
        }
        // add one space for indenting to work out right
        outStr->Append (" ");
        outStr->Append (m->san);
        MoveForward();
    }

    // Now reconstruct the original game state:
    restoreLocation(location);
    return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns the SAN representation of the next move or an empty string ("") if
// not at a move.
const char* Game::GetNextSAN() {
	ASSERT(!CurrentMove->endMarker() || *CurrentMove->san == '\0');

	if (!CurrentMove->endMarker() && *CurrentMove->san == '\0') {
		CurrentPos->MakeSANString(
		    &CurrentMove->moveData, CurrentMove->san,
		    CurrentMove->next->endMarker() ? SAN_MATETEST : SAN_CHECKTEST);
	}
	return CurrentMove->san;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetSAN():
//      Print the SAN representation of the current move to a string.
//      Prints an empty string ("") if not at a move.
void Game::GetSAN(char* str) {
	ASSERT(str != NULL);
	strcpy(str, GetNextSAN());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetPrevSAN():
//      Print the SAN representation of the previous move to a string.
//      Prints an empty string ("") if not at a move.
void
Game::GetPrevSAN (char * str)
{
    ASSERT (str != NULL);
    moveT * m = CurrentMove->prev;
    if (m->startMarker()  ||  m->endMarker()) {
        str[0] = 0;
        return;
    }
    if (m->san[0] == 0) {
        MoveBackup();
        CurrentPos->MakeSANString (&(m->moveData), m->san, SAN_MATETEST);
        MoveForward();
    }
    strcpy (str, m->san);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetPrevMoveUCI():
//      Print the UCI representation of the current move to a string.
//      Prints an empty string ("") if not at a move.
void Game::GetPrevMoveUCI(char* str) const {
    ASSERT(str != NULL);
    const auto m = CurrentMove->prev;
    if (!m->startMarker())
        str = m->moveData.toLongNotation(str);

    *str = '\0';
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetNextMoveUCI():
//      Print the UCI representation of the next move to a string.
//      Prints an empty string ("") if not at a move.
void
Game::GetNextMoveUCI (char * str)
{
    ASSERT (str != NULL);
    if (!CurrentMove->endMarker())
        str = CurrentMove->moveData.toLongNotation(str);

    *str = '\0';
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

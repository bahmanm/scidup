#include "scidup/database/game.h"

#include "movetree.h"
#include "scidup/core/position.h"

#include <algorithm>

namespace scid::database {

void MoveChunkDeleter::operator()(moveT* ptr) const {
	delete[] ptr;
}

Game::~Game() = default;

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

Game* Game::clone() {
	return new Game(*this);
}

void Game::strip(bool variations, bool comments, bool NAGs) {
	while (variations && MoveExitVariation() == OK) {
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

void Game::ClearMoves() {
	if (moveChunks_.empty()) {
		moveChunkUsed_ = MOVE_CHUNKSIZE;
	} else {
		moveChunks_.erase_after(moveChunks_.begin(), moveChunks_.end());
		moveChunkUsed_ = 0;
	}
	StartPos = nullptr;
	CurrentPos->StdStart();

	FirstMove = NewMove(START_MARKER);
	CurrentMove = NewMove(END_MARKER);
	FirstMove->setNext(CurrentMove);

	VarDepth = 0;
	NumHalfMoves = 0;
}

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

} // namespace scid::database

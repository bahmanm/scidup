#include "scidup/core/movetext_cursor.h"

namespace scid::core {

MovetextCursor::MovetextCursor(Game& game)
    : game_(game), currentLine_(&game.movetext_.mainline) {}

Move* MovetextCursor::previousMove() {
	if (isAtLineStart())
		return nullptr;
	return &currentLine().moves[nextIndex_ - 1];
}

const Move* MovetextCursor::previousMove() const {
	if (isAtLineStart())
		return nullptr;
	return &currentLine().moves[nextIndex_ - 1];
}

Move* MovetextCursor::nextMove() {
	if (isAtLineEnd())
		return nullptr;
	return &currentLine().moves[nextIndex_];
}

const Move* MovetextCursor::nextMove() const {
	if (isAtLineEnd())
		return nullptr;
	return &currentLine().moves[nextIndex_];
}

std::size_t MovetextCursor::ply() const {
	std::size_t result = nextIndex_;
	for (auto const& parent : parents_)
		result += parent.nextIndex;
	return result;
}

std::size_t MovetextCursor::variationCount() const {
	auto move = nextMove();
	return move ? move->childVariations.size() : 0;
}

std::size_t MovetextCursor::variationDepth() const {
	return parents_.size();
}

std::size_t MovetextCursor::variationIndex() const {
	return parents_.empty() ? 0 : parents_.back().variationIndex;
}

bool MovetextCursor::isAtLineStart() const {
	return nextIndex_ == 0;
}

bool MovetextCursor::isAtLineEnd() const {
	return nextIndex_ == currentLine().moves.size();
}

bool MovetextCursor::isAtVariationStart() const {
	return isAtLineStart();
}

bool MovetextCursor::isAtVariationEnd() const {
	return isAtLineEnd();
}

bool MovetextCursor::isAtGameStart() const {
	return variationDepth() == 0 && isAtVariationStart();
}

bool MovetextCursor::isAtGameEnd() const {
	return variationDepth() == 0 && isAtVariationEnd();
}

bool MovetextCursor::isAtEmptyVariation() const {
	return variationDepth() != 0 && isAtVariationStart() && isAtVariationEnd();
}

bool MovetextCursor::next() {
	if (isAtLineEnd())
		return false;
	++nextIndex_;
	return true;
}

bool MovetextCursor::previous() {
	if (isAtLineStart())
		return false;
	--nextIndex_;
	return true;
}

bool MovetextCursor::enterVariation(std::size_t index) {
	auto move = nextMove();
	if (!move || index >= move->childVariations.size())
		return false;

	parents_.push_back({currentLine_, nextIndex_, index});
	currentLine_ = &move->childVariations[index].line;
	nextIndex_ = 0;
	return true;
}

bool MovetextCursor::exitVariation() {
	if (parents_.empty())
		return false;

	auto parent = parents_.back();
	parents_.pop_back();
	currentLine_ = parent.line;
	nextIndex_ = parent.nextIndex;
	return true;
}

void MovetextCursor::toStart() {
	currentLine_ = &game_.movetext_.mainline;
	parents_.clear();
	nextIndex_ = 0;
}

void MovetextCursor::toEnd() {
	currentLine_ = &game_.movetext_.mainline;
	parents_.clear();
	nextIndex_ = currentLine().moves.size();
}

bool MovetextCursor::toPly(std::size_t ply) {
	auto& mainline = game_.movetext_.mainline;
	if (ply > mainline.moves.size())
		return false;

	currentLine_ = &mainline;
	parents_.clear();
	nextIndex_ = ply;
	return true;
}

Move& MovetextCursor::addMove(MoveAction action) {
	auto& line = currentLine();
	if (nextIndex_ < line.moves.size())
		line.moves.erase(line.moves.begin() + nextIndex_, line.moves.end());

	auto& move = line.appendMove(action);
	++nextIndex_;
	return move;
}

Variation* MovetextCursor::addVariation(std::string_view initialComment) {
	auto move = nextMove();
	if (!move)
		return nullptr;

	auto& variation = move->addVariation(initialComment);
	parents_.push_back(
	    {currentLine_, nextIndex_, move->childVariations.size() - 1});
	currentLine_ = &variation.line;
	nextIndex_ = 0;
	return &variation;
}

MoveSequence& MovetextCursor::currentLine() {
	return *currentLine_;
}

const MoveSequence& MovetextCursor::currentLine() const {
	return *currentLine_;
}

} // namespace scid::core

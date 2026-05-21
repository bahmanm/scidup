#include "scidup/core/game_cursor.h"

#include <utility>

namespace scid::core {

namespace {

scid::core::Position startPosition(const Game& game) {
	return game.startPosition() ? *game.startPosition()
	                            : scid::core::Position::getStdStart();
}

} // namespace

GameCursor::GameCursor(const Game& game)
    : game_(game), currentLine_(&game.movetext().mainline) {}

const Move* GameCursor::previousMove() const {
	if (isAtLineStart())
		return nullptr;
	return &currentLine().moves[nextIndex_ - 1];
}

const Move* GameCursor::nextMove() const {
	if (isAtLineEnd())
		return nullptr;
	return &currentLine().moves[nextIndex_];
}

const Variation* GameCursor::currentVariation() const {
	if (parents_.empty())
		return nullptr;

	auto const& parent = parents_.back();
	return &parent.line->moves[parent.nextIndex]
	            .childVariations[parent.variationIndex];
}

std::vector<const Move*> GameCursor::movesToCursor() const {
	std::vector<const Move*> result;
	result.reserve(ply());

	for (auto const& parent : parents_) {
		for (std::size_t i = 0; i < parent.nextIndex; ++i)
			result.push_back(&parent.line->moves[i]);
	}
	for (std::size_t i = 0; i < nextIndex_; ++i)
		result.push_back(&currentLine().moves[i]);

	return result;
}

std::optional<scid::core::Position> GameCursor::currentPosition() const {
	auto position = startPosition(game_);
	for (const auto* move : movesToCursor()) {
		if (position.applyMove(move->action) != scid::core::OK)
			return std::nullopt;
	}
	return position;
}

std::size_t GameCursor::ply() const {
	std::size_t result = nextIndex_;
	for (auto const& parent : parents_)
		result += parent.nextIndex;
	return result;
}

std::size_t GameCursor::variationCount() const {
	auto move = nextMove();
	return move ? move->childVariations.size() : 0;
}

std::size_t GameCursor::variationDepth() const {
	return parents_.size();
}

std::size_t GameCursor::variationIndex() const {
	return parents_.empty() ? 0 : parents_.back().variationIndex;
}

bool GameCursor::isAtLineStart() const {
	return nextIndex_ == 0;
}

bool GameCursor::isAtLineEnd() const {
	return nextIndex_ == currentLine().moves.size();
}

bool GameCursor::isAtVariationStart() const {
	return isAtLineStart();
}

bool GameCursor::isAtVariationEnd() const {
	return isAtLineEnd();
}

bool GameCursor::isAtGameStart() const {
	return variationDepth() == 0 && isAtVariationStart();
}

bool GameCursor::isAtGameEnd() const {
	return variationDepth() == 0 && isAtVariationEnd();
}

bool GameCursor::isAtEmptyVariation() const {
	return variationDepth() != 0 && isAtVariationStart() && isAtVariationEnd();
}

bool GameCursor::next() {
	if (isAtLineEnd())
		return false;
	++nextIndex_;
	return true;
}

bool GameCursor::previous() {
	if (isAtLineStart())
		return false;
	--nextIndex_;
	return true;
}

bool GameCursor::enterVariation(std::size_t index) {
	auto move = nextMove();
	if (!move || index >= move->childVariations.size())
		return false;

	parents_.push_back({currentLine_, nextIndex_, index});
	currentLine_ = &move->childVariations[index].line;
	nextIndex_ = 0;
	return true;
}

bool GameCursor::exitVariation() {
	if (parents_.empty())
		return false;

	auto parent = parents_.back();
	parents_.pop_back();
	currentLine_ = parent.line;
	nextIndex_ = parent.nextIndex;
	return true;
}

void GameCursor::toStart() {
	currentLine_ = &game_.movetext().mainline;
	parents_.clear();
	nextIndex_ = 0;
}

void GameCursor::toEnd() {
	currentLine_ = &game_.movetext().mainline;
	parents_.clear();
	nextIndex_ = currentLine().moves.size();
}

bool GameCursor::toPly(std::size_t ply) {
	auto const& mainline = game_.movetext().mainline;
	if (ply > mainline.moves.size())
		return false;

	currentLine_ = &mainline;
	parents_.clear();
	nextIndex_ = ply;
	return true;
}

MovetextLocation GameCursor::location() const {
	std::vector<MovetextLocation::Step> path;
	path.reserve(parents_.size());
	for (auto const& parent : parents_)
		path.push_back({parent.nextIndex, parent.variationIndex});
	return MovetextLocation(std::move(path), nextIndex_);
}

bool GameCursor::restore(MovetextLocation location) {
	auto line = &game_.movetext().mainline;
	std::vector<ParentFrame> parents;
	parents.reserve(location.path_.size());
	for (auto const& step : location.path_) {
		if (step.nextIndex >= line->moves.size())
			return false;
		auto const& move = line->moves[step.nextIndex];
		if (step.variationIndex >= move.childVariations.size())
			return false;

		parents.push_back({line, step.nextIndex, step.variationIndex});
		line = &move.childVariations[step.variationIndex].line;
	}

	if (location.nextIndex_ > line->moves.size())
		return false;

	currentLine_ = line;
	parents_ = std::move(parents);
	nextIndex_ = location.nextIndex_;
	return true;
}

const MoveSequence& GameCursor::currentLine() const {
	return *currentLine_;
}

} // namespace scid::core

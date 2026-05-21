#include "scidup/core/movetext_cursor.h"

#include <algorithm>
#include <utility>

namespace scid::core {

namespace {

constexpr std::size_t MAX_NAGS_PER_MOVE = 8;

bool isMoveNagValue(std::uint8_t nag) {
	return nag >= 1 && nag <= 6;
}

bool isPositionNagValue(std::uint8_t nag) {
	return nag >= 10 && nag <= 21;
}

} // namespace

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

Variation* MovetextCursor::currentVariation() {
	if (parents_.empty())
		return nullptr;

	auto const& parent = parents_.back();
	return &parent.line->moves[parent.nextIndex]
	            .childVariations[parent.variationIndex];
}

const Variation* MovetextCursor::currentVariation() const {
	if (parents_.empty())
		return nullptr;

	auto const& parent = parents_.back();
	return &parent.line->moves[parent.nextIndex]
	            .childVariations[parent.variationIndex];
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

MovetextLocation MovetextCursor::location() const {
	std::vector<MovetextLocation::Step> path;
	path.reserve(parents_.size());
	for (auto const& parent : parents_)
		path.push_back({parent.nextIndex, parent.variationIndex});
	return MovetextLocation(std::move(path), nextIndex_);
}

bool MovetextCursor::restore(MovetextLocation location) {
	auto* line = &game_.movetext_.mainline;
	std::vector<ParentFrame> parents;
	parents.reserve(location.path_.size());
	for (auto const& step : location.path_) {
		if (step.nextIndex >= line->moves.size())
			return false;
		auto& move = line->moves[step.nextIndex];
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

Move& MovetextCursor::addMove(MoveSpec action) {
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

bool MovetextCursor::setPreviousMoveMetadata(MoveMetadata metadata) {
	auto move = previousMove();
	if (!move)
		return false;

	move->metadata = std::move(metadata);
	return true;
}

bool MovetextCursor::setPreviousMoveSan(std::string_view san) {
	auto move = previousMove();
	if (!move)
		return false;

	move->san.assign(san.begin(), san.end());
	return true;
}

bool MovetextCursor::setNextMoveSan(std::string_view san) {
	auto move = nextMove();
	if (!move)
		return false;

	move->san.assign(san.begin(), san.end());
	return true;
}

bool MovetextCursor::setCurrentVariationInitialComment(
    std::string_view comment) {
	auto variation = currentVariation();
	if (!variation)
		return false;

	variation->initialComment.assign(comment.begin(), comment.end());
	return true;
}

bool MovetextCursor::setComment(std::string_view comment) {
	if (isAtLineStart()) {
		if (variationDepth() == 0) {
			game_.setInitialComment(comment);
			return true;
		}
		return setCurrentVariationInitialComment(comment);
	}

	auto move = previousMove();
	if (!move)
		return false;

	move->metadata.comment.assign(comment.begin(), comment.end());
	return true;
}

bool MovetextCursor::addPreviousMoveNag(std::uint8_t nag) {
	auto move = previousMove();
	if (!move)
		return true;

	auto& nags = move->metadata.nags;
	if (nags.size() + 1 >= MAX_NAGS_PER_MOVE)
		return false;
	if (nag == 0)
		return true;

	if (isMoveNagValue(nag)) {
		for (auto& existingNag : nags) {
			if (isMoveNagValue(existingNag)) {
				existingNag = nag;
				return true;
			}
		}
		nags.insert(nags.begin(), nag);
		return true;
	}

	if (isPositionNagValue(nag)) {
		for (auto& existingNag : nags) {
			if (isPositionNagValue(existingNag)) {
				existingNag = nag;
				return true;
			}
		}
	}

	nags.push_back(nag);
	return true;
}

bool MovetextCursor::removePreviousMoveNag(bool moveNag) {
	auto move = previousMove();
	if (!move)
		return true;

	auto& nags = move->metadata.nags;
	auto match = [moveNag](std::uint8_t nag) {
		return moveNag ? isMoveNagValue(nag) : isPositionNagValue(nag);
	};
	auto it = std::find_if(nags.begin(), nags.end(), match);
	if (it != nags.end())
		nags.erase(it);
	return true;
}

void MovetextCursor::clearPreviousMoveNags() {
	if (auto move = previousMove())
		move->metadata.nags.clear();
}

bool MovetextCursor::promoteVariationToFirst() {
	if (parents_.empty())
		return false;

	auto& parent = parents_.back();
	if (parent.nextIndex >= parent.line->moves.size())
		return false;

	auto& variations = parent.line->moves[parent.nextIndex].childVariations;
	if (parent.variationIndex >= variations.size())
		return false;

	if (parent.variationIndex != 0) {
		auto variation = std::move(variations[parent.variationIndex]);
		variations.erase(variations.begin() + parent.variationIndex);
		variations.insert(variations.begin(), std::move(variation));
		parent.variationIndex = 0;
		currentLine_ = &variations.front().line;
	}
	return true;
}

bool MovetextCursor::promoteVariationToMainline() {
	if (parents_.empty())
		return false;

	auto parent = parents_.back();
	if (parent.nextIndex >= parent.line->moves.size())
		return false;

	auto& parentMoves = parent.line->moves;
	auto& rootMove = parentMoves[parent.nextIndex];
	if (parent.variationIndex >= rootMove.childVariations.size())
		return false;
	if (rootMove.childVariations[parent.variationIndex].line.moves.empty())
		return true;

	auto selectedVariation =
	    std::move(rootMove.childVariations[parent.variationIndex]);
	rootMove.childVariations.erase(rootMove.childVariations.begin() +
	                               parent.variationIndex);
	auto siblingVariations = std::move(rootMove.childVariations);
	auto promotedMoves = std::move(selectedVariation.line.moves);
	auto promotedFirstChildren = std::move(promotedMoves.front().childVariations);

	std::vector<Move> demotedMainlineMoves;
	demotedMainlineMoves.reserve(parentMoves.size() - parent.nextIndex);
	auto demotedRootMove = std::move(rootMove);
	demotedRootMove.childVariations = std::move(promotedFirstChildren);
	demotedMainlineMoves.push_back(std::move(demotedRootMove));
	for (auto i = parent.nextIndex + 1; i < parentMoves.size(); ++i)
		demotedMainlineMoves.push_back(std::move(parentMoves[i]));

	Variation demotedMainline;
	demotedMainline.initialComment =
	    std::move(selectedVariation.initialComment);
	demotedMainline.line.moves = std::move(demotedMainlineMoves);

	std::vector<Variation> promotedFirstVariations;
	promotedFirstVariations.reserve(siblingVariations.size() + 1);
	promotedFirstVariations.push_back(std::move(demotedMainline));
	for (auto& variation : siblingVariations)
		promotedFirstVariations.push_back(std::move(variation));
	promotedMoves.front().childVariations = std::move(promotedFirstVariations);

	std::vector<Move> newParentMoves;
	newParentMoves.reserve(parent.nextIndex + promotedMoves.size());
	for (std::size_t i = 0; i < parent.nextIndex; ++i)
		newParentMoves.push_back(std::move(parentMoves[i]));
	for (auto& move : promotedMoves)
		newParentMoves.push_back(std::move(move));

	parent.line->moves = std::move(newParentMoves);
	currentLine_ = parent.line;
	nextIndex_ = parent.nextIndex + nextIndex_;
	parents_.pop_back();
	return true;
}

bool MovetextCursor::deleteVariation() {
	if (parents_.empty())
		return false;

	auto parent = parents_.back();
	if (parent.nextIndex >= parent.line->moves.size())
		return false;

	auto& variations = parent.line->moves[parent.nextIndex].childVariations;
	if (parent.variationIndex >= variations.size())
		return false;

	currentLine_ = parent.line;
	nextIndex_ = parent.nextIndex;
	parents_.pop_back();
	variations.erase(variations.begin() + parent.variationIndex);
	return true;
}

void MovetextCursor::truncate() {
	auto& line = currentLine();
	line.moves.erase(line.moves.begin() + nextIndex_, line.moves.end());
}

void MovetextCursor::truncateBeforeCursor() {
	auto& line = currentLine();
	std::vector<Move> suffix;
	suffix.reserve(line.moves.size() - nextIndex_);
	for (auto i = nextIndex_; i < line.moves.size(); ++i)
		suffix.push_back(std::move(line.moves[i]));

	game_.movetext_.mainline.moves = std::move(suffix);
	currentLine_ = &game_.movetext_.mainline;
	parents_.clear();
	nextIndex_ = 0;
}

MoveSequence& MovetextCursor::currentLine() {
	return *currentLine_;
}

const MoveSequence& MovetextCursor::currentLine() const {
	return *currentLine_;
}

} // namespace scid::core

#include "movetree.h"
#include "scidup/database/game.h"

#include <algorithm>
#include <cstring>

namespace scid::database {
namespace {

constexpr int MOVE_CHUNKSIZE = 128;

simpleMoveT toLegacyMove(Position& position,
                         scid::core::MoveAction const& action) {
	simpleMoveT move = {};
	if (action.isNull()) {
		position.makeMove(action.from, action.to, PAWN, move);
		return move;
	}
	if (action.castling) {
		position.makeMove(action.from, action.from,
		                  action.to > action.from ? KING : QUEEN, move);
		return move;
	}

	const auto notation = action.longNotation();
	if (position.ReadCoordMove(&move, notation.data(), notation.size(),
	                           false) == OK) {
		return move;
	}

	move.from = action.from;
	move.to = action.to;
	move.promote = action.promotion;
	position.fillMove(move);
	return move;
}

void copySan(std::string const& source, sanStringT& dest) {
	std::fill_n(dest, sizeof(dest), '\0');
	std::memcpy(dest, source.data(),
	            std::min(source.size(), sizeof(dest) - 1));
}

void copyMetadata(scid::core::Move const& source, moveT& dest) {
	copySan(source.san, dest.san);
	dest.comment = source.metadata.comment;
	dest.nagCount = static_cast<byte>(
	    std::min<std::size_t>(source.metadata.nags.size(), MAX_NAGS - 1));
	std::fill_n(dest.nags, sizeof(dest.nags), 0);
	std::copy_n(source.metadata.nags.begin(), dest.nagCount, dest.nags);
}

} // namespace

void Game::TEMP_syncLegacyMovetextFromCore() {
	const auto location = coreLocation_;
	moveChunks_.clear();
	moveChunkUsed_ = MOVE_CHUNKSIZE;

	firstMove_ = newMove(START_MARKER);
	firstMove_->comment = std::string(coreGame_.initialComment());

	auto copyLegacyLine = [this](auto& copyLegacyLine,
	                             scid::core::MoveSequence const& source,
	                             moveT& lineStart,
	                             Position& position) -> void {
		auto* tail = &lineStart;
		for (auto const& sourceMove : source.moves) {
			auto* destMove = newMove(NO_MARKER);
			destMove->moveData = toLegacyMove(position, sourceMove.action);
			copyMetadata(sourceMove, *destMove);
			tail->setNext(destMove);

			for (auto const& variation : sourceMove.childVariations) {
				auto* variationStart = newMove(START_MARKER);
				variationStart->comment = variation.initialComment;
				destMove->appendChild(variationStart);

				auto variationPosition = position;
				copyLegacyLine(copyLegacyLine, variation.line, *variationStart,
				               variationPosition);
			}

			position.DoSimpleMove(destMove->moveData);
			tail = destMove;
		}
		tail->setNext(newMove(END_MARKER));
	};

	auto position = coreGame_.startPosition()
	                    ? *coreGame_.startPosition()
	                    : Position::getStdStart();
	copyLegacyLine(copyLegacyLine, coreGame_.movetext().mainline, *firstMove_,
	               position);
	numHalfMoves_ = static_cast<ushort>(coreGame_.mainlineHalfMoveCount());

	auto advanceLegacyLocation = [this](std::size_t count) {
		for (std::size_t i = 0; i < count; ++i) {
			if (!currentMove_ || currentMove_->endMarker())
				return false;
			currentPos_->DoSimpleMove(currentMove_->moveData);
			currentMove_ = currentMove_->next;
		}
		return true;
	};

	auto enterLegacyVariation = [this](std::size_t variationIndex) {
		if (!currentMove_ || currentMove_->startMarker() ||
		    currentMove_->endMarker()) {
			return false;
		}

		auto* variation = currentMove_->varChild;
		for (std::size_t i = 0; variation && i < variationIndex; ++i)
			variation = variation->varChild;
		if (!variation)
			return false;

		currentMove_ = variation->next;
		++varDepth_;
		return true;
	};

	if (auto startPosition = coreGame_.startPosition()) {
		*currentPos_ = *startPosition;
	} else {
		currentPos_->StdStart();
	}
	currentMove_ = firstMove_->next;
	varDepth_ = 0;

	bool restored = true;
	for (auto const& step : location.path()) {
		restored = advanceLegacyLocation(step.nextIndex) &&
		           enterLegacyVariation(step.variationIndex);
		if (!restored)
			break;
	}
	restored = restored && advanceLegacyLocation(location.nextIndex());
	ASSERT(restored);
}

} // namespace scid::database

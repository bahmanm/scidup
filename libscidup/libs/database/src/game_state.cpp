#include "scidup/database/game.h"

#include "movetree.h"

namespace scid::database {

Game::GameSavedPos Game::currentLocation() const {
	return {*currentPos_, coreLocation_};
}

void Game::restoreLocation(const GameSavedPos& savedPos) {
	*currentPos_ = savedPos.pos;
	coreLocation_ = savedPos.coreLocation;
	[[maybe_unused]] const bool restored =
	    TEMP_restoreLegacyCursorFromCoreLocation(coreLocation_);
	ASSERT(restored);
}

bool Game::TEMP_restoreLegacyCursorFromCoreLocation(
    scid::core::MovetextLocation location) {
	auto advanceLegacyLocation = [this](std::size_t count) {
		for (std::size_t i = 0; i < count; ++i) {
			if (!currentMove_ || currentMove_->endMarker())
				return false;
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

	currentMove_ = firstMove_->next;
	varDepth_ = 0;

	for (auto const& step : location.path()) {
		if (!advanceLegacyLocation(step.nextIndex) ||
		    !enterLegacyVariation(step.variationIndex)) {
			return false;
		}
	}
	return advanceLegacyLocation(location.nextIndex());
}

Position* Game::currentPos() {
	return currentPos_.get();
}

const Position* Game::currentPos() const {
	return currentPos_.get();
}

simpleMoveT* Game::currentMove() {
	// TODO [Game]: Keep this legacy pointer until callers stop depending on
	// position-filled simpleMoveT fields that are not present in MoveAction.
	return currentMove_->endMarker() ? nullptr : &currentMove_->moveData;
}

} // namespace scid::database

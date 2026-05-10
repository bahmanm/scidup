#include "scidup/database/game.h"

#include "scidup/core/movetext_cursor.h"
#include "scidup/database/common.h"
#include "movetree.h"

#include <algorithm>

namespace scid::database {

namespace {

bool isMoveNagValue(byte nag) {
	return nag >= 1 && nag <= 6;
}

bool isPositionNagValue(byte nag) {
	return nag >= 10 && nag <= 21;
}

} // namespace

// TODO [Game]: Move NAG/comment storage behind Move.metadata once the core
// Move shape exists. These methods write core metadata first, then rebuild the
// legacy moveT cache for compatibility readers.
void Game::clearNags() {
	scid::core::MovetextCursor cursor(coreGame_);
	[[maybe_unused]] const bool restored = cursor.restore(coreLocation_);
	ASSERT(restored);
	if (auto* move = cursor.previousMove()) {
		move->metadata.nags.clear();
		TEMP_syncLegacyMovetextFromCore();
	}
}

errorT Game::addNag (byte nag) {
	scid::core::MovetextCursor cursor(coreGame_);
	[[maybe_unused]] const bool restored = cursor.restore(coreLocation_);
	ASSERT(restored);
	auto* move = cursor.previousMove();
	if (!move)
		return OK;
	auto& nags = move->metadata.nags;

    if (nags.size() + 1 >= MAX_NAGS) { return ERROR_GameFull; }
    if (nag == 0) { /* Nags cannot be zero! */ return OK; }
	// If it is a move nag replace an existing
	if( nag >= 1 && nag <= 6)
		for(auto& existingNag : nags)
			if(isMoveNagValue(existingNag))
			{
				existingNag = nag;
				TEMP_syncLegacyMovetextFromCore();
				return OK;
			}
	// If it is a position nag replace an existing
	if( nag >= 10 && nag <= 21)
		for(auto& existingNag : nags)
			if(isPositionNagValue(existingNag))
			{
				existingNag = nag;
				TEMP_syncLegacyMovetextFromCore();
				return OK;
			}
	if( nag >= 1 && nag <= 6)
	{
		// Put Move Nags at the beginning
		nags.insert(nags.begin(), nag);
	}
	else
		nags.push_back(nag);
	TEMP_syncLegacyMovetextFromCore();
    return OK;
}

errorT Game::removeNag (bool isMoveNag) {
	scid::core::MovetextCursor cursor(coreGame_);
	[[maybe_unused]] const bool restored = cursor.restore(coreLocation_);
	ASSERT(restored);
	auto* move = cursor.previousMove();
	if (!move)
		return OK;

	auto& nags = move->metadata.nags;
	auto match = [isMoveNag](byte nag) {
		return isMoveNag ? isMoveNagValue(nag)
		                 : isPositionNagValue(nag);
	};
	auto it = std::find_if(nags.begin(), nags.end(), match);
	if (it != nags.end()) {
		nags.erase(it);
		TEMP_syncLegacyMovetextFromCore();
	}
    return OK;
}


void Game::setMoveComment(const char* comment) {
	const std::string_view value = comment ? std::string_view(comment)
	                                       : std::string_view();
	scid::core::MovetextCursor cursor(coreGame_);
	[[maybe_unused]] const bool restored = cursor.restore(coreLocation_);
	ASSERT(restored);
	if (cursor.isAtLineStart()) {
		if (cursor.variationDepth() == 0) {
			coreGame_.setInitialComment(value);
		} else {
			[[maybe_unused]] const bool updated =
			    cursor.setCurrentVariationInitialComment(value);
			ASSERT(updated);
		}
	} else {
		auto* move = cursor.previousMove();
		ASSERT(move);
		move->metadata.comment.assign(value.begin(), value.end());
	}
	TEMP_syncLegacyMovetextFromCore();
}
} // namespace scid::database

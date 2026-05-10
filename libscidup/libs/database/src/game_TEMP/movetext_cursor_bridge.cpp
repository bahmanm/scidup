#include "movetext_cursor_bridge.h"

#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_cursor.h"
#include "movetree.h"

#include <cstddef>
#include <vector>

namespace scid::database {
namespace {

struct LegacyMovetextStep {
	std::size_t nextIndex = 0;
	std::size_t variationIndex = 0;
};

bool findLegacyMovetextLocation(const moveT* lineStart,
                                const moveT* target,
                                std::vector<LegacyMovetextStep>& path,
                                std::size_t& nextIndex) {
	if (lineStart == target) {
		nextIndex = 0;
		return true;
	}

	std::size_t lineIndex = 0;
	for (auto move = lineStart->next; move; move = move->next) {
		if (move == target) {
			nextIndex = lineIndex;
			return true;
		}
		if (move->endMarker())
			return false;

		std::size_t variationIndex = 0;
		for (auto variation = move->varChild; variation;
		     variation = variation->varChild, ++variationIndex) {
			std::vector<LegacyMovetextStep> childPath;
			std::size_t childNextIndex = 0;
			if (findLegacyMovetextLocation(variation, target, childPath,
			                               childNextIndex)) {
				path.push_back({lineIndex, variationIndex});
				path.insert(path.end(), childPath.begin(), childPath.end());
				nextIndex = childNextIndex;
				return true;
			}
		}

		++lineIndex;
	}
	return false;
}

} // namespace

bool TEMP_movetext::moveCursorToLegacyLocation(
    scid::core::MovetextCursor& cursor,
    const moveT* lineStart,
    const moveT* target) {
	std::vector<LegacyMovetextStep> path;
	std::size_t nextIndex = 0;
	if (!findLegacyMovetextLocation(lineStart, target, path, nextIndex))
		return false;

	cursor.toStart();
	for (auto const& step : path) {
		for (std::size_t i = 0; i < step.nextIndex; ++i) {
			if (!cursor.next())
				return false;
		}
		if (!cursor.enterVariation(step.variationIndex))
			return false;
	}
	for (std::size_t i = 0; i < nextIndex; ++i) {
		if (!cursor.next())
			return false;
	}
	return true;
}

bool TEMP_movetext::moveCursorToLegacyLocation(
    scid::core::GameCursor& cursor,
    const moveT* lineStart,
    const moveT* target) {
	std::vector<LegacyMovetextStep> path;
	std::size_t nextIndex = 0;
	if (!findLegacyMovetextLocation(lineStart, target, path, nextIndex))
		return false;

	cursor.toStart();
	for (auto const& step : path) {
		for (std::size_t i = 0; i < step.nextIndex; ++i) {
			if (!cursor.next())
				return false;
		}
		if (!cursor.enterVariation(step.variationIndex))
			return false;
	}
	for (std::size_t i = 0; i < nextIndex; ++i) {
		if (!cursor.next())
			return false;
	}
	return true;
}

} // namespace scid::database

#pragma once

#include "scidup/core/game.h"

#include <cstddef>
#include <vector>

namespace scid::core {

class GameCursor {
private:
	struct LocationStep {
		bool operator==(const LocationStep&) const = default;

		std::size_t nextIndex = 0;
		std::size_t variationIndex = 0;
	};

	struct ParentFrame {
		const MoveSequence* line = nullptr;
		std::size_t nextIndex = 0;
		std::size_t variationIndex = 0;
	};

public:
	class Location {
	public:
		bool operator==(const Location&) const = default;

	private:
		friend class GameCursor;

		Location(std::vector<LocationStep> path, std::size_t nextIndex);

		std::vector<LocationStep> path_;
		std::size_t nextIndex_ = 0;
	};

	explicit GameCursor(const Game& game);

	const Move* previousMove() const;
	const Move* nextMove() const;
	std::size_t ply() const;
	std::size_t variationCount() const;
	std::size_t variationDepth() const;
	std::size_t variationIndex() const;

	bool isAtStart() const;
	bool isAtEnd() const;
	bool isAtVariationStart() const;
	bool isAtVariationEnd() const;
	bool isAtGameStart() const;
	bool isAtGameEnd() const;
	bool isAtEmptyVariation() const;

	bool next();
	bool previous();
	bool enterVariation(std::size_t index);
	bool exitVariation();
	void toStart();
	void toEnd();
	bool toPly(std::size_t ply);

	Location location() const;
	bool restore(Location location);

private:
	const MoveSequence& currentLine() const;

	const Game& game_;
	const MoveSequence* currentLine_ = nullptr;
	std::size_t nextIndex_ = 0;
	std::vector<ParentFrame> parents_;
};

} // namespace scid::core

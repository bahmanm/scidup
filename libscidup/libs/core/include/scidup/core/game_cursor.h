#pragma once

#include "scidup/core/game.h"
#include "scidup/core/movetext_location.h"
#include "scidup/core/position.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace scid::core {

class GameCursor {
private:
	struct ParentFrame {
		const MoveSequence* line = nullptr;
		std::size_t nextIndex = 0;
		std::size_t variationIndex = 0;
	};

public:
	explicit GameCursor(const Game& game);

	const Move* previousMove() const;
	const Move* nextMove() const;
	const Variation* currentVariation() const;
	std::vector<const Move*> movesToCursor() const;
	std::optional<scid::database::Position> currentPosition() const;
	std::size_t ply() const;
	std::size_t variationCount() const;
	std::size_t variationDepth() const;
	std::size_t variationIndex() const;

	bool isAtLineStart() const;
	bool isAtLineEnd() const;
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

	MovetextLocation location() const;
	bool restore(MovetextLocation location);

private:
	const MoveSequence& currentLine() const;

	const Game& game_;
	const MoveSequence* currentLine_ = nullptr;
	std::size_t nextIndex_ = 0;
	std::vector<ParentFrame> parents_;
};

} // namespace scid::core

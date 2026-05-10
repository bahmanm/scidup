#pragma once

#include "scidup/core/game.h"
#include "scidup/core/movetext_location.h"

#include <cstddef>
#include <vector>

namespace scid::core {

class MovetextCursor {
private:
	struct ParentFrame {
		MoveSequence* line = nullptr;
		std::size_t nextIndex = 0;
		std::size_t variationIndex = 0;
	};

public:
	explicit MovetextCursor(Game& game);

	Move* previousMove();
	const Move* previousMove() const;
	Move* nextMove();
	const Move* nextMove() const;
	Variation* currentVariation();
	const Variation* currentVariation() const;
	std::size_t ply() const;
	std::size_t variationCount() const;
	std::size_t variationDepth() const;
	std::size_t variationIndex() const;
	MovetextLocation location() const;
	bool restore(MovetextLocation location);

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

	Move& addMove(MoveAction action);
	Variation* addVariation(std::string_view initialComment = {});

private:
	MoveSequence& currentLine();
	const MoveSequence& currentLine() const;

	Game& game_;
	MoveSequence* currentLine_ = nullptr;
	std::size_t nextIndex_ = 0;
	std::vector<ParentFrame> parents_;
};

} // namespace scid::core

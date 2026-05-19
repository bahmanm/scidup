#pragma once

#include "scidup/core/game_cursor.h"
#include "scidup/core/notation.h"
#include "scidup/core/pgn/traversal.h"
#include "scidup/database/game.h"

#include <cstdint>
#include <string>
#include <vector>

namespace scid::database::gamepos {

struct GamePos {
	uint32_t RAVdepth;
	uint32_t RAVnum;
	std::string FEN; // "Forsyth-Edwards Notation" describing the position.
	std::vector<int> NAGs;   // "Numeric Annotation Glyph"
	std::string comment;     // text annotation of the position.
	std::string lastMoveSAN; // move that was played to reach the position.
};

/**
 * Iterate all the positions of a game and store the corresponding GamePos
 * objects into a container.
 *
 * The order of positions of Recursive Annotation Variations (RAV) follows the
 * PGN standard: "The alternate move sequence given by an RAV is one that may
 * be legally played by first unplaying the move that appears immediately prior
 * to the RAV. Because the RAV is a recursive construct, it may be nested"
 * Each position has a RAVdepth and a RAVnum that allows following a variation
 * from any given position:
 * - skip all the next positions with a bigger RAVdepth
 * - the variation ends with:
 *   - a lower RAVdepth or
 *   - an equal RAVdepth but different RAVnum or
 *   - the end of @e dest
 * @param game: reference to the Game object where the positions are read.
 * @param dest: the container where the GamePos objects are appended.
 */
template <typename TCont>
inline void collectPositions(Game& game, TCont& dest) {
	scid::core::GameCursor cursor(game.coreGame());
	do {
		if (cursor.isAtVariationStart() && !cursor.isAtGameStart())
			continue;

		dest.emplace_back();
		auto& gamepos = dest.back();
		char strBuf[256];
		auto position = cursor.currentPosition();
		ASSERT(position);
		position->PrintFEN(strBuf, sizeof(strBuf));
		gamepos.FEN = strBuf;
		gamepos.RAVdepth = cursor.variationDepth();
		gamepos.RAVnum = cursor.variationIndex();
		if (auto move = cursor.previousMove()) {
			for (auto nag : move->metadata.nags)
				gamepos.NAGs.push_back(nag);
			gamepos.comment = move->metadata.comment;
		} else if (auto variation = cursor.currentVariation()) {
			gamepos.comment = variation->initialComment;
		} else {
			gamepos.comment = game.coreGame().movetext().initialComment;
		}
		gamepos.lastMoveSAN = scid::core::notation::previousSan(
		    game.coreGame(), cursor.location());

	} while (scid::core::pgn::nextLocation(cursor));
}

/**
 * Returns all the positions of a game
 * @param game: reference to the Game object where the positions are read.
 * @returns a std::vector containing the GamePos objects corresponding to all
 * the positions of @e game.
 */
inline std::vector<GamePos> collectPositions(Game& game) {
	std::vector<GamePos> res;
	collectPositions(game, res);
	return res;
}

} // namespace scid::database::gamepos

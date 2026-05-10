#pragma once

#include "scidup/core/game_cursor.h"
#include "scidup/core/notation.h"
#include "scidup/database/game.h"

#include <cstdint>
#include <string>
#include <vector>

namespace scid::database::gamepos {

// TODO [Game]: Rebuild this snapshot helper on top of the future GameCursor
// plus the PGN/export traversal adapter. It currently depends on legacy
// PGN-order cursor methods and should not be moved until those boundaries exist.
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
	do {
		if (game.isAtVariationStart() && !game.isAtStart())
			continue;

		dest.emplace_back();
		auto& gamepos = dest.back();
		char strBuf[256];
		game.currentPos()->PrintFEN(strBuf, sizeof(strBuf));
		gamepos.FEN = strBuf;
		scid::core::GameCursor cursor(game.coreGame());
		[[maybe_unused]] const bool restored = cursor.restore(
		    game.coreLocation());
		ASSERT(restored);
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
		    game.coreGame(), game.coreLocation());

	} while (game.nextPgn() == OK);
}

/**
 * Returns all the positions of a game
 * @param game: reference to the Game object where the positions are read.
 * @returns a std::vector containing the GamePos objects corresponding to all
 * the positions of @e game.
 */
inline std::vector<GamePos> collectPositions(Game& game) {
	std::vector<GamePos> res;
	game.toStart();
	collectPositions(game, res);
	return res;
}

} // namespace scid::database::gamepos

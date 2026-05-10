#include "scidup/core/notation.h"

#include "scidup/core/game.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/movelist.h"
#include "scidup/core/movetext_location.h"
#include "scidup/core/position.h"

#include <cassert>
#include <vector>

namespace scid::core::notation {
namespace {

scid::database::simpleMoveT toSimpleMove(scid::database::Position& position,
                                         MoveAction action) {
	scid::database::simpleMoveT move;
	if (action.isNull()) {
		position.makeMove(action.from, action.to, scid::database::PAWN, move);
		return move;
	}

	const auto notation = action.longNotation();
	[[maybe_unused]] const auto err =
	    position.ReadCoordMove(&move, notation.data(), notation.size(), false);
	assert(err == scid::database::OK);
	return move;
}

} // namespace

std::string currentPositionUci(const Game& game, MovetextLocation location) {
	char fen[256] = {};
	std::vector<std::string> moves;
	scid::database::Position position =
	    game.startPosition() ? *game.startPosition()
	                         : scid::database::Position::getStdStart();

	GameCursor cursor(game);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	assert(restored);

	for (const auto* move : cursor.movesToCursor()) {
		auto simpleMove = toSimpleMove(position, move->action);
		position.DoSimpleMove(simpleMove);
		if (move->action.isNull()) {
			position.PrintFEN(fen, sizeof(fen));
			moves.clear();
		} else {
			moves.push_back(move->action.longNotation());
		}
	}

	std::string res = "position ";
	if (*fen || game.hasNonStandardStart(fen, sizeof(fen))) {
		res += "fen ";
		res += fen;
	} else {
		res += "startpos";
	}
	res += " moves";
	for (auto const& move : moves) {
		res.push_back(' ');
		res += move;
	}
	return res;
}

} // namespace scid::core::notation

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

GameCursor cursorAt(const Game& game, MovetextLocation location) {
	GameCursor cursor(game);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	assert(restored);
	return cursor;
}

scid::database::Position startPosition(const Game& game) {
	return game.startPosition() ? *game.startPosition()
	                            : scid::database::Position::getStdStart();
}

scid::database::Position positionAfter(
    const Game& game,
    const std::vector<const Move*>& moves,
    std::size_t count) {
	auto position = startPosition(game);
	for (std::size_t i = 0; i < count; ++i) {
		auto simpleMove = toSimpleMove(position, moves[i]->action);
		position.DoSimpleMove(simpleMove);
	}
	return position;
}

std::string makeSan(scid::database::Position& position,
                    const Move& move,
                    scid::database::sanFlagT flag) {
	if (!move.san.empty())
		return move.san;

	scid::database::sanStringT san = {};
	auto simpleMove = toSimpleMove(position, move.action);
	position.MakeSANString(&simpleMove, san, flag);
	return san;
}

std::string currentPositionUci(const Game& game, MovetextLocation location) {
	char fen[256] = {};
	std::vector<std::string> moves;
	auto position = startPosition(game);

	auto cursor = cursorAt(game, location);

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

std::string previousMoveUci(const Game& game, MovetextLocation location) {
	auto cursor = cursorAt(game, location);
	const auto move = cursor.previousMove();
	if (!move)
		return {};
	return move->action.longNotation();
}

std::string nextMoveUci(const Game& game, MovetextLocation location) {
	auto cursor = cursorAt(game, location);
	const auto move = cursor.nextMove();
	if (!move)
		return {};
	return move->action.longNotation();
}

std::string previousSan(const Game& game, MovetextLocation location) {
	auto cursor = cursorAt(game, location);
	auto moves = cursor.movesToCursor();
	if (moves.empty())
		return {};

	auto position = positionAfter(game, moves, moves.size() - 1);
	return makeSan(position, *moves.back(), scid::database::SAN_MATETEST);
}

std::string nextSan(const Game& game, MovetextLocation location) {
	auto cursor = cursorAt(game, location);
	const auto move = cursor.nextMove();
	if (!move)
		return {};

	auto moves = cursor.movesToCursor();
	auto position = positionAfter(game, moves, moves.size());
	auto afterMove = cursor;
	[[maybe_unused]] const bool advanced = afterMove.next();
	assert(advanced);
	const auto flag = afterMove.isAtLineEnd() ? scid::database::SAN_MATETEST
	                                          : scid::database::SAN_CHECKTEST;
	return makeSan(position, *move, flag);
}

} // namespace scid::core::notation

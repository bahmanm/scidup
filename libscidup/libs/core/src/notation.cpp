#include "scidup/core/notation.h"

#include "scidup/core/game.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/movelist.h"
#include "scidup/core/movetext_location.h"
#include "scidup/core/position.h"

#include <cassert>
#include <optional>
#include <vector>

namespace scid::core::notation {

std::optional<scid::core::simpleMoveT>
toSimpleMove(scid::core::Position& position, MoveAction const& action) {
	scid::core::simpleMoveT move;
	if (action.isNull()) {
		position.makeMove(action.from, action.to, scid::core::PAWN, move);
		return move;
	}
	if (action.castling) {
		position.makeMove(action.from, action.from,
		                  action.to > action.from ? scid::core::KING
		                                          : scid::core::QUEEN,
		                  move);
		return move;
	}

	const auto notation = action.longNotation();
	const auto err =
	    position.ReadCoordMove(&move, notation.data(), notation.size(), false);
	if (err != scid::core::OK)
		return std::nullopt;
	return move;
}

GameCursor cursorAt(const Game& game, MovetextLocation location) {
	GameCursor cursor(game);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	assert(restored);
	return cursor;
}

scid::core::Position startPosition(const Game& game) {
	return game.startPosition() ? *game.startPosition()
	                            : scid::core::Position::getStdStart();
}

std::optional<scid::core::Position> positionAfter(
    const Game& game,
    const std::vector<const Move*>& moves,
    std::size_t count) {
	auto position = startPosition(game);
	for (std::size_t i = 0; i < count; ++i) {
		auto simpleMove = toSimpleMove(position, moves[i]->action);
		if (!simpleMove)
			return {};
		position.DoSimpleMove(*simpleMove);
	}
	return position;
}

std::string makeSan(scid::core::Position& position,
                    const Move& move,
                    scid::core::sanFlagT flag) {
	if (!move.san.empty())
		return move.san;

	scid::core::sanStringT san = {};
	auto simpleMove = toSimpleMove(position, move.action);
	if (!simpleMove)
		return {};
	position.MakeSANString(&*simpleMove, san, flag);
	return san;
}

std::string currentPositionUci(const Game& game, MovetextLocation location) {
	char fen[256] = {};
	std::vector<std::string> moves;
	auto position = startPosition(game);

	auto cursor = cursorAt(game, location);

	for (const auto* move : cursor.movesToCursor()) {
		auto simpleMove = toSimpleMove(position, move->action);
		assert(simpleMove);
		position.DoSimpleMove(*simpleMove);
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
	if (!position)
		return {};
	return makeSan(*position, *moves.back(), scid::core::SAN_MATETEST);
}

std::string nextSan(const Game& game, MovetextLocation location) {
	auto cursor = cursorAt(game, location);
	const auto move = cursor.nextMove();
	if (!move)
		return {};

	auto moves = cursor.movesToCursor();
	auto position = positionAfter(game, moves, moves.size());
	if (!position)
		return {};
	auto afterMove = cursor;
	[[maybe_unused]] const bool advanced = afterMove.next();
	assert(advanced);
	const auto flag = afterMove.isAtLineEnd() ? scid::core::SAN_MATETEST
	                                          : scid::core::SAN_CHECKTEST;
	return makeSan(*position, *move, flag);
}

std::string partialMoveList(const Game& game, std::size_t plyCount) {
	std::string out;
	auto position = startPosition(game);
	GameCursor cursor(game);

	for (std::size_t i = 0; i < plyCount && !cursor.isAtLineEnd(); ++i) {
		const auto* move = cursor.nextMove();
		assert(move);

		std::string entry;
		if (i == 0 || position.GetToMove() == scid::core::WHITE) {
			entry += std::to_string(position.GetFullMoveCount());
			entry += position.GetToMove() == scid::core::WHITE ? "." : "...";
			entry.push_back(' ');
		}

		auto afterMove = cursor;
		[[maybe_unused]] const bool advanced = afterMove.next();
		assert(advanced);
		const auto flag = afterMove.isAtLineEnd()
		                      ? scid::core::SAN_MATETEST
		                      : scid::core::SAN_CHECKTEST;
		const auto san = makeSan(position, *move, flag);
		if (san.empty())
			break;
		entry += san;

		if (!out.empty())
			out.push_back(' ');
		out += entry;

		auto simpleMove = toSimpleMove(position, move->action);
		if (!simpleMove)
			break;
		position.DoSimpleMove(*simpleMove);
		cursor.next();
	}

	return out;
}

} // namespace scid::core::notation

#include "scidup/database/game.h"

#include "scidup/database/common.h"
#include "scidup/database/game_TEMP/notation.h"
#include "scidup/core/dstring.h"
#include "scidup/core/notation.h"
#include "scidup/core/position.h"
#include "movetree.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace scid::database {

namespace {

scid::core::MoveAction TEMP_moveActionFromLegacyMove(simpleMoveT const& move) {
	return {move.from, move.to, move.promote};
}

template <typename OutputIt>
OutputIt TEMP_copyLongNotation(OutputIt dest, simpleMoveT const& move) {
	const auto notation = TEMP_moveActionFromLegacyMove(move).longNotation();
	return std::copy(notation.begin(), notation.end(), dest);
}

} // namespace

std::string game_notation::currentPositionUci(const Game& game) {
	// TODO [Game]: Move UCI position rendering to a notation helper over
	// GameCursor.
	std::string res = "position startpos moves";
	char FEN[256] = {};

	std::vector<const moveT*> moves;
	const moveT* move = game.CurrentMove;
	while ((move = move->getPrevMove())) {
		if (move->moveData.isNullMove()) {
			Position lastValidPos = *game.currentPos();
				for (const moveT* m : moves) {
					lastValidPos.UndoSimpleMove(m->moveData);
				}
				lastValidPos.PrintFEN(FEN, sizeof(FEN));
				break;
			}
		moves.emplace_back(move);
	}

		if (*FEN || game.HasNonStandardStart(FEN, sizeof(FEN))) {
			res.replace(9, 4, "fen ");
			res.replace(13, 4, FEN);
		}

	const auto allocSpeedup = res.size();
	res.resize(allocSpeedup + moves.size() * 6);
	auto it = res.data() + allocSpeedup;
	for (auto m = moves.crbegin(), end = moves.crend(); m != end; ++m) {
		*it++ = ' ';
		it = TEMP_copyLongNotation(it, (*m)->moveData);
	}
	res.resize(std::distance(res.data(), it)); // shrink
	return res;
}


errorT game_notation::writePartialMoveList(Game& game, DString& out,
                                           uint plyCount) {
    // TODO [Game]: Rebuild this UI compatibility helper on GameCursor plus SAN
    // notation once cursor traversal is no longer stored directly on Game.
    // First, copy the relevant data so we can leave the game state
    // unaltered:
    auto location = game.currentLocation();

    game.MoveToStart();
    char temp [80];
    for (uint i=0; i < plyCount; i++) {
        if (game.AtEnd()) {
            break;
        }
        const auto* pos = game.currentPos();
        if (i != 0) { out.Append (" "); }
        if (i == 0  ||  pos->GetToMove() == WHITE) {
            std::snprintf(temp, sizeof(temp), "%d%s", pos->GetFullMoveCount(),
                     (pos->GetToMove() == WHITE ? "." : "..."));
            out.Append (temp);
        }
        // add one space for indenting to work out right
        out.Append (" ");
        out.Append (game_notation::nextSan(game).c_str());
        game.MoveForward();
    }

    // Now reconstruct the original game state:
    game.restoreLocation(location);
    return OK;
}

std::string game_notation::nextSan(Game& game) {
	// TODO [Game]: Move SAN generation/caching to notation helpers and
	// Move.metadata once Move owns SAN and GameCursor owns the current position.
	ASSERT(!game.CurrentMove->endMarker() || *game.CurrentMove->san == '\0');

	if (!game.CurrentMove->endMarker() && *game.CurrentMove->san == '\0') {
		game.CurrentPos->MakeSANString(
		    &game.CurrentMove->moveData, game.CurrentMove->san,
		    game.CurrentMove->next->endMarker() ? SAN_MATETEST : SAN_CHECKTEST);
		game.TEMP_syncCoreMovetext();
	}
	return game.CurrentMove->san;
}

std::string game_notation::previousSan(Game& game) {
    // TODO [Game]: Move SAN generation/caching to notation helpers and
    // Move.metadata once Move owns SAN and GameCursor owns the current position.
    moveT * m = game.CurrentMove->prev;
    if (m->startMarker()  ||  m->endMarker()) {
        return {};
    }
    if (m->san[0] == 0) {
        game.MoveBackup();
        game.CurrentPos->MakeSANString (&(m->moveData), m->san, SAN_MATETEST);
        game.MoveForward();
        game.TEMP_syncCoreMovetext();
    }
    return m->san;
}

std::string game_notation::previousMoveUci(const Game& game) {
	// TODO [Game]: Move UCI move rendering to a notation helper over
	// MoveAction once GameCursor owns previous/next move traversal.
	const auto move = game.CurrentMove->prev;
	if (move->startMarker()) {
		return {};
	}
	return TEMP_moveActionFromLegacyMove(move->moveData).longNotation();
}

std::string game_notation::nextMoveUci(const Game& game) {
	// TODO [Game]: Move UCI move rendering to a notation helper over
	// MoveAction once GameCursor owns previous/next move traversal.
	const auto move = game.CurrentMove;
	if (move->endMarker()) {
		return {};
	}
	return TEMP_moveActionFromLegacyMove(move->moveData).longNotation();
}

} // namespace scid::database

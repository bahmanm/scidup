#include "scidup/database/game.h"

#include "scidup/database/common.h"
#include "scidup/core/dstring.h"
#include "scidup/core/notation.h"
#include "scidup/core/position.h"
#include "movetree.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace scid::database {

std::string Game::currentPosUCI() const {
	std::string res = "position startpos moves";
	char FEN[256] = {};

	std::vector<const moveT*> moves;
	const moveT* move = CurrentMove;
	while ((move = move->getPrevMove())) {
		if (move->moveData.isNullMove()) {
			Position lastValidPos = *currentPos();
				for (const moveT* m : moves) {
					lastValidPos.UndoSimpleMove(m->moveData);
				}
				lastValidPos.PrintFEN(FEN, sizeof(FEN));
				break;
			}
		moves.emplace_back(move);
	}

		if (*FEN || HasNonStandardStart(FEN, sizeof(FEN))) {
			res.replace(9, 4, "fen ");
			res.replace(13, 4, FEN);
		}

	const auto allocSpeedup = res.size();
	res.resize(allocSpeedup + moves.size() * 6);
	auto it = res.data() + allocSpeedup;
	for (auto m = moves.crbegin(), end = moves.crend(); m != end; ++m) {
		*it++ = ' ';
		it = (*m)->moveData.toLongNotation(it);
	}
	res.resize(std::distance(res.data(), it)); // shrink
	return res;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetPartialMoveList():
//      Write the first few moves of a game.
//
errorT
Game::GetPartialMoveList (DString * outStr, uint plyCount)
{
    // First, copy the relevant data so we can leave the game state
    // unaltered:
    auto location = currentLocation();

    MoveToStart();
    char temp [80];
    for (uint i=0; i < plyCount; i++) {
        if (CurrentMove->marker == END_MARKER) {
            break;
        }
        if (i != 0) { outStr->Append (" "); }
        if (i == 0  ||  CurrentPos->GetToMove() == WHITE) {
            std::snprintf(temp, sizeof(temp), "%d%s", CurrentPos->GetFullMoveCount(),
                     (CurrentPos->GetToMove() == WHITE ? "." : "..."));
            outStr->Append (temp);
        }
        moveT * m = CurrentMove;
        if (m->san[0] == 0) {
            CurrentPos->MakeSANString(&(m->moveData),
                                      m->san, SAN_CHECKTEST);
        }
        // add one space for indenting to work out right
        outStr->Append (" ");
        outStr->Append (m->san);
        MoveForward();
    }

    // Now reconstruct the original game state:
    restoreLocation(location);
    return OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Returns the SAN representation of the next move or an empty string ("") if
// not at a move.
const char* Game::GetNextSAN() {
	ASSERT(!CurrentMove->endMarker() || *CurrentMove->san == '\0');

	if (!CurrentMove->endMarker() && *CurrentMove->san == '\0') {
		CurrentPos->MakeSANString(
		    &CurrentMove->moveData, CurrentMove->san,
		    CurrentMove->next->endMarker() ? SAN_MATETEST : SAN_CHECKTEST);
	}
	return CurrentMove->san;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetSAN():
//      Print the SAN representation of the current move to a string.
//      Prints an empty string ("") if not at a move.
void Game::GetSAN(char* str) {
	ASSERT(str != NULL);
	strcpy(str, GetNextSAN());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetPrevSAN():
//      Print the SAN representation of the previous move to a string.
//      Prints an empty string ("") if not at a move.
void
Game::GetPrevSAN (char * str)
{
    ASSERT (str != NULL);
    moveT * m = CurrentMove->prev;
    if (m->startMarker()  ||  m->endMarker()) {
        str[0] = 0;
        return;
    }
    if (m->san[0] == 0) {
        MoveBackup();
        CurrentPos->MakeSANString (&(m->moveData), m->san, SAN_MATETEST);
        MoveForward();
    }
    strcpy (str, m->san);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetPrevMoveUCI():
//      Print the UCI representation of the current move to a string.
//      Prints an empty string ("") if not at a move.
void Game::GetPrevMoveUCI(char* str) const {
    ASSERT(str != NULL);
    const auto m = CurrentMove->prev;
    if (!m->startMarker())
        str = m->moveData.toLongNotation(str);

    *str = '\0';
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::GetNextMoveUCI():
//      Print the UCI representation of the next move to a string.
//      Prints an empty string ("") if not at a move.
void
Game::GetNextMoveUCI (char * str)
{
    ASSERT (str != NULL);
    if (!CurrentMove->endMarker())
        str = CurrentMove->moveData.toLongNotation(str);

    *str = '\0';
}

} // namespace scid::database

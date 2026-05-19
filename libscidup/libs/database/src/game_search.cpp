#include "scidup/database/game.h"

#include "scidup/database/bytebuf.h"
#include "scidup/database/common.h"
#include "game_search.h"
#include "game_storage.h"

#include <array>
#include <cstddef>

namespace scid::database {

namespace {
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// calcHomePawnMask():
//      Computes the homePawn mask for a position.
//
int calcHomePawnMask (pieceT pawn, const pieceT* board)
{
    ASSERT (pawn == WP  ||  pawn == BP);
    const pieceT* bd = &(board[ (pawn == WP ? H2 : H7) ]);
    int result = 0;
    if (*bd == pawn) { result |= 128; }  bd--;   // H-fyle pawn
    if (*bd == pawn) { result |=  64; }  bd--;   // G-fyle pawn
    if (*bd == pawn) { result |=  32; }  bd--;   // F-fyle pawn
    if (*bd == pawn) { result |=  16; }  bd--;   // E-fyle pawn
    if (*bd == pawn) { result |=   8; }  bd--;   // D-fyle pawn
    if (*bd == pawn) { result |=   4; }  bd--;   // C-fyle pawn
    if (*bd == pawn) { result |=   2; }  bd--;   // B-fyle pawn
    if (*bd == pawn) { result |=   1; }          // A-fyle pawn
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// patternsMatch():
//      Used by materialMatch() to test patterns.
//      Returns true if all the patterns in the list match.
//
bool patternsMatch(const Position* pos, patternT* patterns, size_t patternCount) {
    const pieceT* board = pos->GetBoard();
    for (auto pattern = patterns, patternEnd = patterns + patternCount;
         pattern != patternEnd; ++pattern) {
        if (pattern->rankMatch == NO_RANK) {

            if (pattern->fyleMatch == NO_FYLE) { // Nothing to test!
            } else {  // Test this fyle:
                squareT sq = square_Make (pattern->fyleMatch, RANK_1);
                bool found = false;
                for (uint i=0; i < 8; i++, sq += 8) {
                    if (board[sq] == pattern->pieceMatch) { found = true; break; }
                }
                if (found != pattern->flag) { return false; }
            }

        } else { // rankMatch is a rank from 1 to 8:

            if (pattern->fyleMatch == NO_FYLE) { // Test the whole rank:
                bool found = false;
                squareT sq = square_Make (A_FYLE, pattern->rankMatch);
                for (uint i=0; i < 8; i++, sq++) {
                    if (board[sq] == pattern->pieceMatch) { found = true; break; }
                }
                if (found != pattern->flag) { return false; }
            } else {  // Just test one square:
                squareT sq = square_Make(pattern->fyleMatch, pattern->rankMatch);
                bool found = false;
                if (board[sq] == pattern->pieceMatch) { found = true; }
                if (found != pattern->flag) { return false; }
            }
        }
    }

    // If we reach here, all patterns matched:
    return true;
}

errorT decodeSearchStart(ByteBuffer& buf, Position& position) {
    errorT err = buf.decodeTags([](auto, auto) {});
    if (err != OK)
        return err;

    auto [errStartPos, fen] = buf.decodeStartBoard();
    if (errStartPos)
        return errStartPos;

    if (fen)
        return position.ReadFromFEN(fen);

    position.StdStart();
    return OK;
}

simpleMoveT toSimpleMove(Position& position,
                         scid::core::MoveAction const& action) {
    simpleMoveT move = {};
    if (action.isNull()) {
        position.makeMove(action.from, action.to, PAWN, move);
        return move;
    }
    if (action.castling) {
        position.makeMove(action.from, action.from,
                          action.to > action.from ? KING : QUEEN, move);
        return move;
    }

    const auto notation = action.longNotation();
    if (position.ReadCoordMove(&move, notation.data(), notation.size(),
                               false) == OK) {
        return move;
    }

    move.from = action.from;
    move.to = action.to;
    move.promote = action.promotion;
    position.fillMove(move);
    return move;
}

std::array<uint, 8> pawnFylesFor(const Position& position, pieceT pawn) {
    std::array<uint, 8> result = {};
    const pieceT* board = position.GetBoard();
    uint fyle = 0;
    for (squareT sq = A1; sq <= H8; sq++, board++) {
        if (*board == pawn)
            result[fyle]++;
        fyle = (fyle + 1) & 7;
    }
    return result;
}

bool fyleCountsMatch(const Position& position,
                     const std::array<uint, 8>& whitePawnFyles,
                     const std::array<uint, 8>& blackPawnFyles) {
    auto whiteCurrent = pawnFylesFor(position, WP);
    auto blackCurrent = pawnFylesFor(position, BP);
    for (fyleT fyle = A_FYLE; fyle <= H_FYLE; ++fyle) {
        if (whiteCurrent[fyle] > whitePawnFyles[fyle] ||
            blackCurrent[fyle] > blackPawnFyles[fyle]) {
            return false;
        }
    }
    return true;
}

bool positionMatches(Position* searchPos,
                     Position& currentPosition,
                     gameExactMatchT searchType,
                     const std::array<uint, 8>& whitePawnFyles,
                     const std::array<uint, 8>& blackPawnFyles) {
    if (searchPos->GetToMove() != currentPosition.GetToMove()
        || searchPos->GetCount(WHITE) != currentPosition.GetCount(WHITE)
        || searchPos->GetCount(BLACK) != currentPosition.GetCount(BLACK)
        || searchPos->PieceCount(WP) != currentPosition.PieceCount(WP)
        || searchPos->PieceCount(BP) != currentPosition.PieceCount(BP)
        || searchPos->PieceCount(WN) != currentPosition.PieceCount(WN)
        || searchPos->PieceCount(BN) != currentPosition.PieceCount(BN)
        || searchPos->PieceCount(WB) != currentPosition.PieceCount(WB)
        || searchPos->PieceCount(BB) != currentPosition.PieceCount(BB)
        || searchPos->PieceCount(WR) != currentPosition.PieceCount(WR)
        || searchPos->PieceCount(BR) != currentPosition.PieceCount(BR)
        || searchPos->PieceCount(WQ) != currentPosition.PieceCount(WQ)
        || searchPos->PieceCount(BQ) != currentPosition.PieceCount(BQ)) {
        return false;
    }

    const pieceT* currentBoard = currentPosition.GetBoard();
    const pieceT* searchBoard = searchPos->GetBoard();
    if (searchType == GAME_EXACT_MATCH_Pawns) {
        for (squareT sq = A1; sq <= H8; sq++, currentBoard++, searchBoard++) {
            if (*currentBoard != *searchBoard &&
                (*currentBoard == WP || *currentBoard == BP)) {
                return false;
            }
        }
        return true;
    }
    if (searchType == GAME_EXACT_MATCH_Fyles) {
        return fyleCountsMatch(currentPosition, whitePawnFyles,
                               blackPawnFyles);
    }
    if (searchType == GAME_EXACT_MATCH_Exact) {
        if (searchPos->HashValue() != currentPosition.HashValue())
            return false;
        for (squareT sq = A1; sq <= H8; sq++, currentBoard++, searchBoard++) {
            if (*currentBoard != *searchBoard)
                return false;
        }
    }
    return true;
}

bool varExactMatchLine(scid::core::MoveSequence const& line,
                       Position currentPosition,
                       Position* searchPos,
                       gameExactMatchT searchType,
                       const std::array<uint, 8>& whitePawnFyles,
                       const std::array<uint, 8>& blackPawnFyles) {
    for (auto const& move : line.moves) {
        if (positionMatches(searchPos, currentPosition, searchType,
                            whitePawnFyles, blackPawnFyles)) {
            return true;
        }

        for (auto const& variation : move.childVariations) {
            if (varExactMatchLine(variation.line, currentPosition, searchPos,
                                  searchType, whitePawnFyles,
                                  blackPawnFyles)) {
                return true;
            }
        }

        auto simpleMove = toSimpleMove(currentPosition, move.action);
        currentPosition.DoSimpleMove(simpleMove);
    }

    return line.moves.empty() &&
           positionMatches(searchPos, currentPosition, searchType,
                           whitePawnFyles, blackPawnFyles);
}

Position startPositionFor(const scid::core::Game& game) {
    return game.startPosition() ? *game.startPosition()
                                : Position::getStdStart();
}

bool materialMatches(bool promotionsFlag, ByteBuffer& buf, byte* min,
                     byte* max, patternT* patterns, size_t patternCount,
                     int minPly, int maxPly, int matchLength,
                     bool oppBishops, bool sameBishops, int minDiff,
                     int maxDiff) {
    ASSERT (matchLength >= 1);

    int matchesNeeded = matchLength;
    int matDiff;
    uint plyCount = 0;
    Position currentPosition;
    errorT err = decodeSearchStart(buf, currentPosition);
    while (err == OK) {
        bool foundMatch = false;
        byte wMinor, bMinor;

        // If current pos has LESS than the minimum of pawns, this
        // game can never match so return false;
        if (currentPosition.PieceCount(WP) < min[WP]) { return false; }
        if (currentPosition.PieceCount(BP) < min[BP]) { return false; }

        // If not in the valid move range, go to the next move or return:
        if ((int)plyCount > maxPly) { return false; }
        if ((int)plyCount < minPly) { goto Next_Move; }

// For these comparisons, we really could only do half of them each move,
// according to which side just moved.
        // For non-pawns, the count could be increased by promotions:
        if (currentPosition.PieceCount(WQ) < min[WQ]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(BQ) < min[BQ]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(WR) < min[WR]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(BR) < min[BR]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(WB) < min[WB]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(BB) < min[BB]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(WN) < min[WN]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(BN) < min[BN]) { goto Check_Promotions; }
        wMinor = currentPosition.PieceCount(WB) + currentPosition.PieceCount(WN);
        bMinor = currentPosition.PieceCount(BB) + currentPosition.PieceCount(BN);
        if (wMinor < min[WM]) { goto Check_Promotions; }
        if (bMinor < min[BM]) { goto Check_Promotions; }

        // Now test maximum counts:
        if (currentPosition.PieceCount(WQ) > max[WQ]) { goto Next_Move; }
        if (currentPosition.PieceCount(BQ) > max[BQ]) { goto Next_Move; }
        if (currentPosition.PieceCount(WR) > max[WR]) { goto Next_Move; }
        if (currentPosition.PieceCount(BR) > max[BR]) { goto Next_Move; }
        if (currentPosition.PieceCount(WB) > max[WB]) { goto Next_Move; }
        if (currentPosition.PieceCount(BB) > max[BB]) { goto Next_Move; }
        if (currentPosition.PieceCount(WN) > max[WN]) { goto Next_Move; }
        if (currentPosition.PieceCount(BN) > max[BN]) { goto Next_Move; }
        if (currentPosition.PieceCount(WP) > max[WP]) { goto Next_Move; }
        if (currentPosition.PieceCount(BP) > max[BP]) { goto Next_Move; }
        if (wMinor > max[WM]) { goto Next_Move; }
        if (bMinor > max[BM]) { goto Next_Move; }

        // If both sides have ONE bishop, we need to check if the search
        // was restricted to same-color or opposite-color bishops:
        if (currentPosition.PieceCount(WB) == 1
                && currentPosition.PieceCount(BB) == 1) {
            if (!oppBishops  ||  !sameBishops) { // Check the restriction:
                colorT whiteBishCol = NOCOLOR;
                colorT blackBishCol = NOCOLOR;

                // Search for the white and black bishop, to find their
                // square color:
                const pieceT* bd = currentPosition.GetBoard();
                for (squareT sq = A1; sq <= H8; sq++) {
                    if (bd[sq] == WB) {
                        whiteBishCol = BOARD_SQUARECOLOR [sq];
                    } else if (bd[sq] == BB) {
                        blackBishCol = BOARD_SQUARECOLOR [sq];
                    }
                }
                // They should be valid colors:
                ASSERT (blackBishCol != NOCOLOR  &&  whiteBishCol != NOCOLOR);

                // If the square colors do not match the restriction,
                // then this game cannot match:
                if (oppBishops  &&  blackBishCol == whiteBishCol) {
                    return false;
                }
                if (sameBishops  &&  blackBishCol != whiteBishCol) {
                    return false;
                }
            }
        }

        // Now check if the material difference is in-range:
        matDiff = (int)currentPosition.MaterialValue(WHITE) -
                  (int)currentPosition.MaterialValue(BLACK);
        if (matDiff < minDiff  ||  matDiff > maxDiff) { goto Next_Move; }

        // At this point, the Material matches; do the patterns match?
        if (patternCount == 0 ||
            patternsMatch(&currentPosition, patterns, patternCount)) {
            foundMatch = true;
            matchesNeeded--;
            if (matchesNeeded <= 0) { return true; }
        }
        // No? well, keep trying...
        goto Next_Move;

      Check_Promotions:
        // We only continue if this game has promotion moves:
        if (!promotionsFlag) { return false; }

      Next_Move:
        {
            simpleMoveT sm;
            err = game_storage::decodeMainlineMove(buf, currentPosition, sm);
            if (err == OK) {
                currentPosition.DoSimpleMove(sm);
            }
        }
        plyCount++;
        if (! foundMatch) { matchesNeeded = matchLength; }
    }

    // End of game reached, and no match:
    return false;
}

bool exactMatches(const scid::core::Game& game, Position* searchPos,
                  ByteBuffer* buf, gameExactMatchT searchType) {
    // If buf is NULL, the game is in memory. Otherwise, decode only
    // the necessary moves:
    errorT err = OK;
    Position decodedPosition;
    Position* currentPosition = &decodedPosition;
    const scid::core::MoveSequence* memoryLine = nullptr;
    std::size_t memoryMoveIndex = 0;

    if (buf == NULL) {
        decodedPosition = startPositionFor(game);
        memoryLine = &game.movetext().mainline;
    } else {
        err = decodeSearchStart(*buf, decodedPosition);
    }

    uint search_whiteHPawns = 0;
    uint search_blackHPawns = 0;
    bool check_pawnMaskWhite, check_pawnMaskBlack;
    bool doHomePawnChecks = false;

    uint wpawnFyle [8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint bpawnFyle [8] = {0, 0, 0, 0, 0, 0, 0, 0};;

    if (searchType == GAME_EXACT_MATCH_Fyles) {
        const pieceT* board = searchPos->GetBoard();
        uint fyle = 0;
        for (squareT sq = A1; sq <= H8; sq++, board++) {
            if (*board == WP) {
                wpawnFyle[fyle]++;
            } else if (*board == BP) {
                bpawnFyle[fyle]++;
            }
            fyle = (fyle + 1) & 7;
        }
    }

    if (searchType == GAME_EXACT_MATCH_Exact  ||
        searchType == GAME_EXACT_MATCH_Pawns) {
        doHomePawnChecks = true;
        search_whiteHPawns = calcHomePawnMask (WP, searchPos->GetBoard());
        search_blackHPawns = calcHomePawnMask (BP, searchPos->GetBoard());
    }
    check_pawnMaskWhite = check_pawnMaskBlack = false;

    while (err == OK) {
        const pieceT* currentBoard = currentPosition->GetBoard();
        const pieceT* board = searchPos->GetBoard();
        const pieceT* b1 = currentBoard;
        const pieceT* b2 = board;

        // If NO_SPEEDUPS is defined, a slower search is done without
        // optimisations that detect insufficient material.
#ifndef NO_SPEEDUPS
        // Insufficient material optimisation:
        if (searchPos->GetCount(WHITE) > currentPosition->GetCount(WHITE)  ||
            searchPos->GetCount(BLACK) > currentPosition->GetCount(BLACK)) {
            return false;
        }
        // Insufficient pawns optimisation:
        if (searchPos->PieceCount(WP) > currentPosition->PieceCount(WP)  ||
            searchPos->PieceCount(BP) > currentPosition->PieceCount(BP)) {
            return false;
        }

        // HomePawn mask optimisation:
        // If current pos doesn't have a pawn on home rank where
        // the search pos has one, it can never match.
        // This happens when (current_xxHPawns & search_xxHPawns) is
        // not equal to search_xxHPawns.
        // We do not do this optimisation for a pawn files search,
        // because the exact pawn squares are not important there.
            if (check_pawnMaskWhite) {
                auto current_whiteHPawns = calcHomePawnMask (WP, currentBoard);
                if ((current_whiteHPawns & search_whiteHPawns)
                        != search_whiteHPawns) {
                    return false;
                }
                check_pawnMaskWhite = false;
            }
            if (check_pawnMaskBlack) {
                auto current_blackHPawns = calcHomePawnMask (BP, currentBoard);
                if ((current_blackHPawns & search_blackHPawns)
                        != search_blackHPawns) {
                    return false;
                }
                check_pawnMaskBlack = false;
            }
#endif  // #ifndef NO_SPEEDUPS
        bool found = true;

        // Not correct color: skip to next move
        if (searchPos->GetToMove() != currentPosition->GetToMove()) {
            //skip++;
            goto Move_Forward;
        }

        // Extra material: skip to next move
        if (searchPos->GetCount(WHITE) < currentPosition->GetCount(WHITE)  ||
            searchPos->GetCount(BLACK) < currentPosition->GetCount(BLACK)) {
            //skip++;
            goto Move_Forward;
        }
        // Extra pawns/pieces: skip to next move
        if (searchPos->PieceCount(WP) != currentPosition->PieceCount(WP)  ||
            searchPos->PieceCount(BP) != currentPosition->PieceCount(BP)  ||
            searchPos->PieceCount(WN) != currentPosition->PieceCount(WN)  ||
            searchPos->PieceCount(BN) != currentPosition->PieceCount(BN)  ||
            searchPos->PieceCount(WB) != currentPosition->PieceCount(WB)  ||
            searchPos->PieceCount(BB) != currentPosition->PieceCount(BB)  ||
            searchPos->PieceCount(WR) != currentPosition->PieceCount(WR)  ||
            searchPos->PieceCount(BR) != currentPosition->PieceCount(BR)  ||
            searchPos->PieceCount(WQ) != currentPosition->PieceCount(WQ)  ||
            searchPos->PieceCount(BQ) != currentPosition->PieceCount(BQ)) {
            //skip++;
            goto Move_Forward;
        }

        // NOW, compare the actual boards piece-by-piece.
        if (searchType == GAME_EXACT_MATCH_Exact) {
            if (searchPos->HashValue() == currentPosition->HashValue()) {
                for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2) { found = false; break; }
                }
            } else {
                found = false;
            }
        } else if (searchType == GAME_EXACT_MATCH_Pawns) {
            if (searchPos->PawnHashValue() == currentPosition->PawnHashValue()) {
                for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2  &&  (*b1 == WP  ||  *b1 == BP)) {
                        found = false;
                        break;
                    }
                }
            } else {
                found = false;
            }
        } else if (searchType == GAME_EXACT_MATCH_Fyles) {
            for (fyleT f = A_FYLE; f <= H_FYLE; f++) {
                if (searchPos->FyleCount(WP,f) != currentPosition->FyleCount(WP,f)
                      || searchPos->FyleCount(BP,f) != currentPosition->FyleCount(BP,f)) {
                    found = false;
                    break;
                }
            }
        } else {
            // searchType == GAME_EXACT_Match_Material, so do nothing.
        }

        if (found) {
            return true;
        }

    Move_Forward:
        {
            simpleMoveT nextMove;
            if (buf == NULL) {
                if (memoryLine == nullptr ||
                    memoryMoveIndex >= memoryLine->moves.size()) {
                    err = ERROR_EndOfMoveList;
                } else {
                    nextMove = toSimpleMove(
                        *currentPosition,
                        memoryLine->moves[memoryMoveIndex].action);
                    memoryMoveIndex++;
                    err = OK;
                }
            } else {
                err = game_storage::decodeMainlineMove(*buf, *currentPosition,
                                                       nextMove);
            }

            if (err == OK) {
                currentPosition->DoSimpleMove(nextMove);
                if (doHomePawnChecks) {
                    rankT rTo = square_Rank (nextMove.to);
                    rankT rFrom = square_Rank (nextMove.from);
                    // We only re-check the home pawn masks when something moves
                    // to or from the 2nd/7th rank:
                    if (rTo == RANK_2  ||  rFrom == RANK_2) {
                        check_pawnMaskWhite = true;
                    }
                    if (rTo == RANK_7  ||  rFrom == RANK_7) {
                        check_pawnMaskBlack = true;
                    }
                }
            }
        }
    }
    return false;
}

} // end of anonymous namespace

bool game_search::materialMatch([[maybe_unused]] Game& game, bool promotionsFlag,
                                ByteBuffer& buf, byte* min, byte* max,
                                patternT* patterns, std::size_t patternCount,
                                int minPly, int maxPly, int matchLength,
                                bool oppBishops, bool sameBishops, int minDiff,
                                int maxDiff) {
    return materialMatches(promotionsFlag, buf, min, max, patterns,
                           patternCount, minPly, maxPly, matchLength,
                           oppBishops, sameBishops, minDiff, maxDiff);
}

bool game_search::exactMatch(Game& game, Position* pos, ByteBuffer* buf,
                             gameExactMatchT searchType) {
    return exactMatches(game.coreGame(), pos, buf, searchType);
}

bool game_search::varExactMatch(Game& game, Position* pos,
                                gameExactMatchT searchType) {
    const auto whitePawnFyles = searchType == GAME_EXACT_MATCH_Fyles
                                    ? pawnFylesFor(*pos, WP)
                                    : std::array<uint, 8>{};
    const auto blackPawnFyles = searchType == GAME_EXACT_MATCH_Fyles
                                    ? pawnFylesFor(*pos, BP)
                                    : std::array<uint, 8>{};
    auto const& coreGame = game.coreGame();
    Position startPosition = startPositionFor(coreGame);
    return varExactMatchLine(coreGame.movetext().mainline, startPosition, pos,
                             searchType, whitePawnFyles, blackPawnFyles);
}

} // namespace scid::database

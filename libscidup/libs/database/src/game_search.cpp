#include "bytebuf.h"
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
int calcHomePawnMask (scid::core::pieceT pawn, const scid::core::pieceT* board)
{
    ASSERT (pawn == scid::core::WP  ||  pawn == scid::core::BP);
    const scid::core::pieceT* bd = &(board[ (pawn == scid::core::WP ? scid::core::H2 : scid::core::H7) ]);
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
bool patternsMatch(const scid::core::Position* pos, patternT* patterns, size_t patternCount) {
    const scid::core::pieceT* board = pos->GetBoard();
    for (auto pattern = patterns, patternEnd = patterns + patternCount;
         pattern != patternEnd; ++pattern) {
        if (pattern->rankMatch == scid::core::NO_RANK) {

            if (pattern->fyleMatch == scid::core::NO_FYLE) { // Nothing to test!
            } else {  // Test this fyle:
                scid::core::squareT sq = scid::core::square_Make (pattern->fyleMatch, scid::core::RANK_1);
                bool found = false;
                for (scid::core::uint i=0; i < 8; i++, sq += 8) {
                    if (board[sq] == pattern->pieceMatch) { found = true; break; }
                }
                if (found != pattern->flag) { return false; }
            }

        } else { // rankMatch is a rank from 1 to 8:

            if (pattern->fyleMatch == scid::core::NO_FYLE) { // Test the whole rank:
                bool found = false;
                scid::core::squareT sq = scid::core::square_Make (scid::core::A_FYLE, pattern->rankMatch);
                for (scid::core::uint i=0; i < 8; i++, sq++) {
                    if (board[sq] == pattern->pieceMatch) { found = true; break; }
                }
                if (found != pattern->flag) { return false; }
            } else {  // Just test one square:
                scid::core::squareT sq = scid::core::square_Make(pattern->fyleMatch, pattern->rankMatch);
                bool found = false;
                if (board[sq] == pattern->pieceMatch) { found = true; }
                if (found != pattern->flag) { return false; }
            }
        }
    }

    // If we reach here, all patterns matched:
    return true;
}

scid::core::errorT decodeSearchStart(ByteBuffer& buf, scid::core::Position& position) {
    scid::core::errorT err = buf.decodeTags([](auto, auto) {});
    if (err != scid::core::OK)
        return err;

    auto [errStartPos, fen] = buf.decodeStartBoard();
    if (errStartPos)
        return errStartPos;

    if (fen)
        return position.ReadFromFEN(fen);

    position.StdStart();
    return scid::core::OK;
}

std::array<scid::core::uint, 8> pawnFylesFor(const scid::core::Position& position, scid::core::pieceT pawn) {
    std::array<scid::core::uint, 8> result = {};
    const scid::core::pieceT* board = position.GetBoard();
    scid::core::uint fyle = 0;
    for (scid::core::squareT sq = scid::core::A1; sq <= scid::core::H8; sq++, board++) {
        if (*board == pawn)
            result[fyle]++;
        fyle = (fyle + 1) & 7;
    }
    return result;
}

bool fyleCountsMatch(const scid::core::Position& position,
                     const std::array<scid::core::uint, 8>& whitePawnFyles,
                     const std::array<scid::core::uint, 8>& blackPawnFyles) {
    auto whiteCurrent = pawnFylesFor(position, scid::core::WP);
    auto blackCurrent = pawnFylesFor(position, scid::core::BP);
    for (scid::core::fyleT fyle = scid::core::A_FYLE; fyle <= scid::core::H_FYLE; ++fyle) {
        if (whiteCurrent[fyle] > whitePawnFyles[fyle] ||
            blackCurrent[fyle] > blackPawnFyles[fyle]) {
            return false;
        }
    }
    return true;
}

bool positionMatches(scid::core::Position* searchPos,
                     scid::core::Position& currentPosition,
                     gameExactMatchT searchType,
                     const std::array<scid::core::uint, 8>& whitePawnFyles,
                     const std::array<scid::core::uint, 8>& blackPawnFyles) {
    if (searchPos->GetToMove() != currentPosition.GetToMove()
        || searchPos->GetCount(scid::core::WHITE) != currentPosition.GetCount(scid::core::WHITE)
        || searchPos->GetCount(scid::core::BLACK) != currentPosition.GetCount(scid::core::BLACK)
        || searchPos->PieceCount(scid::core::WP) != currentPosition.PieceCount(scid::core::WP)
        || searchPos->PieceCount(scid::core::BP) != currentPosition.PieceCount(scid::core::BP)
        || searchPos->PieceCount(scid::core::WN) != currentPosition.PieceCount(scid::core::WN)
        || searchPos->PieceCount(scid::core::BN) != currentPosition.PieceCount(scid::core::BN)
        || searchPos->PieceCount(scid::core::WB) != currentPosition.PieceCount(scid::core::WB)
        || searchPos->PieceCount(scid::core::BB) != currentPosition.PieceCount(scid::core::BB)
        || searchPos->PieceCount(scid::core::WR) != currentPosition.PieceCount(scid::core::WR)
        || searchPos->PieceCount(scid::core::BR) != currentPosition.PieceCount(scid::core::BR)
        || searchPos->PieceCount(scid::core::WQ) != currentPosition.PieceCount(scid::core::WQ)
        || searchPos->PieceCount(scid::core::BQ) != currentPosition.PieceCount(scid::core::BQ)) {
        return false;
    }

    const scid::core::pieceT* currentBoard = currentPosition.GetBoard();
    const scid::core::pieceT* searchBoard = searchPos->GetBoard();
    if (searchType == GAME_EXACT_MATCH_Pawns) {
        for (scid::core::squareT sq = scid::core::A1; sq <= scid::core::H8; sq++, currentBoard++, searchBoard++) {
            if (*currentBoard != *searchBoard &&
                (*currentBoard == scid::core::WP || *currentBoard == scid::core::BP)) {
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
        for (scid::core::squareT sq = scid::core::A1; sq <= scid::core::H8; sq++, currentBoard++, searchBoard++) {
            if (*currentBoard != *searchBoard)
                return false;
        }
    }
    return true;
}

bool varExactMatchLine(scid::core::MoveSequence const& line,
                       scid::core::Position currentPosition,
                       scid::core::Position* searchPos,
                       gameExactMatchT searchType,
                       const std::array<scid::core::uint, 8>& whitePawnFyles,
                       const std::array<scid::core::uint, 8>& blackPawnFyles) {
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

        (void)currentPosition.applyMove(move.action);
    }

    return line.moves.empty() &&
           positionMatches(searchPos, currentPosition, searchType,
                           whitePawnFyles, blackPawnFyles);
}

scid::core::Position startPositionFor(const scid::core::Game& game) {
    return game.startPosition() ? *game.startPosition()
                                : scid::core::Position::getStdStart();
}

bool materialMatches(bool promotionsFlag, ByteBuffer& buf, scid::core::byte* min,
                     scid::core::byte* max, patternT* patterns, size_t patternCount,
                     int minPly, int maxPly, int matchLength,
                     bool oppBishops, bool sameBishops, int minDiff,
                     int maxDiff) {
    ASSERT (matchLength >= 1);

    int matchesNeeded = matchLength;
    int matDiff;
    scid::core::uint plyCount = 0;
    scid::core::Position currentPosition;
    scid::core::errorT err = decodeSearchStart(buf, currentPosition);
    while (err == scid::core::OK) {
        bool foundMatch = false;
        scid::core::byte wMinor, bMinor;

        // If current pos has LESS than the minimum of pawns, this
        // game can never match so return false;
        if (currentPosition.PieceCount(scid::core::WP) < min[scid::core::WP]) { return false; }
        if (currentPosition.PieceCount(scid::core::BP) < min[scid::core::BP]) { return false; }

        // If not in the valid move range, go to the next move or return:
        if ((int)plyCount > maxPly) { return false; }
        if ((int)plyCount < minPly) { goto Next_Move; }

// For these comparisons, we really could only do half of them each move,
// according to which side just moved.
        // For non-pawns, the count could be increased by promotions:
        if (currentPosition.PieceCount(scid::core::WQ) < min[scid::core::WQ]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(scid::core::BQ) < min[scid::core::BQ]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(scid::core::WR) < min[scid::core::WR]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(scid::core::BR) < min[scid::core::BR]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(scid::core::WB) < min[scid::core::WB]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(scid::core::BB) < min[scid::core::BB]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(scid::core::WN) < min[scid::core::WN]) { goto Check_Promotions; }
        if (currentPosition.PieceCount(scid::core::BN) < min[scid::core::BN]) { goto Check_Promotions; }
        wMinor = currentPosition.PieceCount(scid::core::WB) + currentPosition.PieceCount(scid::core::WN);
        bMinor = currentPosition.PieceCount(scid::core::BB) + currentPosition.PieceCount(scid::core::BN);
        if (wMinor < min[scid::core::WM]) { goto Check_Promotions; }
        if (bMinor < min[scid::core::BM]) { goto Check_Promotions; }

        // Now test maximum counts:
        if (currentPosition.PieceCount(scid::core::WQ) > max[scid::core::WQ]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::BQ) > max[scid::core::BQ]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::WR) > max[scid::core::WR]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::BR) > max[scid::core::BR]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::WB) > max[scid::core::WB]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::BB) > max[scid::core::BB]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::WN) > max[scid::core::WN]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::BN) > max[scid::core::BN]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::WP) > max[scid::core::WP]) { goto Next_Move; }
        if (currentPosition.PieceCount(scid::core::BP) > max[scid::core::BP]) { goto Next_Move; }
        if (wMinor > max[scid::core::WM]) { goto Next_Move; }
        if (bMinor > max[scid::core::BM]) { goto Next_Move; }

        // If both sides have ONE bishop, we need to check if the search
        // was restricted to same-color or opposite-color bishops:
        if (currentPosition.PieceCount(scid::core::WB) == 1
                && currentPosition.PieceCount(scid::core::BB) == 1) {
            if (!oppBishops  ||  !sameBishops) { // Check the restriction:
                scid::core::colorT whiteBishCol = scid::core::NOCOLOR;
                scid::core::colorT blackBishCol = scid::core::NOCOLOR;

                // Search for the white and black bishop, to find their
                // square color:
                const scid::core::pieceT* bd = currentPosition.GetBoard();
                for (scid::core::squareT sq = scid::core::A1; sq <= scid::core::H8; sq++) {
                    if (bd[sq] == scid::core::WB) {
                        whiteBishCol = scid::core::BOARD_SQUARECOLOR [sq];
                    } else if (bd[sq] == scid::core::BB) {
                        blackBishCol = scid::core::BOARD_SQUARECOLOR [sq];
                    }
                }
                // They should be valid colors:
                ASSERT (blackBishCol != scid::core::NOCOLOR  &&  whiteBishCol != scid::core::NOCOLOR);

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
        matDiff = (int)currentPosition.MaterialValue(scid::core::WHITE) -
                  (int)currentPosition.MaterialValue(scid::core::BLACK);
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
            scid::core::MoveAction action;
            err = game_storage::decodeMainlineMove(buf, currentPosition, action);
            if (err == scid::core::OK) {
                err = currentPosition.applyMove(action);
            }
        }
        plyCount++;
        if (! foundMatch) { matchesNeeded = matchLength; }
    }

    // End of game reached, and no match:
    return false;
}

bool exactMatches(const scid::core::Game& game, scid::core::Position* searchPos,
                  ByteBuffer* buf, gameExactMatchT searchType) {
    // If buf is NULL, the game is in memory. Otherwise, decode only
    // the necessary moves:
    scid::core::errorT err = scid::core::OK;
    scid::core::Position decodedPosition;
    scid::core::Position* currentPosition = &decodedPosition;
    const scid::core::MoveSequence* memoryLine = nullptr;
    std::size_t memoryMoveIndex = 0;

    if (buf == NULL) {
        decodedPosition = startPositionFor(game);
        memoryLine = &game.movetext().mainline;
    } else {
        err = decodeSearchStart(*buf, decodedPosition);
    }

    scid::core::uint search_whiteHPawns = 0;
    scid::core::uint search_blackHPawns = 0;
    bool check_pawnMaskWhite, check_pawnMaskBlack;
    bool doHomePawnChecks = false;

    scid::core::uint wpawnFyle [8] = {0, 0, 0, 0, 0, 0, 0, 0};
    scid::core::uint bpawnFyle [8] = {0, 0, 0, 0, 0, 0, 0, 0};;

    if (searchType == GAME_EXACT_MATCH_Fyles) {
        const scid::core::pieceT* board = searchPos->GetBoard();
        scid::core::uint fyle = 0;
        for (scid::core::squareT sq = scid::core::A1; sq <= scid::core::H8; sq++, board++) {
            if (*board == scid::core::WP) {
                wpawnFyle[fyle]++;
            } else if (*board == scid::core::BP) {
                bpawnFyle[fyle]++;
            }
            fyle = (fyle + 1) & 7;
        }
    }

    if (searchType == GAME_EXACT_MATCH_Exact  ||
        searchType == GAME_EXACT_MATCH_Pawns) {
        doHomePawnChecks = true;
        search_whiteHPawns = calcHomePawnMask (scid::core::WP, searchPos->GetBoard());
        search_blackHPawns = calcHomePawnMask (scid::core::BP, searchPos->GetBoard());
    }
    check_pawnMaskWhite = check_pawnMaskBlack = false;

    while (err == scid::core::OK) {
        const scid::core::pieceT* currentBoard = currentPosition->GetBoard();
        const scid::core::pieceT* board = searchPos->GetBoard();
        const scid::core::pieceT* b1 = currentBoard;
        const scid::core::pieceT* b2 = board;

        // If NO_SPEEDUPS is defined, a slower search is done without
        // optimisations that detect insufficient material.
#ifndef NO_SPEEDUPS
        // Insufficient material optimisation:
        if (searchPos->GetCount(scid::core::WHITE) > currentPosition->GetCount(scid::core::WHITE)  ||
            searchPos->GetCount(scid::core::BLACK) > currentPosition->GetCount(scid::core::BLACK)) {
            return false;
        }
        // Insufficient pawns optimisation:
        if (searchPos->PieceCount(scid::core::WP) > currentPosition->PieceCount(scid::core::WP)  ||
            searchPos->PieceCount(scid::core::BP) > currentPosition->PieceCount(scid::core::BP)) {
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
                auto current_whiteHPawns = calcHomePawnMask (scid::core::WP, currentBoard);
                if ((current_whiteHPawns & search_whiteHPawns)
                        != search_whiteHPawns) {
                    return false;
                }
                check_pawnMaskWhite = false;
            }
            if (check_pawnMaskBlack) {
                auto current_blackHPawns = calcHomePawnMask (scid::core::BP, currentBoard);
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
        if (searchPos->GetCount(scid::core::WHITE) < currentPosition->GetCount(scid::core::WHITE)  ||
            searchPos->GetCount(scid::core::BLACK) < currentPosition->GetCount(scid::core::BLACK)) {
            //skip++;
            goto Move_Forward;
        }
        // Extra pawns/pieces: skip to next move
        if (searchPos->PieceCount(scid::core::WP) != currentPosition->PieceCount(scid::core::WP)  ||
            searchPos->PieceCount(scid::core::BP) != currentPosition->PieceCount(scid::core::BP)  ||
            searchPos->PieceCount(scid::core::WN) != currentPosition->PieceCount(scid::core::WN)  ||
            searchPos->PieceCount(scid::core::BN) != currentPosition->PieceCount(scid::core::BN)  ||
            searchPos->PieceCount(scid::core::WB) != currentPosition->PieceCount(scid::core::WB)  ||
            searchPos->PieceCount(scid::core::BB) != currentPosition->PieceCount(scid::core::BB)  ||
            searchPos->PieceCount(scid::core::WR) != currentPosition->PieceCount(scid::core::WR)  ||
            searchPos->PieceCount(scid::core::BR) != currentPosition->PieceCount(scid::core::BR)  ||
            searchPos->PieceCount(scid::core::WQ) != currentPosition->PieceCount(scid::core::WQ)  ||
            searchPos->PieceCount(scid::core::BQ) != currentPosition->PieceCount(scid::core::BQ)) {
            //skip++;
            goto Move_Forward;
        }

        // NOW, compare the actual boards piece-by-piece.
        if (searchType == GAME_EXACT_MATCH_Exact) {
            if (searchPos->HashValue() == currentPosition->HashValue()) {
                for (scid::core::squareT sq = scid::core::A1;  sq <= scid::core::H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2) { found = false; break; }
                }
            } else {
                found = false;
            }
        } else if (searchType == GAME_EXACT_MATCH_Pawns) {
            if (searchPos->PawnHashValue() == currentPosition->PawnHashValue()) {
                for (scid::core::squareT sq = scid::core::A1;  sq <= scid::core::H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2  &&  (*b1 == scid::core::WP  ||  *b1 == scid::core::BP)) {
                        found = false;
                        break;
                    }
                }
            } else {
                found = false;
            }
        } else if (searchType == GAME_EXACT_MATCH_Fyles) {
            for (scid::core::fyleT f = scid::core::A_FYLE; f <= scid::core::H_FYLE; f++) {
                if (searchPos->FyleCount(scid::core::WP,f) != currentPosition->FyleCount(scid::core::WP,f)
                      || searchPos->FyleCount(scid::core::BP,f) != currentPosition->FyleCount(scid::core::BP,f)) {
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
            scid::core::MoveAction nextMove;
            if (buf == NULL) {
                if (memoryLine == nullptr ||
                    memoryMoveIndex >= memoryLine->moves.size()) {
                    err = scid::core::ERROR_EndOfMoveList;
                } else {
                    nextMove = memoryLine->moves[memoryMoveIndex].action;
                    memoryMoveIndex++;
                    err = scid::core::OK;
                }
            } else {
                err = game_storage::decodeMainlineMove(*buf, *currentPosition,
                                                       nextMove);
            }

            if (err == scid::core::OK) {
                err = currentPosition->applyMove(nextMove);
            }

            if (err == scid::core::OK) {
                if (doHomePawnChecks) {
                    scid::core::rankT rTo = scid::core::square_Rank (nextMove.to);
                    scid::core::rankT rFrom = scid::core::square_Rank (nextMove.from);
                    // We only re-check the home pawn masks when something moves
                    // to or from the 2nd/7th rank:
                    if (rTo == scid::core::RANK_2  ||  rFrom == scid::core::RANK_2) {
                        check_pawnMaskWhite = true;
                    }
                    if (rTo == scid::core::RANK_7  ||  rFrom == scid::core::RANK_7) {
                        check_pawnMaskBlack = true;
                    }
                }
            }
        }
    }
    return false;
}

} // end of anonymous namespace

bool game_search::materialMatch(bool promotionsFlag, ByteBuffer& buf,
                                scid::core::byte* min, scid::core::byte* max,
                                patternT* patterns, std::size_t patternCount,
                                int minPly, int maxPly, int matchLength,
                                bool oppBishops, bool sameBishops, int minDiff,
                                int maxDiff) {
    return materialMatches(promotionsFlag, buf, min, max, patterns,
                           patternCount, minPly, maxPly, matchLength,
                           oppBishops, sameBishops, minDiff, maxDiff);
}

bool game_search::exactMatch(const scid::core::Game& game, scid::core::Position* pos,
                             ByteBuffer* buf, gameExactMatchT searchType) {
    return exactMatches(game, pos, buf, searchType);
}

bool game_search::varExactMatch(const scid::core::Game& game, scid::core::Position* pos,
                                gameExactMatchT searchType) {
    const auto whitePawnFyles = searchType == GAME_EXACT_MATCH_Fyles
                                    ? pawnFylesFor(*pos, scid::core::WP)
                                    : std::array<scid::core::uint, 8>{};
    const auto blackPawnFyles = searchType == GAME_EXACT_MATCH_Fyles
                                    ? pawnFylesFor(*pos, scid::core::BP)
                                    : std::array<scid::core::uint, 8>{};
    scid::core::Position startPosition = startPositionFor(game);
    return varExactMatchLine(game.movetext().mainline, startPosition, pos,
                             searchType, whitePawnFyles, blackPawnFyles);
}

} // namespace scid::database

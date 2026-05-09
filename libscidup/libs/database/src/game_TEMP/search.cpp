#include "scidup/database/game.h"

#include "scidup/database/bytebuf.h"
#include "scidup/database/common.h"
#include "movetree.h"

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
//      Used by Game::materialMatch() to test patterns.
//      Returns 1 if all the patterns in the list match, 0 otherwise.
//
int patternsMatch(const Position* pos, patternT* ptn, size_t ptn_size) {
    const pieceT* board = pos->GetBoard();
    for (auto ptn_end = ptn + ptn_size; ptn != ptn_end; ++ptn) {
        if (ptn->rankMatch == NO_RANK) {

            if (ptn->fyleMatch == NO_FYLE) { // Nothing to test!
            } else {  // Test this fyle:
                squareT sq = square_Make (ptn->fyleMatch, RANK_1);
                int found = 0;
                for (uint i=0; i < 8; i++, sq += 8) {
                    if (board[sq] == ptn->pieceMatch) { found = 1; break; }
                }
                if (found != ptn->flag) { return 0; }
            }

        } else { // rankMatch is a rank from 1 to 8:

            if (ptn->fyleMatch == NO_FYLE) { // Test the whole rank:
                int found = 0;
                squareT sq = square_Make (A_FYLE, ptn->rankMatch);
                for (uint i=0; i < 8; i++, sq++) {
                    if (board[sq] == ptn->pieceMatch) { found = 1; break; }
                }
                if (found != ptn->flag) { return 0; }
            } else {  // Just test one square:
                squareT sq = square_Make(ptn->fyleMatch, ptn->rankMatch);
                int found = 0;
                if (board[sq] == ptn->pieceMatch) { found = 1; }
                if (found != ptn->flag) { return 0; }
            }
        }
    }

    // If we reach here, all patterns matched:
    return 1;
}
} // end of anonymous namespace

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::materialMatch(): Material search test.
//      The parameters min and max should each be an array of 15
//      counts, to specify the maximum and minimum number of counts
//      of each type of piece.
//
bool Game::materialMatch(bool PromotionsFlag, ByteBuffer& buf, byte* min,
                         byte* max, patternT* patterns, size_t ptn_size,
                         int minPly, int maxPly, int matchLength,
                         bool oppBishops, bool sameBishops, int minDiff,
                         int maxDiff) {
    ASSERT (matchLength >= 1);

    int matchesNeeded = matchLength;
    int matDiff;
    uint plyCount = 0;
    errorT err = decodeSkipTags(&buf);
    while (err == OK) {
        bool foundMatch = false;
        byte wMinor, bMinor;

        // If current pos has LESS than the minimum of pawns, this
        // game can never match so return false;
        if (currentPos_->PieceCount(WP) < min[WP]) { return false; }
        if (currentPos_->PieceCount(BP) < min[BP]) { return false; }

        // If not in the valid move range, go to the next move or return:
        if ((int)plyCount > maxPly) { return false; }
        if ((int)plyCount < minPly) { goto Next_Move; }

// For these comparisons, we really could only do half of them each move,
// according to which side just moved.
        // For non-pawns, the count could be increased by promotions:
        if (currentPos_->PieceCount(WQ) < min[WQ]) { goto Check_Promotions; }
        if (currentPos_->PieceCount(BQ) < min[BQ]) { goto Check_Promotions; }
        if (currentPos_->PieceCount(WR) < min[WR]) { goto Check_Promotions; }
        if (currentPos_->PieceCount(BR) < min[BR]) { goto Check_Promotions; }
        if (currentPos_->PieceCount(WB) < min[WB]) { goto Check_Promotions; }
        if (currentPos_->PieceCount(BB) < min[BB]) { goto Check_Promotions; }
        if (currentPos_->PieceCount(WN) < min[WN]) { goto Check_Promotions; }
        if (currentPos_->PieceCount(BN) < min[BN]) { goto Check_Promotions; }
        wMinor = currentPos_->PieceCount(WB) + currentPos_->PieceCount(WN);
        bMinor = currentPos_->PieceCount(BB) + currentPos_->PieceCount(BN);
        if (wMinor < min[WM]) { goto Check_Promotions; }
        if (bMinor < min[BM]) { goto Check_Promotions; }

        // Now test maximum counts:
        if (currentPos_->PieceCount(WQ) > max[WQ]) { goto Next_Move; }
        if (currentPos_->PieceCount(BQ) > max[BQ]) { goto Next_Move; }
        if (currentPos_->PieceCount(WR) > max[WR]) { goto Next_Move; }
        if (currentPos_->PieceCount(BR) > max[BR]) { goto Next_Move; }
        if (currentPos_->PieceCount(WB) > max[WB]) { goto Next_Move; }
        if (currentPos_->PieceCount(BB) > max[BB]) { goto Next_Move; }
        if (currentPos_->PieceCount(WN) > max[WN]) { goto Next_Move; }
        if (currentPos_->PieceCount(BN) > max[BN]) { goto Next_Move; }
        if (currentPos_->PieceCount(WP) > max[WP]) { goto Next_Move; }
        if (currentPos_->PieceCount(BP) > max[BP]) { goto Next_Move; }
        if (wMinor > max[WM]) { goto Next_Move; }
        if (bMinor > max[BM]) { goto Next_Move; }

        // If both sides have ONE bishop, we need to check if the search
        // was restricted to same-color or opposite-color bishops:
        if (currentPos_->PieceCount(WB) == 1
                && currentPos_->PieceCount(BB) == 1) {
            if (!oppBishops  ||  !sameBishops) { // Check the restriction:
                colorT whiteBishCol = NOCOLOR;
                colorT blackBishCol = NOCOLOR;

                // Search for the white and black bishop, to find their
                // square color:
                const pieceT* bd = currentPos_->GetBoard();
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
        matDiff = (int)currentPos_->MaterialValue(WHITE) -
                  (int)currentPos_->MaterialValue(BLACK);
        if (matDiff < minDiff  ||  matDiff > maxDiff) { goto Next_Move; }

        // At this point, the Material matches; do the patterns match?
        if (ptn_size == 0 || patternsMatch(currentPos(), patterns, ptn_size)) {
            foundMatch = true;
            matchesNeeded--;
            if (matchesNeeded <= 0) { return true; }
        }
        // No? well, keep trying...
        goto Next_Move;

      Check_Promotions:
        // We only continue if this game has promotion moves:
        if (! PromotionsFlag) { return false; }

      Next_Move:
        {
            simpleMoveT sm;
            err = decodeNextMove(&buf, sm);
            if (err == OK) {
                currentPos_->DoSimpleMove(sm);
            }
        }
        plyCount++;
        if (! foundMatch) { matchesNeeded = matchLength; }
    }

    // End of game reached, and no match:
    return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::exactMatch():
//      Exact position search test.
//      If sm is not NULL, its from, to, promote etc will be filled with
//      the next move at the matching position, if there is one.
//      If neverMatch is non-NULL, the boolean it points to is set to
//      true if the game could never match even with extra moves.
//
bool
Game::exactMatch (Position * searchPos, ByteBuffer * buf,
                  gameExactMatchT searchType)
{
    // If buf is NULL, the game is in memory. Otherwise, decode only
    // the necessary moves:
    errorT err = OK;

    if (buf == NULL) {
        toStart();
    } else {
        err = decodeSkipTags(buf);
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
        const pieceT* currentBoard = currentPos_->GetBoard();
        const pieceT* board = searchPos->GetBoard();
        const pieceT* b1 = currentBoard;
        const pieceT* b2 = board;

        // If NO_SPEEDUPS is defined, a slower search is done without
        // optimisations that detect insufficient material.
#ifndef NO_SPEEDUPS
        // Insufficient material optimisation:
        if (searchPos->GetCount(WHITE) > currentPos_->GetCount(WHITE)  ||
            searchPos->GetCount(BLACK) > currentPos_->GetCount(BLACK)) {
            return false;
        }
        // Insufficient pawns optimisation:
        if (searchPos->PieceCount(WP) > currentPos_->PieceCount(WP)  ||
            searchPos->PieceCount(BP) > currentPos_->PieceCount(BP)) {
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
        if (searchPos->GetToMove() != currentPos_->GetToMove()) {
            //skip++;
            goto Move_Forward;
        }

        // Extra material: skip to next move
        if (searchPos->GetCount(WHITE) < currentPos_->GetCount(WHITE)  ||
            searchPos->GetCount(BLACK) < currentPos_->GetCount(BLACK)) {
            //skip++;
            goto Move_Forward;
        }
        // Extra pawns/pieces: skip to next move
        if (searchPos->PieceCount(WP) != currentPos_->PieceCount(WP)  ||
            searchPos->PieceCount(BP) != currentPos_->PieceCount(BP)  ||
            searchPos->PieceCount(WN) != currentPos_->PieceCount(WN)  ||
            searchPos->PieceCount(BN) != currentPos_->PieceCount(BN)  ||
            searchPos->PieceCount(WB) != currentPos_->PieceCount(WB)  ||
            searchPos->PieceCount(BB) != currentPos_->PieceCount(BB)  ||
            searchPos->PieceCount(WR) != currentPos_->PieceCount(WR)  ||
            searchPos->PieceCount(BR) != currentPos_->PieceCount(BR)  ||
            searchPos->PieceCount(WQ) != currentPos_->PieceCount(WQ)  ||
            searchPos->PieceCount(BQ) != currentPos_->PieceCount(BQ)) {
            //skip++;
            goto Move_Forward;
        }

        // NOW, compare the actual boards piece-by-piece.
        if (searchType == GAME_EXACT_MATCH_Exact) {
            if (searchPos->HashValue() == currentPos_->HashValue()) {
                for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2) { found = false; break; }
                }
            } else {
                found = false;
            }
        } else if (searchType == GAME_EXACT_MATCH_Pawns) {
            if (searchPos->PawnHashValue() == currentPos_->PawnHashValue()) {
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
                if (searchPos->FyleCount(WP,f) != currentPos_->FyleCount(WP,f)
                      || searchPos->FyleCount(BP,f) != currentPos_->FyleCount(BP,f)) {
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
        if (buf == NULL) {
            err = next();
        } else {
            simpleMoveT nextMove;
            err = decodeNextMove(buf, nextMove);
            if (err == OK) {
                currentPos_->DoSimpleMove(nextMove);
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Game::varExactMatch():
//    Like exactMatch(), but also searches in variations.
//    This is much slower than exactMatch(), since it will
//    search every position until a match is found.
bool
Game::varExactMatch (Position * searchPos, gameExactMatchT searchType)
{
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

    errorT err = OK;
    while (err == OK) {
        // Check if this position matches:
        bool match = false;
        if (searchPos->GetToMove() == currentPos_->GetToMove()
            &&  searchPos->GetCount(WHITE) == currentPos_->GetCount(WHITE)
            &&  searchPos->GetCount(BLACK) == currentPos_->GetCount(BLACK)
            &&  searchPos->PieceCount(WP) == currentPos_->PieceCount(WP)
            &&  searchPos->PieceCount(BP) == currentPos_->PieceCount(BP)
            &&  searchPos->PieceCount(WN) == currentPos_->PieceCount(WN)
            &&  searchPos->PieceCount(BN) == currentPos_->PieceCount(BN)
            &&  searchPos->PieceCount(WB) == currentPos_->PieceCount(WB)
            &&  searchPos->PieceCount(BB) == currentPos_->PieceCount(BB)
            &&  searchPos->PieceCount(WR) == currentPos_->PieceCount(WR)
            &&  searchPos->PieceCount(BR) == currentPos_->PieceCount(BR)
            &&  searchPos->PieceCount(WQ) == currentPos_->PieceCount(WQ)
            &&  searchPos->PieceCount(BQ) == currentPos_->PieceCount(BQ)) {
            match = true;
            const pieceT* b1 = currentPos_->GetBoard();
            const pieceT* b2 = searchPos->GetBoard();
            if (searchType == GAME_EXACT_MATCH_Pawns) {
                for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                    if (*b1 != *b2  &&  (*b1 == WP  ||  *b1 == BP)) {
                        match = false; break;
                    }
                }
            } else if (searchType == GAME_EXACT_MATCH_Fyles) {
                uint wpf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                uint bpf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                uint fyle = 0;
                for (squareT sq = A1;  sq <= H8;  sq++, b1++) {
                    if (*b1 == WP) {
                        wpf[fyle]++;
                        if (wpf[fyle] > wpawnFyle[fyle]) { match = false; break; }
                    } else if (*b1 == BP) {
                        bpf[fyle]++;
                        if (bpf[fyle] > bpawnFyle[fyle]) { match = false; break; }
                    }
                    fyle = (fyle + 1) & 7;
                }
            } else if (searchType == GAME_EXACT_MATCH_Exact) {
                if (searchPos->HashValue() == currentPos_->HashValue()) {
                    for (squareT sq = A1;  sq <= H8;  sq++, b1++, b2++) {
                        if (*b1 != *b2) { match = false; break; }
                    }
                } else {
                    match = false;
                }
            } else {
                // searchType == GAME_EXACT_MATCH_Material, so do nothing.
            }
        }
        if (match) { return true; }

        // Now try searching each variation in turn:
        for (uint i=0; i < currentMove_->numVariations; i++) {
            enterVariation (i);
            match = varExactMatch (searchPos, searchType);
            exitVariation();
            if (match) { return true; }
        }
        // Continue down this variation:
        next();
        if (currentMove_->marker == END_MARKER) {
            err = ERROR_EndOfMoveList;
        }
    }
    return false;
}

bool game_search::materialMatch(Game& game, bool promotionsFlag,
                                ByteBuffer& buf, byte* min, byte* max,
                                patternT* ptn, std::size_t ptnSize,
                                int minPly, int maxPly, int matchLength,
                                bool oppBishops, bool sameBishops, int minDiff,
                                int maxDiff) {
    return game.materialMatch(promotionsFlag, buf, min, max, ptn, ptnSize,
                              minPly, maxPly, matchLength, oppBishops,
                              sameBishops, minDiff, maxDiff);
}

bool game_search::exactMatch(Game& game, Position* pos, ByteBuffer* buf,
                             gameExactMatchT searchType) {
    return game.exactMatch(pos, buf, searchType);
}

bool game_search::varExactMatch(Game& game, Position* pos,
                                gameExactMatchT searchType) {
    return game.varExactMatch(pos, searchType);
}

} // namespace scid::database

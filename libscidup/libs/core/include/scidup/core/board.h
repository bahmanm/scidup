/** @file
 * Chess board constants, piece helpers, square geometry, and directions.
 */

#pragma once

#include "scidup/core/primitives.h"

#include <cassert>

namespace scid::database {

// Minor piece definitions, used for searching by material only:
const pieceT WM = 16, BM = 17;

const uint MAX_PIECE_TYPES = 18;

// PIECE_FLIP[]: array of pieces, with colors reversed.
const pieceT PIECE_FLIP[MAX_PIECE_TYPES] = {
    END_OF_BOARD,
    BK, BQ, BR, BB, BN, BP,
    EMPTY, EMPTY,
    WK, WQ, WR, WB, WN, WP,
    EMPTY, BM, WM
};

const bool PIECE_IS_SLIDER[8] = {
    false,
    false, true, true, true, false, false,
    false,
};

inline bool piece_IsKing(pieceT p) { return (piece_Type(p) == KING); }

inline bool piece_IsSlider(pieceT p) { return PIECE_IS_SLIDER[piece_Type(p)]; }

inline pieceT piece_FromChar(int x) {
    switch (x) {
    case 'K': return KING;
    case 'Q': return QUEEN;
    case 'R': return ROOK;
    case 'N': return KNIGHT;
    case 'B': return BISHOP;
    default:  return EMPTY;
    }
}

inline leftDiagT square_LeftDiag(squareT sq) {
    return square_Rank(sq) + square_Fyle(sq);
}

inline rightDiagT square_RightDiag(squareT sq) {
    return (7 + square_Rank(sq) - square_Fyle(sq));
}

// square_Color:
//   Return WHITE for a light square, BLACK for a dark square.
inline colorT square_Color(squareT sq) {
    return 1 - (square_LeftDiag(sq) & 1);
}

// square_FlipFyle:
//   Return the square with its file flipped: a1 <-> h1, b1 <-> g1, etc.
inline squareT square_FlipFyle(squareT sq) {
    return square_Make(A_FYLE + H_FYLE - square_Fyle(sq), square_Rank(sq));
}

// square_FlipRank:
//   Return the square with its rank flipped: a1 <-> a8, a2 <-> a7, etc.
inline squareT square_FlipRank(squareT sq) {
    return square_Make(square_Fyle(sq), RANK_1 + RANK_8 - square_Rank(sq));
}

// square_FlipDiag:
//   Return the square flipped along the a1-h8 diagonal.
inline squareT square_FlipDiag(squareT sq) {
    return square_Make(square_Rank(sq), square_Fyle(sq));
}

const uint rankFyleDist[64] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    1, 0, 1, 2, 3, 4, 5, 6,
    2, 1, 0, 1, 2, 3, 4, 5,
    3, 2, 1, 0, 1, 2, 3, 4,
    4, 3, 2, 1, 0, 1, 2, 3,
    5, 4, 3, 2, 1, 0, 1, 2,
    6, 5, 4, 3, 2, 1, 0, 1,
    7, 6, 5, 4, 3, 2, 1, 0
};

// square_Distance:
//   Return the distance in king moves between two squares.
inline uint square_Distance(squareT from, squareT to) {
    assert(from <= H8 && to <= H8);
    uint rankd = rankFyleDist[(square_Rank(from) << 3) | square_Rank(to)];
    uint fyled = rankFyleDist[(square_Fyle(from) << 3) | square_Fyle(to)];
    return (rankd > fyled) ? rankd : fyled;
}

// square_NearestCorner:
//   Return the corner (A1/H1/A8/H8) closest to the specified square.
inline squareT square_NearestCorner(squareT sq) {
    if (square_Rank(sq) <= RANK_4) {
        return (square_Fyle(sq) <= D_FYLE) ? A1 : H1;
    } else {
        return (square_Fyle(sq) <= D_FYLE) ? A8 : H8;
    }
}

inline bool square_IsCornerSquare(squareT sq) {
    return (sq == A1 || sq == H1 || sq == A8 || sq == H8);
}

inline bool square_IsEdgeSquare(squareT sq) {
    rankT rank = square_Rank(sq);
    if (rank == RANK_1 || rank == RANK_8) { return true; }
    fyleT fyle = square_Fyle(sq);
    if (fyle == A_FYLE || fyle == H_FYLE) { return true; }
    return false;
}

const int edgeDist[66] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0,
    0, 1, 2, 2, 2, 2, 1, 0,
    0, 1, 2, 3, 3, 2, 1, 0,
    0, 1, 2, 3, 3, 2, 1, 0,
    0, 1, 2, 2, 2, 2, 1, 0,
    0, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, -1
};

inline int square_EdgeDistance(squareT sq) {
    return edgeDist[sq];
}

inline bool square_IsKnightHop(squareT from, squareT to) {
    assert(from <= H8 && to <= H8);
    uint rdist = rankFyleDist[(square_Rank(from) << 3) | square_Rank(to)];
    uint fdist = rankFyleDist[(square_Fyle(from) << 3) | square_Fyle(to)];
    // It is a knight hop only if one distance is two squares and the
    // other is one square -- that is, only if their product equals two.
    return ((rdist * fdist) == 2);
}

inline char square_FyleChar(squareT sq) {
    return square_Fyle(sq) + 'a';
}

inline char square_RankChar(squareT sq) {
    return square_Rank(sq) + '1';
}

// Directions:
// Up = 1, Down = 2, Left = 4, Right = 8, UpLeft = 5, UpRight = 9,
// DownLeft = 6, DownRight = 10
const directionT
    NULL_DIR = 0,
    UP = 1,
    DOWN = 2,
    LEFT = 4,
    RIGHT = 8,
    UP_LEFT = (UP | LEFT),
    UP_RIGHT = (UP | RIGHT),
    DOWN_LEFT = (DOWN | LEFT),
    DOWN_RIGHT = (DOWN | RIGHT);

const directionT dirOpposite[11] = {
    NULL_DIR,
    DOWN,       // opposite of UP (1)
    UP,         // opposite of DOWN (2)
    NULL_DIR,
    RIGHT,      // opposite of LEFT (4)
    DOWN_RIGHT, // opposite of UP_LEFT (5)
    UP_RIGHT,   // opposite of DOWN_LEFT (6)
    NULL_DIR,
    LEFT,       // opposite of RIGHT (8)
    DOWN_LEFT,  // opposite of UP_RIGHT (9)
    UP_LEFT     // opposite of DOWN_RIGHT (10)
};

// direction_Opposite(): return the opposite direction to d
inline directionT direction_Opposite(directionT d) {
    return dirOpposite[d];
}

// dirIsDiagonal[]: array listing the diagonal directions, for fast
//      lookup of whether a direction is a diagonal.
const bool dirIsDiagonal[11] = {
    false,   //  0 = NULL_DIR
    false,   //  1 = UP
    false,   //  2 = DOWN
    false,   //  3 = Invalid
    false,   //  4 = LEFT
    true,    //  5 = UP_LEFT
    true,    //  6 = DOWN_LEFT
    false,   //  7 = Invalid
    false,   //  8 = RIGHT
    true,    //  9 = UP_RIGHT
    true     // 10 = DOWN_RIGHT
};

inline bool direction_IsDiagonal(directionT dir) {
    return dirIsDiagonal[dir];
}

// dirDelta:
//   Array giving the board delta of moving to the next square
//   in that direction.
const int dirDelta[11] = {
    0,    // NULL_DIR
    8,    // UP
    -8,   // DOWN
    0,    // Invalid
    -1,   // LEFT
    7,    // UP_LEFT
    -9,   // DOWN_LEFT
    0,    // Invalid
    1,    // RIGHT
    9,    // UP_RIGHT
    -7    // DOWN_RIGHT
};

inline int direction_Delta(directionT dir) {
    return dirDelta[dir];
}

// The starting Board
const pieceT START_BOARD[66] = {
    WR, WN, WB, WQ, WK, WB, WN, WR,    // A1--H1
    WP, WP, WP, WP, WP, WP, WP, WP,    // A2--H2
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
    BP, BP, BP, BP, BP, BP, BP, BP,
    BR, BN, BB, BQ, BK, BB, BN, BR,
    EMPTY,        // COLOR_SQUARE
    END_OF_BOARD  // NULL_SQUARE
};

// Square colors for the standard chess board:
const colorT BOARD_SQUARECOLOR[66] = {
    BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE,  // a1-h1
    WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK,  // a2-h2
    BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE,  // a3-h3
    WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK,  // a4-h4
    BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE,  // a5-h5
    WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK,  // a6-h6
    BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE,  // a7-h7
    WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK,  // a8-h8
    NOCOLOR, NOCOLOR  // Color square and Null square
};

// square_Adjacent: returns 1 if the two squares are adjacent. Note that
//    diagonal adjacency is included: a1 and b2 are adjacent.
//    Also note that a square is adjacent to itself.
inline bool square_Adjacent(squareT from, squareT to) {
    assert(from <= H8 && to <= H8);
    rankT fromRank = square_Rank(from);
    rankT toRank = square_Rank(to);
    int rdist = (int)fromRank - (int)toRank;
    if (rdist < -1 || rdist > 1) { return false; }
    fyleT fromFyle = square_Fyle(from);
    fyleT toFyle = square_Fyle(to);
    int fdist = (int)fromFyle - (int)toFyle;
    if (fdist < -1 || fdist > 1) { return false; }
    return true;
}

} // namespace scid::database

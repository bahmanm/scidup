#include "scidup/core/square_collections.h"

#include <gtest/gtest.h>

namespace {

using namespace scid::database;

TEST(SquareCollectionsTest, SquareListTracksAddedSquares) {
	SquareList squares;

	EXPECT_EQ(0U, squares.Size());
	EXPECT_FALSE(squares.Contains(E4));

	squares.Add(E4);
	squares.Add(H8);

	EXPECT_EQ(2U, squares.Size());
	EXPECT_EQ(E4, squares.Get(0));
	EXPECT_EQ(H8, squares.Get(1));
	EXPECT_TRUE(squares.Contains(E4));
	EXPECT_TRUE(squares.Contains(H8));
	EXPECT_FALSE(squares.Contains(A1));
}

TEST(SquareCollectionsTest, SquareListRemoveCompactsWithLastSquare) {
	SquareList squares;
	squares.Add(A1);
	squares.Add(B2);
	squares.Add(C3);

	squares.Remove(1);

	EXPECT_EQ(2U, squares.Size());
	EXPECT_EQ(A1, squares.Get(0));
	EXPECT_EQ(C3, squares.Get(1));
	EXPECT_FALSE(squares.Contains(B2));
}

TEST(SquareCollectionsTest, SquareSetTracksMembership) {
	SquareSet squares;

	EXPECT_FALSE(squares.Contains(A1));
	EXPECT_FALSE(squares.Contains(H8));

	squares.Add(A1);
	squares.Add(H8);

	EXPECT_TRUE(squares.Contains(A1));
	EXPECT_TRUE(squares.Contains(H8));
	EXPECT_FALSE(squares.Contains(E4));
}

} // namespace

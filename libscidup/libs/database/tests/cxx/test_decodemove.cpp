/*
 * Copyright (C) 2020 Fulvio Benini
 *
 * Scid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Scid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scid. If not, see <http://www.gnu.org/licenses/>.
 */

#include "scidup/database/bytebuf.h"
#include <gtest/gtest.h>

TEST(Test_decodeMove, pawn_white) {
	const scid::database::squareT from = scid::database::C2;
	std::tuple<unsigned char, int, scid::database::pieceT> data[] = {
	    {0, from + 7, scid::database::INVALID_PIECE}, {1, from + 8, scid::database::INVALID_PIECE},
	    {2, from + 9, scid::database::INVALID_PIECE}, {3, from + 7, scid::database::QUEEN},
	    {4, from + 8, scid::database::QUEEN},         {5, from + 9, scid::database::QUEEN},
	    {6, from + 7, scid::database::ROOK},          {7, from + 8, scid::database::ROOK},
	    {8, from + 9, scid::database::ROOK},          {9, from + 7, scid::database::BISHOP},
	    {10, from + 8, scid::database::BISHOP},       {11, from + 9, scid::database::BISHOP},
	    {12, from + 7, scid::database::KNIGHT},       {13, from + 8, scid::database::KNIGHT},
	    {14, from + 9, scid::database::KNIGHT},       {15, from + 16, scid::database::INVALID_PIECE},
	};
	scid::database::ByteBuffer bbuf(nullptr, 0);
	for (auto [moveCode, expTo, expPromo] : data) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::PAWN, from, moveCode);
		EXPECT_EQ(to, expTo);
		EXPECT_EQ(promo, expPromo);
	}
}

TEST(Test_decodeMove, pawn_black) {
	const scid::database::squareT from = scid::database::D7;
	std::tuple<unsigned char, int, scid::database::pieceT> data[] = {
	    {0, from - 7, scid::database::INVALID_PIECE}, {1, from - 8, scid::database::INVALID_PIECE},
	    {2, from - 9, scid::database::INVALID_PIECE}, {3, from - 7, scid::database::QUEEN},
	    {4, from - 8, scid::database::QUEEN},         {5, from - 9, scid::database::QUEEN},
	    {6, from - 7, scid::database::ROOK},          {7, from - 8, scid::database::ROOK},
	    {8, from - 9, scid::database::ROOK},          {9, from - 7, scid::database::BISHOP},
	    {10, from - 8, scid::database::BISHOP},       {11, from - 9, scid::database::BISHOP},
	    {12, from - 7, scid::database::KNIGHT},       {13, from - 8, scid::database::KNIGHT},
	    {14, from - 9, scid::database::KNIGHT},       {15, from - 16, scid::database::INVALID_PIECE},
	};
	scid::database::ByteBuffer bbuf(nullptr, 0);
	for (auto [moveCode, expTo, expPromo] : data) {
		auto [to, promo] = bbuf.decodeMove(scid::database::BLACK, scid::database::PAWN, from, moveCode);
		EXPECT_EQ(to, expTo);
		EXPECT_EQ(promo, expPromo);
	}
}

TEST(Test_decodeMove, bishop) {
	const scid::database::squareT from = scid::database::G7;
	scid::database::ByteBuffer bbuf(nullptr, 0);
	for (unsigned char moveCode = 0; moveCode < 16; ++moveCode) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::BISHOP, from, moveCode);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
		EXPECT_EQ(scid::database::square_Fyle(to), scid::database::square_Fyle(moveCode));
		int distRank = (to + 64) / 8 - (from + 64) / 8;
		int distFyle = (to + 64) % 8 - (from + 64) % 8;
		if (moveCode < 8)
			EXPECT_EQ(distRank, distFyle);
		else
			EXPECT_EQ(distRank, -distFyle);
	}
}

TEST(Test_decodeMove, knight) {
	const scid::database::squareT from = scid::database::B3;
	std::tuple<unsigned char, int> data[] = {
	    {1, from - 17}, {2, from - 15}, {3, from - 10}, {4, from - 6},
	    {5, from + 6},  {6, from + 10}, {7, from + 15}, {8, from + 17},
	};
	scid::database::ByteBuffer bbuf(nullptr, 0);
	for (auto [moveCode, expTo] : data) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::KNIGHT, from, moveCode);
		EXPECT_EQ(to, expTo);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
	}
	for (auto moveCode : {0, 9, 10, 11, 12, 13, 14, 15}) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::KNIGHT, from, moveCode);
		EXPECT_EQ(to, from);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
	}
}

TEST(Test_decodeMove, rook) {
	const scid::database::squareT from = scid::database::B3;
	scid::database::ByteBuffer bbuf(nullptr, 0);
	for (unsigned char moveCode = 0; moveCode < 8; ++moveCode) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::ROOK, from, moveCode);
		EXPECT_EQ(scid::database::square_Rank(to), scid::database::square_Rank(from));
		EXPECT_EQ(scid::database::square_Fyle(to), moveCode);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
	}
	for (unsigned char moveCode = 8; moveCode < 16; ++moveCode) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::ROOK, from, moveCode);
		EXPECT_EQ(scid::database::square_Fyle(to), scid::database::square_Fyle(from));
		EXPECT_EQ(scid::database::square_Rank(to), moveCode - 8);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
	}
}

TEST(Test_decodeMove, queen) {
	const scid::database::squareT from = scid::database::A1;
	unsigned char data[] = {scid::database::C3 + 64};
	scid::database::ByteBuffer bbuf(data, sizeof data);
	{
		ASSERT_TRUE(bbuf);
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::QUEEN, from, 0);
		EXPECT_EQ(to, scid::database::C3);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
		EXPECT_FALSE(bbuf);
	}
	for (unsigned char moveCode = 1; moveCode < 8; ++moveCode) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::QUEEN, from, moveCode);
		EXPECT_EQ(scid::database::square_Rank(to), scid::database::square_Rank(from));
		EXPECT_EQ(scid::database::square_Fyle(to), moveCode);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
	}
	for (unsigned char moveCode = 8; moveCode < 16; ++moveCode) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::QUEEN, from, moveCode);
		EXPECT_EQ(scid::database::square_Fyle(to), scid::database::square_Fyle(from));
		EXPECT_EQ(scid::database::square_Rank(to), moveCode - 8);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
	}
}

TEST(Test_decodeMove, king) {
	const scid::database::squareT from = scid::database::B3;
	scid::database::ByteBuffer bbuf(nullptr, 0);
	{
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::KING, from, 0);
		EXPECT_EQ(to, from);
		EXPECT_EQ(promo, scid::database::PAWN);
	}
	std::tuple<unsigned char, int> data[] = {
	    {1, from - 9}, {2, from - 8}, {3, from - 7}, {4, from - 1},
	    {5, from + 1}, {6, from + 7}, {7, from + 8}, {8, from + 9},
	};
	for (auto [moveCode, expTo] : data) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::KING, from, moveCode);
		EXPECT_EQ(to, expTo);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
	}
	{
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::KING, from, 9);
		EXPECT_EQ(to, from);
		EXPECT_EQ(promo, scid::database::QUEEN);
	}
	{
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::KING, from, 10);
		EXPECT_EQ(to, from);
		EXPECT_EQ(promo, scid::database::KING);
	}
	for (unsigned char moveCode = 11; moveCode < 16; ++moveCode) {
		auto [to, promo] = bbuf.decodeMove(scid::database::WHITE, scid::database::KING, from, moveCode);
		EXPECT_EQ(to, from);
		EXPECT_EQ(promo, scid::database::INVALID_PIECE);
	}
}

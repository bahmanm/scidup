/*
 * Copyright (C) 2021  Fulvio Benini.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "scidup/core/board_def.h"
#include "scidup/database/position.h"
#include "scidup/database/stored.h"
#include <gtest/gtest.h>

// 1.d4 Nf6 2.c4 e6 3.Nc3 Bb4 4.Qc2 O-O
std::tuple<scid::database::squareT, scid::database::squareT, bool> line63[] = {
    {scid::database::D2, scid::database::D4, false}, {scid::database::G8, scid::database::F6, false}, {scid::database::C2, scid::database::C4, false}, {scid::database::E7, scid::database::E6, false},
    {scid::database::B1, scid::database::C3, false}, {scid::database::F8, scid::database::B4, false}, {scid::database::D1, scid::database::C2, false}, {scid::database::NS, scid::database::NS, true}};
const auto line63_fen =
    "rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/2N5/PPQ1PPPP/R1B1KBNR w KQ - 0 5";

auto cmp_moves = [](auto move, auto line) {
	if (line.isCastle())
		return std::get<2>(move) && line.getFrom() < line.getTo();

	return std::get<0>(move) == line.getFrom() &&
	       std::get<1>(move) == line.getTo();
};

TEST(Test_StoredLine, classify) {
	auto code = scid::database::StoredLine::classify([&](auto begin, auto end) {
		return std::equal(std::begin(line63), std::end(line63), begin, end,
		                  cmp_moves);
	});
	EXPECT_EQ(63, code);
}

TEST(Test_StoredLine, getMove) {
	auto it = std::begin(line63);
	unsigned i = 0;
	while (auto move = scid::database::StoredLine::getMove(63, i++)) {
		EXPECT_TRUE(cmp_moves(*it++, move));
	}
}

TEST(Test_StoredLine, match) {
	scid::database::Position pos;
	pos.ReadFromFEN(line63_fen);
	const auto stored = scid::database::StoredLine(pos.GetBoard(), pos.GetToMove());
	EXPECT_EQ(-1, stored.match(62));
	EXPECT_EQ(8, stored.match(63));
	EXPECT_EQ(8, stored.match(64));
	EXPECT_EQ(-2, stored.match(65));
}

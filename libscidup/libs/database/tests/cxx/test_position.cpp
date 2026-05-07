#include "scidup/database/game_TEMP/pgnparse.h"

#include <gtest/gtest.h>
#include <string_view>

TEST(Test_PrintFen, castling_flag_kside_from_pgn) {
	std::string_view pgn =
	    "[FEN \"Brbnk1r1/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/bRBNK1R1 w KQkq - "
	    "0 1\"]"
	    "1. Rxa1 Rxa8 2. Ra3 Ra6 3. Rh3 Rh6 4. Rhh1 Rhh8 5. O-O O-O";
	scid::database::Game game;
	scid::database::PgnParseLog parseLog;
	ASSERT_TRUE(scid::database::pgnParseGame(pgn.data(), pgn.size(), game, parseLog));
	game.MoveToStart();

	const char* fens[] = {
	    "Brbnk1r1/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/bRBNK1R1 w KQkq - 0 1",
	    "Brbnk1r1/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/R1BNK1R1 b Kkq - 0 1",
	    "r1bnk1r1/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/R1BNK1R1 w Kk - 0 2",
	    "r1bnk1r1/3pppq1/8/ppp3pp/PPP3PP/R7/3PPPQ1/2BNK1R1 b Kk - 1 2",
	    "2bnk1r1/3pppq1/r7/ppp3pp/PPP3PP/R7/3PPPQ1/2BNK1R1 w Kk - 2 3",
	    "2bnk1r1/3pppq1/r7/ppp3pp/PPP3PP/7R/3PPPQ1/2BNK1R1 b Kk - 3 3",
	    "2bnk1r1/3pppq1/7r/ppp3pp/PPP3PP/7R/3PPPQ1/2BNK1R1 w Kk - 4 4",
	    "2bnk1r1/3pppq1/7r/ppp3pp/PPP3PP/8/3PPPQ1/2BNK1RR b Gk - 5 4",
	    "2bnk1rr/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/2BNK1RR w Gg - 6 5",
	    "2bnk1rr/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/2BN1RKR b g - 7 5",
	    "2bn1rkr/3pppq1/8/ppp3pp/PPP3PP/8/3PPPQ1/2BN1RKR w - - 8 6"};
	for (auto expected : fens) {
		char buf[1024];
		game.currentPos()->PrintFEN(buf, sizeof(buf));
		EXPECT_STREQ(buf, expected);
		game.MoveForwardInPGN();
	}
}

TEST(Test_PrintFen, castling_flag_qside_from_pgn) {
	std::string_view pgn =
	    "[FEN \"Br2k1r1/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/bR2K1R1 b "
	    "KQkq - 0 1\"]"
	    "1... Rg6 2. Rg3 Ra6 3. Ra3 Raxa8 4. Raxa1 O-O-O 5. O-O-O";
	scid::database::Game game;
	scid::database::PgnParseLog parseLog;
	ASSERT_TRUE(scid::database::pgnParseGame(pgn.data(), pgn.size(), game, parseLog));
	game.MoveToStart();

	const char* fens[] = {
	    "Br2k1r1/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/bR2K1R1 b KQkq - 0 1",
	    "Br2k3/1b1ppn2/6r1/pppQ1pPp/PPPq1PP1/8/1B1PPN2/bR2K1R1 w KQq - 1 2",
	    "Br2k3/1b1ppn2/6r1/pppQ1pPp/PPPq1PP1/6R1/1B1PPN2/bR2K3 b Qq - 2 2",
	    "Br2k3/1b1ppn2/r7/pppQ1pPp/PPPq1PP1/6R1/1B1PPN2/bR2K3 w Qq - 3 3",
	    "Br2k3/1b1ppn2/r7/pppQ1pPp/PPPq1PP1/R7/1B1PPN2/bR2K3 b Qq - 4 3",
	    "rr2k3/1b1ppn2/8/pppQ1pPp/PPPq1PP1/R7/1B1PPN2/bR2K3 w Qb - 0 4",
	    "rr2k3/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/RR2K3 b Bb - 0 4",
	    "r1kr4/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/RR2K3 w B - 1 5",
	    "r1kr4/1b1ppn2/8/pppQ1pPp/PPPq1PP1/8/1B1PPN2/R1KR4 b - - 2 5"};
	for (auto expected : fens) {
		char buf[1024];
		game.currentPos()->PrintFEN(buf, sizeof(buf));
		EXPECT_STREQ(buf, expected);
		game.MoveForwardInPGN();
	}
}

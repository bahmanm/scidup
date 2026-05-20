/*
 * Copyright (C) 2017 Fulvio Benini
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

#include "scidup/database/game.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_cursor.h"
#include "scidup/core/nags.h"
#include "scidup/core/notation.h"
#include "scidup/core/pgn/encode.h"
#include "scidup/core/pgn/traversal.h"
#include "scidup/database/scidbase.h"
#include "game_storage.h"
#include "legacy_pgn.h"
#include "nag_format.h"
#include "piece_translation.h"
#include "scidup/database/pgnparse.h"
#include "pgnparse_impl.h"
#include <algorithm>
#include "scidup/database/bytebuf.h"
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace {

const char* gameUTF8 = SCIDUP_TEST_RESOURCES_DIR "res_gameUTF8.pgn";
const char* gameLatin1 = SCIDUP_TEST_RESOURCES_DIR "res_gameLatin1.pgn";
const char* gameLatin1Conv = SCIDUP_TEST_RESOURCES_DIR "res_gameLatin1expected.pgn";

void expectMoveAction(const scid::core::Move* move,
                      scid::database::squareT from,
                      scid::database::squareT to) {
	ASSERT_NE(nullptr, move);
	EXPECT_EQ(from, move->action.from);
	EXPECT_EQ(to, move->action.to);
}

scid::database::LegacyGameEncodeOptions scidFlagsPgnOptions() {
	return {
	    PGN_STYLE_TAGS | PGN_STYLE_VARS | PGN_STYLE_COMMENTS |
	        PGN_STYLE_SCIDFLAGS,
	    scid::database::PGN_FORMAT_Plain,
	    0,
	};
}

std::optional<scid::database::Position>
currentPosition(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	EXPECT_TRUE(cursor.restore(game.coreLocation()));
	auto position = cursor.currentPosition();
	EXPECT_TRUE(position.has_value());
	return position;
}

std::string currentFen(const scid::database::Game& game) {
	auto position = currentPosition(game);
	if (!position)
		return {};

	char buf[1024];
	position->PrintFEN(buf, sizeof(buf));
	return buf;
}

scid::database::simpleMoveT makeCurrentMove(scid::database::Game& game,
                                            scid::database::squareT from,
                                            scid::database::squareT to) {
	scid::database::simpleMoveT move;
	if (auto position = currentPosition(game))
		position->makeMove(from, to, scid::database::EMPTY, move);
	return move;
}

void addMove(scid::database::Game& game,
             scid::database::simpleMoveT const& move) {
	scid::core::MovetextCursor cursor(game.coreGame());
	ASSERT_TRUE(cursor.restore(game.coreLocation()));
	cursor.addMove({move.from, move.to, move.promote, move.isCastle() != 0});
	game.restoreLocation(cursor.location());
}

void addVariation(scid::database::Game& game) {
	scid::core::MovetextCursor cursor(game.coreGame());
	ASSERT_TRUE(cursor.restore(game.coreLocation()));
	ASSERT_TRUE(cursor.previous());
	ASSERT_NE(nullptr, cursor.addVariation());
	game.restoreLocation(cursor.location());
}

std::string nextCoreSan(const scid::database::Game& game) {
	return scid::core::notation::nextSan(game.coreGame(), game.coreLocation());
}

scid::core::GameCursor coreCursor(const scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	[[maybe_unused]] const bool restored = cursor.restore(game.coreLocation());
	EXPECT_TRUE(restored);
	return cursor;
}

bool nextPgn(scid::database::Game& game) {
	auto cursor = coreCursor(game);
	if (!scid::core::pgn::nextLocation(cursor))
		return false;
	game.restoreLocation(cursor.location());
	return true;
}

bool toPgnLocation(scid::database::Game& game, unsigned location) {
	scid::core::GameCursor cursor(game.coreGame());
	if (!scid::core::pgn::seekLocation(cursor, location))
		return false;
	game.restoreLocation(cursor.location());
	return true;
}

unsigned pgnLocation(const scid::database::Game& game) {
	return scid::core::pgn::locationOf(coreCursor(game));
}

unsigned pgnOffset(const scid::database::Game& game) {
	return scid::core::pgn::offsetOf(coreCursor(game));
}

void setCurrentComment(scid::database::Game& game, std::string_view comment) {
	scid::core::MovetextCursor cursor(game.coreGame());
	ASSERT_TRUE(cursor.restore(game.coreLocation()));
	ASSERT_TRUE(cursor.setComment(comment));
}

bool addCurrentNag(scid::database::Game& game, scid::database::byte nag) {
	scid::core::MovetextCursor cursor(game.coreGame());
	EXPECT_TRUE(cursor.restore(game.coreLocation()));
	return cursor.addPreviousMoveNag(nag);
}

bool removeCurrentNag(scid::database::Game& game, bool moveNag) {
	scid::core::MovetextCursor cursor(game.coreGame());
	EXPECT_TRUE(cursor.restore(game.coreLocation()));
	return cursor.removePreviousMoveNag(moveNag);
}

void clearCurrentNags(scid::database::Game& game) {
	scid::core::MovetextCursor cursor(game.coreGame());
	ASSERT_TRUE(cursor.restore(game.coreLocation()));
	cursor.clearPreviousMoveNags();
}

void stripMovetext(scid::database::Game& game, bool variations,
                   bool comments, bool nags) {
	if (variations) {
		scid::core::MovetextCursor cursor(game.coreGame());
		ASSERT_TRUE(cursor.restore(game.coreLocation()));
		while (cursor.exitVariation()) {
		}
		game.restoreLocation(cursor.location());
	}

	game.coreGame().stripMovetext(variations, comments, nags);
}

scid::database::errorT resetGameStartFen(scid::database::Game& game,
                                         const char* fen) {
	scid::database::Position position;
	if (auto err = position.ReadFromFEN(fen))
		return err;

	game.coreGame().clearMovetext();
	game.coreGame().setStartPosition(position);

	game.restoreLocation(scid::core::MovetextLocation{});
	return scid::database::OK;
}

std::string nextLegacySan(scid::database::Game& game) {
	scid::core::GameCursor cursor(game.coreGame());
	EXPECT_TRUE(cursor.restore(game.coreLocation()));
	auto position = cursor.currentPosition();
	auto* move = cursor.nextMove();
	if (!position || !move)
		return {};

	auto simpleMove = scid::core::notation::toSimpleMove(*position, move->action);
	if (!simpleMove)
		return {};

	scid::database::sanStringT san = {};
	position->MakeSANString(&*simpleMove, san, scid::database::SAN_MATETEST);
	return san;
}

} // namespace

TEST(Test_Game, clone) {
	for (auto filename : {gameUTF8, gameLatin1, gameLatin1Conv}) {

		scid::database::scidBaseT dbase;
		ASSERT_EQ(scid::database::OK, dbase.open("PGN", scid::database::FMODE_Both, filename));
		ASSERT_NE(nullptr, dbase.getIndexEntry_bounds(0));

		scid::database::Game game;
		ASSERT_EQ(scid::database::OK, dbase.getGame(*dbase.getIndexEntry(0), game));

		std::mt19937 re(std::random_device{}());
		ASSERT_TRUE(
		    toPgnLocation(game, std::uniform_int_distribution<unsigned>{0, 500}(re)));

		std::unique_ptr<scid::database::Game> clone{game.clone()};

		ASSERT_EQ(pgnOffset(*clone), pgnOffset(game));

		auto position = currentPosition(game);
		auto clonePosition = currentPosition(*clone);
		ASSERT_TRUE(position);
		ASSERT_TRUE(clonePosition);
		auto board = position->GetBoard();
		ASSERT_TRUE(
		    std::equal(board, board + 66, clonePosition->GetBoard()));

		ASSERT_EQ(nextCoreSan(game), nextCoreSan(*clone));

		auto pgnGame =
		    scid::database::legacy_pgn::encode(game, scidFlagsPgnOptions(), 75,
		                                       true);

		auto pgnClone = scid::database::legacy_pgn::encode(
		    *clone, scidFlagsPgnOptions(), 75, true);

		ASSERT_TRUE(std::equal(pgnClone.first, pgnClone.first + pgnClone.second,
		                       pgnGame.first, pgnGame.first + pgnGame.second));
	}
}

TEST(Test_Game, WriteToPGNDoesNotMutateOptions) {
	scid::database::Game game;
	const auto options = scid::database::LegacyGameEncodeOptions{
	    PGN_STYLE_TAGS | PGN_STYLE_COLUMN,
	    scid::database::PGN_FORMAT_Plain,
	    0,
	};

	auto first = scid::database::legacy_pgn::encode(game, options, 75, true);
	std::string firstPgn(first.first, first.second);
	auto second = scid::database::legacy_pgn::encode(game, options, 75, true);

	EXPECT_EQ(firstPgn, std::string(second.first, second.second));
}

TEST(Test_Game, LegacyGameEncodeOptionsFormatFromString) {
	scid::database::gameFormatT format = scid::database::PGN_FORMAT_Color;

	ASSERT_TRUE(
	    scid::database::LegacyGameEncodeOptions::legacyFormatFromString(
	        "pgn", &format));
	EXPECT_TRUE(
	    (scid::database::LegacyGameEncodeOptions{0, format, 0}
	         .isPlainFormat()));

	ASSERT_TRUE(
	    scid::database::LegacyGameEncodeOptions::legacyFormatFromString(
	        "html", &format));
	EXPECT_TRUE(
	    (scid::database::LegacyGameEncodeOptions{0, format, 0}.isHtmlFormat()));

	ASSERT_TRUE(
	    scid::database::LegacyGameEncodeOptions::legacyFormatFromString(
	        "latex", &format));
	const auto options = scid::database::LegacyGameEncodeOptions{0, format, 0};
	EXPECT_TRUE(options.isLatexFormat());

	EXPECT_FALSE(
	    scid::database::LegacyGameEncodeOptions::legacyFormatFromString(
	        "unknown", &format));
}

TEST(Test_Game, LegacyNagFormatParsePrint) {
	auto parseNag = [](std::string_view nag) {
		return scid::database::game_parseNag(
		    {nag.data(), nag.data() + nag.size()});
	};

	EXPECT_EQ(scid::core::NAG_ExcellentMove,
	          parseNag("!!"));
	EXPECT_EQ(scid::core::NAG_Diagram, parseNag("D"));
	EXPECT_EQ(scid::core::NAG_Comment, parseNag("$145"));

	char nagText[20] = {};
	scid::database::game_printNag(scid::core::NAG_Diagram, nagText, true,
	                              scid::database::PGN_FORMAT_Plain);
	EXPECT_STREQ("D", nagText);

	scid::database::game_printNag(scid::core::NAG_GoodMove, nagText, false,
	                              scid::database::PGN_FORMAT_Plain);
	EXPECT_STREQ("$1", nagText);
}

TEST(Test_Game, LegacyPieceTranslation) {
	const auto savedLanguage = scid::database::language;
	scid::database::language = 3; // German: N -> S, B -> L.

	char san[] = "Nf3 Bc4";
	scid::database::transPieces(san);

	EXPECT_STREQ("Sf3 Lc4", san);
	EXPECT_EQ('S', scid::database::transPiecesChar('N'));

	scid::database::language = savedLanguage;
}

TEST(Test_Game, locationInPGN) {
	for (auto filename : {gameUTF8, gameLatin1, gameLatin1Conv}) {

		scid::database::scidBaseT dbase;
		ASSERT_EQ(scid::database::OK, dbase.open("PGN", scid::database::FMODE_Both, filename));
		ASSERT_NE(nullptr, dbase.getIndexEntry_bounds(0));

		scid::database::Game game;
		 auto bufGame = dbase.getGame(*dbase.getIndexEntry(0));
		 ASSERT_TRUE(bufGame);
		 game.clear();
		 ASSERT_EQ(scid::database::OK,
		          scid::database::game_storage::decodeMovesOnly(
		              game.coreGame(), bufGame));
		 game.restoreLocation(scid::core::MovetextLocation{});

		unsigned location = 1;
		game.restoreLocation(scid::core::MovetextLocation{});
		while (true) {
			++location;
			if (!nextPgn(game)) {
				ASSERT_FALSE(toPgnLocation(game, location));
				break;
			}

			ASSERT_EQ(location, pgnLocation(game));
			if (!coreCursor(game).isAtVariationStart()) {
				ASSERT_EQ(location, pgnOffset(game));
			}

			std::string san = nextCoreSan(game);
			auto ply1 = coreCursor(game).ply();
			ASSERT_TRUE(toPgnLocation(game, location));
			auto ply2 = coreCursor(game).ply();
			ASSERT_EQ(ply1, ply2);
			ASSERT_EQ(location, pgnLocation(game));
			ASSERT_EQ(san, nextCoreSan(game));
		}
	}
}

TEST(Test_Game, toStart_toEnd) {
	scid::database::scidBaseT dbase;
	ASSERT_EQ(scid::database::OK, dbase.open("PGN", scid::database::FMODE_Both, gameUTF8));
	auto ie = dbase.getIndexEntry_bounds(0);
	ASSERT_NE(nullptr, ie);

	auto randomEngine = std::mt19937(std::random_device{}());
	auto distribution = std::uniform_int_distribution<>{2, 500};
	scid::database::Game game;
	ASSERT_EQ(scid::database::OK, dbase.getGame(*ie, game));

	for (int i = 0; i < 10; i++) {
		ASSERT_TRUE(toPgnLocation(game, distribution(randomEngine)));
		ASSERT_NE(0, coreCursor(game).ply());
		game.restoreLocation(scid::core::MovetextLocation{}); // Move to start from any position
		EXPECT_EQ(0, coreCursor(game).ply());
	}
	game.restoreLocation(scid::core::MovetextLocation{}); // Move to start from start
	EXPECT_EQ(0, coreCursor(game).ply());
	{
		scid::core::GameCursor cursor(game.coreGame());
		cursor.toEnd();
		game.restoreLocation(cursor.location());
	} // Move to end from start
	EXPECT_EQ(149, coreCursor(game).ply());
	{
		scid::core::GameCursor cursor(game.coreGame());
		cursor.toEnd();
		game.restoreLocation(cursor.location());
	} // Move to end from end
	EXPECT_EQ(149, coreCursor(game).ply());
	for (int i = 0; i < 10; i++) {
		ASSERT_TRUE(toPgnLocation(game, distribution(randomEngine)));
		{
			scid::core::GameCursor cursor(game.coreGame());
			cursor.toEnd();
			game.restoreLocation(cursor.location());
		} // Move to end from any position
		EXPECT_EQ(149, coreCursor(game).ply());
	}
}

TEST(Test_Game, coreGamePgnEncodingIncludesLegacyMetadataTags) {
	using namespace std::literals;

	scid::database::Game game;

	game.coreGame().addTag("White", "white player");
	game.coreGame().addTag("Black", "black player");
	game.coreGame().setDate(scid::database::date_parsePGNTag("2018.06.11", 10));
	game.coreGame().setWhiteRating({2800, scid::database::RATING_Rapid});
	game.coreGame().setBlackRating({2650, scid::database::RATING_Elo});
	game.coreGame().setEco("A01");
	game.coreGame().setEventDate(scid::database::date_parsePGNTag("2018.06.01", 10));
	game.coreGame().addTag("UTCDate", "2018.06.10");
	game.coreGame().addTag("Annotator", "Example");
	game.coreGame().setResult(scid::database::RESULT_Black);
	const char* fen = "8/N2P1pk1/2n2q2/1P2pp2/5PN1/QKPp1P2/8/8 w - - 0 1";
	ASSERT_EQ(scid::database::OK, resetGameStartFen(game, fen));
	game.coreGame().addTag("Event", "event nAme");
	game.coreGame().addTag("Round", "round 4");
	game.coreGame().addTag("Site", "a long site maybe in a long country");

	std::string pgn;
	scid::core::pgn::encode_game(game.coreGame(), pgn);

	auto expected = "[Event\0\"event nAme\"]\n"sv
	                "[Site\0\"a long site maybe in a long country\"]\n"sv
	                "[Date\0\"2018.06.11\"]\n"sv
	                "[Round\0\"round 4\"]\n"sv
	                "[White\0\"white player\"]\n"sv
	                "[Black\0\"black player\"]\n"sv
	                "[Result\0\"0-1\"]\n"sv
	                "[WhiteRapid\0\"2800\"]\n"sv
	                "[BlackElo\0\"2650\"]\n"sv
	                "[ECO\0\"A01\"]\n"sv
	                "[EventDate\0\"2018.06.01\"]\n"sv
	                "[UTCDate\0\"2018.06.10\"]\n"sv
	                "[Annotator\0\"Example\"]\n"sv
	                "[FEN\0\"8/N2P1pk1/2n2q2/1P2pp2/5PN1/"
	                "QKPp1P2/8/8 w - - 0 1\"]\n"sv
	                "\n"sv
	                "0-1\n"sv;
	EXPECT_EQ(expected, pgn);
}

TEST(Test_Game, empty_tag_name) {
	std::vector<unsigned char> encodedGame;
	{
		scid::database::Game game;
		game.coreGame().addTag("Normal tag ", "normal  value");
		game.coreGame().addTag("", "empty tag name");
		game.coreGame().addTag("Annotator", "common tag");
		EXPECT_EQ(game.coreGame().extraTags().size(), 3);

		scid::database::game_storage::encode(game.coreGame(), game.scidFlags(),
		                                     encodedGame);
	}

	scid::database::ByteBuffer bbuf(encodedGame.data(), encodedGame.size());
	int i = 0;
	bbuf.decodeTags([&i](auto tag_name, auto tag_value) {
		if (i++ == 0) {
			EXPECT_EQ(tag_name, "Normal tag ");
			EXPECT_EQ(tag_value, "normal  value");
		} else {
			EXPECT_EQ(tag_name, "Annotator");
			EXPECT_EQ(tag_value, "common tag");
		}
	});
	EXPECT_EQ(i, 2);
}

TEST(Test_Game, encodeFEN) {
	std::vector<unsigned char> encodedGame;
	const char* kiwipete =
	    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
	{
		scid::database::Game game;
		ASSERT_EQ(scid::database::OK, resetGameStartFen(game, kiwipete));
		scid::database::game_storage::encode(game.coreGame(), game.scidFlags(),
		                                     encodedGame);
	}
	{
		scid::database::ByteBuffer bbuf(encodedGame.data(), encodedGame.size());
		scid::database::Game game;
		game.clear();
		scid::database::game_storage::decodeMovesOnly(game.coreGame(), bbuf);
		game.restoreLocation(scid::core::MovetextLocation{});
		EXPECT_STREQ(kiwipete, currentFen(game).c_str());
	}
}

TEST(Test_Game, currentPositionUci_startpos) {
	std::string_view pgn = "1.d4 (1.e4 e5 ( 1...c5)) (1.c4) 1...d5 2.c4";
	scid::database::Game game;
	scid::database::pgn::parse_game({pgn.data(), pgn.data() + pgn.size()},
	                                scid::database::PgnVisitor{game});

	const std::pair<unsigned, const char*> expected[] = {
	    {0, "position startpos moves"},
	    {1, "position startpos moves"},
	    {2, "position startpos moves d2d4"},
	    {3, "position startpos moves"},
	    {4, "position startpos moves e2e4"},
	    {5, "position startpos moves e2e4 e7e5"},
	    {6, "position startpos moves e2e4"},
	    {7, "position startpos moves e2e4 c7c5"},
	    {8, "position startpos moves"},
	    {9, "position startpos moves c2c4"},
	    {10, "position startpos moves d2d4 d7d5"},
	    {11, "position startpos moves d2d4 d7d5 c2c4"}};
	for (auto [pos, str] : expected) {
		ASSERT_TRUE(toPgnLocation(game, pos));
		EXPECT_EQ(str, scid::core::notation::currentPositionUci(
		                   game.coreGame(), game.coreLocation()));
	}
}

TEST(Test_Game, coreGameMovetextMirrorsLegacyMoveTree) {
	std::string_view pgn =
	    "1.d4! {Best by test} ({Queen pawn alternative} 1.e4 e5 ( 1...c5)) "
	    "(1.c4) 1...d5 2.c4";
	scid::database::Game game;
	scid::database::pgn::parse_game({pgn.data(), pgn.data() + pgn.size()},
	                                scid::database::PgnVisitor{game});

	scid::core::GameCursor cursor(game.coreGame());
	expectMoveAction(cursor.nextMove(), scid::database::D2, scid::database::D4);
	ASSERT_TRUE(cursor.next());
	expectMoveAction(cursor.nextMove(), scid::database::D7, scid::database::D5);
	ASSERT_TRUE(cursor.next());
	expectMoveAction(cursor.nextMove(), scid::database::C2, scid::database::C4);
	ASSERT_TRUE(cursor.next());
	EXPECT_TRUE(cursor.isAtGameEnd());

	auto const& mainline = game.coreGame().movetext().mainline.moves;
	ASSERT_EQ(3U, mainline.size());
	ASSERT_EQ(1U, mainline[0].metadata.nags.size());
	EXPECT_EQ(scid::core::NAG_GoodMove, mainline[0].metadata.nags[0]);
	EXPECT_EQ("Best by test", mainline[0].metadata.comment);
	EXPECT_TRUE(mainline[0].san.empty());
	ASSERT_EQ(2U, mainline[0].childVariations.size());
	EXPECT_EQ("Queen pawn alternative",
	          mainline[0].childVariations[0].initialComment);

	cursor.toStart();
	ASSERT_EQ(2U, cursor.variationCount());
	ASSERT_TRUE(cursor.enterVariation(0));
	expectMoveAction(cursor.nextMove(), scid::database::E2, scid::database::E4);
	ASSERT_TRUE(cursor.next());
	expectMoveAction(cursor.nextMove(), scid::database::E7, scid::database::E5);
	ASSERT_EQ(1U, cursor.variationCount());
	ASSERT_TRUE(cursor.enterVariation(0));
	expectMoveAction(cursor.nextMove(), scid::database::C7, scid::database::C5);
	ASSERT_TRUE(cursor.next());
	EXPECT_TRUE(cursor.isAtVariationEnd());

	ASSERT_TRUE(cursor.exitVariation());
	ASSERT_TRUE(cursor.exitVariation());
	ASSERT_TRUE(cursor.enterVariation(1));
	expectMoveAction(cursor.nextMove(), scid::database::C2, scid::database::C4);
	ASSERT_TRUE(cursor.next());
	EXPECT_TRUE(cursor.isAtVariationEnd());

	game.restoreLocation(scid::core::MovetextLocation{});
	EXPECT_EQ("d4", nextCoreSan(game));
	EXPECT_TRUE(game.coreGame().movetext().mainline.moves[0].san.empty());
}

TEST(Test_Game, coreGameMovetextMirrorsProgrammaticVariationAdds) {
	scid::database::Game game;

	addMove(game, makeCurrentMove(game, scid::database::E2,
	                              scid::database::E4));
	addVariation(game);
	addMove(game, makeCurrentMove(game, scid::database::D2,
	                              scid::database::D4));

	auto const& mainline = game.coreGame().movetext().mainline.moves;
	ASSERT_EQ(1U, mainline.size());
	EXPECT_EQ("e2e4", mainline[0].action.longNotation());
	ASSERT_EQ(1U, mainline[0].childVariations.size());

	auto const& variation = mainline[0].childVariations[0].line.moves;
	ASSERT_EQ(1U, variation.size());
	EXPECT_EQ("d2d4", variation[0].action.longNotation());
}

TEST(Test_Game, stateQueriesMirrorCoreCursorForProgrammaticVariation) {
	scid::database::Game game;

	addMove(game, makeCurrentMove(game, scid::database::E2,
	                              scid::database::E4));
	addVariation(game);
	auto cursor = coreCursor(game);
	EXPECT_EQ(0U, coreCursor(game).ply());
	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_EQ(0U, cursor.variationIndex());
	EXPECT_EQ(0U, cursor.variationCount());
	EXPECT_TRUE(cursor.isAtVariationStart());
	EXPECT_TRUE(cursor.isAtVariationEnd());
	EXPECT_TRUE(cursor.isAtEmptyVariation());
	EXPECT_FALSE(cursor.isAtGameStart());
	EXPECT_FALSE(cursor.isAtGameEnd());

	addMove(game, makeCurrentMove(game, scid::database::D2,
	                              scid::database::D4));
	auto cursorAfterMove = coreCursor(game);
	EXPECT_EQ(1U, coreCursor(game).ply());
	EXPECT_EQ(1U, cursorAfterMove.variationDepth());
	EXPECT_EQ(0U, cursorAfterMove.variationIndex());
	EXPECT_FALSE(cursorAfterMove.isAtVariationStart());
	EXPECT_TRUE(cursorAfterMove.isAtVariationEnd());
	EXPECT_FALSE(cursorAfterMove.isAtEmptyVariation());

	{
		scid::core::GameCursor cursor(game.coreGame());
		ASSERT_TRUE(cursor.restore(game.coreLocation()));
		ASSERT_TRUE(cursor.exitVariation());
		game.restoreLocation(cursor.location());
	}
	auto cursorAfterExit = coreCursor(game);
	EXPECT_EQ(0U, coreCursor(game).ply());
	EXPECT_EQ(0U, cursorAfterExit.variationDepth());
	EXPECT_EQ(0U, cursorAfterExit.variationIndex());
	EXPECT_EQ(1U, cursorAfterExit.variationCount());
	EXPECT_TRUE(cursorAfterExit.isAtGameStart());
	EXPECT_FALSE(cursorAfterExit.isAtGameEnd());
}

TEST(Test_Game, savedLocationRestoresProgrammaticVariationState) {
	scid::database::Game game;

	addMove(game, makeCurrentMove(game, scid::database::E2,
	                              scid::database::E4));
	addVariation(game);
	addMove(game, makeCurrentMove(game, scid::database::D2,
	                              scid::database::D4));

	auto location = game.coreLocation();
	{
		scid::core::GameCursor cursor(game.coreGame());
		ASSERT_TRUE(cursor.restore(game.coreLocation()));
		ASSERT_TRUE(cursor.exitVariation());
		game.restoreLocation(cursor.location());
	}
	ASSERT_TRUE(coreCursor(game).isAtGameStart());

	game.restoreLocation(location);
	auto cursor = coreCursor(game);
	EXPECT_EQ(1U, coreCursor(game).ply());
	EXPECT_EQ(1U, cursor.variationDepth());
	EXPECT_TRUE(cursor.isAtVariationEnd());
	EXPECT_FALSE(cursor.isAtEmptyVariation());
}

TEST(Test_Game, coreGameMoveMetadataMirrorsProgrammaticNagMutation) {
	scid::database::Game game;

	addMove(game, makeCurrentMove(game, scid::database::E2,
	                              scid::database::E4));
	ASSERT_TRUE(addCurrentNag(game, scid::core::NAG_GoodMove));
	ASSERT_TRUE(addCurrentNag(game, scid::core::NAG_PoorMove));
	ASSERT_TRUE(addCurrentNag(game, scid::core::NAG_Equal));

	auto const& firstNags =
	    game.coreGame().movetext().mainline.moves[0].metadata.nags;
	ASSERT_EQ(2U, firstNags.size());
	EXPECT_EQ(scid::core::NAG_PoorMove, firstNags[0]);
	EXPECT_EQ(scid::core::NAG_Equal, firstNags[1]);

	ASSERT_TRUE(removeCurrentNag(game, true));
	auto const& afterRemove =
	    game.coreGame().movetext().mainline.moves[0].metadata.nags;
	ASSERT_EQ(1U, afterRemove.size());
	EXPECT_EQ(scid::core::NAG_Equal, afterRemove[0]);

	clearCurrentNags(game);
	EXPECT_TRUE(
	    game.coreGame().movetext().mainline.moves[0].metadata.nags.empty());
}

TEST(Test_Game, coreGameVariationMetadataMirrorsProgrammaticNagMutation) {
	scid::database::Game game;

	addMove(game, makeCurrentMove(game, scid::database::E2,
	                              scid::database::E4));
	addVariation(game);
	addMove(game, makeCurrentMove(game, scid::database::D2,
	                              scid::database::D4));
	ASSERT_TRUE(addCurrentNag(game, scid::core::NAG_InterestingMove));

	auto const& variationMove =
	    game.coreGame()
	        .movetext()
	        .mainline.moves[0]
	        .childVariations[0]
	        .line.moves[0];
	ASSERT_EQ(1U, variationMove.metadata.nags.size());
	EXPECT_EQ(scid::core::NAG_InterestingMove,
	          variationMove.metadata.nags[0]);
}

TEST(Test_Game, coreGameMirrorsProgrammaticCommentMutation) {
	using namespace std::literals;

	scid::database::Game game;
	setCurrentComment(game, "Before the first move");
	addMove(game, makeCurrentMove(game, scid::database::E2,
	                              scid::database::E4));
	setCurrentComment(game, "After e4");
	addVariation(game);
	setCurrentComment(game, "Queen pawn alternative");
	addMove(game, makeCurrentMove(game, scid::database::D2,
	                              scid::database::D4));
	setCurrentComment(game, "After d4");

	auto const& movetext = game.coreGame().movetext();
	EXPECT_EQ("Before the first move"sv, movetext.initialComment);

	auto const& mainlineMove = movetext.mainline.moves[0];
	EXPECT_EQ("After e4"sv, mainlineMove.metadata.comment);
	ASSERT_EQ(1U, mainlineMove.childVariations.size());
	EXPECT_EQ("Queen pawn alternative"sv,
	          mainlineMove.childVariations[0].initialComment);

	auto const& variationMove =
	    mainlineMove.childVariations[0].line.moves[0];
	EXPECT_EQ("After d4"sv, variationMove.metadata.comment);
}

TEST(Test_Game, moveCommentReadsCoreCommentAtCurrentLocation) {
	scid::database::Game game;
	setCurrentComment(game, "Before the first move");
	game.coreGame().setInitialComment("Core initial comment");
	EXPECT_EQ("Core initial comment",
	          std::string(scid::database::currentMoveComment(game)));

	addMove(game, makeCurrentMove(game, scid::database::E2,
	                              scid::database::E4));
	scid::core::MovetextCursor mainlineCursor(game.coreGame());
	ASSERT_TRUE(mainlineCursor.restore(game.coreLocation()));
	scid::core::MoveMetadata metadata;
	metadata.comment = "Core previous move comment";
	ASSERT_TRUE(mainlineCursor.setPreviousMoveMetadata(std::move(metadata)));
	EXPECT_EQ("Core previous move comment",
	          std::string(scid::database::currentMoveComment(game)));

	addVariation(game);
	scid::core::MovetextCursor variationCursor(game.coreGame());
	ASSERT_TRUE(variationCursor.restore(game.coreLocation()));
	ASSERT_TRUE(variationCursor.setCurrentVariationInitialComment(
	    "Core variation comment"));
	EXPECT_EQ("Core variation comment",
	          std::string(scid::database::currentMoveComment(game)));
}

TEST(Test_Game, coreGameMirrorsStrip) {
	scid::database::Game game;
	setCurrentComment(game, "Before the first move");
	addMove(game, makeCurrentMove(game, scid::database::E2,
	                              scid::database::E4));
	ASSERT_TRUE(addCurrentNag(game, scid::core::NAG_GoodMove));
	setCurrentComment(game, "After e4");
	addVariation(game);
	setCurrentComment(game, "Alternative");
	addMove(game, makeCurrentMove(game, scid::database::D2,
	                              scid::database::D4));

	stripMovetext(game, true, true, true);

	auto const& movetext = game.coreGame().movetext();
	EXPECT_TRUE(movetext.initialComment.empty());
	ASSERT_EQ(1U, movetext.mainline.moves.size());
	auto const& move = movetext.mainline.moves[0];
	EXPECT_TRUE(move.metadata.comment.empty());
	EXPECT_TRUE(move.metadata.nags.empty());
	EXPECT_TRUE(move.childVariations.empty());
}

TEST(Test_Game, coreGameMirrorsInitialMovetextComment) {
	using namespace std::literals;

	scid::database::Game game;
	setCurrentComment(game, "Before the first move");

	EXPECT_EQ("Before the first move"sv, game.coreGame().initialComment());

	std::string encoded;
	scid::core::pgn::encode_game(game.coreGame(), encoded);

	auto expected = "[Event\0\"\"]\n"sv
	                "[Site\0\"\"]\n"sv
	                "[Date\0\"????.??.??\"]\n"sv
	                "[Round\0\"\"]\n"sv
	                "[White\0\"\"]\n"sv
	                "[Black\0\"\"]\n"sv
	                "[Result\0\"*\"]\n"sv
	                "\n"sv
	                "{Before the first move}\n"sv
	                "*\n"sv;
	EXPECT_EQ(expected, encoded);
}

TEST(Test_Game, coreGameCanBeEncodedAsPlainPgnWithoutStoredSan) {
	using namespace std::literals;

	std::string_view pgn =
	    "1.d4! {Best by test} (1.e4 e5 ( 1...c5)) (1.c4) 1...d5 2.c4";
	scid::database::Game game;
	scid::database::pgn::parse_game({pgn.data(), pgn.data() + pgn.size()},
	                                scid::database::PgnVisitor{game});

	std::string encoded;
	scid::core::pgn::encode_game(game.coreGame(), encoded);

	auto expected = "[Event\0\"\"]\n"sv
	                "[Site\0\"\"]\n"sv
	                "[Date\0\"????.??.??\"]\n"sv
	                "[Round\0\"\"]\n"sv
	                "[White\0\"\"]\n"sv
	                "[Black\0\"\"]\n"sv
	                "[Result\0\"*\"]\n"sv
	                "\n"sv
	                "1.d4\0$1\0{Best by test}\0"sv
	                "(1.e4\0e5\0(1...c5))\0"sv
	                "(1.c4)\0"sv
	                "1...d5\0"sv
	                "2.c4\n"sv
	                "*\n"sv;
	EXPECT_EQ(expected, encoded);
}

TEST(Test_Game, currentPositionUci_fen) {
	std::string_view pgn =
	    "[FEN 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198]\n"
	    "198.Kd2 ( 198.Nxd3 a1=R+ 199.Kd2 cxd3 )198...a1=Q 199.Ke3 Qe1+ 0-1";
	scid::database::Game game;
	scid::database::pgn::parse_game({pgn.data(), pgn.data() + pgn.size()},
	                                scid::database::PgnVisitor{game});

	const std::pair<unsigned, const char*> expected[] = {
	    // clang-format off
	    {0, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves"},
	    {1, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves"},
	    {2, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves c1d2"},
	    {3, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves"},
	    {4, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves f2d3"},
	    {5, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves f2d3 a2a1r"},
	    {6, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves f2d3 a2a1r c1d2"},
	    {7, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves f2d3 a2a1r c1d2 c4d3"},
	    {8, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves c1d2 a2a1q"},
	    {9, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves c1d2 a2a1q d2e3"},
	    {10, "position fen 8/8/8/8/2p5/1k1p4/p4N2/2K5 w - - 0 198 moves c1d2 a2a1q d2e3 a1e1"}
	    // clang-format on
	};
	for (auto [pos, str] : expected) {
		ASSERT_TRUE(toPgnLocation(game, pos));
		EXPECT_EQ(str, scid::core::notation::currentPositionUci(
		                   game.coreGame(), game.coreLocation()));
	}
}

TEST(Test_Game, illegalPGN_Castling) {
	std::string_view pgn = "1.e4 e5 2.Nf3 Nf6 3.Be2 Be7 4.O-O O-O 5.O-O";
	scid::database::Game game;
	scid::database::PgnParseLog pgnLog;
	EXPECT_FALSE(scid::database::pgnParseGame(pgn.data(), pgn.size(), game, pgnLog));
	EXPECT_FALSE(pgnLog.log.empty());
	{
		scid::core::GameCursor cursor(game.coreGame());
		cursor.toEnd();
		game.restoreLocation(cursor.location());
	}
	EXPECT_STREQ(
	    currentFen(game).c_str(),
	    "rnbq1rk1/ppppbppp/5n2/4p3/4P3/5N2/PPPPBPPP/RNBQ1RK1 w - - 6 5");
	{
		scid::core::GameCursor cursor(game.coreGame());
		ASSERT_TRUE(cursor.restore(game.coreLocation()));
		ASSERT_TRUE(cursor.previous());
		game.restoreLocation(cursor.location());
	}
	EXPECT_STREQ(
	    currentFen(game).c_str(),
	    "rnbqk2r/ppppbppp/5n2/4p3/4P3/5N2/PPPPBPPP/RNBQ1RK1 b kq - 5 4");
}

TEST(Test_Game, illegalPGN_KingCapture) {
	std::string_view pgn = "1.d4 e6 2.e4 Bb4+ 3.-- Be1";
	scid::database::Game game;
	scid::database::PgnParseLog pgnLog;
	EXPECT_FALSE(scid::database::pgnParseGame(pgn.data(), pgn.size(), game, pgnLog));
	EXPECT_FALSE(pgnLog.log.empty());
	{
		scid::core::GameCursor cursor(game.coreGame());
		cursor.toEnd();
		game.restoreLocation(cursor.location());
	}
	EXPECT_STREQ(
	    currentFen(game).c_str(),
	    "rnbqk1nr/pppp1ppp/4p3/8/1b1PP3/8/PPP2PPP/RNBQKBNR w KQkq - 1 3");
}

namespace {

/// Replace the move after the first comment with @e movecode
auto make_invalid(unsigned char movecode, std::string_view pgn) {
	std::vector<unsigned char> data;
	scid::database::Game g;
	scid::database::pgn::parse_game({pgn.data(), pgn.data() + pgn.size()},
	                                scid::database::PgnVisitor{g});
	scid::database::game_storage::encode(g.coreGame(), g.scidFlags(), data);
	auto comment_tag = std::find(data.begin(), data.end(), 12);
	if (comment_tag != data.end())
		*++comment_tag = movecode;
	return data;
}

template <typename DataT> std::string decode_gameview(DataT const& data) {
	auto bbuf = scid::database::ByteBuffer{data.data() + 1, data.size()};
	auto fen = bbuf.decodeStartBoard().second;
	if (fen) {
		scid::database::Position startPos;
		startPos.ReadFromFEN(fen);
		return scid::database::GameView(bbuf, startPos).getMoveSAN(0, 99);
	}
	return scid::database::GameView(bbuf).getMoveSAN(0, 99);
}

template <typename DataT> std::string decode_game(DataT const& data) {
	auto bbuf = scid::database::ByteBuffer{data.data(), data.size()};
	scid::database::Game game;
	game.clear();
	scid::database::game_storage::decodeMovesOnly(game.coreGame(), bbuf);
	game.restoreLocation(scid::core::MovetextLocation{});
	std::string moves;
	do {
		moves += ' ';
		moves.append(nextLegacySan(game));
		scid::core::GameCursor cursor(game.coreGame());
		EXPECT_TRUE(cursor.restore(game.coreLocation()));
		if (!cursor.next())
			break;
		game.restoreLocation(cursor.location());
	} while (true);
	moves.erase(0, 1);
	return moves;
}
} // namespace

TEST(Test_Game, illegalSCID4_Castling) {
	// The castling rook's index may change when another piece is captured.
	auto change_idx = make_invalid(
	    0, // unchanged, valid sequence
	    "[FEN kr6/8/8/8/8/8/8/1NB1K2R b K - 0 1]\n 1...Rxb1 2.O-O Kb7 3.Rf7+");
	EXPECT_EQ(decode_game(change_idx), "Rxb1 O-O Kb7 Rf7+ ");
	EXPECT_EQ(decode_gameview(change_idx), "1...Rxb1  2.O-O Kb7  3.Rf7+");

	// Chess960. Allowed by gameview.
	auto chess960 = make_invalid(
	    9, // replace null-move with long castle
	    "[FEN bnrbkrqn/pppppppp/8/8/8/8/PPPPPPPP/BNRBKRQN w KQkq - 0 1]\n"
	    "1.b3 Ng6 2.e4 e5 3.Ng3 Nc6 4.f3 Bg5 5.Be2 a6 6.Nc3 d6 7.Nd5 {_} -- "
	    "8.Nf5");
	EXPECT_EQ(decode_game(chess960),
	          "b3 Ng6 e4 e5 Ng3 Nc6 f3 Bg5 Be2 a6 Nc3 d6 Nd5 O-O-O Nf5 ");
	EXPECT_EQ(decode_gameview(chess960),
	          "1.b3 Ng6  2.e4 e5  3.Ng3 Nc6  4.f3 Bg5  5.Be2 a6  6.Nc3 d6  "
	          "7.Nd5 O-O-O  8.Nf5");

	// Illegal castling. Allowed by both gameview and game.
	// The chess rules for castling (king not in check, empty squares between
	// the rook and the king final positions) are not enforced.
	auto obstacles = make_invalid(9, // replace 4.h4 with O-O-O
	                              "1.d4 d5 2.Qd3 Nf6 3.Bg5 Nc6 {_} 4.h4");
	EXPECT_EQ(decode_game(obstacles), "d4 d5 Qd3 Nf6 Bg5 Nc6 O-O-O ");
	EXPECT_EQ(decode_gameview(obstacles),
	          "1.d4 d5  2.Qd3 Nf6  3.Bg5 Nc6  4.O-O-O");

	auto check = make_invalid(
	    10, // replace 5...h5 with O-O
	    "1.d4 d5 2.Nf3 e6 3.e3 Nf6 4.Nc3 Be7 5.Bb5+ {_} h5");
	EXPECT_EQ(decode_game(check), "d4 d5 Nf3 e6 e3 Nf6 Nc3 Be7 Bb5+ O-O ");
	EXPECT_EQ(decode_gameview(check),
	          "1.d4 d5  2.Nf3 e6  3.e3 Nf6  4.Nc3 Be7  5.Bb5+ O-O");

	// Castle twice. Allowed by gameview: no changes to the board; the notations
	// is wrongly reported as O-O-O because the rook is to the left of the king.
	auto twice = make_invalid(
	    10, // replace 5.a4 with O-O
	    "1.e4 e5 2.Nf3 Nf6 3.Be2 Be7 4.O-O O-O {_} 5.a4 a5 6.Kh1");
	EXPECT_EQ(decode_game(twice), "e4 e5 Nf3 Nf6 Be2 Be7 O-O O-O ");
	EXPECT_EQ(decode_gameview(twice),
	          "1.e4 e5  2.Nf3 Nf6  3.Be2 Be7  4.O-O O-O  5.O-O-O a5  6.Kh1");

	// Moved rook. Allowed by gameview: the rook is moved to D1
	auto moved_rook = make_invalid(
	    9, // replace 2.c4 with O-O-O
	    "[FEN r3k2r/2p2p2/2pq1p2/p2pp2p/P2PP2P/2PQ1P2/2P2P2/R3K2R w KQkq]\n"
	    "1.Ra3 Rh6 {_} 2.c4 O-O-O 3. Ra2");
	EXPECT_EQ(decode_game(moved_rook), "Ra3 Rh6 ");
	EXPECT_EQ(decode_gameview(moved_rook), "1.Ra3 Rh6  2.O-O O-O-O  3.Rd2");

	// No rook
	auto no_rook = make_invalid(
	    9, // replace 2.c4 with O-O-O
	    "[FEN 2k2r2/ppp5/8/8/8/2P1N2R/5PP1/4K3 b - - 0 1]\n"
	    "1...a5 {_} 2.c4 a4 3.Rh5");
	EXPECT_EQ(decode_game(no_rook), "a5 ");
	EXPECT_EQ(decode_gameview(no_rook), "1...a5");

	// Captured rook
	auto captured_rook = make_invalid(
	    9, // replace 4..f5 with O-O-O
	    "[FEN r3k2r/2p2p2/2pq1p2/p2pp2p/P2PP2P/2PQ1P2/2P2P2/R3K2R w KQkq]\n"
	    "1.Qa6 c5 2.Qxa8+ Qd8 3.Qc6+ Qd7 4.O-O {_} f5 5.Kh1 Kd8");
	EXPECT_EQ(decode_game(captured_rook), "Qa6 c5 Qxa8+ Qd8 Qc6+ Qd7 O-O ");
	EXPECT_EQ(decode_gameview(captured_rook),
	          "1.Qa6 c5  2.Qxa8+ Qd8  3.Qc6+ Qd7  4.O-O");

	// occupied squares. Allowed by gameview: both pieces will be present on the
	// same square. The previous piece can be moved.
	auto occ_king = make_invalid(
	    0, // replace 2.Nf3 with a null move
	    "1.e4 e5 {_} 2.Nf3 Nf6 3.Be2 Be7 4.O-O O-O 5.Kh1 a5 6.Nxe5");
	EXPECT_EQ(decode_game(occ_king), "e4 e5 -- Nf6 Be2 Be7 ");
	EXPECT_EQ(decode_gameview(occ_king),
	          "1.e4 e5  2.-- Nf6  3.Be2 Be7  4.O-O O-O  5.Kh1 a5  6.Nf3");

	auto occ_rook = make_invalid(
	    0, // replace 3...Be7 with a null move
	    "1.e4 e5 2.Nf3 Nf6 3.Be2 {_} Be7 4.O-O O-O 5.Kh1");
	EXPECT_EQ(decode_game(occ_rook), "e4 e5 Nf3 Nf6 Be2 -- O-O ");
	EXPECT_EQ(decode_gameview(occ_rook),
	          "1.e4 e5  2.Nf3 Nf6  3.Be2 --  4.O-O O-O  5.Kh1");
}

TEST(Test_Game, illegalSCID4_KingCapture) {
	auto data = make_invalid(0, // null move
	                         "1.d4 e6 2.e4 Bb4+ {_} 3.Ke2 Be1 4.Ke1");
	EXPECT_EQ(decode_gameview(data), "1.d4 e6  2.e4 Bb4+  3.-- Bxe1  4.a4");
	EXPECT_EQ(decode_game(data), "d4 e6 e4 Bb4+ -- ");
}

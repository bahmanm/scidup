#include "scidup/core/game.h"
#include "scidup/core/game_cursor.h"
#include "scidup/core/nags.h"
#include "scidup/core/pgn/traversal.h"
#include "scidup/database/scidbase.h"
#include "scidup_app_legacy_pgn.h"
#include "scidup_app_nag_format.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <random>

namespace {

const char* gameUTF8 = SCIDUP_DATABASE_TEST_RESOURCES_DIR "res_gameUTF8.pgn";
const char* gameLatin1 =
    SCIDUP_DATABASE_TEST_RESOURCES_DIR "res_gameLatin1.pgn";
const char* gameLatin1Conv =
    SCIDUP_DATABASE_TEST_RESOURCES_DIR "res_gameLatin1expected.pgn";

scidup::app::LegacyGameEncodeOptions scidFlagsPgnOptions() {
	return {
	    PGN_STYLE_TAGS | PGN_STYLE_VARS | PGN_STYLE_COMMENTS |
	        PGN_STYLE_SCIDFLAGS,
	    scidup::app::PGN_FORMAT_Plain,
	    0,
	};
}

scid::core::GameCursor coreCursor(const scid::core::Game& game,
                                  scid::core::MovetextLocation location) {
	scid::core::GameCursor cursor(game);
	[[maybe_unused]] const bool restored = cursor.restore(location);
	EXPECT_TRUE(restored);
	return cursor;
}

bool toPgnLocation(scid::core::Game& game,
                   scid::core::MovetextLocation& currentLocation,
                   unsigned location) {
	scid::core::GameCursor cursor(game);
	if (!scid::core::pgn::seekLocation(cursor, location))
		return false;
	currentLocation = cursor.location();
	return true;
}

unsigned pgnOffset(const scid::core::Game& game,
                   scid::core::MovetextLocation location) {
	return scid::core::pgn::offsetOf(coreCursor(game, location));
}

std::string nextSan(const scid::core::Game& game,
                    scid::core::MovetextLocation location) {
	scid::core::GameCursor cursor(game);
	EXPECT_TRUE(cursor.restore(location));
	auto position = cursor.currentPosition();
	auto* move = cursor.nextMove();
	if (!position || !move)
		return {};

	return position->makeSan(move->spec, scid::core::SAN_MATETEST);
}

} // namespace

TEST(Test_LegacyPgn, CloneEncodingMatchesOriginal) {
	for (auto filename : {gameUTF8, gameLatin1, gameLatin1Conv}) {
		scid::database::scidBaseT dbase;
		ASSERT_EQ(scid::core::OK,
		          dbase.open("PGN", scid::database::FMODE_Both, filename));
		ASSERT_NE(nullptr, dbase.getIndexEntry_bounds(0));

		scid::core::Game game;
		std::array<char, 22> scidFlags{};
		ASSERT_EQ(scid::core::OK,
		          dbase.loadGame(*dbase.getIndexEntry(0), game,
		                         scidFlags.data(), scidFlags.size()));
		scid::core::MovetextLocation location;

		std::mt19937 re(std::random_device{}());
		ASSERT_TRUE(toPgnLocation(
		    game, location, std::uniform_int_distribution<unsigned>{0, 500}(re)));

		scid::core::Game clone{game};
		auto cloneLocation = location;

		ASSERT_EQ(pgnOffset(clone, cloneLocation), pgnOffset(game, location));
		ASSERT_EQ(nextSan(game, location), nextSan(clone, cloneLocation));

		auto pgnGame = scidup::app::legacy_pgn::encode(
		    game, scidFlags.data(), scidFlagsPgnOptions(), 75, true);
		auto pgnClone = scidup::app::legacy_pgn::encode(
		    clone, scidFlags.data(), scidFlagsPgnOptions(), 75, true);

		ASSERT_TRUE(std::equal(pgnClone.first, pgnClone.first + pgnClone.second,
		                       pgnGame.first, pgnGame.first + pgnGame.second));
	}
}

TEST(Test_LegacyPgn, EncodeDoesNotMutateOptions) {
	scid::core::Game game;
	const auto options = scidup::app::LegacyGameEncodeOptions{
	    PGN_STYLE_TAGS | PGN_STYLE_COLUMN,
	    scidup::app::PGN_FORMAT_Plain,
	    0,
	};

	auto first = scidup::app::legacy_pgn::encode(game, "", options, 75, true);
	std::string firstPgn(first.first, first.second);
	auto second = scidup::app::legacy_pgn::encode(game, "", options, 75, true);

	EXPECT_EQ(firstPgn, std::string(second.first, second.second));
}

TEST(Test_LegacyPgn, EncodeOptionsFormatFromString) {
	scidup::app::gameFormatT format = scidup::app::PGN_FORMAT_Color;

	ASSERT_TRUE(scidup::app::LegacyGameEncodeOptions::legacyFormatFromString(
	    "pgn", &format));
	EXPECT_TRUE(
	    (scidup::app::LegacyGameEncodeOptions{0, format, 0}.isPlainFormat()));

	ASSERT_TRUE(scidup::app::LegacyGameEncodeOptions::legacyFormatFromString(
	    "html", &format));
	EXPECT_TRUE(
	    (scidup::app::LegacyGameEncodeOptions{0, format, 0}.isHtmlFormat()));

	ASSERT_TRUE(scidup::app::LegacyGameEncodeOptions::legacyFormatFromString(
	    "latex", &format));
	const auto options = scidup::app::LegacyGameEncodeOptions{0, format, 0};
	EXPECT_TRUE(options.isLatexFormat());

	EXPECT_FALSE(scidup::app::LegacyGameEncodeOptions::legacyFormatFromString(
	    "unknown", &format));
}

TEST(Test_LegacyPgn, NagFormatKeepsStyledExportOutput) {
	char nagText[20] = {};
	scidup::app::game_printNag(scid::core::NAG_Diagram, nagText, true,
	                           scidup::app::PGN_FORMAT_HTML);
	EXPECT_STREQ("<i>(D)</i>", nagText);

	scidup::app::game_printNag(scid::core::NAG_GoodMove, nagText, false,
	                           scidup::app::PGN_FORMAT_LaTeX);
	EXPECT_STREQ("\\$1", nagText);
}

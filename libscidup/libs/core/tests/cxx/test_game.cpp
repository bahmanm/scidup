#include "scidup/core/game.h"

#include <gtest/gtest.h>

namespace {

TEST(CoreGameTest, DefaultsToEmptyMetadataAndStandardStart) {
	scid::core::Game game;

	EXPECT_TRUE(game.event().empty());
	EXPECT_TRUE(game.site().empty());
	EXPECT_TRUE(game.round().empty());
	EXPECT_TRUE(game.white().name.empty());
	EXPECT_TRUE(game.black().name.empty());
	EXPECT_EQ(scid::database::ZERO_DATE, game.date());
	EXPECT_EQ(scid::database::ZERO_DATE, game.eventDate());
	EXPECT_EQ(scid::database::RESULT_None, game.result());
	EXPECT_TRUE(game.extraTags().empty());
	EXPECT_FALSE(game.hasNonStandardStart());
	EXPECT_EQ(nullptr, game.startPosition());
	EXPECT_EQ(0, game.initialPlyCounter());
}

TEST(CoreGameTest, StandardTagsMapToMetadataAndExtraTagsStaySeparate) {
	scid::core::Game game;

	game.addTag("Event", "Candidates");
	game.addTag("Site", "Toronto");
	game.addTag("White", "Player A");
	game.addTag("Black", "Player B");
	game.addTag("Round", "7");
	game.addTag("Annotator", "Example");

	EXPECT_EQ("Candidates", game.event());
	EXPECT_EQ("Toronto", game.site());
	EXPECT_EQ("Player A", game.white().name);
	EXPECT_EQ("Player B", game.black().name);
	EXPECT_EQ("7", game.round());

	ASSERT_EQ(1U, game.extraTags().size());
	EXPECT_EQ("Annotator", game.extraTags()[0].first);
	EXPECT_EQ("Example", game.extraTags()[0].second);
	ASSERT_NE(nullptr, game.findExtraTag("Annotator"));
	EXPECT_EQ("Example", *game.findExtraTag("Annotator"));

	game.removeExtraTag("Annotator");
	EXPECT_EQ(nullptr, game.findExtraTag("Annotator"));
	EXPECT_TRUE(game.extraTags().empty());
}

TEST(CoreGameTest, FindOrCreateTagReusesExistingValue) {
	scid::core::Game game;
	game.findOrCreateTag("White") = "Player A";
	game.findOrCreateTag("Annotator") = "Example";
	game.findOrCreateTag("Annotator").append(" 2");

	EXPECT_EQ("Player A", game.white().name);
	ASSERT_EQ(1U, game.extraTags().size());
	EXPECT_EQ("Annotator", game.extraTags()[0].first);
	EXPECT_EQ("Example 2", game.extraTags()[0].second);
}

TEST(CoreGameTest, StoresRatingsDatesAndResult) {
	scid::core::Game game;
	scid::core::Player white;
	white.name = "Player A";
	white.rating.value = 2800;
	white.rating.type = scid::database::RATING_Rapid;
	scid::core::Player black;
	black.name = "Player B";
	black.rating.value = 2650;

	game.setWhite(white);
	game.setBlack(black);
	game.setDate(scid::database::date_parsePGNTag("2018.06.11", 10));
	game.setEventDate(scid::database::date_parsePGNTag("2018.06.01", 10));
	game.setResult(scid::database::RESULT_White);

	EXPECT_EQ("Player A", game.white().name);
	EXPECT_EQ(2800, game.white().rating.value);
	EXPECT_EQ(scid::database::RATING_Rapid, game.white().rating.type);
	EXPECT_EQ("Player B", game.black().name);
	EXPECT_EQ(2650, game.black().rating.value);
	EXPECT_EQ(scid::database::date_parsePGNTag("2018.06.11", 10), game.date());
	EXPECT_EQ(scid::database::date_parsePGNTag("2018.06.01", 10),
	          game.eventDate());
	EXPECT_EQ(scid::database::RESULT_White, game.result());
	EXPECT_EQ("1-0", game.resultString());
	EXPECT_EQ(2725, game.averageRating());
	EXPECT_EQ(game.date(), game.header().event.date);
	EXPECT_EQ(game.white().rating.value, game.header().white.rating.value);
}

TEST(CoreGameTest, SetsPlayerNamesAndRatingsDirectly) {
	scid::core::Game game;

	game.setWhiteName("Player A");
	game.setBlackName("Player B");
	game.setWhiteRating({2800, scid::database::RATING_Rapid});
	game.setBlackRating({0, scid::database::RATING_Elo});

	EXPECT_EQ("Player A", game.white().name);
	EXPECT_EQ("Player B", game.black().name);
	EXPECT_EQ(2800, game.white().rating.value);
	EXPECT_EQ(scid::database::RATING_Rapid, game.white().rating.type);
	EXPECT_EQ(0, game.averageRating());
}

TEST(CoreGameTest, SetStartFenStoresNonStandardStartPosition) {
	const char* fen = "8/K7/8/8/7k/8/8/8 w - - 45 25";
	scid::core::Game game;

	ASSERT_EQ(scid::database::OK, game.setStartFen(fen));

	EXPECT_TRUE(game.hasNonStandardStart());
	ASSERT_NE(nullptr, game.startPosition());
	EXPECT_EQ(48, game.initialPlyCounter());

	char printed[256];
	ASSERT_TRUE(game.hasNonStandardStart(printed, sizeof(printed)));
	EXPECT_STREQ(fen, printed);
}

TEST(CoreGameTest, InvalidStartFenLeavesExistingStartPositionUnchanged) {
	const char* fen = "8/K7/8/8/7k/8/8/8 w - - 45 25";
	scid::core::Game game;
	ASSERT_EQ(scid::database::OK, game.setStartFen(fen));

	EXPECT_NE(scid::database::OK, game.setStartFen("invalid"));

	char printed[256];
	ASSERT_TRUE(game.hasNonStandardStart(printed, sizeof(printed)));
	EXPECT_STREQ(fen, printed);
}

} // namespace

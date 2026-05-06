#include "scidup/eco/book.h"
#include "scidup/database/misc.h"
#include "scidup/database/position.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace {

std::filesystem::path testFilePath(std::string_view stem) {
	auto path = std::filesystem::temp_directory_path();
	path /= std::string(stem) + "_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + ".eco";
	return path;
}

void writeFile(const std::filesystem::path& path, std::string_view contents) {
	std::ofstream out(path);
	ASSERT_TRUE(out.good());
	out << contents;
}

void play(scid::database::Position& position, std::string_view san) {
	scid::database::simpleMoveT sm;
	const char* begin = san.data();
	const char* end = begin + san.size();
	ASSERT_EQ(scid::database::OK, position.ParseMove(&sm, begin, end));
	position.DoSimpleMove(sm);
}

class EcoBookTest : public ::testing::Test {
protected:
	std::filesystem::path path_;

	void SetUp() override {
		path_ = testFilePath(::testing::UnitTest::GetInstance()->current_test_info()->name());
	}

	void TearDown() override {
		std::error_code ec;
		std::filesystem::remove(path_, ec);
	}
};

constexpr std::string_view kEcoFile = R"eco(
# Test ECO file
A00a "Start position" *
B20 "Sicilian Defence" 1.e4 c5 *
C50a "Italian Game" 1.e4 e5 2.Nf3 Nc6 3.Bc4 *
)eco";

} // namespace

TEST_F(EcoBookTest, MissingFileReturnsOpenError) {
	auto [err, book] = scidup::eco::Book::load(path_);

	EXPECT_EQ(scidup::eco::ERROR_FileOpen, err);
	EXPECT_EQ(0u, book.size());
}

TEST_F(EcoBookTest, LoadIndexesPositionsAndClassifiesKnownLines) {
	writeFile(path_, kEcoFile);

	auto [err, book] = scidup::eco::Book::load(path_);
	ASSERT_EQ(scidup::eco::OK, err);
	EXPECT_EQ(3u, book.size());

	scid::database::Position position;
	position.StdStart();
	EXPECT_EQ("A00a [Start position]", book.findEcoString(position));
	EXPECT_EQ(scid::database::eco_FromString("A00a"), book.findEco(position));

	play(position, "e4");
	play(position, "c5");
	EXPECT_EQ("B20 [Sicilian Defence]", book.findEcoString(position));
	EXPECT_EQ(scid::database::eco_FromString("B20"), book.findEco(position));
}

TEST_F(EcoBookTest, LinesWithPrefixReturnsStructuredRows) {
	writeFile(path_, kEcoFile);

	auto [err, book] = scidup::eco::Book::load(path_);
	ASSERT_EQ(scidup::eco::OK, err);

	auto lines = book.linesWithPrefix("C50");
	ASSERT_EQ(1u, lines.size());
	EXPECT_EQ("C50a", lines[0].code);
	EXPECT_EQ("Italian Game", lines[0].name);
	EXPECT_EQ("1.e4 e5 2.Nf3 Nc6 3.Bc4 ", lines[0].moves);
}

TEST_F(EcoBookTest, CorruptFileReturnsCorruptError) {
	writeFile(path_, "A00 \"Broken\" 1.NotAMove *\n");

	auto [err, book] = scidup::eco::Book::load(path_);

	EXPECT_EQ(scidup::eco::ERROR_Corrupt, err);
	EXPECT_EQ(0u, book.size());
}

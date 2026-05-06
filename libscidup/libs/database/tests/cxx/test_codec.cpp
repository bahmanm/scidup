/*
* Copyright (C) 2016 Fulvio Benini

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
#include "codec.h"
#include "scidup/database/game.h"
#include "scidup/database/index.h"
#include "scidup/database/misc.h"
#include "scidup/database/namebase.h"
#include <cstring>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

namespace {

scid::database::fileModeT fmodes[] = {scid::database::FMODE_Create, scid::database::FMODE_ReadOnly, scid::database::FMODE_WriteOnly,
                      scid::database::FMODE_Both};
const char* filename = "codecbase";

scid::database::CodecType codecs[] = {scid::database::CodecType::Memory, scid::database::CodecType::Scid4,
                                  scid::database::CodecType::Scid5, scid::database::CodecType::Pgn};

std::vector<std::pair<scid::database::CodecType, std::string>> unsupportedVec = {
    {scid::database::CodecType::Memory, "FMODE" + std::to_string(scid::database::FMODE_None)},
    {scid::database::CodecType::Memory, "FMODE" + std::to_string(scid::database::FMODE_ReadOnly)},
    {scid::database::CodecType::Memory, "FMODE" + std::to_string(scid::database::FMODE_WriteOnly)},
    {scid::database::CodecType::Memory, "FMODE" + std::to_string(scid::database::FMODE_Both)},

    {scid::database::CodecType::Scid4, "FMODE" + std::to_string(scid::database::FMODE_None)},
    {scid::database::CodecType::Scid4, "FMODE" + std::to_string(scid::database::FMODE_WriteOnly)},
    {scid::database::CodecType::Scid4, "empty_filename"},

    {scid::database::CodecType::Scid5, "FMODE" + std::to_string(scid::database::FMODE_None)},
    {scid::database::CodecType::Scid5, "FMODE" + std::to_string(scid::database::FMODE_WriteOnly)},
    {scid::database::CodecType::Scid5, "empty_filename"},

    {scid::database::CodecType::Pgn, "FMODE" + std::to_string(scid::database::FMODE_None)},
    {scid::database::CodecType::Pgn, "saveGame_game"},
    {scid::database::CodecType::Pgn, "empty_filename"}};

class Supports {
	scid::database::CodecType dbtype_;

public:
	Supports(scid::database::CodecType dbtype) : dbtype_(dbtype) {}

	bool operator()(const std::string& feature) const {
		auto it = std::find(unsupportedVec.begin(), unsupportedVec.end(),
		                    std::make_pair(dbtype_, feature));
		return it == unsupportedVec.end();
	}
};

template <int nGames, int maxMoves, int maxCommentLen> class GameGenerator {
	typedef std::vector<std::unique_ptr<scid::database::Game>> Vec;
	Vec v_;
	std::vector<std::vector<scid::database::byte>> encoded_;
	std::mt19937 mt_;

public:
	const Vec& get() {
		if (v_.empty()) {
			for (int i = 0; i < nGames; i++) {
				v_.push_back(genGame());
			}
			encodeGames();
		}
		return v_;
	}

	const std::vector<std::vector<scid::database::byte>>& getNative() {
		if (encoded_.empty())
			get();

		return encoded_;
	}

	void cmp(scid::database::ICodecDatabase* codec, const scid::database::Index& idx) {
		auto encoded = getNative();
		ASSERT_EQ(encoded.size(), size_t(idx.GetNumGames()));
		int g = 0;
		for (auto& game : encoded) {
			auto entry = idx.GetEntry(g++);
			auto data = codec->getGameData(entry->GetOffset(),
			                               entry->GetLength());
			ASSERT_TRUE(data);
			ASSERT_EQ(game.size(), entry->GetLength());
			EXPECT_TRUE(std::equal(
			    data.data(), data.data() + entry->GetLength(), game.data()));
		}
	}

private:
	void encodeGames() {
		for (auto& game : v_) {
			game->Encode(encoded_.emplace_back());
		}
	}

	std::unique_ptr<scid::database::Game> genGame() {
		auto res = std::unique_ptr<scid::database::Game>(new scid::database::Game);
		res->GetCurrentPos()->StdStart();
		scid::database::MoveList mlist;
		for (auto i = rand(0, maxMoves); i > 0; --i) {
			res->GetCurrentPos()->GenerateMoves(&mlist, scid::database::EMPTY, scid::database::GEN_ALL_MOVES,
			                                    true);
			if (mlist.Size() == 0)
				break;

			auto move = *mlist.Get(rand(0, mlist.Size() - 1));
			res->GetCurrentPos()->fillMove(move);
			res->AddMove(move);

			if (rand(0, 6) == 0)
				res->SetMoveComment(rand_comment().c_str());

			int varOp = rand(0, 80 + int(res->GetVarLevel()) * 20);
			if (varOp < 20) {
				res->AddVariation();
			} else if (varOp > 80) {
				res->MoveExitVariation();
				res->MoveForward();
			}
		}
		return res;
	}

	int rand(int low, int up) {
		return std::uniform_int_distribution<int>(low, up)(mt_);
	}

	std::string rand_comment() {
		size_t len = rand(0, maxCommentLen);
		std::string res(len, ' ');
		std::uniform_int_distribution<int> dist{33, 122};
		std::generate_n(res.begin(), res.size(),
		                [&]() { return static_cast<char>(dist(mt_)); });
		return res;
	}
};
GameGenerator<1000, 2000, 300> gameGenerator;

template <typename Oper>
void makeDatabase(scid::database::CodecType dbtype, const char* test, Oper op) {
	Supports supports(dbtype);
	if (!supports(test))
		return;

	scid::database::fileModeT fMode = scid::database::FMODE_Create;

	struct Cleanup {
		std::vector<std::string> filenames;

		~Cleanup() {
			for (const auto& fname : filenames) {
				EXPECT_EQ(0, std::remove(fname.c_str()));
			}
		}
	} cleanup;

	{
		scid::database::Index idx;
		scid::database::NameBase nb;
		auto err = scid::database::openCodec(dbtype, fMode, filename, scid::database::Progress(),
		                                &idx, &nb);
		auto codec = std::unique_ptr<scid::database::ICodecDatabase>(err.first);
		ASSERT_NE(nullptr, codec);
		cleanup.filenames = codec->getFilenames();
		ASSERT_EQ(scid::database::OK, err.second);

		op(codec.get(), idx, nb);

		ASSERT_EQ(gameGenerator.get().size(), size_t(idx.GetNumGames()));
		ASSERT_EQ(scid::database::OK, codec->flush());
		gameGenerator.cmp(codec.get(), idx);
	}

	if (supports("FMODE" + std::to_string(scid::database::FMODE_ReadOnly))) {
		scid::database::Index idx;
		scid::database::NameBase nb;
		auto err = scid::database::openCodec(dbtype, scid::database::FMODE_ReadOnly, filename,
		                                scid::database::Progress(), &idx, &nb);
		auto codec = std::unique_ptr<scid::database::ICodecDatabase>(err.first);
		ASSERT_NE(nullptr, codec);
		ASSERT_EQ(scid::database::OK, err.second);
		ASSERT_EQ(gameGenerator.get().size(), size_t(idx.GetNumGames()));

		gameGenerator.cmp(codec.get(), idx);
	}
}

} // end of anonymous namespace

class Test_Codec : public ::testing::TestWithParam<scid::database::CodecType> {};

// Try to get a scid::database::ICodecDatabase pointer for each supported file mode, then test
// the consistency of getType() and getFilenames().
TEST_P(Test_Codec, fileModeT) {
	scid::database::CodecType dbtype = GetParam();
	Supports supports(dbtype);

	struct Cleanup {
		std::vector<std::string> filenames;

		~Cleanup() {
			for (const auto& fname : filenames) {
				EXPECT_EQ(0, std::remove(fname.c_str()));
			}
		}
	} cleanup;

	for (auto& fmode : fmodes) {
		scid::database::Index idx;
		scid::database::NameBase nb;
		auto err = scid::database::openCodec(dbtype, fmode, filename, scid::database::Progress(),
		                                &idx, &nb);
		auto codec = std::unique_ptr<scid::database::ICodecDatabase>(err.first);

		if (supports("FMODE" + std::to_string(fmode))) {
			ASSERT_NE(nullptr, codec);
			EXPECT_EQ(dbtype, codec->getType());
			auto tmp = codec->getFilenames();
			if (cleanup.filenames.empty())
				cleanup.filenames = tmp;
			else
				EXPECT_TRUE(tmp == cleanup.filenames);
		} else {
			EXPECT_EQ(nullptr, codec);
		}
	}
}

TEST_P(Test_Codec, create_emptyfilename) {
	scid::database::CodecType dbtype = GetParam();
	Supports supports(dbtype);

	if (!supports("FMODE" + std::to_string(scid::database::FMODE_Create))) {
		return;
	}

	scid::database::Index idx;
	scid::database::NameBase nb;
	auto err = scid::database::openCodec(dbtype, scid::database::FMODE_Create, "", scid::database::Progress(), &idx,
	                                &nb);
	auto codec = std::unique_ptr<scid::database::ICodecDatabase>(err.first);

	if (!supports("empty_filename")) {
		EXPECT_EQ(nullptr, codec);
	} else {
		EXPECT_NE(nullptr, codec);
		for (const auto& fname : codec->getFilenames()) {
			EXPECT_EQ(0, std::remove(fname.c_str()));
		}
	}
}

// Creates two databases; remove the first one and rename the second to the
// first. This test mimic the process perfomed to finalize the compaction of a
// database.
TEST_P(Test_Codec, rename) {
	scid::database::CodecType dbtype = GetParam();
	Supports supports(dbtype);

	if (!supports("FMODE" + std::to_string(scid::database::FMODE_Create))) {
		return;
	}
	struct Cleanup {
		std::vector<std::string> filenames1, filenames2;

		~Cleanup() {
			for (const auto& fname : filenames1) {
				EXPECT_EQ(0, std::remove(fname.c_str()));
			}
			for (const auto& fname : filenames2) {
				EXPECT_NE(0, std::remove(fname.c_str()));
			}
		}
	} cleanup;

	{
		scid::database::Index idx1, idx2;
		scid::database::NameBase nb1, nb2;
		auto err = scid::database::openCodec(dbtype, scid::database::FMODE_Create, filename,
		                                scid::database::Progress(), &idx1, &nb1);
		auto codec1 = std::unique_ptr<scid::database::ICodecDatabase>(err.first);
		EXPECT_EQ(scid::database::OK, codec1->flush());
		ASSERT_NE(nullptr, codec1);
		ASSERT_EQ(scid::database::OK, err.second);

		std::string renamed_name = std::string(filename) + "__renamed__";
		err = scid::database::openCodec(dbtype, scid::database::FMODE_Create, renamed_name.c_str(),
		                           scid::database::Progress(), &idx2, &nb2);
		auto codec2 = std::unique_ptr<scid::database::ICodecDatabase>(err.first);
		EXPECT_EQ(scid::database::OK, codec2->flush());
		ASSERT_NE(nullptr, codec2);
		ASSERT_EQ(scid::database::OK, err.second);

		cleanup.filenames1 = codec1->getFilenames();
		cleanup.filenames2 = codec2->getFilenames();
		EXPECT_EQ(cleanup.filenames1.size(), cleanup.filenames2.size());
	}

	for (const auto& fname : cleanup.filenames1) {
		EXPECT_EQ(0, std::remove(fname.c_str()));
	}
	for (size_t i = 0, n = cleanup.filenames2.size(); i < n; i++) {
		const char* s1 = cleanup.filenames1[i].c_str();
		const char* s2 = cleanup.filenames2[i].c_str();
		EXPECT_EQ(0, std::rename(s2, s1));
	}

	if (supports("FMODE" + std::to_string(scid::database::FMODE_ReadOnly))) {
		scid::database::Index idx_reopen;
		scid::database::NameBase nb_reopen;
		auto err = scid::database::openCodec(dbtype, scid::database::FMODE_ReadOnly, filename,
		                                scid::database::Progress(), &idx_reopen, &nb_reopen);
		auto codec3 = std::unique_ptr<scid::database::ICodecDatabase>(err.first);
		ASSERT_NE(nullptr, codec3);
		ASSERT_EQ(scid::database::OK, err.second);

		auto filenames3 = codec3->getFilenames();
		EXPECT_TRUE(cleanup.filenames1 == filenames3);
	}
}

INSTANTIATE_TEST_SUITE_P(CodecDatabase, Test_Codec,
                         ::testing::ValuesIn(codecs));

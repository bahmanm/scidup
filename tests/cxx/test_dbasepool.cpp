/*
 * Copyright (C) 2026
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

#include "dbasepool.h"
#include "scid/database/scidbase.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

class Test_DBasePool : public ::testing::Test {
protected:
	void SetUp() override {
		tempDir_ = std::filesystem::temp_directory_path() / uniqueDirName();
		ASSERT_TRUE(std::filesystem::create_directories(tempDir_));
		DBasePool::init();
	}

	void TearDown() override {
		DBasePool::closeAll();
		std::error_code ec;
		std::filesystem::remove_all(tempDir_, ec);
	}

	static std::string uniqueDirName() {
		static size_t counter = 0;
		return "scidup_test_dbasepool_" + std::to_string(++counter);
	}

	std::string dbPath(const char* name) const {
		return (tempDir_ / name).string();
	}

	scid::database::scidBaseT* openScid4(const std::string& filename) {
		auto* slot = DBasePool::getFreeSlot();
		EXPECT_NE(nullptr, slot);
		if (slot == nullptr)
			return nullptr;

		EXPECT_EQ(scid::core::OK, slot->open("SCID4", scid::database::FMODE_Create, filename.c_str()));
		return slot;
	}

	std::filesystem::path tempDir_;
};

} // namespace

TEST_F(Test_DBasePool, clipbase_is_initialised) {
	const int clipbaseHandle = DBasePool::getClipBase();
	EXPECT_EQ(9, clipbaseHandle);

	auto* clipbase = DBasePool::getBase(clipbaseHandle);
	ASSERT_NE(nullptr, clipbase);
	EXPECT_TRUE(clipbase->isOpen());
	EXPECT_EQ("<clipbase>", clipbase->getFileName());

	std::vector<int> handles = DBasePool::getHandles();
	ASSERT_EQ(1U, handles.size());
	EXPECT_EQ(clipbaseHandle, handles.front());
}

TEST_F(Test_DBasePool, open_two_databases_and_find_them_by_filename) {
	auto firstPath = dbPath("first_db");
	auto secondPath = dbPath("second_db");

	auto* first = openScid4(firstPath);
	auto* second = openScid4(secondPath);
	ASSERT_NE(nullptr, first);
	ASSERT_NE(nullptr, second);

	EXPECT_TRUE(first->isOpen());
	EXPECT_TRUE(second->isOpen());
	EXPECT_EQ(1, DBasePool::find(first->getFileName().c_str()));
	EXPECT_EQ(2, DBasePool::find(second->getFileName().c_str()));
	EXPECT_EQ(0, DBasePool::find(dbPath("missing_db").c_str()));

	auto* firstHandle = DBasePool::getBase(1);
	auto* secondHandle = DBasePool::getBase(2);
	ASSERT_EQ(first, firstHandle);
	ASSERT_EQ(second, secondHandle);
	EXPECT_EQ(0, DBasePool::getBase(3));

	std::vector<int> handles = DBasePool::getHandles();
	ASSERT_EQ(3U, handles.size());
	EXPECT_EQ(1, handles[0]);
	EXPECT_EQ(2, handles[1]);
	EXPECT_EQ(DBasePool::getClipBase(), handles[2]);
}

TEST_F(Test_DBasePool, closed_slot_is_reused) {
	auto* first = openScid4(dbPath("first_db"));
	auto* second = openScid4(dbPath("second_db"));
	ASSERT_NE(nullptr, first);
	ASSERT_NE(nullptr, second);

	EXPECT_EQ(1, DBasePool::switchCurrent(first));
	EXPECT_EQ(2, DBasePool::switchCurrent(second));
	EXPECT_EQ(2, DBasePool::switchCurrent());

	first->Close();
	EXPECT_EQ(nullptr, DBasePool::getBase(1));

	auto* reused = DBasePool::getFreeSlot();
	ASSERT_NE(nullptr, reused);
	EXPECT_EQ(first, reused);

	auto thirdPath = dbPath("third_db");
	ASSERT_EQ(scid::core::OK, reused->open("SCID4", scid::database::FMODE_Create, thirdPath.c_str()));
	EXPECT_EQ(1, DBasePool::find(reused->getFileName().c_str()));
	EXPECT_EQ(2, DBasePool::find(second->getFileName().c_str()));
}

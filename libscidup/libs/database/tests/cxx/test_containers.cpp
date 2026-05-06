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

#include "scidup/database/bytebuf.h"
#include "scidup/database/containers.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace {

int nObjects = 0;

struct RefCounted {
	char ch[3];

public:
	RefCounted() { ++nObjects; }

	RefCounted(const RefCounted& v) {
		std::copy_n(v.ch, sizeof ch, ch);
		++nObjects;
	}

	RefCounted& operator=(const RefCounted&) = default;

	RefCounted* clone() { return new RefCounted(*this); }

	~RefCounted() { --nObjects; }
};

} // namespace

TEST(Test_Containers, VectorChunked) {
	scid::database::VectorChunked<RefCounted, 3> v; // 8 elements per chunk

	// Test push_back
	for (size_t i = 0; i < 30; ++i) {
		EXPECT_EQ(i, v.size());
		EXPECT_EQ(size_t(nObjects), v.capacity());
		size_t expCapacity = (i == 0) ? 0 : 8 + 8 * (i / 8);
		EXPECT_EQ(v.capacity(), expCapacity);
		RefCounted tmp;
		tmp.ch[0] = static_cast<char>(i);
		v.push_back(tmp);
	}

	// Test operator[]
	std::vector<RefCounted*> ref;
	for (size_t i = 0; i < 30; ++i) {
		ref.push_back(&v[i]);
	}
	std::vector<const RefCounted*> ref_const;
	for (size_t i = 0; i < 30; ++i) {
		const auto& v_const = v;
		ref_const.push_back(&v_const[i]);
	}

	// Test validity of pointers after grow
	v.resize(55);
	EXPECT_EQ(55U, v.size());
	EXPECT_EQ(size_t(nObjects), v.capacity());
	EXPECT_EQ(56U, v.capacity());
	for (size_t i = 0; i < 30; ++i) {
		EXPECT_EQ(ref[i], &v[i]);
		EXPECT_EQ(ref_const[i], &v[i]);
	}

	// Test access with iterators
	for (size_t i = 30; i < v.size();) {
		auto contiguous = v.contiguous(i);
		RefCounted* it = &v[i];
		for (size_t j = 0; j < contiguous; j++) {
			(*it++).ch[0] = (char)i;
			++i;
		}
	}

	// Test values correctness
	for (int i = 0; i < 55; ++i) {
		EXPECT_EQ(i, v[i].ch[0]);
	}

	// Test memory release
	v.resize(16);
	EXPECT_EQ(16U, v.size());
	EXPECT_EQ(size_t(nObjects), v.capacity());
	EXPECT_EQ(24U, v.capacity());
	EXPECT_EQ(15, v[15].ch[0]);

	v.resize(15);
	EXPECT_EQ(15U, v.size());
	EXPECT_EQ(size_t(nObjects), v.capacity());
	EXPECT_EQ(16U, v.capacity());
	EXPECT_EQ(14, v[14].ch[0]);

	v.resize(0);
	EXPECT_EQ(0U, v.size());
	EXPECT_EQ(size_t(nObjects), v.capacity());
	EXPECT_EQ(0U, v.capacity());
}

TEST(Test_Containers, ByteBuffer_GetTerminatedString) {
	const char* test_data[] = {"abcd", "", "efg"};
	auto v = [&] {
		std::vector<char> res;
		for (auto str : test_data) {
			res.insert(res.end(), str, str + std::strlen(str) + 1);
		}
		return res;
	}();
	scid::database::ByteBuffer buf(reinterpret_cast<unsigned char*>(v.data()), v.size());
	for (auto str : test_data) {
		EXPECT_STREQ(str, buf.GetTerminatedString());
	}
}

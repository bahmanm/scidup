/** @file
 * Square collection helpers.
 */

#pragma once

#include "scidup/core/board.h"

#include <cassert>

namespace scid::database {

constexpr uint MAX_SQUARELIST = 65; // 64 squares plus null square

class SquareList {
	uint ListSize;
	squareT Squares[MAX_SQUARELIST];

public:
	SquareList() { ListSize = 0; }

	void Init() { ListSize = 0; }
	void Clear() { ListSize = 0; }
	void Add(squareT sq) {
		Squares[ListSize] = sq;
		ListSize++;
	}
	uint Size() { return ListSize; }

	squareT Get(uint index) {
		assert(index < ListSize);
		return Squares[index];
	}

	bool Contains(squareT sq) {
		for (uint i = 0; i < ListSize; i++) {
			if (Squares[i] == sq) {
				return true;
			}
		}
		return false;
	}

	void Remove(uint index) {
		assert(index < ListSize);
		ListSize--;
		if (index != ListSize) {
			Squares[index] = Squares[ListSize];
		}
	}
};

class SquareSet {
	unsigned long long bits_ = 0;

public:
	void Add(squareT sq) {
		assert(sq < 64);
		bits_ |= 1ull << sq;
	}

	bool Contains(squareT sq) {
		assert(sq < 64);
		return (bits_ & (1ull << sq)) != 0;
	}
};

} // namespace scid::database

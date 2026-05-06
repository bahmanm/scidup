/*
 * Copyright (C) 1999-2000  Shane Hudson
 * Copyright (C) 2017  Fulvio Benini

 * This file is part of Scid (Shane's Chess Information Database).
 *
 * Scid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Scid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scid.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SCIDUP_ECO_BOOK_H
#define SCIDUP_ECO_BOOK_H

#include "scidup/core/error.h"
#include "scidup/eco/code.h"
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace scid::database {
class Position;
}

namespace scidup::eco {

using Error = scid::database::errorT;
using Position = scid::database::Position;

inline constexpr Error OK = scid::database::OK;
inline constexpr Error ERROR_FileOpen = scid::database::ERROR_FileOpen;
inline constexpr Error ERROR_Corrupt = scid::database::ERROR_Corrupt;

/**
 * A Book is a collection of chess positions, each with the corresponding ECO
 * code, a mnemonic name, and the list of moves to reach the position.
 */
class Book {
public:
	struct Line {
		std::string_view code;
		std::string_view name;
		std::string_view moves;
	};

private:
	struct BookData {
		std::unique_ptr<char[]> compactStr;
		std::unique_ptr<char[]> comment;

		BookData(char* compact, char* comm)
		    : compactStr(compact), comment(comm) {}
	};

	std::unordered_multimap<unsigned, BookData> pos_;
	std::vector<const char*> comments_;
	unsigned lineCount_ = 0;
	unsigned leastMaterial_ = 32; // The smallest amount of material in any
	                             // position in the book. In the range 0..32.

public:
	/**
	 * Read a file with a list of ECO codes and creates a Book object.
	 * The file is composed of lines like this:
	 * C50a "Italian Game"  1.e4 e5 2.Nf3 Nc6 3.Bc4 *
	 * @param path: the path of the file to be read.
	 * @returns
	 * - on success, a @e std::pair containing scidup::eco::OK and the newly created object.
	 * - on failure, a @e std::pair containing an error code and an empty object.
	 */
	static std::pair<Error, Book> load(const std::filesystem::path& path);

	/**
	 * Retrieve an ECO string containing the ECO code and the mnemonic name.
	 * @param position: the position to search for.
	 * @returns an empty string_view if the position is not found
	 */
	std::string_view findEcoString(const Position& position) const;

	/**
	 * Retrieve the ECO code of a position.
	 * @param position: the position to search for.
	 * @returns the corresponding ECO code or scidup::eco::ECO_None if not found.
	 */
	Code findEco(const Position& position) const;

	std::vector<Line> linesWithPrefix(std::string_view ecoPrefix) const;

	unsigned lineCount() const { return lineCount_; }
	unsigned fewestPieces() const { return leastMaterial_; }
	size_t size() const { return pos_.size(); }
};

} // namespace scidup::eco

#endif // SCIDUP_ECO_BOOK_H

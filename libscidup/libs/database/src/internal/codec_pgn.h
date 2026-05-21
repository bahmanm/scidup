/*
 * Copyright (C) 2016-2018  Fulvio Benini

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

/** @file
 * Implements the CodecPgn class, which manages the databases encoded in PGN
 * format.
 */

#ifndef CODEC_PGN_H
#define CODEC_PGN_H

#include "codec_proxy.h"
#include "filebuf.h"
#include "scidup/core/pgn/decode.h"
#include "scidup/core/pgn/encode.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace scid::database {

class CodecPgn final : public CodecProxy<CodecPgn> {
	FilebufAppend file_;
	std::string filename_;
	std::vector<char> buf_;
	size_t nParsed_ = 0;
	size_t nRead_ = 0;
	scid::core::pgn::ParseLog parseLog_;

public:
	CodecType getType() const final { return CodecType::Pgn; }

	std::vector<std::string> getFilenames() const final {
		return std::vector<std::string>(1, filename_);
	};

	scid::core::errorT flush() final {
		scid::core::errorT errFile = (file_.pubsync() == 0) ? scid::core::OK : scid::core::ERROR_FileWrite;
		scid::core::errorT errProxy = CodecProxy<CodecPgn>::flush();
		return (errFile != scid::core::OK) ? errFile : errProxy;
	}

	/**
	 * Opens/creates a PGN database.
	 * After successfully opening/creating the file, the object is ready for
	 * parseNext() calls.
	 * @param filename: full path of the pgn file to be opened.
	 * @param fmode:    valid file access mode.
	 * @returns scid::core::OK in case of success, an @e scid::core::errorT code otherwise.
	 */
	scid::core::errorT open(const char* filename, fileModeT fmode) {
		ASSERT(filename);

		buf_.resize(128 * 1024);
		nRead_ = nParsed_ = buf_.size();
		filename_ = filename;
		if (filename_.empty())
			return scid::core::ERROR_FileOpen;

		if (auto err = file_.open(filename, fmode))
			return err;

		return file_.pubseekpos(0) == 0 ? scid::core::OK : scid::core::ERROR_FileSeek;
	}

	/**
	 * Reads the next game.
	 * @param game: the Game object where the data will be stored.
	 * @returns
	 * - scid::core::ERROR_NotFound if there are no more games to be read.
	 * - scid::core::OK otherwise.
	 */
	scid::core::errorT parseNext(scid::core::Game& game, char* scidFlagsOut,
	                 std::size_t scidFlagsOutLen) {
		const auto verge = 3 * (nRead_ / 4);
		if (nParsed_ > verge && nRead_ == buf_.size()) {
			nParsed_ -= verge;
			nRead_ -= verge;
			std::copy_n(buf_.data() + verge, nRead_, buf_.data());
			nRead_ += file_.sgetn(buf_.data() + nRead_, verge);
		}

		game.clear();
		const auto startBytes = parseLog_.n_bytes;
		const auto startLogSize = parseLog_.log.size();
		const auto parsed = scid::core::pgn::parseGame(
		    buf_.data() + nParsed_, nRead_ - nParsed_, game, parseLog_);
		const auto parsedBytes = parseLog_.n_bytes - startBytes;

		bool eof = (nRead_ - nParsed_ == parsedBytes);
		if (eof && nRead_ == buf_.size()) {
			// Reached the end of input, but the file contains more bytes.
			if (nRead_ <= 128 * 1024 * 1024) {
				// Double the buffer size and retry.
				buf_.resize(nRead_ * 2);
				nRead_ += file_.sgetn(buf_.data() + nRead_, nRead_);
				return parseNext(game, scidFlagsOut, scidFlagsOutLen);
			}
			// Abort
			nRead_ = nParsed_ = 0;
			parseLog_.log.append("PGN parsing aborted.\n");
			return scid::core::ERROR_NotFound;
		}

		nParsed_ += parsedBytes;
		if (auto scidFlags = game.findExtraTag("ScidFlags");
		    scidFlags && scidFlagsOut && scidFlagsOutLen > 0) {
			std::fill_n(scidFlagsOut, scidFlagsOutLen, 0);
			std::copy_n(scidFlags->data(),
			            std::min(scidFlagsOutLen - 1, scidFlags->size()),
			            scidFlagsOut);
			game.removeExtraTag("ScidFlags");
		}
		if (!parsed && parseLog_.log.size() == startLogSize)
			return scid::core::ERROR_NotFound;

		return scid::core::OK;
	}

	/**
	 * Returns info about the parsing progress.
	 * @returns a pair<size_t, size_t> where first element is the quantity of
	 * data parsed and second one is the total amount of data of the database.
	 */
	std::pair<size_t, size_t> parseProgress() {
		return std::make_pair(parseLog_.n_bytes / 1024, file_.size() / 1024);
	}

	/**
	 * Returns the list of errors produced by parseNext() calls.
	 */
	const char* parseErrors() { return parseLog_.log.c_str(); }

	/**
	 * Add a game into the database.
	 * The @e game is encoded in pgn format and appended at the end of @e file_.
	 * @param game: core game data to append.
	 * @returns scid::core::OK in case of success, an @e scid::core::errorT code otherwise.
	 */
	scid::core::errorT gameAdd(scid::core::Game const& game, const char*) {
		buf_.clear();
		scid::core::pgn::encode(game, buf_);
		buf_.push_back('\n');
		return file_.append(buf_.data(), buf_.size());
	}
};


} // namespace scid::database
#endif

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

#include "scidup/eco/book.h"
#include "scidup/database/common.h"
#include "scidup/database/misc.h"
#include "scidup/core/position.h"
#include <algorithm>
#include <fstream>

namespace {

std::string_view
epd_findOpcode (const char * epdStr, const char * opcode)
{
    const char * s = epdStr;
    while (*s != 0) {
        while (*s == ' '  ||  *s == '\n') { s++; }
        if (scid::database::strIsPrefix (opcode, s)) {
            const char *codeEnd = s + scid::database::strLength(opcode);
            if (*codeEnd == ' ') {
                return codeEnd + 1;
            }
        }
        while (*s != '\n'  &&  *s != 0) { s++; }
    }
    return {};
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scid::database::Position::ReadLine():
//      Parse a sequence of moves separated by whitespace and
//      move numbers, e.g. "1.e4 e5 2.Nf3" or "e4 e5 Nf3".
//
scid::database::errorT ReadLine(scid::database::Position& pos, const char* s) {
	while (true) {
		while (!isalpha(static_cast<unsigned char>(*s)) && *s != 0) {
			s++;
		}
		if (*s == '\0')
			return scid::database::OK;

		const char* begin = s;
		while (!isspace(static_cast<unsigned char>(*s)) && *s != '\0') {
			s++;
		}

		scid::database::simpleMoveT sm;
		scid::database::errorT err = pos.ParseMove(&sm, begin, s);
		if (err != scid::database::OK)
			return err;

		pos.DoSimpleMove(sm);
	}
}



} // namespace

namespace scidup::eco {

std::string_view Book::findEcoString(const scid::database::Position& position) const {
	auto [it, end] = pos_.equal_range(position.HashValue());
	if (it == end)
		return {};

	char cboard[36];
	position.PrintCompactStr(cboard);
	it = std::find_if(it, end, [&](const auto& data) {
		return std::equal(cboard, cboard + 36, data.second.compactStr.get());
	});
	if (it == end)
		return {};

	auto res = epd_findOpcode(it->second.comment.get(), "eco");
	return res.substr(0, res.find('\n'));
}

Code Book::findEco(const scid::database::Position& position) const {
	auto it = findEcoString(position);
	if (it.empty())
		return ECO_None;

	char buf[8] = {0};
	it.copy(buf, 6);
	return scid::database::eco_FromString(buf);
}

std::vector<Book::Line> Book::linesWithPrefix(const std::string_view prefix) const {
	auto res = std::vector<Line>();
	for (const char* comment : comments_) {
		const auto eco = epd_findOpcode(comment, "eco");
		if (eco.empty() || eco.substr(0, prefix.size()) != prefix)
			continue;

		const auto ecoLine = eco.substr(0, eco.find('\n'));
		const auto codeEnd = ecoLine.find(' ');
		const auto nameStart = ecoLine.find('[');
		const auto nameEnd = ecoLine.rfind(']');
		auto name = std::string_view();
		if (nameStart != std::string_view::npos &&
		    nameEnd != std::string_view::npos && nameStart < nameEnd) {
			name = ecoLine.substr(nameStart + 1, nameEnd - nameStart - 1);
		}

		const auto moves = epd_findOpcode(comment, "moves");
		res.push_back(Line{
		    ecoLine.substr(0, codeEnd),
		    name,
		    moves.substr(0, moves.find('\n')),
		});
	}
	return res;
}

std::pair<Error, Book>
Book::load(const std::filesystem::path& path) {
    std::filebuf fp;
    if (!fp.open(path, std::ios::in | std::ios::binary))
        return std::make_pair(ERROR_FileOpen, Book{});

    Book book;
    book.lineCount_ = 1;
    scid::database::Position std_start;
    std_start.StdStart();
    std::string text;
    std::string moves;
    scid::database::ecoStringT ecoStr;
    Code ecoCode;
    int ch;
    Error err = scid::database::OK;
    bool done = false;

    // Loop to read in and add all positions:

    while (!done) {
        // Find the next ECO code:
        while (true) {
            ch = fp.sbumpc();
            if (ch == EOF) { done = true; break; }
            if (ch == '\n') { book.lineCount_++; }
            if (ch >= 'A'  &&  ch <= 'E') { break; }
            if (ch == '#') {
                while (ch != '\n'  &&  ch != EOF) {
                    ch = fp.sbumpc();
                }
                if (ch == EOF) { done = true; }
                book.lineCount_++;
            }
        }
        if (done) { break; }

        // Read in the rest of the ECO code:
        ecoStr[0] = ch;
        ch = fp.sbumpc();
        if (ch < '0'  ||  ch > '9') { goto corrupt; }
        ecoStr[1] = ch;
        ch = fp.sbumpc();
        if (ch < '0'  ||  ch > '9') { goto corrupt; }
        ecoStr[2] = ch;
        ecoStr[3] = 0;

        // Now check for optional extra part of code, e.g. "A00a1":
        ch = fp.sbumpc();
        if (ch >= 'a'  &&  ch <= 'z') {
            ecoStr[3] = ch; ecoStr[4] = 0;
            ch = fp.sbumpc();
            if (ch >= '1'  &&  ch <= '4') {
                ecoStr[4] = ch; ecoStr[5] = 0;
            }
        }

        // Now put ecoCode in the text string and read the text in quotes:
        ecoCode = scid::database::eco_FromString (ecoStr);
        scid::database::eco_ToExtendedString (ecoCode, ecoStr);
        text.clear();
        text.append("eco ");
        text.append(ecoStr);
        text.append(" [");

        // Find the start of the text:
        while ((ch = fp.sbumpc()) != '"') {
            if (ch == EOF) { goto corrupt; }
        }
        while ((ch = fp.sbumpc()) != '"') {
            if (ch == EOF) { goto corrupt; }
            text.push_back((char) ch);
        }
        text.append("]\n");

        // Now read the position:
        moves.clear();
        char prev = 0;
        while ((ch = fp.sbumpc()) != '*') {
            if (ch == EOF) { goto corrupt; }
            if (ch == '\n') {
                ch = ' ';
                book.lineCount_++;
            }
            if (ch != ' '  ||  prev != ' ') {
                moves.push_back((char) ch);
            }
            prev = ch;
        }
        scid::database::Position pos (std_start);
        err = ReadLine(pos, moves.c_str());
        if (err != scid::database::OK) { goto corrupt; }
        text.append("moves ");
        text.append(scid::database::strTrimLeft(moves.c_str()));
        text.push_back('\n');

        char* cboard = new char[36];
        pos.PrintCompactStr(cboard);
        auto it = book.pos_.emplace(
            pos.HashValue(), BookData{cboard, scid::database::strDuplicate(text.c_str())});
        book.comments_.push_back(it->second.comment.get());
        book.leastMaterial_ = std::min(book.leastMaterial_, pos.TotalMaterial());
    }
    return std::pair<Error, Book>(scid::database::OK, std::move(book));

corrupt:
    return std::make_pair(ERROR_Corrupt, Book{});
}

} // namespace scidup::eco

//////////////////////////////////////////////////////////////////////
//  EOF: book.cpp
//////////////////////////////////////////////////////////////////////

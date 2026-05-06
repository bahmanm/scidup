/*
* Copyright (C) 2015 Fulvio Benini
* Copyright (c) 2001-2003  Shane Hudson (2nd part of the file)

* This file is part of Scid (Shane's Chess Information Database).
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

#include <scidup/spelling/spelling.h>
#include "scidup/database/date.h"
#include "scidup/database/misc.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace {

enum InfoType {
	SPELL_SECTIONSTART,
	SPELL_NEWNAME, SPELL_ALIAS, SPELL_PREFIX, SPELL_INFIX, SPELL_SUFFIX,
	SPELL_BIO, SPELL_ELO,
	SPELL_EMPTY, SPELL_OLDBIO, SPELL_UNKNOWN
};

struct Parser {
	char* name;
	char* extra;
	enum InfoType type;

	Parser(char* line);
};

/**
 * Parser::Parser() - Parse a "spelling" line.
 *
 * Fill data members doing the following tasks:
 * - separate the optional comment (a comment starts with '#' and
 *   extend to the end of the line) from the name data.
 * - remove leading and trailing white-spaces.
 * - identify the type of data
 */
Parser::Parser(char* line) {
	ASSERT(line != 0);

	extra = strchr(line, '#');
	if (extra != NULL) {
		// Make [line, extra) a null terminated string
		*extra++ = 0;
	}
	name = (char*) scid::database::strTrimLeft(line);
	scid::database::strTrimRight(name);

	type = SPELL_UNKNOWN;
	switch (*name) {
		case 0:
			type = SPELL_EMPTY;
			break;
		case '>':
			type = SPELL_OLDBIO;
			break;
		case '=':
			type = SPELL_ALIAS;
			// Skip over "=" and spaces:
			name++;
			while (*name == ' ') { name++; }
			break;
		case '%':
			if (scid::database::strIsPrefix("%Elo ", name)) {
				type = SPELL_ELO;
				name += 5; //Skip "%Elo "
			} else if (scid::database::strIsPrefix("%Bio ", name)) {
				type = SPELL_BIO;
				name += 5; //Skip "%Bio "
			} else if (scid::database::strIsPrefix("%Prefix ", name)) {
				type = SPELL_PREFIX;
			} else if (scid::database::strIsPrefix("%Infix ", name)) {
				type = SPELL_INFIX;
			} else if (scid::database::strIsPrefix("%Suffix ", name)) {
				type = SPELL_SUFFIX;
			}
			break;
		case '@':
			type = SPELL_SECTIONSTART;
			name++; //Skip '@'
			// Now check if there is a list of characters to exclude from
			// comparisons, e.g:   @PLAYER ", .-"
			// would indicate to exclude dots, commas, spaces and dashes.
			extra = strchr(name, '"');
			if (extra != NULL) {
				char* end = strchr(++extra, '"');
				if (end != NULL) {
					*end = 0;
				} else {
					extra = NULL;
				}
			}
			break;
		default:
			type = SPELL_NEWNAME;
			if (extra != NULL) {
				// Spelling files can provide player informations like titles/gender,
				// countries, highest elo, date of birth, date of death. For example:
				// Polgar, Judit           #gm+w HUN [2735] 1976
				scid::database::strTrimRight(extra);
			}
	}
}

} // End of anonymous namespace


namespace scidup::spelling {

size_t NameNormalizer::normalize(std::string* name) const
{
	size_t corrections = 0;
	Cont::const_iterator it;

	for (it = prefix_.begin(); it != prefix_.end(); it++) {
		const std::string& s = it->first;
		if (name->compare(0, s.length(), s) == 0) {
			corrections++;
			name->replace(0, s.length(), it->second);
			break;
		}
	}

	for (it = infix_.begin(); it != infix_.end(); it++) {
		const std::string& s = it->first;
		size_t pos = name->find(s);
		while (pos != std::string::npos) {
			corrections++;
			name->replace(pos, s.length(), it->second);
			pos = name->find(s, pos + it->second.length());
		}
	}

	for (it = suffix_.begin(); it != suffix_.end(); it++) {
		const std::string& s = it->first;
		if (name->length() < s.length()) continue;
		size_t pos = name->length() - s.length();
		if (name->compare(pos, s.length(), s) == 0) {
			corrections++;
			name->replace(pos, s.length(), it->second);
			break;
		}
	}

	return corrections;
}

scid::database::errorT NameNormalizer::addPrefix(const char* s)
{
	return add(prefix_, s);
}

scid::database::errorT NameNormalizer::addInfix(const char* s)
{
	return add(infix_, s);
}

scid::database::errorT NameNormalizer::addSuffix(const char* s)
{
	return add(suffix_, s);
}

scid::database::errorT NameNormalizer::add(Cont& v, const char* s)
{
	ASSERT(s != 0);
	std::vector<size_t> parse;
	for (size_t i=0; *(s+i) != 0; i++) {
		if (*(s+i) == '"') parse.push_back(i);
	}
	if (parse.size() != 4) return scid::database::ERROR_CorruptData;
	parse[0] += 1; //skip "
	parse[1] -= parse[0]; //n_chars
	if (parse[1] == 0) return scid::database::ERROR_CorruptData;
	parse[2] += 1; //skip "
	parse[3] -= parse[2]; //n_chars
	v.push_back(std::make_pair(
		std::string(s + parse[0], parse[1]),
		std::string(s + parse[2], parse[3])
	));
	return scid::database::OK;
}

scid::database::eloT PlayerElo::getElo(scid::database::dateT date) const
{
	scid::database::uint year = scid::database::date_GetYear(date);
	auto itBegin = std::find_if(elo_.begin(), elo_.end(),
	                            [&](const std::pair<uint16_t, scid::database::eloT>& e) {
		                            return e.first == year;
	                            });
	auto itEnd = std::find_if(itBegin, elo_.end(),
	                          [&](const std::pair<uint16_t, scid::database::eloT>& e) {
		                          return e.first != year;
	                          });

	size_t n = std::distance(itBegin, itEnd);
	if (n == 0) return 0; // No data for that year

	scid::database::uint month = scid::database::date_GetMonth(date);
	if (month == 0 || month > 12) month = 0;
	else month -= 1;

	size_t idx;
	if (year == 2009 && n == 5) {
		//2 trimonthly + 3 bimonthly
		idx = (month < 6) ? month / 3 : (month - 2)/2;

	} else if (year == 2012 && n == 9) {
		//3 bimonthly + 6 monthly
		idx = (month < 6) ? month / 2 : month - 3;

	} else if (year > 2012) {
		// monthly
		if (month >= n) return 0;
		idx = month;

	} else {
		idx = month * n / 12;
	}

	return (itBegin + idx)->second;
}

#ifdef SCIDUP_SPELLING_VALIDATE
std::string PlayerElo::isValid() const
{
	for (size_t i=1, n=elo_.size(); i < n; i++) {
		if (elo_[i].first < elo_[i -1].first) return "unsorted";
	}

	auto count = [this](scid::database::uint year) {
		return std::count_if(this->elo_.begin(), this->elo_.end(),
			[&](const std::pair<uint16_t, scid::database::eloT>& e) { return e.first == year; });
	};

	auto expected = [](scid::database::uint year) {
		if (year < 1990) return 1;
		if (year < 2001) return 2;
		if (year < 2009) return 4;
		if (year < 2010) return 5;
		if (year < 2012) return 6;
		if (year < 2013) return 9;
		return 12;
	};

	for (scid::database::uint y=1970; y<2015; y++) {
		auto n = count(y);
		if (n == 0) continue;
		if (n != expected(y))
			return std::to_string(y) + ": " + std::to_string(n) + "(" +
			       std::to_string(expected(y)) + ")";
	}

	return std::string();
}
#endif

const char* PlayerInfo::getComment() const
{
	return (comment_ != 0) ? comment_ : "";
}

SpellChecker::Idx::Idx() = default;

SpellChecker::Idx::Idx(const std::string& a, int32_t i) : alias(a), idx(i) {}

bool SpellChecker::Idx::operator<(const Idx& b) const
{
	return alias < b.alias;
}

bool SpellChecker::Idx::operator<(const std::string& b) const
{
	return alias < b;
}

std::pair<scid::database::errorT, std::unique_ptr<SpellChecker>> SpellChecker::create(
    const char* filename, const scid::database::Progress& progress)
{
	auto res = std::unique_ptr<SpellChecker>(new SpellChecker);
	scid::database::errorT err = res->read(filename, progress);
	if (err != scid::database::OK) {
		res.reset();
	}
	return std::make_pair(err, std::move(res));
}

std::vector<const char*> SpellChecker::find(const scid::database::nameT& nt, const char* name, scid::database::uint nMaxRes) const
{
	ASSERT(nt < scid::database::NUM_NAME_TYPES);
	ASSERT(name != 0);
	std::vector<const char*> res;
	std::pair<IdxIt, IdxIt> it;
	if (nt != scid::database::NAME_PLAYER) it = idxFind(nt, name);
	else it = idxFindPlayer(name);
	for (; it.first != it.second && res.size() < nMaxRes; it.first++) {
		const char* corrected = names_[nt][it.first->idx];
		if (std::find(res.begin(), res.end(), corrected) == res.end()) {
			res.push_back(corrected);
		}
	}
	return res;
}

const NameNormalizer& SpellChecker::getGeneralCorrections(const scid::database::nameT& nt) const
{
	ASSERT(nt < scid::database::NUM_NAME_TYPES);
	return general_[nt];
}

const PlayerInfo* SpellChecker::getPlayerInfo(const char* name,
                                              std::vector<const char*>* bio) const
{
	ASSERT(name != 0);
	IdxIt it = idxFindPlayerUnambiguous(name);
	if (it == idx_[scid::database::NAME_PLAYER].end()) return 0; // not found

	if (bio != 0) *bio = pInfo_[it->idx].bio_;
	return &(pInfo_[it->idx]);
}

const PlayerElo* SpellChecker::getPlayerElo(const char* name) const
{
	ASSERT(name != 0);
	if (!hasEloData()) return 0;
	IdxIt it = idxFindPlayerUnambiguous(name);
	if (it == idx_[scid::database::NAME_PLAYER].end()) return 0; // not found
	return &(pElo_[it->idx]);
}

bool SpellChecker::hasEloData() const
{
	return pElo_.size() != 0;
}

size_t SpellChecker::numCorrectNames(const scid::database::nameT& nt) const
{
	ASSERT(nt < scid::database::NUM_NAME_TYPES);
	return names_[nt].size();
}

const char* SpellChecker::storeString(const char* s)
{
	if (s == nullptr) return nullptr;
	strings_.push_back(s);
	return strings_.back().c_str();
}

std::string SpellChecker::normalizeAndTransform(const scid::database::nameT& nt, const char* s) const
{
	std::string res;
	for (const char* i = s; *i != 0; i++) {
		if (excludeChars_[nt].find(*i) != std::string::npos) continue;

		res += *i;
	}
	return res;
}

std::pair<SpellChecker::IdxIt, SpellChecker::IdxIt>
SpellChecker::idxFind(const scid::database::nameT& nt, const char* prefix) const
{
	std::pair<IdxIt, IdxIt> res;
	std::string s = normalizeAndTransform(nt, prefix);
	res.first = std::lower_bound(idx_[nt].begin(), idx_[nt].end(), s);
	for (res.second = res.first; res.second != idx_[nt].end(); res.second++) {
		if (res.second->alias.compare(0, s.length(), s) != 0) break;
		if (res.second->alias == s) return std::make_pair(res.second, res.second +1);
	}
	return res;
}

std::pair<SpellChecker::IdxIt, SpellChecker::IdxIt>
SpellChecker::idxFindPlayer(const char* prefix) const
{
	std::pair<IdxIt, IdxIt> res = idxFind(scid::database::NAME_PLAYER, prefix);
	if (res.first == res.second) {
		// For spelling of player names (not other types), Scid will also try
		// to move the text after the last space in the name to the start of
		// the name for correction purposes, when it cannot find a correction.
		// This is done to correct names where the surname is last.
		std::string s = prefix;
		size_t pos = s.rfind(' ');
		if (pos != std::string::npos) {
			std::string inv = s.substr(pos);
			inv.append(s, 0, pos);
			return idxFind(scid::database::NAME_PLAYER, inv.c_str());
		}
	}
	return res;
}

SpellChecker::IdxIt SpellChecker::idxFindPlayerUnambiguous(const char* name) const
{
	std::pair<IdxIt, IdxIt> it = idxFindPlayer(name);
	if (it.first == it.second) return idx_[scid::database::NAME_PLAYER].end();

	for (IdxIt i = it.first; i != it.second; i++) {
		if (i->idx != it.first->idx) //ambiguous
			return idx_[scid::database::NAME_PLAYER].end();
	}
	return it.first;
}

#ifndef SCIDUP_SPELLING_VALIDATE
class SpellChecker::SpellingValidate {
public:
	SpellingValidate(const char*, const SpellChecker&) {}
	void ignoredLine(const char*) {}
	void idxDuplicates(const scid::database::nameT&) {}
	void checkEloData() {}
};
#else
class SpellChecker::SpellingValidate {
	const SpellChecker& spell_;
	std::ofstream f_;

public:
	SpellingValidate(const char* spellfile, const SpellChecker& sp) : spell_(sp) {
		f_.open(spellfile + std::string(".validate"));
	}
	void ignoredLine(const char* line) {
		f_ << "Ignored line:" << '\n';
		f_ << line << '\n';
		f_ << '\n';
	}
	static bool cmpIdxAlias(const Idx& a, const Idx& b) {
		return a.alias == b.alias;
	}
	void idxDuplicates(const scid::database::nameT& nt) {
		IdxIt it = spell_.idx_[nt].begin();
		IdxIt it_end = spell_.idx_[nt].end();
		for (;;) {
			it = std::adjacent_find(it, it_end, cmpIdxAlias);
			if (it == it_end) return;

			IdxIt it_endDuplicates = std::upper_bound(it, it_end, *it);
			f_ << "Duplicate hash: " << it->alias << '\n';
			for(; it != it_endDuplicates; it++) {
				f_ << spell_.names_[nt][it->idx];
				f_ << " - Idx:" << it->idx << '\n';
			}
			f_ << '\n';
		}
	}
	void checkEloData() {
		for (size_t i=0, n = spell_.pElo_.size(); i < n; i++) {
			std::string s = spell_.pElo_[i].isValid();
			if (! s.empty()) {
				f_ << "Elo error: " << s << " --- ";
				f_ << spell_.names_[scid::database::NAME_PLAYER][i] << '\n';
			}
		}
	}
};
#endif

/**
 * class SpellingLoader - load data into a SpellChecker object
 *
 * This class take parsed "spelling" data and store it into the right
 * data members of the associated SpellChecker object.
 * Reading from a "spelling" file is not stateless and the Parser object
 * cannot contain all the necessary data: a SpellingLoader object keep track
 * of the current scid::database::nameT section and the current correct name.
 * The SpellingValidate object is used to log ignored data, usually
 * caused by typos like "@Eol" or "@Preffix".
 */
class SpellingLoader {
	SpellChecker& sp_;
	SpellChecker::SpellingValidate& validate_;
	scid::database::nameT nt_;
	int32_t nameIdx_;

public:
	SpellingLoader(SpellChecker& sp, SpellChecker::SpellingValidate& v)
	: sp_(sp), validate_(v), nt_(scid::database::NAME_INVALID), nameIdx_(-1) {
	}

	scid::database::errorT load(const Parser& data) {
		switch (data.type) {
			case SPELL_SECTIONSTART:
				nt_ = scid::database::NameBase::NameTypeFromString(data.name);
				if (!scid::database::NameBase::IsValidNameType(nt_)) return scid::database::ERROR_CorruptData;
				if (data.extra != NULL) {
					sp_.excludeChars_[nt_] = data.extra;
				} else {
					sp_.excludeChars_[nt_].clear();
				}
				nameIdx_ = -1;
				return scid::database::OK;
			case SPELL_NEWNAME:
			case SPELL_ALIAS:
			case SPELL_PREFIX:
			case SPELL_INFIX:
			case SPELL_SUFFIX:
				return nameSection(data);
			case SPELL_BIO:
			case SPELL_ELO:
				return playerInfo(data);
			case SPELL_EMPTY:
				return scid::database::OK;
			case SPELL_OLDBIO:
			case SPELL_UNKNOWN:
				validate_.ignoredLine(data.name);
				return scid::database::OK;
		}

		ASSERT(0);
		return scid::database::ERROR_CorruptData;
	}

private:
	scid::database::errorT nameSection(const Parser& data) {
		// Must be in a valid name section
		if (!scid::database::NameBase::IsValidNameType(nt_)) return scid::database::ERROR_CorruptData;

		switch (data.type) {
			case SPELL_NEWNAME:
				ASSERT(sp_.names_[nt_].size() < (1ULL << 31));
				nameIdx_ = static_cast<int32_t>(sp_.names_[nt_].size());
				sp_.names_[nt_].push_back(sp_.storeString(data.name));
				if (nt_ == scid::database::NAME_PLAYER) {
					sp_.pInfo_.push_back(sp_.storeString(data.extra));
				}
				/* FALLTHRU */
			case SPELL_ALIAS:
				if (nameIdx_ == -1) {
					return scid::database::ERROR_CorruptData;
				} else {
					sp_.idx_[nt_].push_back(SpellChecker::Idx(
						sp_.normalizeAndTransform(nt_, data.name),
						nameIdx_
						));
				}
				return scid::database::OK;
			case SPELL_PREFIX:
				return sp_.general_[nt_].addPrefix(data.name);
			case SPELL_INFIX:
				return sp_.general_[nt_].addInfix(data.name);
			case SPELL_SUFFIX:
				return sp_.general_[nt_].addSuffix(data.name);
			default:
				ASSERT(0);
		}

		return scid::database::ERROR_CorruptData;
	}

	scid::database::errorT playerInfo(const Parser& data) {
		// SPELL_BIO and SPELL_ELO are valid only for a PLAYER name
		if (nt_ != scid::database::NAME_PLAYER || nameIdx_ == -1) return scid::database::ERROR_CorruptData;

		if (data.type == SPELL_BIO) {
			sp_.pInfo_[nameIdx_].bio_.push_back(sp_.storeString(data.name));
		} else {
			ASSERT(data.type == SPELL_ELO);
			// if necessary, add empty PlayerElo objects
			sp_.pElo_.resize(nameIdx_ + 1);
			sp_.pElo_[nameIdx_].addEloData(data.name);
		}

		return scid::database::OK;
	}
};


/**
 * SpellChecker::read() - Read a "spelling" file.
 *
 * This functions tries to open the @filename file and to load the data
 * into the SpellChecker object.
 * The object must be empty. In practice the requirement is to not call
 * this function twice, because this is the only non-const member function.
 * If the function fails (result != scid::database::OK) the object state is undefined
 * and the only valid operation is to destroy the object.
 * If SCIDUP_SPELLING_VALIDATE is defined, it also creates a @filename.validate log.
 */
scid::database::errorT SpellChecker::read(const char* filename, const scid::database::Progress& progress)
{
	ASSERT(filename != NULL);
	ASSERT(strings_.empty());

	// Open the file and read it into memory; Parser mutates each line.
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file) return scid::database::ERROR_FileOpen;

	const std::streamoff fileSize = file.tellg();
	if (fileSize < 0) return scid::database::ERROR_FileOpen;
	file.seekg(0, std::ios::beg);

	std::vector<char> fileBuffer(static_cast<size_t>(fileSize));
	if (!fileBuffer.empty() &&
	    !file.read(fileBuffer.data(), static_cast<std::streamsize>(fileBuffer.size()))) {
		return scid::database::ERROR_FileRead;
	}

	SpellingValidate validate(filename, *this);

	// Parse the file lines.
	std::vector<char> lineBuffer;
	scid::database::uint report_i = 0;
	std::streamsize report_done = 0;
	SpellingLoader loader(*this, validate);
	for (size_t lineStart = 0; lineStart < fileBuffer.size();) {
		auto lineEnd = std::find(fileBuffer.begin() + lineStart,
		                         fileBuffer.end(), '\n');
		const auto nextLine = lineEnd == fileBuffer.end()
		                          ? fileBuffer.size()
		                          : static_cast<size_t>(
		                                std::distance(fileBuffer.begin(), lineEnd)) +
		                                1;

		lineBuffer.assign(fileBuffer.begin() + lineStart, lineEnd);
		lineBuffer.push_back('\0');

		report_done += static_cast<std::streamsize>(nextLine - lineStart);
		if ((++report_i % 10000) == 0) {
			if (!progress.report(static_cast<size_t>(report_done),
			                     static_cast<size_t>(fileSize)))
				return scid::database::ERROR_UserCancel;
		}

		scid::database::errorT err = loader.load(Parser(lineBuffer.data()));
		if (err != scid::database::OK) return err;

		lineStart = nextLine;
	}
	if (report_done != fileSize) return scid::database::ERROR_FileRead;

	// Success:
	if (pElo_.size() > 0) {
		// if necessary, add empty PlayerElo objects
		pElo_.resize(pInfo_.size());
		validate.checkEloData();
	}

	// Sort the index
	for (scid::database::nameT i=0; i < scid::database::NUM_NAME_TYPES; i++) {
		std::sort(idx_[i].begin(), idx_[i].end());
		validate.idxDuplicates(i);
	}
	return scid::database::OK;
}


//////////////////////////////////////////////////////////////////////
//
//  FILE:       spelling.cpp
//              SpellChecker class methods
//
//  Part of:    Scid (Shane's Chess Information Database)
//  Version:    3.5
//
//  Notice:     Copyright (c) 2001-2003  Shane Hudson.  All rights reserved.
//
//  Author:     Shane Hudson (sgh@users.sourceforge.net)
//
//////////////////////////////////////////////////////////////////////

// Retrieve the list of Rating figures for given player (aka node) from the given (ssp) string
// The string is formatted as:
// [%Elo ]<year>:<<rating>|?>,...,<<rating>|?> [<year>:<<rating>|?>,...,<<rating>|?>...]
//
// The ratings are stored in a rating array for this player, in the order of appearance
// and without any assumption on the period that the rating refers to.
// This is accomplished by assuming that for all years the same number of rating figures
// could be given (see ELO_RATINGS_PER_YEAR above).
//
// The (external) algorithm to map ratings to actual periods must be able to cope with
// the holes that - as a consequence - will appear in the rating graph constructed here!
//
void PlayerElo::addEloData(const char * str)
{
    while (1) {
        // Get the year in which the rating figures to follow were published
        //
        str = scid::database::strTrimLeft (str);
        if (! isdigit(static_cast<unsigned char>(*str))) { break; }
        uint16_t year = scid::database::strGetUnsigned (str);
        str += 4;
        if (*str != ':') { break; }
        str++;

        // Now read all the ratings for this year:
        //
        scid::database::eloT elo = 0;
        while (1) {
            if (isdigit(static_cast<unsigned char>(*str))) {
                elo = scid::database::strGetUnsigned (str);
                str += 4;
            } else if (*str == '?') {
                elo = 0;
                str++;
            } else if (*str == ' ') {
                break;
            } else {
                // Invalid data seen:
                return;
            }

            elo_.push_back(std::make_pair(year, elo));

            if (*str == ',') { str++; }
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// PlayerInfo::GetTitle:
//    Extract the first title appearing in the player
//    comment, and return it.
const char *
PlayerInfo::getTitle() const
{
    static const char * titles[] = {
        "GM", "IM", "FM",
        "WGM", "WIM", "WFM", "W",
        "CGM", "CIM", "HGM",
        NULL
    };
    const char ** titlePtr = titles;

    const char* comment = getComment();
    if (*comment == 0) { return ""; }

    while (*titlePtr != NULL) {
        if (scid::database::strIsCasePrefix (*titlePtr, comment)) { return *titlePtr; }
        titlePtr++;
    }
    return "";
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// PlayerInfo::GetLastCountry:
//    Scan the player comment string for the country field (which
//    is the second field, after the title), then return the
//    last three letters in the country field, or the empty string
//    if the country field is less than 3 characters long.
std::string
PlayerInfo::getLastCountry() const
{
    const char* start = getComment();
    if (*start == 0) { return ""; }

    // Skip over the title field:
    while (*start != ' '  &&  *start != 0) { start++; }
    while (*start == ' ') { start++; }

    const char * end = start;
    int length = 0;
    while (*end != ' '  &&  *end != 0) { end++; length++; }
    // Return the final three characters of the country field:
    if (length >= 3) {
        return std::string(start + length - 3, 3);
    }
    return "";
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// PlayerInfo::GetPeakRating:
//    Scan the player comment string for the peak rating
//    field (which is contained in brackets), convert it
//    to an unsigned integer, and return it.
scid::database::eloT
PlayerInfo::getPeakRating() const
{
    const char* s = getComment();
    if (*s == 0) { return 0; }

    while (*s != '['  &&  *s != 0) { s++; }
    if (*s != '[') { return 0; }
    s++;
    return scid::database::strGetUnsigned (s);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// PlayerInfo::GetBirthdate:
//    Scan the player comment string for the birthdate
//    field, convert it to a date, and return it.
scid::database::dateT
PlayerInfo::getBirthdate() const
{
    const char* s = getComment();
    if (*s == 0) { return scid::database::ZERO_DATE; }

    // Find the end-bracket character after the rating:
    while (*s != ']'  &&  *s != 0) { s++; }
    if (*s != ']') { return scid::database::ZERO_DATE; }
    s++;
    // Now skip over any spaces:
    while (*s == ' ') { s++; }
    if (*s == 0) { return scid::database::ZERO_DATE; }
    return scid::database::date_EncodeFromString (s);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// PlayerInfo::GetDeathdate:
//    Scan the player comment string for the deathdate
//    field, convert it to a date, and return it.
scid::database::dateT
PlayerInfo::getDeathdate() const
{
    const char* s = getComment();
    if (*s == 0) { return scid::database::ZERO_DATE; }

    // Find the end-bracket character after the rating:
    while (*s != ']'  &&  *s != 0) { s++; }
    if (*s != ']') { return scid::database::ZERO_DATE; }
    s++;
    // Now skip over any spaces:
    while (*s == ' ') { s++; }
    // Now skip over the birthdate and dashes:
    while (*s != 0  &&  *s != '-') { s++; }
    while (*s == '-') { s++; }
    if (*s == 0) { return scid::database::ZERO_DATE; }
    return scid::database::date_EncodeFromString (s);
}

//////////////////////////////////////////////////////////////////////
//  EOF: spelling.cpp
//////////////////////////////////////////////////////////////////////

} // namespace scidup::spelling

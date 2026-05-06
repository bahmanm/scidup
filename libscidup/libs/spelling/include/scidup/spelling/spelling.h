/*
# Copyright (C) 2015 Fulvio Benini

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

#ifndef SCIDUP_SPELLING_SPELLING_H
#define SCIDUP_SPELLING_SPELLING_H

#include "scidup/database/namebase.h"
#include "scidup/core/date.h"
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <utility>

/*
* A "spelling" file contains the correct names for players, events, sites and rounds.
* Optionally it can provide further informations for players like elo, birthdate, etc..
* See the header of spelling.ssp for a more detailed description of the format.
*/

namespace scidup::spelling {

/**
 * class NameNormalizer - apply general corrections to a name
 *
 * Spelling files can provide general corrections in the form:
 * %Prefix "wrong prefix" "correct prefix"
 * %Infix "wrong suffix" "correct suffix"
 * %Suffix "wrong suffix" "correct suffix"
 *
 * Example:
 * %Prefix "II " "2. "
 * %Infix "3rd " "3. "
 * %Suffix "(Italy)" "ITA"
 * "II champ 3rd II 3rd (Italy) (Italy)" --> "2. champ 3. II 3. (Italy) ITA"
 */
class NameNormalizer {
	typedef std::vector< std::pair<std::string,std::string> > Cont;
	Cont prefix_;
	Cont infix_ ;
	Cont suffix_;

public:
	/**
	 * normalize() - correct a name
	 * @name: the name to be corrected
	 *
	 * Return: count of corrections applied
	 */
	size_t normalize(std::string* name) const;

	/**
	 * add*fix() - add a general correction
	 *
	 * Adds a general prefix, infix or suffix correction.
	 * Syntax for @e s is:
	 * %Suffix "wrong suffix" "correct suffix"
	 * Return: OK if successful
	 */
	scid::database::errorT addPrefix(const char* s);
	scid::database::errorT addInfix (const char* s);
	scid::database::errorT addSuffix(const char* s);

private:
	scid::database::errorT add(Cont& v, const char* s);
};


/**
 * class PlayerElo - elo ratings of a player
 *
 * Spelling files can provide elo ratings of a player in the form:
 * %Elo YEAR:ELO_1PERIOD,ELO_2PERIOD,ELO_3PERIOD,... YEAR:ELO_1PERIOD,...
 */
class PlayerElo {
	std::vector< std::pair<uint16_t, scid::database::ratingT> > elo_;

public:
	void addEloData(const char* str);

	scid::database::ratingT getElo (scid::database::dateT date) const;

#ifdef SCIDUP_SPELLING_VALIDATE
	std::string isValid() const;
#endif
};


/**
 * class PlayerInfo - player informations
 *
 * Spelling files can provide player informations like titles/gender,
 * countries, highest elo, date of birth, date of death. For example:
 * Polgar, Judit           #gm+w HUN [2735] 1976
 *
 * Generic information can be provided in the form:
 * %Bio This is a generic information
 */
class PlayerInfo {
	const char* comment_;
	std::vector<const char*> bio_;

	friend class SpellingLoader;
	friend class SpellChecker;

public:
	PlayerInfo(const char* s) : comment_(s) {}
	const char* getTitle() const;
	std::string getLastCountry() const;
	scid::database::dateT getBirthdate() const;
	scid::database::dateT getDeathdate() const;
	scid::database::ratingT getPeakRating() const;
	const char* getComment() const;
};


/**
 * class SpellChecker - name spelling
 *
 * Read a spell file and allow to retrieve corrected names and players data.
 * if SCIDUP_SPELLING_VALIDATE is defined also check the spell file for errors.
 */
class SpellChecker {
	struct Idx {
		std::string alias;
		int32_t idx;

		Idx();
		Idx(const std::string& a, int32_t i);
		bool operator<(const Idx& b) const;
		bool operator<(const std::string& b) const;
	};
	typedef std::vector<Idx>::const_iterator IdxIt;

	NameNormalizer general_[scid::database::NUM_NAME_TYPES];
	std::string excludeChars_[scid::database::NUM_NAME_TYPES];
	std::vector<Idx> idx_[scid::database::NUM_NAME_TYPES];
	std::vector<const char*> names_[scid::database::NUM_NAME_TYPES];
	std::vector<PlayerInfo> pInfo_;
	std::vector<PlayerElo>  pElo_;
	std::deque<std::string> strings_;

	friend class SpellingLoader;

public:
	/**
	 * create() - Create a new SpellChecker object
	 *
	 * Create a new SpellChecker reading from @e filename.
	 * Return:
	 * - OK and a pointer to the new object.
	 * - on error the ERROR_*CODE* and nullptr.
	 */
	static std::pair<scid::database::errorT, std::unique_ptr<SpellChecker>> create(
	    const char* filename, const scid::database::Progress& progress);

	/**
	 * find() - search for correct names
	 * @nt:      the type of the name to be corrected
	 * @name:    the name to be corrected
	 * @nMaxRes: max size of the returned vector
	 *
	 * Return: a vector of correct names.
	 * @name will be normalized removing excludeChars_[@nt].
	 * If an exact match for normalized @name is found the result vector will
	 * contain only the corresponding correct name, otherwise will contain all
	 * the correct names that have @name as a prefix.
	 */
	std::vector<const char*> find(const scid::database::nameT& nt, const char* name, scid::database::uint nMaxRes = 10) const;

	const NameNormalizer& getGeneralCorrections(const scid::database::nameT& nt) const;

	/**
	* SpellChecker::getPlayerInfo() - get extra info about a player
	*
	* Get extra data like titles/gender, countries, highest elo,
	* date of birth, date of death or biographic informations.
	* Return:
	* - on success a pointer to a valid PlayerInfo object containing
	*   the available data. If @bio != 0 the vector is filled with
	*   the available biographic informations.
	* - if @name is not found or is ambiguous (match multiple players)
	*   returns NULL and @bio is untouched.
	*/
	const PlayerInfo* getPlayerInfo(const char* name,
	                                std::vector<const char*>* bio = 0) const;

	const PlayerElo* getPlayerElo(const char* name) const;

	bool hasEloData() const;

	size_t numCorrectNames(const scid::database::nameT& nt) const;

private:
	SpellChecker() = default;
	SpellChecker(const SpellChecker&) = delete;
	SpellChecker& operator=(const SpellChecker&) = delete;

	scid::database::errorT read(const char* filename, const scid::database::Progress& progress);

	const char* storeString(const char* s);

	std::string normalizeAndTransform(const scid::database::nameT& nt, const char* s) const;

	std::pair<IdxIt, IdxIt> idxFind(const scid::database::nameT& nt, const char* prefix) const;

	std::pair<IdxIt, IdxIt> idxFindPlayer(const char* prefix) const;

	IdxIt idxFindPlayerUnambiguous(const char* name) const;

	class SpellingValidate;

};


} // namespace scidup::spelling

#endif

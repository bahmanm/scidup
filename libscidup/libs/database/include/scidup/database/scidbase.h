/*
# Copyright (C) 2014-2019 Fulvio Benini

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

#ifndef SCIDBASE_H
#define SCIDBASE_H

#include "scidup/core/game.h"
#include "scidup/core/game_result.h"
#include "scidup/core/fullmove.h"
#include "scidup/core/board.h"
#include "scidup/database/game_id.h"
#include "scidup/database/game_info.h"
#include "scidup/database/hfilter.h"
#include "scidup/database/index.h"
#include "scidup/database/namebase.h"
#include "scidup/database/tree.h"
#include "scidup/eco/code.h"
#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace scidup::eco {
class Book;
}

namespace scid::core {
class Position;
}

namespace scid::database {

class ByteBuffer;
class GameView;
class Progress;
class SortCache;

// Pattern filter for material searches.
// It can specify, for example, a white pawn on the f-file, or a black bishop
// on f2 and white king on e1.
struct patternT {
	scid::core::pieceT pieceMatch;
	scid::core::rankT rankMatch;
	scid::core::fyleT fyleMatch;
	scid::core::byte flag; // 0 means this pattern must not occur.
};

enum gameExactMatchT : int {
	GAME_EXACT_MATCH_Exact = 0,
	GAME_EXACT_MATCH_Pawns,
	GAME_EXACT_MATCH_Fyles,
	GAME_EXACT_MATCH_Material
};

struct scidBaseT {
	struct EcoClassificationOptions {
		bool classifyExistingCodes = true;
		bool extendedCodes = false;
		std::optional<scid::core::dateT> minDate;
	};

	struct RatingUpdateStats {
		scid::core::uint changedRatings = 0;
		scid::core::uint changedGames = 0;
	};

	struct Stats {
		scid::core::uint flagCount[IndexEntry::IDX_NUM_FLAGS]; // Num of games with each
		                                           // flag set.
		scid::core::dateT minDate;
		scid::core::dateT maxDate;
		uint64_t nYears;
		uint64_t sumYears;
		scid::core::uint nResults[scid::core::NUM_RESULT_TYPES];
		scid::core::uint nRatings;
		uint64_t sumRatings;
		scid::core::uint minRating;
		scid::core::uint maxRating;

		Stats(const scidBaseT* dbase);

		struct Eco {
			scid::core::uint count;
			scid::core::uint results[scid::core::NUM_RESULT_TYPES];

			Eco();
		};
		const Eco* getEcoStats(const char* ecoStr) const;

	private:
		Eco ecoEmpty_;
		Eco ecoValid_;
		Eco ecoStats_[(1 + (1 << 16) / 131) * 27];
		Eco ecoGroup1_[(1 + (1 << 16) / 131) / 100];
		Eco ecoGroup2_[(1 + (1 << 16) / 131) / 10];
		Eco ecoGroup3_[(1 + (1 << 16) / 131)];
	};

	scidBaseT();
	~scidBaseT();

		scid::core::errorT open(std::string_view dbType, fileModeT fMode, const char* filename,
		            const Progress& progress = {});

	void Close();

	std::string getFileName() const;
	bool isOpen() const { return inUse; }
	bool isReadOnly() const { return fileMode_ == FMODE_ReadOnly; }
	gamenumT numGames() const { return idx->GetNumGames(); }

	/// Returns a vector of tag pairs containing extra information about the
	/// database (type, description, autoload, etc..)
		std::vector<std::pair<const char*, std::string>> getExtraInfo() const;

	/// Store an extra information about the database (type, description, etc..)
		scid::core::errorT setExtraInfo(const char* tagname, const char* new_value);

	const IndexEntry* getIndexEntry(gamenumT g) const {
		assert(g < numGames());
		return idx->GetEntry(g);
	}
	const IndexEntry* getIndexEntry_bounds(gamenumT g) const {
		static_assert(std::is_unsigned_v<gamenumT>);
		return g < numGames() ? getIndexEntry(g) : nullptr;
	}
	GameInfo gameInfo(gamenumT g) const;
	std::optional<GameInfo> gameInfoBounds(gamenumT g) const {
		static_assert(std::is_unsigned_v<gamenumT>);
		return g < numGames() ? std::optional<GameInfo>{gameInfo(g)}
		                      : std::nullopt;
	}
	scid::core::errorT updateGameInfo(gamenumT g, const GameInfoUpdate& update);
	TagRoster tagRoster(gamenumT gnum) const {
		return tagRoster(*getIndexEntry(gnum));
	}
	TagRoster tagRoster(IndexEntry const& ie) const {
		return TagRoster::make(ie, *nb_);
	}

	const NameBase* getNameBase() const { return nb_; }

	/// Return the highest elo of the player (in the database's games)
	scid::core::ratingT peakElo(idNumberT playerID) const {
		if (peakEloCache_.empty()) {
			for (gamenumT gnum = 0, n = numGames(); gnum < n; gnum++) {
				IndexEntry const& ie = *getIndexEntry(gnum);
				auto updateMax = [&](auto id, auto elo) {
					auto& max_value = peakEloCache_[id];
					max_value = std::max(max_value, elo);
				};
				updateMax(ie.GetWhite(), ie.GetWhiteElo());
				updateMax(ie.GetBlack(), ie.GetBlackElo());
			}
		}
		return peakEloCache_[playerID];
	}

	scid::core::errorT loadGame(const IndexEntry& ie, scid::core::Game& dest,
	                char* scidFlags, std::size_t scidFlagsLen) const;
	scid::core::errorT loadGame(gamenumT gNum, scid::core::Game& dest,
	               char* scidFlags, std::size_t scidFlagsLen) const;
	scid::core::errorT loadGameMovesOnly(gamenumT gNum,
	                                     scid::core::Game& dest) const;
	scid::core::errorT loadGameMovesOnly(const IndexEntry& ie,
	                                     scid::core::Game& dest) const;
	scid::core::errorT gameTags(
	    gamenumT gNum,
	    std::vector<std::pair<std::string, std::string>>& dest) const;
	scid::core::errorT loadStandardTags(gamenumT gNum,
	                                    scid::core::Game& dest,
	                                    char* scidFlags,
	                                    std::size_t scidFlagsLen) const;
	scid::core::errorT gameTags(
	    const IndexEntry& ie,
	    std::vector<std::pair<std::string, std::string>>& dest) const;
	std::vector<scid::core::FullMove> mainlineMoves(
	    gamenumT gNum, std::size_t maxPly) const;
	std::vector<scid::core::FullMove> mainlineMoves(
	    const IndexEntry* ie, std::size_t maxPly) const;
	std::string moveSAN(gamenumT gNum, int plyToSkip, int count) const;
	std::string moveSAN(const IndexEntry* ie, int plyToSkip, int count) const;
	std::optional<scidup::eco::Code> inferEcoCode(
	    const IndexEntry& ie, const scidup::eco::Book& book,
	    bool extendedCodes) const;
	std::pair<scid::core::errorT, size_t> classifyEcoCodes(
	    HFilter filter, const Progress& progress, const scidup::eco::Book& book,
	    EcoClassificationOptions options);
	std::pair<scid::core::errorT, size_t>
	replaceGameDates(HFilter filter, const Progress& progress,
	                 scid::core::dateT oldDate, scid::core::dateT newDate);
	std::pair<scid::core::errorT, size_t>
	replaceGameEventDates(HFilter filter, const Progress& progress,
	                      scid::core::dateT oldDate,
	                      scid::core::dateT newDate);
	std::pair<scid::core::errorT, size_t>
	setPlayerRatings(HFilter filter, const Progress& progress, idNumberT player,
	                 scid::core::ratingT rating,
	                 scid::core::ratingTypeT ratingType);
	template <typename TRatingResolver>
	std::pair<scid::core::errorT, RatingUpdateStats> updatePlayerRatings(
	    HFilter filter, const Progress& progress, bool overwrite,
	    bool saveRatings, TRatingResolver ratingFor);
	scid::core::errorT searchBoard(const IndexEntry& ie,
	                               scid::core::Game& game,
	                               scid::core::Position* pos,
	                               scid::core::Position* posFlip,
	                               bool useVariations,
	                               bool possibleMatch,
	                               bool possibleFlippedMatch,
	                               gameExactMatchT searchType,
	                               scid::core::uint& ply) const;
	scid::core::errorT searchBoard(gamenumT gNum,
	                               scid::core::Game& game,
	                               scid::core::Position* pos,
	                               scid::core::Position* posFlip,
	                               bool useVariations,
	                               bool possibleMatch,
	                               bool possibleFlippedMatch,
	                               gameExactMatchT searchType,
	                               scid::core::uint& ply) const;
	bool materialSearchMatch(const IndexEntry& ie, bool possibleMatch,
	                         bool possibleFlippedMatch,
	                         scid::core::byte* min, scid::core::byte* max,
	                         scid::core::byte* minFlipped,
	                         scid::core::byte* maxFlipped,
	                         patternT* patterns, std::size_t patternCount,
	                         patternT* flippedPatterns,
	                         std::size_t flippedPatternCount, int minPly,
	                         int maxPly, int matchLength, bool oppBishops,
	                         bool sameBishops, int minDiff,
	                         int maxDiff) const;
	bool materialSearchMatch(gamenumT gNum, bool possibleMatch,
	                         bool possibleFlippedMatch,
	                         scid::core::byte* min, scid::core::byte* max,
	                         scid::core::byte* minFlipped,
	                         scid::core::byte* maxFlipped,
	                         patternT* patterns, std::size_t patternCount,
	                         patternT* flippedPatterns,
	                         std::size_t flippedPatternCount, int minPly,
	                         int maxPly, int matchLength, bool oppBishops,
	                         bool sameBishops, int minDiff,
	                         int maxDiff) const;
	bool setPositionSearchFilter(const scid::core::Position& pos,
	                             HFilter& filter,
	                             const Progress& progress) const;

	scid::core::errorT importGames(const scidBaseT* srcBase, const HFilter& filter,
	                   const Progress& progress);
		scid::core::errorT importGames(std::string_view dbType, const char* filename,
		                   const Progress& progress, std::string& errorMsg);

	/**
	 * Add or replace a game into the database.
	 * @param game: core game data to store.
	 * @param scidFlags: database/application Scid flags for the game.
	 * @param replacedGameId: id of the game to replace.
	 *                        If >= numGames(), a new game will be added.
	 * @returns scid::core::OK if successful or an error code.
	 */
	scid::core::errorT saveGame(scid::core::Game const& game, const char* scidFlags,
	                gamenumT replacedGameId = INVALID_GAMEID);
	scid::core::errorT addGame(scid::core::Game const& game, const char* scidFlags) {
		return saveGame(game, scidFlags, INVALID_GAMEID);
	}

	bool getFlag(scid::core::uint flag, scid::core::uint gNum) const {
		return idx->GetEntry(gNum)->GetFlag(flag);
	}
	scid::core::errorT setFlag(bool value, scid::core::uint flag, scid::core::uint gNum);
	scid::core::errorT setFlags(bool value, scid::core::uint flag, const HFilter& filter);
	scid::core::errorT invertFlag(scid::core::uint flag, scid::core::uint gNum);
	scid::core::errorT invertFlags(scid::core::uint flag, const HFilter& filter);

	/**
	 * A Filter is a selection of games, usually obtained searching the
	 * database. A new Filter is created calling the function newFilter()
	 * and must be released calling the function deleteFilter().
	 */
	std::string newFilter();
	void deleteFilter(const char* filterId);
	HFilter getFilter(std::string_view filterId) const;
	HFilter defaultFilter() const { return HFilter(dbFilter); }
	gamenumT defaultFilterCount() const { return dbFilter->Count(); }
	scid::core::byte defaultFilterGet(gamenumT g) const { return dbFilter->Get(g); }
	void defaultFilterSet(gamenumT g, scid::core::byte value) { dbFilter->Set(g, value); }
	void defaultFilterFill(scid::core::byte value) { dbFilter->Fill(value); }
	uint64_t cacheInvalidationToken() const { return cacheInvalidationToken_; }

	/// A composed filter is a special construct created combining two filters
	/// and includes only the games contained in both filters. It should NOT be
	/// deleted and became invalid if any of its components is deleted.
	/// @mainFilter: valid identifier of the main filter (the filter which is
	///              modified by non-const operations).
	/// @maskFilter: valid identifier of the mask filter (const).
	/// @return the id of the composed filter.
	std::string composeFilter(std::string_view mainFilter,
	                          std::string_view maskFilter) const;

	/// Get the components of a composed filter.
	/// @filterId: valid identifier of a filter.
	/// @return the components (second is empty if its not a a composed filter).
	std::pair<std::string, std::string>
	getFilterComponents(std::string_view filterId) const;

	const Stats& getStats() const;
	std::vector<TreeNode> getTreeStat(const HFilter& filter) const;
	scid::core::uint getNameFreq(nameT nt, idNumberT id) {
		if (nameFreq_[nt].size() == 0)
			nameFreq_ = getNameBase()->calcNameFreq(*idx);
		return nameFreq_[nt][id];
	}

	scid::core::errorT getCompactStat(unsigned long long* n_deleted,
	                      unsigned long long* n_unused,
	                      unsigned long long* n_sparse,
	                      unsigned long long* n_badNameId);
	scid::core::errorT compact(const Progress& progress);

	/**
	 * Increment the reference count of a SortCache object matching @e criteria.
	 * @param criteria: the list of fields by which games will be ordered.
	 *                  Each field should be followed by '+' to indicate an
	 *                  ascending order or by '-' for a descending order.
	 * @returns true on success
	 */
	bool createSortCache(const char* criteria);

	/**
	 * Decrement the reference count of the SortCache object matching @e
	 * criteria. Cached objects with refCount <= 0 are destroyed independently
	 * from the value of @e criteria.
	 * @param criteria: the list of fields by which games will be ordered.
	 *                  Each field should be followed by '+' to indicate an
	 *                  ascending order or by '-' for a descending order.
	 */
	void releaseSortCache(const char* criteria);

	/**
	 * Retrieve a list of ordered game indexes sorted by @e criteria.
	 * This function will be much faster if a SortCache object matching @e
	 * criteria already exists (previously created with @e createSortCache).
	 * @param criteria: the list of fields by which games will be ordered.
	 *                  Each field should be followed by '+' to indicate an
	 *                  ascending order or by '-' for a descending order.
	 * @param start:    the offset of the first row to return.
	 *                  The offset of the initial row is 0.
	 * @param count:    maximum number of rows to return.
	 * @param filter:   a reference to a valid (!= NULL) HFilter object.
	 *                  Games not included into the filter will be ignored.
	 * @param[out] destCont: valid pointer to an array where the sorted list of
	 *                       games will be stored (should be able to contain at
	 *                       least @e count elements).
	 * @returns the number of games' ids stored into @e destCont.
	 */
	size_t listGames(const char* criteria, size_t start, size_t count,
	                 const HFilter& filter, gamenumT* destCont);

	/**
	 * Get the sorted position of a game.
	 * This function will be much faster if a SortCache object matching @e
	 * criteria already exists (previously created with @e createSortCache).
	 * @param criteria: the list of fields by which games will be ordered.
	 *                  Each field should be followed by '+' to indicate an
	 *                  ascending order or by '-' for a descending order.
	 * @param filter:   a reference to a valid (!= NULL) HFilter object.
	 *                  Games not included into the filter will be ignored.
	 * @param gameId:   the id of the game.
	 * @returns the sorted position of @e gameId.
	 */
	size_t sortedPosition(const char* criteria, const HFilter& filter,
	                      gamenumT gameId);

	/**
	 * Transform the names of the games included in @e hfilter.
	 * The function @e getID maps all the old idNumberT to the new idNumberT.
	 * It's invoked for each game and must accept as parameters a idNumberT and
	 * a const GameInfo&; must return the (eventually different) idNumberT.
	 * @param nt:       type of the names to be modified.
	 * @param hfilter:  HFilter containing the games to be transformed.
	 * @param progress: a Progress object used for GUI communications.
	 * @param newNames: optional vector of names to be added to the database.
	 * @param fnInit:   function that is invoked before beginning the
	 *                  transformation; must accept a vector that contains the
	 *                  idNumberTs of the names in @e newNames.
	 * @param getID:    function that maps the old idNumberTs to the new ones.
	 * @returns a std::pair containing scid::core::OK (or an error code) and the number of
	 * games modified.
	 */
	template <typename TInitFunc, typename TMapFunc>
	std::pair<scid::core::errorT, size_t>
	transformNames(nameT nt, HFilter hfilter, const Progress& progress,
	               const std::vector<std::string>& newNames, TInitFunc fnInit,
	               TMapFunc getID);

	/**
	 * Strip the games included in @e hfilter.
	 * @param hfilter:  HFilter containing the games to be transformed.
	 * @param progress: a Progress object used for GUI communications.
	 * @param entry_op: operator that will be applied to games.
	 * @returns a std::pair containing scid::core::OK (or an error code) and the number of
	 * games modified.
	 */
	std::pair<scid::core::errorT, size_t>
	stripGames(HFilter hfilter, const Progress& progress,
	           std::vector<std::string_view> const& removeTags);

	std::unique_ptr<gamenumT[]> extractDuplicates() {
		return std::move(duplicates_);
	}
	void setDuplicates(std::unique_ptr<gamenumT[]> duplicates) {
		duplicates_ = std::move(duplicates);
	}
	gamenumT getDuplicates(gamenumT gNum) const {
		return duplicates_ ? duplicates_[gNum] : 0;
	}

private:
	struct Storage;

	bool inUse; // true if the database is open (in use).
	Filter* dbFilter;
	std::unique_ptr<Storage> storage_;
	Index* idx;
	NameBase* nb_;
	fileModeT fileMode_; // Read-only, write-only, or both.
	std::vector<std::pair<std::string, Filter*>> filters_;
	mutable Filter all_filter_{0};
	mutable Stats* stats_;
	std::array<std::vector<int>, NUM_NAME_TYPES> nameFreq_;
	// For each game: idx of duplicate game + 1 (0 if there is no duplicate).
	std::unique_ptr<gamenumT[]> duplicates_;
	std::vector<std::pair<std::string, SortCache*>> sortCaches_;
	mutable std::unordered_map<idNumberT, scid::core::ratingT> peakEloCache_;
	scid::core::errorT err_open_ = scid::core::OK;
	uint64_t cacheInvalidationToken_ = 0;

private:
	friend class SearchPos;

	static GameInfo makeGameInfo_(const IndexEntry& ie);
	ByteBuffer gameData(const IndexEntry& ie) const;
	GameView gameView(const IndexEntry* ie) const;
		scid::core::errorT openHelper(std::string_view dbType, fileModeT mode,
		                  const char* filename, const Progress& progress = {});

	void clear();

	/// This function must be called before modifying the games of the database.
	/// Currently this function do not guarantees that the database is not
	/// altered in case of errors.
	scid::core::errorT beginTransaction();

	/// Update caches and flush the database's files.
	/// This function must be called after changing one or more games.
	/// @param gameId: id of the modified game
	///                INVALID_GAMEID to update all games.
	/// @returns scid::core::OK if successful or an error code.
	scid::core::errorT endTransaction(gamenumT gameId = INVALID_GAMEID);

		scid::core::errorT importGameHelper(const scidBaseT* sourceBase, scid::core::uint gNum);
		scid::core::errorT saveGameData(IndexEntry const& ie, TagRoster const& tags,
		                     ByteBuffer const& data, gamenumT replaced);
		scid::core::errorT saveIndexEntry(IndexEntry const& ie, gamenumT replaced);
		std::pair<scid::core::errorT, idNumberT> addName(nameT nt, const char* name);

		SortCache* getSortCache(const char* criteria);

	template <typename TOper>
	std::pair<scid::core::errorT, size_t>
	transformIndex(HFilter hfilter, const Progress& progress, TOper entry_op) {
		if (auto errModify = beginTransaction())
			return {errModify, 0};

		auto res = transformIndex_(hfilter, progress, entry_op);
		auto err = endTransaction();
		res.first = (res.first == scid::core::OK) ? err : res.first;
		return res;
	}

	/**
	 * Apply a transform operator to games' IndexEntry included in @e hfilter.
	 * The @entry_op should accept a IndexEntry& parameter and return true when
	 * the IndexEntry was modified.
	 * @param hfilter:  HFilter containing the games to be transformed.
	 * @param progress: a Progress object used for GUI communications.
	 * @param entry_op: operator that will be applied to games' IndexEntry.
	 * @returns a std::pair containing scid::core::OK (or an error code) and the number of
	 * games modified.
	 */
	template <typename TOper>
	std::pair<scid::core::errorT, size_t>
	transformIndex_(HFilter hfilter, const Progress& progress, TOper entry_op) {
		size_t nCorrections = 0;
		size_t iProg = 0;
		size_t totProg = hfilter->size();
		for (auto& gnum : hfilter) {
			if ((++iProg % 8192 == 0) && !progress.report(iProg, totProg))
				return std::make_pair(scid::core::ERROR_UserCancel, nCorrections);

			IndexEntry newIE = *getIndexEntry(gnum);
			if (!entry_op(newIE))
				continue;

				auto err = saveIndexEntry(newIE, gnum);
			if (err != scid::core::OK)
				return std::make_pair(err, nCorrections);

			++nCorrections;
		}
		return std::make_pair(scid::core::OK, nCorrections);
	}
};

template <typename TInitFunc, typename TMapFunc>
std::pair<scid::core::errorT, size_t>
scidBaseT::transformNames(nameT nt, HFilter hfilter, const Progress& progress,
                          const std::vector<std::string>& newNames,
                          TInitFunc initFunc, TMapFunc getNewID) {
	if (auto errModify = beginTransaction())
		return {errModify, 0};

	std::vector<idNumberT> nameIDs(newNames.size());
	auto it = nameIDs.begin();
	for (auto& name : newNames) {
			auto id = addName(nt, name.c_str());
		if (id.first != scid::core::OK) {
			endTransaction();
			return std::make_pair(id.first, size_t(0));
		}
		*it++ = id.second;
	}

	initFunc(nameIDs);

	auto res = transformIndex_(hfilter, progress, [&](IndexEntry& ie) {
		const IndexEntry& ie_const = ie;
		idNumberT oldID;
		idNumberT oldBlackID = 0;
		idNumberT newBlackID = 0;
		switch (nt) {
		case NAME_PLAYER:
			oldID = ie_const.GetWhite();
			oldBlackID = ie_const.GetBlack();
			newBlackID = getNewID(oldBlackID, makeGameInfo_(ie_const));
			break;
		case NAME_EVENT:
			oldID = ie_const.GetEvent();
			break;
		case NAME_SITE:
			oldID = ie_const.GetSite();
			break;
		default:
			ASSERT(nt == NAME_ROUND);
			oldID = ie_const.GetRound();
		}
		const auto newID = getNewID(oldID, makeGameInfo_(ie_const));
		if (oldID == newID && oldBlackID == newBlackID)
			return false;

		switch (nt) {
		case NAME_PLAYER:
			ie.SetWhite(newID);
			ie.SetBlack(newBlackID);
			break;
		case NAME_EVENT:
			ie.SetEvent(newID);
			break;
		case NAME_SITE:
			ie.SetSite(newID);
			break;
		default:
			ASSERT(nt == NAME_ROUND);
			ie.SetRound(newID);
		}
		return true;
	});

	auto err = endTransaction();
	res.first = (res.first == scid::core::OK) ? err : res.first;
	return res;
}

template <typename TRatingResolver>
std::pair<scid::core::errorT, scidBaseT::RatingUpdateStats>
scidBaseT::updatePlayerRatings(HFilter filter, const Progress& progress,
                               bool overwrite, bool saveRatings,
                               TRatingResolver ratingFor) {
	RatingUpdateStats stats;
	auto entry_op = [&](IndexEntry& ie) {
		const auto date = ie.GetDate();
		const auto whiteElo = (!overwrite && ie.GetWhiteElo() != 0)
		                          ? 0
		                          : ratingFor(ie.GetWhite(), date);
		const auto blackElo = (!overwrite && ie.GetBlackElo() != 0)
		                          ? 0
		                          : ratingFor(ie.GetBlack(), date);
		const auto changes = (whiteElo != 0 ? 1 : 0) + (blackElo != 0 ? 1 : 0);
		if (changes == 0)
			return false;

		stats.changedRatings += changes;
		stats.changedGames++;
		if (!saveRatings)
			return false;

		if (whiteElo != 0) {
			ie.SetWhiteElo(whiteElo);
			ie.SetWhiteRatingType(scid::core::RATING_Elo);
		}
		if (blackElo != 0) {
			ie.SetBlackElo(blackElo);
			ie.SetBlackRatingType(scid::core::RATING_Elo);
		}
		return true;
	};

	if (!saveRatings) {
		auto res = transformIndex_(filter, progress, entry_op);
		return {res.first, stats};
	}

	auto res = transformIndex(filter, progress, entry_op);
	return {res.first, stats};
}

} // namespace scid::database
#endif

/*
* Copyright (C) 2014-2016  Fulvio Benini

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
*/

#include "scidup/database/scidbase.h"
#include "bytebuf.h"
#include "codec_memory.h"
#include "codec_pgn.h"
#include "codec_scid4.h"
#include "codec_scid5.h"
#include "game_search.h"
#include "game_storage.h"
#include "gameview.h"
#include "scidup/database/common.h"
#include "scidup/database/game_id.h"
#include "scidup/eco/book.h"
#include "sortcache.h"
#include "stored.h"
#include <algorithm>
#include <filesystem>

namespace scid::database {

struct scidBaseT::Storage {
	std::unique_ptr<ICodecDatabase> codec;
};

namespace {

std::pair<CodecType, scid::core::errorT> parseCodec(std::string_view dbType) {
	if (dbType == "PGN") {
		return {CodecType::Pgn, scid::core::OK};
	}
	if (dbType == "MEMORY") {
		return {CodecType::Memory, scid::core::OK};
	}
	if (dbType == "SCID4") {
		return {CodecType::Scid4, scid::core::OK};
	}
	if (dbType == "SCID5") {
		return {CodecType::Scid5, scid::core::OK};
	}
	return {CodecType::Scid5, scid::core::ERROR_BadArg};
}

std::string_view codecName(CodecType codec) {
	switch (codec) {
	case CodecType::Memory:
		return "MEMORY";
	case CodecType::Pgn:
		return "PGN";
	case CodecType::Scid4:
		return "SCID4";
	case CodecType::Scid5:
		return "SCID5";
	}
	ASSERT(false);
	return {};
}

} // namespace

std::pair<ICodecDatabase*, scid::core::errorT>
openCodec(CodecType codec, fileModeT fMode, const char* filename,
          const Progress& progress, Index* idx, NameBase* nb) {
	auto createCodec = [](auto codec) -> ICodecDatabase* {
		switch (codec) {
		case CodecType::Memory:
			return new CodecMemory();
		case CodecType::Scid4:
			return new CodecSCID4();
		case CodecType::Pgn:
			return new CodecPgn();
		case CodecType::Scid5:
			return new CodecSCID5();
		}
		ASSERT(0);
		return nullptr;
	};

	auto obj = createCodec(codec);
	auto err = obj->dyn_open(fMode, filename, progress, idx, nb);
	if (err != scid::core::OK && err != scid::core::ERROR_NameDataLoss) {
		delete obj;
		obj = nullptr;
	}
	return {obj, err};
}

scidBaseT::scidBaseT() {
	storage_ = std::make_unique<Storage>();
	idx = new Index;
	nb_ = new NameBase;
	inUse = false;
	fileMode_ = FMODE_None;
	dbFilter = new Filter(0);
	stats_ = NULL;
}

scidBaseT::~scidBaseT() {
	if (inUse)
		Close();

	delete idx;
	delete nb_;
	delete stats_;
	delete dbFilter;
}

scid::core::errorT scidBaseT::open(std::string_view dbType, fileModeT fMode,
                       const char* filename, const Progress& progress) {
	return openHelper(dbType, fMode, filename, progress);
}

scid::core::errorT scidBaseT::openHelper(std::string_view dbType, fileModeT fMode,
                             const char* filename, const Progress& progress) {
	assert(filename);

	if (inUse)
		return scid::core::ERROR_FileInUse;

	auto [dbtype, parseErr] = parseCodec(dbType);
	if (parseErr != scid::core::OK)
		return parseErr;

	auto [db, err] = openCodec(dbtype, fMode, filename, progress, idx, nb_);
	if (db) {
		storage_->codec.reset(db);
		inUse = true;
		fileMode_ = (fMode == FMODE_Create) ? FMODE_Both : fMode;
		err_open_ = err;

		// Initialize the filters: all the games are included by default.
		all_filter_.Init(numGames());
		dbFilter->Init(numGames());
		ASSERT(filters_.empty());
	} else {
		idx->Close();
		nb_->Clear();
	}

	return err;
}

std::vector<std::pair<const char*, std::string>> scidBaseT::getExtraInfo() const {
	return storage_->codec->getExtraInfo();
}

scid::core::errorT scidBaseT::setExtraInfo(const char* tagname, const char* new_value) {
	if (isReadOnly())
		return scid::core::ERROR_FileReadOnly;

	const auto res = storage_->codec->setExtraInfo(tagname, new_value);
	return (res != scid::core::OK) ? res : storage_->codec->flush();
}

void scidBaseT::Close() {
	ASSERT(inUse);

	for (auto& sortCache : sortCaches_) {
		delete sortCache.second;
	}
	sortCaches_.clear();

	idx->Close();
	nb_->Clear();
	storage_->codec = nullptr;

	clear();
	fileMode_ = FMODE_None;
	all_filter_.Init(0);
	dbFilter->Init(0);
	for (size_t i = 0, n = filters_.size(); i < n; i++)
		delete filters_[i].second;
	filters_.clear();
	inUse = false;
}

void scidBaseT::clear() {
	if (stats_ != NULL) {
		delete stats_;
		stats_ = NULL;
	}
	duplicates_.reset();
	for (nameT nt = NAME_PLAYER; nt < NUM_NAME_TYPES; nt++) {
		nameFreq_[nt].resize(0);
	}
	peakEloCache_.clear();
	++cacheInvalidationToken_;
}

std::string scidBaseT::getFileName() const {
	if (!inUse) {
		return "<empty>";
	}
	const auto filenames = storage_->codec->getFilenames();
	return filenames.empty() ? "<clipbase>" : filenames[0];
}

scid::core::errorT scidBaseT::beginTransaction() {
	if (err_open_) // Allow modifications only if open returned scid::core::OK
		return err_open_;

	if (isReadOnly())
		return scid::core::ERROR_FileReadOnly;

	for (auto& sortCache : sortCaches_) {
		sortCache.second->prepareForChanges();
	}
	return scid::core::OK;
}

scid::core::errorT scidBaseT::endTransaction(gamenumT gNum) {
	clear();
	scid::core::errorT res = storage_->codec->flush();

	auto n_games = numGames();
	if (dbFilter->Size() != n_games) {
		all_filter_.Resize(n_games);
		dbFilter->Resize(n_games);
		for (auto& filter : filters_) {
			filter.second->Resize(n_games);
		}
	}

	for (auto& sortCache : sortCaches_) {
		sortCache.second->checkForChanges(gNum);
	}

	return res;
}

scid::core::errorT scidBaseT::loadGame(gamenumT gNum, scid::core::Game& dest,
                           char* scidFlags,
                           std::size_t scidFlagsLen) const {
	const auto* ie = getIndexEntry_bounds(gNum);
	if (!ie)
		return scid::core::ERROR_BadArg;
	return loadGame(*ie, dest, scidFlags, scidFlagsLen);
}

GameView scidBaseT::gameView(const IndexEntry* ie) const {
	auto data = storage_->codec->getGameMoves(*ie);
	if (data) {
		auto [errPos, fen] = data.decodeStartBoard();
		if (errPos == scid::core::OK) {
			if (fen) {
				scid::core::Position startPos;
				if (startPos.ReadFromFEN(fen) == scid::core::OK) {
					return GameView(data, startPos);
				}
			} else {
				return GameView(data);
			}
		}
	}
	return GameView({nullptr, 0});
}

ByteBuffer scidBaseT::gameData(const IndexEntry& ie) const {
	return storage_->codec->getGameData(ie.GetOffset(), ie.GetLength());
}

scid::core::errorT scidBaseT::loadGame(const IndexEntry& ie, scid::core::Game& dest,
                           char* scidFlags,
                           std::size_t scidFlagsLen) const {
	auto err = game_storage::decode(dest, scidFlags, scidFlagsLen, ie,
	                                tagRoster(ie),
	                                gameData(ie));
	return err;
}

scid::core::errorT scidBaseT::loadGameMovesOnly(const IndexEntry& ie,
                                                scid::core::Game& dest) const {
	auto data = gameData(ie);
	if (!data)
		return scid::core::ERROR_FileRead;
	return game_storage::decodeMovesOnly(dest, data);
}

scid::core::errorT scidBaseT::gameTags(
    const IndexEntry& ie,
    std::vector<std::pair<std::string, std::string>>& dest) const {
	auto data = gameData(ie);
	return data.decodeTags([&](auto const& tag, auto const& value) {
		dest.emplace_back(tag, value);
	});
}

std::vector<scid::core::FullMove>
scidBaseT::mainlineMoves(const IndexEntry* ie, std::size_t maxPly) const {
	std::vector<scid::core::FullMove> moves;
	moves.reserve(std::min<std::size_t>(maxPly, ie->GetNumHalfMoves()));
	gameView(ie).mainLine([&](auto move) {
		if (moves.size() >= maxPly)
			return false;
		moves.push_back(move);
		return true;
	});
	return moves;
}

std::string scidBaseT::moveSAN(const IndexEntry* ie, int plyToSkip,
                               int count) const {
	return gameView(ie).getMoveSAN(plyToSkip, count);
}

std::optional<scidup::eco::Code>
scidBaseT::inferEcoCode(const IndexEntry& ie, const scidup::eco::Book& book,
                        bool extendedCodes) const {
	auto data = gameData(ie);
	scid::core::Position currentPosition;
	if (data.decodeTags([](auto, auto) {}) != scid::core::OK)
		return std::nullopt;

	const auto [errStartPos, fen] = data.decodeStartBoard();
	if (errStartPos)
		return std::nullopt;
	if (fen) {
		if (currentPosition.ReadFromFEN(fen) != scid::core::OK)
			return std::nullopt;
	} else {
		currentPosition.StdStart();
	}

	scidup::eco::Code ecoCode = scidup::eco::ECO_None;
	for (;;) {
		if (currentPosition.TotalMaterial() < book.fewestPieces())
			break;

		const auto eco = book.findEco(currentPosition);
		if (eco != scidup::eco::ECO_None) {
			ecoCode = eco;
		}

		scid::core::simpleMoveT sm;
		if (game_storage::decodeMainlineMove(data, currentPosition, sm) !=
		    scid::core::OK)
			break;

		currentPosition.DoSimpleMove(sm);
	}

	if (!extendedCodes) {
		ecoCode = scidup::eco::basicCode(ecoCode);
	}
	return ecoCode;
}

scid::core::errorT scidBaseT::searchBoard(
    const IndexEntry& ie, scid::core::Game& game, scid::core::Position* pos,
    scid::core::Position* posFlip, bool useVariations, bool possibleMatch,
    bool possibleFlippedMatch, gameExactMatchT searchType,
    scid::core::uint& ply) const {
	auto data = gameData(ie);
	if (!data)
		return scid::core::ERROR_FileRead;

	ply = 0;
	constexpr scid::core::uint matchedPly = 1;
	if (useVariations) {
		game.clear();
		game_storage::decodeMovesOnly(game, data);
		if (ply == 0 && possibleMatch &&
		    game_search::exactMatch(game, pos, nullptr, searchType)) {
			ply = matchedPly;
		}
		if (ply == 0 && possibleFlippedMatch &&
		    game_search::exactMatch(game, posFlip, nullptr, searchType)) {
			ply = matchedPly;
		}
		if (ply == 0 && possibleMatch &&
		    game_search::varExactMatch(game, pos, searchType)) {
			ply = matchedPly;
		}
		if (ply == 0 && possibleFlippedMatch &&
		    game_search::varExactMatch(game, posFlip, searchType)) {
			ply = matchedPly;
		}
		return scid::core::OK;
	}

	if (possibleMatch) {
		auto dataClone = data;
		if (game_search::exactMatch(game, pos, &dataClone, searchType)) {
			ply = matchedPly;
		}
	}
	if (ply == 0 && possibleFlippedMatch &&
	    game_search::exactMatch(game, posFlip, &data, searchType)) {
		ply = matchedPly;
	}
	return scid::core::OK;
}

bool scidBaseT::materialSearchMatch(
    const IndexEntry& ie, bool possibleMatch, bool possibleFlippedMatch,
    scid::core::byte* min, scid::core::byte* max,
    scid::core::byte* minFlipped, scid::core::byte* maxFlipped,
    patternT* patterns, std::size_t patternCount, patternT* flippedPatterns,
    std::size_t flippedPatternCount, int minPly, int maxPly, int matchLength,
    bool oppBishops, bool sameBishops, int minDiff, int maxDiff) const {
	auto data = gameData(ie);
	if (!data)
		return false;

	const bool hasPromotion =
	    ie.GetPromotionsFlag() || ie.GetUnderPromoFlag();
	bool result = false;
	if (possibleMatch) {
		auto dataClone = data;
		result = game_search::materialMatch(
		    hasPromotion, dataClone, min, max, patterns, patternCount,
		    minPly, maxPly, matchLength, oppBishops, sameBishops, minDiff,
		    maxDiff);
	}
	if (!result && possibleFlippedMatch) {
		result = game_search::materialMatch(
		    hasPromotion, data, minFlipped, maxFlipped, flippedPatterns,
		    flippedPatternCount, minPly, maxPly, matchLength, oppBishops,
		    sameBishops, minDiff, maxDiff);
	}
	return result;
}

scid::core::errorT scidBaseT::saveGame(scid::core::Game const& game,
                           const char* scidFlags,
                           gamenumT replacedGameId) {
	if (auto errModify = beginTransaction())
		return errModify;

	std::vector<scid::core::byte> buf;
	auto [ie, tags] = game_storage::encode(game, scidFlags, buf);
	auto gamedata = ByteBuffer(buf.data(), buf.size());

	scid::core::errorT err = (replacedGameId < numGames())
	                 ? storage_->codec->saveGame(ie, tags, gamedata, replacedGameId)
	                 : storage_->codec->addGame(ie, tags, gamedata);
	scid::core::errorT errClear = endTransaction(replacedGameId);
	return (err != scid::core::OK) ? err : errClear;
}

std::pair<scid::core::errorT, size_t>
scidBaseT::stripGames(HFilter hfilter, const Progress& progress,
                      std::vector<std::string_view> const& removeTags) {
	if (auto errModify = beginTransaction())
		return {errModify, 0};

	std::vector<std::pair<std::string_view, std::string_view>> tagsBuf;
	std::vector<scid::core::byte> encodeBuf;
	size_t nCorrections = 0;
	size_t iProg = 0;
	const size_t totProg = hfilter->size();
	scid::core::errorT err = scid::core::OK;
	for (const auto gnum : hfilter) {
		if ((++iProg % 1024 == 0) && !progress.report(iProg, totProg)) {
			err = scid::core::ERROR_UserCancel;
			break;
		}

		bool changed = false;
		tagsBuf.clear();
		IndexEntry const& ie = *getIndexEntry(gnum);
		auto gamedata = gameData(ie);
		auto err = gamedata.decodeTags(
		    [&](auto const& tag, auto const& value) {
			    if (std::find(removeTags.begin(), removeTags.end(), tag) !=
			        removeTags.end())
				    changed = true;
			    else
				    tagsBuf.emplace_back(tag, value);
		    });
		if (err != scid::core::OK)
			break;

		if (!changed)
			continue;

		encodeBuf.clear();
		encodeTags(tagsBuf, encodeBuf);
		encodeBuf.insert(encodeBuf.end(), gamedata.data(),
		                 gamedata.data() + gamedata.size());
		err = saveGameData(ie, tagRoster(ie),
		                   {encodeBuf.data(), encodeBuf.size()}, gnum);
		if (err != scid::core::OK)
			break;

		++nCorrections;
	}
	const auto err_trans = endTransaction();
	if (err == scid::core::OK)
		err = err_trans;
	return {err, nCorrections};
}

scid::core::errorT scidBaseT::importGames(const scidBaseT* srcBase, const HFilter& filter,
                              const Progress& progress) {
	ASSERT(srcBase != 0);
	ASSERT(filter != 0);
	if (srcBase == this)
		return scid::core::ERROR_BadArg;

	if (auto errModify = beginTransaction())
		return errModify;

	scid::core::errorT err = scid::core::OK;
	size_t iProgress = 0;
	size_t totGames = filter->size();
	for (const auto gNum : filter) {
		err = importGameHelper(srcBase, gNum);
		if (err != scid::core::OK)
			break;

		if (++iProgress % 8192 == 0) {
			if (!progress.report(iProgress, totGames))
				break;
		}
	}
	scid::core::errorT errClear = endTransaction();
	return (err == scid::core::OK) ? errClear : err;
}

scid::core::errorT scidBaseT::importGameHelper(const scidBaseT* srcBase, gamenumT gNum) {
	const auto ie = srcBase->getIndexEntry(gNum);
	if (const auto data = srcBase->storage_->codec->getGameData(
	        ie->GetOffset(), ie->GetLength()))
		return storage_->codec->addGame(*ie, srcBase->tagRoster(*ie), data);

	return scid::core::ERROR_FileRead;
}

scid::core::errorT scidBaseT::saveGameData(IndexEntry const& ie, TagRoster const& tags,
                               ByteBuffer const& data, gamenumT replaced) {
	return storage_->codec->saveGame(ie, tags, data, replaced);
}

scid::core::errorT scidBaseT::saveIndexEntry(IndexEntry const& ie, gamenumT replaced) {
	return storage_->codec->saveIndexEntry(ie, replaced);
}

std::pair<scid::core::errorT, idNumberT> scidBaseT::addName(nameT nt, const char* name) {
	return storage_->codec->addName(nt, name);
}

scid::core::errorT scidBaseT::importGames(std::string_view dbType,
                              const char* filename, const Progress& progress,
                              std::string& errorMsg) {
	auto [dbtype, parseErr] = parseCodec(dbType);
	if (parseErr != scid::core::OK)
		return parseErr;
	if (dbtype != CodecType::Pgn)
		return scid::core::ERROR_BadArg;

	if (auto errModify = beginTransaction())
		return errModify;

	CodecPgn pgn;
	auto res = pgn.open(filename, FMODE_ReadOnly);
	if (res == scid::core::OK) {
		uint64_t nChess960Errors = 0;
		std::vector<scid::core::byte> buf;
		res = CodecPgn::parseGames(progress, pgn, [&](scid::core::Game& game,
		                                              const char* scidFlags) {
			buf.clear();
			auto [ie, tags] = game_storage::encode(game, scidFlags, buf);
			auto err = storage_->codec->addGame(ie, tags, {buf.data(), buf.size()});
			if (err == scid::core::ERROR_CodecChess960) {
				++nChess960Errors;
				err = scid::core::OK;
			}
			return err;
		});
		errorMsg = pgn.parseErrors();
		if (nChess960Errors) {
			errorMsg.append("Ignored ");
			errorMsg.append(std::to_string(nChess960Errors));
			errorMsg.append(" chess960 game(s).\n");
		}
	}

	auto res_endTrans = endTransaction();
	return (res != scid::core::OK) ? res : res_endTrans;
}

scid::core::errorT scidBaseT::invertFlag(scid::core::uint flag, scid::core::uint gNum) {
	return setFlag(!getFlag(flag, gNum), flag, gNum);
}

scid::core::errorT scidBaseT::invertFlags(scid::core::uint flag, const HFilter& filter) {
	return transformIndex(filter, Progress(),
	                      [&](IndexEntry& ie) {
		                      const auto value = ie.GetFlag(flag);
		                      ie.SetFlag(flag, !value);
		                      return true;
	                      })
	    .first;
}

scid::core::errorT scidBaseT::setFlag(bool value, scid::core::uint flag, scid::core::uint gNum) {
	ASSERT(gNum < idx->GetNumGames());

	IndexEntry ie = *getIndexEntry(gNum);
	ie.SetFlag(flag, value);

	if (auto errModify = beginTransaction())
		return errModify;

	// Preserve the duplicate list when just a single flag is changed.
	auto keep_duplicates = extractDuplicates();

	const auto res = storage_->codec->saveIndexEntry(ie, gNum);
	const auto err = endTransaction(gNum);

	setDuplicates(std::move(keep_duplicates));
	return res != scid::core::OK ? res : err;
}

scid::core::errorT scidBaseT::setFlags(bool value, scid::core::uint flag, const HFilter& filter) {
	return transformIndex(filter, Progress(),
	                      [&](IndexEntry& ie) {
		                      ie.SetFlag(flag, value);
		                      return true;
	                      })
	    .first;
}

/**
 * Filters
 */
std::string scidBaseT::newFilter() {
	std::string newname = (filters_.size() == 0) ? "a_" : filters_.back().first;
	if (newname[0] == 'z') {
		newname = 'a' + newname;
	} else {
		newname = ++(newname[0]) + newname.substr(1);
	}
	filters_.push_back(std::make_pair(newname, new Filter(numGames())));
	return newname;
}

std::string scidBaseT::composeFilter(std::string_view mainFilter,
                                     std::string_view maskFilter) const {
	std::string res;
	if (mainFilter.empty())
		return res;

	if (mainFilter[0] != '+') {
		res = mainFilter;
	} else {
		size_t maskName = mainFilter.find('+', 1);
		if (maskName != std::string::npos)
			res = mainFilter.substr(1, maskName - 1);
	}

	if (!maskFilter.empty()) {
		res = '+' + res + "+";
		res.append(maskFilter);
	}

	if (getFilter(res) == 0)
		res.clear();
	return res;
}

void scidBaseT::deleteFilter(const char* filterId) {
	for (size_t i = 0, n = filters_.size(); i < n; i++) {
		if (filters_[i].first == filterId) {
			delete filters_[i].second;
			filters_.erase(filters_.begin() + i);
			break;
		}
	}
}

HFilter scidBaseT::getFilter(std::string_view filterId) const {
	const auto findFilter = [&](auto const& id) -> Filter* {
		if (id == "dbfilter")
			return dbFilter;
		if (id == "all")
			return &all_filter_;

		for (auto const& [name, filter] : filters_) {
			if (name == id)
				return filter;
		}
		return nullptr;
	};

	Filter* main = nullptr;
	const Filter* mask = nullptr;
	if (filterId.empty() || filterId[0] != '+') {
		main = findFilter(filterId);
	} else {
		size_t maskName = filterId.find('+', 1);
		if (maskName != std::string::npos) {
			main = findFilter(filterId.substr(1, maskName - 1));
			mask = findFilter(filterId.substr(maskName + 1));
		}
	}
	return HFilter(main, mask);
}

std::pair<std::string, std::string>
scidBaseT::getFilterComponents(std::string_view filterID) const {
	if (filterID.empty())
		return {};

	if (filterID[0] != '+')
		return {std::string(filterID), {}};

	size_t maskName = filterID.find('+', 1);
	ASSERT(maskName != std::string::npos);
	ASSERT(getFilter(filterID.substr(1, maskName - 1)) != nullptr);
	ASSERT(getFilter(filterID.substr(maskName + 1)) != nullptr);

	return {std::string(filterID.substr(1, maskName - 1)),
	        std::string(filterID.substr(maskName + 1))};
}

/**
 * Statistics
 */
const scidBaseT::Stats& scidBaseT::getStats() const {
	if (stats_ == NULL)
		stats_ = new scidBaseT::Stats(this);
	return *stats_;
}

scidBaseT::Stats::Eco::Eco() : count(0) {
	std::fill_n(results, scid::core::NUM_RESULT_TYPES, 0);
}

scidBaseT::Stats::Stats(const scidBaseT* dbase) {
	std::fill(flagCount, flagCount + IndexEntry::IDX_NUM_FLAGS, 0);
	minDate = scid::core::ZERO_DATE;
	maxDate = scid::core::ZERO_DATE;
	nYears = 0;
	sumYears = 0;
	std::fill_n(nResults, scid::core::NUM_RESULT_TYPES, 0);
	nRatings = 0;
	sumRatings = 0;
	minRating = 0;
	maxRating = 0;

	// Read stats from index entry of each game:
	for (gamenumT gnum = 0, n = dbase->numGames(); gnum < n; gnum++) {
		const IndexEntry* ie = dbase->getIndexEntry(gnum);
		nResults[ie->GetResult()]++;
		scid::core::ratingT elo = ie->GetWhiteElo();
		if (elo > 0) {
			nRatings++;
			sumRatings += elo;
			if (minRating == 0) {
				minRating = elo;
			}
			if (elo < minRating) {
				minRating = elo;
			}
			if (elo > maxRating) {
				maxRating = elo;
			}
		}
		elo = ie->GetBlackElo();
		if (elo > 0) {
			nRatings++;
			sumRatings += elo;
			if (minRating == 0) {
				minRating = elo;
			}
			if (elo < minRating) {
				minRating = elo;
			}
			if (elo > maxRating) {
				maxRating = elo;
			}
		}
		scid::core::dateT date = ie->GetDate();
		if (gnum == 0) {
			maxDate = minDate = date;
		}
		if (scid::core::date_GetYear(date) > 0) {
			if (date < minDate) {
				minDate = date;
			}
			if (date > maxDate) {
				maxDate = date;
			}
			nYears++;
			sumYears += scid::core::date_GetYear(date);
		}

		for (scid::core::uint flag = 0; flag < IndexEntry::IDX_NUM_FLAGS; flag++) {
			bool value = ie->GetFlag(1 << flag);
			if (value) {
				flagCount[flag]++;
			}
		}

		scid::core::resultT result = ie->GetResult();
		scidup::eco::Code eco = ie->GetEcoCode();
		if (eco == 0) {
			ecoEmpty_.count++;
			ecoEmpty_.results[result]++;
		} else {
			ecoValid_.count++;
			ecoValid_.results[result]++;
			eco = scidup::eco::reduce(eco);
			ecoStats_[eco].count++;
			ecoStats_[eco].results[result]++;
			eco /= 27;
			ecoGroup3_[eco].count++;
			ecoGroup3_[eco].results[result]++;
			eco /= 10;
			ecoGroup2_[eco].count++;
			ecoGroup2_[eco].results[result]++;
			eco /= 10;
			ecoGroup1_[eco].count++;
			ecoGroup1_[eco].results[result]++;
		}
	}
}

const scidBaseT::Stats::Eco*
scidBaseT::Stats::getEcoStats(const char* ecoStr) const {
	ASSERT(ecoStr != 0);

	if (*ecoStr == 0)
		return &ecoValid_;

	scidup::eco::Code eco = scidup::eco::fromString(ecoStr);
	if (eco == 0)
		return 0;
	eco = scidup::eco::reduce(eco);

	switch (strlen(ecoStr)) {
	case 0:
		return &ecoValid_;
	case 1:
		return &(ecoGroup1_[eco / 2700]);
	case 2:
		return &(ecoGroup2_[eco / 270]);
	case 3:
		return &(ecoGroup3_[eco / 27]);
	case 4:
	case 5:
		return &(ecoStats_[eco]);
	}

	return 0;
}

std::vector<TreeNode> scidBaseT::getTreeStat(const HFilter& filter) const {
	std::vector<TreeNode> res;
	for (gamenumT gnum = 0, n = numGames(); gnum < n; gnum++) {
		scid::core::uint ply = filter.get(gnum);
		if (ply == 0)
			continue;
		else
			ply--;

		const IndexEntry* ie = getIndexEntry(gnum);
		scid::core::FullMove move = StoredLine::getMove(ie->GetStoredLineCode(), ply);
		if (!move)
			move = gameView(ie).getMove(ply);

		auto it = std::find_if(
		    res.begin(), res.end(),
		    [move](auto const& stat) { return stat.move == move; });

		auto& node = (it != res.end()) ? *it : res.emplace_back(move);
		node.add(ie->GetResult(), ie->GetWhiteElo(), ie->GetBlackElo(),
		         ie->GetYear());
	}

	std::sort(res.begin(), res.end(), TreeNode::cmp_ngames_desc());
	return res;
}

scid::core::errorT scidBaseT::getCompactStat(unsigned long long* n_deleted,
                                 unsigned long long* n_unused,
                                 unsigned long long* n_sparse,
                                 unsigned long long* n_badNameId) {
	std::vector<scid::core::uint> nbFreq[NUM_NAME_TYPES];
	for (nameT n = NAME_PLAYER; n < NUM_NAME_TYPES; n++) {
		nbFreq[n].resize(nb_->namebase_size(n), 0);
	}

	uint64_t last_offset = 0;
	*n_sparse = 0;
	*n_deleted = 0;
	for (gamenumT i = 0, n = numGames(); i < n; i++) {
		const IndexEntry* ie = getIndexEntry(i);
		if (ie->GetDeleteFlag()) {
			*n_deleted += 1;
			continue;
		}

		auto offset = ie->GetOffset();
		if (offset < last_offset)
			*n_sparse += 1;
		last_offset = offset;

		nbFreq[NAME_PLAYER][ie->GetWhite()] += 1;
		nbFreq[NAME_PLAYER][ie->GetBlack()] += 1;
		nbFreq[NAME_EVENT][ie->GetEvent()] += 1;
		nbFreq[NAME_SITE][ie->GetSite()] += 1;
		nbFreq[NAME_ROUND][ie->GetRound()] += 1;
	}

	*n_unused = 0;
	for (nameT n = NAME_PLAYER; n < NUM_NAME_TYPES; n++) {
		*n_unused += std::count(nbFreq[n].begin(), nbFreq[n].end(), 0);
	}

	*n_badNameId = idx->GetBadNameIdCount();
	return scid::core::OK;
}

scid::core::errorT scidBaseT::compact(const Progress& progress) {
	std::vector<std::string> filenames = storage_->codec->getFilenames();
	if (filenames.empty())
		return scid::core::ERROR_CodecUnsupFeat;

	// 1) Create a new temporary database
	CodecType dbtype = storage_->codec->getType();
	scidBaseT tmp;
	auto tmp_filename = std::filesystem::path(filenames[0]);
	tmp_filename.replace_filename(tmp_filename.stem().u8string() +
	                              u8"__COMPACT__" +
	                              tmp_filename.extension().u8string());
	if (auto err_Create = tmp.openHelper(codecName(dbtype), FMODE_Create,
	                                     tmp_filename.string().c_str()))
		return err_Create;

	// 2) Create the list of games to be copied
	std::vector<std::pair<uint64_t, gamenumT>> sort;
	for (gamenumT i = 0, n = numGames(); i < n; i++) {
		const IndexEntry* ie = getIndexEntry(i);
		if (ie->GetDeleteFlag()) {
			continue;
		}
		uint64_t order = static_cast<uint64_t>(ie->GetStoredLineCode()) << 56;
		const scid::core::byte* hp = ie->GetHomePawnData();
		order |= static_cast<uint64_t>(hp[0]) << 48;
		order |= static_cast<uint64_t>(hp[1]) << 40;
		order |= static_cast<uint64_t>(hp[2]) << 32;
		order |= static_cast<uint64_t>(hp[3]) << 24;
		order |= ie->GetFinalMatSig() & 0xFFFFFF;
		sort.emplace_back(order, i);
	}
	// Reorder only larger, not PGN, databases
	if (sort.size() > 10000 && storage_->codec->getType() != CodecType::Pgn)
		std::stable_sort(sort.begin(), sort.end());

	// 3) Copy the Index Header
	auto extraInfo = getExtraInfo();
	scid::core::errorT err_Header = tmp.beginTransaction();
	for (auto& pair : extraInfo) {
		if (err_Header != scid::core::OK)
			break;

		if (std::strcmp(pair.first, "autoload") == 0) {
			gamenumT autoloadOld = strGetUnsigned(pair.second.c_str());
			size_t autoloadNew = 1;
			for (size_t i = 0, n = sort.size(); i < n; ++i) {
				if (sort[i].second + 1 == autoloadOld) {
					autoloadNew = i + 1;
					break;
				}
			}
			pair.second = std::to_string(autoloadNew);
		}
		err_Header = tmp.storage_->codec->setExtraInfo(pair.first, pair.second.c_str());
	}

	// 4) Copy the games
	scid::core::uint iProgress = 0;
	bool err_UserCancel = false;
	scid::core::errorT err_AddGame = scid::core::OK;
	for (auto it = sort.cbegin(); it != sort.cend(); ++it) {
		err_AddGame = tmp.importGameHelper(this, it->second);
		if (err_AddGame != scid::core::OK)
			break;

		// TODO:
		//- update bookmarks game number
		//   (*it).second   == old game number
		//   tmp.numGames() == new game number
		if (++iProgress % 8192 == 0) {
			if (!progress.report(iProgress, sort.size())) {
				err_UserCancel = true;
				break;
			}
		}
	}

	// 5) Finalize the new database
	std::vector<std::string> tmp_filenames = tmp.storage_->codec->getFilenames();
	scid::core::errorT err_NbWrite = tmp.endTransaction();
	tmp.Close();
	auto err_Close = (filenames.size() == tmp_filenames.size()) ? scid::core::OK : scid::core::ERROR;

	// 6) Error: cleanup and report
	if (err_Header != scid::core::OK || err_AddGame != scid::core::OK || err_UserCancel ||
	    err_NbWrite != scid::core::OK || err_Close != scid::core::OK) {
		for (size_t i = 0, n = tmp_filenames.size(); i < n; i++) {
			std::remove(tmp_filenames[i].c_str());
		}
		if (err_Header != scid::core::OK)
			return err_Header;
		if (err_AddGame != scid::core::OK)
			return err_AddGame;
		if (err_UserCancel)
			return scid::core::ERROR_UserCancel;
		if (err_NbWrite != scid::core::OK)
			return err_NbWrite;

		return err_Close;
	}

	// 7) Remember the active filters and SortCaches
	std::vector<std::string> filters(filters_.size());
	for (size_t i = 0, n = filters_.size(); i < n; i++) {
		filters[i] = filters_[i].first;
	}
	std::vector<std::pair<std::string, int>> oldSC;
	for (auto& sortCache : sortCaches_) {
		int refCount = sortCache.second->incrRef(0);
		if (refCount >= 0)
			oldSC.emplace_back(sortCache.first, refCount);
	}

	// 8) Remove the old database
	Close();
	for (size_t i = 0, n = filenames.size(); i < n; i++) {
		if (std::remove(filenames[i].c_str()) != 0)
			return scid::core::ERROR_CompactRemove;
	}

	// 9) Success: rename the files and open the new database
	for (size_t i = 0, n = filenames.size(); i < n; i++) {
		const char* s1 = tmp_filenames[i].c_str();
		const char* s2 = filenames[i].c_str();
		std::rename(s1, s2);
	}
	scid::core::errorT res = openHelper(codecName(dbtype), FMODE_Both, filenames[0].c_str());

	// 10) Re-create filters and SortCaches
	if (res == scid::core::OK || res == scid::core::ERROR_NameDataLoss) {
		for (size_t i = 0, n = filters.size(); i < n; i++) {
			filters_.push_back(
			    std::make_pair(filters[i], new Filter(numGames())));
		}
		for (size_t i = 0, n = oldSC.size(); i < n; i++) {
			const std::string& criteria = oldSC[i].first;
			SortCache* sc = SortCache::create(idx, nb_, criteria.c_str());
			if (sc != NULL) {
				sc->incrRef(oldSC[i].second);
				sortCaches_.emplace_back(criteria, sc);
			}
		}
	}

	return res;
}

/**
 * Retrieve a SortCache object matching the supplied @e criteria.
 * A new SortCache with refCount equal to 0 is created if a suitable object is
 * not found in @e sortCaches_. Objects with refCount <= 0 are destroyed by the
 * @e releaseSortCache function independently from the provided @e criteria
 * argument (implementing a rudimentary garbage collector).
 * @param criteria: the list of fields by which games will be ordered.
 *                  Each field should be followed by '+' to indicate an
 *                  ascending order or by '-' for a descending order.
 * @returns a pointer to a SortCache object in case of success, NULL otherwise.
 */
SortCache* scidBaseT::getSortCache(const char* criteria) {
	ASSERT(criteria != NULL);

	for (auto& sortCache : sortCaches_) {
		if (sortCache.first == criteria)
			return sortCache.second;
	}

	SortCache* sc = SortCache::create(idx, getNameBase(), criteria);
	if (sc != NULL)
		sortCaches_.emplace_back(criteria, sc);

	return sc;
}

void scidBaseT::releaseSortCache(const char* criteria) {
	size_t i = 0;
	while (i < sortCaches_.size()) {
		const char* tmp = sortCaches_[i].first.c_str();
		int decr = std::strcmp(criteria, tmp) ? 0 : -1;
		if (sortCaches_[i].second->incrRef(decr) <= 0) {
			delete sortCaches_[i].second;
			sortCaches_.erase(sortCaches_.begin() + i);
			continue; // do not increment i
		}
		i += 1;
	}
}

bool scidBaseT::createSortCache(const char* criteria) {
	if (auto sc = getSortCache(criteria)) {
		sc->incrRef(1);
		return true;
	}
	return false;
}

size_t scidBaseT::listGames(const char* criteria, size_t start, size_t count,
                            const HFilter& filter, gamenumT* destCont) {
	const SortCache* sc = getSortCache(criteria);
	if (sc == NULL)
		return 0;

	return sc->select(start, count, filter, destCont);
}

size_t scidBaseT::sortedPosition(const char* criteria, const HFilter& filter,
                                 gamenumT gameId) {
	ASSERT(filter != NULL && filter->size() <= numGames());

	if (gameId >= numGames() || filter->get(gameId) == 0)
		return INVALID_GAMEID;

	SortCache* sc = getSortCache(criteria);
	if (sc == NULL)
		return INVALID_GAMEID;

	return sc->sortedPosition(gameId, filter);
}

} // namespace scid::database

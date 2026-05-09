#include "scidup/database/game.h"

#include "scidup/database/misc.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace scid::database {

std::string& Game::addTag(std::string_view tag, std::string_view value) {
	return coreGame_.addTag(tag, value);
}

std::string& Game::find_or_create_tag(std::string_view tag) {
	return coreGame_.findOrCreateTag(tag);
}

void Game::viewTagPairsImpl(
    const std::function<void(const char*, const char*)>& visitor) const {
	// TODO [Game]: Move PGN/header tag projection out of Game once the core
	// metadata model exists. This is formatting/export compatibility, not
	// storage for the aggregate itself.
	char strBuf[256];
	visitor("Event", GetEventStr());
	visitor("Site", GetSiteStr());
	date_DecodeToString(GetDate(), strBuf);
	visitor("Date", strBuf);
	visitor("Round", GetRoundStr());
	visitor("White", GetWhiteStr());
	visitor("Black", GetBlackStr());
	visitor("Result", RESULT_LONGSTR[GetResult()]);

	if (auto elo = GetWhiteElo()) {
		std::string rType = "White";
		std::string eloStr = std::to_string(elo);
		rType.append(ratingTypeNames[GetWhiteRatingType()]);
		visitor(rType.c_str(), eloStr.c_str());
	}
	if (auto elo = GetBlackElo()) {
		std::string rType = "Black";
		std::string eloStr = std::to_string(elo);
		rType.append(ratingTypeNames[GetBlackRatingType()]);
		visitor(rType.c_str(), eloStr.c_str());
	}
	if (GetEco() != scidup::eco::ECO_None) {
		scidup::eco::toExtendedString(GetEco(), strBuf);
		visitor("ECO", strBuf);
	}
	if (GetEventDate() != ZERO_DATE) {
		date_DecodeToString(GetEventDate(), strBuf);
		visitor("EventDate", strBuf);
	}
	// TODO?
	// if (*ScidFlags)
	//     visitor("ScidFlags", ScidFlags);

	for (auto& e : GetExtraTags()) {
		visitor(e.first.c_str(), e.second.c_str());
	}
	if (HasNonStandardStart(strBuf, sizeof(strBuf))) {
		visitor("FEN", strBuf);
	}
}

const std::vector<std::pair<std::string, std::string>>& Game::GetExtraTags()
    const {
	return coreGame_.extraTags();
}

const char* Game::FindExtraTag(const char* tag) const {
	auto value = coreGame_.findExtraTag(tag);
	return value ? value->c_str() : NULL;
}

void Game::ClearExtraTags() {
	coreGame_.clearExtraTags();
}

void Game::RemoveExtraTag(std::string_view tag) {
	coreGame_.removeExtraTag(tag);
}

void Game::SetEventStr(const char* str) {
	coreGame_.setEvent(str);
}

void Game::SetSiteStr(const char* str) {
	coreGame_.setSite(str);
}

void Game::SetWhiteStr(const char* str) {
	coreGame_.setWhiteName(str);
}

void Game::SetBlackStr(const char* str) {
	coreGame_.setBlackName(str);
}

void Game::SetRoundStr(const char* str) {
	coreGame_.setRound(str);
}

void Game::SetDate(dateT date) {
	coreGame_.setDate(date);
}

void Game::SetEventDate(dateT date) {
	coreGame_.setEventDate(date);
}

void Game::SetResult(resultT res) {
	coreGame_.setResult(res);
}

void Game::SetWhiteElo(ratingT elo) {
	auto rating = coreGame_.white().rating;
	rating.value = elo;
	coreGame_.setWhiteRating(rating);
}

void Game::SetBlackElo(ratingT elo) {
	auto rating = coreGame_.black().rating;
	rating.value = elo;
	coreGame_.setBlackRating(rating);
}

void Game::SetWhiteRatingType(ratingTypeT b) {
	auto rating = coreGame_.white().rating;
	rating.type = b > 7 ? 0 : b;
	coreGame_.setWhiteRating(rating);
}

void Game::SetBlackRatingType(ratingTypeT b) {
	auto rating = coreGame_.black().rating;
	rating.type = b > 7 ? 0 : b;
	coreGame_.setBlackRating(rating);
}

void Game::SetEco(scidup::eco::Code eco) {
	EcoCode = eco;
	if (eco == scidup::eco::ECO_None) {
		coreGame_.setEco({});
		return;
	}

	char ecoStr[sizeof(scidup::eco::String)] = {};
	scidup::eco::toExtendedString(eco, ecoStr);
	coreGame_.setEco(ecoStr);
}

const char* Game::GetEventStr() const {
	return coreGame_.event().c_str();
}

const char* Game::GetSiteStr() const {
	return coreGame_.site().c_str();
}

const char* Game::GetWhiteStr() const {
	return coreGame_.white().name.c_str();
}

const char* Game::GetBlackStr() const {
	return coreGame_.black().name.c_str();
}

const char* Game::GetRoundStr() const {
	return coreGame_.round().c_str();
}

dateT Game::GetDate() const {
	return coreGame_.date();
}

dateT Game::GetEventDate() const {
	return coreGame_.eventDate();
}

resultT Game::GetResult() const {
	return coreGame_.result();
}

std::string_view Game::GetResultStr() const {
	return coreGame_.resultString();
}

int Game::setRating(colorT col, const char* ratingType, size_t ratingTypeLen,
                    std::pair<const char*, const char*> rating) {
	// TODO [Game]: Move PGN rating-tag parsing to the PGN decoder/import
	// boundary. Core metadata should receive a typed Rating value.
	auto begin = ratingTypeNames;
	const size_t ratingSz = 7;
	auto it = std::find_if(begin, begin + ratingSz, [&](auto rType) {
		return std::equal(ratingType, ratingType + ratingTypeLen, rType,
		                  rType + std::strlen(rType));
	});
	auto rType = static_cast<ratingTypeT>(std::distance(begin, it));
	if (rType >= ratingSz)
		return -1;

	int res = 1;
	auto elo = strGetUnsigned(std::string{rating.first, rating.second}.c_str());
	if (elo > MAX_ELO) {
		elo = 0;
		res = 0;
	}
	if (col == WHITE) {
		SetWhiteElo(static_cast<ratingT>(elo));
		SetWhiteRatingType(rType);
	} else {
		SetBlackElo(static_cast<ratingT>(elo));
		SetBlackRatingType(rType);
	}
	return res;
}

ratingT Game::GetWhiteElo() const {
	return coreGame_.white().rating.value;
}

ratingT Game::GetBlackElo() const {
	return coreGame_.black().rating.value;
}

ratingTypeT Game::GetWhiteRatingType() const {
	return coreGame_.white().rating.type;
}

ratingTypeT Game::GetBlackRatingType() const {
	return coreGame_.black().rating.type;
}

scidup::eco::Code Game::GetEco() const {
	return EcoCode;
}

ratingT Game::GetAverageElo() {
	return coreGame_.averageRating();
}

} // namespace scid::database

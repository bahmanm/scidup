#include "scidup/database/game.h"

#include "scidup/database/misc.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace scid::database {

std::string* Game::find_std_tag(std::string_view tag) {
	if (tag.size() == 5) {
		if (tag == "Event")
			return &EventStr;
		if (tag == "Round")
			return &RoundStr;
		if (tag == "White")
			return &WhiteStr;
		if (tag == "Black")
			return &BlackStr;
	} else if (tag.size() == 4) {
		if (tag == "Site")
			return &SiteStr;
	}
	return nullptr;
}

std::string& Game::addTag(std::string_view tag, std::string_view value) {
	if (auto overwrite = find_std_tag(tag))
		return overwrite->assign(value);

	return extraTags_.emplace_back(tag, value).second;
}

std::string& Game::find_or_create_tag(std::string_view tag) {
	if (auto value = find_std_tag(tag))
		return *value;

	auto it = std::find_if(extraTags_.begin(), extraTags_.end(),
	                       [&](auto const& elem) { return elem.first == tag; });
	if (it != extraTags_.end())
		return it->second;

	return extraTags_.emplace_back(tag, std::string()).second;
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
	return extraTags_;
}

const char* Game::FindExtraTag(const char* tag) const {
	for (auto& e : extraTags_) {
		if (e.first == tag)
			return e.second.c_str();
	}
	return NULL;
}

void Game::ClearExtraTags() {
	extraTags_.clear();
}

void Game::RemoveExtraTag(std::string_view tag) {
	std::erase_if(extraTags_, [&](auto elem) { return elem.first == tag; });
}

void Game::SetEventStr(const char* str) {
	EventStr = str;
}

void Game::SetSiteStr(const char* str) {
	SiteStr = str;
}

void Game::SetWhiteStr(const char* str) {
	WhiteStr = str;
}

void Game::SetBlackStr(const char* str) {
	BlackStr = str;
}

void Game::SetRoundStr(const char* str) {
	RoundStr = str;
}

void Game::SetDate(dateT date) {
	Date = date;
}

void Game::SetEventDate(dateT date) {
	EventDate = date;
}

void Game::SetResult(resultT res) {
	Result = res;
}

void Game::SetWhiteElo(ratingT elo) {
	WhiteElo = elo;
}

void Game::SetBlackElo(ratingT elo) {
	BlackElo = elo;
}

void Game::SetWhiteRatingType(ratingTypeT b) {
	WhiteRatingType = b > 7 ? 0 : b;
}

void Game::SetBlackRatingType(ratingTypeT b) {
	BlackRatingType = b > 7 ? 0 : b;
}

void Game::SetEco(scidup::eco::Code eco) {
	EcoCode = eco;
}

const char* Game::GetEventStr() const {
	return EventStr.c_str();
}

const char* Game::GetSiteStr() const {
	return SiteStr.c_str();
}

const char* Game::GetWhiteStr() const {
	return WhiteStr.c_str();
}

const char* Game::GetBlackStr() const {
	return BlackStr.c_str();
}

const char* Game::GetRoundStr() const {
	return RoundStr.c_str();
}

dateT Game::GetDate() const {
	return Date;
}

dateT Game::GetEventDate() const {
	return EventDate;
}

resultT Game::GetResult() const {
	return Result;
}

std::string_view Game::GetResultStr() const {
	using namespace std::literals;
	static std::string_view res[] = {"*"sv, "1-0"sv, "0-1"sv, "1/2-1/2"sv};
	return res[Result];
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
	return WhiteElo;
}

ratingT Game::GetBlackElo() const {
	return BlackElo;
}

ratingTypeT Game::GetWhiteRatingType() const {
	return WhiteRatingType;
}

ratingTypeT Game::GetBlackRatingType() const {
	return BlackRatingType;
}

scidup::eco::Code Game::GetEco() const {
	return EcoCode;
}

ratingT Game::GetAverageElo() {
	auto white = WhiteElo;
	auto black = BlackElo;
	return (white == 0 || black == 0) ? 0 : (white + black) / 2;
}

} // namespace scid::database

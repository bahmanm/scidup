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
			return &header_.event.name;
		if (tag == "Round")
			return &header_.event.round;
		if (tag == "White")
			return &header_.white.name;
		if (tag == "Black")
			return &header_.black.name;
	} else if (tag.size() == 4) {
		if (tag == "Site")
			return &header_.event.site;
	}
	return nullptr;
}

std::string& Game::addTag(std::string_view tag, std::string_view value) {
	if (auto overwrite = find_std_tag(tag))
		return overwrite->assign(value);

	return header_.tags.emplace_back(tag, value).second;
}

std::string& Game::find_or_create_tag(std::string_view tag) {
	if (auto value = find_std_tag(tag))
		return *value;

	auto it = std::find_if(header_.tags.begin(), header_.tags.end(),
	                       [&](auto const& elem) { return elem.first == tag; });
	if (it != header_.tags.end())
		return it->second;

	return header_.tags.emplace_back(tag, std::string()).second;
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
	return header_.tags;
}

const char* Game::FindExtraTag(const char* tag) const {
	for (auto& e : header_.tags) {
		if (e.first == tag)
			return e.second.c_str();
	}
	return NULL;
}

void Game::ClearExtraTags() {
	header_.tags.clear();
}

void Game::RemoveExtraTag(std::string_view tag) {
	std::erase_if(header_.tags, [&](auto elem) { return elem.first == tag; });
}

void Game::SetEventStr(const char* str) {
	header_.event.name = str;
}

void Game::SetSiteStr(const char* str) {
	header_.event.site = str;
}

void Game::SetWhiteStr(const char* str) {
	header_.white.name = str;
}

void Game::SetBlackStr(const char* str) {
	header_.black.name = str;
}

void Game::SetRoundStr(const char* str) {
	header_.event.round = str;
}

void Game::SetDate(dateT date) {
	header_.event.date = date;
}

void Game::SetEventDate(dateT date) {
	header_.event.eventDate = date;
}

void Game::SetResult(resultT res) {
	header_.result = res;
}

void Game::SetWhiteElo(ratingT elo) {
	header_.white.rating.value = elo;
}

void Game::SetBlackElo(ratingT elo) {
	header_.black.rating.value = elo;
}

void Game::SetWhiteRatingType(ratingTypeT b) {
	header_.white.rating.type = b > 7 ? 0 : b;
}

void Game::SetBlackRatingType(ratingTypeT b) {
	header_.black.rating.type = b > 7 ? 0 : b;
}

void Game::SetEco(scidup::eco::Code eco) {
	EcoCode = eco;
}

const char* Game::GetEventStr() const {
	return header_.event.name.c_str();
}

const char* Game::GetSiteStr() const {
	return header_.event.site.c_str();
}

const char* Game::GetWhiteStr() const {
	return header_.white.name.c_str();
}

const char* Game::GetBlackStr() const {
	return header_.black.name.c_str();
}

const char* Game::GetRoundStr() const {
	return header_.event.round.c_str();
}

dateT Game::GetDate() const {
	return header_.event.date;
}

dateT Game::GetEventDate() const {
	return header_.event.eventDate;
}

resultT Game::GetResult() const {
	return header_.result;
}

std::string_view Game::GetResultStr() const {
	using namespace std::literals;
	static std::string_view res[] = {"*"sv, "1-0"sv, "0-1"sv, "1/2-1/2"sv};
	return res[header_.result];
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
	return header_.white.rating.value;
}

ratingT Game::GetBlackElo() const {
	return header_.black.rating.value;
}

ratingTypeT Game::GetWhiteRatingType() const {
	return header_.white.rating.type;
}

ratingTypeT Game::GetBlackRatingType() const {
	return header_.black.rating.type;
}

scidup::eco::Code Game::GetEco() const {
	return EcoCode;
}

ratingT Game::GetAverageElo() {
	auto white = header_.white.rating.value;
	auto black = header_.black.rating.value;
	return (white == 0 || black == 0) ? 0 : (white + black) / 2;
}

} // namespace scid::database

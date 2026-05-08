#pragma once

#include "scidup/core/date.h"
#include "scidup/core/error.h"
#include "scidup/core/game_result.h"
#include "scidup/core/position.h"
#include "scidup/core/rating.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scid::core {

struct Player {
	std::string name;
	scid::database::ratingT rating = 0;
	scid::database::ratingTypeT ratingType = scid::database::RATING_Elo;
};

class Game {
public:
	Game();

	void clear();

	const std::string& event() const;
	const std::string& site() const;
	const std::string& round() const;
	const Player& white() const;
	const Player& black() const;
	scid::database::dateT date() const;
	scid::database::dateT eventDate() const;
	scid::database::resultT result() const;

	void setEvent(std::string_view value);
	void setSite(std::string_view value);
	void setRound(std::string_view value);
	void setWhite(Player value);
	void setBlack(Player value);
	void setDate(scid::database::dateT value);
	void setEventDate(scid::database::dateT value);
	void setResult(scid::database::resultT value);

	std::string& addTag(std::string_view tag, std::string_view value);
	const std::vector<std::pair<std::string, std::string>>& extraTags() const;
	const std::string* findExtraTag(std::string_view tag) const;
	void clearExtraTags();
	void removeExtraTag(std::string_view tag);

	bool hasNonStandardStart() const;
	bool hasNonStandardStart(char* outFen, std::size_t outFenLen) const;
	const scid::database::Position* startPosition() const;
	scid::database::errorT setStartFen(const char* fen);
	void setStartPosition(const scid::database::Position& position);
	void clearStartPosition();
	long long initialPlyCounter() const;

private:
	std::string* findStandardTag(std::string_view tag);

	std::string event_;
	std::string site_;
	std::string round_;
	Player white_;
	Player black_;
	scid::database::dateT date_ = scid::database::ZERO_DATE;
	scid::database::dateT eventDate_ = scid::database::ZERO_DATE;
	scid::database::resultT result_ = scid::database::RESULT_None;
	std::vector<std::pair<std::string, std::string>> extraTags_;
	std::optional<scid::database::Position> startPosition_;
};

} // namespace scid::core

#pragma once

#include "scidup/core/date.h"
#include "scidup/core/error.h"
#include "scidup/core/game_result.h"
#include "scidup/core/nags.h"
#include "scidup/core/notation.h"
#include "scidup/core/position.h"
#include "scidup/core/rating.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scid::core {

using TagPair = std::pair<std::string, std::string>;

struct Rating {
	scid::database::ratingT value = 0;
	scid::database::ratingTypeT type = scid::database::RATING_Elo;
};

struct Player {
	std::string name;
	Rating rating;
};

struct EventInfo {
	std::string name;
	std::string site;
	std::string round;
	scid::database::dateT date = scid::database::ZERO_DATE;
	scid::database::dateT eventDate = scid::database::ZERO_DATE;
};

struct GameHeader {
	EventInfo event;
	Player white;
	Player black;
	scid::database::resultT result = scid::database::RESULT_None;
	std::vector<TagPair> tags;
};

struct MoveAction {
	scid::database::squareT from = scid::database::NULL_SQUARE;
	scid::database::squareT to = scid::database::NULL_SQUARE;
	scid::database::pieceT promotion = scid::database::EMPTY;

	bool isNull() const;
	std::string longNotation() const;
};

struct MoveMetadata {
	std::vector<std::uint8_t> nags;
	std::string comment;
};

struct Variation;

struct Move {
	MoveAction action;
	std::string san;
	MoveMetadata metadata;
	std::vector<Variation> childVariations;
};

struct MoveSequence {
	std::vector<Move> moves;
};

struct Variation {
	MoveSequence line;
};

struct Movetext {
	MoveSequence mainline;
};

class Game {
public:
	Game();

	void clear();

	const GameHeader& header() const;
	const Movetext& movetext() const;
	const std::string& event() const;
	const std::string& site() const;
	const std::string& round() const;
	const Player& white() const;
	const Player& black() const;
	scid::database::dateT date() const;
	scid::database::dateT eventDate() const;
	scid::database::resultT result() const;
	std::string_view resultString() const;
	scid::database::ratingT averageRating() const;

	void setEvent(std::string_view value);
	void setSite(std::string_view value);
	void setRound(std::string_view value);
	void setWhiteName(std::string_view value);
	void setBlackName(std::string_view value);
	void setWhite(Player value);
	void setBlack(Player value);
	void setWhiteRating(Rating value);
	void setBlackRating(Rating value);
	void setDate(scid::database::dateT value);
	void setEventDate(scid::database::dateT value);
	void setResult(scid::database::resultT value);

	std::string& addTag(std::string_view tag, std::string_view value);
	std::string& findOrCreateTag(std::string_view tag);
	const std::vector<TagPair>& extraTags() const;
	const std::string* findExtraTag(std::string_view tag) const;
	void clearExtraTags();
	void removeExtraTag(std::string_view tag);

	bool hasNonStandardStart() const;
	bool hasNonStandardStart(char* outFen, std::size_t outFenLen) const;
	scid::database::Position* startPosition();
	const scid::database::Position* startPosition() const;
	scid::database::errorT setStartFen(const char* fen);
	void setStartPosition(const scid::database::Position& position);
	void clearStartPosition();
	long long initialPlyCounter() const;

	Move& appendMainlineMove(MoveAction action);
	void clearMovetext();

private:
	std::string* findStandardTag(std::string_view tag);

	GameHeader header_;
	Movetext movetext_;
	std::optional<scid::database::Position> startPosition_;
};

} // namespace scid::core

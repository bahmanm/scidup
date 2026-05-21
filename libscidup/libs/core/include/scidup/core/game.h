#pragma once

#include "scidup/core/date.h"
#include "scidup/core/error.h"
#include "scidup/core/game_result.h"
#include "scidup/core/move.h"
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

class MovetextCursor;

using TagPair = std::pair<std::string, std::string>;

struct Rating {
	scid::core::ratingT value = 0;
	scid::core::ratingTypeT type = scid::core::RATING_Elo;
};

struct Player {
	std::string name;
	Rating rating;
};

struct EventInfo {
	std::string name;
	std::string site;
	std::string round;
	scid::core::dateT date = scid::core::ZERO_DATE;
	scid::core::dateT eventDate = scid::core::ZERO_DATE;
};

struct GameHeader {
	EventInfo event;
	Player white;
	Player black;
	scid::core::resultT result = scid::core::RESULT_None;
	std::string eco;
	std::vector<TagPair> tags;
};

struct MoveMetadata {
	std::vector<std::uint8_t> nags;
	std::string comment;
};

struct Variation;

struct Move {
	Variation& addVariation(std::string_view initialComment = {});

	MoveSpec spec;
	std::string san;
	MoveMetadata metadata;
	std::vector<Variation> childVariations;
};

struct MoveSequence {
	Move& appendMove(MoveSpec spec);

	std::vector<Move> moves;
};

struct Variation {
	std::string initialComment;
	MoveSequence line;
};

struct Movetext {
	std::string initialComment;
	MoveSequence mainline;
};

class Game {
public:
	Game();

	void clear();

	const GameHeader& header() const;
	const Movetext& movetext() const;
	std::string_view initialComment() const;
	std::size_t mainlineHalfMoveCount() const;
	const std::string& event() const;
	const std::string& site() const;
	const std::string& round() const;
	const Player& white() const;
	const Player& black() const;
	scid::core::dateT date() const;
	scid::core::dateT eventDate() const;
	scid::core::resultT result() const;
	std::string_view resultString() const;
	const std::string& eco() const;
	scid::core::ratingT averageRating() const;

	void setEvent(std::string_view value);
	void setSite(std::string_view value);
	void setRound(std::string_view value);
	void setWhiteName(std::string_view value);
	void setBlackName(std::string_view value);
	void setWhite(Player value);
	void setBlack(Player value);
	void setWhiteRating(Rating value);
	void setBlackRating(Rating value);
	void setDate(scid::core::dateT value);
	void setEventDate(scid::core::dateT value);
	void setResult(scid::core::resultT value);
	void setEco(std::string_view value);

	std::string& addTag(std::string_view tag, std::string_view value);
	std::string& findOrCreateTag(std::string_view tag);
	const std::vector<TagPair>& extraTags() const;
	const std::string* findExtraTag(std::string_view tag) const;
	void clearExtraTags();
	void removeExtraTag(std::string_view tag);

	bool hasNonStandardStart() const;
	bool hasNonStandardStart(char* outFen, std::size_t outFenLen) const;
	scid::core::Position* startPosition();
	const scid::core::Position* startPosition() const;
	scid::core::errorT setStartFen(const char* fen);
	void setStartPosition(const scid::core::Position& position);
	void clearStartPosition();
	long long initialPlyCounter() const;

	Move& appendMainlineMove(MoveSpec spec);
	void setInitialComment(std::string_view value);
	void clearMovetext();
	void stripMovetext(bool variations, bool comments, bool nags);

private:
	friend class MovetextCursor;

	std::string* findStandardTag(std::string_view tag);

	GameHeader header_;
	Movetext movetext_;
	std::optional<scid::core::Position> startPosition_;
};

} // namespace scid::core

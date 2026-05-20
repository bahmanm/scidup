#ifndef SCIDUP_DATABASE_INTERNAL_PGNPARSE_IMPL_H
#define SCIDUP_DATABASE_INTERNAL_PGNPARSE_IMPL_H

#include "scidup/core/game_cursor.h"
#include "scidup/core/movetext_cursor.h"
#include "pgn_lexer.h"
#include "nag_format.h"
#include "scidup/database/pgnparse.h"
#include "scidup/database/misc.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scid::database {

inline scid::core::MovetextLocation currentLocation(
    const Game& game, const scid::core::MovetextLocation* location) {
	(void)game;
	return location ? *location : scid::core::MovetextLocation{};
}

inline void setCurrentLocation(Game& game,
                               scid::core::MovetextLocation* location,
                               scid::core::MovetextLocation value) {
	if (location) {
		*location = value;
	}
	(void)game;
}

inline std::string_view currentMoveComment(
    const Game& game, const scid::core::MovetextLocation* location = nullptr) {
	scid::core::GameCursor cursor(game.coreGame());
	[[maybe_unused]] const bool restored = cursor.restore(
	    currentLocation(game, location));
	ASSERT(restored);

	if (auto move = cursor.previousMove())
		return move->metadata.comment;
	if (auto variation = cursor.currentVariation())
		return variation->initialComment;
	return game.coreGame().movetext().initialComment;
}

inline bool setCurrentMoveComment(
    Game& game, std::string_view comment,
    const scid::core::MovetextLocation* location = nullptr) {
	scid::core::MovetextCursor cursor(game.coreGame());
	[[maybe_unused]] const bool restored =
	    cursor.restore(currentLocation(game, location));
	ASSERT(restored);
	return cursor.setComment(comment);
}

inline bool addCurrentMoveNag(
    Game& game, byte nag,
    const scid::core::MovetextLocation* location = nullptr) {
	scid::core::MovetextCursor cursor(game.coreGame());
	[[maybe_unused]] const bool restored =
	    cursor.restore(currentLocation(game, location));
	ASSERT(restored);
	return cursor.addPreviousMoveNag(nag);
}

inline errorT resetStartFen(Game& game, scid::core::MovetextLocation* location,
                            const char* fen) {
	Position position;
	if (auto err = position.ReadFromFEN(fen))
		return err;

	game.coreGame().clearMovetext();
	game.coreGame().setStartPosition(position);

	setCurrentLocation(game, location, scid::core::MovetextLocation{});
	return OK;
}

class PgnVisitor {
	Game& game;
	scid::core::MovetextLocation ownedLocation_;
	scid::core::MovetextLocation* location_;
	std::vector<std::pair<size_t, std::string>> errors_;
	size_t linenum_ = 0;
	int nErrorsAllowed_ = 2;

	using TView = std::pair<const char*, const char*>;

public:
	explicit PgnVisitor(Game& g,
	                    scid::core::MovetextLocation* location = nullptr)
	    : game(g), location_(location ? location : &ownedLocation_) {}

	auto const& errors() const { return errors_; }
	size_t lineNumber() const { return linenum_; }
	bool ignoredAfterError() const { return nErrorsAllowed_ < 0; }

	void visitPGN_inputEOF() {
		if (nErrorsAllowed_)
			logErr("Unexpected end of input (result missing ?).");
	}

	void visitPGN_inputUnexpectedPGNHeader() {
		if (nErrorsAllowed_)
			logErr("Unexpected end of game: PGN header '[' seen "
			       "inside game (result missing ?).");
	}

	bool visitPGN_Comment(TView comment) {
		if (nErrorsAllowed_ < 0) {
			linenum_ += std::count(comment.first, comment.second, '\n');
			return true;
		}

		linenum_ += pgn::trim(comment);
		auto str = std::string(currentMoveComment(game, location_));
		auto prevSz = str.size();
		str.append(comment.first, comment.second);
		linenum_ += pgn::normalize(str, prevSz);
		[[maybe_unused]] const bool updated =
		    setCurrentMoveComment(game, str, location_);
		ASSERT(updated);
		return true;
	}

	bool visitPGN_EndOfLine() {
		++linenum_;
		return true;
	}

	void visitPGN_EPD(TView line) {
		ASSERT(nErrorsAllowed_ >= 0);
		std::string tmp(line.first, line.second);
		if (resetStartFen(game, location_, tmp.c_str()) == OK) {
			auto opcode = std::find_if(
			    line.first, line.second, [spaces = 0](char ch) mutable {
				    return (ch == ' ') ? spaces++ == 4 : spaces == 4;
			    });
			visitPGN_Comment(std::make_pair(opcode, line.second));
		} else {
			logFatalErr("Failed to parse EPD record: ", line);
		}
	}

	bool visitPGN_Escape(TView) { return true; }

	bool visitPGN_MoveNum(TView) { return true; }

	bool visitPGN_NAG(TView token) {
		if (nErrorsAllowed_ < 0)
			return true;

		auto nag_code = game_parseNag(token);
		if (nag_code == 0 || !addCurrentMoveNag(game, nag_code, location_))
			return logErr("Invalid annotation symbol: ", token);

		return true;
	}

	void visitPGN_ResultFinal(char resultCh) {
		auto result = RESULT_None;
		switch (resultCh) {
		case '0':
			result = RESULT_Black;
			break;
		case '1':
			result = RESULT_White;
			break;
		case '/':
			result = RESULT_Draw;
			break;
		default:
			ASSERT(resultCh == '*');
		}

		auto prev_result = game.coreGame().result();
		if (result != prev_result) {
			game.coreGame().setResult(result);
			if (prev_result != RESULT_None && nErrorsAllowed_ >= 0)
				logErr("Final result did not match the header tag.");
		}
	}

	bool visitPGN_SANMove(TView tok) {
		if (nErrorsAllowed_ < 0)
			return true;

		scid::core::GameCursor cursor(game.coreGame());
		[[maybe_unused]] const bool restored = cursor.restore(
		    currentLocation(game, location_));
		ASSERT(restored);
		auto position = cursor.currentPosition();
		if (!position)
			return logFatalErr("Failed to parse the move: ", tok);

		simpleMoveT sm;
		auto err = position->ParseMove(&sm, tok.first, tok.second);
		if (err != OK) {
			if (game_parseNag(tok))
				return visitPGN_NAG(tok);

			return logFatalErr("Failed to parse the move: ", tok);
		}
		scid::core::MovetextCursor moveCursor(game.coreGame());
		[[maybe_unused]] const bool moveRestored =
		    moveCursor.restore(currentLocation(game, location_));
		ASSERT(moveRestored);
		moveCursor.addMove(
		    {sm.from, sm.to, sm.promote, sm.isCastle() != 0});
		setCurrentLocation(game, location_, moveCursor.location());
		return true;
	}

	bool visitPGN_Suffix(TView token) { return visitPGN_NAG(token); }

	bool visitPGN_TagPair(TView tag, TView value) {
		linenum_ += std::count(value.first, value.second, '\n');
		if (nErrorsAllowed_ < 0)
			return true;

		auto tagLen = std::distance(tag.first, tag.second);
		auto valueLen = std::distance(value.first, value.second);
		if (tagLen == 0 || tagLen + valueLen > 240 ||
		    !parseTagPair(tag.first, tagLen, value))
		{
			const auto tag_sv = std::string_view(tag.first, tagLen);
			const auto value_sv = std::string_view(value.first, valueLen);
			std::string tag_parsed;
			std::string value_parsed;
			if (auto parsed = parsedTagValue(tag_sv)) {
				tag_parsed = tag_sv;
				value_parsed = std::move(*parsed);
			}
			if (tag_sv != tag_parsed || value_sv != value_parsed) {
				std::string err(tag_sv);
				err.append(" \"");
				err.append(value_sv);
				err.append("\" ==> ");
				err.append(tag_parsed);
				err.append(" \"");
				err.append(value_parsed);
				err.push_back('"');
				logWarning("Error parsing the tag pair: ",
				           {err.c_str(), err.c_str() + err.size()});
			}
		}
		return true;
	}

	bool visitPGN_Unknown(TView token) {
		if (nErrorsAllowed_ < 0)
			return true;

		std::string tmp(token.first, token.second);
		if (tmp == "0-0" || tmp == "00") {
			tmp = "O-O";
			return visitPGN_SANMove({tmp.c_str(), tmp.c_str() + 3});
		}
		if (tmp == "0-0-0" || tmp == "000") {
			tmp = "O-O-O";
			return visitPGN_SANMove({tmp.c_str(), tmp.c_str() + 5});
		}

		return logErr("Unknown token: ", token);
	}

	bool visitPGN_VariationStart() {
		if (nErrorsAllowed_ < 0)
			return true;

		scid::core::MovetextCursor cursor(game.coreGame());
		[[maybe_unused]] const bool restored =
		    cursor.restore(currentLocation(game, location_));
		ASSERT(restored);
		if (!cursor.previous() || !cursor.addVariation())
			return logFatalErr("Failed to add a new variation.");
		setCurrentLocation(game, location_, cursor.location());

		return true;
	}

	bool visitPGN_VariationEnd() {
		if (nErrorsAllowed_ < 0)
			return true;

		scid::core::GameCursor cursor(game.coreGame());
		[[maybe_unused]] const bool restored =
		    cursor.restore(currentLocation(game, location_));
		ASSERT(restored);
		if (!cursor.exitVariation() || !cursor.next())
			return logFatalErr("Failed to exit from variation.");
		setCurrentLocation(game, location_, cursor.location());

		return true;
	}

private:
	bool logWarning(const char* str1, TView str2 = {nullptr, nullptr}) {
		errors_.emplace_back(linenum_, str1);
		if (std::distance(str2.first, str2.second) > 200) {
			errors_.back().second.append(str2.first, 200);
			errors_.back().second.append("...");
		} else {
			errors_.back().second.append(str2.first, str2.second);
		}
		return true;
	}

	bool logErr(const char* str1, TView str2 = {nullptr, nullptr}) {
		--nErrorsAllowed_;
		return logWarning(str1, str2);
	}

	bool logFatalErr(const char* str1, TView str2 = {nullptr, nullptr}) {
		nErrorsAllowed_ = 0;
		return logErr(str1, str2);
	}

	bool parseTagResult(TView str) {
		auto len = std::distance(str.first, str.second);
		if (len > 0 && *str.first == '*') {
			game.coreGame().setResult(RESULT_None);
			return true;
		}
		if (len >= 3) {
			if (std::equal(str.first, str.first + 3, "1-0")) {
				game.coreGame().setResult(RESULT_White);
				return true;
			}
			if (std::equal(str.first, str.first + 3, "0-1")) {
				game.coreGame().setResult(RESULT_Black);
				return true;
			}
			if (std::equal(str.first, str.first + 3, "1/2")) {
				game.coreGame().setResult(RESULT_Draw);
				return true;
			}
		}
		return logErr("Invalid Result tag: ", str);
	}

	int parseRating(colorT col, const char* ratingType, size_t ratingTypeLen,
	                TView rating) {
		const auto ratingTypeView = std::string_view{ratingType, ratingTypeLen};
		constexpr size_t ratingTypeCount = 7;
		auto begin = ratingTypeNames;
		auto it = std::find_if(begin, begin + ratingTypeCount,
		                       [&](auto rType) {
			                       return ratingTypeView == std::string_view{rType};
		                       });
		auto rType = static_cast<ratingTypeT>(std::distance(begin, it));
		if (rType >= ratingTypeCount)
			return -1;

		int res = 1;
		auto elo = strGetUnsigned(std::string{rating.first, rating.second}.c_str());
		if (elo > MAX_ELO) {
			elo = 0;
			res = 0;
		}
		if (col == WHITE) {
			game.coreGame().setWhiteRating(
			    {static_cast<ratingT>(elo), rType});
		} else {
			game.coreGame().setBlackRating(
			    {static_cast<ratingT>(elo), rType});
		}
		return res;
	}

	std::optional<std::string> parsedTagValue(std::string_view tag) const {
		auto const& coreGame = game.coreGame();
		char strBuf[256];

		if (tag == "Event")
			return coreGame.event();
		if (tag == "Site")
			return coreGame.site();
		if (tag == "Date") {
			date_DecodeToString(coreGame.date(), strBuf);
			return strBuf;
		}
		if (tag == "Round")
			return coreGame.round();
		if (tag == "White")
			return coreGame.white().name;
		if (tag == "Black")
			return coreGame.black().name;
		if (tag == "Result")
			return std::string(coreGame.resultString());

		if (coreGame.white().rating.value != 0) {
			std::string ratingTag = "White";
			ratingTag.append(ratingTypeNames[coreGame.white().rating.type]);
			if (tag == ratingTag)
				return std::to_string(coreGame.white().rating.value);
		}
		if (coreGame.black().rating.value != 0) {
			std::string ratingTag = "Black";
			ratingTag.append(ratingTypeNames[coreGame.black().rating.type]);
			if (tag == ratingTag)
				return std::to_string(coreGame.black().rating.value);
		}
		if (tag == "ECO" && !coreGame.eco().empty())
			return coreGame.eco();
		if (tag == "EventDate" &&
		    coreGame.eventDate() != scid::database::ZERO_DATE) {
			date_DecodeToString(coreGame.eventDate(), strBuf);
			return strBuf;
		}
		for (auto const& entry : coreGame.extraTags()) {
			if (tag == entry.first)
				return entry.second;
		}
		if (tag == "FEN" && coreGame.hasNonStandardStart(strBuf, sizeof(strBuf)))
			return strBuf;

		return std::nullopt;
	}

	bool parseTagPair(const char* tag, size_t tagLen, TView value) {
		switch (tagLen) {
		case 3:
			if (std::equal(tag, tag + 3, "ECO")) {
				std::string tmp{value.first, value.second};
				scidup::eco::String ecoStr;
				scidup::eco::toExtendedString(
				    scidup::eco::fromString(tmp.c_str()), ecoStr);
				game.coreGame().setEco(ecoStr);
				return true;
			}
			if (std::equal(tag, tag + 3, "FEN")) {
				std::string tmp{value.first, value.second};
				return resetStartFen(game, location_, tmp.c_str()) == OK;
			}
			break;
		case 4:
			if (std::equal(tag, tag + 4, "Date")) {
				const auto date = date_parsePGNTag(value);
				game.coreGame().setDate(date);
				return !date_isPartial(date);
			}
			break;
		case 6:
			if (std::equal(tag, tag + 6, "Result"))
				return parseTagResult(value);
			break;
		case 7:
			if (std::equal(tag, tag + 7, "UTCDate") &&
			    game.coreGame().date() == ZERO_DATE) {
				const auto date = date_parsePGNTag(value);
				if (!date_isPartial(date))
					game.coreGame().setDate(date);
			}
			break;
		case 9:
			if (std::equal(tag, tag + 9, "EventDate")) {
				const auto date = date_parsePGNTag(value);
				game.coreGame().setEventDate(date);
				return !date_isPartial(date);
			}
			if (std::equal(tag, tag + 9, "ScidFlags")) {
				game.setScidFlags(value.first,
				                  std::distance(value.first, value.second));
				return true;
			}
			break;
		}
		if (tagLen >= 8) {
			if (std::equal(tag, tag + 5, "White") &&
			    game.coreGame().white().rating.value == 0) {
				auto res = parseRating(WHITE, tag + 5, tagLen - 5, value);
				if (res >= 0)
					return res;
			} else if (std::equal(tag, tag + 5, "Black") &&
			           game.coreGame().black().rating.value == 0) {
				auto res = parseRating(BLACK, tag + 5, tagLen - 5, value);
				if (res >= 0)
					return res;
			}
		}
		size_t valueLen = std::distance(value.first, value.second);
		auto& str =
		    game.coreGame().addTag({tag, tagLen}, {value.first, valueLen});
		linenum_ += pgn::normalize<true>(str, 0);
		return true;
	}
};

namespace pgn_impl {

inline bool logGame(PgnParseLog& log, size_t nBytes, const PgnVisitor& visitor) {
	++log.n_games;
	for (auto& e : visitor.errors()) {
		log.log += "(game " + std::to_string(log.n_games);
		log.log += ", line " + std::to_string(log.n_lines + e.first) + ") ";
		log.log += e.second;
		log.log += "\n";
	}
	log.n_lines += visitor.lineNumber();
	log.n_bytes += nBytes;
	if (visitor.ignoredAfterError()) {
		log.log += "(game " + std::to_string(log.n_games);
		log.log += ", line " + std::to_string(log.n_lines) + ") ";
		log.log += "End of game, ignored the part after the last error.\n";
		return false;
	}
	return true;
}

} // namespace pgn_impl

} // namespace scid::database

#endif

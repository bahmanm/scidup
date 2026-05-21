#include "scidup/core/nags.h"

#include "nagtext.h"

#include <algorithm>
#include <charconv>
#include <iterator>

namespace scid::core {

namespace {

std::uint8_t parseUnsigned(std::string_view text) {
	unsigned value = 0;
	const auto* begin = text.data();
	const auto* end = begin + text.size();
	const auto result = std::from_chars(begin, end, value);
	if (result.ec != std::errc{} || result.ptr != end || value > 255)
		return 0;
	return static_cast<std::uint8_t>(value);
}

} // namespace

std::string_view nagSymbol(std::uint8_t nag) {
	if (nag >= std::size(plainNagSymbols))
		return {};
	return plainNagSymbols[nag];
}

std::string formatNag(std::uint8_t nag, bool asSymbol) {
	if (nag == 0)
		return {};

	if (asSymbol) {
		auto symbol = nagSymbol(nag);
		if (!symbol.empty())
			return std::string{symbol};
	}

	return "$" + std::to_string(nag);
}

std::uint8_t parseNag(std::string_view text) {
	if (text.empty() || text.size() > 7)
		return 0;

	if (text.front() == '$')
		return parseUnsigned(text.substr(1));

	if (std::all_of(text.begin(), text.end(),
	                [](char ch) { return ch >= '0' && ch <= '9'; }))
		return parseUnsigned(text);

	const auto* str = text.data();
	const auto size = text.size();
	auto at = [&](std::size_t index) -> char {
		return index < size ? str[index] : '\0';
	};

	if (at(0) == '!') {
		if (size == 1)
			return NAG_GoodMove;
		if (at(1) == '!' && size == 2)
			return NAG_ExcellentMove;
		if (at(1) == '?' && size == 2)
			return NAG_InterestingMove;
		return 0;
	}

	if (at(0) == '?') {
		if (size == 1)
			return NAG_PoorMove;
		if (at(1) == '?' && size == 2)
			return NAG_Blunder;
		if (at(1) == '!' && size == 2)
			return NAG_DubiousMove;
		return 0;
	}

	if (at(0) == '+') {
		if (at(1) == '=')
			return NAG_WhiteSlight;
		if (at(1) == '-' && size == 2)
			return NAG_WhiteDecisive;
		if (at(1) == '>')
			return NAG_WithAttack;
		if (at(1) == '/' && at(2) == '-')
			return NAG_WhiteClear;
		if (at(1) == '/' && at(2) == '=')
			return NAG_WhiteSlight;
		if (at(1) == '-' && at(2) == '-')
			return NAG_WhiteCrushing;
		return 0;
	}

	if (at(0) == '=') {
		if (size == 1)
			return NAG_Equal;
		if (at(1) == '+')
			return NAG_BlackSlight;
		if (at(1) == '/' && at(2) == '+')
			return NAG_BlackSlight;
		if (at(1) == '/' && at(2) == '&')
			return NAG_Compensation;
		return 0;
	}

	if (at(0) == '-') {
		if (at(1) == '+')
			return NAG_BlackDecisive;
		if (at(1) == '>')
			return NAG_WithBlackAttack;
		if (at(1) == '/' && at(2) == '+')
			return NAG_BlackClear;
		if (at(1) == '-' && at(2) == '+')
			return NAG_BlackCrushing;
		if (at(1) == '-' && size == 2)
			return NAG_See;
		return 0;
	}

	if (at(0) == '/') {
		if (size == 1)
			return NAG_Diagonal;
		if (at(1) == '\\')
			return NAG_WithIdea;
		return 0;
	}

	if (at(0) == 'R') {
		if (size == 1)
			return NAG_VariousMoves;
		if (at(1) == 'R')
			return NAG_Comment;
		return 0;
	}

	if (at(0) == 'z') {
		if (at(1) == 'z')
			return NAG_BlackZugZwang;
		return 0;
	}

	if (at(0) == 'Z') {
		if (at(1) == 'Z')
			return NAG_ZugZwang;
		return 0;
	}

	if (at(0) == 'B') {
		if (at(1) == 'B')
			return NAG_BishopPair;
		if (at(1) == 'b')
			return NAG_OppositeBishops;
		return 0;
	}

	if (at(0) == 'o') {
		if (at(1) == '-' && at(2) == 'o')
			return NAG_SeparatedPawns;
		if (at(1) == 'o' && size == 2)
			return NAG_UnitedPawns;
		if (at(1) == '^' && size == 2)
			return NAG_PassedPawn;
		return 0;
	}

	if (at(0) == '(') {
		if (at(1) == '_' && at(2) == ')')
			return NAG_BetterIs;
		return 0;
	}

	if (at(0) == '[') {
		if (at(1) == ']' && size == 2)
			return NAG_OnlyMove;
		if (at(1) == '+' && at(2) == ']')
			return NAG_SlightCentre;
		if (at(1) == '+' && at(2) == '+' && at(3) == ']')
			return NAG_Centre;
		return 0;
	}

	if (at(0) == '_') {
		if (at(1) == '|' && at(2) == '_')
			return NAG_Ending;
		if (at(1) == '|' && size == 2)
			return NAG_Without;
		return 0;
	}

	if (at(0) == '|') {
		if (at(1) == '|')
			return NAG_Etc;
		if (at(1) == '_')
			return NAG_With;
		return 0;
	}

	if (at(0) == '>') {
		if (size == 1)
			return NAG_SlightKingSide;
		if (at(1) == '>' && size == 2)
			return NAG_ModerateKingSide;
		if (at(1) == '>' && at(2) == '>')
			return NAG_KingSide;
		return 0;
	}

	if (at(0) == '<') {
		if (size == 1)
			return NAG_SlightQueenSide;
		if (at(1) == '<' && size == 2)
			return NAG_ModerateQueenSide;
		if (at(1) == '<' && at(2) == '<' && size == 3)
			return NAG_QueenSide;
		if (at(1) == '=' && at(2) == '>' && size == 3)
			return NAG_File;
		if (at(1) == '+' && at(2) == '>' && size == 3)
			return NAG_SlightCounterPlay;
		if (at(1) == '-' && at(2) == '>' && size == 3)
			return NAG_BlackSlightCounterPlay;
		if (at(1) == '+' && at(2) == '+' && at(3) == '>' && size == 4)
			return NAG_CounterPlay;
		if (at(1) == '-' && at(2) == '-' && at(3) == '>' && size == 4)
			return NAG_BlackCounterPlay;
		if (at(1) == '+' && at(2) == '+' && at(3) == '+' && at(4) == '>')
			return NAG_DecisiveCounterPlay;
		if (at(1) == '-' && at(2) == '-' && at(3) == '-' && at(4) == '>')
			return NAG_BlackDecisiveCounterPlay;
		return 0;
	}

	if (text == "~=")
		return NAG_Compensation;
	if (text == "~")
		return NAG_Unclear;
	if (text == "x")
		return NAG_WeakPoint;
	if (text == "N")
		return NAG_Novelty;
	if (text == "D")
		return NAG_Diagram;

	return 0;
}

} // namespace scid::core

#include "scidup/core/nags.h"

#include "nagtext.h"

#include <algorithm>
#include <charconv>
#include <iterator>

namespace scid::core {

namespace {

Nag parseUnsigned(std::string_view text) {
	unsigned value = 0;
	const auto* begin = text.data();
	const auto* end = begin + text.size();
	const auto result = std::from_chars(begin, end, value);
	if (result.ec != std::errc{} || result.ptr != end || value > 255)
		return Nag::None;
	return nagFromCode(static_cast<std::uint8_t>(value));
}

} // namespace

std::string_view nagToSymbol(Nag nag) {
	const auto value = nagCode(nag);
	if (value >= std::size(plainNagSymbols))
		return {};
	return plainNagSymbols[value];
}

std::string nagToString(Nag nag, bool asSymbol) {
	if (nag == Nag::None)
		return {};

	if (asSymbol) {
		auto symbol = nagToSymbol(nag);
		if (!symbol.empty())
			return std::string{symbol};
	}

	return "$" + std::to_string(nagCode(nag));
}

Nag nagFromString(std::string_view text) {
	if (text.empty() || text.size() > 7)
		return Nag::None;

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
			return Nag::GoodMove;
		if (at(1) == '!' && size == 2)
			return Nag::ExcellentMove;
		if (at(1) == '?' && size == 2)
			return Nag::InterestingMove;
		return Nag::None;
	}

	if (at(0) == '?') {
		if (size == 1)
			return Nag::PoorMove;
		if (at(1) == '?' && size == 2)
			return Nag::Blunder;
		if (at(1) == '!' && size == 2)
			return Nag::DubiousMove;
		return Nag::None;
	}

	if (at(0) == '+') {
		if (at(1) == '=')
			return Nag::WhiteSlight;
		if (at(1) == '-' && size == 2)
			return Nag::WhiteDecisive;
		if (at(1) == '>')
			return Nag::WithAttack;
		if (at(1) == '/' && at(2) == '-')
			return Nag::WhiteClear;
		if (at(1) == '/' && at(2) == '=')
			return Nag::WhiteSlight;
		if (at(1) == '-' && at(2) == '-')
			return Nag::WhiteCrushing;
		return Nag::None;
	}

	if (at(0) == '=') {
		if (size == 1)
			return Nag::Equal;
		if (at(1) == '+')
			return Nag::BlackSlight;
		if (at(1) == '/' && at(2) == '+')
			return Nag::BlackSlight;
		if (at(1) == '/' && at(2) == '&')
			return Nag::Compensation;
		return Nag::None;
	}

	if (at(0) == '-') {
		if (at(1) == '+')
			return Nag::BlackDecisive;
		if (at(1) == '>')
			return Nag::WithBlackAttack;
		if (at(1) == '/' && at(2) == '+')
			return Nag::BlackClear;
		if (at(1) == '-' && at(2) == '+')
			return Nag::BlackCrushing;
		if (at(1) == '-' && size == 2)
			return Nag::See;
		return Nag::None;
	}

	if (at(0) == '/') {
		if (size == 1)
			return Nag::Diagonal;
		if (at(1) == '\\')
			return Nag::WithIdea;
		return Nag::None;
	}

	if (at(0) == 'R') {
		if (size == 1)
			return Nag::VariousMoves;
		if (at(1) == 'R')
			return Nag::Comment;
		return Nag::None;
	}

	if (at(0) == 'z') {
		if (at(1) == 'z')
			return Nag::BlackZugZwang;
		return Nag::None;
	}

	if (at(0) == 'Z') {
		if (at(1) == 'Z')
			return Nag::ZugZwang;
		return Nag::None;
	}

	if (at(0) == 'B') {
		if (at(1) == 'B')
			return Nag::BishopPair;
		if (at(1) == 'b')
			return Nag::OppositeBishops;
		return Nag::None;
	}

	if (at(0) == 'o') {
		if (at(1) == '-' && at(2) == 'o')
			return Nag::SeparatedPawns;
		if (at(1) == 'o' && size == 2)
			return Nag::UnitedPawns;
		if (at(1) == '^' && size == 2)
			return Nag::PassedPawn;
		return Nag::None;
	}

	if (at(0) == '(') {
		if (at(1) == '_' && at(2) == ')')
			return Nag::BetterIs;
		return Nag::None;
	}

	if (at(0) == '[') {
		if (at(1) == ']' && size == 2)
			return Nag::OnlyMove;
		if (at(1) == '+' && at(2) == ']')
			return Nag::SlightCentre;
		if (at(1) == '+' && at(2) == '+' && at(3) == ']')
			return Nag::Centre;
		return Nag::None;
	}

	if (at(0) == '_') {
		if (at(1) == '|' && at(2) == '_')
			return Nag::Ending;
		if (at(1) == '|' && size == 2)
			return Nag::Without;
		return Nag::None;
	}

	if (at(0) == '|') {
		if (at(1) == '|')
			return Nag::Etc;
		if (at(1) == '_')
			return Nag::With;
		return Nag::None;
	}

	if (at(0) == '>') {
		if (size == 1)
			return Nag::SlightKingSide;
		if (at(1) == '>' && size == 2)
			return Nag::ModerateKingSide;
		if (at(1) == '>' && at(2) == '>')
			return Nag::KingSide;
		return Nag::None;
	}

	if (at(0) == '<') {
		if (size == 1)
			return Nag::SlightQueenSide;
		if (at(1) == '<' && size == 2)
			return Nag::ModerateQueenSide;
		if (at(1) == '<' && at(2) == '<' && size == 3)
			return Nag::QueenSide;
		if (at(1) == '=' && at(2) == '>' && size == 3)
			return Nag::File;
		if (at(1) == '+' && at(2) == '>' && size == 3)
			return Nag::SlightCounterPlay;
		if (at(1) == '-' && at(2) == '>' && size == 3)
			return Nag::BlackSlightCounterPlay;
		if (at(1) == '+' && at(2) == '+' && at(3) == '>' && size == 4)
			return Nag::CounterPlay;
		if (at(1) == '-' && at(2) == '-' && at(3) == '>' && size == 4)
			return Nag::BlackCounterPlay;
		if (at(1) == '+' && at(2) == '+' && at(3) == '+' && at(4) == '>')
			return Nag::DecisiveCounterPlay;
		if (at(1) == '-' && at(2) == '-' && at(3) == '-' && at(4) == '>')
			return Nag::BlackDecisiveCounterPlay;
		return Nag::None;
	}

	if (text == "~=")
		return Nag::Compensation;
	if (text == "~")
		return Nag::Unclear;
	if (text == "x")
		return Nag::WeakPoint;
	if (text == "N")
		return Nag::Novelty;
	if (text == "D")
		return Nag::Diagram;

	return Nag::None;
}

} // namespace scid::core

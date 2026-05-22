#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace scid::core {

enum class Nag : std::uint8_t {
	None = 0,
	GoodMove = 1,
	PoorMove = 2,
	ExcellentMove = 3,
	Blunder = 4,
	InterestingMove = 5,
	DubiousMove = 6,
	OnlyMove = 8,
	Equal = 10,
	Unclear = 13,
	WhiteSlight = 14,
	BlackSlight = 15,
	WhiteClear = 16,
	BlackClear = 17,
	WhiteDecisive = 18,
	BlackDecisive = 19,
	WhiteCrushing = 20,
	BlackCrushing = 21,
	ZugZwang = 22,
	BlackZugZwang = 23,
	MoreRoom = 26,
	DevelopmentAdvantage = 35,
	WithInitiative = 36,
	WithAttack = 40,
	WithBlackAttack = 41,
	Compensation = 44,
	SlightCentre = 48,
	Centre = 50,
	SlightKingSide = 54,
	ModerateKingSide = 56,
	KingSide = 58,
	SlightQueenSide = 60,
	ModerateQueenSide = 62,
	QueenSide = 64,
	SlightCounterPlay = 130,
	BlackSlightCounterPlay = 131,
	CounterPlay = 132,
	BlackCounterPlay = 133,
	DecisiveCounterPlay = 134,
	BlackDecisiveCounterPlay = 135,
	TimeLimit = 136,
	WithIdea = 140,
	BetterIs = 142,
	VariousMoves = 144,
	Comment = 145,
	Novelty = 146,
	WeakPoint = 147,
	Ending = 148,
	File = 149,
	Diagonal = 150,
	BishopPair = 151,
	OppositeBishops = 153,
	SameBishops = 154,
	Etc = 190,
	DoublePawns = 191,
	SeparatedPawns = 192,
	UnitedPawns = 193,
	Diagram = 201,
	See = 210,
	Mate = 211,
	PassedPawn = 212,
	MorePawns = 213,
	With = 214,
	Without = 215
};

constexpr std::uint8_t nagCode(Nag nag) {
	return static_cast<std::uint8_t>(nag);
}

constexpr Nag nagFromCode(std::uint8_t value) {
	return static_cast<Nag>(value);
}

inline constexpr std::uint8_t maxNagCode = nagCode(Nag::Without);

std::string nagToString(Nag nag, bool asSymbol);
std::string_view nagToSymbol(Nag nag);
Nag nagFromString(std::string_view text);

} // namespace scid::core

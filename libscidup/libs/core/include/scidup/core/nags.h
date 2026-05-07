#pragma once

#include <cstdint>

namespace scid::core {

// Common NAG annotation symbol values.
// TODO [Game]: Replace these loose constants with a scoped Nag enum once the
// Move metadata shape is settled.
const std::uint8_t
    NAG_GoodMove = 1,
    NAG_PoorMove = 2,
    NAG_ExcellentMove = 3,
    NAG_Blunder = 4,
    NAG_InterestingMove = 5,
    NAG_DubiousMove = 6,
    NAG_OnlyMove = 8,
    NAG_Equal = 10,
    NAG_Unclear = 13,
    NAG_WhiteSlight = 14,
    NAG_BlackSlight = 15,
    NAG_WhiteClear = 16,
    NAG_BlackClear = 17,
    NAG_WhiteDecisive = 18,
    NAG_BlackDecisive = 19,
    NAG_WhiteCrushing = 20,
    NAG_BlackCrushing = 21,
    NAG_ZugZwang = 22,
    NAG_BlackZugZwang = 23,
    NAG_MoreRoom = 26,
    NAG_DevelopmentAdvantage = 35,
    NAG_WithInitiative = 36,
    NAG_WithAttack = 40,
    NAG_WithBlackAttack = 41,
    NAG_Compensation = 44,
    NAG_SlightCentre = 48,
    NAG_Centre = 50,
    NAG_SlightKingSide = 54,
    NAG_ModerateKingSide = 56,
    NAG_KingSide = 58,
    NAG_SlightQueenSide = 60,
    NAG_ModerateQueenSide = 62,
    NAG_QueenSide = 64,
    NAG_SlightCounterPlay = 130,
    NAG_CounterPlay = 132,
    NAG_DecisiveCounterPlay = 134,
    NAG_BlackSlightCounterPlay = 131,
    NAG_BlackCounterPlay = 133,
    NAG_BlackDecisiveCounterPlay = 135,
    NAG_TimeLimit = 136,
    NAG_WithIdea = 140,
    NAG_BetterIs = 142,
    NAG_VariousMoves = 144,
    NAG_Comment = 145,
    NAG_Novelty = 146,
    NAG_WeakPoint = 147,
    NAG_Ending = 148,
    NAG_File = 149,
    NAG_Diagonal = 150,
    NAG_BishopPair = 151,
    NAG_OppositeBishops = 153,
    NAG_SameBishops = 154,
    NAG_Etc = 190,
    NAG_DoublePawns = 191,
    NAG_SeparatedPawns = 192,
    NAG_UnitedPawns = 193,
    NAG_Diagram = 201,
    NAG_See = 210,
    NAG_Mate = 211,
    NAG_PassedPawn = 212,
    NAG_MorePawns = 213,
    NAG_With = 214,
    NAG_Without = 215;

const std::uint8_t MAX_NAGS_ARRAY = 215;

} // namespace scid::core

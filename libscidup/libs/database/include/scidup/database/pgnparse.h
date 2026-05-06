/*
 * Copyright (C) 2018  Fulvio Benini
 *
 * This file is part of SCID (Shane's Chess Information Database).
 *
 * SCID is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * SCID is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SCID. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/** @file
 * Implements a parser that converts PGN text into SCID's Game objects.
 */

#ifndef SCID_PGNPARSE_H
#define SCID_PGNPARSE_H

#include "scidup/database/game.h"
#include <cstddef>
#include <string>

namespace scid::database {

/**
 * Format and store errors.
 */
struct PgnParseLog {
	std::string log;
	unsigned long long n_bytes = 0;
	unsigned long long n_lines = 0;
	unsigned long long n_games = 0;
};

/**
 * Convert PGN text into a SCID's Game object.
 * @param input:    the memory containing the PGN text.
 * @param inputLen: the number of chars in @e input.
 * @param game:     the Game object where the game will be stored.
 *                  The object is not automatically cleared so that moves can
 *                  be added to an already existing one.
 * @param log:      stores eventual parsing error.
 * @returns true if a game was parsed successfully (maybe with errors, but
 * without ignoring any part), false otherwise.
 */
bool pgnParseGame(const char* input, size_t inputLen, Game& game,
                  PgnParseLog& log);

} // namespace scid::database
#endif // idndef SCID_PGNPARSE_H

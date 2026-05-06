/*
* Copyright (C) 2013-2018  Fulvio Benini

* This file is part of Scid (Shane's Chess Information Database).
*
* Scid is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation.
*
* Scid is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Scid.  If not, see <http://www.gnu.org/licenses/>.
*/

/** @file
 * Defines the classes used to search for positions.
 */

#ifndef SEARCHPOS_H
#define SEARCHPOS_H

#include "scidup/core/fullmove.h"
#include "scidup/database/common.h"
#include "scidup/database/matsig.h"
#include "scidup/core/position.h"
#include "scidup/database/scidbase.h"
#include <algorithm>
#include <memory>

namespace scid::database {

class StoredLine;

/// Search for an exact position (same material in the same squares).
class SearchPos {
	matSigT materialSig_;
	pieceT board_[64];
	std::unique_ptr<StoredLine> storedLine_;
	std::pair<uint16_t, uint16_t> hpSig_;
	colorT toMove_;
	bool isStdStard_;

public:
	explicit SearchPos(Position const& pos);
	~SearchPos();

	/// Disable the stored lines optimization
	void disableOptStoredLine();

	/// Disable the home pawn optimization
	void disableOptHpSig() { hpSig_ = {0, 0}; }

	/// Search for the position using the optimizations in a game's index.
	/// @returns
	/// -2 : the game cannot reach the searched position
	/// -1 : the game can reach the searched position
	/// >=0: the game reach the searched position at the returned ply
	int index_match(const IndexEntry& ie) const;

	/// Search the position in the main line of the specified game.
	/// @returns a std::pair containg the ply where the position was reached and
	///          the next move. Returns ply==0 if the position was not found.
	/// TODO: filling the SAN info of the returned move may be unnecessary
	std::pair<int, FullMove> match(scidBaseT const& base, gamenumT gnum) const;

	/// Reset @e filter to include only the games that reached the searched
	/// position in their main line.
	bool setFilter(scidBaseT const& base, HFilter& filter,
	               const Progress& progress) const {
		if (toMove_ == BLACK)
			return SetFilter<BLACK>(base, filter, progress);

		if (!isStdStard_)
			return SetFilter<WHITE>(base, filter, progress);

		return setFilterStdStart(base, filter);
	}

private:
	bool setFilterStdStart(scidBaseT const& base, HFilter& filter) const {
		filter->includeAll();
		for (gamenumT i = 0, n = base.numGames(); i < n; i++) {
			const IndexEntry* ie = base.getIndexEntry(i);
			if (ie->GetStartFlag()) {
				int ply = base.getGame(ie).search<WHITE>(board_);
				filter.set(i, (ply > 255) ? 255 : ply);
			}
		}
		return true;
	}

	template <colorT TOMOVE>
	bool SetFilter(scidBaseT const& base, HFilter& filter,
	               const Progress& prg) const {
		filter->clear();
		long long progress = 0;
		for (gamenumT i = 0, n = base.numGames(); i < n; i++) {
			const IndexEntry* ie = base.getIndexEntry(i);
			int ply = index_match(*ie);
			if (ply >= 0) {
				filter.set(i, static_cast<byte>(ply + 1));
			} else if (ply == -1) {
				ply = base.getGame(ie).search<TOMOVE>(board_);
				if (ply != 0)
					filter.set(i, (ply > 255) ? 255 : ply);
			}
			if (progress++ % 512 == 0 && !prg.report(i, n))
				return false;
		}
		return true;
	}

	/// Return true if any searched material count is below its final-game
	/// counterpart.
	static bool less_mat(matSigT a, matSigT b, bool promo, bool upromo) {
		int wp_diff = static_cast<int>(MATSIG_Count_WP(a)) -
		              static_cast<int>(MATSIG_Count_WP(b));
		int bp_diff = static_cast<int>(MATSIG_Count_BP(a)) -
		              static_cast<int>(MATSIG_Count_BP(b));
		if (wp_diff < 0 || bp_diff < 0)
			return true;

		int wq_diff = static_cast<int>(MATSIG_Count_WQ(a)) -
		              static_cast<int>(MATSIG_Count_WQ(b));
		int bq_diff = static_cast<int>(MATSIG_Count_BQ(a)) -
		              static_cast<int>(MATSIG_Count_BQ(b));
		if (promo) {
			wq_diff += wp_diff;
			bq_diff += bp_diff;
		}
		if (wq_diff < 0 || bq_diff < 0)
			return true;

		if (upromo)
			return false;

		return MATSIG_Count_WR(a) < MATSIG_Count_WR(b) ||
		       MATSIG_Count_WB(a) < MATSIG_Count_WB(b) ||
		       MATSIG_Count_WN(a) < MATSIG_Count_WN(b) ||
		       MATSIG_Count_BR(a) < MATSIG_Count_BR(b) ||
		       MATSIG_Count_BB(a) < MATSIG_Count_BB(b) ||
		       MATSIG_Count_BN(a) < MATSIG_Count_BN(b);
	}
};


} // namespace scid::database
#endif

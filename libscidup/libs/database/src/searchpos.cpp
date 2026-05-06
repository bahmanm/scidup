#include "scidup/database/searchpos.h"

#include "stored.h"

namespace scid::database {

SearchPos::SearchPos(Position const& pos) {
	std::copy_n(pos.GetBoard(), 64, board_);
	materialSig_ = matsig_Make(pos.GetMaterial());
	hpSig_ = hpSig_make(board_);
	toMove_ = pos.GetToMove();
	isStdStard_ = pos.IsStdStart();

	if ((board_[E1] == WK || board_[G1] == WK) &&
	    (board_[E8] == BK || board_[G8] == BK)) {
		storedLine_ = std::make_unique<StoredLine>(board_, toMove_);
	}
}

SearchPos::~SearchPos() = default;

void SearchPos::disableOptStoredLine() {
	storedLine_ = nullptr;
}

int SearchPos::index_match(const IndexEntry& ie) const {
	if (!ie.GetStartFlag()) {
		if (storedLine_) {
			int ply = storedLine_->match(ie.GetStoredLineCode());
			if (ply != -1)
				return ply;
		}
		if (!hpSig_match(hpSig_.first, hpSig_.second, ie.GetHomePawnData()))
			return -2;
	}
	if (less_mat(materialSig_, ie.GetFinalMatSig(), ie.GetPromotionsFlag(),
	             ie.GetUnderPromoFlag())) {
		return -2;
	}
	return -1;
}

std::pair<int, FullMove> SearchPos::match(scidBaseT const& base,
                                          gamenumT gnum) const {
	const IndexEntry* ie = base.getIndexEntry(gnum);
	int ply = index_match(*ie);
	if (ply < -1)
		return {};

	if (ply >= 0) {
		auto move = StoredLine::getMove(ie->GetStoredLineCode(), ply);
		if (!move)
			move = base.getGame(ie).getMove(ply);

		return {ply + 1, move};
	}

	auto gameview = base.getGame(ie);
	ply = (toMove_ == WHITE) ? gameview.search<WHITE>(board_)
	                         : gameview.search<BLACK>(board_);
	if (ply > 0)
		return {ply, gameview.getMove(0)};

	return {};
}

} // namespace scid::database

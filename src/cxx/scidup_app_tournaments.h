#ifndef SCIDUP_APP_TOURNAMENTS_H
#define SCIDUP_APP_TOURNAMENTS_H

#include "scidup/core/game_result.h"
#include "scidup/database/game_id.h"
#include "scidup/database/misc.h"
#include "scidup/database/scidbase.h"
#include <algorithm>
#include <numeric>
#include <vector>

namespace scidup::app::tournaments {

namespace db = scid::database;

struct TourneyGame {
	db::idNumberT siteID_;
	db::idNumberT eventID_;
	db::dateT eventDate_;
	db::idNumberT whiteID_;
	db::idNumberT blackID_;
	db::ratingT wElo_;
	db::ratingT bElo_;
	db::dateT date_;
	db::gamenumT gnum_;
	db::resultT result_;

	TourneyGame(const db::IndexEntry* ie, db::gamenumT gnum) {
		siteID_ = ie->GetSite();
		eventID_ = ie->GetEvent();
		eventDate_ = ie->GetEventDate();
		whiteID_ = ie->GetWhite();
		blackID_ = ie->GetBlack();
		wElo_ = ie->GetWhiteElo();
		bElo_ = ie->GetBlackElo();
		date_ = ie->GetDate();
		gnum_ = gnum;
		result_ = ie->GetResult();
	}
};

class Tourney {
public:
	template <typename Iter>
	Tourney(Iter begin, Iter end)
	    : begin_(begin), minDateGame_(begin) {
		ASSERT(begin != end);

		n_games_ = static_cast<db::gamenumT>(std::distance(begin, end));

		for (auto it = begin; it != end; it++) {
			auto& white = add_player(it->whiteID_, it->wElo_);
			white.score += db::RESULT_SCORE[it->result_];
			auto& black = add_player(it->blackID_, it->bElo_);
			black.score += db::RESULT_SCORE[db::RESULT_OPPOSITE[it->result_]];

			if (it->date_ < minDateGame_->date_) minDateGame_ = it;
		}
		std::sort(players_.begin(), players_.end(),
		          [](auto& a, auto& b) { return a.score > b.score; });

		const auto eloSum = std::accumulate(players_.begin(), players_.end(),
		                                    std::make_pair(0ull, db::gamenumT{0}),
		                                    [](auto res, const auto& player) {
			                                    if (player.elo != 0) {
				                                    res.first += player.elo;
				                                    ++res.second;
			                                    }
			                                    return res;
		                                    });
		avgElo_ = (eloSum.second == 0)
		              ? 0
		              : static_cast<unsigned>(eloSum.first / eloSum.second);
	}

	db::idNumberT getEventId() const { return begin_->eventID_; }
	db::idNumberT getSiteId() const { return begin_->siteID_; }
	db::dateT getStartDate() const { return minDateGame_->date_; }
	db::gamenumT getStartGameNum() const { return minDateGame_->gnum_; }
	unsigned getAvgElo() const { return avgElo_; }
	db::gamenumT nGames() const { return n_games_; }
	unsigned nPlayers() const { return static_cast<unsigned>(players_.size()); }

	struct Player {
		db::idNumberT nameId;
		uint16_t score;
		db::ratingT elo;

		bool operator==(db::idNumberT id) const { return nameId == id; }
	};
	const Player& getPlayer(size_t position) const {
		ASSERT(position < players_.size());
		return players_[position];
	}

private:
	std::vector<TourneyGame>::const_iterator begin_;
	std::vector<TourneyGame>::const_iterator minDateGame_;
	std::vector<Player> players_;
	db::gamenumT n_games_;
	unsigned avgElo_;

	Player& add_player(db::idNumberT nameID, db::ratingT elo) {
		auto it = std::find(players_.begin(), players_.end(), nameID);
		if (it != players_.end()) {
			if (elo > it->elo)
				it->elo = elo;
			return *it;
		}
		players_.push_back({nameID, 0, elo});
		return players_.back();
	};
};

class SearchTournaments {
	const db::scidBaseT* dbase_;
	std::vector<TourneyGame> games_;
	std::vector<Tourney> tourney_;

public:
	SearchTournaments(const db::scidBaseT* dbase, const db::HFilter& filter)
	    : dbase_(dbase) {
		ASSERT(dbase != 0);
		ASSERT(filter != 0);

		games_.reserve(filter->size());
		for (auto gnum : filter) {
			games_.emplace_back(dbase->getIndexEntry(gnum), gnum);
		}

		std::sort(games_.begin(), games_.end(),
		          [](const TourneyGame& a, const TourneyGame& b) {
			          if (a.eventID_ != b.eventID_)
				          return a.eventID_ < b.eventID_;
			          if (a.siteID_ != b.siteID_)
				          return a.siteID_ < b.siteID_;
			          db::dateT d1 = a.eventDate_ != 0 ? a.eventDate_ : a.date_;
			          db::dateT d2 = b.eventDate_ != 0 ? b.eventDate_ : b.date_;
			          return d1 < d2;
		          });

		auto it = games_.begin();
		const auto it_end = games_.end();
		while (it != it_end) {
			const auto start = it;
			it = std::find_if(it, it_end, [start](const TourneyGame& g) {
				if (start->eventID_ != g.eventID_ ||
				    start->siteID_ != g.siteID_)
					return true;

				if (start->eventDate_ != 0 && g.eventDate_ == 0)
					return start->eventDate_ > g.date_;

				if (start->eventDate_ == 0 && g.eventDate_ != 0)
					return g.eventDate_ > start->date_;

				return start->eventDate_ != g.eventDate_;
			});
			tourney_.emplace_back(start, it);
		}
	}

	using Iter = std::vector<Tourney>::const_iterator;
	Iter begin() const { return tourney_.begin(); }
	Iter end() const { return tourney_.end(); }

	void filterByAvgElo(const db::StrRange& range) {
		tourney_.erase(
			std::remove_if(tourney_.begin(), tourney_.end(),
				Filter<&Tourney::getAvgElo>(range)),
			tourney_.end());
	}

	void filterByNPlayers(const db::StrRange& range) {
		tourney_.erase(
			std::remove_if(tourney_.begin(), tourney_.end(),
				Filter<&Tourney::nPlayers>(range)),
			tourney_.end());
	}

	void filterByNGames(const db::StrRange& range) {
		tourney_.erase(
			std::remove_if(tourney_.begin(), tourney_.end(),
				Filter<&Tourney::nGames>(range)),
			tourney_.end());
	}

	void filterByPlayer(const char* name) {
		tourney_.erase(
			std::remove_if(tourney_.begin(), tourney_.end(),
				FilterByPlayer(name, dbase_->getNameBase())),
			tourney_.end());
	}

	bool sort(const char* criteria, size_t max);

private:
	template <db::uint (Tourney::* f)() const>
	class Filter {
		const db::StrRange& range_;

	public:
		Filter(const db::StrRange& range) : range_(range) {}

		bool operator()(const Tourney& t) {
			return !range_.inRange((t.*f)());
		}
	};

	class FilterByPlayer {
		const char* name_;
		const db::NameBase* nb_;

	public:
		FilterByPlayer(const char* name, const db::NameBase* nb)
		: name_(name), nb_(nb) {}

		bool operator()(const Tourney& t) {
			for (size_t i = 0, n = t.nPlayers(); i < n; i++) {
				const char* name = nb_->GetName(db::NAME_PLAYER, t.getPlayer(i).nameId);
				if (db::strAlphaContains(name, name_)) return false;
			}
			return true;
		}
	};

	struct SortDate {
		bool operator()(const Tourney& a, const Tourney& b) {
			return a.getStartDate() > b.getStartDate();
		}
	};

	template <db::uint (Tourney::* f)() const>
	struct SortDesc {
		bool operator()(const Tourney& a, const Tourney& b) {
			return (a.*f)() > (b.*f)();
		}
	};

	template <db::nameT nt, db::idNumberT (Tourney::* f)() const>
	class SortId {
		const db::NameBase* nb_;
	public:
		SortId(const db::NameBase* nb) : nb_(nb) {}
		bool operator()(const Tourney& a, const Tourney& b) {
			const char* nameA = nb_->GetName(nt, (a.*f)());
			const char* nameB = nb_->GetName(nt, (b.*f)());
			return db::strCaseCompare(nameA, nameB) < 0;
		}
	};
};

inline bool SearchTournaments::sort(const char* criteria, size_t nOrdered) {
	static const char* criterions [] = {
		 "Date", "Elo", "Event", "Games", "Players", "Site", NULL
	};
	enum { DATE, ELO, EVENT, GAMES, PLAYERS, SITE };

	std::vector<Tourney>::iterator begin = tourney_.begin();
	std::vector<Tourney>::iterator it = (nOrdered < tourney_.size()) ?
		tourney_.begin() + nOrdered : tourney_.end();
	std::vector<Tourney>::iterator end = tourney_.end();

	switch (db::strUniqueMatch(criteria, criterions)) {
	case DATE:
		std::partial_sort(begin, it, end, SortDate());
		break;
	case ELO:
		std::partial_sort(begin, it, end, SortDesc<&Tourney::getAvgElo>());
		break;
	case EVENT:
		std::partial_sort(begin, it, end,
			SortId<db::NAME_EVENT, &Tourney::getEventId>(dbase_->getNameBase()));
		break;
	case GAMES:
		std::partial_sort(begin, it, end, SortDesc<&Tourney::nGames>());
		break;
	case PLAYERS:
		std::partial_sort(begin, it, end, SortDesc<&Tourney::nPlayers>());
		break;
	case SITE:
		std::partial_sort(begin, it, end,
			SortId<db::NAME_SITE, &Tourney::getSiteId>(dbase_->getNameBase()));
		break;
	default:
		return false;
	}

	return true;
}

} // namespace scidup::app::tournaments

#endif

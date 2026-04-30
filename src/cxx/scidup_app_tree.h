#ifndef SCIDUP_APP_TREE_H
#define SCIDUP_APP_TREE_H

#include "scidbase.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace scidup::app::tree {

struct State {
	std::unique_ptr<Filter> filter = std::make_unique<Filter>(0);
	TreeCache cache;
	uint64_t cacheToken = 0;

	void reset(scidBaseT const& base) {
		filter->Init(base.numGames());
		cache.CacheResize(250);
		cacheToken = base.cacheInvalidationToken();
	}

	void sync(scidBaseT const& base) {
		if (filter->Size() != base.numGames()) {
			filter->Resize(base.numGames());
		}
		if (cacheToken != base.cacheInvalidationToken()) {
			cache.Clear();
			cacheToken = base.cacheInvalidationToken();
		}
	}
};

namespace detail {
inline auto& states() {
	static std::unordered_map<scidBaseT const*, State> value;
	return value;
}

inline State& stateFor(scidBaseT const& base) { return states()[&base]; }

inline HFilter resolveSingleFilter(scidBaseT const& base, std::string_view id) {
	if (id == "tree") {
		auto& state = stateFor(base);
		state.sync(base);
		return HFilter(state.filter.get());
	}
	return base.getFilter(id);
}
} // namespace detail

inline void reset(scidBaseT& base) { detail::stateFor(base).reset(base); }

inline void release(scidBaseT& base) { detail::states().erase(&base); }

class Session {
public:
	explicit Session(scidBaseT& base) : base_(&base) {}

	HFilter filter() const {
		auto& s = state();
		s.sync(*base_);
		return HFilter(s.filter.get());
	}

	TreeCache& cache() const {
		auto& s = state();
		s.sync(*base_);
		return s.cache;
	}

	bool cacheRestore(Position const& pos) const {
		auto& s = state();
		s.sync(*base_);
		return s.cache.cacheRestore(pos, *s.filter);
	}

	void cacheAdd(Position const& pos) const {
		auto& s = state();
		s.sync(*base_);
		s.cache.cacheAdd(pos, *s.filter);
	}

	void cacheResize(size_t maxSize) const { state().cache.CacheResize(maxSize); }
	size_t cacheSize() const { return state().cache.Size(); }

private:
	State& state() const { return detail::stateFor(*base_); }
	scidBaseT* base_;
};

inline Session session(scidBaseT& base) { return Session(base); }

inline HFilter resolveFilter(scidBaseT const& base, std::string_view filterId) {
	if (filterId.empty() || filterId[0] != '+') {
		return detail::resolveSingleFilter(base, filterId);
	}

	size_t maskName = filterId.find('+', 1);
	if (maskName == std::string::npos) {
		return HFilter(nullptr);
	}

	auto main = detail::resolveSingleFilter(base, filterId.substr(1, maskName - 1));
	auto mask = detail::resolveSingleFilter(base, filterId.substr(maskName + 1));
	if (main == nullptr || mask == nullptr) {
		return HFilter(nullptr);
	}
	return HFilter(main.mainFilter(), mask.mainFilter());
}

inline std::pair<std::string, std::string>
getFilterComponents(scidBaseT const& base, std::string_view filterId) {
	if (filterId.empty()) {
		return {};
	}
	if (filterId[0] != '+') {
		return (resolveFilter(base, filterId) != nullptr)
		           ? std::make_pair(std::string(filterId), std::string())
		           : std::make_pair(std::string(), std::string());
	}

	size_t maskName = filterId.find('+', 1);
	if (maskName == std::string::npos) {
		return {};
	}
	auto main = filterId.substr(1, maskName - 1);
	auto mask = filterId.substr(maskName + 1);
	if (resolveFilter(base, main) == nullptr || resolveFilter(base, mask) == nullptr) {
		return {};
	}
	return {std::string(main), std::string(mask)};
}

inline std::string composeFilter(scidBaseT const& base, std::string_view mainFilter,
                                 std::string_view maskFilter) {
	std::string res;
	if (mainFilter.empty()) {
		return res;
	}

	auto filters = getFilterComponents(base, mainFilter);
	if (!filters.first.empty()) {
		res = filters.first;
	}

	if (!maskFilter.empty()) {
		res = '+' + res + "+";
		res.append(maskFilter);
	}

	if (resolveFilter(base, res) == nullptr) {
		res.clear();
	}
	return res;
}

} // namespace scidup::app::tree

#endif

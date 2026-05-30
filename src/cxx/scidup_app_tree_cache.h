#pragma once

#include "scid/database/game_id.h"
#include "scid/database/hfilter.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace scidup::app::tree {
namespace detail {

class CompressedFilter {
  std::unique_ptr<scid::core::byte[]> CompressedData;
  scid::database::gamenumT CFilterSize = 0;
  scid::database::gamenumT CompressedLength = 0;

public:
  CompressedFilter() = default;
  CompressedFilter(CompressedFilter &&) noexcept = default;
  CompressedFilter &operator=(CompressedFilter &&) noexcept = default;
  CompressedFilter(const CompressedFilter &) = delete;
  CompressedFilter &operator=(const CompressedFilter &) = delete;
  ~CompressedFilter() = default;

  void CompressFrom(scid::database::Filter *filter);
  scid::core::errorT UncompressTo(scid::database::Filter *filter) const;

private:
  scid::core::errorT Verify(scid::database::Filter *filter);
};

struct CachedFilter {
  CompressedFilter cfilter_;
  scid::core::pieceT board_[64];
  scid::core::colorT toMove_;
};

} // namespace detail

class TreeCache {
  std::vector<detail::CachedFilter> cache_;
  std::vector<uint32_t> cacheTime_;
  uint32_t cacheTimeCounter_ = 0;

public:
  void Clear() {
    cache_.clear();
    cacheTime_.clear();
  }

  size_t Size() const { return cache_.capacity(); }

  void CacheResize(size_t max_size) {
    Clear();
    cache_.reserve(max_size);
    cacheTime_.reserve(max_size);
  }

  template <typename PosT>
  void cacheAdd(PosT const &pos, scid::database::Filter &filter) {
    size_t idx;
    if (cache_.size() < Size() || cache_.empty()) {
      idx = cache_.size();
      cache_.emplace_back();
      cacheTime_.emplace_back();
    } else {
      auto it = std::min_element(cacheTime_.begin(), cacheTime_.end());
      idx = std::distance(cacheTime_.begin(), it);
    }
    auto board = pos.GetBoard();
    std::copy(board, board + 64, cache_[idx].board_);
    cache_[idx].toMove_ = pos.GetToMove();
    cache_[idx].cfilter_.CompressFrom(&filter);
    cacheTime_[idx] = cacheTimeCounter_++;
  }

  template <typename PosT>
  bool cacheRestore(PosT const &pos, scid::database::Filter &filter) {
    auto it = std::find_if(cache_.begin(), cache_.end(), [&pos](auto const &e) {
      return e.toMove_ == pos.GetToMove() &&
             std::equal(e.board_, e.board_ + 64, pos.GetBoard());
    });
    if (it == cache_.end())
      return false;

    auto idx = std::distance(cache_.begin(), it);
    if (it->cfilter_.UncompressTo(&filter) != scid::core::OK) {
      ASSERT(false); // corrupted data: should not happen
      return false;
    }
    cacheTime_[idx] = cacheTimeCounter_++;
    return true;
  }
};

} // namespace scidup::app::tree

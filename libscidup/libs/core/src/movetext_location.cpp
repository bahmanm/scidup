#include "scidup/core/movetext_location.h"

#include <utility>

namespace scid::core {

MovetextLocation::MovetextLocation(std::vector<Step> path,
                                   std::size_t nextIndex)
    : path_(std::move(path)), nextIndex_(nextIndex) {}

} // namespace scid::core

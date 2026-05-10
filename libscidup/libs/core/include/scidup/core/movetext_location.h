#pragma once

#include <cstddef>
#include <vector>

namespace scid::core {

class GameCursor;
class MovetextCursor;

class MovetextLocation {
public:
	MovetextLocation() = default;

	bool operator==(const MovetextLocation&) const = default;

private:
	struct Step {
		bool operator==(const Step&) const = default;

		std::size_t nextIndex = 0;
		std::size_t variationIndex = 0;
	};

	MovetextLocation(std::vector<Step> path, std::size_t nextIndex);

	std::vector<Step> path_;
	std::size_t nextIndex_ = 0;

	friend class GameCursor;
	friend class MovetextCursor;
};

} // namespace scid::core

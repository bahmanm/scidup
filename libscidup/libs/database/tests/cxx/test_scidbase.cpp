/*
* Copyright (C) 2016 Fulvio Benini

* Scid is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation.
*
* Scid is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Scid. If not, see <http://www.gnu.org/licenses/>.
*/

#include "scidup/database/scidbase.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

TEST(ScidbaseTest, NewComposeDeleteGetFilter) {
  scid::database::scidBaseT dbase;
  ASSERT_EQ(scid::core::OK,
            dbase.open("MEMORY", scid::database::FMODE_Create, "Memory"));

  std::vector<std::pair<std::string, bool>> tests = {
      {" dbfilter", false},   {"dbfilter ", false}, {"dbfilter", true},
      {"tree", false},        {"+dbfilter", false}, {"++dbfilter", false},
      {"++dbfilter+", false}, {"", false},          {"+", false},
      {"++", false},          {"+++", false},       {" +", false},
      {"+ ", false}};

  auto check = [&]() {
    std::string mask;
    for (const auto &e : tests) {
      bool valid = dbase.getFilter(e.first) != 0;
      EXPECT_EQ(e.second, valid);

      auto composed = dbase.composeFilter(e.first, mask);
      EXPECT_EQ(valid, dbase.getFilter(composed) != nullptr);
      if (valid) {
        auto [f1, f2] = dbase.getFilterComponents(composed);
        EXPECT_EQ(f1, e.first);
        EXPECT_EQ(f2, mask);
        EXPECT_EQ(e.first, dbase.composeFilter(composed, ""));

        mask = e.first;
      }
    }
  };

  check();

  for (size_t i = 0; i < 100; i++) {
    tests.emplace_back(dbase.newFilter(), true);
    auto unique = std::find(tests.begin(), tests.end(), tests.back());
    EXPECT_EQ(tests.end(), ++unique);
    check();
  }

  for (size_t i = 0, n = tests.size(); i < n; i += 4) {
    tests[i].second = false;
    dbase.deleteFilter(tests[i].first.c_str());
    check();
  }
}

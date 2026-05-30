#include "scidup/core/game_positions.h"
#include "scidup/core/pgn/decode.h"
#include "scidup/core/primitives.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

namespace {

template <typename TCont> std::string encodePgn(const TCont &game) {
  std::string res;

  struct {
    const char sep = ' ';
    const char endl = '\n';
    const char RAVstart = '(';
    const char RAVend = ')';
    const char commentStart = '{';
    const char commentEnd = '}';
    const char *MoveNumEndW = ".";
    const char *MoveNumEndB = "...";
  } token;

  const size_t lineLen = 80;
  auto formatLine = [&](std::string &src, std::string &dest) {
    size_t skip_start = src.find_first_not_of(token.sep);
    src.erase(0, skip_start);
    while (src.size() > lineLen) {
      size_t line_break = src.rfind(token.sep, lineLen);
      if (line_break == std::string::npos) {
        line_break = src.find(token.sep, lineLen);
        if (line_break == std::string::npos)
          break;
      }

      dest.append(src.begin(), src.begin() + line_break);
      dest += token.endl;
      src.erase(0, line_break + 1);
    }
  };

  auto FEN_getColor = [](const std::string &FEN) {
    scid::core::colorT res = scid::core::NOCOLOR;
    size_t toMove = FEN.find(' ');
    if (toMove != std::string::npos && ++toMove < FEN.size()) {
      if (FEN[toMove] == 'w')
        res = scid::core::WHITE;
      else if (FEN[toMove] == 'b')
        res = scid::core::BLACK;
    }
    return res;
  };
  auto FEN_getMoveNum = [](const std::string &FEN) {
    std::string res;
    size_t moveNum = FEN.rfind(' ');
    if (moveNum != std::string::npos && ++moveNum < FEN.size())
      res = FEN.substr(moveNum, std::string::npos);
    return res;
  };

  std::vector<scid::core::uint> RAVid = {0};
  auto handleRAV = [&](scid::core::uint RAVdepth,
                       const scid::core::uint &RAVnum) {
    std::pair<scid::core::uint, scid::core::uint> res = {0, 0};
    if (++RAVdepth > RAVid.size()) {
      RAVid.push_back(RAVnum);
      assert(RAVdepth == RAVid.size());
      res.first++;
    } else {
      while (RAVdepth < RAVid.size()) {
        RAVid.pop_back();
        res.second++;
      }

      if (RAVnum != RAVid.back()) {
        RAVid.back() = RAVnum;
        res.first++;
        res.second++;
      }
    }
    return res;
  };

  bool forceMoveNum = false;
  auto needMoveNum = [&forceMoveNum](scid::core::colorT lastCol) {
    bool res = (lastCol == scid::core::BLACK || forceMoveNum);
    forceMoveNum = false;
    return res;
  };

  std::string line;
  for (auto &pos : game) {
    auto ravs = handleRAV(pos.RAVdepth, pos.RAVnum);
    for (scid::core::uint i = 0; i < ravs.second; i++) {
      line += token.sep;
      line += token.RAVend;
      forceMoveNum = true;
    }
    for (scid::core::uint i = 0; i < ravs.first; i++) {
      line += token.sep;
      line += token.RAVstart;
      forceMoveNum = true;
    }

    if (pos.lastMoveSAN == "") {
      if (pos.FEN !=
          "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
        if (!res.empty() || !line.empty())
          return "error";
        res += "[FEN \"";
        res += pos.FEN;
        res += '\"';
        res += token.endl;
        res += token.endl;
      }
    } else {
      scid::core::colorT lastCol = FEN_getColor(pos.FEN);
      assert(lastCol != scid::core::NOCOLOR);
      bool printMoveNum = needMoveNum(lastCol);
      if (printMoveNum) {
        std::string num = FEN_getMoveNum(pos.FEN);
        const char *endMoveNum = token.MoveNumEndW;
        if (lastCol == scid::core::WHITE) {
          endMoveNum = token.MoveNumEndB;
          int temp = atoi(num.c_str());
          num = std::to_string(--temp);
        }
        line += token.sep;
        line += num;
        line += endMoveNum;
      }

      line += token.sep;
      line += pos.lastMoveSAN;

      for (auto &e : pos.NAGs) {
        line += token.sep;
        line += '$';
        line += std::to_string(e);
        forceMoveNum = true;
      }
    }

    if (!pos.comment.empty()) {
      line += token.sep;
      line += token.commentStart;
      size_t prevLen = line.size();
      line += pos.comment;

      std::string::iterator strip_begin = line.begin() + prevLen;
      line.erase(std::remove(strip_begin, line.end(), token.endl), line.end());
      line.erase(std::remove(strip_begin, line.end(), '\t'), line.end());
      line.erase(std::remove(strip_begin, line.end(), '\v'), line.end());
      line.erase(std::remove(strip_begin, line.end(), token.commentEnd),
                 line.end());

      line += token.commentEnd;
      forceMoveNum = true;
    }

    formatLine(line, res);
  }
  auto ravs = handleRAV(0, 0);
  for (scid::core::uint i = 0; i < ravs.second; i++) {
    line += token.sep;
    line += token.RAVend;
  }

  formatLine(line, res);
  res.append(line.begin(), line.end());

  return res;
}

scid::core::gamepos::GamePos makeGamePos(scid::core::uint RAVdepth,
                                         scid::core::uint RAVnum,
                                         const char *FEN, const char *SAN) {
  scid::core::gamepos::GamePos res;
  res.RAVdepth = RAVdepth;
  res.RAVnum = RAVnum;
  res.FEN = FEN;
  res.lastMoveSAN = SAN;
  return res;
}

const std::string test_pgnShort =
    "1. d4 d5 2. c4 ( 2. Nf3 Nf6 ( 2... Bg4 ) 3. c3 ) "
    "( 2. g3 Nf6 3. Bg2 ( 3. Nf3 ) )";

std::vector<scid::core::gamepos::GamePos> expectedGamePositions() {
  return {
      makeGamePos(
          0, 0, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", ""),
      makeGamePos(0, 0,
                  "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1",
                  "d4"),
      makeGamePos(
          0, 0, "rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2",
          "d5"),
      makeGamePos(
          0, 0, "rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq - 0 2",
          "c4"),
      makeGamePos(
          1, 0,
          "rnbqkbnr/ppp1pppp/8/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R b KQkq - 1 2",
          "Nf3"),
      makeGamePos(
          1, 0,
          "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 2 3",
          "Nf6"),
      makeGamePos(
          2, 0,
          "rn1qkbnr/ppp1pppp/8/3p4/3P2b1/5N2/PPP1PPPP/RNBQKB1R w KQkq - 2 3",
          "Bg4"),
      makeGamePos(
          1, 0,
          "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/2P2N2/PP2PPPP/RNBQKB1R b KQkq - 0 3",
          "c3"),
      makeGamePos(
          1, 1,
          "rnbqkbnr/ppp1pppp/8/3p4/3P4/6P1/PPP1PP1P/RNBQKBNR b KQkq - 0 2",
          "g3"),
      makeGamePos(
          1, 1,
          "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/6P1/PPP1PP1P/RNBQKBNR w KQkq - 1 3",
          "Nf6"),
      makeGamePos(
          1, 1,
          "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/6P1/PPP1PPBP/RNBQK1NR b KQkq - 2 3",
          "Bg2"),
      makeGamePos(
          2, 0,
          "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5NP1/PPP1PP1P/RNBQKB1R b KQkq - 2 3",
          "Nf3"),
  };
}

scid::core::Game parseShortGame() {
  scid::core::Game game;
  scid::core::pgn::ParseLog parseLog;
  EXPECT_TRUE(scid::core::pgn::parseGame(test_pgnShort.c_str(),
                                         test_pgnShort.size(), game, parseLog));
  EXPECT_STREQ(parseLog.log.c_str(), "");
  return game;
}

} // namespace

TEST(CoreGamePositionsTest, CollectsFenAndRavMetadata) {
  auto game = parseShortGame();

  auto gamepos = scid::core::gamepos::collectPositions(game);
  auto expected = expectedGamePositions();

  EXPECT_EQ(expected.size(), gamepos.size());
  size_t n = std::min(expected.size(), gamepos.size());
  for (size_t i = 0; i < n; i++) {
    const auto &e1 = expected[i];
    const auto &e2 = gamepos[i];
    EXPECT_EQ(e1.RAVdepth, e2.RAVdepth);
    EXPECT_EQ(e1.RAVnum, e2.RAVnum);
    EXPECT_EQ(e1.FEN, e2.FEN);
    EXPECT_EQ(e1.NAGs, e2.NAGs);
    EXPECT_EQ(e1.comment, e2.comment);
    EXPECT_EQ(e1.lastMoveSAN, e2.lastMoveSAN);
  }
}

TEST(CoreGamePositionsTest, PreservesEnoughDataToReconstructMovetext) {
  auto game = parseShortGame();

  auto gamepos = scid::core::gamepos::collectPositions(game);

  EXPECT_EQ(test_pgnShort.c_str(), encodePgn(gamepos));
}

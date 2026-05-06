#pragma once

#include "scidup/core/primitives.h"

namespace scid::database {

const uint NUM_RESULT_TYPES = 4;
typedef byte resultT;
const resultT RESULT_None = 0, RESULT_White = 1, RESULT_Black = 2,
              RESULT_Draw = 3;

const uint RESULT_SCORE[4] = {1, 2, 0, 1};
const char RESULT_CHAR[4] = {'*', '1', '0', '='};
const char RESULT_STR[4][4] = {"*", "1-0", "0-1", "=-="};
const char RESULT_LONGSTR[4][8] = {"*", "1-0", "0-1", "1/2-1/2"};
const resultT RESULT_OPPOSITE[4] = {
    RESULT_None, RESULT_Black, RESULT_White, RESULT_Draw};

} // namespace scid::database

#include "pgn_lexer.h"

#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>

TEST(Test_PgnLexer, is_PGNsymbol) {
	bool chars[256] = {false};
	for (unsigned ch = 'A'; ch <= 'Z'; ++ch) {
		chars[ch] = true;
	}
	for (unsigned ch = 'a'; ch <= 'z'; ++ch) {
		chars[ch] = true;
	}
	for (unsigned ch = '0'; ch <= '9'; ++ch) {
		chars[ch] = true;
	}
	const unsigned extra[] = {'_', '+', '#', '=', ':', '-'};
	for (unsigned ch : extra) {
		chars[ch] = true;
	}
	const unsigned drawresult_unclear[] = {'/', '~'};
	for (unsigned ch : drawresult_unclear) {
		chars[ch] = true;
	}
	const unsigned chess_variants[] = {',', '@'};
	for (unsigned ch : chess_variants) {
		chars[ch] = true;
	}

	for (int i = 0; i < 256; i++) {
		EXPECT_EQ(
		    chars[i],
		    scid::core::pgn_impl::is_PGNsymbol(static_cast<signed char>(i)));
		EXPECT_EQ(
		    chars[i],
		    scid::core::pgn_impl::is_PGNsymbol(static_cast<unsigned char>(i)));
	}
}

TEST(Test_PgnLexer, pgn_trim) {
	const char* tests[] = {
	    "Surname, Name  (2800)",                //
	    "Surname, Name  (2800)       ",         //
	    "    Surname, Name  (2800)",            //
	    "    Surname, Name  (2800)       ",     //
	    "\vSurname, Name  (2800)\r\n",          //
	    " \t\n   Surname, Name  (2800)  \r\v  " //
	};

	for (auto str : tests) {
		auto str_view = std::make_pair(str, str + std::strlen(str));
		size_t n_newlines = std::count(str_view.first, str_view.second, '\n');
		EXPECT_EQ(n_newlines, scid::core::pgn::trim(str_view));
		EXPECT_TRUE(std::equal(str_view.first, str_view.second, tests[0]));
	}
}

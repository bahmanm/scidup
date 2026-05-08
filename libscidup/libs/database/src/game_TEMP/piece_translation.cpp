#include "scidup/database/game_TEMP/piece_translation.h"

namespace scid::database {

int language = 0; // default to english
//  0 = en,
//  1 = fr, 2 = es, 3 = de, 4 = it, 5 = ne, 6 = cz
//  7 = hu, 8 = no, 9 = sw, 10 = ca, 11 = fi, 12 = gr
//  TODO Piece translations for greek
const char* langPieces[] = {"",
                            "PPKRQDRTBFNC",
                            "PPKRQDRTBANC",
                            "PBKKQDRTBLNS",
                            "PPKRQDRTBANC",
                            "PpKKQDRTBLNP",
                            "PPKKQDRVBSNJ",
                            "PGKKQVRBBFNH",
                            "PBKKQDRTBLNS",
                            "PBKKQDRTBLNS",
                            "PPKRQDRTBANC",
                            "PSKKQDRTBLNR",
                            ""};

void transPieces(char* s) {
	if (language == 0)
		return;
	char* ptr = s;
	int i;

	while (*ptr) {
		if (*ptr >= 'A' && *ptr <= 'Z') {
			for (i = 0; i < 12; i += 2) {
				if (*ptr == langPieces[language][i]) {
					*ptr = langPieces[language][i + 1];
					break;
				}
			}
		}
		ptr++;
	}
}

char transPiecesChar(char c) {
	char ret = c;
	if (language == 0)
		return c;
	for (int i = 0; i < 12; i += 2) {
		if (c == langPieces[language][i]) {
			ret = langPieces[language][i + 1];
			break;
		}
	}
	return ret;
}

} // namespace scid::database

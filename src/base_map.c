#include "base_map.h"

char CHAR_TO_BASE[256] = { };
char BASE_TO_CHAR[16] = { };
char BASE_TO_COMP[16] = { };

void init_base_map(void)
{
	CHAR_TO_BASE['A'] = CHAR_TO_BASE['a'] = BASE_A;
	CHAR_TO_BASE['C'] = CHAR_TO_BASE['c'] = BASE_C;
	CHAR_TO_BASE['G'] = CHAR_TO_BASE['g'] = BASE_G;
	CHAR_TO_BASE['T'] = CHAR_TO_BASE['t'] = BASE_T;
	CHAR_TO_BASE['U'] = CHAR_TO_BASE['u'] = BASE_T;
	CHAR_TO_BASE['M'] = CHAR_TO_BASE['m'] = BASE_A | BASE_C; /* aMino */
	CHAR_TO_BASE['K'] = CHAR_TO_BASE['k'] = BASE_G | BASE_T; /* Keto */
	CHAR_TO_BASE['R'] = CHAR_TO_BASE['r'] = BASE_A | BASE_G; /* puRine */
	CHAR_TO_BASE['Y'] = CHAR_TO_BASE['y'] = BASE_C | BASE_T; /* pYrimidine */
	CHAR_TO_BASE['S'] = CHAR_TO_BASE['s'] = BASE_C | BASE_G; /* strong */
	CHAR_TO_BASE['W'] = CHAR_TO_BASE['w'] = BASE_A | BASE_T; /* weak */
	CHAR_TO_BASE['B'] = CHAR_TO_BASE['b'] = BASE_C | BASE_G | BASE_T; /* not 'A' */
	CHAR_TO_BASE['D'] = CHAR_TO_BASE['d'] = BASE_A | BASE_G | BASE_T; /* not 'C' */
	CHAR_TO_BASE['H'] = CHAR_TO_BASE['h'] = BASE_A | BASE_C | BASE_T; /* not 'G' */
	CHAR_TO_BASE['V'] = CHAR_TO_BASE['v'] = BASE_A | BASE_C | BASE_G; /* not 'T/U' */
	CHAR_TO_BASE['N'] = CHAR_TO_BASE['n'] = BASE_N;
	CHAR_TO_BASE['X'] = CHAR_TO_BASE['x'] = BASE_N;

	BASE_TO_CHAR[BASE_A] = 'A';
	BASE_TO_CHAR[BASE_C] = 'C';
	BASE_TO_CHAR[BASE_G] = 'G';
	BASE_TO_CHAR[BASE_T] = 'T';
	BASE_TO_CHAR[BASE_A | BASE_C] = 'M';
	BASE_TO_CHAR[BASE_G | BASE_T] = 'K';
	BASE_TO_CHAR[BASE_A | BASE_G] = 'R';
	BASE_TO_CHAR[BASE_C | BASE_T] = 'Y';
	BASE_TO_CHAR[BASE_C | BASE_G] = 'S';
	BASE_TO_CHAR[BASE_A | BASE_T] = 'W';
	BASE_TO_CHAR[BASE_C | BASE_G | BASE_T] = 'B';
	BASE_TO_CHAR[BASE_A | BASE_G | BASE_T] = 'D';
	BASE_TO_CHAR[BASE_A | BASE_C | BASE_T] = 'H';
	BASE_TO_CHAR[BASE_A | BASE_C | BASE_G] = 'V';
	BASE_TO_CHAR[BASE_N] = 'N';

	BASE_TO_COMP[BASE_A] = BASE_T;
	BASE_TO_COMP[BASE_C] = BASE_G;
	BASE_TO_COMP[BASE_G] = BASE_C;
	BASE_TO_COMP[BASE_T] = BASE_A;
	BASE_TO_COMP[BASE_A | BASE_C] = BASE_T | BASE_G;
	BASE_TO_COMP[BASE_G | BASE_T] = BASE_C | BASE_A;
	BASE_TO_COMP[BASE_A | BASE_G] = BASE_T | BASE_C;
	BASE_TO_COMP[BASE_C | BASE_T] = BASE_G | BASE_A;
	BASE_TO_COMP[BASE_C | BASE_G] = BASE_G | BASE_C;
	BASE_TO_COMP[BASE_A | BASE_T] = BASE_T | BASE_A;
	BASE_TO_COMP[BASE_C | BASE_G | BASE_T] = BASE_G | BASE_C | BASE_A;
	BASE_TO_COMP[BASE_A | BASE_G | BASE_T] = BASE_T | BASE_C | BASE_A;
	BASE_TO_COMP[BASE_A | BASE_C | BASE_T] = BASE_T | BASE_G | BASE_A;
	BASE_TO_COMP[BASE_A | BASE_C | BASE_G] = BASE_T | BASE_G | BASE_C;
	BASE_TO_COMP[BASE_N] = BASE_N;
}
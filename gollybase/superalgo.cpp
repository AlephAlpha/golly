// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "superalgo.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if defined(WIN32) || defined(WIN64)
#define strncasecmp _strnicmp
#define firstbitset(x,y) unsigned long _r ; _BitScanReverse(&_r, y) ; x = (state) _r
#else
#define firstbitset(x,y) x = (state) (ffs(y) - 1)
#endif

using namespace std ;

int superalgo::NumCellStates() {
   return maxCellStates ;
}

static const char *DEFAULTRULE = "LifeSuper" ;
const char* superalgo::DefaultRule() {
   return DEFAULTRULE ;
}

static const char *DEFAULTB = "3" ;
static const char *DEFAULTS = "23" ;

// postfix and number of states for [R]Super rules
static const char *SUPERPOSTFIX = "Super" ;
static const int superStates = 26 ;

// postfix and number of states for [R]History rules
static const char *HISTORYPOSTFIX = "History" ;
static const int historyStates = 7 ;

// postfix and number of states for [R]Investigator rules
static const char *INVESTIGATORPOSTFIX = "Investigator" ;
static const int investigatorStates = 21 ;

// bit masks for [R]Super neighboring cell states
static const int aliveWith14 = (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11) | (1 << 13) | (1 << 14) | (1 << 15) | (1 << 17) | (1 << 19) | (1 << 21) | (1 << 23) | (1 << 25) ;
static const int aliveWith14or18 = aliveWith14 | (1 << 18) ;
static const int alive1or3or5or7 = (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) ;
static const int alive9to25 = (1 << 9) | (1 << 11) | (1 << 13) | (1 << 15) | (1 << 17) | (1 << 19) | (1 << 21) | (1 << 23) | (1 << 25) ;
static const int alive1or3or5or9or11 = (1 << 1) | (1 << 3) | (1 << 5) | (1 << 9) | (1 << 11) ;
static const int alive7or13or15or17or19or21or23or25 = (1 << 7) | (1 << 13) | (1 << 15) | (1 << 17) | (1 << 19) | (1 << 21) | (1 << 23) | (1 << 25) ;
static const int alive1or5or7or9or11 = (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11) ;
static const int alive13or15or17or19or21or23or25 = (1 << 13) | (1 << 15) | (1 << 17) | (1 << 19) | (1 << 21) | (1 << 23) | (1 << 25) ;
static const int alive9or11 = (1 << 9) | (1 << 11) ;
static const int alive1or3or5or13or15or17or19or21or23or25 = (1 << 1) | (1 << 3) | (1 << 5) | (1 << 13) | (1 << 15) | (1 << 17) | (1 << 19) | (1 << 21) | (1 << 23) | (1 << 25) ;

// bit masks for [R]Investigator neighboring cell states
static const int deadForcer = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7) | (1 << 14) | (1 << 16) ;
static const int birthForcer = (1 << 8) | (1 << 9) | (1 << 12) | (1 << 13) | (1 << 14) ;
static const int requireState1 = (1 << 15) | (1 << 16) ;
static const int treatIfDead = (1 << 1) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8) | (1 << 10) | (1 << 12) | (1 << 15) |  (1 << 16) | (1 << 17) | (1 << 19) ;
static const int treatIfAlive = treatIfDead ^ ((1 << 17) | (1 << 18) | (1 << 19) | (1 << 20)) ;
static const state nextState[] = {0, 1, 2, 3, 4, 5, 7, 6, 8, 9, 11, 10, 13, 12, 14, 15, 16, 17, 18, 20, 19};

// XPM data for the 31x31 icons
static const char* super31x31[] = {
/* width height num_colors chars_per_pixel */
"31 775 5 1",
/* colors */
". c #000000",
"B c #404040",
"C c #808080",
"D c #C0C0C0",
"E c #FFFFFF",
/* icon for state 1 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 2 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 3 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 4 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 5 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 6 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 7 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 8 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 9 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 10 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 11 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 12 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 13 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 14 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 15 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 16 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 17 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 18 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 19 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 20 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 21 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 22 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 23 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"...............................",
/* icon for state 24 */
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.E.E.E.E.E.E.E.E.",
/* icon for state 25 */
"...............................",
"...............................",
"..........BCDEEEEEDCB..........",
".........CEEEEEEEEEEEC.........",
".......BEEEEEEEEEEEEEEEB.......",
"......DEEEEEEEEEEEEEEEEED......",
".....DEEEEEEEEEEEEEEEEEEED.....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..EEEEEEEEEEEEEEEEEEEEEEEEEEE..",
"..DEEEEEEEEEEEEEEEEEEEEEEEEED..",
"..CEEEEEEEEEEEEEEEEEEEEEEEEEC..",
"..BEEEEEEEEEEEEEEEEEEEEEEEEEB..",
"...CEEEEEEEEEEEEEEEEEEEEEEEC...",
"....EEEEEEEEEEEEEEEEEEEEEEE....",
"....BEEEEEEEEEEEEEEEEEEEEEB....",
".....DEEEEEEEEEEEEEEEEEEED.....",
"......DEEEEEEEEEEEEEEEEED......",
".......BEEEEEEEEEEEEEEEB.......",
".........CEEEEEEEEEEEC.........",
"..........BCDEEEEEDCB..........",
"...............................",
"..............................."};

// XPM data for the 15x15 icons
static const char* super15x15[] = {
/* width height num_colors chars_per_pixel */
"15 375 5 1",
/* colors */
". c #000000",
"B c #404040",
"C c #808080",
"D c #C0C0C0",
"E c #FFFFFF",
/* icon for state 1 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 2 */
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
/* icon for state 3 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 4 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 5 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 6 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 7 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 8 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 9 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 10 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 11 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 12 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 13 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 14 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 15 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 16 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 17 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 18 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 19 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 20 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 21 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 22 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 23 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"...............",
/* icon for state 24 */
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
".E.E.E.E.E.E.E.",
"E.E.E.E.E.E.E.E",
/* icon for state 25 */
"...............",
"....BDEEEDB....",
"...DEEEEEEED...",
"..DEEEEEEEEED..",
".BEEEEEEEEEEEB.",
".DEEEEEEEEEEED.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".EEEEEEEEEEEEE.",
".DEEEEEEEEEEED.",
".BEEEEEEEEEEEB.",
"..DEEEEEEEEED..",
"...DEEEEEEED...",
"....BDEEEDB....",
"..............."};

// XPM data for the 7x7 icons
static const char* super7x7[] = {
/* width height num_colors chars_per_pixel */
"7 175 6 1",
/* colors */
". c #000000",
"B c #404040",
"C c #808080",
"D c #C0C0C0",
"E c #FFFFFF",
"F c #E0E0E0",
/* icon for state 1 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 2 */
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
/* icon for state 3 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 4 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 5 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 6 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 7 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 8 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 9 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 10 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 11 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 12 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 13 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 14 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 15 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 16 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 17 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 18 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 19 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 20 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 21 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 22 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 23 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB.",
/* icon for state 24 */
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
".E.E.E.",
"E.E.E.E",
/* icon for state 25 */
".BFEFB.",
"BEEEEEB",
"FEEEEEF",
"EEEEEEE",
"FEEEEEF",
"BEEEEEB",
".BFEFB."};

// returns a count of the number of bits set in given int
static int bitcount(int v) {
   int r = 0 ;
   while (v) {
      r++ ;
      v &= v - 1 ;
   }
   return r ;
}

state superalgo::slowcalc(state nw, state n, state ne, state w, state c,
                                state e, state sw, state s, state se) {
   // result
   state result = 0 ;
   int typeMask = 0 ;
   int calc = 0 ;
   bool process = true ;

   // get the lookup table
   char *lookup = rule3x3 ;

   result = c ;

   // typemask has a bit set per state in the neighbouring cells
   switch (neighbormask) {
      case HEXAGONAL:
         typeMask = (1 << nw) | (1 << n) | (1 << e) | (1 << w) | (1 << s) | (1 << se) ;
      break ;

      case VON_NEUMANN:
         typeMask = (1 << n) | (1 << e) | (1 << w) | (1 << s) ;
      break ;

      default:
         // Moore
         typeMask = (1 << nw) | (1 << n) | (1 << ne) | (1 << e) | (1 << w) | (1 << sw) | (1 << s) | (1 << se) ;
         break ;
   }

   // check rule type
   switch (rule_type) {
      case HISTORY:
         // [R]History
         // handle state 6
         process = true ;
         if (typeMask & (1 << 6)) {
            process = false ;
            switch (c) {
               case 1:
                  result = 2 ;
                  break ;

               case 3:
               case 5:
                  result = 4 ;
                  break ;

               default:
               process = true ;
               break ;
            }
         }

         // check whether state still needs processing
         if (process) {
            // create lookup index for next generation
            int index = ((nw & 1) << 8) | ((n & 1) << 7) | ((ne & 1) << 6)
               | ((w & 1) << 5) | ((c & 1) << 4) | ((e & 1) << 3)
               | ((sw & 1) << 2) | ((s & 1) << 1) | (se & 1) ;

            // get cell state
            if (lookup[index]) {
               // cell alive
               // was cell alive in this generation
               if ((c & 1) == 0) {
                  // cell was dead so has been born now
                  switch (c) {
                     case 4:
                        result = 3 ;
                        break ;

                     case 6:
                        break ;

                     default:
                        result = 1 ;
                  }
               }
            } else {
               // cell dead
               // was cell alive in this generation
               if ((c & 1) != 0) {
                  // cell was alive so has died
                  if (c == 5) {
                     result = 4 ;
                  } else {
                     result = c + 1 ;
                  }
               }
            }
         }
         break ;

      case SUPER:
         // [R]Super
         // handle state 6
         process = true ;
         if (typeMask & (1 << 6)) {
            process = false ;
            if (c == 7 || c == 8 || c >= 13) {
               result = 0 ;
            } else {
               switch (c) {
                  case 1:
                     result = 2 ;
                     break ;

                  case 3:
                  case 5:
                     result = 4 ;
                     break ;

                  case 9:
                     result = 10 ;
                     break ;

                  case 11:
                     result = 12 ;
                     break ;

                  default:
                     // not handled here so process below
                     process = true ;
                     break ;
               }
            }
         }

         // check whether state still needs processing
         if (process) {
            // create lookup index for next generation
            int index = ((nw & 1) << 8) | ((n & 1) << 7) | ((ne & 1) << 6)
               | ((w & 1) << 5) | ((c & 1) << 4) | ((e & 1) << 3)
               | ((sw & 1) << 2) | ((s & 1) << 1) | (se & 1) ;

            // get cell state
            if (lookup[index]) {
               // cell alive
               // was cell alive in this generation
               if ((c & 1) == 0) {
                  // cell was dead so has been born now
                  switch (c) {
                     case 4:
                        result = 3 ;
                        break ;

                     case 6:
                        break ;

                     case 8:
                        result = 7 ;
                        break ;

                     default:
                        result = 1 ;
                        calc = typeMask & alive9to25 ;
                        // check if the neighbors are of just one state
                        if (((typeMask & alive1or3or5or7) == 0) && (calc && ((calc & (calc - 1)) == 0))) {
                           // the bit index gives the cell state
                           firstbitset(result, calc) ;
                        } else {
                           calc = typeMask & alive13or15or17or19or21or23or25 ;
                           if ((typeMask & (1 << 3)) && (calc && (calc & (calc - 1)) == 0) && ((typeMask & alive1or5or7or9or11) == 0)) {
                              firstbitset(result, calc) ;
                           } else {
                              calc = typeMask & alive9or11 ;
                              if ((typeMask & (1 << 7)) && (calc && (calc & (calc - 1)) == 0) && ((typeMask & alive1or3or5or13or15or17or19or21or23or25) == 0)) {
                                 firstbitset(result, calc) ;
                              } else {
                                 calc = typeMask & alive7or13or15or17or19or21or23or25 ;
                                 if (calc && ((typeMask & alive1or3or5or9or11) == 0)) {
                                    result = 13 ;
                                 }
                              }
                           }
                        }
                        break ;
                  }
               }
            } else {
               // cell dead
               // was cell alive in this generation
               if ((c & 1) != 0) {
                  // cell was alive so has died
                  if (c <= 11) {
                     if (c == 5) {
                        result = 4 ;
                     } else {
                        result = c + 1 ;
                     }
                  } else {
                     result = 0 ;
                  }
               } else {
                  // cell is still dead
                  if (c == 14) {
                     result = 0 ;
                  } else {
                     if (c > 14) {
                        switch (c) {
                           case 16:
                              if ((typeMask & aliveWith14) != 0) {
                                 result = 14 ;
                              }
                              break ;

                           case 18:
                              if ((typeMask & (1 << 22)) != 0) {
                                 result = 22 ;
                              }
                              break ;

                           case 20:
                              if ((typeMask & (1 << 18)) != 0) {
                                 result = 18 ;
                              }
                              break ;

                           case 22:
                              if ((typeMask & (1 << 20)) != 0) {
                                 result = 20 ;
                              }
                              break ;

                           case 24:
                              if ((typeMask & aliveWith14or18) != 0) {
                                 result = 18 ;
                              }
                              break ;
                        }
                     }
                  }
               }
            }
         }
         break ;

      case INVESTIGATOR:
         if (c >= 2) {
            result = nextState[c] ;
         } else {
            // check for all cells dead
            if (typeMask == 1 && c == 0) {
               result = 0;
            } else {
               if (typeMask & (c ? deadForcer : birthForcer)) {
                  result = 1 - c ;
               } else {
                  if (!c && (typeMask & requireState1) && !(typeMask & 2)) {
                     result = 0 ;
                  } else {
                     calc = c ? treatIfAlive : treatIfDead ;
                     result = lookup[(((calc >> nw) & 1) << 8) |
                        (((calc >> n) & 1) << 7) |
                        (((calc >> ne) & 1) << 6) |
                        (((calc >> w) & 1) << 5) |
                        (c << 4) |
                        (((calc >> e) & 1) << 3) |
                        (((calc >> sw) & 1) << 2) |
                        (((calc >> s) & 1) << 1) |
                        ((calc >> se) & 1)] ;
                  }
               }
            }
         }
         break ;
   }

   // output new cell state
   return result ;
}

static lifealgo *creator() { return new superalgo() ; }

void superalgo::doInitializeAlgoInfo(staticAlgoInfo &ai) {
   ghashbase::doInitializeAlgoInfo(ai) ;
   ai.setAlgorithmName("Super") ;
   ai.setAlgorithmCreator(&creator) ;
   ai.minstates = superStates + historyStates + investigatorStates ;
   ai.maxstates = superStates + historyStates + investigatorStates ;

   // init default color scheme
   ai.defgradient = false ;             // use gradient
   ai.defr1 = 255 ;                     // start color = red
   ai.defg1 = 0 ;
   ai.defb1 = 0 ;
   ai.defr2 = 255 ;                     // end color = yellow
   ai.defg2 = 255 ;
   ai.defb2 = 0 ;

   // first 26 colors are for [R]Super rules
   ai.defr[0] = 48   ; ai.defg[0] = 48   ; ai.defb[0] = 48;
   ai.defr[1] = 0    ; ai.defg[1] = 255  ; ai.defb[1] = 0 ;
   ai.defr[2] = 0    ; ai.defg[2] = 0    ; ai.defb[2] = 160 ;
   ai.defr[3] = 255  ; ai.defg[3] = 216  ; ai.defb[3] = 255 ;
   ai.defr[4] = 255  ; ai.defg[4] = 0    ; ai.defb[4] = 0 ;
   ai.defr[5] = 255  ; ai.defg[5] = 255  ; ai.defb[5] = 0 ;
   ai.defr[6] = 96   ; ai.defg[6] = 96   ; ai.defb[6] = 96 ;
   ai.defr[7] = 255  ; ai.defg[7] = 105  ; ai.defb[7] = 180 ;
   ai.defr[8] = 128  ; ai.defg[8] = 0    ; ai.defb[8] = 128 ;
   ai.defr[9] = 0    ; ai.defg[9] = 191  ; ai.defb[9] = 255 ;
   ai.defr[10] = 0   ; ai.defg[10] = 64  ; ai.defb[10] = 128 ;
   ai.defr[11] = 64  ; ai.defg[11] = 224 ; ai.defb[11] = 208 ;
   ai.defr[12] = 0   ; ai.defg[12] = 128 ; ai.defb[12] = 64 ;
   ai.defr[13] = 255 ; ai.defg[13] = 255 ; ai.defb[13] = 255 ;
   ai.defr[14] = 255 ; ai.defg[14] = 99  ; ai.defb[14] = 71 ;
   ai.defr[15] = 250 ; ai.defg[15] = 128 ; ai.defb[15] = 114 ;
   ai.defr[16] = 219 ; ai.defg[16] = 112 ; ai.defb[16] = 147 ;
   ai.defr[17] = 255 ; ai.defg[17] = 165 ; ai.defb[17] = 0 ;
   ai.defr[18] = 245 ; ai.defg[18] = 222 ; ai.defb[18] = 179 ;
   ai.defr[19] = 0   ; ai.defg[19] = 255 ; ai.defb[19] = 255 ;
   ai.defr[20] = 192 ; ai.defg[20] = 192 ; ai.defb[20] = 192 ;
   ai.defr[21] = 192 ; ai.defg[21] = 255 ; ai.defb[21] = 128 ;
   ai.defr[22] = 255 ; ai.defg[22] = 182 ; ai.defb[22] = 193 ;
   ai.defr[23] = 0   ; ai.defg[23] = 255 ; ai.defb[23] = 127 ;
   ai.defr[24] = 1   ; ai.defg[24] = 1   ; ai.defb[24] = 1 ;
   ai.defr[25] = 255 ; ai.defg[25] = 0   ; ai.defb[25] = 127 ;

   // next 7 colors are for [R]History rules
   ai.defr[26] = 48   ; ai.defg[26] = 48   ; ai.defb[26] = 48;
   ai.defr[27] = 0    ; ai.defg[27] = 255  ; ai.defb[27] = 0 ;
   ai.defr[28] = 0    ; ai.defg[28] = 0    ; ai.defb[28] = 128 ;
   ai.defr[29] = 216  ; ai.defg[29] = 255  ; ai.defb[29] = 216 ;
   ai.defr[30] = 255  ; ai.defg[30] = 0    ; ai.defb[30] = 0 ;
   ai.defr[31] = 255  ; ai.defg[31] = 255  ; ai.defb[31] = 0 ;
   ai.defr[32] = 96   ; ai.defg[32] = 96   ; ai.defb[32] = 96 ;

   // next 21 colors are for [R]Investigator rules
   ai.defr[33] = 0    ; ai.defg[33] = 0    ; ai.defb[33] =  0 ;
   ai.defr[34] = 0    ; ai.defg[34] = 236  ; ai.defb[34] =  91 ;
   ai.defr[35] = 0    ; ai.defg[35] = 192  ; ai.defb[35] =  254 ;
   ai.defr[36] = 254  ; ai.defg[36] = 0    ; ai.defb[36] =  0 ;
   ai.defr[37] = 254  ; ai.defg[37] = 254  ; ai.defb[37] =  254 ;
   ai.defr[38] = 75   ; ai.defg[38] = 75   ; ai.defb[38] =  75 ;
   ai.defr[39] = 239  ; ai.defg[39] = 41   ; ai.defb[39] =  254 ;
   ai.defr[40] = 64   ; ai.defg[40] = 0    ; ai.defb[40] =  128 ;
   ai.defr[41] = 254  ; ai.defg[41] = 230  ; ai.defb[41] =  0 ;
   ai.defr[42] = 150  ; ai.defg[42] = 128  ; ai.defb[42] =  0 ;
   ai.defr[43] = 130  ; ai.defg[43] = 200  ; ai.defb[43] =  0 ;
   ai.defr[44] = 0    ; ai.defg[44] = 120  ; ai.defb[44] =  40 ;
   ai.defr[45] = 254  ; ai.defg[45] = 140  ; ai.defb[45] =  0 ;
   ai.defr[46] = 140  ; ai.defg[46] = 70   ; ai.defb[46] =  0 ;
   ai.defr[47] = 0    ; ai.defg[47] = 0    ; ai.defb[47] =  254 ;
   ai.defr[48] = 192  ; ai.defg[48] = 192  ; ai.defb[48] =  192 ;
   ai.defr[49] = 128  ; ai.defg[49] = 128  ; ai.defb[49] =  128 ;
   ai.defr[50] = 254  ; ai.defg[50] = 112  ; ai.defb[50] =  140 ;
   ai.defr[51] = 174  ; ai.defg[51] = 0    ; ai.defb[51] =  168 ;
   ai.defr[52] = 0    ; ai.defg[52] = 152  ; ai.defb[52] =  127 ;
   ai.defr[53] = 0    ; ai.defg[53] = 73   ; ai.defb[53] =  59 ;

   // init default icon data
   ai.defxpm7x7 = super7x7;
   ai.defxpm15x15 = super15x15;
   ai.defxpm31x31 = super31x31;
}

superalgo::superalgo() {
   int i ;

   // base64 encoding characters
   base64_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" ;

   // all valid rule letters
   valid_rule_letters = "012345678ceaiknjqrytwz-" ;

   // rule letters per neighbor count
   rule_letters[0] = "ce" ;
   rule_letters[1] = "ceaikn" ;
   rule_letters[2] = "ceaiknjqry" ;
   rule_letters[3] = "ceaiknjqrytwz" ;

   // isotropic neighborhoods per neighbor count
   static int entry0[2] = { 1, 2 } ;
   static int entry1[6] = { 5, 10, 3, 40, 33, 68 } ;
   static int entry2[10] = { 69, 42, 11, 7, 98, 13, 14, 70, 41, 97 } ;
   static int entry3[13] = { 325, 170, 15, 45, 99, 71, 106, 102, 43, 101, 105, 78, 108 } ;
   rule_neighborhoods[0] = entry0 ;
   rule_neighborhoods[1] = entry1 ;
   rule_neighborhoods[2] = entry2 ;
   rule_neighborhoods[3] = entry3 ;

   // bit offset for suvival part of rule
   survival_offset = 9 ;

   // bit in letter bit mask indicating negative
   negative_bit = 13 ; 

   // maximum number of letters per neighbor count
   max_letters[0] = 0 ;
   max_letters[1] = (int) strlen(rule_letters[0]) ;
   max_letters[2] = (int) strlen(rule_letters[1]) ;
   max_letters[3] = (int) strlen(rule_letters[2]) ;
   max_letters[4] = (int) strlen(rule_letters[3]) ;
   max_letters[5] = max_letters[3] ;
   max_letters[6] = max_letters[2] ;
   max_letters[7] = max_letters[1] ;
   max_letters[8] = max_letters[0] ;
   for (i = 0 ; i < survival_offset ; i++) {
      max_letters[i + survival_offset] = max_letters[i] ;
   }

   // canonical letter order per neighbor count
   static int order0[1] = { 0 } ;
   static int order1[2] = { 0, 1 } ;
   static int order2[6] = { 2, 0, 1, 3, 4, 5 } ;
   static int order3[10] = { 2, 0, 1, 3, 6, 4, 5, 7, 8, 9 } ;
   static int order4[13] = { 2, 0, 1, 3, 6, 4, 5, 7, 8, 10, 11, 9, 12 } ;
   order_letters[0] = order0 ;
   order_letters[1] = order1 ;
   order_letters[2] = order2 ;
   order_letters[3] = order3 ;
   order_letters[4] = order4 ;
   order_letters[5] = order3 ;
   order_letters[6] = order2 ;
   order_letters[7] = order1 ;
   order_letters[8] = order0 ;
   for (i = 0 ; i < survival_offset ; i++) {
      order_letters[i + survival_offset] = order_letters[i] ;
   }

   // initialize
   initRule() ;
}

superalgo::~superalgo() {
}

// initialize
void superalgo::initRule() {
   // default to Moore neighborhood totalistic rule
   neighbormask = MOORE ; 
   neighbors = 8 ;
   totalistic = true ;
   using_map = false ;
   rule_type = SUPER ;

   maxCellStates = superStates ;

   // one bit for each neighbor count
   // s = survival, b = birth
   // bit:     17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
   // meaning: s8 s7 s6 s5 s4 s3 s2 s1 s0 b8 b7 b6 b5 b4 b3 b2 b1 b0
   rulebits = 0 ;

   // one bit for each letter per neighbor count
   // N = negative bit
   // bit:     13 12 11 10  9  8  7  6  5  4  3  2  1  0
   // meaning:  N  z  w  t  y  r  q  j  n  k  i  a  e  c
   memset(letter_bits, 0, sizeof(letter_bits)) ;

   // canonical rule string
   memset(canonrule, 0, sizeof(canonrule)) ;
}

// set 3x3 grid based on totalistic value
void superalgo::setTotalistic(int value, bool survival) {
   int mask = 0 ;
   int nbrs = 0 ;
   int nhood = 0 ;
   int i = 0 ;
   int j = 0 ;
   int offset = 0 ;

   // check if this value has already been processed
   if (survival) {
      offset = survival_offset ;
   }
   if ((rulebits & (1 << (value + offset))) == 0) {
       // update the rulebits
       rulebits |= 1 << (value + offset) ;

       // update the mask if survival
       if (survival) {
          mask = 0x10 ;
       }

       // fill the array based on totalistic value
       for (i = 0 ; i < ALL3X3 ; i += 32) {
          for (j = 0 ; j < 16 ; j++) {
             nbrs = 0 ;
             nhood = (i+j) & neighbormask ;
             while (nhood > 0) {
                nbrs += (nhood & 1) ;
                nhood >>= 1 ;
             }
             if (value == nbrs) {
                rule3x3[i+j+mask] = 1 ;
             }
          }
      }
   }
}

// flip bits
int superalgo::flipBits(int x) {
   return ((x & 0x07) << 6) | ((x & 0x1c0) >> 6) | (x & 0x38) ;
}

// rotate 90
int superalgo::rotateBits90Clockwise(int x) {
   return ((x & 0x4) << 6) | ((x & 0x20) << 2) | ((x & 0x100) >> 2)
                        | ((x & 0x2) << 4) | (x & 0x10) | ((x & 0x80) >> 4)
                        | ((x & 0x1) << 2) | ((x & 0x8) >> 2) | ((x & 0x40) >> 6) ;
}

// set symmetrical neighborhood into 3x3 map
void superalgo::setSymmetrical512(int x, int b) {
   int y = x ;
   int i = 0 ;

   // process each of the 4 rotations
   for (i = 0 ; i < 4 ; i++) {
      rule3x3[y] = (char) b ;
      y = rotateBits90Clockwise(y) ;
   }

   // flip
   y = flipBits(y) ;

   // process each of the 4 rotations
   for (i = 0 ; i < 4 ; i++) {
      rule3x3[y] = (char) b ;
      y = rotateBits90Clockwise(y) ;
   }
}

// set symmetrical neighborhood
void superalgo::setSymmetrical(int value, bool survival, int lindex, int normal) {
   int xorbit = 0 ;
   int nindex = value - 1 ;
   int x = 0 ;
   int offset = 0 ;

   // check for homogeneous bits
   if (value == 0 || value == 8) {
      setTotalistic(value, survival) ;
   } else {
      // update the rulebits
      if (survival) {
         offset = survival_offset ;
      }
      rulebits |= 1 << (value + offset) ;

      // reflect the index if in second half
      if (nindex > 3) {
         nindex = 6 - nindex ;
         xorbit = 0x1ef ;
      }

      // update the letterbits
      letter_bits[value + offset] |= 1 << lindex ;

      if (!normal) {
         // set the negative bit
         letter_bits[value + offset] |= 1 << negative_bit ;
      }

      // lookup the neighborhood
      x = rule_neighborhoods[nindex][lindex] ^ xorbit ;
      if (survival) {
         x |= 0x10 ;
      }
      setSymmetrical512(x, normal) ;
   }
}

// set totalistic birth or survival rule from a string
void superalgo::setTotalisticRuleFromString(const char *rule, bool survival) {
   char current ;

   // process each character in the rule string
   while ( *rule ) {
      current = *rule ;
      rule++ ;

      // convert the digit to an integer
      current -= '0' ;

      // set totalistic
      setTotalistic(current, survival) ;
   }
}

// set rule from birth or survival string
void superalgo::setRuleFromString(const char *rule, bool survival) {
   // current and next character
   char current ;
   char next ;

   // whether character normal or inverted
   int normal = 1 ;

   // letter index
   char *letterindex = 0 ;
   int lindex = 0 ;
   int nindex = 0 ;

   // process each character
   while ( *rule ) {
      current = *rule ;
      rule++ ;

      // find the index in the valid character list
      letterindex = strchr((char*) valid_rule_letters, current) ;
      lindex = letterindex ? int(letterindex - valid_rule_letters) : -1 ;

      // check if it is a digit
      if (lindex >= 0 && lindex <= 8) {
         // determine what follows the digit
         next = *rule ;
         nindex = -1 ;
         if (next) {
            letterindex = strchr((char*) rule_letters[3], next) ;
            if (letterindex) {
               nindex = int(letterindex - rule_letters[3]) ;
            }
         }

         // is the next character a digit or minus?
         if (nindex == -1) {
            setTotalistic(lindex, survival) ;
         }

         // check for inversion
         normal = 1 ;
         if (next == '-') {
            rule++ ;
            next = *rule ;

            // invert following character meanings
            normal = 0 ;
         }

         // process non-totalistic characters
         if (next) {
            letterindex = strchr((char*) rule_letters[3], next) ;
            nindex = -1 ;
            if (letterindex) {
               nindex = int(letterindex - rule_letters[3]) ;
            }
            while (nindex >= 0) {
               // set symmetrical
               setSymmetrical(lindex, survival, nindex, normal) ;

               // get next character
               rule++ ;
               next = *rule ;
               nindex = -1 ;
               if (next) {
                  letterindex = strchr((char*) rule_letters[3], next) ;
                  if (letterindex) {
                     nindex = int(letterindex - rule_letters[3]) ;
                  }
               }
            }
         }
      }
   }
}

// create the rule map from the base64 encoded map
void superalgo::createRuleMapFromMAP(const char *base64) {
   // set the number of characters to read
   int power2 = 1 << (neighbors + 1) ;
   int fullchars = power2 / 6 ;
   int remainbits = power2 % 6 ;

   // create an array to read the MAP bits
   char bits[ALL3X3] ;

   // decode the base64 string
   int i = 0 ;
   char c = 0 ;
   int j = 0 ;
   const char *index = 0 ;

   for (i = 0 ; i < fullchars ; i++) {
      // convert character to base64 index
      index = strchr(base64_characters, *base64) ;
      base64++ ;
      c = index ? (char)(index - base64_characters) : 0 ;

      // decode the character
      bits[j] = c >> 5 ;
      j++ ;
      bits[j] = (c >> 4) & 1 ;
      j++ ;
      bits[j] = (c >> 3) & 1 ;
      j++ ;
      bits[j] = (c >> 2) & 1 ;
      j++ ;
      bits[j] = (c >> 1) & 1 ;
      j++ ;
      bits[j] = c & 1 ;
      j++ ;
   }

   // decode remaining bits from final character
   if (remainbits > 0) {
      index = strchr(base64_characters, *base64) ;
      c = index ? (char)(index - base64_characters) : 0 ;
      int b = 5 ;

      while (remainbits > 0) {
          bits[j] = (c >> b) & 1 ;
          b-- ;
          j++ ;
          remainbits-- ;
      }
   }

   // copy into rule array using the neighborhood mask
   int k, m ;
   for (i = 0 ; i < ALL3X3 ; i++) {
      k = 0 ;
      m = neighbors ;
      for (j = 8 ; j >= 0 ; j--) {
         if (neighbormask & (1 << j)) {
             if (i & (1 << j)) {
                k |= (1 << m) ;
             }
             m-- ;
         }
      }
      rule3x3[i] = bits[k] ;
   }
}

// create the rule map from birth and survival strings
void superalgo::createRuleMap(const char *birth, const char *survival) {
   // clear the rule array
   memset(rule3x3, 0, ALL3X3) ;

   // check for totalistic rule
   if (totalistic) {
      // set the totalistic birth rule
      setTotalisticRuleFromString(birth, false) ;

      // set the totalistic surivival rule
      setTotalisticRuleFromString(survival, true) ;
   } else {
      // set the non-totalistic birth rule
      setRuleFromString(birth, false) ;

      // set the non-totalistic survival rule
      setRuleFromString(survival, true) ;
   }
}

// add canonical letter representation
int superalgo::addLetters(int count, int p) {
   int bits ;            // bitmask of letters defined at this count
   int negative = 0 ;    // whether negative
   int setbits ;         // how many bits are defined
   int maxbits ;         // maximum number of letters at this count
   int letter = 0 ;
   int j ;

   // check if letters are defined for this neighbor count
   if (letter_bits[count]) {
      // get the bit mask
      bits = letter_bits[count] ;

      // check for negative
      if (bits & (1 << negative_bit)) {
         // letters are negative
         negative = 1 ;
         bits &= ~(1 << negative_bit) ;
      }

      // compute the number of bits set
      setbits = bitcount(bits) ;

      // get the maximum number of allowed letters at this neighbor count
      maxbits = max_letters[count] ;

      // do not invert if not negative and seven letters
      if (!(!negative && setbits == 7 && maxbits == 13)) {
         // if maximum letters minus number used is greater than number used then invert
         if (setbits + negative > (maxbits >> 1)) {
            // invert maximum letters for this count
            bits = ~bits & ((1 << maxbits) - 1) ;
            if (bits) {
               negative = !negative ;
            }
         }
      }

      // if negative and no letters then remove neighborhood count
      if (negative && !bits) {
         canonrule[p] = 0 ;
         p-- ;
      } else {
         // check whether to output minus
         if (negative) {
            canonrule[p++] = '-' ;
         }

         // add defined letters
         for (j = 0 ; j < maxbits ; j++) {
            // lookup the letter in order
            letter = order_letters[count][j] ;
            if (bits & (1 << letter)) {
               canonrule[p++] = rule_letters[3][letter] ;
            }
         }
      }
   }

   return p ;
}

// AKT: store valid rule in canonical format for getrule()
void superalgo::createCanonicalName(const char *base64, const char *postfix) {
   int p = 0 ;
   int np = 0 ;
   int i = 0 ;

   // the canonical version of a rule containing letters
   // might be simply totalistic
   bool stillnontotalistic = false ;

   // check for map rule
   if (using_map) {
      // output map header
      canonrule[p++] = 'M' ;
      canonrule[p++] = 'A' ;
      canonrule[p++] = 'P' ;

      // compute number of base64 characters
      int power2 = 1 << (neighbors + 1) ;
      int fullchars = power2 / 6 ;
      int remainbits = power2 % 6 ;

      // copy base64 part
      for (i = 0 ; i < fullchars ; i++) {
         if (*base64) {
            canonrule[p++] = *base64 ;
            base64++ ;
         }
      }

      // copy final bits of last character
      if (*base64) {
         const char *index = strchr(base64_characters, *base64) ;
         int c = index ? (char)(index - base64_characters) : 0 ;
         int k = 0 ;
         int m = 5 ;
         for (i = 0 ; i < remainbits ; i++) {
            k |= c & (1 << m) ;
            m-- ;
         }
         canonrule[p++] = base64_characters[k] ;
      }
   } else {
      if (strcmp(base64, DEFAULTRULE) == 0) {
         canonrule[p++] = 'L' ;
         canonrule[p++] = 'i' ;
         canonrule[p++] = 'f' ;
         canonrule[p++] = 'e' ;
      } else {
         // output birth part
         canonrule[p++] = 'B' ;
         for (i = 0 ; i <= neighbors ; i++) {
            if (rulebits & (1 << i)) {
               canonrule[p++] = '0' + (char)i ;

               // check for non-totalistic
               if (!totalistic) {
                  // add any defined letters
                  np = addLetters(i, p) ;

                  // check if letters were added
                  if (np != p) {
                     if (np > p) {
                        stillnontotalistic = true ;
                     }
                     p = np ;
                  }
               }
            }
         }

         // add slash
         canonrule[p++] = '/' ;

         // output survival part
         canonrule[p++] = 'S' ;
         for (i = 0 ; i <= neighbors ; i++) {
            if (rulebits & (1 << (survival_offset+i))) {
               canonrule[p++] = '0' + (char)i ;

               // check for non-totalistic
               if (!totalistic) {
                  // add any defined letters
                  np = addLetters(survival_offset + i, p) ;

                  // check if letters were added
                  if (np != p) {
                     if (np > p) {
                        stillnontotalistic = true ;
                     }
                     p = np ;
                  }
               }
            }
         }
      }
   }

   // check if non-totalistic became totalistic
   if (!totalistic && !stillnontotalistic) {
      totalistic = true ;
   }

   // add neighborhood if not MAP rule
   if (!using_map) {
      if (neighbormask == HEXAGONAL) canonrule[p++] = 'H' ;
      if (neighbormask == VON_NEUMANN) canonrule[p++] = 'V' ;
   }

   // add the rule family postfix
   for (i = 0 ; i < (int) strlen(postfix) ; i++) {
      canonrule[p++] = postfix[i] ;
   }

   // check for bounded grid
   if (gridwd > 0 || gridht > 0) {
      // algo->setgridsize() was successfully called above, so append suffix
      const char* bounds = canonicalsuffix() ;
      i = 0 ;
      while (bounds[i]) canonrule[p++] = bounds[i++] ;
   }

   // null terminate
   canonrule[p] = 0 ;

   // set canonical rule
   ghashbase::setrule(canonrule) ;
}

// remove character from a string in place
void superalgo::removeChar(char *string, char skip) {
   int src = 0 ;
   int dst = 0 ;
   char c = string[src++] ;

   // copy characters other than skip
   while ( c ) {
      if (c != skip) {
         string[dst++] = c ;
      }
      c = string[src++] ;
   }

   // ensure null terminated
   string[dst] = 0 ;
}

// check whether non-totalistic letters are valid for defined neighbor counts
bool superalgo::lettersValid(const char *part) {
   char c ;
   char *cptr = NULL ;
   int nindex = 0 ;
   int currentCount = -1 ;
   bool negative = false ;
   bool usedNormal[9] ;
   bool usedNegative[9] ;

   // clear used flags
   for (int i = 0; i < 9; i += 1) {
      usedNormal[i] = false ;
      usedNegative[i] = false ;
   }

   // get next character
   while ( *part ) {
      c = *part ;
      if (c >= '0' && c <= '8') {
         currentCount = c - '0' ;
         nindex = currentCount - 1 ;
         if (nindex > 3) {
            nindex = 6 - nindex ;
         }
         negative = false ;
      } else {
         // check for minus
         if (c == '-') {
            negative = true ;
         } else {
            // not valid if 0 or 8
            if (currentCount == 0 || currentCount == 8) {
               return false ;
            }

            // check against valid rule letters for this neighbor count
            cptr = strchr((char*) rule_letters[nindex], c) ;
            if (cptr == NULL) {
               return false ;
            } else {
               // mark letter used at this neighbor count
               if (negative) {
                  usedNegative[currentCount] = true ;
               } else {
                  usedNormal[currentCount] = true ;
               }

               // check for both negative and positive used
               if (usedNormal[currentCount] && usedNegative[currentCount]) {
                  return false ;
               }
            }
         }
      }
      part++ ;
   }

   return true ;
}

// find postfix in rule string
const char *superalgo::findPostfix(const char *rulestring, const char *postfix) {
   const char *result = NULL ;
   int postfixlen = (int) strlen(postfix) ;

   // find the end of the rule ignoring any bounded grid definition
   const char *end = rulestring + strlen(rulestring) ;
   const char *colonpos = strchr(rulestring, ':') ;
   if (colonpos) end = colonpos ;

   // check the rule string is at least as long as the postfix
   if (end - rulestring >= postfixlen) {
      if (strncasecmp(end - postfixlen, postfix, postfixlen) == 0) {
         result = end - postfixlen ;
      }
   }

   return result ;
}

// setup B0 with Smax
void superalgo::setupB0Smax() {
   char tmp ;

   // B0 with Smax: rule -> NOT(reverse(bits))
   for (int i = 0 ; i < ALL3X3 / 2 ; i++) {
      tmp = rule3x3[i] ;
      rule3x3[i] = 1 - rule3x3[ALL3X3 - i - 1] ;
      rule3x3[ALL3X3 - i - 1] = 1 - tmp ;
   }
}

// set rule
const char *superalgo::setrule(const char *rulestring) {
   char *r = (char *)rulestring ;
   char tidystring[MAXRULESIZE] ;  // tidy version of rule string
   char *t = (char *)tidystring ;
   const char *end = r + strlen(r) ; // end of rule string
   char c ;
   char *charpos = 0 ;
   int digit ;
   int maxdigit = 0 ;              // maximum digit value found
   char *colonpos = 0 ;            // position of colon
   char *slashpos = 0 ;            // position of slash
   char *underscorepos = 0 ;       // poisition of underscore
   const char *postfixpos = 0 ;    // position of postfix
   const char *postfix = NULL ;    // which postfix is being used
   char *bpos = 0 ;                // position of b
   char *spos = 0 ;                // position of s
   rule_types rule = SUPER ;       // rule type

   // initialize rule type
   initRule() ;

   // check if rule is too long
   if (strlen(rulestring) > (size_t) MAXRULESIZE) {
      return "Rule name is too long." ;
   }

   // check for Super postfix
   postfix = SUPERPOSTFIX ;
   postfixpos = findPostfix(r, postfix) ;
   if (postfixpos) {
      // [R]Super rule
      end = postfixpos ;
      rule = SUPER ;
   } else {
      postfix = HISTORYPOSTFIX ;
      postfixpos = findPostfix(r, postfix) ;
      if (postfixpos) {
         // [R]History rule
         end = postfixpos ;
         rule = HISTORY ;
      } else {
         postfix = INVESTIGATORPOSTFIX ;
         postfixpos = findPostfix(r, postfix) ;
         if (postfixpos) {
            // [R]Investigator rule
            end = postfixpos ;
            rule = INVESTIGATOR ;
         } else {
            return "Missing Super, History or Investigator postfix." ;
         }
      }
   }

   // check for colon
   colonpos = strchr(r, ':') ;

   // skip any whitespace
   while (*r == ' ') {
      r++ ;
   }

   // check for Life
   if (strncasecmp(r, "life", 4) == 0) {
      r += 4 ;
      if (r != end) {
         c = (char)tolower(*r++);
         if (c == 'h') {
               neighbormask = HEXAGONAL ;
               neighbors = 6 ;
         } else {
            if (c == 'v') {
               neighbormask = VON_NEUMANN ;
               neighbors = 4 ;
            } else {
               return "Bad character found." ;
            }
         }
         if (r != end)
            return "Bad character found." ;
      }
      bpos = (char*) DEFAULTB ;
      spos = (char*) DEFAULTS ;
   } else {
      // check for map
      if (strncasecmp(r, "map", 3) == 0) {
         // attempt to decode map
         r += 3 ;
         bpos = r ;

         // check the length of the map
         int maplen = (int) (end - r) ;

         // check if there is base64 padding
         if (maplen > 2 && !strncmp(r + maplen - 2, "==", 2)) {
            // remove padding
            maplen -= 2 ;
         }

         // check if the map length is valid for Moore, Hexagonal or von Neumann neighborhoods
         if (!(maplen == MAP512LENGTH || maplen == MAP128LENGTH || maplen == MAP32LENGTH)) {
            return "MAP rule needs 6, 22 or 86 base64 characters." ;
         }

         // validate the characters
         spos = r + maplen ;
         while (r < spos) {
            if (!strchr(base64_characters, *r)) {
               return "MAP contains illegal base64 character." ;
            }
            r++ ;
         }

         // set the neighborhood based on the map length
         if (maplen == MAP128LENGTH) {
            neighbormask = HEXAGONAL ;
            neighbors = 6 ;
         } else {
            if (maplen == MAP32LENGTH) {
               neighbormask = VON_NEUMANN ;
               neighbors = 4 ;
            }
         }

         // skip base64 padding if present
         if (strncmp(r, "==", 2) == 0) {
            r += 2 ;
         }

         // check for postfix
         if (r == postfixpos) {
            r += strlen(postfix) ;
            if (*r && r != colonpos) {
               return "Illegal trailing characters after MAP." ;
            }
         } else {
            return "Badly positioned postfix." ;
         }

         // map looks valid
         using_map = true ;
      } else {
         // create lower case version of rule name without spaces
         while (r < end) {
            // get the next character and convert to lowercase
            c = (char) tolower(*r) ;

            // process the character
            switch (c) {
            // birth
            case 'b':
               if (bpos) {
                  // multiple b found
                  return "Only one B allowed." ;
               }
               bpos = t ;
               *t = c ;
               t++ ;
               break ;

            // survival
            case 's':
               if (spos) {
                  // multiple s found
                  return "Only one S allowed." ;
               }
               spos = t ;
               *t = c ;
               t++ ;
               break ;

            // slash
            case '/':
               if (slashpos) {
                  // multiple slashes found
                  return "Only one slash allowed." ;
               }
               slashpos = t ;
               *t = c ;
               t++ ;
               break ;

            // underscore
            case '_':
               if (underscorepos) {
                  // multiple underscores found
                  return "Only one underscore allowed." ;
               }
               underscorepos = t ;
               *t = c ;
               t++ ;
               break ;

            // hex
            case 'h':
               if (neighbormask != MOORE) {
                  // multiple neighborhoods specified
                  return "Only one neighborhood allowed." ;
               }
               neighbormask = HEXAGONAL ;
               neighbors = 6 ;
               *t = c ;
               t++ ;
               break ;

            // von neumann
            case 'v':
               if (neighbormask != MOORE) {
                  // multiple neighborhoods specified
                  return "Only one neighborhood allowed." ;
               }
               neighbormask = VON_NEUMANN ;
               neighbors = 4 ;
               *t = c ;
               t++ ;
               break ;

            // minus
            case '-':
               // check if previous character is a digit
               if (t == tidystring || *(t-1) < '0' || *(t-1) > '8') {
                  // minus can only follow a digit
                  return "Minus can only follow a digit." ;
               }
               *t = c ;
               t++ ;
               totalistic = false ;
               break ;

            // other characters
            default:
               // ignore space
               if (c != ' ') {
                  // check character is valid
                  charpos = strchr((char*) valid_rule_letters, c) ;
                  if (charpos) {
                     // copy character
                     *t = c ; 
                     t++ ;

                     // check if totalistic (i.e. found a valid non-digit)
                     digit = int(charpos - valid_rule_letters) ;
                     if (digit > 8) {
                        totalistic = false ;
                     } else {
                        // update maximum digit found
                        if (digit > maxdigit) {
                           maxdigit = digit ;
                        }
                     }
                  } else {
                     return "Bad character found." ;
                  }
               }
               break ;
            }

            // next character
            r++ ;
         }

         // ensure null terminated
         *t = 0 ;

         // don't allow empty rule string
         t = tidystring ;
         if (*t == 0) {
            return "Rule cannot be empty string." ;
         }

         // underscore only valid for non-totalistic rules
         if (underscorepos && totalistic) {
            return "Underscore not valid for totalistic rules, use slash." ;
         }

         // if neighborhood specified then must be last character
         if (neighbormask != MOORE) {
            size_t len = strlen(t) ;
            if (len) {
               c = t[len - 1] ;
               if (!((c == 'h') || (c == 'v'))) {
                  return "Neighborhood must be at end of rule." ;
               }
               // remove character
               t[len - 1] = 0 ;
            }
         }

         // at least one of slash, b or s must be present
         if (!(slashpos || bpos || spos)) {
            return "Rule must contain a slash or B or S." ;
         }

         // digits can not be greater than the number of neighbors for the defined neighborhood
         if (maxdigit > neighbors) {
            return "Digit greater than neighborhood allows." ;
         }

         // if slash present and both b and s then one must be each side of the slash
         if (slashpos && bpos && spos) {
            if ((bpos < slashpos && spos < slashpos) || (bpos > slashpos && spos > slashpos)) {
               return "B and S must be either side of slash." ;
            }
         }

         // if slash present and only one of b and s then the letter must be at the start or immediately after the slash
        if (slashpos && ((bpos && !spos) || (spos && !bpos))) {
            if (bpos) {
               if (!(bpos == tidystring || bpos == slashpos + 1)) {
                  return "B must be at start of rule or immediately after slash." ;
               }
            }
            else {
               if (!(spos == tidystring || spos == slashpos + 1)) {
                  return "S must be at start of rule or immediately after slash." ;
               }
            }
         }

         // check if there was a slash to divide birth from survival
         if (!slashpos) {
            // check if both b and s exist
            if (bpos && spos) {
               // determine whether b or s is first
               if (bpos < spos) {
                  // skip b and cut the string using s
                  removeChar(bpos, 'b') ;
                  *(spos - 1) = 0 ;
               } else {
                  // skip s and cut the string using b
                  removeChar(spos, 's') ;
                  *(bpos - 1) = 0 ;
               }
            } else {
               // just bpos
               if (bpos) {
                  if (bpos == t) {
                        // bpos at beginning of string so remove 'b' and set spos to the null at the end
                        removeChar(bpos, 'b') ;
                        spos = bpos + strlen(bpos) ;
                  }
                  else {
                        // bpos not at beginning so set spos to beginning and null the 'b' in bpos and increment
                        *bpos = 0 ;
                        bpos++ ;
                        spos = t ;
                  }
               }
               else {
                  // just spos
                  if (spos) {
                     if (spos == t) {
                           // spos at beginning of string so remove 's' and set bpos to the null at the end
                           removeChar(spos, 's') ;
                           bpos = spos + strlen(spos) ;
                     }
                     else {
                           // spos not at beginning so set bpos to beginning and null the 's' in spos and increment
                           *spos = 0 ;
                           spos++ ;
                           bpos = t ;
                     }
                  }
               }
            }
         } else {
            // slash exists so set determine which part is b and which is s
            *slashpos = 0 ;

            // check if b or s are defined
            if (bpos || spos) {
               // check for birth first
               if ((bpos && bpos < slashpos) || (spos && spos > slashpos)) {
                  // birth then survival
                  bpos = t ;
                  spos = slashpos + 1 ;
               } else {
                  // survival then birth
                  bpos = slashpos + 1 ;
                  spos = t ;
               }

               // remove b or s from rule parts
               removeChar(bpos, 'b') ;
               removeChar(spos, 's') ;
            } else {
               // no b or s so survival first
               spos = t ;
               bpos = slashpos + 1 ;
            }
         }

         // if not totalistic and a part exists it must start with a digit
         if (!totalistic) {
            // check birth
            c = *bpos ;
            if (c && (c < '0' || c > '8')) {
               return "Non-totalistic birth must start with a digit." ;
            }

            // check survival
            c = *spos ;
            if (c && (c < '0' || c > '8')) {
               return "Non-totalistic survival must start with a digit." ;
            }

            // one of birth or survival must be at the start of the rule
            if (!(bpos == tidystring || spos == tidystring)) {
               return "Invalid characters at start of rule." ;
            }
         }

         // if not totalistic then neighborhood must be Moore
         if (!totalistic && neighbormask != MOORE) {
            return "Non-totalistic only supported with Moore neighborhood." ;
         }

         // validate letters used against each specified neighbor count
         if (!lettersValid(bpos)) {
            return "Letter not valid for birth neighbor count." ;
         }

         if (!lettersValid(spos)) {
            return "Letter not valid for survival neighbor count." ;
         }
      }
   }

   // AKT: check for rule suffix like ":T200,100" to specify a bounded universe
   if (colonpos) {
      const char* err = setgridsize(colonpos) ;
      if (err) return err ;
   } else {
      // universe is unbounded
      gridwd = 0 ;
      gridht = 0 ;
   }

   // check for map
   if (using_map) {
      // generate the 3x3 map from the 512bit map
      createRuleMapFromMAP(bpos) ;
   } else {
      // generate the 3x3 map from the birth and survival rules
      createRuleMap(bpos, spos) ;
   }

   // check for B0
   if (rule3x3[0]) {
      // check for Smax
      if (rule3x3[ALL3X3 - 1]) {
         setupB0Smax() ;
      } else {
         switch (rule) {
            case HISTORY:
               return "History only supports B0 with Smax" ;
               break ;

            case SUPER:
               return "Super only supports B0 with Smax" ;
               break ;

            case INVESTIGATOR:
               return "Investigator only supports B0 with Smax" ;
               break ;
         }
      }
   }

   // save the canonical rule name
   if (strcmp(bpos, DEFAULTB) == 0 && strcmp(spos, DEFAULTS) == 0 && neighbormask == MOORE) {
      createCanonicalName(DEFAULTRULE, postfix) ;
   } else {
      createCanonicalName(bpos, postfix) ;
   }

   // setup number of states based on rule family
   switch (rule) {
      case HISTORY:
         maxCellStates = historyStates ;
         break ;

      case SUPER:
         maxCellStates = superStates ;
         break ;

      case INVESTIGATOR:
         maxCellStates = investigatorStates ;
         break ;
   }
   rule_type = rule ;

   // set grid_type
   if (neighbormask == HEXAGONAL)
      grid_type = HEX_GRID;
   else if (neighbormask == VON_NEUMANN)
      grid_type = VN_GRID;
   else // neighbormask == MOORE
      grid_type = SQUARE_GRID;

   // exit with success
   return 0 ;
}

const char* superalgo::getrule() {
   return canonrule ;
}

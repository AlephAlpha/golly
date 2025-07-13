// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _PREFS_H_
#define _PREFS_H_

#include <string>       // for std::string
#include <list>         // for std::list

#include "utils.h"      // for gColor

// Routines for loading and saving user preferences:

void GetPrefs();        // Read preferences from prefsfile.
void SavePrefs();       // Write preferences to prefsfile.

// Various constants:

const int minfontsize = 6;              // minimum value of helpfontsize
const int maxfontsize = 30;             // maximum value of helpfontsize

const int MAX_SPACING = 1000;           // maximum value of boldspacing
const int MAX_BASESTEP = 2000000000;    // maximum base step
const int MAX_DELAY = 5000;             // maximum mindelay or maxdelay
const int MAX_RECENT = 100;             // maximum value of maxpatterns
const int MIN_MEM_MB = 10;              // minimum value of maxhashmem
const int MAX_MEM_MB = 500;             // maximum value of maxhashmem
const int MAX_SENSITIVITY = 20;         // maximum value of wheelsens

// These global paths must be set in platform-specific code before GetPrefs is called:

extern std::string supplieddir;     // path of parent directory for supplied help/patterns/rules
extern std::string helpdir;         // path of directory for supplied help
extern std::string patternsdir;     // path of directory for supplied patterns
extern std::string rulesdir;        // path of directory for supplied rules
extern std::string userdir;         // path of parent directory for user's rules/patterns/downloads
extern std::string userrules;       // path of directory for user's rules
extern std::string savedir;         // path of directory for user's saved patterns
extern std::string downloaddir;     // path of directory for user's downloaded files
extern std::string tempdir;         // path of directory for temporary data
extern std::string clipfile;        // path of temporary file for storing clipboard data
extern std::string prefsfile;       // path of file for storing user's preferences

// Global preference data:

extern int debuglevel;              // for displaying debug info if > 0
extern int helpfontsize;            // font size in help window
extern char initrule[];             // initial rule
extern bool initautofit;            // initial autofit setting
extern bool inithyperspeed;         // initial hyperspeed setting
extern bool initshowhashinfo;       // initial showhashinfo setting
extern bool autostop;               // automatically stop generating if pattern dies?
extern bool savexrle;               // save RLE file using XRLE format?
extern bool showtool;               // show tool bar?
extern bool showlayer;              // show layer bar?
extern bool showedit;               // show edit bar?
extern bool showallstates;          // show all cell states in edit bar?
extern bool showstatus;             // show status bar?
extern bool showexact;              // show exact numbers in status bar?
extern bool showxystate;            // show state of cell at XY location?
extern bool showtimeline;           // show timeline bar?
extern bool showtiming;             // show timing messages?
extern bool showgridlines;          // display grid lines?
extern bool showicons;              // display icons for cell states?
extern bool swapcolors;             // swap colors used for cell states?
extern bool allowundo;              // allow undo/redo?
extern bool allowbeep;              // okay to play beep sound?
extern bool restoreview;            // should reset/undo restore view?
extern int canchangerule;           // if > 0 then paste can change rule
extern int randomfill;              // random fill percentage
extern int opacity;                 // percentage opacity of live cells in overlays
extern int tileborder;              // width of tiled window borders
extern int mingridmag;              // minimum mag to draw grid lines
extern int boldspacing;             // spacing of bold grid lines
extern bool showboldlines;          // show bold grid lines?
extern bool mathcoords;             // show Y values increasing upwards?
extern bool syncviews;              // synchronize viewports?
extern bool syncmodes;              // synchronize touch modes?
extern bool stacklayers;            // stack all layers?
extern bool tilelayers;             // tile all layers?
extern bool asktosave;              // ask to save changes?
extern int newmag;                  // mag setting for new pattern
extern bool newremovesel;           // new pattern removes selection?
extern bool openremovesel;          // opening pattern removes selection?
extern int mindelay;                // minimum millisec delay
extern int maxdelay;                // maximum millisec delay
extern int maxhashmem;              // maximum memory (in MB) for hashlife-based algos
extern int wheelsens;               // mouse wheel sensitivity

// Recent patterns:

extern int numpatterns;             // current number of recent pattern files
extern int maxpatterns;             // maximum number of recent pattern files
extern std::list<std::string> recentpatterns; // list of recent pattern files

// Colors:

extern gColor borderrgb;            // color for border around bounded grid
extern gColor selectrgb;            // color for selected cells
extern gColor pastergb;             // color for pattern to be pasted

// Paste info:

typedef enum {
    And, Copy, Or, Xor              // logical paste modes
} paste_mode;

extern paste_mode pmode;            // current paste mode

const char* GetPasteMode();         // get pmode
void SetPasteMode(const char* s);   // set pmode

#endif

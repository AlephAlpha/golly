// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h" // for wxToolTip
#endif
#include "wx/stdpaths.h"    // for wxStandardPaths
#include "wx/filename.h"    // for wxFileName
#include "wx/propdlg.h"     // for wxPropertySheetDialog
#include "wx/colordlg.h"    // for wxColourDialog
#include "wx/bookctrl.h"    // for wxBookCtrlBase
#include "wx/notebook.h"    // for wxNotebookEvent
#include "wx/spinctrl.h"    // for wxSpinCtrl
#include "wx/image.h"       // for wxImage

#include "lifealgo.h"
#include "liferules.h"      // for MAXRULESIZE
#include "viewport.h"       // for MAX_MAG
#include "util.h"           // for linereader

#include "wxgolly.h"        // for wxGetApp, mainptr, viewptr
#include "wxmain.h"         // for ID_*, mainptr->...
#include "wxview.h"         // for viewptr->..., glMajor, etc
#include "wxutils.h"        // for Warning, Fatal, Beep, FillRect
#include "wxhelp.h"         // for GetHelpFrame
#include "wxinfo.h"         // for GetInfoFrame
#include "wxalgos.h"        // for InitAlgorithms, NumAlgos, algoinfo, etc
#include "wxrender.h"       // for DrawOneIcon
#include "wxlayer.h"        // for currlayer, UpdateLayerColors
#include "wxscript.h"       // for inscript
#include "wxprefs.h"

// cursor bitmaps
#include "bitmaps/pick_curs.xpm"
#include "bitmaps/hand_curs.xpm"
#include "bitmaps/zoomin_curs.xpm"
#include "bitmaps/zoomout_curs.xpm"
#ifdef __WXMSW__
    #include "bitmaps/cross_curs.xpm"
#endif

// -----------------------------------------------------------------------------

// Golly's preferences file is a simple text file.  It's initially created in
// a user-specific data directory (datadir) but we look in the application
// directory (gollydir) first because this makes uninstalling simple and allows
// multiple copies/versions of the app to have separate preferences.

const wxString PREFS_NAME = wxT("GollyPrefs");

wxString prefspath;              // full path to prefs file

// location of supplied scripts (relative to app)
const wxString SCRIPT_DIR = wxT("Scripts");

const int PREFS_VERSION = 4;     // increment if necessary due to changes in syntax/semantics
int currversion = PREFS_VERSION; // might be changed by prefs_version
const int PREF_LINE_SIZE = 5000; // must be quite long for storing file paths

// initialize exported preferences:

wxString gollydir;               // path of directory containing app
wxString datadir;                // path of directory for user-specific data
wxString tempdir;                // path of directory for temporary data
wxString rulesdir;               // path of directory for app's rule data
wxString userrules;              // path of directory for user's rule data
wxString downloaddir;            // path of directory for downloaded data
wxString userdir;                // path of directory added by user

int debuglevel = 0;              // for displaying debug info if > 0

int mainx = 30;                  // main window's initial location
int mainy = 40;
int mainwd = 800;                // main window's initial size
int mainht = 600;
bool maximize = false;           // maximize main window?

int helpx = 60;                  // help window's initial location
int helpy = 60;
int helpwd = 700;                // help window's initial size
int helpht = 500;
#ifdef __WXMAC__
    int helpfontsize = 12;       // font size in help window
#else
    int helpfontsize = 10;       // font size in help window
#endif

int infox = 90;                  // info window's initial location
int infoy = 90;
int infowd = 700;                // info window's initial size
int infoht = 500;

int rulex = 200;                 // rule dialog's initial location
int ruley = 200;
int ruleexwd = 500;              // rule dialog's initial extra size
int ruleexht = 200;
bool showalgohelp = false;       // show algorithm help in rule dialog?

char initrule[MAXRULESIZE] = "B3/S23"; // initial rule
bool initautofit = false;        // initial autofit setting
bool inithyperspeed = false;     // initial hyperspeed setting
bool initshowhashinfo = false;   // initial showhashinfo setting
bool showpopulation = true;      // show population counts while generating?
bool autostop = false;           // automatically stop generating if pattern dies?
bool savexrle = true;            // save RLE file using XRLE format?
bool showtips = true;            // show button tips?
bool showtool = true;            // show tool bar?
bool showlayer = false;          // show layer bar?
bool showedit = true;            // show edit bar?
bool showallstates = false;      // show all cell states in edit bar?
bool showstatus = true;          // show status bar?
bool showexact = false;          // show exact numbers in status bar?
bool showxystate = false;        // show state of cell at XY location?
bool showscrollbars = true;      // show scroll bars?
bool showtimeline = false;       // show timeline bar?
bool showgridlines = true;       // display grid lines?
bool showoverlay = false;        // display the current overlay?
bool showicons = false;          // display icons for cell states?
bool smartscale = false;         // smarter scaling when zoomed out?
bool swapcolors = false;         // swap colors used for cell states?
bool scrollpencil = true;        // scroll if pencil cursor is dragged outside view?
bool scrollcross = true;         // scroll if cross cursor is dragged outside view?
bool scrollhand = true;          // scroll if hand cursor is dragged outside view?
bool allowundo = true;           // allow undo/redo?
bool allowbeep = true;           // okay to play beep sound?
bool showselpop = false;         // should selection population be shown?
bool restoreview = true;         // should reset/undo restore view?
int controlspos = 1;             // position of translucent controls (1 is top left corner)
int canchangerule = 0;           // if > 0 then paste can change rule
int randomfill = 50;             // random fill percentage (1..100)
int opacity = 50;                // percentage opacity of live cells in overlays (1..100)
int tileborder = 3;              // thickness of tiled window borders
int mingridmag = 2;              // minimum mag to draw grid lines
int boldspacing = 10;            // spacing of bold grid lines
bool showboldlines = true;       // show bold grid lines?
bool mathcoords = false;         // show Y values increasing upwards?
bool cellborders = true;         // should zoomed cells have borders?
bool syncviews = false;          // synchronize viewports?
bool synccursors = true;         // synchronize cursors?
bool stacklayers = false;        // stack all layers?
bool tilelayers = false;         // tile all layers?
bool askonnew = true;            // ask to save changes before creating new pattern?
bool askonload = true;           // ask to save changes before loading pattern file?
bool askondelete = true;         // ask to save changes before deleting layer?
bool askonquit = true;           // ask to save changes before quitting app?
bool warn_on_save = true;        // warn if saving non-starting generation?
int newmag = MAX_MAG;            // mag setting for new pattern
bool newremovesel = true;        // new pattern removes selection?
bool openremovesel = true;       // opening pattern removes selection?
wxCursor* newcurs = NULL;        // cursor after creating new pattern (if not NULL)
wxCursor* opencurs = NULL;       // cursor after opening pattern (if not NULL)
int mousewheelmode = 2;          // 0:Ignore, 1:forward=ZoomOut, 2:forward=ZoomIn
int wheelsens = MAX_SENSITIVITY; // mouse wheel sensitivity
int thumbrange = 10;             // thumb box scrolling range in terms of view wd/ht
int mindelay = 250;              // minimum millisec delay
int maxdelay = 2000;             // maximum millisec delay
wxString opensavedir;            // directory for Open and Save dialogs
wxString overlaydir;             // directory for Save Overlay dialog
wxString rundir;                 // directory for Run Script dialog
wxString choosedir;              // directory used by Choose File button
wxString filedir;                // directory used by Show Files
wxString texteditor;             // path of user's preferred text editor
wxString pythonlib;              // name of Python library (loaded at runtime)
int dirwinwd = 180;              // width of directory window
bool showfiles = true;           // show file directory?
wxMenu* patternSubMenu = NULL;   // submenu of recent pattern files
wxMenu* scriptSubMenu = NULL;    // submenu of recent script files
int numpatterns = 0;             // current number of recent pattern files
int numscripts = 0;              // current number of recent script files
int maxpatterns = 20;            // maximum number of recent pattern files (1..MAX_RECENT)
int maxscripts = 20;             // maximum number of recent script files (1..MAX_RECENT)
wxArrayString namedrules;        // initialized in GetPrefs

wxColor* borderrgb;              // color for border around bounded grid
wxColor* selectrgb;              // color for selected cells
wxColor* pastergb;               // color for pasted pattern

// these settings must be global -- they are changed by GetPrefs *before* the
// view window is created
paste_location plocation = TopLeft;
paste_mode pmode = Or;

// these must be global -- they are created before the view window is created
wxCursor* curs_pencil;           // for drawing cells
wxCursor* curs_pick;             // for picking cell states
wxCursor* curs_cross;            // for selecting cells
wxCursor* curs_hand;             // for moving view by dragging
wxCursor* curs_zoomin;           // for zooming in to a clicked cell
wxCursor* curs_zoomout;          // for zooming out from a clicked cell
wxCursor* curs_wait;             // for indicating a lengthy task is in progress
wxCursor* curs_hidden;           // for hiding cursor when mouse is in overlay

// local (ie. non-exported) globals:

int mingridindex;                // mingridmag - 2
int newcursindex;
int opencursindex;

#if defined(__WXMAC__) && wxCHECK_VERSION(2,9,0)
    // wxMOD_CONTROL has been changed to mean Command key down (sheesh!)
    #define wxMOD_CONTROL wxMOD_RAW_CONTROL
    #define ControlDown RawControlDown
#endif

// set of modifier keys (note that MSVC didn't like MK_*)
const int mk_CMD     = 1;        // command key on Mac, control key on Win/Linux
const int mk_ALT     = 2;        // option key on Mac
const int mk_SHIFT   = 4;
#ifdef __WXMAC__
const int mk_CTRL    = 8;        // control key is separate modifier on Mac
const int MAX_MODS   = 16;
#else
const int MAX_MODS   = 8;
#endif

// WXK_* key codes like WXK_F1 have values > 300, so to minimize the
// size of the keyaction table (see below) we use our own internal
// key codes for function keys and other special keys
const int IK_HOME       = 1;
const int IK_END        = 2;
const int IK_PAGEUP     = 3;
const int IK_PAGEDOWN   = 4;
const int IK_HELP       = 5;
const int IK_INSERT     = 6;
const int IK_DELETE     = 8;     // treat delete like backspace (consistent with GSF_dokey)
const int IK_TAB        = 9;
const int IK_RETURN     = 13;
const int IK_LEFT       = 28;
const int IK_RIGHT      = 29;
const int IK_UP         = 30;
const int IK_DOWN       = 31;
const int IK_F1         = 'A';   // we use shift+a for the real A, etc
const int IK_F24        = 'X';
const int MAX_KEYCODES  = 128;

// names of the non-displayable keys we currently support;
// note that these names can be used in menu item accelerator strings
// so they must match legal wx names (listed in wxMenu::Append docs)
const char NK_HOME[]    = "Home";
const char NK_END[]     = "End";
const char NK_PGUP[]    = "PgUp";
const char NK_PGDN[]    = "PgDn";
const char NK_HELP[]    = "Help";
const char NK_INSERT[]  = "Insert";
const char NK_DELETE[]  = "Delete";
const char NK_TAB[]     = "Tab";
#ifdef __WXMSW__
const char NK_RETURN[]  = "Enter";
#else
const char NK_RETURN[]  = "Return";
#endif
const char NK_LEFT[]    = "Left";
const char NK_RIGHT[]   = "Right";
const char NK_UP[]      = "Up";
const char NK_DOWN[]    = "Down";
const char NK_SPACE[]   = "Space";

const action_info nullaction = { DO_NOTHING, wxEmptyString };

// table for converting key combinations into actions
action_info keyaction[MAX_KEYCODES][MAX_MODS] = {{ nullaction }};

// strings for setting menu item accelerators
wxString accelerator[MAX_ACTIONS];

// -----------------------------------------------------------------------------

bool ConvertKeyAndModifiers(int wxkey, int wxmods, int* newkey, int* newmods)
{
    // first convert given wx modifiers (set by wxKeyEvent::GetModifiers)
    // to a corresponding set of mk_* values
    int ourmods = 0;
    if (wxmods & wxMOD_CMD)       ourmods |= mk_CMD;
    if (wxmods & wxMOD_ALT)       ourmods |= mk_ALT;
    if (wxmods & wxMOD_SHIFT)     ourmods |= mk_SHIFT;
#ifdef __WXMAC__
    if (wxmods & wxMOD_CONTROL)   ourmods |= mk_CTRL;
#endif

    // now convert given wx key code to corresponding IK_* code
    int ourkey;
    if (wxkey >= 'A' && wxkey <= 'Z') {
        // convert A..Z to shift+a..shift+z so we can use A..X
        // for our internal function keys (IK_F1 to IK_F24)
        ourkey = wxkey + 32;
        ourmods |= mk_SHIFT;

    } else if (wxkey >= WXK_F1 && wxkey <= WXK_F24) {
        // convert wx function key code to IK_F1..IK_F24
        ourkey = IK_F1 + (wxkey - WXK_F1);

    } else if (wxkey >= WXK_NUMPAD0 && wxkey <= WXK_NUMPAD9) {
        // treat numpad digits like ordinary digits
        ourkey = '0' + (wxkey - WXK_NUMPAD0);

    } else {
        switch (wxkey) {
            case WXK_HOME:          ourkey = IK_HOME; break;
            case WXK_END:           ourkey = IK_END; break;
            case WXK_PAGEUP:        ourkey = IK_PAGEUP; break;
            case WXK_PAGEDOWN:      ourkey = IK_PAGEDOWN; break;
            case WXK_HELP:          ourkey = IK_HELP; break;
            case WXK_INSERT:        ourkey = IK_INSERT; break;
            case WXK_BACK:          // treat backspace like delete
            case WXK_DELETE:        ourkey = IK_DELETE; break;
            case WXK_TAB:           ourkey = IK_TAB; break;
            case WXK_NUMPAD_ENTER:  // treat enter like return
            case WXK_RETURN:        ourkey = IK_RETURN; break;
            case WXK_LEFT:          ourkey = IK_LEFT; break;
            case WXK_RIGHT:         ourkey = IK_RIGHT; break;
            case WXK_UP:            ourkey = IK_UP; break;
            case WXK_DOWN:          ourkey = IK_DOWN; break;
            case WXK_ADD:           ourkey = '+'; break;
            case WXK_SUBTRACT:      ourkey = '-'; break;
            case WXK_DIVIDE:        ourkey = '/'; break;
            case WXK_MULTIPLY:      ourkey = '*'; break;
            default:                ourkey = wxkey;
        }
    }

    if (ourkey < 0 || ourkey >= MAX_KEYCODES) return false;

    *newkey = ourkey;
    *newmods = ourmods;
    return true;
}

// -----------------------------------------------------------------------------

action_info FindAction(int wxkey, int wxmods)
{
    // convert given wx key code and modifier set to our internal values
    // and return the corresponding action
    int ourkey, ourmods;
    if ( ConvertKeyAndModifiers(wxkey, wxmods, &ourkey, &ourmods) ) {
        return keyaction[ourkey][ourmods];
    } else {
        return nullaction;
    }
}

// -----------------------------------------------------------------------------

void AddDefaultKeyActions()
{
    // these default shortcuts are similar to the hard-wired shortcuts in v1.2

    // include some examples of DO_OPENFILE
#ifdef __WXMSW__
    keyaction[(int)'g'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'g'][mk_ALT].file =     wxT("Scripts\\Lua\\toSuper.lua");
    keyaction[(int)'h'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'h'][mk_ALT].file =     wxT("Scripts\\Lua\\toHistory.lua");
    keyaction[(int)'i'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'i'][mk_ALT].file =     wxT("Scripts\\Lua\\toInvestigator.lua");
    keyaction[(int)'j'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'j'][mk_ALT].file =     wxT("Scripts\\Lua\\toStandard.lua");
    keyaction[(int)'k'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'k'][mk_ALT].file =     wxT("Scripts\\Lua\\toChangeState.lua");
    keyaction[(int)'l'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'l'][mk_ALT].file =     wxT("Help\\Lexicon\\lex.htm");
    keyaction[(int)'s'][mk_SHIFT].id =     DO_OPENFILE;
    keyaction[(int)'s'][mk_SHIFT].file =   wxT("Scripts\\Lua\\shift.lua");
    keyaction[(int)'o'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'o'][mk_ALT].file =     wxT("Scripts\\Lua\\safeopenclip.lua");
#else
    keyaction[(int)'g'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'g'][mk_ALT].file =     wxT("Scripts/Lua/toSuper.lua");
    keyaction[(int)'h'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'h'][mk_ALT].file =     wxT("Scripts/Lua/toHistory.lua");
    keyaction[(int)'i'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'i'][mk_ALT].file =     wxT("Scripts/Lua/toInvestigator.lua");
    keyaction[(int)'j'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'j'][mk_ALT].file =     wxT("Scripts/Lua/toStandard.lua");
    keyaction[(int)'k'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'k'][mk_ALT].file =     wxT("Scripts/Lua/toChangeState.lua");
    keyaction[(int)'l'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'l'][mk_ALT].file =     wxT("Help/Lexicon/lex.htm");
    keyaction[(int)'s'][mk_SHIFT].id =     DO_OPENFILE;
    keyaction[(int)'s'][mk_SHIFT].file =   wxT("Scripts/Lua/shift.lua");
    keyaction[(int)'o'][mk_ALT].id =       DO_OPENFILE;
    keyaction[(int)'o'][mk_ALT].file =     wxT("Scripts/Lua/safeopenclip.lua");
#endif

    // File menu
    keyaction[(int)'n'][mk_CMD].id =    DO_NEWPATT;
    keyaction[(int)'o'][mk_CMD].id =    DO_OPENPATT;
    keyaction[(int)'o'][mk_SHIFT+mk_CMD].id = DO_OPENCLIP;
    keyaction[(int)'s'][mk_CMD].id =    DO_SAVE;
#ifdef __WXMSW__
    // Windows does not support ctrl+non-alphanumeric
#else
    keyaction[(int)','][mk_CMD].id =    DO_PREFS;
#endif
    keyaction[(int)','][0].id =         DO_PREFS;
    keyaction[(int)'q'][mk_CMD].id =    DO_QUIT;

    // Edit menu
    keyaction[(int)'z'][0].id =         DO_UNDO;
    keyaction[(int)'z'][mk_CMD].id =    DO_UNDO;
    keyaction[(int)'z'][mk_SHIFT].id =  DO_REDO;
    keyaction[(int)'z'][mk_SHIFT+mk_CMD].id = DO_REDO;
    keyaction[(int)'x'][mk_CMD].id =    DO_CUT;
    keyaction[(int)'c'][mk_CMD].id =    DO_COPY;
    keyaction[IK_DELETE][0].id =        DO_CLEAR;
    keyaction[IK_DELETE][mk_SHIFT].id = DO_CLEAROUT;
    keyaction[(int)'v'][0].id =         DO_PASTE;
    keyaction[(int)'v'][mk_CMD].id =    DO_PASTE;
    keyaction[(int)'m'][mk_SHIFT].id =  DO_PASTEMODE;
    keyaction[(int)'l'][mk_SHIFT].id =  DO_PASTELOC;
    keyaction[(int)'a'][0].id =         DO_SELALL;
    keyaction[(int)'a'][mk_CMD].id =    DO_SELALL;
    keyaction[(int)'k'][0].id =         DO_REMOVESEL;
    keyaction[(int)'k'][mk_CMD].id =    DO_REMOVESEL;
    keyaction[(int)'s'][0].id =         DO_SHRINKFIT;
    keyaction[(int)'5'][mk_CMD].id =    DO_RANDFILL;
    keyaction[(int)'y'][0].id =         DO_FLIPTB;
    keyaction[(int)'x'][0].id =         DO_FLIPLR;
    keyaction[(int)'>'][0].id =         DO_ROTATECW;
    keyaction[(int)'<'][0].id =         DO_ROTATEACW;
    keyaction[IK_F1+1][0].id =          DO_CURSDRAW;
    keyaction[IK_F1+2][0].id =          DO_CURSPICK;
    keyaction[IK_F1+3][0].id =          DO_CURSSEL;
    keyaction[IK_F1+4][0].id =          DO_CURSMOVE;
    keyaction[IK_F1+5][0].id =          DO_CURSIN;
    keyaction[IK_F1+6][0].id =          DO_CURSOUT;
    keyaction[(int)'c'][0].id =         DO_CURSCYCLE;

    // Control menu
    keyaction[IK_RETURN][0].id =        DO_STARTSTOP;
    keyaction[(int)' '][0].id =         DO_NEXTGEN;
    keyaction[IK_TAB][0].id =           DO_NEXTSTEP;
    keyaction[(int)'r'][mk_CMD].id =    DO_RESET;
    keyaction[(int)'+'][0].id =         DO_FASTER;
    keyaction[(int)'+'][mk_SHIFT].id =  DO_FASTER;
    keyaction[(int)'='][0].id =         DO_FASTER;
    keyaction[(int)'_'][0].id =         DO_SLOWER;
    keyaction[(int)'_'][mk_SHIFT].id =  DO_SLOWER;
    keyaction[(int)'-'][0].id =         DO_SLOWER;
    keyaction[(int)'t'][0].id =         DO_AUTOFIT;
    keyaction[(int)'t'][mk_CMD].id =    DO_AUTOFIT;
    keyaction[(int)'u'][mk_CMD].id =    DO_HASHING;
#ifdef __WXMAC__
    keyaction[(int)' '][mk_CTRL].id =   DO_ADVANCE;
#else
    // on Windows/Linux mk_CMD is control key
    keyaction[(int)' '][mk_CMD].id =    DO_ADVANCE;
#endif
    keyaction[(int)' '][mk_SHIFT].id =  DO_ADVANCEOUT;
    keyaction[(int)'t'][mk_SHIFT].id =  DO_TIMING;

    // View menu
    keyaction[IK_LEFT][0].id =          DO_LEFT;
    keyaction[IK_RIGHT][0].id =         DO_RIGHT;
    keyaction[IK_UP][0].id =            DO_UP;
    keyaction[IK_DOWN][0].id =          DO_DOWN;
    keyaction[IK_F1+10][0].id =         DO_FULLSCREEN;
    keyaction[(int)'f'][0].id =         DO_FIT;
    keyaction[(int)'f'][mk_CMD].id =    DO_FIT;
    keyaction[(int)'f'][mk_SHIFT].id =  DO_FITSEL;
    keyaction[(int)'f'][mk_SHIFT+mk_CMD].id = DO_FITSEL;
    keyaction[(int)'m'][0].id =         DO_MIDDLE;
    keyaction[(int)'m'][mk_CMD].id =    DO_MIDDLE;
    keyaction[(int)'0'][0].id =         DO_CHANGE00;
    keyaction[(int)'9'][0].id =         DO_RESTORE00;
    keyaction[(int)'9'][mk_CMD].id =    DO_RESTORE00;
    keyaction[(int)']'][0].id =         DO_ZOOMIN;
    keyaction[(int)'['][0].id =         DO_ZOOMOUT;
#ifdef __WXMSW__
    // Windows does not support ctrl+non-alphanumeric
#else
    keyaction[(int)']'][mk_CMD].id =    DO_ZOOMIN;
    keyaction[(int)'['][mk_CMD].id =    DO_ZOOMOUT;
#endif
    keyaction[(int)'1'][0].id =         DO_SCALE1;
    keyaction[(int)'2'][0].id =         DO_SCALE2;
    keyaction[(int)'4'][0].id =         DO_SCALE4;
    keyaction[(int)'8'][0].id =         DO_SCALE8;
    keyaction[(int)'6'][0].id =         DO_SCALE16;
    keyaction[(int)'3'][0].id =         DO_SCALE32;
    keyaction[(int)'\''][0].id =        DO_SHOWTOOL;
    keyaction[(int)'\\'][0].id =        DO_SHOWLAYER;
    keyaction[(int)'/'][0].id =         DO_SHOWEDIT;
    keyaction[(int)'.'][0].id =         DO_SHOWSTATES;
    keyaction[(int)';'][0].id =         DO_SHOWSTATUS;
#ifdef __WXMSW__
    // Windows does not support ctrl+non-alphanumeric
#else
    keyaction[(int)'\''][mk_CMD].id =   DO_SHOWTOOL;
    keyaction[(int)'\\'][mk_CMD].id =   DO_SHOWLAYER;
    keyaction[(int)'/'][mk_CMD].id =    DO_SHOWEDIT;
    keyaction[(int)'.'][mk_CMD].id =    DO_SHOWSTATES;
    keyaction[(int)';'][mk_CMD].id =    DO_SHOWSTATUS;
#endif
    keyaction[(int)'e'][0].id =         DO_SHOWEXACT;
    keyaction[(int)'e'][mk_CMD].id =    DO_SHOWEXACT;
    keyaction[(int)'l'][0].id =         DO_SHOWGRID;
    keyaction[(int)'l'][mk_CMD].id =    DO_SHOWGRID;
    keyaction[(int)'b'][0].id =         DO_INVERT;
    keyaction[(int)'b'][mk_CMD].id =    DO_INVERT;
    keyaction[(int)'i'][0].id =         DO_INFO;
    keyaction[(int)'i'][mk_CMD].id =    DO_INFO;

    // Layer menu
    // none

    // Help menu
    keyaction[(int)'h'][0].id =         DO_HELP;
    keyaction[(int)'?'][0].id =         DO_HELP;
    keyaction[IK_HELP][0].id =          DO_HELP;
#ifdef __WXMAC__
    // cmd-? is the usual shortcut in Mac apps
    keyaction[(int)'?'][mk_CMD].id =    DO_HELP;
    // we can only detect shift+cmd+/ so we have to assume '?' is above '/' -- yuk
    keyaction[(int)'/'][mk_SHIFT+mk_CMD].id = DO_HELP;
#else
    // F1 is the usual shortcut in Win/Linux apps
    keyaction[IK_F1][0].id =            DO_HELP;
#endif
}

// -----------------------------------------------------------------------------

const char* GetActionName(action_id action)
{
    switch (action) {
        case DO_NOTHING:        return "NONE";
        case DO_OPENFILE:       return "Open:";
        // File menu
        case DO_NEWPATT:        return "New Pattern";
        case DO_OPENPATT:       return "Open Pattern...";
        case DO_OPENCLIP:       return "Open Clipboard";
        case DO_OPENRECENT:     return "Open Recent Pattern";
        case DO_SHOWFILES:      return "Show Files";
        case DO_SETFOLDER:      return "Set Folder...";
        case DO_SAVE:           return "Save Pattern...";
        case DO_SAVEXRLE:       return "Save Extended RLE";
        case DO_RUNSCRIPT:      return "Run Script...";
        case DO_RUNCLIP:        return "Run Clipboard";
        case DO_RUNRECENT:      return "Run Recent Script";
        case DO_PREFS:          return "Preferences...";
        case DO_QUIT:           return "Quit Golly";
        // Edit menu
        case DO_UNDO:           return "Undo";
        case DO_REDO:           return "Redo";
        case DO_DISABLE:        return "Disable Undo/Redo";
        case DO_CUT:            return "Cut Selection";
        case DO_COPY:           return "Copy Selection";
        case DO_CLEAR:          return "Clear Selection";
        case DO_CLEAROUT:       return "Clear Outside";
        case DO_PASTE:          return "Paste";
        case DO_PASTEMODE:      return "Cycle Paste Mode";
        case DO_PASTELOC:       return "Cycle Paste Location";
        case DO_PASTESEL:       return "Paste to Selection";
        case DO_SELALL:         return "Select All";
        case DO_REMOVESEL:      return "Remove Selection";
        case DO_SHRINK:         return "Shrink Selection";
        case DO_SHRINKFIT:      return "Shrink and Fit";
        case DO_RANDFILL:       return "Random Fill";
        case DO_FLIPTB:         return "Flip Top-Bottom";
        case DO_FLIPLR:         return "Flip Left-Right";
        case DO_ROTATECW:       return "Rotate Clockwise";
        case DO_ROTATEACW:      return "Rotate Anticlockwise";
        case DO_CURSDRAW:       return "Cursor Mode: Draw";
        case DO_CURSPICK:       return "Cursor Mode: Pick";
        case DO_CURSSEL:        return "Cursor Mode: Select";
        case DO_CURSMOVE:       return "Cursor Mode: Move";
        case DO_CURSIN:         return "Cursor Mode: Zoom In";
        case DO_CURSOUT:        return "Cursor Mode: Zoom Out";
        case DO_CURSCYCLE:      return "Cycle Cursor Mode";
        // Control menu
        case DO_STARTSTOP:      return "Start/Stop Generating";
        case DO_NEXTGEN:        return "Next Generation";
        case DO_NEXTSTEP:       return "Next Step";
        case DO_NEXTHIGHER:     return "Next Higher State";
        case DO_NEXTLOWER:      return "Next Lower State";
        case DO_RESET:          return "Reset";
        case DO_SETGEN:         return "Set Generation...";
        case DO_FASTER:         return "Faster";
        case DO_SLOWER:         return "Slower";
        case DO_SETBASE:        return "Set Base Step...";
        case DO_AUTOFIT:        return "Auto Fit";
        case DO_HASHING:        return "Use Hashing";
        case DO_HYPER:          return "Hyperspeed";
        case DO_HASHINFO:       return "Show Hash Info";
        case DO_SHOWPOP:        return "Show Population";
        case DO_AUTOSTOP:       return "Stop If Dead";
        case DO_RECORD:         return "Start/Stop Recording";
        case DO_DELTIME:        return "Delete Timeline";
        case DO_PLAYBACK:       return "Play Timeline Backwards";
        case DO_SETRULE:        return "Set Rule...";
        case DO_ADVANCE:        return "Advance Selection";
        case DO_ADVANCEOUT:     return "Advance Outside";
        case DO_TIMING:         return "Show Timing";
        // View menu
        case DO_LEFT:           return "Scroll Left";
        case DO_RIGHT:          return "Scroll Right";
        case DO_UP:             return "Scroll Up";
        case DO_DOWN:           return "Scroll Down";
        case DO_NE:             return "Scroll NE";
        case DO_NW:             return "Scroll NW";
        case DO_SE:             return "Scroll SE";
        case DO_SW:             return "Scroll SW";
        case DO_FULLSCREEN:     return "Full Screen";
        case DO_FIT:            return "Fit Pattern";
        case DO_FITSEL:         return "Fit Selection";
        case DO_MIDDLE:         return "Middle";
        case DO_CHANGE00:       return "Change Origin";
        case DO_RESTORE00:      return "Restore Origin";
        case DO_ZOOMIN:         return "Zoom In";
        case DO_ZOOMOUT:        return "Zoom Out";
        case DO_SCALE1:         return "Set Scale 1:1";
        case DO_SCALE2:         return "Set Scale 1:2";
        case DO_SCALE4:         return "Set Scale 1:4";
        case DO_SCALE8:         return "Set Scale 1:8";
        case DO_SCALE16:        return "Set Scale 1:16";
        case DO_SCALE32:        return "Set Scale 1:32";
        case DO_SMARTSCALE:     return "Smarter Scaling";
        case DO_SHOWTOOL:       return "Show Tool Bar";
        case DO_SHOWLAYER:      return "Show Layer Bar";
        case DO_SHOWEDIT:       return "Show Edit Bar";
        case DO_SHOWSTATES:     return "Show All States";
        case DO_SHOWSCROLL:     return "Show Scroll Bars";
        case DO_SHOWSTATUS:     return "Show Status Bar";
        case DO_SHOWEXACT:      return "Show Exact Numbers";
        case DO_SHOWXYSTATE:    return "Show XY State";
        case DO_SETCOLORS:      return "Set Layer Colors...";
        case DO_SHOWICONS:      return "Show Cell Icons";
        case DO_INVERT:         return "Invert Colors";
        case DO_SHOWGRID:       return "Show Grid Lines";
        case DO_SHOWTIME:       return "Show Timeline";
        case DO_INFO:           return "Pattern Info";
        // Layer menu
        case DO_SAVEOVERLAY:    return "Save Overlay...";
        case DO_SHOWOVERLAY:    return "Show Overlay";
        case DO_DELOVERLAY:     return "Delete Overlay";
        case DO_ADD:            return "Add Layer";
        case DO_CLONE:          return "Clone Layer";
        case DO_DUPLICATE:      return "Duplicate Layer";
        case DO_DELETE:         return "Delete Layer";
        case DO_DELOTHERS:      return "Delete Other Layers";
        case DO_MOVELAYER:      return "Move Layer...";
        case DO_NAMELAYER:      return "Name Layer...";
        case DO_SYNCVIEWS:      return "Synchronize Views";
        case DO_SYNCCURS:       return "Synchronize Cursors";
        case DO_STACK:          return "Stack Layers";
        case DO_TILE:           return "Tile Layers";
        // Help menu
        case DO_HELP:           return "Show Help";
        case DO_ABOUT:          return "About Golly";
        default:                Warning(_("Bug detected in GetActionName!"));
    }
    return "BUG";
}

// -----------------------------------------------------------------------------

// for case-insensitive string comparison
#ifdef __WXMSW__
    #define ISTRCMP stricmp
#else
    #define ISTRCMP strcasecmp
#endif

void GetKeyAction(char* value)
{
    // parse strings like "z undo" or "space+ctrl advance selection";
    // note that some errors detected here can be Fatal because the user
    // has to quit Golly anyway to edit the prefs file
    char* start = value;
    char* p = start;
    int modset = 0;
    int key = -1;

    // extract key, skipping first char in case it's '+'
    if (*p > 0) p++;
    while (1) {
        if (*p == 0) {
            Fatal(wxString::Format(_("Bad key_action value: %s"),
                                   wxString(value,wxConvLocal).c_str()));
        }
        if (*p == ' ' || *p == '+') {
            // we found end of key
            char oldp = *p;
            *p = 0;
            int len = (int)strlen(start);
            if (len == 1) {
                key = start[0];
                if (key < ' ' || key > '~') {
                    // this can happen if the user's language setting is not English,
                    // so change key and continue rather than call Fatal
                    Warning(wxString::Format(_("Non-displayable key in key_action: %s"),
                                             wxString(start,wxConvLocal).c_str()));
                    key = '!';
                }
                if (key >= 'A' && key <= 'Z') {
                    // convert A..Z to shift+a..shift+z so we can use A..X
                    // for our internal function keys (IK_F1 to IK_F24)
                    key += 32;
                    modset |= mk_SHIFT;
                }
            } else if (len > 1) {
                if ((start[0] == 'f' || start[0] == 'F') && start[1] >= '1' && start[1] <= '9') {
                    // we have a function key
                    char* p = &start[1];
                    int num;
                    sscanf(p, "%d", &num);
                    if (num >= 1 && num <= 24) key = IK_F1 + num - 1;
                } else {
                    if      (ISTRCMP(start, NK_HOME) == 0)    key = IK_HOME;
                    else if (ISTRCMP(start, NK_END) == 0)     key = IK_END;
                    else if (ISTRCMP(start, NK_PGUP) == 0)    key = IK_PAGEUP;
                    else if (ISTRCMP(start, NK_PGDN) == 0)    key = IK_PAGEDOWN;
                    else if (ISTRCMP(start, NK_HELP) == 0)    key = IK_HELP;
                    else if (ISTRCMP(start, NK_INSERT) == 0)  key = IK_INSERT;
                    else if (ISTRCMP(start, NK_DELETE) == 0)  key = IK_DELETE;
                    else if (ISTRCMP(start, NK_TAB) == 0)     key = IK_TAB;
                    else if (ISTRCMP(start, NK_RETURN) == 0)  key = IK_RETURN;
                    else if (ISTRCMP(start, NK_LEFT) == 0)    key = IK_LEFT;
                    else if (ISTRCMP(start, NK_RIGHT) == 0)   key = IK_RIGHT;
                    else if (ISTRCMP(start, NK_UP) == 0)      key = IK_UP;
                    else if (ISTRCMP(start, NK_DOWN) == 0)    key = IK_DOWN;
                    else if (ISTRCMP(start, NK_SPACE) == 0)   key = ' ';
                }
                if (key < 0)
                    Fatal(wxString::Format(_("Unknown key in key_action: %s"),
                                           wxString(start,wxConvLocal).c_str()));
            }
            *p = oldp;     // restore ' ' or '+'
            start = p;
            start++;
            break;
        }
        p++;
    }

    // *p is ' ' or '+' so extract zero or more modifiers
    while (*p != ' ') {
        p++;
        if (*p == 0) {
            Fatal(wxString::Format(_("No action in key_action value: %s"),
                                   wxString(value,wxConvLocal).c_str()));
        }
        if (*p == ' ' || *p == '+') {
            // we found end of modifier
            char oldp = *p;
            *p = 0;
#ifdef __WXMAC__
            if      (ISTRCMP(start, "cmd") == 0)   modset |= mk_CMD;
            else if (ISTRCMP(start, "opt") == 0)   modset |= mk_ALT;
            else if (ISTRCMP(start, "ctrl") == 0)  modset |= mk_CTRL;
#else
            if      (ISTRCMP(start, "ctrl") == 0)  modset |= mk_CMD;
            else if (ISTRCMP(start, "alt") == 0)   modset |= mk_ALT;
#endif
            else if    (ISTRCMP(start, "shift") == 0) modset |= mk_SHIFT;
            else
                Fatal(wxString::Format(_("Unknown modifier in key_action: %s"),
                                       wxString(start,wxConvLocal).c_str()));
            *p = oldp;     // restore ' ' or '+'
            start = p;
            start++;
        }
    }

    // *p is ' ' so skip and check the action string
    p++;
    action_info action = nullaction;

    // first look for "Open:" followed by file path
    if (strncmp(p, "Open:", 5) == 0) {
        action.id = DO_OPENFILE;
        action.file = wxString(&p[5], wxConvLocal);
    } else {
        // assume DO_NOTHING is 0 and start with action 1
        for (int i = 1; i < MAX_ACTIONS; i++) {
            if (strcmp(p, GetActionName((action_id) i)) == 0) {
                action.id = (action_id) i;
                break;
            }
        }
    }

    // test for some deprecated actions
    if (action.id == DO_NOTHING) {
        if (strcmp(p, "Swap Cell Colors") == 0) action.id = DO_INVERT;
    }

    keyaction[key][modset] = action;
}

// -----------------------------------------------------------------------------

wxString GetKeyCombo(int key, int modset)
{
    // build a key combo string for display in prefs dialog and help window
    wxString result = wxEmptyString;

#ifdef __WXMAC__
    if (mk_ALT & modset)    result += wxT("Option-");
    if (mk_SHIFT & modset)  result += wxT("Shift-");
    if (mk_CTRL & modset)   result += wxT("Control-");
    if (mk_CMD & modset)    result += wxT("Command-");
#else
    if (mk_ALT & modset)    result += wxT("Alt+");
    if (mk_SHIFT & modset)  result += wxT("Shift+");
    if (mk_CMD & modset)    result += wxT("Control+");
#endif

    if (key >= IK_F1 && key <= IK_F24) {
        // function key
        result += wxString::Format(wxT("F%d"), key - IK_F1 + 1);

    } else if (key >= 'a' && key <= 'z') {
        // display A..Z rather than a..z
        result += wxChar(key - 32);

    } else if (key > ' ' && key <= '~') {
        // displayable char, but excluding space (that's handled below)
        result += wxChar(key);

    } else {
        // non-displayable char
        switch (key) {
            // these strings can be more descriptive than the NK_* strings
            case IK_HOME:     result += _("Home"); break;
            case IK_END:      result += _("End"); break;
            case IK_PAGEUP:   result += _("PageUp"); break;
            case IK_PAGEDOWN: result += _("PageDown"); break;
            case IK_HELP:     result += _("Help"); break;
            case IK_INSERT:   result += _("Insert"); break;
            case IK_DELETE:   result += _("Delete"); break;
            case IK_TAB:      result += _("Tab"); break;
#ifdef __WXMSW__
            case IK_RETURN:   result += _("Enter"); break;
#else
            case IK_RETURN:   result += _("Return"); break;
#endif
            case IK_LEFT:     result += _("Left"); break;
            case IK_RIGHT:    result += _("Right"); break;
            case IK_UP:       result += _("Up"); break;
            case IK_DOWN:     result += _("Down"); break;
            case ' ':         result += _("Space"); break;
            default:          result = wxEmptyString;
        }
    }

    return result;
}

// -----------------------------------------------------------------------------

wxString GetShortcutTable()
{
    // return HTML data to display current keyboard shortcuts in help window
    wxString result;
    result += _("<html><title>Golly Help: Keyboard Shortcuts</title>");
    result += _("<body bgcolor=\"#FFFFCE\">");
    result += _("<p><font size=+1><b>Keyboard shortcuts</b></font>");
    result += _("<p>Use <a href=\"prefs:keyboard\">Preferences > Keyboard</a>");
    result += _(" to change the following keyboard shortcuts:");
    result += _("<p><center>");
    result += _("<table cellspacing=1 border=2 cols=2 width=\"90%\">");
    result += _("<tr><td align=center>Key Combination</td><td align=center>Action</td></tr>");

    bool assigned[MAX_ACTIONS] = {false};

    for (int key = 0; key < MAX_KEYCODES; key++) {
        for (int modset = 0; modset < MAX_MODS; modset++) {
            action_info action = keyaction[key][modset];
            if ( action.id != DO_NOTHING ) {
                assigned[action.id] = true;
                wxString keystring = GetKeyCombo(key, modset);
                if (key == '<') {
                    keystring.Replace(_("<"), _("&lt;"));
                }
                result += _("<tr><td align=right>");
                result += keystring;
                result += _("&nbsp;</td><td>&nbsp;");
                result += wxString(GetActionName(action.id), wxConvLocal);
                if (action.id == DO_OPENFILE) {
                    result += _("&nbsp;");
                    result += action.file;
                }
                result += _("</td></tr>");
            }
        }
    }

    result += _("</table></center>");

    // also list unassigned actions
    result += _("<p>The following actions currently have no keyboard shortcuts:<p>");
    for (int i = 1; i < MAX_ACTIONS; i++) {
        if (!assigned[i]) {
            wxString name = wxString(GetActionName((action_id) i),wxConvLocal);
            result += wxString::Format(_("<dd>%s</dd>"), name.c_str());
        }
    }

    result += _("</body></html>");
    return result;
}

// -----------------------------------------------------------------------------

wxString GetModifiers(int modset)
{
    wxString modkeys = wxEmptyString;
#ifdef __WXMAC__
    if (mk_ALT & modset)    modkeys += wxT("+opt");
    if (mk_SHIFT & modset)  modkeys += wxT("+shift");
    if (mk_CTRL & modset)   modkeys += wxT("+ctrl");
    if (mk_CMD & modset)    modkeys += wxT("+cmd");
#else
    if (mk_ALT & modset)    modkeys += wxT("+alt");
    if (mk_SHIFT & modset)  modkeys += wxT("+shift");
    if (mk_CMD & modset)    modkeys += wxT("+ctrl");
#endif
    return modkeys;
}

// -----------------------------------------------------------------------------

wxString GetKeyName(int key)
{
    wxString result;

    if (key >= IK_F1 && key <= IK_F24) {
        // function key
        result.Printf(wxT("F%d"), key - IK_F1 + 1);

    } else if (key > ' ' && key <= '~') {
        // displayable char, but excluding space (that's handled below)
        result = wxChar(key);

    } else {
        // non-displayable char
        switch (key) {
            case IK_HOME:     result = wxString(NK_HOME, wxConvLocal); break;
            case IK_END:      result = wxString(NK_END, wxConvLocal); break;
            case IK_PAGEUP:   result = wxString(NK_PGUP, wxConvLocal); break;
            case IK_PAGEDOWN: result = wxString(NK_PGDN, wxConvLocal); break;
            case IK_HELP:     result = wxString(NK_HELP, wxConvLocal); break;
            case IK_INSERT:   result = wxString(NK_INSERT, wxConvLocal); break;
            case IK_DELETE:   result = wxString(NK_DELETE, wxConvLocal); break;
            case IK_TAB:      result = wxString(NK_TAB, wxConvLocal); break;
            case IK_RETURN:   result = wxString(NK_RETURN, wxConvLocal); break;
            case IK_LEFT:     result = wxString(NK_LEFT, wxConvLocal); break;
            case IK_RIGHT:    result = wxString(NK_RIGHT, wxConvLocal); break;
            case IK_UP:       result = wxString(NK_UP, wxConvLocal); break;
            case IK_DOWN:     result = wxString(NK_DOWN, wxConvLocal); break;
            case ' ':         result = wxString(NK_SPACE, wxConvLocal); break;
            default:          result = wxEmptyString;
        }
    }

    return result;
}

// -----------------------------------------------------------------------------

void SaveKeyActions(FILE* f)
{
    bool assigned[MAX_ACTIONS] = {false};

    fputs("\n", f);
    for (int key = 0; key < MAX_KEYCODES; key++) {
        for (int modset = 0; modset < MAX_MODS; modset++) {
            action_info action = keyaction[key][modset];
            if ( action.id != DO_NOTHING ) {
                assigned[action.id] = true;
                fprintf(f, "key_action=%s%s %s%s\n",
                        (const char*) GetKeyName(key).mb_str(wxConvLocal),
                        (const char*) GetModifiers(modset).mb_str(wxConvLocal),
                        GetActionName(action.id),
                        (const char*) action.file.mb_str(wxConvLocal));
            }
        }
    }

    // list all unassigned actions in comment lines
    fputs("# unassigned actions:\n", f);
    for (int i = 1; i < MAX_ACTIONS; i++) {
        if ( !assigned[i] ) {
            fprintf(f, "# key_action=key+mods %s", GetActionName((action_id) i));
            if ( i == DO_OPENFILE ) fputs("file", f);
            fputs("\n", f);
        }
    }
    fputs("\n", f);
}

// -----------------------------------------------------------------------------

void CreateAccelerator(action_id action, int modset, int key)
{
    accelerator[action] = wxT("\t");
#ifdef __WXMAC__
    if (modset & mk_CTRL) accelerator[action] += wxT("RawCtrl+");
#endif
    if (modset & mk_CMD) accelerator[action] += wxT("Ctrl+");
    if (modset & mk_ALT) accelerator[action] += wxT("Alt+");
    if (modset & mk_SHIFT) accelerator[action] += wxT("Shift+");
    if (key >= 'a' && key <= 'z') {
        // convert a..z to A..Z
        accelerator[action] += wxChar(key - 32);
#ifdef __WXMAC__
    } else if (key == IK_DELETE) {
        // must use "Back" to get correct symbol (<+ rather than +>)
        accelerator[action] += wxT("Back");
#endif
    } else {
        accelerator[action] += GetKeyName(key);
    }
}

// -----------------------------------------------------------------------------

void UpdateAcceleratorStrings()
{
    for (int i = 0; i < MAX_ACTIONS; i++)
        accelerator[i] = wxEmptyString;

    // go thru keyaction table looking for key combos that are valid menu item
    // accelerators and construct suitable strings like "\tCtrl+Alt+Shift+K"
    // or "\tF12" or "\tReturn" etc
    for (int key = 0; key < MAX_KEYCODES; key++) {
        for (int modset = 0; modset < MAX_MODS; modset++) {
            action_info info = keyaction[key][modset];
            action_id action = info.id;
            if (action != DO_NOTHING && accelerator[action].IsEmpty()) {
                // check if key can be used as an accelerator
                if ((key >= ' ' && key <= '~') ||
                    (key >= IK_F1 && key <= IK_F24) ||
                    (key >= IK_LEFT && key <= IK_DOWN) ||
                    key == IK_HOME ||
                    key == IK_END ||
                    key == IK_PAGEUP ||
                    key == IK_PAGEDOWN ||
                    key == IK_DELETE ||
                    key == IK_TAB ||
                    key == IK_RETURN ) {
                    CreateAccelerator(action, modset, key);
                }
            }
        }
    }

    // go thru keyaction table again looking only for key combos containing Ctrl;
    // we do this so that the Paste menu item will have the standard Ctrl+V
    // shortcut rather than a plain V if both those shortcuts are assigned
    for (int key = 0; key < MAX_KEYCODES; key++) {
        for (int modset = 0; modset < MAX_MODS; modset++) {
            action_info info = keyaction[key][modset];
            action_id action = info.id;
            if (action != DO_NOTHING && (modset & mk_CMD)) {
                CreateAccelerator(action, modset, key);
            }
        }
    }
}

// -----------------------------------------------------------------------------

wxString GetAccelerator(action_id action)
{
    return accelerator[action];
}

// -----------------------------------------------------------------------------

void RemoveAccelerator(wxMenuBar* mbar, int item, action_id action)
{
    if (!accelerator[action].IsEmpty()) {
        // remove accelerator from given menu item
        wxString label = wxMenuItem::GetLabelText(mbar->GetLabel(item));
        label.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
        mbar->SetLabel(item, label);
    }
}

// -----------------------------------------------------------------------------

void SetAccelerator(wxMenuBar* mbar, int item, action_id action)
{
    wxString accel = accelerator[action];

    if (inscript) {
        // RunScript has called mainptr->UpdateMenuAccelerators()
        // so remove accelerator from menu item to allow keyboard shortcuts
        // to be passed to script
        if (accel.IsEmpty()) return;
        if (action == DO_STARTSTOP) {
            // don't remove Escape from "Stop Script" menu item
            return;
        } else {
            accel = wxEmptyString;
        }
    } else if (viewptr->waitingforclick) {
        // PatternView::PasteTemporaryToCurrent has called mainptr->UpdateMenuAccelerators()
        // so remove accelerator to allow keyboard shortcuts while waiting for paste click
        if (accel.IsEmpty()) return;
        accel = wxEmptyString;
    }

    // we need to remove old accelerator string from GetLabel text
    wxString label = wxMenuItem::GetLabelText(mbar->GetLabel(item));
    label.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
    mbar->SetLabel(item, label + accel);
}

// -----------------------------------------------------------------------------

void CreateCursors()
{
    curs_pencil = new wxCursor(wxCURSOR_PENCIL);
    if (curs_pencil == NULL) Fatal(_("Failed to create pencil cursor!"));

    wxBitmap bitmap_pick = XPM_BITMAP(pick_curs);
    wxImage image_pick = bitmap_pick.ConvertToImage();
    image_pick.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 0);
    image_pick.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 15);
    curs_pick = new wxCursor(image_pick);
    if (curs_pick == NULL) Fatal(_("Failed to create pick cursor!"));

#ifdef __WXMSW__
    // don't use wxCURSOR_CROSS because it disappears on black background
    wxBitmap bitmap_cross = XPM_BITMAP(cross_curs);
    wxImage image_cross = bitmap_cross.ConvertToImage();
    image_cross.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 8);
    image_cross.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 8);
    curs_cross = new wxCursor(image_cross);
#else
    curs_cross = new wxCursor(wxCURSOR_CROSS);
#endif
    if (curs_cross == NULL) Fatal(_("Failed to create cross cursor!"));

    wxBitmap bitmap_hand = XPM_BITMAP(hand_curs);
    wxImage image_hand = bitmap_hand.ConvertToImage();
    image_hand.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 8);
    image_hand.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 8);
    curs_hand = new wxCursor(image_hand);
    if (curs_hand == NULL) Fatal(_("Failed to create hand cursor!"));

    wxBitmap bitmap_zoomin = XPM_BITMAP(zoomin_curs);
    wxImage image_zoomin = bitmap_zoomin.ConvertToImage();
    image_zoomin.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 6);
    image_zoomin.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 6);
    curs_zoomin = new wxCursor(image_zoomin);
    if (curs_zoomin == NULL) Fatal(_("Failed to create zoomin cursor!"));

    wxBitmap bitmap_zoomout = XPM_BITMAP(zoomout_curs);
    wxImage image_zoomout = bitmap_zoomout.ConvertToImage();
    image_zoomout.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 6);
    image_zoomout.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 6);
    curs_zoomout = new wxCursor(image_zoomout);
    if (curs_zoomout == NULL) Fatal(_("Failed to create zoomout cursor!"));

    curs_wait = new wxCursor(wxCURSOR_WAIT);
    if (curs_wait == NULL) Fatal(_("Failed to create wait cursor!"));

    curs_hidden = new wxCursor(wxCURSOR_BLANK);
    if (curs_hidden == NULL) Fatal(_("Failed to create hidden cursor!"));

    // default cursors for new pattern or after opening pattern
    newcurs = curs_pencil;
    opencurs = curs_zoomin;
}

// -----------------------------------------------------------------------------

void FreeCursors()
{
    delete curs_pencil;
    delete curs_pick;
    delete curs_cross;
    delete curs_hand;
    delete curs_zoomin;
    delete curs_zoomout;
    delete curs_wait;
    delete curs_hidden;
}

// -----------------------------------------------------------------------------

const char* CursorToString(wxCursor* curs)
{
    if (curs == curs_pencil)   return "Draw";
    if (curs == curs_pick)     return "Pick";
    if (curs == curs_cross)    return "Select";
    if (curs == curs_hand)     return "Move";
    if (curs == curs_zoomin)   return "Zoom In";
    if (curs == curs_zoomout)  return "Zoom Out";
    return "No Change";   // curs is NULL
}

// -----------------------------------------------------------------------------

wxCursor* StringToCursor(const char* s)
{
    if (strcmp(s, "Draw") == 0)      return curs_pencil;
    if (strcmp(s, "Pick") == 0)      return curs_pick;
    if (strcmp(s, "Select") == 0)    return curs_cross;
    if (strcmp(s, "Move") == 0)      return curs_hand;
    if (strcmp(s, "Zoom In") == 0)   return curs_zoomin;
    if (strcmp(s, "Zoom Out") == 0)  return curs_zoomout;
    return NULL;   // "No Change"
}

// -----------------------------------------------------------------------------

int CursorToIndex(wxCursor* curs)
{
    if (curs == curs_pencil)   return 0;
    if (curs == curs_pick)     return 1;
    if (curs == curs_cross)    return 2;
    if (curs == curs_hand)     return 3;
    if (curs == curs_zoomin)   return 4;
    if (curs == curs_zoomout)  return 5;
    return 6;   // curs is NULL
}

// -----------------------------------------------------------------------------

wxCursor* IndexToCursor(int i)
{
    if (i == 0) return curs_pencil;
    if (i == 1) return curs_pick;
    if (i == 2) return curs_cross;
    if (i == 3) return curs_hand;
    if (i == 4) return curs_zoomin;
    if (i == 5) return curs_zoomout;
    return NULL;   // "No Change"
}

// -----------------------------------------------------------------------------

// following routines cannot be PatternView methods -- they are called by
// GetPrefs() before the view window is created

const char* GetPasteLocation()
{
    switch (plocation) {
        case TopLeft:     return "TopLeft";
        case TopRight:    return "TopRight";
        case BottomRight: return "BottomRight";
        case BottomLeft:  return "BottomLeft";
        case Middle:      return "Middle";
        default:          return "unknown";
    }
}

// -----------------------------------------------------------------------------

void SetPasteLocation(const char* s)
{
    if (strcmp(s, "TopLeft") == 0) {
        plocation = TopLeft;
    } else if (strcmp(s, "TopRight") == 0) {
        plocation = TopRight;
    } else if (strcmp(s, "BottomRight") == 0) {
        plocation = BottomRight;
    } else if (strcmp(s, "BottomLeft") == 0) {
        plocation = BottomLeft;
    } else {
        plocation = Middle;
    }
}

// -----------------------------------------------------------------------------

const char* GetPasteMode()
{
    switch (pmode) {
        case And:   return "And";
        case Copy:  return "Copy";
        case Or:    return "Or";
        case Xor:   return "Xor";
        default:    return "unknown";
    }
}

// -----------------------------------------------------------------------------

void SetPasteMode(const char* s)
{
    if (strcmp(s, "And") == 0) {
        pmode = And;
    } else if (strcmp(s, "Copy") == 0) {
        pmode = Copy;
    } else if (strcmp(s, "Or") == 0) {
        pmode = Or;
    } else {
        pmode = Xor;
    }
}

// -----------------------------------------------------------------------------

void UpdateStatusBrushes()
{
    for (int i = 0; i < NumAlgos(); i++) {
        algoinfo[i]->statusbrush->SetColour(algoinfo[i]->statusrgb);
    }
}

// -----------------------------------------------------------------------------

void CreateDefaultColors()
{
    borderrgb  = new wxColor(128, 128, 128);  // 50% gray
    selectrgb  = new wxColor( 75, 175,   0);  // dark green (will be 50% transparent)
    pastergb   = new wxColor(255,   0,   0);  // red

    // set default status brushes (in case prefs file doesn't exist)
    UpdateStatusBrushes();
}

// -----------------------------------------------------------------------------

void FreeDefaultColors()
{
    delete borderrgb;
    delete selectrgb;
    delete pastergb;
}

// -----------------------------------------------------------------------------

void GetColor(const char* value, wxColor* rgb)
{
    unsigned int r, g, b;
    sscanf(value, "%u,%u,%u", &r, &g, &b);
    rgb->Set(r, g, b);
}

// -----------------------------------------------------------------------------

void SaveColor(FILE* f, const char* name, const wxColor* rgb)
{
    fprintf(f, "%s=%d,%d,%d\n", name, rgb->Red(), rgb->Green(), rgb->Blue());
}

// -----------------------------------------------------------------------------

void GetRelPath(const char* value, wxString& path,
                const wxString& defdir = wxEmptyString,
                bool isdir = true)
{
    path = wxString(value, wxConvLocal);
    wxFileName fname(path);

    if (currversion < 4 && fname.IsAbsolute() && defdir.length() > 0) {
        // if old version's absolute path ends with defdir then update
        // path so new version will see correct dir
        wxString suffix = wxFILE_SEP_PATH + defdir;
        if (path.EndsWith(suffix)) {
            path = gollydir + defdir;
            // nicer if directory path ends with separator
            if (isdir && path.Last() != wxFILE_SEP_PATH) path += wxFILE_SEP_PATH;
            return;
        }
    }

    // if path isn't absolute then prepend Golly directory
    if (!fname.IsAbsolute()) path = gollydir + path;

    // if path doesn't exist then reset to default directory
    if (!wxFileName::DirExists(path)) path = gollydir + defdir;

    // nicer if directory path ends with separator
    if (isdir && path.Last() != wxFILE_SEP_PATH) path += wxFILE_SEP_PATH;
}

// -----------------------------------------------------------------------------

void SaveRelPath(FILE* f, const char* name, wxString path)
{
    // if given path is inside Golly directory then save as a relative path
    if (path.StartsWith(gollydir)) {
        // remove gollydir from start of path
        path.erase(0, gollydir.length());
    }
    fprintf(f, "%s=%s\n", name, (const char*)path.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

#define STRINGIFY(arg) STR2(arg)
#define STR2(arg) #arg
const char* GOLLY_VERSION = STRINGIFY(VERSION);

void SavePrefs()
{
    if (mainptr == NULL || currlayer == NULL) {
        // should never happen but play safe
        return;
    }

#ifdef __WXMAC__
    // we need to convert prefspath to decomposed UTF8 so fopen will work
    FILE* f = fopen(prefspath.fn_str(), "w");
#else
    FILE* f = fopen(prefspath.mb_str(wxConvLocal), "w");
#endif
    if (f == NULL) {
        Warning(_("Could not save preferences file!"));
        return;
    }

    fprintf(f, "# NOTE: If you edit this file then do so when Golly isn't running\n");
    fprintf(f, "# otherwise all your changes will be clobbered when Golly quits.\n\n");
    fprintf(f, "prefs_version=%d\n", PREFS_VERSION);
    fprintf(f, "golly_version=%s\n", GOLLY_VERSION);
    wxString wxversion = wxVERSION_STRING;
    fprintf(f, "wx_version=%s\n", (const char*)wxversion.mb_str(wxConvLocal));
    fprintf(f, "opengl_version=%d.%d, glMaxTextureSize=%d\n", glMajor, glMinor, glMaxTextureSize);
#if defined(__WXMAC__)
    fprintf(f, "platform=Mac\n");
#elif defined(__WXMSW__)
    fprintf(f, "platform=Windows\n");
#elif defined(__WXGTK__)
    fprintf(f, "platform=Linux\n");
#else
    fprintf(f, "platform=unknown\n");
#endif
    fprintf(f, "debug_level=%d\n", debuglevel);

    SaveKeyActions(f);

    // save main window's location and size
#ifdef __WXMSW__
    if (mainptr->fullscreen || mainptr->IsIconized()) {
        // use mainx, mainy, mainwd, mainht set by mainptr->ToggleFullScreen()
        // or by mainptr->OnSize
    }
#else
    if (mainptr->fullscreen) {
        // use mainx, mainy, mainwd, mainht set by mainptr->ToggleFullScreen()
    }
#endif
    else
    {
        wxRect r = mainptr->GetRect();
        mainx = r.x;
        mainy = r.y;
        mainwd = r.width;
        mainht = r.height;
    }
    fprintf(f, "main_window=%d,%d,%d,%d\n", mainx, mainy, mainwd, mainht);
    fprintf(f, "maximize=%d\n", mainptr->IsMaximized() ? 1 : 0);

#ifdef __WXMSW__
    if (GetHelpFrame() && !GetHelpFrame()->IsIconized())
#else
    if (GetHelpFrame())
#endif
    {
        wxRect r = GetHelpFrame()->GetRect();
        helpx = r.x;
        helpy = r.y;
        helpwd = r.width;
        helpht = r.height;
    }
    fprintf(f, "help_window=%d,%d,%d,%d\n", helpx, helpy, helpwd, helpht);
    fprintf(f, "help_font_size=%d (%d..%d)\n", helpfontsize, minfontsize, maxfontsize);

#ifdef __WXMSW__
    if (GetInfoFrame() && !GetInfoFrame()->IsIconized())
#else
    if (GetInfoFrame())
#endif
    {
        wxRect r = GetInfoFrame()->GetRect();
        infox = r.x;
        infoy = r.y;
        infowd = r.width;
        infoht = r.height;
    }
    fprintf(f, "info_window=%d,%d,%d,%d\n", infox, infoy, infowd, infoht);
    fprintf(f, "rule_dialog=%d,%d,%d,%d\n", rulex, ruley, ruleexwd, ruleexht);
    fprintf(f, "show_algo_help=%d\n", showalgohelp ? 1 : 0);

    fprintf(f, "allow_undo=%d\n", allowundo ? 1 : 0);
    fprintf(f, "allow_beep=%d\n", allowbeep ? 1 : 0);
    fprintf(f, "show_selpop=%d\n", showselpop ? 1 : 0);
    fprintf(f, "restore_view=%d\n", restoreview ? 1 : 0);
    fprintf(f, "paste_location=%s\n", GetPasteLocation());
    fprintf(f, "paste_mode=%s\n", GetPasteMode());
    fprintf(f, "scroll_pencil=%d\n", scrollpencil ? 1 : 0);
    fprintf(f, "scroll_cross=%d\n", scrollcross ? 1 : 0);
    fprintf(f, "scroll_hand=%d\n", scrollhand ? 1 : 0);
    fprintf(f, "controls_pos=%d (0..4)\n", controlspos);
    fprintf(f, "can_change_rule=%d (0..2)\n", canchangerule);
    fprintf(f, "random_fill=%d (1..100)\n", randomfill);
    fprintf(f, "min_delay=%d (0..%d millisecs)\n", mindelay, MAX_DELAY);
    fprintf(f, "max_delay=%d (0..%d millisecs)\n", maxdelay, MAX_DELAY);
    fprintf(f, "auto_fit=%d\n", currlayer->autofit ? 1 : 0);
    fprintf(f, "hyperspeed=%d\n", currlayer->hyperspeed ? 1 : 0);
    fprintf(f, "hash_info=%d\n", currlayer->showhashinfo ? 1 : 0);
    fprintf(f, "show_population=%d\n", showpopulation ? 1 : 0);
    fprintf(f, "autostop=%d\n", autostop ? 1 : 0);

    fputs("\n", f);

    fprintf(f, "init_algo=%s\n", GetAlgoName(currlayer->algtype));
    for (int i = 0; i < NumAlgos(); i++) {
        fputs("\n", f);
        fprintf(f, "algorithm=%s\n", GetAlgoName(i));
        fprintf(f, "max_mem=%d\n", algoinfo[i]->algomem);
        fprintf(f, "base_step=%d\n", algoinfo[i]->defbase);
        SaveColor(f, "status_rgb", &algoinfo[i]->statusrgb);
        SaveColor(f, "from_rgb", &algoinfo[i]->fromrgb);
        SaveColor(f, "to_rgb", &algoinfo[i]->torgb);
        fprintf(f, "use_gradient=%d\n", algoinfo[i]->gradient ? 1 : 0);
        fputs("colors=", f);
        for (int state = 0; state < algoinfo[i]->maxstates; state++) {
            // only write out state,r,g,b tuple if color is different to default
            if (algoinfo[i]->algor[state] != algoinfo[i]->defr[state] ||
                algoinfo[i]->algog[state] != algoinfo[i]->defg[state] ||
                algoinfo[i]->algob[state] != algoinfo[i]->defb[state] ) {
                fprintf(f, "%d,%d,%d,%d,", state, algoinfo[i]->algor[state],
                        algoinfo[i]->algog[state],
                        algoinfo[i]->algob[state]);
            }
        }
        fputs("\n", f);
    }

    fputs("\n", f);

    fprintf(f, "rule=%s\n", currlayer->algo->getrule());
    if (namedrules.GetCount() > 1) {
        size_t i;
        for (i=1; i<namedrules.GetCount(); i++)
            fprintf(f, "named_rule=%s\n", (const char*)namedrules[i].mb_str(wxConvLocal));
    }

    fputs("\n", f);

    fprintf(f, "show_tips=%d\n", showtips ? 1 : 0);
    fprintf(f, "show_tool=%d\n", showtool ? 1 : 0);
    fprintf(f, "show_layer=%d\n", showlayer ? 1 : 0);
    fprintf(f, "show_edit=%d\n", showedit ? 1 : 0);
    fprintf(f, "show_states=%d\n", showallstates ? 1 : 0);
    fprintf(f, "show_status=%d\n", showstatus ? 1 : 0);
    fprintf(f, "show_exact=%d\n", showexact ? 1 : 0);
    fprintf(f, "show_xystate=%d\n", showxystate ? 1 : 0);
    fprintf(f, "show_scrollbars=%d\n", showscrollbars ? 1 : 0);
    fprintf(f, "show_timeline=%d\n", showtimeline ? 1 : 0);
    fprintf(f, "grid_lines=%d\n", showgridlines ? 1 : 0);
    fprintf(f, "overlay=%d\n", showoverlay ? 1 : 0);
    fprintf(f, "min_grid_mag=%d (2..%d)\n", mingridmag, MAX_MAG);
    fprintf(f, "bold_spacing=%d (2..%d)\n", boldspacing, MAX_SPACING);
    fprintf(f, "show_bold_lines=%d\n", showboldlines ? 1 : 0);
    fprintf(f, "math_coords=%d\n", mathcoords ? 1 : 0);
    fprintf(f, "cell_borders=%d\n", cellborders ? 1 : 0);

    fputs("\n", f);

    fprintf(f, "sync_views=%d\n", syncviews ? 1 : 0);
    fprintf(f, "sync_cursors=%d\n", synccursors ? 1 : 0);
    fprintf(f, "stack_layers=%d\n", stacklayers ? 1 : 0);
    fprintf(f, "tile_layers=%d\n", tilelayers ? 1 : 0);
    fprintf(f, "tile_border=%d (1..10)\n", tileborder);
    fprintf(f, "ask_on_new=%d\n", askonnew ? 1 : 0);
    fprintf(f, "ask_on_load=%d\n", askonload ? 1 : 0);
    fprintf(f, "ask_on_delete=%d\n", askondelete ? 1 : 0);
    fprintf(f, "ask_on_quit=%d\n", askonquit ? 1 : 0);
    fprintf(f, "warn_on_save=%d\n", warn_on_save ? 1 : 0);

    fputs("\n", f);

    fprintf(f, "show_icons=%d\n", showicons ? 1 : 0);
    fprintf(f, "smart_scale=%d\n", smartscale ? 1 : 0);
    fprintf(f, "swap_colors=%d\n", swapcolors ? 1 : 0);
    fprintf(f, "opacity=%d (1..100)\n", opacity);
    SaveColor(f, "border_rgb", borderrgb);
    SaveColor(f, "select_rgb", selectrgb);
    SaveColor(f, "paste_rgb", pastergb);

    fputs("\n", f);

    fprintf(f, "mouse_wheel_mode=%d\n", mousewheelmode);
    fprintf(f, "wheel_sensitivity=%d (1..%d)\n", wheelsens, MAX_SENSITIVITY);
    fprintf(f, "thumb_range=%d (2..%d)\n", thumbrange, MAX_THUMBRANGE);
    fprintf(f, "new_mag=%d (0..%d)\n", newmag, MAX_MAG);
    fprintf(f, "new_remove_sel=%d\n", newremovesel ? 1 : 0);
    fprintf(f, "new_cursor=%s\n", CursorToString(newcurs));
    fprintf(f, "open_remove_sel=%d\n", openremovesel ? 1 : 0);
    fprintf(f, "open_cursor=%s\n", CursorToString(opencurs));
    fprintf(f, "save_xrle=%d\n", savexrle ? 1 : 0);

    fputs("\n", f);

    SaveRelPath(f, "open_save_dir", opensavedir);
    SaveRelPath(f, "overlay_dir", overlaydir);
    SaveRelPath(f, "run_dir", rundir);
    SaveRelPath(f, "choose_dir", choosedir);
    SaveRelPath(f, "file_dir", filedir);
    SaveRelPath(f, "user_rules", userrules);
    SaveRelPath(f, "download_dir", downloaddir);
    SaveRelPath(f, "user_dir", userdir);

    fputs("\n", f);

    fprintf(f, "text_editor=%s\n", (const char*)texteditor.mb_str(wxConvLocal));
    fprintf(f, "python3_lib=%s\n", (const char*)pythonlib.mb_str(wxConvLocal));
    fprintf(f, "dir_width=%d\n", dirwinwd);
    fprintf(f, "show_files=%d\n", showfiles ? 1 : 0);
    fprintf(f, "max_patterns=%d (1..%d)\n", maxpatterns, MAX_RECENT);
    fprintf(f, "max_scripts=%d (1..%d)\n", maxscripts, MAX_RECENT);

    if (numpatterns > 0) {
        fputs("\n", f);
        int i;
        for (i = 0; i < numpatterns; i++) {
            wxMenuItem* item = patternSubMenu->FindItemByPosition(i);
            if (item) {
                wxString path = wxMenuItem::GetLabelText(item->GetItemLabel());
                fprintf(f, "recent_pattern=%s\n", (const char*)path.mb_str(wxConvLocal));
            }
        }
    }

    if (numscripts > 0) {
        fputs("\n", f);
        int i;
        for (i = 0; i < numscripts; i++) {
            wxMenuItem* item = scriptSubMenu->FindItemByPosition(i);
            if (item) {
                wxString path = wxMenuItem::GetLabelText(item->GetItemLabel());
                fprintf(f, "recent_script=%s\n", (const char*)path.mb_str(wxConvLocal));
            }
        }
    }

    fclose(f);
}

// -----------------------------------------------------------------------------

void AddDefaultRules()
{
    namedrules.Add(wxT("LifeHistory|LifeHistory"));
    namedrules.Add(wxT("3-4 Life|B34/S34"));
    namedrules.Add(wxT("HighLife|B36/S23"));
    namedrules.Add(wxT("AntiLife|B0123478/S01234678"));
    namedrules.Add(wxT("Life without Death|B3/S012345678"));
    namedrules.Add(wxT("Plow World|B378/S012345678"));
    namedrules.Add(wxT("Day and Night|B3678/S34678"));
    namedrules.Add(wxT("Diamoeba|B35678/S5678"));
    namedrules.Add(wxT("LongLife|B345/S5"));
    namedrules.Add(wxT("Seeds|B2/S"));
    namedrules.Add(wxT("Persian Rug|B234/S"));
    namedrules.Add(wxT("Replicator|B1357/S1357"));
    namedrules.Add(wxT("Fredkin|B1357/S02468"));
    namedrules.Add(wxT("Morley|B368/S245"));
    namedrules.Add(wxT("Wolfram 22|W22"));
    namedrules.Add(wxT("Wolfram 30|W30"));
    namedrules.Add(wxT("Wolfram 110|W110"));
    namedrules.Add(wxT("WireWorld|WireWorld"));
    namedrules.Add(wxT("JvN29|JvN29"));
    namedrules.Add(wxT("Nobili32|Nobili32"));
    namedrules.Add(wxT("Hutton32|Hutton32"));
}

// -----------------------------------------------------------------------------

bool GetKeywordAndValue(linereader& lr, char* line, char** keyword, char** value)
{
    // the linereader class handles all line endings (CR, CR+LF, LF)
    // and terminates line buffer with \0
    while ( lr.fgets(line, PREF_LINE_SIZE) != 0 ) {
        if ( line[0] == '#' || line[0] == 0 ) {
            // skip comment line or empty line
        } else {
            // line should have format keyword=value
            *keyword = line;
            *value = line;
            while ( **value != '=' && **value != 0 ) *value += 1;
            **value = 0;   // terminate keyword
            *value += 1;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------

void CheckVisibility(int* x, int* y, int* wd, int* ht)
{
    wxRect maxrect = wxGetClientDisplayRect();
    // reset x,y if title bar isn't clearly visible
    if ( *y + 10 < maxrect.y || *y + 10 > maxrect.GetBottom() ||
        *x + 10 > maxrect.GetRight() || *x + *wd - 10 < maxrect.x ) {
        *x = wxDefaultCoord;
        *y = wxDefaultCoord;
    }
    // reduce wd,ht if too big for screen
    if (*wd > maxrect.width) *wd = maxrect.width;
    if (*ht > maxrect.height) *ht = maxrect.height;
}

// -----------------------------------------------------------------------------

char* ReplaceDeprecatedAlgo(char* algoname)
{
    if (strcmp(algoname, "RuleTable") == 0 ||
        strcmp(algoname, "RuleTree") == 0) {
        // RuleTable and RuleTree algos have been replaced by RuleLoader
        return (char*)"RuleLoader";
    } else {
        return algoname;
    }
}

// -----------------------------------------------------------------------------

void InitPaths()
{
#ifdef __WXGTK__
    // on Linux we want datadir to be "~/.golly" rather than "~/.Golly"
    wxGetApp().SetAppName(_("golly"));
#endif

    // init datadir and create the directory if it doesn't exist;
    // the directory will probably be:
    // Win: C:\Documents and Settings\username\Application Data\Golly
    // Mac: ~/Library/Application Support/Golly
    // Unix: ~/.golly
    datadir = wxStandardPaths::Get().GetUserDataDir();
    if ( !wxFileName::DirExists(datadir) ) {
        if ( !wxFileName::Mkdir(datadir, 0777, wxPATH_MKDIR_FULL) ) {
            Warning(_("Could not create a user-specific data directory!\nWill try to use the application directory instead."));
            datadir = gollydir;
        }
    }
    if (datadir.Last() != wxFILE_SEP_PATH) datadir += wxFILE_SEP_PATH;

    // init tempdir to a temporary directory unique to this process
    tempdir = wxFileName::CreateTempFileName(wxT("golly_"));
    // on Linux the file is in /tmp;
    // on my Mac the file is in /private/var/tmp/folders.502/TemporaryItems;
    // on WinXP the file is in C:\Documents and Settings\Andy\Local Settings\Temp
    // (or shorter equivalent C:\DOCUME~1\Andy\LOCALS~1\Temp) but the file name
    // is gol*.tmp (ie. only 1st 3 chars of the prefix are used, and .tmp is added)
    wxRemoveFile(tempdir);
    if ( !wxFileName::Mkdir(tempdir, 0777, wxPATH_MKDIR_FULL) ) {
        Warning(_("Could not create temporary directory:\n") + tempdir);
        // use standard directory for temp files
        tempdir = wxStandardPaths::Get().GetTempDir();
        if ( !wxFileName::DirExists(tempdir) ) {
            // should never happen, but play safe
            Fatal(_("Sorry, temporary directory does not exist:\n") + tempdir);
        }
    }
    if (tempdir.Last() != wxFILE_SEP_PATH) tempdir += wxFILE_SEP_PATH;

#ifdef __WXGTK__
    // "Golly" is nicer for warning dialogs etc
    wxGetApp().SetAppName(_("Golly"));
#endif

    // init prefspath -- look in gollydir first, then in datadir
    prefspath = gollydir + PREFS_NAME;
    if ( !wxFileExists(prefspath) ) {
        prefspath = datadir + PREFS_NAME;
    }
}

// -----------------------------------------------------------------------------

void CreateMissingFolders()
{
    // create userrules and downloaddir if they don't exist
    if ( !wxFileName::DirExists(userrules) ) {
        if ( !wxFileName::Mkdir(userrules, 0777, wxPATH_MKDIR_FULL) ) {
            Warning(_("Could not create your rules directory:\n") + userrules);
        }
    }
    if ( !wxFileName::DirExists(downloaddir) ) {
        if ( !wxFileName::Mkdir(downloaddir, 0777, wxPATH_MKDIR_FULL) ) {
            Warning(_("Could not create your download directory:\n") + downloaddir);
        }
    }
}

// -----------------------------------------------------------------------------

void GetPrefs()
{
    int algoindex = -1;                 // unknown algorithm
    bool sawkeyaction = false;          // saw at least one key_action entry?

    MAX_MAG = 5;                        // maximum cell size = 32x32
    // this might be better for high-res screens???!!!
    // MAX_MAG = 6;                        // maximum cell size = 64x64

    InitPaths();                        // init datadir, tempdir and prefspath
    InitAlgorithms();                   // init algoinfo data

    rulesdir = gollydir + wxT("Rules");
    rulesdir += wxFILE_SEP_PATH;

    userrules = datadir + wxT("Rules");
    userrules += wxFILE_SEP_PATH;

    downloaddir = datadir + wxT("Downloads");
    downloaddir += wxFILE_SEP_PATH;

    rundir = gollydir + SCRIPT_DIR;
    opensavedir = gollydir;
    choosedir = gollydir;
    filedir = gollydir;
    overlaydir = datadir;
    userdir = baddir;                   // initially an illegal dir path

    // init the text editor to something reasonable
#ifdef __WXMSW__
    texteditor = wxT("Notepad");
#elif defined(__WXMAC__)
    texteditor = wxT("/Applications/TextEdit.app");
#else // assume Linux
    // don't attempt to guess which editor might be available;
    // set the string empty so the user is asked to choose their
    // preferred editor the first time texteditor is used
    texteditor = wxEmptyString;
#endif

    // init names of Python libraries
#ifdef __WXMSW__
    pythonlib = wxT("python39.dll");
#elif defined(__WXMAC__)
    pythonlib = wxT("/Library/Frameworks/Python.framework/Versions/3.9/Python");
#else // assume Linux
    pythonlib = wxT(STRINGIFY(PYTHON_SHLIB));
#endif

    // create curs_* and initialize newcurs and opencurs
    CreateCursors();

    CreateDefaultColors();

    // initialize Open Recent submenu
    patternSubMenu = new wxMenu();
    patternSubMenu->AppendSeparator();
    patternSubMenu->Append(ID_CLEAR_MISSING_PATTERNS, _("Clear Missing Files"));
    patternSubMenu->Append(ID_CLEAR_ALL_PATTERNS, _("Clear All Files"));

    // initialize Run Recent submenu
    scriptSubMenu = new wxMenu();
    scriptSubMenu->AppendSeparator();
    scriptSubMenu->Append(ID_CLEAR_MISSING_SCRIPTS, _("Clear Missing Files"));
    scriptSubMenu->Append(ID_CLEAR_ALL_SCRIPTS, _("Clear All Files"));

    namedrules.Add(wxT("Life|B3/S23"));      // must be 1st entry

    if ( !wxFileExists(prefspath) ) {
        AddDefaultRules();
        AddDefaultKeyActions();
        UpdateAcceleratorStrings();
        CreateMissingFolders();
        return;
    }

#ifdef __WXMAC__
    // we need to convert prefspath to decomposed UTF8 so fopen will work
    FILE* f = fopen(prefspath.fn_str(), "r");
#else
    FILE* f = fopen(prefspath.mb_str(wxConvLocal), "r");
#endif
    if (f == NULL) {
        Warning(_("Could not read preferences file!"));
        return;
    }

    linereader reader(f);
    char line[PREF_LINE_SIZE];
    char* keyword;
    char* value;
    while ( GetKeywordAndValue(reader, line, &keyword, &value) ) {

        if (strcmp(keyword, "prefs_version") == 0) {
            sscanf(value, "%d", &currversion);

        } else if (strcmp(keyword, "debug_level") == 0) {
            sscanf(value, "%d", &debuglevel);

        } else if (strcmp(keyword, "key_action") == 0) {
            GetKeyAction(value);
            sawkeyaction = true;

        } else if (strcmp(keyword, "main_window") == 0) {
            sscanf(value, "%d,%d,%d,%d", &mainx, &mainy, &mainwd, &mainht);
            // avoid very small window
            if (mainwd < minmainwd) mainwd = minmainwd;
            if (mainht < minmainht) mainht = minmainht;
            CheckVisibility(&mainx, &mainy, &mainwd, &mainht);

        } else if (strcmp(keyword, "maximize") == 0) {
            maximize = value[0] == '1';

        } else if (strcmp(keyword, "help_window") == 0) {
            sscanf(value, "%d,%d,%d,%d", &helpx, &helpy, &helpwd, &helpht);
            if (helpwd < minhelpwd) helpwd = minhelpwd;
            if (helpht < minhelpht) helpht = minhelpht;
            CheckVisibility(&helpx, &helpy, &helpwd, &helpht);

        } else if (strcmp(keyword, "help_font_size") == 0) {
            sscanf(value, "%d", &helpfontsize);
            if (helpfontsize < minfontsize) helpfontsize = minfontsize;
            if (helpfontsize > maxfontsize) helpfontsize = maxfontsize;

        } else if (strcmp(keyword, "info_window") == 0) {
            sscanf(value, "%d,%d,%d,%d", &infox, &infoy, &infowd, &infoht);
            if (infowd < mininfowd) infowd = mininfowd;
            if (infoht < mininfoht) infoht = mininfoht;
            CheckVisibility(&infox, &infoy, &infowd, &infoht);

        } else if (strcmp(keyword, "rule_dialog") == 0) {
            sscanf(value, "%d,%d,%d,%d", &rulex, &ruley, &ruleexwd, &ruleexht);
            if (ruleexwd < 100) ruleexwd = 100;
            if (ruleexht < 0) ruleexht = 0;
            CheckVisibility(&rulex, &ruley, &ruleexwd, &ruleexht);

        } else if (strcmp(keyword, "show_algo_help") == 0) {
            showalgohelp = value[0] == '1';

        } else if (strcmp(keyword, "allow_undo") == 0) {
            allowundo = value[0] == '1';

        } else if (strcmp(keyword, "allow_beep") == 0) {
            allowbeep = value[0] == '1';

        } else if (strcmp(keyword, "show_selpop") == 0) {
            showselpop = value[0] == '1';

        } else if (strcmp(keyword, "restore_view") == 0) {
            restoreview = value[0] == '1';

        } else if (strcmp(keyword, "paste_location") == 0) {
            SetPasteLocation(value);

        } else if (strcmp(keyword, "paste_mode") == 0) {
            SetPasteMode(value);

        } else if (strcmp(keyword, "scroll_pencil") == 0) {
            scrollpencil = value[0] == '1';

        } else if (strcmp(keyword, "scroll_cross") == 0) {
            scrollcross = value[0] == '1';

        } else if (strcmp(keyword, "scroll_hand") == 0) {
            scrollhand = value[0] == '1';

        } else if (strcmp(keyword, "controls_pos") == 0) {
            sscanf(value, "%d", &controlspos);
            if (controlspos < 0) controlspos = 0;
            if (controlspos > 4) controlspos = 4;

        } else if (strcmp(keyword, "can_change_rule") == 0) {
            sscanf(value, "%d", &canchangerule);
            if (canchangerule < 0) canchangerule = 0;
            if (canchangerule > 2) canchangerule = 2;

        } else if (strcmp(keyword, "random_fill") == 0) {
            sscanf(value, "%d", &randomfill);
            if (randomfill < 1) randomfill = 1;
            if (randomfill > 100) randomfill = 100;

        } else if (strcmp(keyword, "q_base_step") == 0) {     // deprecated
            int base;
            sscanf(value, "%d", &base);
            if (base < 2) base = 2;
            if (base > MAX_BASESTEP) base = MAX_BASESTEP;
            algoinfo[QLIFE_ALGO]->defbase = base;

        } else if (strcmp(keyword, "h_base_step") == 0) {     // deprecated
            int base;
            sscanf(value, "%d", &base);
            if (base < 2) base = 2;
            if (base > MAX_BASESTEP) base = MAX_BASESTEP;
            algoinfo[HLIFE_ALGO]->defbase = base;

        } else if (strcmp(keyword, "algorithm") == 0) {
            if (strcmp(value, "RuleTable") == 0) {
                // use deprecated RuleTable settings for RuleLoader
                // (deprecated RuleTree settings will simply be ignored)
                value = (char*)"RuleLoader";
            }
            algoindex = -1;
            for (int i = 0; i < NumAlgos(); i++) {
                if (strcmp(value, GetAlgoName(i)) == 0) {
                    algoindex = i;
                    break;
                }
            }

        } else if (strcmp(keyword, "max_mem") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos()) {
                int maxmem;
                sscanf(value, "%d", &maxmem);
                if (maxmem < MIN_MEM_MB) maxmem = MIN_MEM_MB;
                if (maxmem > MAX_MEM_MB) maxmem = MAX_MEM_MB;
                algoinfo[algoindex]->algomem = maxmem;
            }

        } else if (strcmp(keyword, "base_step") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos()) {
                int base;
                sscanf(value, "%d", &base);
                if (base < 2) base = 2;
                if (base > MAX_BASESTEP) base = MAX_BASESTEP;
                algoinfo[algoindex]->defbase = base;
            }

        } else if (strcmp(keyword, "status_rgb") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos())
                GetColor(value, &algoinfo[algoindex]->statusrgb);

        } else if (strcmp(keyword, "from_rgb") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos())
                GetColor(value, &algoinfo[algoindex]->fromrgb);

        } else if (strcmp(keyword, "to_rgb") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos())
                GetColor(value, &algoinfo[algoindex]->torgb);

        } else if (strcmp(keyword, "use_gradient") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos())
                algoinfo[algoindex]->gradient = value[0] == '1';

        } else if (strcmp(keyword, "colors") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos()) {
                int state, r, g, b;
                while (sscanf(value, "%d,%d,%d,%d,", &state, &r, &g, &b) == 4) {
                    if (state >= 0 && state < algoinfo[algoindex]->maxstates) {
                        algoinfo[algoindex]->algor[state] = r;
                        algoinfo[algoindex]->algog[state] = g;
                        algoinfo[algoindex]->algob[state] = b;
                    }
                    while (*value != ',') value++;
                    value++;
                    while (*value != ',') value++;
                    value++;
                    while (*value != ',') value++;
                    value++;
                    while (*value != ',') value++;
                    value++;
                }
            }

        } else if (strcmp(keyword, "min_delay") == 0) {
            sscanf(value, "%d", &mindelay);
            if (mindelay < 0) mindelay = 0;
            if (mindelay > MAX_DELAY) mindelay = MAX_DELAY;

        } else if (strcmp(keyword, "max_delay") == 0) {
            sscanf(value, "%d", &maxdelay);
            if (maxdelay < 0) maxdelay = 0;
            if (maxdelay > MAX_DELAY) maxdelay = MAX_DELAY;

        } else if (strcmp(keyword, "auto_fit") == 0) {
            initautofit = value[0] == '1';

        } else if (strcmp(keyword, "hashing") == 0) {            // deprecated
            initalgo = value[0] == '1' ? HLIFE_ALGO : QLIFE_ALGO;

        } else if (strcmp(keyword, "init_algo") == 0) {
            value = ReplaceDeprecatedAlgo(value);
            int i = staticAlgoInfo::nameToIndex(value);
            if (i >= 0 && i < NumAlgos())
                initalgo = i;

        } else if (strcmp(keyword, "hyperspeed") == 0) {
            inithyperspeed = value[0] == '1';

        } else if (strcmp(keyword, "hash_info") == 0) {
            initshowhashinfo = value[0] == '1';

        } else if (strcmp(keyword, "show_population") == 0) {
            showpopulation = value[0] == '1';

        } else if (strcmp(keyword, "autostop") == 0) {
            autostop = value[0] == '1';

        } else if (strcmp(keyword, "max_hash_mem") == 0) {       // deprecated
            int maxmem;
            sscanf(value, "%d", &maxmem);
            if (maxmem < MIN_MEM_MB) maxmem = MIN_MEM_MB;
            if (maxmem > MAX_MEM_MB) maxmem = MAX_MEM_MB;
            // change all except QLIFE_ALGO
            for (int i = 0; i < NumAlgos(); i++)
                if (i != QLIFE_ALGO) algoinfo[i]->algomem = maxmem;

        } else if (strcmp(keyword, "rule") == 0) {
            if (strlen(value)+1 > sizeof(initrule)) {
                // should never happen if sizeof(initrule) is MAXRULESIZE
                Fatal(_("Initial rule is too long: ")+value);
            }
            strcpy(initrule, value);

        } else if (strcmp(keyword, "named_rule") == 0) {
            // value must have format "name|rule" with name and rule non-empty
            wxString str(value,wxConvLocal);
            int barcount = str.Freq('|');
            if (barcount == 0) {
                Fatal(_("Missing \"|\" separator in named_rule entry: ") + str);
            } else if (barcount > 1) {
                Fatal(_("Too many \"|\" separators in named_rule entry: ") + str);
            } else {
                wxString name = str.BeforeFirst('|');
                wxString rule = str.AfterFirst('|');
                if (name.IsEmpty()) {
                    Fatal(_("Empty name in named_rule entry: ") + str);
                } else if (rule.IsEmpty()) {
                    Fatal(_("Empty rule in named_rule entry: ") + str);
                } else {
                    namedrules.Add(str);
                }
            }

        } else if (strcmp(keyword, "show_tips") == 0) {
            showtips = value[0] == '1';

        } else if (strcmp(keyword, "show_tool") == 0) {
            showtool = value[0] == '1';

        } else if (strcmp(keyword, "show_layer") == 0) {
            showlayer = value[0] == '1';

        } else if (strcmp(keyword, "show_edit") == 0) {
            showedit = value[0] == '1';

        } else if (strcmp(keyword, "show_states") == 0) {
            showallstates = value[0] == '1';

        } else if (strcmp(keyword, "show_status") == 0) {
            showstatus = value[0] == '1';

        } else if (strcmp(keyword, "show_exact") == 0) {
            showexact = value[0] == '1';

        } else if (strcmp(keyword, "show_xystate") == 0) {
            showxystate = value[0] == '1';

        } else if (strcmp(keyword, "show_scrollbars") == 0) {
            showscrollbars = value[0] == '1';

        } else if (strcmp(keyword, "show_timeline") == 0) {
            showtimeline = value[0] == '1';

        } else if (strcmp(keyword, "grid_lines") == 0) {
            showgridlines = value[0] == '1';

        } else if (strcmp(keyword, "overlay") == 0) {
            showoverlay = value[0] == '1';

        } else if (strcmp(keyword, "min_grid_mag") == 0) {
            sscanf(value, "%d", &mingridmag);
            if (mingridmag < 2) mingridmag = 2;
            if (mingridmag > MAX_MAG) mingridmag = MAX_MAG;

        } else if (strcmp(keyword, "bold_spacing") == 0) {
            sscanf(value, "%d", &boldspacing);
            if (boldspacing < 2) boldspacing = 2;
            if (boldspacing > MAX_SPACING) boldspacing = MAX_SPACING;

        } else if (strcmp(keyword, "show_bold_lines") == 0) {
            showboldlines = value[0] == '1';

        } else if (strcmp(keyword, "math_coords") == 0) {
            mathcoords = value[0] == '1';

        } else if (strcmp(keyword, "cell_borders") == 0) {
            cellborders = value[0] == '1';

        } else if (strcmp(keyword, "sync_views") == 0) {
            syncviews = value[0] == '1';

        } else if (strcmp(keyword, "sync_cursors") == 0) {
            synccursors = value[0] == '1';

        } else if (strcmp(keyword, "stack_layers") == 0) {
            stacklayers = value[0] == '1';

        } else if (strcmp(keyword, "tile_layers") == 0) {
            tilelayers = value[0] == '1';

        } else if (strcmp(keyword, "tile_border") == 0) {
            sscanf(value, "%d", &tileborder);
            if (tileborder < 1) tileborder = 1;
            if (tileborder > 10) tileborder = 10;

        } else if (strcmp(keyword, "ask_on_new") == 0)    { askonnew = value[0] == '1';
        } else if (strcmp(keyword, "ask_on_load") == 0)   { askonload = value[0] == '1';
        } else if (strcmp(keyword, "ask_on_delete") == 0) { askondelete = value[0] == '1';
        } else if (strcmp(keyword, "ask_on_quit") == 0)   { askonquit = value[0] == '1';
        } else if (strcmp(keyword, "warn_on_save") == 0)  { warn_on_save = value[0] == '1';

        } else if (strcmp(keyword, "show_icons") == 0) {
            showicons = value[0] == '1';

        } else if (strcmp(keyword, "smart_scale") == 0) {
            smartscale = value[0] == '1';

        } else if (strcmp(keyword, "swap_colors") == 0) {
            swapcolors = value[0] == '1';

        } else if (strcmp(keyword, "opacity") == 0) {
            sscanf(value, "%d", &opacity);
            if (opacity < 1) opacity = 1;
            if (opacity > 100) opacity = 100;

        } else if (strcmp(keyword, "border_rgb") == 0) { GetColor(value, borderrgb);
        } else if (strcmp(keyword, "select_rgb") == 0) { GetColor(value, selectrgb);
        } else if (strcmp(keyword, "paste_rgb") == 0)  { GetColor(value, pastergb);

        } else if (strcmp(keyword, "dead_rgb") == 0) {
            // use deprecated value to set color of state 0 in all algos
            // (only done once because dead_rgb is no longer saved in prefs file)
            wxColor color;
            GetColor(value, &color);
            for (int i = 0; i < NumAlgos(); i++) {
                algoinfo[i]->algor[0] = color.Red();
                algoinfo[i]->algog[0] = color.Green();
                algoinfo[i]->algob[0] = color.Blue();
            }

        } else if (strcmp(keyword, "qlife_rgb") == 0) {       // deprecated
            GetColor(value, &algoinfo[QLIFE_ALGO]->statusrgb);

        } else if (strcmp(keyword, "hlife_rgb") == 0) {       // deprecated
            GetColor(value, &algoinfo[HLIFE_ALGO]->statusrgb);

        } else if (strcmp(keyword, "mouse_wheel_mode") == 0) {
            sscanf(value, "%d", &mousewheelmode);
            if (mousewheelmode < 0) mousewheelmode = 0;
            if (mousewheelmode > 2) mousewheelmode = 2;

        } else if (strcmp(keyword, "wheel_sensitivity") == 0) {
            sscanf(value, "%d", &wheelsens);
            if (wheelsens < 1) wheelsens = 1;
            if (wheelsens > MAX_SENSITIVITY) wheelsens = MAX_SENSITIVITY;

        } else if (strcmp(keyword, "thumb_range") == 0) {
            sscanf(value, "%d", &thumbrange);
            if (thumbrange < 2) thumbrange = 2;
            if (thumbrange > MAX_THUMBRANGE) thumbrange = MAX_THUMBRANGE;

        } else if (strcmp(keyword, "new_mag") == 0) {
            sscanf(value, "%d", &newmag);
            if (newmag < 0) newmag = 0;
            if (newmag > MAX_MAG) newmag = MAX_MAG;

        } else if (strcmp(keyword, "new_remove_sel") == 0) {
            newremovesel = value[0] == '1';

        } else if (strcmp(keyword, "new_cursor") == 0) {
            newcurs = StringToCursor(value);

        } else if (strcmp(keyword, "open_remove_sel") == 0) {
            openremovesel = value[0] == '1';

        } else if (strcmp(keyword, "open_cursor") == 0) {
            opencurs = StringToCursor(value);

        } else if (strcmp(keyword, "save_xrle") == 0) {
            savexrle = value[0] == '1';

        } else if (strcmp(keyword, "open_save_dir") == 0) { GetRelPath(value, opensavedir);
        } else if (strcmp(keyword, "overlay_dir") == 0)   { GetRelPath(value, overlaydir);
        } else if (strcmp(keyword, "run_dir") == 0)       { GetRelPath(value, rundir, SCRIPT_DIR);
        } else if (strcmp(keyword, "choose_dir") == 0)    { GetRelPath(value, choosedir);
        } else if (strcmp(keyword, "file_dir") == 0)      { GetRelPath(value, filedir);
        } else if (strcmp(keyword, "pattern_dir") == 0)   { GetRelPath(value, filedir);     // deprecated
        } else if (strcmp(keyword, "user_rules") == 0)    { GetRelPath(value, userrules);
        } else if (strcmp(keyword, "download_dir") == 0)  { GetRelPath(value, downloaddir);
        } else if (strcmp(keyword, "user_dir") == 0)      { GetRelPath(value, userdir, baddir);

        } else if (strcmp(keyword, "text_editor") == 0) {
            texteditor = wxString(value,wxConvLocal);

        } else if (strcmp(keyword, "python3_lib") == 0) {
            pythonlib = wxString(value,wxConvLocal);

        } else if (strcmp(keyword, "dir_width") == 0) {
            sscanf(value, "%d", &dirwinwd);
            if (dirwinwd < MIN_DIRWD) dirwinwd = MIN_DIRWD;

        } else if (strcmp(keyword, "show_files") == 0 ||
                   strcmp(keyword, "show_patterns") == 0) {     // deprecated
            showfiles = value[0] == '1';

        } else if (strcmp(keyword, "show_scripts") == 0) {
            // deprecated

        } else if (strcmp(keyword, "max_patterns") == 0) {
            sscanf(value, "%d", &maxpatterns);
            if (maxpatterns < 1) maxpatterns = 1;
            if (maxpatterns > MAX_RECENT) maxpatterns = MAX_RECENT;

        } else if (strcmp(keyword, "max_scripts") == 0) {
            sscanf(value, "%d", &maxscripts);
            if (maxscripts < 1) maxscripts = 1;
            if (maxscripts > MAX_RECENT) maxscripts = MAX_RECENT;

        } else if (strcmp(keyword, "recent_pattern") == 0) {
            // append path to Open Recent submenu
            if (numpatterns < maxpatterns && value[0]) {
                numpatterns++;
                wxString path(value, wxConvLocal);
                if (currversion < 2 && path.StartsWith(gollydir)) {
                    // remove gollydir from start of path
                    path.erase(0, gollydir.length());
                }
                // duplicate ampersands so they appear in menu
                path.Replace(wxT("&"), wxT("&&"));
                patternSubMenu->Insert(numpatterns - 1, ID_OPEN_RECENT + numpatterns, path);
            }

        } else if (strcmp(keyword, "recent_script") == 0) {
            // append path to Run Recent submenu
            if (numscripts < maxscripts && value[0]) {
                numscripts++;
                wxString path(value, wxConvLocal);
                if (currversion < 2 && path.StartsWith(gollydir)) {
                    // remove gollydir from start of path
                    path.erase(0, gollydir.length());
                }
                // duplicate ampersands so they appear in menu
                path.Replace(wxT("&"), wxT("&&"));
                scriptSubMenu->Insert(numscripts - 1, ID_RUN_RECENT + numscripts, path);
            }
        }
    }

    reader.close();

    // colors for status brushes may have changed
    UpdateStatusBrushes();

    // stacklayers and tilelayers must not both be true
    if (stacklayers && tilelayers) tilelayers = false;

    // if no named_rule entries then add default names
    if (namedrules.GetCount() == 1) AddDefaultRules();

    // if no key_action entries then use default shortcuts
    if (!sawkeyaction) AddDefaultKeyActions();

    // initialize accelerator array
    UpdateAcceleratorStrings();

    // create some important directories if they don't exist
    CreateMissingFolders();
}

// -----------------------------------------------------------------------------

// global data used in CellBoxes and PrefsDialog methods:

static int coloralgo;         // currently selected algorithm in Color pane
static int gradstates;        // current number of gradient states

const int CELLSIZE = 16;      // wd and ht of each cell in CellBoxes
const int NUMCOLS = 32;       // number of columns in CellBoxes
const int NUMROWS = 8;        // number of rows in CellBoxes

// -----------------------------------------------------------------------------

// define a window for displaying cell colors/icons:

class CellBoxes : public wxPanel
{
public:
    CellBoxes(wxWindow* parent, wxWindowID id, const wxPoint& pos,
              const wxSize& size) : wxPanel(parent, id, pos, size) { }

    wxStaticText* statebox;    // for showing state of cell under cursor
    wxStaticText* rgbbox;      // for showing color of cell under cursor

private:
    void GetGradientColor(int state, unsigned char* r,
                          unsigned char* g,
                          unsigned char* b);

    void OnEraseBackground(wxEraseEvent& event);
    void OnPaint(wxPaintEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseMotion(wxMouseEvent& event);
    void OnMouseExit(wxMouseEvent& event);

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(CellBoxes, wxPanel)
EVT_ERASE_BACKGROUND (CellBoxes::OnEraseBackground)
EVT_PAINT            (CellBoxes::OnPaint)
EVT_LEFT_DOWN        (CellBoxes::OnMouseDown)
EVT_LEFT_DCLICK      (CellBoxes::OnMouseDown)
EVT_MOTION           (CellBoxes::OnMouseMotion)
EVT_ENTER_WINDOW     (CellBoxes::OnMouseMotion)
EVT_LEAVE_WINDOW     (CellBoxes::OnMouseExit)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

void CellBoxes::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
    // do nothing
}

// -----------------------------------------------------------------------------

void CellBoxes::GetGradientColor(int state, unsigned char* r,
                                 unsigned char* g,
                                 unsigned char* b)
{
    // calculate gradient color for given state (> 0 and < gradstates)
    AlgoData* ad = algoinfo[coloralgo];
    if (state == 1) {
        *r = ad->fromrgb.Red();
        *g = ad->fromrgb.Green();
        *b = ad->fromrgb.Blue();
    } else if (state == gradstates - 1) {
        *r = ad->torgb.Red();
        *g = ad->torgb.Green();
        *b = ad->torgb.Blue();
    } else {
        unsigned char r1 = ad->fromrgb.Red();
        unsigned char g1 = ad->fromrgb.Green();
        unsigned char b1 = ad->fromrgb.Blue();
        unsigned char r2 = ad->torgb.Red();
        unsigned char g2 = ad->torgb.Green();
        unsigned char b2 = ad->torgb.Blue();
        int N = gradstates - 2;
        double rfrac = (double)(r2 - r1) / (double)N;
        double gfrac = (double)(g2 - g1) / (double)N;
        double bfrac = (double)(b2 - b1) / (double)N;
        *r = (int)(r1 + (state-1) * rfrac + 0.5);
        *g = (int)(g1 + (state-1) * gfrac + 0.5);
        *b = (int)(b1 + (state-1) * bfrac + 0.5);
    }
}

// -----------------------------------------------------------------------------

void CellBoxes::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    dc.SetPen(*wxBLACK_PEN);

#ifdef __WXMSW__
    // we have to use theme background color on Windows
    wxBrush bgbrush(GetBackgroundColour());
#else
    wxBrush bgbrush(*wxTRANSPARENT_BRUSH);
#endif

    // draw cell boxes
    wxRect r = wxRect(0, 0, CELLSIZE+1, CELLSIZE+1);
    int col = 0;
    for (int state = 0; state < 256; state++) {
        if (state < algoinfo[coloralgo]->maxstates) {
            if (state == 0) {
                wxColor color(algoinfo[coloralgo]->algor[0],
                              algoinfo[coloralgo]->algog[0],
                              algoinfo[coloralgo]->algob[0]);
                dc.SetBrush(wxBrush(color));
                DrawRect(dc, r);
                dc.SetBrush(wxNullBrush);
            } else if (showicons) {
                wxBitmap** iconmaps = algoinfo[coloralgo]->icons15x15;
                if (iconmaps && iconmaps[state]) {
                    dc.SetBrush(*wxTRANSPARENT_BRUSH);
                    DrawRect(dc, r);
                    dc.SetBrush(wxNullBrush);
                    if (algoinfo[coloralgo]->gradient) {
                        if (state > 0 && state < gradstates) {
                            unsigned char red, green, blue;
                            GetGradientColor(state, &red, &green, &blue);
                            DrawOneIcon(dc, r.x + 1, r.y + 1, iconmaps[state],
                                        algoinfo[coloralgo]->algor[0],
                                        algoinfo[coloralgo]->algog[0],
                                        algoinfo[coloralgo]->algob[0],
                                        red, green, blue,
                                        false);     // default icons are grayscale
                        } else {
                            dc.SetBrush(bgbrush);
                            DrawRect(dc, r);
                            dc.SetBrush(wxNullBrush);
                        }
                    } else {
                        DrawOneIcon(dc, r.x + 1, r.y + 1, iconmaps[state],
                                    algoinfo[coloralgo]->algor[0],
                                    algoinfo[coloralgo]->algog[0],
                                    algoinfo[coloralgo]->algob[0],
                                    algoinfo[coloralgo]->algor[state],
                                    algoinfo[coloralgo]->algog[state],
                                    algoinfo[coloralgo]->algob[state],
                                    false);     // default icons are grayscale
                    }
                } else {
                    dc.SetBrush(bgbrush);
                    DrawRect(dc, r);
                    dc.SetBrush(wxNullBrush);
                }
            } else if (algoinfo[coloralgo]->gradient) {
                if (state > 0 && state < gradstates) {
                    unsigned char red, green, blue;
                    GetGradientColor(state, &red, &green, &blue);
                    wxColor color(red, green, blue);
                    dc.SetBrush(wxBrush(color));
                    DrawRect(dc, r);
                    dc.SetBrush(wxNullBrush);
                } else {
                    dc.SetBrush(bgbrush);
                    DrawRect(dc, r);
                    dc.SetBrush(wxNullBrush);
                }
            } else {
                wxColor color(algoinfo[coloralgo]->algor[state],
                              algoinfo[coloralgo]->algog[state],
                              algoinfo[coloralgo]->algob[state]);
                dc.SetBrush(wxBrush(color));
                DrawRect(dc, r);
                dc.SetBrush(wxNullBrush);
            }

        } else {
            // state >= maxstates
            dc.SetBrush(bgbrush);
            DrawRect(dc, r);
            dc.SetBrush(wxNullBrush);
        }

        col++;
        if (col < NUMCOLS) {
            r.x += CELLSIZE;
        } else {
            r.x = 0;
            r.y += CELLSIZE;
            col = 0;
        }
    }

    dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void CellBoxes::OnMouseDown(wxMouseEvent& event)
{
    int col = event.GetX() / CELLSIZE;
    int row = event.GetY() / CELLSIZE;
    int state = row * NUMCOLS + col;
    if (state >= 0 && state < algoinfo[coloralgo]->maxstates) {
        if (algoinfo[coloralgo]->gradient && state > 0) {
            Beep();
        } else {
            // let user change color of this cell state
            wxColour rgb(algoinfo[coloralgo]->algor[state],
                         algoinfo[coloralgo]->algog[state],
                         algoinfo[coloralgo]->algob[state]);
            wxColourData data;
            data.SetChooseFull(true);    // for Windows
            data.SetColour(rgb);

            wxColourDialog dialog(this, &data);
            if ( dialog.ShowModal() == wxID_OK ) {
                wxColourData retData = dialog.GetColourData();
                wxColour c = retData.GetColour();
                if (rgb != c) {
                    // change color
                    algoinfo[coloralgo]->algor[state] = c.Red();
                    algoinfo[coloralgo]->algog[state] = c.Green();
                    algoinfo[coloralgo]->algob[state] = c.Blue();
                    Refresh(false);
                }
            }
        }
    }

    event.Skip();
}

// -----------------------------------------------------------------------------

void CellBoxes::OnMouseMotion(wxMouseEvent& event)
{
    int col = event.GetX() / CELLSIZE;
    int row = event.GetY() / CELLSIZE;
    int state = row * NUMCOLS + col;
    if (state < 0 || state > 255) {
        statebox->SetLabel(_(" "));
        rgbbox->SetLabel(_(" "));
    } else {
        statebox->SetLabel(wxString::Format(_("%d"),state));
        if (state < algoinfo[coloralgo]->maxstates) {
            unsigned char r, g, b;
            if (algoinfo[coloralgo]->gradient && state > 0) {
                GetGradientColor(state, &r, &g, &b);
            } else {
                r = algoinfo[coloralgo]->algor[state];
                g = algoinfo[coloralgo]->algog[state];
                b = algoinfo[coloralgo]->algob[state];
            }
            rgbbox->SetLabel(wxString::Format(_("%d,%d,%d"),r,g,b));
        } else {
            rgbbox->SetLabel(_(" "));
        }
    }
}

// -----------------------------------------------------------------------------

void CellBoxes::OnMouseExit(wxMouseEvent& WXUNUSED(event))
{
    statebox->SetLabel(_(" "));
    rgbbox->SetLabel(_(" "));
}

// -----------------------------------------------------------------------------

#if defined(__WXMAC__) && wxCHECK_VERSION(2,7,2)
    // fix wxMac 2.7.2+ bug in wxTextCtrl::SetSelection
    #define ALL_TEXT 0,999
#else
    #define ALL_TEXT -1,-1
#endif

const wxString HASH_MEM_NOTE = _("MB (best if ~50% of RAM)");
const wxString HASH_STEP_NOTE = _("(best if power of 2)");
const wxString NONHASH_MEM_NOTE = _("MB (0 means no limit)");
const wxString NONHASH_STEP_NOTE = _(" ");

const int BITMAP_WD = 60;        // width of bitmap in color buttons
const int BITMAP_HT = 20;        // height of bitmap in color buttons

const int PAGESIZE = 10;         // scroll amount when paging

static size_t currpage = 0;      // current page in PrefsDialog

// these are global so we can remember current key combination
static int currkey = ' ';
static int currmods = mk_ALT + mk_SHIFT + mk_CMD;

// -----------------------------------------------------------------------------

enum {
    // these *_PAGE values must correspond to currpage values
    FILE_PAGE = 0,
    EDIT_PAGE,
    CONTROL_PAGE,
    VIEW_PAGE,
    LAYER_PAGE,
    COLOR_PAGE,
    KEYBOARD_PAGE
};

enum {
    // File prefs
    PREF_NEW_REM_SEL = wxID_HIGHEST + 1,  // avoid problems with FindWindowById
    PREF_NEW_CURSOR,
    PREF_NEW_SCALE,
    PREF_OPEN_REM_SEL,
    PREF_OPEN_CURSOR,
    PREF_MAX_PATTERNS,
    PREF_MAX_SCRIPTS,
    PREF_EDITOR_BUTT,
    PREF_EDITOR_BOX,
    PREF_DOWNLOAD_BUTT,
    PREF_DOWNLOAD_BOX,
    // Edit prefs
    PREF_RANDOM_FILL,
    PREF_PASTE_0,
    PREF_PASTE_1,
    PREF_PASTE_2,
    PREF_SCROLL_PENCIL,
    PREF_SCROLL_CROSS,
    PREF_SCROLL_HAND,
    PREF_BEEP,
    PREF_SHOW_SEL_POP,
    // Control prefs
    PREF_ALGO_MENU1,
    PREF_MAX_MEM,
    PREF_MEM_NOTE,
    PREF_BASE_STEP,
    PREF_STEP_NOTE,
    PREF_MIN_DELAY,
    PREF_MAX_DELAY,
    PREF_RULES_BUTT,
    PREF_RULES_BOX,
    // View prefs
    PREF_SHOW_TIPS,
    PREF_RESTORE,
    PREF_Y_UP,
    PREF_CELL_BORDERS,
    PREF_SHOW_BOLD,
    PREF_BOLD_SPACING,
    PREF_MIN_GRID_SCALE,
    PREF_MOUSE_WHEEL,
    PREF_SENSITIVITY,
    PREF_THUMB_RANGE,
    PREF_CONTROLS,
    // Layer prefs
    PREF_OPACITY,
    PREF_TILE_BORDER,
    PREF_ASK_NEW,
    PREF_ASK_LOAD,
    PREF_ASK_DELETE,
    PREF_ASK_QUIT,
    PREF_WARN_SAVE,
    // Color prefs
    PREF_ALGO_MENU2,
    PREF_GRADIENT_CHECK,
    PREF_ICON_CHECK,
    PREF_CELL_PANEL,
    PREF_SCROLL_BAR,
    PREF_STATE_BOX,
    PREF_RGB_BOX,
    PREF_STATUS_BUTT,
    PREF_FROM_BUTT,
    PREF_TO_BUTT,
    PREF_SELECT_BUTT,
    PREF_PASTE_BUTT,
    PREF_BORDER_BUTT,
    // Keyboard prefs
    PREF_KEYCOMBO,
    PREF_ACTION,
    PREF_CHOOSE,
    PREF_FILE_BOX
};

// define a multi-page dialog for changing various preferences

class PrefsDialog : public wxPropertySheetDialog
{
public:
    PrefsDialog(wxWindow* parent, const wxString& page);
    ~PrefsDialog() { delete onetimer; }

    wxPanel* CreateFilePrefs(wxWindow* parent);
    wxPanel* CreateEditPrefs(wxWindow* parent);
    wxPanel* CreateControlPrefs(wxWindow* parent);
    wxPanel* CreateViewPrefs(wxWindow* parent);
    wxPanel* CreateLayerPrefs(wxWindow* parent);
    wxPanel* CreateColorPrefs(wxWindow* parent);
    wxPanel* CreateKeyboardPrefs(wxWindow* parent);

    virtual bool TransferDataFromWindow();    // called when user hits OK

#ifdef __WXMAC__
    void OnSpinCtrlChar(wxKeyEvent& event);
#endif

    static void UpdateChosenFile();

private:
    bool GetCheckVal(long id);
    int GetChoiceVal(long id);
    int GetSpinVal(long id);
    int GetRadioVal(long firstid, int numbuttons);
    bool BadSpinVal(int id, int minval, int maxval, const wxString& prefix);
    bool ValidatePage();
    wxBitmapButton* AddColorButton(wxWindow* parent, wxBoxSizer* hbox,
                                   int id, wxColor* rgb, const wxString& text);
    void ChangeButtonColor(int id, wxColor& rgb);
    void UpdateButtonColor(int id, wxColor& rgb);
    void UpdateScrollBar();

    void OnCheckBoxClicked(wxCommandEvent& event);
    void OnColorButton(wxCommandEvent& event);
    void OnPageChanging(wxNotebookEvent& event);
    void OnPageChanged(wxNotebookEvent& event);
    void OnChoice(wxCommandEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnScroll(wxScrollEvent& event);
    void OnOneTimer(wxTimerEvent& event);

    bool ignore_page_event;             // used to prevent currpage being changed
    int algopos1;                       // selected algorithm in PREF_ALGO_MENU1

    int new_algomem[MAX_ALGOS];         // new max mem values for each algorithm
    int new_defbase[MAX_ALGOS];         // new default base step values for each algorithm

    CellBoxes* cellboxes;               // for displaying cell colors/icons
    wxCheckBox* gradcheck;              // use gradient?
    wxCheckBox* iconcheck;              // show icons?
    wxBitmapButton* frombutt;           // button to set gradient's start color
    wxBitmapButton* tobutt;             // button to set gradient's end color
    wxScrollBar* scrollbar;             // for changing number of gradient states

    wxString neweditor;                 // new text editor
    wxString newdownloaddir;            // new directory for downloaded files
    wxString newuserrules;              // new directory for user's rules

    wxTimer* onetimer;                  // one shot timer (see OnOneTimer)

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(PrefsDialog, wxPropertySheetDialog)
EVT_CHECKBOX               (wxID_ANY,        PrefsDialog::OnCheckBoxClicked)
EVT_BUTTON                 (wxID_ANY,        PrefsDialog::OnColorButton)
EVT_NOTEBOOK_PAGE_CHANGING (wxID_ANY,        PrefsDialog::OnPageChanging)
EVT_NOTEBOOK_PAGE_CHANGED  (wxID_ANY,        PrefsDialog::OnPageChanged)
EVT_CHOICE                 (wxID_ANY,        PrefsDialog::OnChoice)
EVT_BUTTON                 (wxID_ANY,        PrefsDialog::OnButton)
EVT_COMMAND_SCROLL         (PREF_SCROLL_BAR, PrefsDialog::OnScroll)
EVT_TIMER                  (wxID_ANY,        PrefsDialog::OnOneTimer)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

// define a text control for showing current key combination

class KeyComboCtrl : public wxTextCtrl
{
public:
    KeyComboCtrl(wxWindow* parent, wxWindowID id, const wxString& value,
                 const wxPoint& pos, const wxSize& size, int style = 0)
    : wxTextCtrl(parent, id, value, pos, size, style) {}
    ~KeyComboCtrl() {}

    // handlers to intercept keyboard events
    void OnKeyDown(wxKeyEvent& event);
    void OnChar(wxKeyEvent& event);

private:
    int realkey;            // key code set by OnKeyDown
    wxString debugkey;      // display debug info for OnKeyDown and OnChar

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(KeyComboCtrl, wxTextCtrl)
EVT_KEY_DOWN  (KeyComboCtrl::OnKeyDown)
EVT_CHAR      (KeyComboCtrl::OnChar)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

void KeyComboCtrl::OnKeyDown(wxKeyEvent& event)
{
    realkey = event.GetKeyCode();
    int mods = event.GetModifiers();

    if (debuglevel == 1) {
        // set debugkey now but don't show it until OnChar
        debugkey = wxString::Format(_("OnKeyDown: key=%d (%c) mods=%d"),
                                    realkey, realkey < 128 ? wxChar(realkey) : wxChar('?'), mods);
    }

    if (realkey == WXK_ESCAPE) {
        // escape key is reserved for other uses
        Beep();
        return;
    }

#ifdef __WXOSX__
    // pass arrow key or function key or delete key directly to OnChar
    if ( (realkey >=  WXK_LEFT && realkey <= WXK_DOWN) ||
        (realkey >= WXK_F1 && realkey <= WXK_F24) || realkey == WXK_BACK ) {
        OnChar(event);
        return;
    }
#endif

    // WARNING: logic must match that in PatternView::OnKeyDown
    if (mods == wxMOD_NONE || realkey > 127) {
        // tell OnChar handler to ignore realkey
        realkey = 0;
    }

#ifdef __WXOSX__
    // pass ctrl/cmd-key combos directly to OnChar
    if (realkey > 0 && ((mods & wxMOD_CONTROL) || (mods & wxMOD_CMD))) {
        OnChar(event);
        return;
    }
#endif

#ifdef __WXMAC__
    // prevent ctrl-[ cancelling dialog (it translates to escape)
    if (realkey == '[' && (mods & wxMOD_CONTROL)) {
        OnChar(event);
        return;
    }
    // avoid translating option-E/I/N/U/`
    if (mods == wxMOD_ALT && (realkey == 'E' || realkey == 'I' || realkey == 'N' ||
                              realkey == 'U' || realkey == '`')) {
        OnChar(event);
        return;
    }
#endif

#ifdef __WXMSW__
    // on Windows, OnChar is NOT called for some ctrl-key combos like
    // ctrl-0..9 or ctrl-alt-key, so we call OnChar ourselves
    if (realkey > 0 && (mods & wxMOD_CONTROL)) {
        OnChar(event);
        return;
    }
#endif

/* this didn't work!!! -- OnKeyDown is not getting called
#ifdef __WXGTK__
    // on Linux we need to avoid alt-C/O selecting Cancel/OK button
    if ((realkey == 'C' || realkey == 'O') && mods == wxMOD_ALT) {
        OnChar(event);
        return;
    }
#endif
*/

    event.Skip();
}

// -----------------------------------------------------------------------------

static bool inonchar = false;

void KeyComboCtrl::OnChar(wxKeyEvent& event)
{
    // avoid infinite recursion due to ChangeValue call below
    if (inonchar) { event.Skip(); return; }
    inonchar = true;

    int key = event.GetKeyCode();
    int mods = event.GetModifiers();

    if (debuglevel == 1) {
        debugkey += wxString::Format(_("\nOnChar: key=%d (%c) mods=%d"),
                                     key, key < 128 ? wxChar(key) : wxChar('?'), mods);
        Warning(debugkey);
    }

    // WARNING: logic must match that in PatternView::OnChar
    if (realkey > 0 && mods != wxMOD_NONE) {
#ifdef __WXGTK__
        // sigh... wxGTK returns inconsistent results for shift-comma combos
        // so we assume that '<' is produced by pressing shift-comma
        // (which might only be true for US keyboards)
        if (key == '<' && (mods & wxMOD_SHIFT)) realkey = ',';
#endif
#ifdef __WXMSW__
        // sigh... wxMSW returns inconsistent results for some shift-key combos
        // so again we assume we're using a US keyboard
        if (key == '~' && (mods & wxMOD_SHIFT)) realkey = '`';
        if (key == '+' && (mods & wxMOD_SHIFT)) realkey = '=';
#endif
        if (mods == wxMOD_SHIFT && key != realkey) {
            // use translated key code but remove shift key;
            // eg. we want shift-'/' to be seen as '?'
            mods = wxMOD_NONE;
        } else {
            // use key code seen by OnKeyDown
            key = realkey;
            if (key >= 'A' && key <= 'Z') key += 32;  // convert A..Z to a..z
        }
    }

    // convert wx key and mods to our internal key code and modifiers
    // and, if they are valid, display the key combo and update the action
    if ( ConvertKeyAndModifiers(key, mods, &currkey, &currmods) ) {
        wxChoice* actionmenu = (wxChoice*) FindWindowById(PREF_ACTION);
        if (actionmenu) {
            wxString keystring = GetKeyCombo(currkey, currmods);
            if (!keystring.IsEmpty()) {
                ChangeValue(keystring);
            } else {
                currkey = 0;
                currmods = 0;
                ChangeValue(_("UNKNOWN KEY"));
            }
            actionmenu->SetSelection(keyaction[currkey][currmods].id);
            PrefsDialog::UpdateChosenFile();
            SetFocus();
            SetSelection(ALL_TEXT);
        } else {
            Warning(_("Failed to find wxChoice control!"));
        }
    } else {
        // unsupported key combo
        Beep();
    }

    // do NOT pass event on to next handler
    // event.Skip();

    inonchar = false;
}

// -----------------------------------------------------------------------------

#ifdef __WXMAC__

// override key event handler for wxSpinCtrl to allow key checking
// and to get tab key navigation to work correctly
class MySpinCtrl : public wxSpinCtrl
{
public:
    MySpinCtrl(wxWindow* parent, wxWindowID id, const wxString& str,
               const wxPoint& pos, const wxSize& size)
    : wxSpinCtrl(parent, id, str, pos, size)
    {
        // create a dynamic event handler for the underlying wxTextCtrl
        wxTextCtrl* textctrl = GetText();
        if (textctrl) {
            textctrl->Connect(wxID_ANY, wxEVT_CHAR,
                              wxKeyEventHandler(PrefsDialog::OnSpinCtrlChar));
        }
    }
};

void PrefsDialog::OnSpinCtrlChar(wxKeyEvent& event)
{
    int key = event.GetKeyCode();

    if (event.CmdDown()) {
        // allow handling of cmd-x/v/etc
        event.Skip();

    } else if ( key == WXK_TAB ) {
        // note that FindFocus() returns pointer to wxTextCtrl window in wxSpinCtrl
        if ( currpage == FILE_PAGE ) {
            wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_MAX_PATTERNS);
            wxSpinCtrl* s2 = (wxSpinCtrl*) FindWindowById(PREF_MAX_SCRIPTS);
            wxTextCtrl* t1 = s1->GetText();
            wxTextCtrl* t2 = s2->GetText();
            wxWindow* focus = FindFocus();
            if ( focus == t1 ) { s2->SetFocus(); s2->SetSelection(ALL_TEXT); }
            if ( focus == t2 ) { s1->SetFocus(); s1->SetSelection(ALL_TEXT); }
        } else if ( currpage == EDIT_PAGE ) {
            // only one spin ctrl on this page
            wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_RANDOM_FILL);
            if ( s1 ) { s1->SetFocus(); s1->SetSelection(ALL_TEXT); }
        } else if ( currpage == CONTROL_PAGE ) {
            wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_MAX_MEM);
            wxSpinCtrl* s2 = (wxSpinCtrl*) FindWindowById(PREF_BASE_STEP);
            wxSpinCtrl* s3 = (wxSpinCtrl*) FindWindowById(PREF_MIN_DELAY);
            wxSpinCtrl* s4 = (wxSpinCtrl*) FindWindowById(PREF_MAX_DELAY);
            wxTextCtrl* t1 = s1->GetText();
            wxTextCtrl* t2 = s2->GetText();
            wxTextCtrl* t3 = s3->GetText();
            wxTextCtrl* t4 = s4->GetText();
            wxWindow* focus = FindFocus();
            if ( focus == t1 ) { s2->SetFocus(); s2->SetSelection(ALL_TEXT); }
            if ( focus == t2 ) { s3->SetFocus(); s3->SetSelection(ALL_TEXT); }
            if ( focus == t3 ) { s4->SetFocus(); s4->SetSelection(ALL_TEXT); }
            if ( focus == t4 ) { s1->SetFocus(); s1->SetSelection(ALL_TEXT); }
        } else if ( currpage == VIEW_PAGE ) {
            wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_BOLD_SPACING);
            wxSpinCtrl* s2 = (wxSpinCtrl*) FindWindowById(PREF_SENSITIVITY);
            wxSpinCtrl* s3 = (wxSpinCtrl*) FindWindowById(PREF_THUMB_RANGE);
            wxTextCtrl* t1 = s1->GetText();
            wxTextCtrl* t2 = s2->GetText();
            wxTextCtrl* t3 = s3->GetText();
            wxWindow* focus = FindFocus();
            wxCheckBox* checkbox = (wxCheckBox*) FindWindowById(PREF_SHOW_BOLD);
            if (checkbox) {
                if (checkbox->GetValue()) {
                    if ( focus == t1 ) { s2->SetFocus(); s2->SetSelection(ALL_TEXT); }
                    if ( focus == t2 ) { s3->SetFocus(); s3->SetSelection(ALL_TEXT); }
                    if ( focus == t3 ) { s1->SetFocus(); s1->SetSelection(ALL_TEXT); }
                } else {
                    if ( focus == t2 ) { s3->SetFocus(); s3->SetSelection(ALL_TEXT); }
                    if ( focus == t3 ) { s2->SetFocus(); s2->SetSelection(ALL_TEXT); }
                }
            } else {
                Beep();
            }
        } else if ( currpage == LAYER_PAGE ) {
            wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_OPACITY);
            wxSpinCtrl* s2 = (wxSpinCtrl*) FindWindowById(PREF_TILE_BORDER);
            wxTextCtrl* t1 = s1->GetText();
            wxTextCtrl* t2 = s2->GetText();
            wxWindow* focus = FindFocus();
            if ( focus == t1 ) { s2->SetFocus(); s2->SetSelection(ALL_TEXT); }
            if ( focus == t2 ) { s1->SetFocus(); s1->SetSelection(ALL_TEXT); }
        } else if ( currpage == COLOR_PAGE ) {
            // no spin ctrls on this page
        } else if ( currpage == KEYBOARD_PAGE ) {
            // no spin ctrls on this page
        }

    } else if ( key >= ' ' && key <= '~' ) {
        if ( key >= '0' && key <= '9' ) {
            // allow digits
            event.Skip();
        } else {
            // disallow any other displayable ascii char
            Beep();
        }

    } else {
        event.Skip();
    }
}

#else

#define MySpinCtrl wxSpinCtrl

#endif // !__WXMAC__

// -----------------------------------------------------------------------------

PrefsDialog::PrefsDialog(wxWindow* parent, const wxString& page)
{
    // not using validators so no need for this:
    // SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);

    Create(parent, wxID_ANY, _("Preferences"));
    CreateButtons(wxOK | wxCANCEL);

    wxBookCtrlBase* notebook = GetBookCtrl();

    wxPanel* filePrefs = CreateFilePrefs(notebook);
    wxPanel* editPrefs = CreateEditPrefs(notebook);
    wxPanel* ctrlPrefs = CreateControlPrefs(notebook);
    wxPanel* viewPrefs = CreateViewPrefs(notebook);
    wxPanel* layerPrefs = CreateLayerPrefs(notebook);
    wxPanel* colorPrefs = CreateColorPrefs(notebook);
    wxPanel* keyboardPrefs = CreateKeyboardPrefs(notebook);

    // AddPage and SetSelection cause OnPageChanging and OnPageChanged to be called
    // so we use a flag to prevent currpage being changed (and unnecessary validation)
    ignore_page_event = true;

    notebook->AddPage(filePrefs, _("File"));
    notebook->AddPage(editPrefs, _("Edit"));
    notebook->AddPage(ctrlPrefs, _("Control"));
    notebook->AddPage(viewPrefs, _("View"));
    notebook->AddPage(layerPrefs, _("Layer"));
    notebook->AddPage(colorPrefs, _("Color"));
    notebook->AddPage(keyboardPrefs, _("Keyboard"));

    if (!page.IsEmpty()) {
        if (page == wxT("file"))            currpage = FILE_PAGE;
        else if (page == wxT("edit"))       currpage = EDIT_PAGE;
        else if (page == wxT("control"))    currpage = CONTROL_PAGE;
        else if (page == wxT("view"))       currpage = VIEW_PAGE;
        else if (page == wxT("layer"))      currpage = LAYER_PAGE;
        else if (page == wxT("color"))      currpage = COLOR_PAGE;
        else if (page == wxT("keyboard"))   currpage = KEYBOARD_PAGE;
    }

    // show the desired page
    notebook->SetSelection(currpage);

    ignore_page_event = false;

    LayoutDialog();

    // ensure top text box has focus and text is selected by creating
    // a one-shot timer which will call OnOneTimer after short delay
    onetimer = new wxTimer(this, wxID_ANY);
    if (onetimer) onetimer->Start(10, wxTIMER_ONE_SHOT);
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnOneTimer(wxTimerEvent& WXUNUSED(event))
{
    MySpinCtrl* s1 = NULL;

    if (currpage == FILE_PAGE) {
        s1 = (MySpinCtrl*) FindWindowById(PREF_MAX_PATTERNS);

    } else if (currpage == EDIT_PAGE) {
        s1 = (MySpinCtrl*) FindWindowById(PREF_RANDOM_FILL);

    } else if (currpage == CONTROL_PAGE) {
        s1 = (MySpinCtrl*) FindWindowById(PREF_MAX_MEM);

    } else if (currpage == VIEW_PAGE) {
        s1 = (MySpinCtrl*) FindWindowById(showgridlines ? PREF_BOLD_SPACING : PREF_SENSITIVITY);

    } else if (currpage == LAYER_PAGE) {
        s1 = (MySpinCtrl*) FindWindowById(PREF_OPACITY);

    } else if (currpage == COLOR_PAGE) {
        // no spin ctrls on this page
        return;

    } else if (currpage == KEYBOARD_PAGE) {
        KeyComboCtrl* k = (KeyComboCtrl*) FindWindowById(PREF_KEYCOMBO);
        if (k) {
            k->SetFocus();
            k->SetSelection(ALL_TEXT);
        }
        return;
    }

    if (s1) {
        s1->SetFocus();
        s1->SetSelection(ALL_TEXT);
    }
}

// -----------------------------------------------------------------------------

// these consts are used to get nicely spaced controls on each platform:

#ifdef __WXMAC__
    #define GROUPGAP (12)      // vertical gap between a group of controls
    #define SBTOPGAP (2)       // vertical gap before first item in wxStaticBoxSizer
    #define SBBOTGAP (2)       // vertical gap after last item in wxStaticBoxSizer
#if wxCHECK_VERSION(3,0,0)
    #define SVGAP (8)          // vertical gap above wxSpinCtrl box
    #define S2VGAP (6)         // vertical gap between 2 wxSpinCtrl boxes
    #define SPINGAP (6)        // horizontal gap around each wxSpinCtrl box
#else
    #define SVGAP (4)          // vertical gap above wxSpinCtrl box
    #define S2VGAP (0)         // vertical gap between 2 wxSpinCtrl boxes
    #define SPINGAP (3)        // horizontal gap around each wxSpinCtrl box
#endif
    #define CH2VGAP (6)        // vertical gap between 2 check/radio boxes
    #define CVGAP (9)          // vertical gap above wxChoice box
    #define LRGAP (5)          // space left and right of vertically stacked boxes
    #define CHOICEGAP (6)      // horizontal gap to left of wxChoice box
#elif defined(__WXMSW__)
    #define GROUPGAP (10)
    #define SBTOPGAP (7)
    #define SBBOTGAP (7)
    #define SVGAP (7)
    #define S2VGAP (5)
    #define CH2VGAP (8)
    #define CVGAP (7)
    #define LRGAP (5)
    #define SPINGAP (6)
    #define CHOICEGAP (6)
#else // assume Linux
    #define GROUPGAP (10)
    #define SBTOPGAP (12)
    #define SBBOTGAP (7)
    #define SVGAP (7)
    #define S2VGAP (5)
    #define CH2VGAP (8)
    #define CVGAP (7)
    #define LRGAP (5)
    #define SPINGAP (6)
    #define CHOICEGAP (6)
#endif

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateFilePrefs(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    wxArrayString newcursorChoices;
    newcursorChoices.Add(wxT("Draw"));
    newcursorChoices.Add(wxT("Pick"));
    newcursorChoices.Add(wxT("Select"));
    newcursorChoices.Add(wxT("Move"));
    newcursorChoices.Add(wxT("Zoom In"));
    newcursorChoices.Add(wxT("Zoom Out"));
    newcursorChoices.Add(wxT("No Change"));

    wxArrayString opencursorChoices = newcursorChoices;

    wxArrayString newscaleChoices;
    newscaleChoices.Add(wxT("1:1"));
    newscaleChoices.Add(wxT("1:2"));
    newscaleChoices.Add(wxT("1:4"));
    newscaleChoices.Add(wxT("1:8"));
    newscaleChoices.Add(wxT("1:16"));
    newscaleChoices.Add(wxT("1:32"));

    // on new pattern

    wxStaticBox* sbox1 = new wxStaticBox(panel, wxID_ANY, _("On creating a new pattern:"));
    wxBoxSizer* ssizer1 = new wxStaticBoxSizer(sbox1, wxVERTICAL);

    wxCheckBox* check1 = new wxCheckBox(panel, PREF_NEW_REM_SEL, _("Remove selection"));
    wxBoxSizer* check1box = new wxBoxSizer(wxHORIZONTAL);
    check1box->Add(check1, 0, wxALL, 0);

    wxBoxSizer* setcurs1 = new wxBoxSizer(wxHORIZONTAL);
    setcurs1->Add(new wxStaticText(panel, wxID_STATIC, _("Set cursor:")), 0, wxALL, 0);

    wxBoxSizer* setscalebox = new wxBoxSizer(wxHORIZONTAL);
    setscalebox->Add(new wxStaticText(panel, wxID_STATIC, _("Set scale:")), 0, wxALL, 0);

    wxChoice* choice3 = new wxChoice(panel, PREF_NEW_CURSOR,
                                     wxDefaultPosition, wxDefaultSize,
                                     newcursorChoices);

    wxChoice* choice1 = new wxChoice(panel, PREF_NEW_SCALE,
                                     wxDefaultPosition, wxDefaultSize,
                                     newscaleChoices);

    wxBoxSizer* hbox1 = new wxBoxSizer(wxHORIZONTAL);
    hbox1->Add(check1box, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox1->AddStretchSpacer(20);
    hbox1->Add(setcurs1, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox1->Add(choice3, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);
    hbox1->AddStretchSpacer(20);

    wxBoxSizer* hbox2 = new wxBoxSizer(wxHORIZONTAL);
    hbox2->Add(setscalebox, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox2->Add(choice1, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);

    ssizer1->AddSpacer(SBTOPGAP);
    ssizer1->Add(hbox1, 1, wxGROW | wxLEFT | wxRIGHT, LRGAP);
    ssizer1->AddSpacer(CVGAP);
    ssizer1->Add(hbox2, 0, wxLEFT | wxRIGHT, LRGAP);
    ssizer1->AddSpacer(SBBOTGAP);

    // on opening pattern

    wxStaticBox* sbox2 = new wxStaticBox(panel, wxID_ANY, _("On opening a pattern file or the clipboard:"));
    wxBoxSizer* ssizer2 = new wxStaticBoxSizer(sbox2, wxVERTICAL);

    wxCheckBox* check2 = new wxCheckBox(panel, PREF_OPEN_REM_SEL, _("Remove selection"));
    wxBoxSizer* check2box = new wxBoxSizer(wxHORIZONTAL);
    check2box->Add(check2, 0, wxALL, 0);

    wxChoice* choice4 = new wxChoice(panel, PREF_OPEN_CURSOR,
                                     wxDefaultPosition, wxDefaultSize,
                                     opencursorChoices);

    wxBoxSizer* setcurs2 = new wxBoxSizer(wxHORIZONTAL);
    setcurs2->Add(new wxStaticText(panel, wxID_STATIC, _("Set cursor:")), 0, wxALL, 0);

    wxBoxSizer* hbox4 = new wxBoxSizer(wxHORIZONTAL);
    hbox4->Add(check2box, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox4->AddStretchSpacer(20);
    hbox4->Add(setcurs2, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox4->Add(choice4, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);
    hbox4->AddStretchSpacer(20);

    ssizer2->AddSpacer(SBTOPGAP);
    ssizer2->Add(hbox4, 1, wxGROW | wxLEFT | wxRIGHT, LRGAP);
    ssizer2->AddSpacer(SBBOTGAP);

    // max_patterns and max_scripts

    wxBoxSizer* maxbox = new wxBoxSizer(wxHORIZONTAL);
    maxbox->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum number of recent patterns:")),
                0, wxALL, 0);

    wxBoxSizer* minbox = new wxBoxSizer(wxHORIZONTAL);
    minbox->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum number of recent scripts:")),
                0, wxALL, 0);

    // align spin controls by setting minbox same width as maxbox
    minbox->SetMinSize( maxbox->GetMinSize() );

    wxSpinCtrl* spin1 = new MySpinCtrl(panel, PREF_MAX_PATTERNS, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, wxDefaultCoord));

    wxSpinCtrl* spin2 = new MySpinCtrl(panel, PREF_MAX_SCRIPTS, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, wxDefaultCoord));

    wxBoxSizer* hpbox = new wxBoxSizer(wxHORIZONTAL);
    hpbox->Add(maxbox, 0, wxALIGN_CENTER_VERTICAL, 0);
    hpbox->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

    wxBoxSizer* hsbox = new wxBoxSizer(wxHORIZONTAL);
    hsbox->Add(minbox, 0, wxALIGN_CENTER_VERTICAL, 0);
    hsbox->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

    wxButton* editorbutt = new wxButton(panel, PREF_EDITOR_BUTT, _("Text Editor..."));
    wxStaticText* editorbox = new wxStaticText(panel, PREF_EDITOR_BOX, texteditor);
    neweditor = texteditor;

    wxButton* downloadbutt = new wxButton(panel, PREF_DOWNLOAD_BUTT, _("Downloads..."));
    wxStaticText* downloadbox = new wxStaticText(panel, PREF_DOWNLOAD_BOX, downloaddir);
    newdownloaddir = downloaddir;

    wxBoxSizer* hebox = new wxBoxSizer(wxHORIZONTAL);
    hebox->Add(editorbutt, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    hebox->Add(editorbox, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, LRGAP);

    wxBoxSizer* hdbox = new wxBoxSizer(wxHORIZONTAL);
    hdbox->Add(downloadbutt, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    hdbox->Add(downloadbox, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, LRGAP);

    vbox->Add(ssizer1, 0, wxGROW | wxALL, 2);
    vbox->AddSpacer(10);
    vbox->Add(ssizer2, 0, wxGROW | wxALL, 2);
    vbox->AddSpacer(10);
    vbox->Add(hpbox, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(S2VGAP);
    vbox->Add(hsbox, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(10);
    vbox->Add(hebox, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(10);
    vbox->Add(hdbox, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(5);

    // init control values
    check1->SetValue(newremovesel);
    check2->SetValue(openremovesel);
    choice1->SetSelection(newmag);
    newcursindex = CursorToIndex(newcurs);
    opencursindex = CursorToIndex(opencurs);
    choice3->SetSelection(newcursindex);
    choice4->SetSelection(opencursindex);
    spin1->SetRange(1, MAX_RECENT); spin1->SetValue(maxpatterns);
    spin2->SetRange(1, MAX_RECENT); spin2->SetValue(maxscripts);
    spin1->SetFocus();
    spin1->SetSelection(ALL_TEXT);

    topSizer->Add(vbox, 1, wxGROW | wxLEFT | wxALL, 5);
    panel->SetSizer(topSizer);
    topSizer->Fit(panel);
    return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateEditPrefs(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    // random_fill

    wxBoxSizer* hbox1 = new wxBoxSizer(wxHORIZONTAL);
    hbox1->Add(new wxStaticText(panel, wxID_STATIC, _("Random fill percentage:")),
               0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin1 = new MySpinCtrl(panel, PREF_RANDOM_FILL, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, wxDefaultCoord));
    hbox1->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

    // can_change_rule

    wxStaticBox* sbox1 = new wxStaticBox(panel, wxID_ANY, _("When pasting a clipboard pattern:"));
    wxBoxSizer* ssizer1 = new wxStaticBoxSizer(sbox1, wxVERTICAL);

    wxRadioButton* radio0 = new wxRadioButton(panel, PREF_PASTE_0, _("Never change rule"),
                                              wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    wxRadioButton* radio1 = new wxRadioButton(panel, PREF_PASTE_1,
                                              _("Only change rule if one is specified and the universe is empty"));
    wxRadioButton* radio2 = new wxRadioButton(panel, PREF_PASTE_2,
                                              _("Always change rule if one is specified"));

    ssizer1->AddSpacer(SBTOPGAP);
    ssizer1->Add(radio0, 0, wxLEFT | wxRIGHT, LRGAP);
    ssizer1->AddSpacer(CH2VGAP);
    ssizer1->Add(radio1, 0, wxLEFT | wxRIGHT, LRGAP);
    ssizer1->AddSpacer(CH2VGAP);
    ssizer1->Add(radio2, 0, wxLEFT | wxRIGHT, LRGAP);
    ssizer1->AddSpacer(SBBOTGAP);

    // scroll_pencil, scroll_cross, scroll_hand

    wxStaticBox* sbox2 = new wxStaticBox(panel, wxID_ANY,
                                         _("If the cursor is dragged outside the viewport:"));
    wxBoxSizer* ssizer2 = new wxStaticBoxSizer(sbox2, wxVERTICAL);

    wxCheckBox* check1 = new wxCheckBox(panel, PREF_SCROLL_PENCIL,
                                        _("Scroll when drawing cells (using the pencil cursor)"));
    wxCheckBox* check2 = new wxCheckBox(panel, PREF_SCROLL_CROSS,
                                        _("Scroll when selecting cells (using the cross cursor)"));
    wxCheckBox* check3 = new wxCheckBox(panel, PREF_SCROLL_HAND,
                                        _("Scroll when moving view (using the hand cursor)"));

    ssizer2->AddSpacer(SBTOPGAP);
    ssizer2->Add(check1, 0, wxLEFT | wxRIGHT, LRGAP);
    ssizer2->AddSpacer(CH2VGAP);
    ssizer2->Add(check2, 0, wxLEFT | wxRIGHT, LRGAP);
    ssizer2->AddSpacer(CH2VGAP);
    ssizer2->Add(check3, 0, wxLEFT | wxRIGHT, LRGAP);
    ssizer2->AddSpacer(SBBOTGAP);

    // allow_beep

    wxCheckBox* check4 = new wxCheckBox(panel, PREF_BEEP, _("Allow beep sound"));

    // show_selpop

    wxCheckBox* check5 = new wxCheckBox(panel, PREF_SHOW_SEL_POP, _("Show selection population"));

    vbox->AddSpacer(SVGAP);
    vbox->Add(hbox1, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(GROUPGAP);
    vbox->Add(ssizer1, 0, wxGROW | wxALL, 2);
    vbox->AddSpacer(GROUPGAP);
    vbox->Add(ssizer2, 0, wxGROW | wxALL, 2);
    vbox->AddSpacer(GROUPGAP);
    vbox->Add(check4, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(SVGAP);
    vbox->Add(check5, 0, wxLEFT | wxRIGHT, LRGAP);

    // init control values
    spin1->SetRange(1, 100);
    spin1->SetValue(randomfill);
    spin1->SetFocus();
    spin1->SetSelection(ALL_TEXT);
    radio0->SetValue(canchangerule == 0);
    radio1->SetValue(canchangerule == 1);
    radio2->SetValue(canchangerule == 2);
    check1->SetValue(scrollpencil);
    check2->SetValue(scrollcross);
    check3->SetValue(scrollhand);
    check4->SetValue(allowbeep);
    check5->SetValue(showselpop);

    topSizer->Add(vbox, 1, wxGROW | wxLEFT | wxALL, 5);
    panel->SetSizer(topSizer);
    topSizer->Fit(panel);
    return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateControlPrefs(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    // create a choice menu to select algo

    wxArrayString algoChoices;
    for (int i = 0; i < NumAlgos(); i++) {
        algoChoices.Add( wxString(GetAlgoName(i), wxConvLocal) );
    }
    wxChoice* algomenu = new wxChoice(panel, PREF_ALGO_MENU1,
                                      wxDefaultPosition, wxDefaultSize, algoChoices);
    algopos1 = currlayer->algtype;

    wxBoxSizer* longbox = new wxBoxSizer(wxHORIZONTAL);
    longbox->Add(new wxStaticText(panel, wxID_STATIC, _("Settings for this algorithm:")),
                 0, wxALL, 0);

    wxBoxSizer* menubox = new wxBoxSizer(wxHORIZONTAL);
    menubox->Add(longbox, 0, wxALIGN_CENTER_VERTICAL, 0);
    menubox->Add(algomenu, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);

    // maximum memory and base step

    wxBoxSizer* membox = new wxBoxSizer(wxHORIZONTAL);
    membox->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum memory:")), 0, wxALL, 0);

    wxBoxSizer* basebox = new wxBoxSizer(wxHORIZONTAL);
    basebox->Add(new wxStaticText(panel, wxID_STATIC, _("Default base step:")), 0, wxALL, 0);

    // align spin controls
    membox->SetMinSize( longbox->GetMinSize() );
    basebox->SetMinSize( longbox->GetMinSize() );

    wxBoxSizer* hbox1 = new wxBoxSizer(wxHORIZONTAL);
    hbox1->Add(membox, 0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin1 = new MySpinCtrl(panel, PREF_MAX_MEM, wxEmptyString,
                                       wxDefaultPosition, wxSize(80, wxDefaultCoord));
    hbox1->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

    wxString memnote = algoinfo[algopos1]->canhash ? HASH_MEM_NOTE : NONHASH_MEM_NOTE;
    hbox1->Add(new wxStaticText(panel, PREF_MEM_NOTE, memnote),
               0, wxALIGN_CENTER_VERTICAL, 0);

    wxBoxSizer* hbox2 = new wxBoxSizer(wxHORIZONTAL);
    hbox2->Add(basebox, 0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin2 = new MySpinCtrl(panel, PREF_BASE_STEP, wxEmptyString,
                                       wxDefaultPosition, wxSize(80, wxDefaultCoord));
    hbox2->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

    wxString stepnote = algoinfo[algopos1]->canhash ? HASH_STEP_NOTE : NONHASH_STEP_NOTE;
    hbox2->Add(new wxStaticText(panel, PREF_STEP_NOTE, stepnote),
               0, wxALIGN_CENTER_VERTICAL, 0);

    // min_delay and max_delay

    wxBoxSizer* minbox = new wxBoxSizer(wxHORIZONTAL);
    minbox->Add(new wxStaticText(panel, wxID_STATIC, _("Minimum delay:")), 0, wxALL, 0);

    wxBoxSizer* maxbox = new wxBoxSizer(wxHORIZONTAL);
    maxbox->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum delay:")), 0, wxALL, 0);

    // align spin controls
    minbox->SetMinSize( maxbox->GetMinSize() );

    wxBoxSizer* hbox3 = new wxBoxSizer(wxHORIZONTAL);
    hbox3->Add(minbox, 0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin3 = new MySpinCtrl(panel, PREF_MIN_DELAY, wxEmptyString,
                                       wxDefaultPosition, wxSize(80, wxDefaultCoord));
    hbox3->Add(spin3, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
    hbox3->Add(new wxStaticText(panel, wxID_STATIC, _("millisecs")),
               0, wxALIGN_CENTER_VERTICAL, 0);

    wxBoxSizer* hbox4 = new wxBoxSizer(wxHORIZONTAL);
    hbox4->Add(maxbox, 0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin4 = new MySpinCtrl(panel, PREF_MAX_DELAY, wxEmptyString,
                                       wxDefaultPosition, wxSize(80, wxDefaultCoord));
    hbox4->Add(spin4, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
    hbox4->Add(new wxStaticText(panel, wxID_STATIC, _("millisecs")),
               0, wxALIGN_CENTER_VERTICAL, 0);

    // user_rules

    wxButton* rulesbutt = new wxButton(panel, PREF_RULES_BUTT, _("Your Rules..."));
    wxStaticText* rulesbox = new wxStaticText(panel, PREF_RULES_BOX, userrules);
    newuserrules = userrules;

    wxBoxSizer* hrbox = new wxBoxSizer(wxHORIZONTAL);
    hrbox->Add(rulesbutt, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    hrbox->Add(rulesbox, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, LRGAP);

    wxString note = _("Golly looks for .rule files in the above folder before looking in the Rules folder.");
    wxBoxSizer* notebox = new wxBoxSizer(wxHORIZONTAL);
    notebox->Add(new wxStaticText(panel, wxID_STATIC, note));

    // position things
    vbox->AddSpacer(5);
    vbox->Add(menubox, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(SVGAP);
    vbox->Add(hbox1, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(S2VGAP);
    vbox->Add(hbox2, 0, wxLEFT | wxRIGHT, LRGAP);

    vbox->AddSpacer(5);
    vbox->AddSpacer(GROUPGAP);
    vbox->Add(hbox3, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(S2VGAP);
    vbox->Add(hbox4, 0, wxLEFT | wxRIGHT, LRGAP);

    vbox->AddSpacer(15);
    vbox->AddSpacer(GROUPGAP);
    vbox->Add(hrbox, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(15);
    vbox->Add(notebox, 0, wxLEFT, LRGAP);

    // init control values;
    // to avoid a wxGTK bug we use SetRange and SetValue rather than specifying
    // the min,max,init values in the wxSpinCtrl constructor
    spin1->SetRange(MIN_MEM_MB, MAX_MEM_MB);
    spin1->SetValue(algoinfo[algopos1]->algomem);
    spin2->SetRange(2, MAX_BASESTEP);
    spin2->SetValue(algoinfo[algopos1]->defbase);
    spin3->SetRange(0, MAX_DELAY);           spin3->SetValue(mindelay);
    spin4->SetRange(0, MAX_DELAY);           spin4->SetValue(maxdelay);
    spin1->SetFocus();
    spin1->SetSelection(ALL_TEXT);
    algomenu->SetSelection(algopos1);

    for (int i = 0; i < NumAlgos(); i++) {
        new_algomem[i] = algoinfo[i]->algomem;
        new_defbase[i] = algoinfo[i]->defbase;
    }

    topSizer->Add(vbox, 1, wxGROW | wxLEFT | wxALL, 5);
    panel->SetSizer(topSizer);
    topSizer->Fit(panel);
    return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateViewPrefs(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    // show_tips

#if wxUSE_TOOLTIPS
    wxCheckBox* check3 = new wxCheckBox(panel, PREF_SHOW_TIPS, _("Show button tips"));
#endif

    // restore_view

    wxCheckBox* check4 = new wxCheckBox(panel, PREF_RESTORE, _("Reset/Undo will restore view"));

    // math_coords

    wxCheckBox* check1 = new wxCheckBox(panel, PREF_Y_UP, _("Y coordinates increase upwards"));

    // zoomed cell borders

    wxCheckBox* check5 = new wxCheckBox(panel, PREF_CELL_BORDERS, _("Zoomed cells have borders"));

    // show_bold_lines and bold_spacing

    wxBoxSizer* hbox2 = new wxBoxSizer(wxHORIZONTAL);
    wxCheckBox* check2 = new wxCheckBox(panel, PREF_SHOW_BOLD, _("Show bold grid lines every"));

    wxSpinCtrl* spin2 = new MySpinCtrl(panel, PREF_BOLD_SPACING, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, wxDefaultCoord));

    hbox2->Add(check2, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox2->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
    hbox2->Add(new wxStaticText(panel, wxID_STATIC, _("cells")),
               0, wxALIGN_CENTER_VERTICAL, 0);

    // min_grid_mag (2..MAX_MAG)

    wxBoxSizer* hbox3 = new wxBoxSizer(wxHORIZONTAL);

    wxArrayString mingridChoices;
    mingridChoices.Add(wxT("1:4"));
    mingridChoices.Add(wxT("1:8"));
    mingridChoices.Add(wxT("1:16"));
    mingridChoices.Add(wxT("1:32"));
    wxChoice* choice3 = new wxChoice(panel, PREF_MIN_GRID_SCALE,
                                     wxDefaultPosition, wxDefaultSize,
                                     mingridChoices);

    wxBoxSizer* longbox = new wxBoxSizer(wxHORIZONTAL);
    longbox->Add(new wxStaticText(panel, wxID_STATIC, _("Minimum scale for grid:")),
                 0, wxALL, 0);

    hbox3->Add(longbox, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox3->Add(choice3, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);

    // mouse_wheel_mode

    wxBoxSizer* wheelbox = new wxBoxSizer(wxHORIZONTAL);
    wheelbox->Add(new wxStaticText(panel, wxID_STATIC, _("Mouse wheel action:")), 0, wxALL, 0);

    // align by setting wheelbox same width as longbox
    wheelbox->SetMinSize( longbox->GetMinSize() );

    wxArrayString mousewheelChoices;
    mousewheelChoices.Add(_("Disabled"));
    mousewheelChoices.Add(_("Forward zooms out"));
    mousewheelChoices.Add(_("Forward zooms in"));
    wxChoice* choice4 = new wxChoice(panel, PREF_MOUSE_WHEEL, wxDefaultPosition, wxDefaultSize,
                                     mousewheelChoices);

    wxBoxSizer* hbox4 = new wxBoxSizer(wxHORIZONTAL);
    hbox4->Add(wheelbox, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox4->Add(choice4, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);

    // wheel_sensitivity

    wxBoxSizer* senslabel = new wxBoxSizer(wxHORIZONTAL);
    senslabel->Add(new wxStaticText(panel, wxID_STATIC, _("Wheel sensitivity:")),
                   0, wxALIGN_CENTER_VERTICAL, 0);

    senslabel->SetMinSize( longbox->GetMinSize() );

    wxBoxSizer* hbox7 = new wxBoxSizer(wxHORIZONTAL);
    hbox7->Add(senslabel, 0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin4 = new MySpinCtrl(panel, PREF_SENSITIVITY, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, wxDefaultCoord));
    hbox7->Add(spin4, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

    // thumb_range

    wxBoxSizer* thumblabel = new wxBoxSizer(wxHORIZONTAL);
    thumblabel->Add(new wxStaticText(panel, wxID_STATIC, _("Thumb scroll range:")),
                    0, wxALIGN_CENTER_VERTICAL, 0);

    thumblabel->SetMinSize( longbox->GetMinSize() );

    wxBoxSizer* hbox5 = new wxBoxSizer(wxHORIZONTAL);
    hbox5->Add(thumblabel, 0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin5 = new MySpinCtrl(panel, PREF_THUMB_RANGE, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, wxDefaultCoord));
    hbox5->Add(spin5, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
    hbox5->Add(new wxStaticText(panel, wxID_STATIC, _("times view size")),
               0, wxALIGN_CENTER_VERTICAL, 0);

    // controls_pos

    wxBoxSizer* posbox = new wxBoxSizer(wxHORIZONTAL);
    posbox->Add(new wxStaticText(panel, wxID_STATIC, _("Translucent buttons:")), 0, wxALL, 0);

    // align by setting posbox same width as longbox
    posbox->SetMinSize( longbox->GetMinSize() );

    wxArrayString posChoices;
    posChoices.Add(_("Disabled"));
    posChoices.Add(_("Top left corner"));
    posChoices.Add(_("Top right corner"));
    posChoices.Add(_("Bottom right corner"));
    posChoices.Add(_("Bottom left corner"));
    wxChoice* choice5 = new wxChoice(panel, PREF_CONTROLS, wxDefaultPosition, wxDefaultSize,
                                     posChoices);

    wxBoxSizer* hbox6 = new wxBoxSizer(wxHORIZONTAL);
    hbox6->Add(posbox, 0, wxALIGN_CENTER_VERTICAL, 0);
    hbox6->Add(choice5, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);

    // position things
    vbox->AddSpacer(5);
#if wxUSE_TOOLTIPS
    vbox->Add(check3, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(CH2VGAP + 3);
#endif
    vbox->Add(check4, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(CH2VGAP + 3);
    vbox->Add(check1, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(SVGAP);
    vbox->Add(check5, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(SVGAP);
    vbox->Add(hbox2, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(SVGAP);
#ifdef __WXMAC__
    vbox->AddSpacer(10);
#endif
    vbox->Add(hbox3, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(CVGAP);
    vbox->Add(hbox4, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(SVGAP);
    vbox->Add(hbox7, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(SVGAP);
    vbox->Add(hbox5, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(SVGAP);
    vbox->Add(hbox6, 0, wxLEFT | wxRIGHT, LRGAP);

    // init control values
#if wxUSE_TOOLTIPS
    check3->SetValue(showtips);
#endif
    check4->SetValue(restoreview);
    check1->SetValue(mathcoords);
    check5->SetValue(cellborders);
    check2->SetValue(showboldlines);
    spin4->SetRange(1, MAX_SENSITIVITY); spin4->SetValue(wheelsens);
    spin5->SetRange(2, MAX_THUMBRANGE);  spin5->SetValue(thumbrange);
    spin2->SetRange(2, MAX_SPACING);     spin2->SetValue(boldspacing);
    spin2->Enable(showboldlines);
    if (showboldlines) {
        spin2->SetFocus();
        spin2->SetSelection(ALL_TEXT);
    } else {
        spin4->SetFocus();
        spin4->SetSelection(ALL_TEXT);
    }
    mingridindex = mingridmag - 2;
    choice3->SetSelection(mingridindex);
    choice4->SetSelection(mousewheelmode);
    choice5->SetSelection(controlspos);

    topSizer->Add(vbox, 1, wxGROW | wxLEFT | wxALL, 5);
    panel->SetSizer(topSizer);
    topSizer->Fit(panel);
    return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateLayerPrefs(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    // opacity

    wxBoxSizer* opacitybox = new wxBoxSizer(wxHORIZONTAL);
    opacitybox->Add(new wxStaticText(panel, wxID_STATIC,
                                     _("Opacity percentage when drawing stacked layers:")),
                    0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin1 = new MySpinCtrl(panel, PREF_OPACITY, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, wxDefaultCoord));
    opacitybox->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

    // tile_border

    wxBoxSizer* borderbox = new wxBoxSizer(wxHORIZONTAL);
    borderbox->Add(new wxStaticText(panel, wxID_STATIC,
                                    _("Border thickness for tiled layers:")),
                   0, wxALIGN_CENTER_VERTICAL, 0);
    wxSpinCtrl* spin2 = new MySpinCtrl(panel, PREF_TILE_BORDER, wxEmptyString,
                                       wxDefaultPosition, wxSize(70, wxDefaultCoord));
    borderbox->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

    // ask_on_new, ask_on_load, ask_on_delete, ask_on_quit, warn_on_save

    wxStaticBox* sbox1 = new wxStaticBox(panel, wxID_ANY, _("Ask to save changes to layer before:"));
    wxBoxSizer* ssizer1 = new wxStaticBoxSizer(sbox1, wxVERTICAL);

    wxCheckBox* check1 = new wxCheckBox(panel, PREF_ASK_NEW, _("Creating a new pattern"));
    wxCheckBox* check2 = new wxCheckBox(panel, PREF_ASK_LOAD, _("Opening a pattern file"));
    wxCheckBox* check3 = new wxCheckBox(panel, PREF_ASK_DELETE, _("Deleting layer"));
    wxCheckBox* check4 = new wxCheckBox(panel, PREF_ASK_QUIT, _("Quitting application"));
    wxCheckBox* check5 = new wxCheckBox(panel, PREF_WARN_SAVE, _("Warn if saving non-starting generation"));

    wxBoxSizer* b1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* b2 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* b3 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* b4 = new wxBoxSizer(wxHORIZONTAL);
    b1->Add(check1, 0, wxALL, 0);
    b2->Add(check2, 0, wxALL, 0);
    b3->Add(check3, 0, wxALL, 0);
    b4->Add(check4, 0, wxALL, 0);
    wxSize wd1 = b1->GetMinSize();
    wxSize wd2 = b2->GetMinSize();
    wxSize wd3 = b3->GetMinSize();
    wxSize wd4 = b4->GetMinSize();
    if (wd1.GetWidth() > wd2.GetWidth())
        b2->SetMinSize(wd1);
    else
        b1->SetMinSize(wd2);
    if (wd3.GetWidth() > wd4.GetWidth())
        b4->SetMinSize(wd3);
    else
        b3->SetMinSize(wd4);

    wxBoxSizer* hbox1 = new wxBoxSizer(wxHORIZONTAL);
    hbox1->Add(b1, 0, wxLEFT | wxRIGHT, LRGAP);
    hbox1->AddStretchSpacer(20);
    hbox1->Add(b3, 0, wxLEFT | wxRIGHT, LRGAP);
    hbox1->AddStretchSpacer(20);

    wxBoxSizer* hbox2 = new wxBoxSizer(wxHORIZONTAL);
    hbox2->Add(b2, 0, wxLEFT | wxRIGHT, LRGAP);
    hbox2->AddStretchSpacer(20);
    hbox2->Add(b4, 0, wxLEFT | wxRIGHT, LRGAP);
    hbox2->AddStretchSpacer(20);

    ssizer1->AddSpacer(SBTOPGAP);
    ssizer1->Add(hbox1, 1, wxGROW | wxLEFT | wxRIGHT, LRGAP);
    ssizer1->AddSpacer(CH2VGAP);
    ssizer1->Add(hbox2, 1, wxGROW | wxLEFT | wxRIGHT, LRGAP);
    ssizer1->AddSpacer(SBBOTGAP);

    // position things
    vbox->AddSpacer(SVGAP);
    vbox->Add(opacitybox, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(S2VGAP);
    vbox->Add(borderbox, 0, wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(GROUPGAP);
    vbox->Add(ssizer1, 0, wxGROW | wxALL, 2);
    vbox->AddSpacer(GROUPGAP);
    vbox->Add(check5, 0, wxLEFT | wxRIGHT, LRGAP);

    // init control values
    spin1->SetRange(1, 100);
    spin1->SetValue(opacity);

    spin2->SetRange(1, 10);
    spin2->SetValue(tileborder);

    spin1->SetFocus();
    spin1->SetSelection(ALL_TEXT);

    check1->SetValue(askonnew);
    check2->SetValue(askonload);
    check3->SetValue(askondelete);
    check4->SetValue(askonquit);
    check5->SetValue(warn_on_save);

    topSizer->Add(vbox, 1, wxGROW | wxLEFT | wxALL, 5);
    panel->SetSizer(topSizer);
    topSizer->Fit(panel);
    return panel;
}

// -----------------------------------------------------------------------------

wxBitmapButton* PrefsDialog::AddColorButton(wxWindow* parent, wxBoxSizer* hbox,
                                            int id, wxColor* rgb, const wxString& text)
{
    wxBitmap bitmap(BITMAP_WD, BITMAP_HT);
    wxMemoryDC dc;
    dc.SelectObject(bitmap);
    wxRect rect(0, 0, BITMAP_WD, BITMAP_HT);
    wxBrush brush(*rgb);
    FillRect(dc, rect, brush);
    dc.SelectObject(wxNullBitmap);

    wxBitmapButton* bb = new wxBitmapButton(parent, id, bitmap, wxPoint(0,0),
#if defined(__WXOSX_COCOA__)
                                            wxSize(BITMAP_WD + 12, BITMAP_HT + 12));
#else
                                            wxDefaultSize);
#endif
    if (bb) {
        hbox->Add(new wxStaticText(parent, wxID_STATIC, text), 0, wxALIGN_CENTER_VERTICAL, 0);
        hbox->Add(bb, 0, wxALIGN_CENTER_VERTICAL, 0);
    }
    return bb;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateColorPrefs(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    // create a choice menu to select algo
    wxArrayString algoChoices;
    for (int i = 0; i < NumAlgos(); i++) {
        algoChoices.Add( wxString(GetAlgoName(i), wxConvLocal) );
    }
    wxChoice* algomenu = new wxChoice(panel, PREF_ALGO_MENU2,
                                      wxDefaultPosition, wxDefaultSize, algoChoices);
    coloralgo = currlayer->algtype;
    algomenu->SetSelection(coloralgo);

    // create bitmap buttons
    wxBoxSizer* statusbox = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* frombox = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* tobox = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* colorbox = new wxBoxSizer(wxHORIZONTAL);
    AddColorButton(panel, statusbox, PREF_STATUS_BUTT,
                   &algoinfo[coloralgo]->statusrgb, _("Status bar: "));
    frombutt = AddColorButton(panel, frombox, PREF_FROM_BUTT, &algoinfo[coloralgo]->fromrgb, _(""));
    tobutt = AddColorButton(panel, tobox, PREF_TO_BUTT, &algoinfo[coloralgo]->torgb, _(" to "));
    AddColorButton(panel, colorbox, PREF_SELECT_BUTT, selectrgb, _("Selection: "));
    // don't use AddSpacer(20) because that will also add 20 *vertical* units!
    colorbox->AddSpacer(10);
    colorbox->AddSpacer(10);
    AddColorButton(panel, colorbox, PREF_PASTE_BUTT, pastergb, _("Paste: "));
    // don't use AddSpacer(20) because that will also add 20 *vertical* units!
    colorbox->AddSpacer(10);
    colorbox->AddSpacer(10);
    AddColorButton(panel, colorbox, PREF_BORDER_BUTT, borderrgb, _("Grid border: "));

    wxBoxSizer* algobox = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* algolabel = new wxBoxSizer(wxHORIZONTAL);
    algolabel->Add(new wxStaticText(panel, wxID_STATIC, _("Default colors for:")), 0, wxALL, 0);
    algobox->Add(algolabel, 0, wxALIGN_CENTER_VERTICAL, 0);
    algobox->Add(algomenu, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
    algobox->AddStretchSpacer();
    algobox->Add(statusbox, 0, wxALIGN_CENTER_VERTICAL | wxALL, 0);
    algobox->AddStretchSpacer();

    gradcheck = new wxCheckBox(panel, PREF_GRADIENT_CHECK, _("Use gradient from "));
    gradcheck->SetValue(algoinfo[coloralgo]->gradient);

    wxBoxSizer* gradbox = new wxBoxSizer(wxHORIZONTAL);
    gradbox->Add(gradcheck, 0, wxALIGN_CENTER_VERTICAL, 0);
    gradbox->Add(frombox, 0, wxALIGN_CENTER_VERTICAL, 0);
    gradbox->Add(tobox, 0, wxALIGN_CENTER_VERTICAL, 0);
    gradbox->AddSpacer(10);

    // create scroll bar filling right part of gradbox
    wxSize minsize = gradbox->GetMinSize();
    int scrollbarwd = NUMCOLS*CELLSIZE+1 - minsize.GetWidth();
#ifdef __WXMAC__
    int scrollbarht = 15;   // must be this height on Mac
#else
    int scrollbarht = 16;
#endif
    scrollbar = new wxScrollBar(panel, PREF_SCROLL_BAR, wxDefaultPosition,
                                wxSize(scrollbarwd, scrollbarht), wxSB_HORIZONTAL);
    if (scrollbar == NULL) Fatal(_("Failed to create scroll bar!"));
    gradbox->Add(scrollbar, 0, wxALIGN_CENTER_VERTICAL, 0);

    gradstates = algoinfo[coloralgo]->maxstates;
    UpdateScrollBar();
    scrollbar->Enable(algoinfo[coloralgo]->gradient);
    frombutt->Enable(algoinfo[coloralgo]->gradient);
    tobutt->Enable(algoinfo[coloralgo]->gradient);

    // create child window for displaying cell colors/icons
    cellboxes = new CellBoxes(panel, PREF_CELL_PANEL, wxPoint(0,0),
                              wxSize(NUMCOLS*CELLSIZE+1,NUMROWS*CELLSIZE+1));

    iconcheck = new wxCheckBox(panel, PREF_ICON_CHECK, _("Show icons"));
    iconcheck->SetValue(showicons);

    wxStaticText* statebox = new wxStaticText(panel, PREF_STATE_BOX, _("999"));
    cellboxes->statebox = statebox;
    wxBoxSizer* hbox1 = new wxBoxSizer(wxHORIZONTAL);
    hbox1->Add(statebox, 0, 0, 0);
    hbox1->SetMinSize( hbox1->GetMinSize() );

    wxStaticText* rgbbox = new wxStaticText(panel, PREF_RGB_BOX, _("999,999,999"));
    cellboxes->rgbbox = rgbbox;
    wxBoxSizer* hbox2 = new wxBoxSizer(wxHORIZONTAL);
    hbox2->Add(rgbbox, 0, 0, 0);
    hbox2->SetMinSize( hbox2->GetMinSize() );

    statebox->SetLabel(_(" "));
    rgbbox->SetLabel(_(" "));

    wxBoxSizer* botbox = new wxBoxSizer(wxHORIZONTAL);
    botbox->Add(new wxStaticText(panel, wxID_STATIC, _("State: ")), 0, wxALIGN_CENTER_VERTICAL, 0);
    botbox->Add(hbox1, 0, wxALIGN_CENTER_VERTICAL, 0);
    botbox->Add(20, 0, 0);
    botbox->Add(new wxStaticText(panel, wxID_STATIC, _("RGB: ")), 0, wxALIGN_CENTER_VERTICAL, 0);
    botbox->Add(hbox2, 0, wxALIGN_CENTER_VERTICAL, 0);
    botbox->AddStretchSpacer();
    botbox->Add(iconcheck, 0, wxALIGN_CENTER_VERTICAL, 0);

    //!!! avoid wxMac bug -- can't click on bitmap buttons inside wxStaticBoxSizer
    //!!! wxStaticBox* sbox1 = new wxStaticBox(panel, wxID_ANY, _("Cell colors:"));
    //!!! wxBoxSizer* ssizer1 = new wxStaticBoxSizer(sbox1, wxVERTICAL);
    wxBoxSizer* ssizer1 = new wxBoxSizer(wxVERTICAL);

    ssizer1->AddSpacer(10);
    ssizer1->Add(gradbox, 0, wxLEFT | wxRIGHT, 0);
    ssizer1->AddSpacer(8);
    ssizer1->Add(cellboxes, 0, wxLEFT | wxRIGHT, 0);
    ssizer1->AddSpacer(8);
    ssizer1->Add(botbox, 1, wxGROW | wxLEFT | wxRIGHT, 0);
    ssizer1->AddSpacer(SBBOTGAP);

    wxStaticText* sbox2 = new wxStaticText(panel, wxID_STATIC, _("Global colors used by all algorithms:"));
    wxBoxSizer* ssizer2 = new wxBoxSizer(wxVERTICAL);

    ssizer2->Add(sbox2, 0, 0, 0);
    ssizer2->AddSpacer(10);
    ssizer2->Add(colorbox, 1, wxGROW | wxLEFT | wxRIGHT, 0);

    vbox->AddSpacer(5);
    vbox->Add(algobox, 1, wxGROW | wxLEFT | wxRIGHT, LRGAP);
    vbox->Add(ssizer1, 0, wxGROW | wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(15);
    vbox->Add(ssizer2, 0, wxGROW | wxLEFT | wxRIGHT, LRGAP);
    vbox->AddSpacer(2);

    topSizer->Add(vbox, 1, wxGROW | wxLEFT | wxALL, 5);
    panel->SetSizer(topSizer);
    topSizer->Fit(panel);
    return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateKeyboardPrefs(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    // make sure this is the first control added so it gets focus on a page change
    KeyComboCtrl* keycombo =
    new KeyComboCtrl(panel, PREF_KEYCOMBO, wxEmptyString,
                     wxDefaultPosition, wxSize(230, wxDefaultCoord),
                     wxTE_CENTER
                     | wxTE_PROCESS_TAB
                     | wxTE_PROCESS_ENTER   // so enter key won't select OK on Windows
#ifdef __WXOSX__
                     // avoid wxTE_RICH2 otherwise we see scroll bar
                     );
#else
                     | wxTE_RICH2 );        // better for Windows
#endif

    wxArrayString actionChoices;
    for (int i = 0; i < MAX_ACTIONS; i++) {
        actionChoices.Add( wxString(GetActionName((action_id) i), wxConvLocal) );
    }
    actionChoices[DO_OPENFILE] = _("Open Chosen File");
    wxChoice* actionmenu = new wxChoice(panel, PREF_ACTION,
                                        wxDefaultPosition, wxDefaultSize, actionChoices);

    wxBoxSizer* hbox0 = new wxBoxSizer(wxHORIZONTAL);
    hbox0->Add(new wxStaticText(panel, wxID_STATIC,
                                _("Type a key combination, then select the desired action:")));

    wxBoxSizer* keybox = new wxBoxSizer(wxVERTICAL);
    keybox->Add(new wxStaticText(panel, wxID_STATIC, _("Key Combination")), 0, wxALIGN_CENTER, 0);
    keybox->AddSpacer(5);
    keybox->Add(keycombo, 0, wxALIGN_CENTER, 0);

    wxBoxSizer* actbox = new wxBoxSizer(wxVERTICAL);
    actbox->Add(new wxStaticText(panel, wxID_STATIC, _("Action")), 0, wxALIGN_CENTER, 0);
    actbox->AddSpacer(5);
    actbox->Add(actionmenu, 0, wxALIGN_CENTER, 0);

    wxBoxSizer* hbox1 = new wxBoxSizer(wxHORIZONTAL);
    hbox1->Add(keybox, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, LRGAP);
    hbox1->AddSpacer(15);
    hbox1->Add(actbox, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, LRGAP);

    wxButton* choose = new wxButton(panel, PREF_CHOOSE, _("Choose File..."));
    wxStaticText* filebox = new wxStaticText(panel, PREF_FILE_BOX, wxEmptyString);

    wxBoxSizer* hbox2 = new wxBoxSizer(wxHORIZONTAL);
    hbox2->Add(choose, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, LRGAP);
    hbox2->Add(filebox, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, LRGAP);

    wxBoxSizer* midbox = new wxBoxSizer(wxVERTICAL);
    midbox->Add(hbox1, 0, wxLEFT | wxRIGHT, LRGAP);
    midbox->AddSpacer(15);
    midbox->Add(hbox2, 0, wxLEFT, LRGAP);

    wxString notes = _("Note:");
    notes += _("\n- Different key combinations can be assigned to the same action.");
    notes += _("\n- The Escape key is reserved for hard-wired actions.");
    wxBoxSizer* hbox3 = new wxBoxSizer(wxHORIZONTAL);
    hbox3->Add(new wxStaticText(panel, wxID_STATIC, notes));

    vbox->AddSpacer(5);
    vbox->Add(hbox0, 0, wxLEFT, LRGAP);
    vbox->AddSpacer(15);
    vbox->Add(midbox, 0, wxALIGN_CENTER, 0);
    vbox->AddSpacer(30);
    vbox->Add(hbox3, 0, wxLEFT, LRGAP);

    // initialize controls
    keycombo->ChangeValue( GetKeyCombo(currkey, currmods) );
    actionmenu->SetSelection( keyaction[currkey][currmods].id );
    UpdateChosenFile();
    keycombo->SetFocus();
    keycombo->SetSelection(ALL_TEXT);

    topSizer->Add(vbox, 1, wxGROW | wxLEFT | wxALL, 5);
    panel->SetSizer(topSizer);
    topSizer->Fit(panel);
    return panel;
}

// -----------------------------------------------------------------------------

void PrefsDialog::UpdateChosenFile()
{
    wxStaticText* filebox = (wxStaticText*) FindWindowById(PREF_FILE_BOX);
    if (filebox) {
        action_id action = keyaction[currkey][currmods].id;
        if (action == DO_OPENFILE) {
            // display current file name
            filebox->SetLabel(keyaction[currkey][currmods].file);
        } else {
            // clear file name; don't set keyaction[currkey][currmods].file empty
            // here because user might change their mind (TransferDataFromWindow
            // will eventually set the file empty)
            filebox->SetLabel(wxEmptyString);
        }
    }
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnChoice(wxCommandEvent& event)
{
    int id = event.GetId();

    if ( id == PREF_ACTION ) {
        int i = event.GetSelection();
        if (i >= 0 && i < MAX_ACTIONS) {
            action_id action = (action_id) i;
            keyaction[currkey][currmods].id = action;

            if ( action == DO_OPENFILE && keyaction[currkey][currmods].file.IsEmpty() ) {
                // call OnButton (which will call UpdateChosenFile)
                wxCommandEvent buttevt(wxEVT_COMMAND_BUTTON_CLICKED, PREF_CHOOSE);
                OnButton(buttevt);
            } else {
                UpdateChosenFile();
            }
        }
    }

    else if ( id == PREF_ALGO_MENU1 ) {
        int i = event.GetSelection();
        if (i >= 0 && i < NumAlgos() && i != algopos1) {
            // first update values for previous selection
            new_algomem[algopos1] = GetSpinVal(PREF_MAX_MEM);
            new_defbase[algopos1] = GetSpinVal(PREF_BASE_STEP);
            algopos1 = i;

            // show values for new selection
            wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_MAX_MEM);
            wxSpinCtrl* s2 = (wxSpinCtrl*) FindWindowById(PREF_BASE_STEP);
            if (s1 && s2) {
                s1->SetValue(new_algomem[algopos1]);
                s2->SetValue(new_defbase[algopos1]);
                wxWindow* focus = FindFocus();
#ifdef __WXMAC__
                // FindFocus returns pointer to text ctrl
                wxTextCtrl* t1 = s1->GetText();
                wxTextCtrl* t2 = s2->GetText();
                if (focus == t1) { s1->SetFocus(); s1->SetSelection(ALL_TEXT); }
                if (focus == t2) { s2->SetFocus(); s2->SetSelection(ALL_TEXT); }
#else
                // probably pointless -- pop-up menu has focus on Win & Linux???
                if (focus == s1) { s1->SetFocus(); s1->SetSelection(ALL_TEXT); }
                if (focus == s2) { s2->SetFocus(); s2->SetSelection(ALL_TEXT); }
#endif
            }

            // change comments depending on whether or not algo uses hashing
            wxStaticText* membox = (wxStaticText*) FindWindowById(PREF_MEM_NOTE);
            wxStaticText* stepbox = (wxStaticText*) FindWindowById(PREF_STEP_NOTE);
            if (membox && stepbox) {
                if (algoinfo[algopos1]->canhash) {
                    membox->SetLabel(HASH_MEM_NOTE);
                    stepbox->SetLabel(HASH_STEP_NOTE);
                } else {
                    membox->SetLabel(NONHASH_MEM_NOTE);
                    stepbox->SetLabel(NONHASH_STEP_NOTE);
                }
            }
        }
    }

    else if ( id == PREF_ALGO_MENU2 ) {
        int i = event.GetSelection();
        if (i >= 0 && i < NumAlgos() && i != coloralgo) {
            coloralgo = i;

            AlgoData* ad = algoinfo[coloralgo];

            // update colors in some bitmap buttons
            UpdateButtonColor(PREF_STATUS_BUTT, ad->statusrgb);
            UpdateButtonColor(PREF_FROM_BUTT, ad->fromrgb);
            UpdateButtonColor(PREF_TO_BUTT, ad->torgb);

            gradstates = ad->maxstates;
            UpdateScrollBar();

            gradcheck->SetValue(ad->gradient);
            scrollbar->Enable(ad->gradient);
            frombutt->Enable(ad->gradient);
            tobutt->Enable(ad->gradient);

            cellboxes->Refresh(false);
        }
    }
}

// -----------------------------------------------------------------------------

void ChooseTextEditor(wxWindow* parent, wxString& result)
{
    #ifdef __WXMSW__
        wxString filetypes = _("Applications (*.exe)|*.exe");
    #elif defined(__WXMAC__)
        wxString filetypes = _("Applications (*.app)|*.app");
    #else // assume Linux
        wxString filetypes = _("All files (*)|*");
    #endif

    wxFileDialog opendlg(parent, _("Choose a text editor"),
                         wxEmptyString, wxEmptyString, filetypes,
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    #ifdef __WXMSW__
        opendlg.SetDirectory(_("C:\\Program Files"));
    #elif defined(__WXMAC__)
        opendlg.SetDirectory(_("/Applications"));
    #else // assume Linux
        opendlg.SetDirectory(_("/usr/bin"));
    #endif

    if ( opendlg.ShowModal() == wxID_OK ) {
        result = opendlg.GetPath();
    } else {
        result = wxEmptyString;
    }
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnButton(wxCommandEvent& event)
{
    int id = event.GetId();

    if ( id == PREF_CHOOSE ) {
        // ask user to choose an appropriate file
        wxString filetypes = _("All files (*)|*");
        filetypes +=         _("|Pattern (*.rle;*.mc;*.lif)|*.rle;*.mc;*.lif");
        filetypes +=         _("|Script (*.lua;*.py)|*.lua;*.py");
        filetypes +=         _("|Rule (*.rule)|*.rule");
        filetypes +=         _("|HTML (*.html;*.htm)|*.html;*.htm");

        wxFileDialog opendlg(this, _("Choose a pattern/script/rule/HTML file"),
                             choosedir, wxEmptyString, filetypes,
                             wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if ( opendlg.ShowModal() == wxID_OK ) {
            wxFileName fullpath( opendlg.GetPath() );
            choosedir = fullpath.GetPath();
            wxString path = opendlg.GetPath();
            if (path.StartsWith(gollydir)) {
                // remove gollydir from start of path
                path.erase(0, gollydir.length());
            }
            keyaction[currkey][currmods].file = path;
            keyaction[currkey][currmods].id = DO_OPENFILE;
            wxChoice* actionmenu = (wxChoice*) FindWindowById(PREF_ACTION);
            if (actionmenu) {
                actionmenu->SetSelection(DO_OPENFILE);
            }
        }

        UpdateChosenFile();

    } else if ( id == PREF_EDITOR_BUTT ) {
        // ask user to choose a text editor
        wxString result;
        ChooseTextEditor(this, result);
        if ( !result.IsEmpty() ) {
            neweditor = result;
            wxStaticText* editorbox = (wxStaticText*) FindWindowById(PREF_EDITOR_BOX);
            if (editorbox) {
                editorbox->SetLabel(neweditor);
            }
        }

    } else if ( id == PREF_DOWNLOAD_BUTT ) {
        // ask user to choose folder for downloaded files
        wxDirDialog dirdlg(this, _("Choose a folder for downloaded files"),
                           newdownloaddir, wxDD_NEW_DIR_BUTTON);
        if ( dirdlg.ShowModal() == wxID_OK ) {
            wxString newdir = dirdlg.GetPath();
            if (newdir.Last() != wxFILE_SEP_PATH) newdir += wxFILE_SEP_PATH;
            if (newdownloaddir != newdir) {
                newdownloaddir = newdir;
                wxStaticText* dirbox = (wxStaticText*) FindWindowById(PREF_DOWNLOAD_BOX);
                if (dirbox) {
                    dirbox->SetLabel(newdownloaddir);
                }
            }
        }

    } else if ( id == PREF_RULES_BUTT ) {
        // ask user to choose folder for their rules
        wxDirDialog dirdlg(this, _("Choose a folder for your rules"),
                           newuserrules, wxDD_NEW_DIR_BUTTON);
        if ( dirdlg.ShowModal() == wxID_OK ) {
            wxString newdir = dirdlg.GetPath();
            if (newdir.Last() != wxFILE_SEP_PATH) newdir += wxFILE_SEP_PATH;
            if (newuserrules != newdir) {
                newuserrules = newdir;
                wxStaticText* dirbox = (wxStaticText*) FindWindowById(PREF_RULES_BOX);
                if (dirbox) {
                    dirbox->SetLabel(newuserrules);
                }
            }
        }
    }

    event.Skip();  // need this so other buttons work correctly
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnCheckBoxClicked(wxCommandEvent& event)
{
    int id = event.GetId();

    if ( id == PREF_SHOW_BOLD ) {
        // enable/disable PREF_BOLD_SPACING spin control
        wxCheckBox* checkbox = (wxCheckBox*) FindWindow(PREF_SHOW_BOLD);
        wxSpinCtrl* spinctrl = (wxSpinCtrl*) FindWindow(PREF_BOLD_SPACING);
        if (checkbox && spinctrl) {
            bool ticked = checkbox->GetValue();
            spinctrl->Enable(ticked);
            if (ticked) spinctrl->SetFocus();
        }

    } else if ( id == PREF_GRADIENT_CHECK ) {
        AlgoData* ad = algoinfo[coloralgo];
        ad->gradient = gradcheck->GetValue() == 1;
        scrollbar->Enable(ad->gradient);
        frombutt->Enable(ad->gradient);
        tobutt->Enable(ad->gradient);
        cellboxes->Refresh(false);

    } else if ( id == PREF_ICON_CHECK ) {
        showicons = iconcheck->GetValue() == 1;
        cellboxes->Refresh(false);
    }
}

// -----------------------------------------------------------------------------

void PrefsDialog::UpdateButtonColor(int id, wxColor& rgb)
{
    wxBitmapButton* bb = (wxBitmapButton*) FindWindow(id);
    if (bb) {
        wxBitmap bitmap(BITMAP_WD, BITMAP_HT);
        wxMemoryDC dc;
        dc.SelectObject(bitmap);
        wxRect rect(0, 0, BITMAP_WD, BITMAP_HT);
        wxBrush brush(rgb);
        FillRect(dc, rect, brush);
        dc.SelectObject(wxNullBitmap);
        bb->SetBitmapLabel(bitmap);
        bb->Refresh();
    }
}

// -----------------------------------------------------------------------------

void PrefsDialog::ChangeButtonColor(int id, wxColor& rgb)
{
    wxColourData data;
    data.SetChooseFull(true);    // for Windows
    data.SetColour(rgb);

    wxColourDialog dialog(this, &data);
    if ( dialog.ShowModal() == wxID_OK ) {
        wxColourData retData = dialog.GetColourData();
        wxColour c = retData.GetColour();

        if (rgb != c) {
            // change given color
            rgb.Set(c.Red(), c.Green(), c.Blue());

            // also change color of bitmap in corresponding button
            UpdateButtonColor(id, rgb);

            if (id == PREF_FROM_BUTT || id == PREF_TO_BUTT) {
                cellboxes->Refresh(false);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnColorButton(wxCommandEvent& event)
{
    int id = event.GetId();

    if ( id == PREF_STATUS_BUTT ) {
        ChangeButtonColor(id, algoinfo[coloralgo]->statusrgb);

    } else if ( id == PREF_FROM_BUTT ) {
        ChangeButtonColor(id, algoinfo[coloralgo]->fromrgb);

    } else if ( id == PREF_TO_BUTT ) {
        ChangeButtonColor(id, algoinfo[coloralgo]->torgb);

    } else if ( id == PREF_SELECT_BUTT ) {
        ChangeButtonColor(id, *selectrgb);

    } else if ( id == PREF_PASTE_BUTT ) {
        ChangeButtonColor(id, *pastergb);

    } else if ( id == PREF_BORDER_BUTT ) {
        ChangeButtonColor(id, *borderrgb);

    } else {
        // process other buttons like Cancel and OK
        event.Skip();
    }
}

// -----------------------------------------------------------------------------

void PrefsDialog::UpdateScrollBar()
{
    AlgoData* ad = algoinfo[coloralgo];
    scrollbar->SetScrollbar(gradstates - ad->minstates, 1,
                            ad->maxstates - ad->minstates + 1,  // range
                            PAGESIZE, true);
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnScroll(wxScrollEvent& event)
{
    WXTYPE type = event.GetEventType();

    if (type == wxEVT_SCROLL_LINEUP) {
        gradstates--;
        if (gradstates < algoinfo[coloralgo]->minstates)
            gradstates = algoinfo[coloralgo]->minstates;
        cellboxes->Refresh(false);

    } else if (type == wxEVT_SCROLL_LINEDOWN) {
        gradstates++;
        if (gradstates > algoinfo[coloralgo]->maxstates)
            gradstates = algoinfo[coloralgo]->maxstates;
        cellboxes->Refresh(false);

    } else if (type == wxEVT_SCROLL_PAGEUP) {
        gradstates -= PAGESIZE;
        if (gradstates < algoinfo[coloralgo]->minstates)
            gradstates = algoinfo[coloralgo]->minstates;
        cellboxes->Refresh(false);

    } else if (type == wxEVT_SCROLL_PAGEDOWN) {
        gradstates += PAGESIZE;
        if (gradstates > algoinfo[coloralgo]->maxstates)
            gradstates = algoinfo[coloralgo]->maxstates;
        cellboxes->Refresh(false);

    } else if (type == wxEVT_SCROLL_THUMBTRACK) {
        gradstates = algoinfo[coloralgo]->minstates + event.GetPosition();
        if (gradstates < algoinfo[coloralgo]->minstates)
            gradstates = algoinfo[coloralgo]->minstates;
        if (gradstates > algoinfo[coloralgo]->maxstates)
            gradstates = algoinfo[coloralgo]->maxstates;
        cellboxes->Refresh(false);

    } else if (type == wxEVT_SCROLL_THUMBRELEASE) {
        UpdateScrollBar();
    }
}

// -----------------------------------------------------------------------------

bool PrefsDialog::GetCheckVal(long id)
{
    wxCheckBox* checkbox = (wxCheckBox*) FindWindow(id);
    if (checkbox) {
        return checkbox->GetValue();
    } else {
        Warning(_("Bug in GetCheckVal!"));
        return false;
    }
}

// -----------------------------------------------------------------------------

int PrefsDialog::GetChoiceVal(long id)
{
    wxChoice* choice = (wxChoice*) FindWindow(id);
    if (choice) {
        return choice->GetSelection();
    } else {
        Warning(_("Bug in GetChoiceVal!"));
        return 0;
    }
}

// -----------------------------------------------------------------------------

int PrefsDialog::GetRadioVal(long firstid, int numbuttons)
{
    for (int i = 0; i < numbuttons; i++) {
        wxRadioButton* radio = (wxRadioButton*) FindWindow(firstid + i);
        if (radio->GetValue()) return i;
    }
    Warning(_("Bug in GetRadioVal!"));
    return 0;
}

// -----------------------------------------------------------------------------

int PrefsDialog::GetSpinVal(long id)
{
    wxSpinCtrl* spinctrl = (wxSpinCtrl*) FindWindow(id);
    if (spinctrl) {
        return spinctrl->GetValue();
    } else {
        Warning(_("Bug in GetSpinVal!"));
        return 0;
    }
}

// -----------------------------------------------------------------------------

bool PrefsDialog::BadSpinVal(int id, int minval, int maxval, const wxString& prefix)
{
    wxSpinCtrl* spinctrl = (wxSpinCtrl*) FindWindow(id);
    // spinctrl->GetValue() always returns a value within range even if
    // the text ctrl doesn't contain a valid number -- yuk!
    int i = spinctrl->GetValue();
    if (i < minval || i > maxval) {
        wxString msg;
        msg.Printf(_("%s must be from %d to %d."), prefix.c_str(), minval, maxval);
        Warning(msg);
        spinctrl->SetFocus();
        spinctrl->SetSelection(ALL_TEXT);
        return true;
    } else {
        return false;
    }
}

// -----------------------------------------------------------------------------

bool PrefsDialog::ValidatePage()
{
    // validate all spin control values on current page
    if (currpage == FILE_PAGE) {
        if ( BadSpinVal(PREF_MAX_PATTERNS, 1, MAX_RECENT, _("Maximum number of recent patterns")) )
            return false;
        if ( BadSpinVal(PREF_MAX_SCRIPTS, 1, MAX_RECENT, _("Maximum number of recent scripts")) )
            return false;

    } else if (currpage == EDIT_PAGE) {
        if ( BadSpinVal(PREF_RANDOM_FILL, 1, 100, _("Random fill percentage")) )
            return false;

    } else if (currpage == CONTROL_PAGE) {
        if ( BadSpinVal(PREF_MAX_MEM, MIN_MEM_MB, MAX_MEM_MB, _("Maximum memory")) )
            return false;
        if ( BadSpinVal(PREF_BASE_STEP, 2, MAX_BASESTEP, _("Default base step")) )
            return false;
        if ( BadSpinVal(PREF_MIN_DELAY, 0, MAX_DELAY, _("Minimum delay")) )
            return false;
        if ( BadSpinVal(PREF_MAX_DELAY, 0, MAX_DELAY, _("Maximum delay")) )
            return false;

    } else if (currpage == VIEW_PAGE) {
        if ( BadSpinVal(PREF_BOLD_SPACING, 2, MAX_SPACING, _("Spacing of bold grid lines")) )
            return false;
        if ( BadSpinVal(PREF_SENSITIVITY, 1, MAX_SENSITIVITY, _("Wheel sensitivity")) )
            return false;
        if ( BadSpinVal(PREF_THUMB_RANGE, 2, MAX_THUMBRANGE, _("Thumb scrolling range")) )
            return false;

    } else if (currpage == LAYER_PAGE) {
        if ( BadSpinVal(PREF_OPACITY, 1, 100, _("Percentage opacity")) )
            return false;
        if ( BadSpinVal(PREF_TILE_BORDER, 1, 10, _("Tile border thickness")) )
            return false;

    } else if (currpage == COLOR_PAGE) {
        // no spin ctrls on this page

    } else if (currpage == KEYBOARD_PAGE) {
        // no spin ctrls on this page

    } else {
        Warning(_("Bug in ValidatePage!"));
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnPageChanging(wxNotebookEvent& event)
{
    if (ignore_page_event) return;
    // validate current page and veto change if invalid
    if (!ValidatePage()) event.Veto();
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnPageChanged(wxNotebookEvent& event)
{
    if (ignore_page_event) return;
    currpage = event.GetSelection();

#ifdef __WXMSW__
    // ensure key combo box has focus
    if (currpage == KEYBOARD_PAGE) {
        KeyComboCtrl* keycombo = (KeyComboCtrl*) FindWindowById(PREF_KEYCOMBO);
        if (keycombo) {
            keycombo->SetFocus();
            keycombo->SetSelection(ALL_TEXT);
        }
    }
#endif
}

// -----------------------------------------------------------------------------

bool PrefsDialog::TransferDataFromWindow()
{
    if (!ValidatePage()) return false;

    // set global prefs to current control values

    // FILE_PAGE
    newremovesel  = GetCheckVal(PREF_NEW_REM_SEL);
    newcursindex  = GetChoiceVal(PREF_NEW_CURSOR);
    newmag        = GetChoiceVal(PREF_NEW_SCALE);
    openremovesel = GetCheckVal(PREF_OPEN_REM_SEL);
    opencursindex = GetChoiceVal(PREF_OPEN_CURSOR);
    maxpatterns   = GetSpinVal(PREF_MAX_PATTERNS);
    maxscripts    = GetSpinVal(PREF_MAX_SCRIPTS);
    texteditor    = neweditor;
    downloaddir   = newdownloaddir;

    // EDIT_PAGE
    randomfill    = GetSpinVal(PREF_RANDOM_FILL);
    canchangerule = GetRadioVal(PREF_PASTE_0, 3);
    scrollpencil  = GetCheckVal(PREF_SCROLL_PENCIL);
    scrollcross   = GetCheckVal(PREF_SCROLL_CROSS);
    scrollhand    = GetCheckVal(PREF_SCROLL_HAND);
    allowbeep     = GetCheckVal(PREF_BEEP);
    showselpop    = GetCheckVal(PREF_SHOW_SEL_POP);

    // CONTROL_PAGE
    new_algomem[algopos1] = GetSpinVal(PREF_MAX_MEM);
    new_defbase[algopos1] = GetSpinVal(PREF_BASE_STEP);
    for (int i = 0; i < NumAlgos(); i++) {
        algoinfo[i]->algomem = new_algomem[i];
        algoinfo[i]->defbase = new_defbase[i];
    }
    mindelay = GetSpinVal(PREF_MIN_DELAY);
    maxdelay = GetSpinVal(PREF_MAX_DELAY);
    userrules = newuserrules;

    // VIEW_PAGE
#if wxUSE_TOOLTIPS
    showtips       = GetCheckVal(PREF_SHOW_TIPS);
    wxToolTip::Enable(showtips);
#endif
    restoreview    = GetCheckVal(PREF_RESTORE);
    mathcoords     = GetCheckVal(PREF_Y_UP);
    cellborders    = GetCheckVal(PREF_CELL_BORDERS);
    showboldlines  = GetCheckVal(PREF_SHOW_BOLD);
    boldspacing    = GetSpinVal(PREF_BOLD_SPACING);
    mingridindex   = GetChoiceVal(PREF_MIN_GRID_SCALE);
    mousewheelmode = GetChoiceVal(PREF_MOUSE_WHEEL);
    wheelsens      = GetSpinVal(PREF_SENSITIVITY);
    thumbrange     = GetSpinVal(PREF_THUMB_RANGE);
    controlspos    = GetChoiceVal(PREF_CONTROLS);

    // LAYER_PAGE
    opacity         = GetSpinVal(PREF_OPACITY);
    tileborder      = GetSpinVal(PREF_TILE_BORDER);
    askonnew        = GetCheckVal(PREF_ASK_NEW);
    askonload       = GetCheckVal(PREF_ASK_LOAD);
    askondelete     = GetCheckVal(PREF_ASK_DELETE);
    askonquit       = GetCheckVal(PREF_ASK_QUIT);
    warn_on_save    = GetCheckVal(PREF_WARN_SAVE);

    // COLOR_PAGE
    // no need to validate anything

    // KEYBOARD_PAGE
    // go thru keyaction table and make sure the file field is empty
    // if the action isn't DO_OPENFILE
    for (int key = 0; key < MAX_KEYCODES; key++)
        for (int modset = 0; modset < MAX_MODS; modset++)
            if ( keyaction[key][modset].id != DO_OPENFILE &&
                !keyaction[key][modset].file.IsEmpty() )
                keyaction[key][modset].file = wxEmptyString;

    // update globals corresponding to some wxChoice menu selections
    mingridmag = mingridindex + 2;
    newcurs = IndexToCursor(newcursindex);
    opencurs = IndexToCursor(opencursindex);

    return true;
}

// -----------------------------------------------------------------------------

// class for saving and restoring AlgoData color info in ChangePrefs()
class SaveColorInfo {
public:
    SaveColorInfo(int algo) {
        AlgoData* ad = algoinfo[algo];
        statusrgb = ad->statusrgb;
        gradient = ad->gradient;
        fromrgb = ad->fromrgb;
        torgb = ad->torgb;
        for (int i = 0; i < ad->maxstates; i++) {
            algor[i] = ad->algor[i];
            algog[i] = ad->algog[i];
            algob[i] = ad->algob[i];
        }
    }

    void RestoreColorInfo(int algo) {
        AlgoData* ad = algoinfo[algo];
        ad->statusrgb = statusrgb;
        ad->gradient = gradient;
        ad->fromrgb = fromrgb;
        ad->torgb = torgb;
        for (int i = 0; i < ad->maxstates; i++) {
            ad->algor[i] = algor[i];
            ad->algog[i] = algog[i];
            ad->algob[i] = algob[i];
        }
    }

    bool ColorInfoChanged(int algo) {
        AlgoData* ad = algoinfo[algo];
        // ignore ad->statusrgb
        if (ad->gradient != gradient) return true;
        if (gradient && ad->fromrgb != fromrgb) return true;
        if (gradient && ad->torgb != torgb) return true;
        for (int i = 0; i < ad->maxstates; i++) {
            if (ad->algor[i] != algor[i]) return true;
            if (ad->algog[i] != algog[i]) return true;
            if (ad->algob[i] != algob[i]) return true;
        }
        // get here if there was no change
        return false;
    }

    // this must match color info in AlgoData
    wxColor statusrgb;
    bool gradient;
    wxColor fromrgb;
    wxColor torgb;
    unsigned char algor[256];
    unsigned char algog[256];
    unsigned char algob[256];
};

// -----------------------------------------------------------------------------

bool ChangePrefs(const wxString& page)
{
    // save current keyboard shortcuts so we can restore them or detect a change
    action_info savekeyaction[MAX_KEYCODES][MAX_MODS];
    for (int key = 0; key < MAX_KEYCODES; key++)
        for (int modset = 0; modset < MAX_MODS; modset++)
            savekeyaction[key][modset] = keyaction[key][modset];

    bool wasswapped = swapcolors;
    if (swapcolors) {
        swapcolors = false;
        InvertCellColors();
        mainptr->UpdateEverything();
    }

    // save current color info so we can restore it if user cancels changes
    wxColor save_selectrgb = *selectrgb;
    wxColor save_pastergb = *pastergb;
    wxColor save_borderrgb = *borderrgb;
    SaveColorInfo* save_info[MAX_ALGOS];
    for (int i = 0; i < NumAlgos(); i++) {
        save_info[i] = new SaveColorInfo(i);
    }

    // save showicons option in case user cancels dialog
    bool saveshowicons = showicons;

    // save the default base step for the current layer's algo so we can detect a change
    int old_defbase = algoinfo[currlayer->algtype]->defbase;

    bool result;
    PrefsDialog dialog(mainptr, page);
    int button = dialog.ShowModal();
    if (viewptr) viewptr->ResetMouseDown();
    if (button == wxID_OK) {
        // TransferDataFromWindow has validated and updated all global prefs;
        // if a keyboard shortcut changed then update menu item accelerators
        for (int key = 0; key < MAX_KEYCODES; key++)
            for (int modset = 0; modset < MAX_MODS; modset++)
                if (savekeyaction[key][modset].id != keyaction[key][modset].id) {
                    // first update accelerator array
                    UpdateAcceleratorStrings();
                    mainptr->UpdateMenuAccelerators();
                    goto done;
                }
        done:

        // if the default base step for the current layer's algo changed
        // then reset the current base step (this should result in less confusion)
        if (old_defbase != algoinfo[currlayer->algtype]->defbase) {
            currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
            mainptr->SetGenIncrement();
        }

        // if the default colors/icons for the current layer's algo changed
        // then reset the current layer's colors (and any clones)
        if (save_info[currlayer->algtype]->ColorInfoChanged(currlayer->algtype)) {
            UpdateLayerColors();
        }

        result = true;
    } else {
        // user hit Cancel, so restore keyaction array in case it was changed
        for (int key = 0; key < MAX_KEYCODES; key++)
            for (int modset = 0; modset < MAX_MODS; modset++)
                keyaction[key][modset] = savekeyaction[key][modset];

        // restore color info saved above
        *selectrgb = save_selectrgb;
        *pastergb = save_pastergb;
        *borderrgb = save_borderrgb;
        for (int i = 0; i < NumAlgos(); i++) {
            save_info[i]->RestoreColorInfo(i);
        }

        // restore showicons option
        showicons = saveshowicons;

        result = false;
    }

    UpdateStatusBrushes();

    for (int i = 0; i < NumAlgos(); i++) {
        delete save_info[i];
    }

    if (wasswapped) {
        swapcolors = true;
        InvertCellColors();
        // let caller do this
        // mainptr->UpdateEverything();
    }

    return result;
}

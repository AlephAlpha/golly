// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "lifealgo.h"
#include "viewport.h"       // for MAX_MAG
#include "util.h"           // for linereader, lifeerrors

#include "utils.h"          // for gColor, SetColor, Warning, Fatal, Beep
#include "status.h"         // for DisplayMessage
#include "algos.h"          // for NumAlgos, algoinfo, etc
#include "layer.h"          // for currlayer
#include "prefs.h"

#ifdef ANDROID_GUI
    #include "jnicalls.h"   // for BeginProgress, etc
#endif

#ifdef WEB_GUI
    #include "webcalls.h"   // for BeginProgress, etc
#endif

#ifdef IOS_GUI
    #import "PatternViewController.h"   // for BeginProgress, etc
#endif

// -----------------------------------------------------------------------------

// Golly's preferences file is a simple text file.

const int PREFS_VERSION = 1;        // increment if necessary due to changes in syntax/semantics
int currversion = PREFS_VERSION;    // might be changed by prefs_version
const int PREF_LINE_SIZE = 5000;    // must be quite long for storing file paths

std::string supplieddir;            // path of parent directory for supplied help/patterns/rules
std::string helpdir;                // path of directory for supplied help
std::string patternsdir;            // path of directory for supplied patterns
std::string rulesdir;               // path of directory for supplied rules
std::string userdir;                // path of parent directory for user's rules/patterns/downloads
std::string userrules;              // path of directory for user's rules
std::string savedir;                // path of directory for user's saved patterns
std::string downloaddir;            // path of directory for user's downloaded files
std::string tempdir;                // path of directory for temporary data
std::string clipfile;               // path of temporary file for storing clipboard data
std::string prefsfile;              // path of file for storing user's preferences

// initialize exported preferences:

int debuglevel = 0;                 // for displaying debug info if > 0
int helpfontsize = 10;              // font size in help window
char initrule[256] = "B3/S23";      // initial rule
bool initautofit = false;           // initial autofit setting
bool inithyperspeed = false;        // initial hyperspeed setting
bool initshowhashinfo = false;      // initial showhashinfo setting
bool savexrle = true;               // save RLE file using XRLE format?
bool showtool = true;               // show tool bar?
bool showlayer = false;             // show layer bar?
bool showedit = true;               // show edit bar?
bool showallstates = false;         // show all cell states in edit bar?
bool showstatus = true;             // show status bar?
bool showexact = false;             // show exact numbers in status bar?
bool showtimeline = false;          // show timeline bar?
bool showtiming = false;            // show timing messages?
bool showgridlines = true;          // display grid lines?
bool showicons = false;             // display icons for cell states?
bool swapcolors = false;            // swap colors used for cell states?
bool allowundo = true;              // allow undo/redo?
bool allowbeep = true;              // okay to play beep sound?
bool restoreview = true;            // should reset/undo restore view?
int canchangerule = 1;              // if > 0 then paste can change rule if pattern is empty
int randomfill = 50;                // random fill percentage (1..100)
int opacity = 80;                   // percentage opacity of live cells in overlays (1..100)
int tileborder = 3;                 // thickness of tiled window borders
int mingridmag = 2;                 // minimum mag to draw grid lines
int boldspacing = 10;               // spacing of bold grid lines
bool showboldlines = true;          // show bold grid lines?
bool mathcoords = false;            // show Y values increasing upwards?
bool syncviews = false;             // synchronize viewports?
bool syncmodes = true;              // synchronize touch modes?
bool stacklayers = false;           // stack all layers?
bool tilelayers = false;            // tile all layers?
bool asktosave = true;              // ask to save changes?
int newmag = 5;                     // mag setting for new pattern
bool newremovesel = true;           // new pattern removes selection?
bool openremovesel = true;          // opening pattern removes selection?
int mindelay = 250;                 // minimum millisec delay
int maxdelay = 2000;                // maximum millisec delay
int maxhashmem = 300;               // maximum memory (in MB) for hashlife-based algos
int wheelsens = 2;                  // mouse wheel sensitivity

int numpatterns = 0;                // current number of recent pattern files
int maxpatterns = 20;               // maximum number of recent pattern files
std::list<std::string> recentpatterns; // list of recent pattern files

gColor borderrgb;                   // color for border around bounded grid
gColor selectrgb;                   // color for selected cells
gColor pastergb;                    // color for pasted pattern

paste_mode pmode = Or;              // logical paste mode

// -----------------------------------------------------------------------------

const char* GetPasteMode()
{
    switch (pmode) {
        case And:   return "AND";
        case Copy:  return "COPY";
        case Or:    return "OR";
        case Xor:   return "XOR";
        default:    return "unknown";
    }
}

// -----------------------------------------------------------------------------

void SetPasteMode(const char* s)
{
    if (strcmp(s, "AND") == 0) {
        pmode = And;
    } else if (strcmp(s, "COPY") == 0) {
        pmode = Copy;
    } else if (strcmp(s, "OR") == 0) {
        pmode = Or;
    } else {
        pmode = Xor;
    }
}

// -----------------------------------------------------------------------------

void CreateDefaultColors()
{
    SetColor(borderrgb, 128, 128, 128);     // 50% gray
    SetColor(selectrgb,  75, 175,   0);     // dark green
    SetColor(pastergb,  255,   0,   0);     // red
}

// -----------------------------------------------------------------------------

void GetColor(const char* value, gColor& rgb)
{
    unsigned int r, g, b;
    sscanf(value, "%u,%u,%u", &r, &g, &b);
    SetColor(rgb, r, g, b);
}

// -----------------------------------------------------------------------------

void SaveColor(FILE* f, const char* name, const gColor rgb)
{
    fprintf(f, "%s=%u,%u,%u\n", name, rgb.r, rgb.g, rgb.b);
}

// -----------------------------------------------------------------------------

void SavePrefs()
{
    if (currlayer == NULL) {
        // should never happen but play safe
        Warning("Bug: currlayer is NULL!");
        return;
    }

    FILE* f = fopen(prefsfile.c_str(), "w");
    if (f == NULL) {
        Warning("Could not save preferences file!");
        return;
    }

    fprintf(f, "prefs_version=%d\n", PREFS_VERSION);
    fprintf(f, "debug_level=%d\n", debuglevel);
    fprintf(f, "help_font_size=%d (%d..%d)\n", helpfontsize, minfontsize, maxfontsize);
    fprintf(f, "allow_undo=%d\n", allowundo ? 1 : 0);
    fprintf(f, "allow_beep=%d\n", allowbeep ? 1 : 0);
    fprintf(f, "restore_view=%d\n", restoreview ? 1 : 0);
    fprintf(f, "paste_mode=%s\n", GetPasteMode());
    fprintf(f, "can_change_rule=%d (0..2)\n", canchangerule);
    fprintf(f, "random_fill=%d (1..100)\n", randomfill);
    fprintf(f, "min_delay=%d (0..%d millisecs)\n", mindelay, MAX_DELAY);
    fprintf(f, "max_delay=%d (0..%d millisecs)\n", maxdelay, MAX_DELAY);
    fprintf(f, "auto_fit=%d\n", currlayer->autofit ? 1 : 0);
    fprintf(f, "hyperspeed=%d\n", currlayer->hyperspeed ? 1 : 0);
    fprintf(f, "hash_info=%d\n", currlayer->showhashinfo ? 1 : 0);
    fprintf(f, "max_hash_mem=%d\n", maxhashmem);
    fprintf(f, "wheel_sens=%d\n", wheelsens);

    fputs("\n", f);

    fprintf(f, "init_algo=%s\n", GetAlgoName(currlayer->algtype));
    for (int i = 0; i < NumAlgos(); i++) {
        fputs("\n", f);
        fprintf(f, "algorithm=%s\n", GetAlgoName(i));
        fprintf(f, "base_step=%d\n", algoinfo[i]->defbase);
        SaveColor(f, "status_rgb", algoinfo[i]->statusrgb);
        SaveColor(f, "from_rgb", algoinfo[i]->fromrgb);
        SaveColor(f, "to_rgb", algoinfo[i]->torgb);
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
    fprintf(f, "show_tool=%d\n", showtool ? 1 : 0);
    fprintf(f, "show_layer=%d\n", showlayer ? 1 : 0);
    fprintf(f, "show_edit=%d\n", showedit ? 1 : 0);
    fprintf(f, "show_states=%d\n", showallstates ? 1 : 0);
    fprintf(f, "show_status=%d\n", showstatus ? 1 : 0);
    fprintf(f, "show_exact=%d\n", showexact ? 1 : 0);
    fprintf(f, "show_timeline=%d\n", showtimeline ? 1 : 0);
    fprintf(f, "show_timing=%d\n", showtiming ? 1 : 0);
    fprintf(f, "grid_lines=%d\n", showgridlines ? 1 : 0);
    fprintf(f, "min_grid_mag=%d (2..%d)\n", mingridmag, MAX_MAG);
    fprintf(f, "bold_spacing=%d (2..%d)\n", boldspacing, MAX_SPACING);
    fprintf(f, "show_bold_lines=%d\n", showboldlines ? 1 : 0);
    fprintf(f, "math_coords=%d\n", mathcoords ? 1 : 0);

    fputs("\n", f);

    fprintf(f, "sync_views=%d\n", syncviews ? 1 : 0);
    fprintf(f, "sync_modes=%d\n", syncmodes ? 1 : 0);
    fprintf(f, "stack_layers=%d\n", stacklayers ? 1 : 0);
    fprintf(f, "tile_layers=%d\n", tilelayers ? 1 : 0);
    fprintf(f, "tile_border=%d (1..10)\n", tileborder);
    fprintf(f, "ask_to_save=%d\n", asktosave ? 1 : 0);

    fputs("\n", f);

    fprintf(f, "show_icons=%d\n", showicons ? 1 : 0);
    fprintf(f, "swap_colors=%d\n", swapcolors ? 1 : 0);
    fprintf(f, "opacity=%d (1..100)\n", opacity);
    SaveColor(f, "border_rgb", borderrgb);
    SaveColor(f, "select_rgb", selectrgb);
    SaveColor(f, "paste_rgb", pastergb);

    fputs("\n", f);

    fprintf(f, "new_mag=%d (0..%d)\n", newmag, MAX_MAG);
    fprintf(f, "new_remove_sel=%d\n", newremovesel ? 1 : 0);
    fprintf(f, "open_remove_sel=%d\n", openremovesel ? 1 : 0);
    fprintf(f, "save_xrle=%d\n", savexrle ? 1 : 0);
    fprintf(f, "max_patterns=%d (1..%d)\n", maxpatterns, MAX_RECENT);

    if (!recentpatterns.empty()) {
        fputs("\n", f);
        std::list<std::string>::iterator next = recentpatterns.begin();
        while (next != recentpatterns.end()) {
            std::string path = *next;
            fprintf(f, "recent_pattern=%s\n", path.c_str());
            next++;
        }
    }

    fclose(f);
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

// let gollybase code call Fatal, Warning, BeginProgress, etc

class my_errors : public lifeerrors
{
public:
    virtual void fatal(const char* s) {
        Fatal(s);
    }

    virtual void warning(const char* s) {
        Warning(s);
    }

    virtual void status(const char* s) {
        DisplayMessage(s);
    }

    virtual void beginprogress(const char* s) {
        BeginProgress(s);
        // init flag for isaborted() calls
        aborted = false;
    }

    virtual bool abortprogress(double f, const char* s) {
        return AbortProgress(f, s);
    }

    virtual void endprogress() {
        EndProgress();
    }

    virtual const char* getuserrules() {
        return (const char*) userrules.c_str();
    }

    virtual const char* getrulesdir() {
        return (const char*) rulesdir.c_str();
    }

    virtual const char* gettempdir() {
        return (const char*) tempdir.c_str();
    }
};

static my_errors myerrhandler;    // create instance

// -----------------------------------------------------------------------------

void GetPrefs()
{
    int algoindex = -1;     // unknown algorithm

    // let gollybase code call Fatal, Warning, BeginProgress, etc
    lifeerrors::seterrorhandler(&myerrhandler);

    CreateDefaultColors();

    FILE* f = fopen(prefsfile.c_str(), "r");
    if (f == NULL) {
        // should only happen 1st time app is run
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

        } else if (strcmp(keyword, "help_font_size") == 0) {
            sscanf(value, "%d", &helpfontsize);
            if (helpfontsize < minfontsize) helpfontsize = minfontsize;
            if (helpfontsize > maxfontsize) helpfontsize = maxfontsize;

        } else if (strcmp(keyword, "allow_undo") == 0) {
            allowundo = value[0] == '1';

        } else if (strcmp(keyword, "allow_beep") == 0) {
            allowbeep = value[0] == '1';

        } else if (strcmp(keyword, "restore_view") == 0) {
            restoreview = value[0] == '1';

        } else if (strcmp(keyword, "paste_mode") == 0) {
            SetPasteMode(value);

        } else if (strcmp(keyword, "can_change_rule") == 0) {
            sscanf(value, "%d", &canchangerule);
            if (canchangerule < 0) canchangerule = 0;
            if (canchangerule > 2) canchangerule = 2;

        } else if (strcmp(keyword, "random_fill") == 0) {
            sscanf(value, "%d", &randomfill);
            if (randomfill < 1) randomfill = 1;
            if (randomfill > 100) randomfill = 100;

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
                GetColor(value, algoinfo[algoindex]->statusrgb);

        } else if (strcmp(keyword, "from_rgb") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos())
                GetColor(value, algoinfo[algoindex]->fromrgb);

        } else if (strcmp(keyword, "to_rgb") == 0) {
            if (algoindex >= 0 && algoindex < NumAlgos())
                GetColor(value, algoinfo[algoindex]->torgb);

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
                    while (*value != ',') value++; value++;
                    while (*value != ',') value++; value++;
                    while (*value != ',') value++; value++;
                    while (*value != ',') value++; value++;
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

        } else if (strcmp(keyword, "init_algo") == 0) {
            value = ReplaceDeprecatedAlgo(value);
            int i = staticAlgoInfo::nameToIndex(value);
            if (i >= 0 && i < NumAlgos())
                initalgo = i;

        } else if (strcmp(keyword, "hyperspeed") == 0) {
            inithyperspeed = value[0] == '1';

        } else if (strcmp(keyword, "hash_info") == 0) {
            initshowhashinfo = value[0] == '1';

        } else if (strcmp(keyword, "max_hash_mem") == 0) {
            sscanf(value, "%d", &maxhashmem);
            if (maxhashmem < MIN_MEM_MB) maxhashmem = MIN_MEM_MB;
            if (maxhashmem > MAX_MEM_MB) maxhashmem = MAX_MEM_MB;

        } else if (strcmp(keyword, "wheel_sens") == 0) {
            sscanf(value, "%d", &wheelsens);
            if (wheelsens < 1) wheelsens = 1;
            if (wheelsens > MAX_SENSITIVITY) wheelsens = MAX_SENSITIVITY;

        } else if (strcmp(keyword, "rule") == 0) {
            strncpy(initrule, value, sizeof(initrule));

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

        } else if (strcmp(keyword, "show_timeline") == 0) {
            showtimeline = value[0] == '1';

        } else if (strcmp(keyword, "show_timing") == 0) {
            showtiming = value[0] == '1';

        } else if (strcmp(keyword, "grid_lines") == 0) {
            showgridlines = value[0] == '1';

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

        } else if (strcmp(keyword, "sync_views") == 0) {
            syncviews = value[0] == '1';

        } else if (strcmp(keyword, "sync_modes") == 0) {
            syncmodes = value[0] == '1';

        } else if (strcmp(keyword, "stack_layers") == 0) {
            stacklayers = value[0] == '1';

        } else if (strcmp(keyword, "tile_layers") == 0) {
            tilelayers = value[0] == '1';

        } else if (strcmp(keyword, "tile_border") == 0) {
            sscanf(value, "%d", &tileborder);
            if (tileborder < 1) tileborder = 1;
            if (tileborder > 10) tileborder = 10;

        } else if (strcmp(keyword, "ask_to_save") == 0) {
            asktosave = value[0] == '1';

        } else if (strcmp(keyword, "show_icons") == 0) {
            showicons = value[0] == '1';

        } else if (strcmp(keyword, "swap_colors") == 0) {
            swapcolors = value[0] == '1';

        } else if (strcmp(keyword, "opacity") == 0) {
            sscanf(value, "%d", &opacity);
            if (opacity < 1) opacity = 1;
            if (opacity > 100) opacity = 100;

        } else if (strcmp(keyword, "border_rgb") == 0) { GetColor(value, borderrgb);
        } else if (strcmp(keyword, "select_rgb") == 0) { GetColor(value, selectrgb);
        } else if (strcmp(keyword, "paste_rgb") == 0)  { GetColor(value, pastergb);

        } else if (strcmp(keyword, "new_mag") == 0) {
            sscanf(value, "%d", &newmag);
            if (newmag < 0) newmag = 0;
            if (newmag > MAX_MAG) newmag = MAX_MAG;

        } else if (strcmp(keyword, "new_remove_sel") == 0) {
            newremovesel = value[0] == '1';

        } else if (strcmp(keyword, "open_remove_sel") == 0) {
            openremovesel = value[0] == '1';

        } else if (strcmp(keyword, "save_xrle") == 0) {
            savexrle = value[0] == '1';

        } else if (strcmp(keyword, "max_patterns") == 0) {
            sscanf(value, "%d", &maxpatterns);
            if (maxpatterns < 1) maxpatterns = 1;
            if (maxpatterns > MAX_RECENT) maxpatterns = MAX_RECENT;

        } else if (strcmp(keyword, "recent_pattern") == 0) {
            if (numpatterns < maxpatterns && value[0]) {
                // append path to recentpatterns if file exists
                std::string path = value;
                if (path.find("Patterns/") == 0 || FileExists(userdir + path)) {
                    recentpatterns.push_back(path);
                    numpatterns++;
                }
            }
        }
    }
    reader.close();

    // stacklayers and tilelayers must not both be true
    if (stacklayers && tilelayers) tilelayers = false;
}

// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "bigint.h"
#include "lifealgo.h"

#include "utils.h"      // for Fatal, Beep
#include "prefs.h"      // for mindelay, maxdelay, etc
#include "algos.h"      // for algoinfo
#include "layer.h"      // for currlayer
#include "view.h"       // for nopattupdate, widescreen, PointInView, etc
#include "status.h"
#include <math.h>       // for fabs

#ifdef ANDROID_GUI
    #include "jnicalls.h"   // for UpdateStatus, GetRuleName
#endif

#ifdef WEB_GUI
    #include "webcalls.h"   // for UpdateStatus, GetRuleName
#endif

#ifdef IOS_GUI
    #import "PatternViewController.h"   // for UpdateStatus
    #import "RuleViewController.h"      // for GetRuleName
#endif

// -----------------------------------------------------------------------------

std::string status1;    // top line
std::string status2;    // middle line
std::string status3;    // bottom line

// prefixes used when widescreen is true:
const char* large_gen_prefix =   "Generation=";
const char* large_algo_prefix =  "    Algorithm=";
const char* large_rule_prefix =  "    Rule=";
const char* large_pop_prefix =   "    Population=";
const char* large_scale_prefix = "    Scale=";
const char* large_step_prefix =  "    ";
const char* large_xy_prefix =    "    XY";

// prefixes used when widescreen is false:
const char* small_gen_prefix =   "Gen=";
const char* small_algo_prefix =  "   Algo=";
const char* small_rule_prefix =  "   Rule=";
const char* small_pop_prefix =   "   Pop=";
const char* small_scale_prefix = "   Scale=";
const char* small_step_prefix =  "   ";
const char* small_xy_prefix =    "   XY";

static bigint currx, curry;     // cursor's XY location in cell coords
static int currstate = -1;      // cell state at XY location
static bool showxy = false;     // show cursor's XY location?

// -----------------------------------------------------------------------------

void UpdateStatusLines()
{
    std::string rule = currlayer->algo->getrule();

    status1 = "Pattern=";
    if (currlayer->dirty) {
        // display asterisk to indicate pattern has been modified
        status1 += "*";
    }
    status1 += currlayer->currname;
    status1 += widescreen ? large_algo_prefix : small_algo_prefix;
    status1 += GetAlgoName(currlayer->algtype);
    status1 += widescreen ? large_rule_prefix : small_rule_prefix;
    status1 += rule;

    // show rule name if one exists and is not same as rule
    // (best NOT to remove any suffix like ":T100,200" in case we allow
    // users to name "B3/S23:T100,200" as "Life on torus")
    std::string rulename = GetRuleName(rule);
    if (!rulename.empty() && rulename != rule) {
        status1 += " [";
        status1 += rulename;
        status1 += "]";
    }

    char scalestr[32];
    int mag = currlayer->view->getmag();
    if (mag < 0) {
        sprintf(scalestr, "2^%d:1", -mag);
    } else {
        sprintf(scalestr, "1:%d", 1 << mag);
    }

    char stepstr[32];
    if (currlayer->currexpo < 0) {
        // show delay in secs
        sprintf(stepstr, "Delay=%gs", (double)GetCurrentDelay() / 1000.0);
    } else {
        sprintf(stepstr, "Step=%d^%d", currlayer->currbase, currlayer->currexpo);
    }

    status2 = widescreen ? large_gen_prefix : small_gen_prefix;
    if (nopattupdate) {
        status2 += "0";
    } else {
        status2 += Stringify(currlayer->algo->getGeneration());
    }
    status2 += widescreen ? large_pop_prefix : small_pop_prefix;
    if (nopattupdate) {
        status2 += "0";
    } else {
        bigint popcount = currlayer->algo->getPopulation();
        if (popcount.sign() < 0) {
            // getPopulation returns -1 if it can't be calculated
            status2 += "?";
        } else {
            status2 += Stringify(popcount);
        }
    }
    status2 += widescreen ? large_scale_prefix : small_scale_prefix;
    status2 += scalestr;
    status2 += widescreen ? large_step_prefix : small_step_prefix;
    status2 += stepstr;
    status2 += widescreen ? large_xy_prefix : small_xy_prefix;
    status2 += showxystate ? "S=" : "=";
    #ifdef WEB_GUI
        // in web app we show the cursor's current cell location,
        // or nothing if the cursor is outside the viewport (ie. showxy is false)
        if (showxy) {
            bigint xo, yo;
            bigint xpos = currx;   xpos -= currlayer->originx;
            bigint ypos = curry;   ypos -= currlayer->originy;
            if (mathcoords) {
                // Y values increase upwards
                bigint temp = 0;
                temp -= ypos;
                ypos = temp;
            }
            status2 += Stringify(xpos);
            status2 += " ";
            status2 += Stringify(ypos);
            if (showxystate) {
                if (currstate < 0) {
                    // currx,curry is outside editable boundary
                    status2 += " ?";
                } else {
                    char statebuf[8];
                    sprintf(statebuf, " %d", currstate);
                    status2 += statebuf;
                    if (currlayer->statenames.size() > 0 && currlayer->statenames[currstate].length() > 0) {
                        status2 += " ";
                        status2 += currlayer->statenames[currstate];
                    }
                }
            }
        }
    #else
        // in iOS and Android apps we show location of the cell in middle of viewport
        status2 += Stringify(currlayer->view->x);
        status2 += " ";
        status2 += Stringify(currlayer->view->y);
    #endif
}

// -----------------------------------------------------------------------------

void ClearMessage()
{
    if (status3.length() == 0) return;    // no need to clear message

    status3.clear();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

void DisplayMessage(const char* s)
{
    status3 = s;
    UpdateStatus();
}

// -----------------------------------------------------------------------------

void ErrorMessage(const char* s)
{
    Beep();
    DisplayMessage(s);
}

// -----------------------------------------------------------------------------

void SetMessage(const char* s)
{
    // set message string without displaying it
    status3 = s;
}

// -----------------------------------------------------------------------------

int GetCurrentDelay()
{
    int gendelay = mindelay * (1 << (-currlayer->currexpo - 1));
    if (gendelay > maxdelay) gendelay = maxdelay;
    return gendelay;
}

// -----------------------------------------------------------------------------

char* Stringify(const bigint& b)
{
    static char buf[32];
    char* p = buf;
    double d = b.todouble();
    if ( fabs(d) > 1000000000.0 ) {
        // use e notation for abs value > 10^9 (agrees with min & max_coord)
        sprintf(p, "%g", d);
    } else {
        // show exact value with commas inserted for readability
        if ( d < 0 ) {
            d = - d;
            *p++ = '-';
        }
        sprintf(p, "%.f", d);
        int len = (int)strlen(p);
        int commas = ((len + 2) / 3) - 1;
        int dest = len + commas;
        int src = len;
        p[dest] = 0;
        while (commas > 0) {
            p[--dest] = p[--src];
            p[--dest] = p[--src];
            p[--dest] = p[--src];
            p[--dest] = ',';
            commas--;
        }
        if ( p[-1] == '-' ) p--;
    }
    return p;
}

// -----------------------------------------------------------------------------

void CheckMouseLocation(int x, int y)
{
    // check if we need to update XY location in status bar
    bool mouse_in_grid = false;
    bigint xpos, ypos;
    if (PointInView(x, y)) {
        // get mouse location in cell coords
        pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
        xpos = cellpos.first;
        ypos = cellpos.second;
        // check if xpos,ypos is inside grid (possibly bounded)
        mouse_in_grid = CellInGrid(xpos, ypos);
    }

    if (mouse_in_grid) {
        int newstate;
        if (showxystate) {
            if (currlayer->algo->isEmpty()) {
                newstate = 0;
            } else if ( xpos < bigint::min_coord || ypos < bigint::min_coord ||
                        xpos > bigint::max_coord || ypos > bigint::max_coord ) {
                // outside editable boundary so we'll display "?" for the state
                newstate = -1;
            } else {
                newstate = currlayer->algo->getcell(xpos.toint(), ypos.toint());
            }
        } else {
            newstate = currstate;
        }
        if ( xpos != currx || ypos != curry || newstate != currstate ) {
            // show new XY location
            currx = xpos;
            curry = ypos;
            currstate = newstate;
            showxy = true;
            UpdateStatus();
        } else if (!showxy) {
            // mouse moved from outside grid and back over currx,curry
            showxy = true;
            UpdateStatus();
        }
    } else {
        // mouse is outside grid so clear XY location if necessary
        if (showxy) {
            showxy = false;
            UpdateStatus();
        }
    }
}

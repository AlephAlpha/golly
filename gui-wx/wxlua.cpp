// This file is part of Golly.
// See docs/License.html for the copyright notice.

/*
    Golly uses a statically embedded Lua interpreter to execute .lua scripts.
    Here is the official Lua copyright notice:

    Copyright 1994-2022 Lua.org, PUC-Rio.
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/filename.h"    // for wxFileName
#include "wx/dir.h"         // for wxDir

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"
#include "writepattern.h"

#include "wxgolly.h"        // for wxGetApp, mainptr, viewptr, statusptr, stopwatch
#include "wxmain.h"         // for mainptr->...
#include "wxselect.h"       // for Selection
#include "wxview.h"         // for viewptr->...
#include "wxstatus.h"       // for statusptr->...
#include "wxutils.h"        // for Warning, Note, GetString, SaveChanges, etc
#include "wxprefs.h"        // for gollydir, etc
#include "wxhelp.h"         // for ShowHelp
#include "wxundo.h"         // for currlayer->undoredo->...
#include "wxalgos.h"        // for *_ALGO, CreateNewUniverse, etc
#include "wxlayer.h"        // for AddLayer, currlayer, currindex, etc
#include "wxoverlay.h"      // for curroverlay->...
#include "wxscript.h"       // for inscript, abortmsg, GSF_*, etc
#include "wxlua.h"

#include "lua.hpp"          // Lua header files for C++

// -----------------------------------------------------------------------------

// some useful macros

#define CHECK_RGB(r,g,b,cmd)                                                \
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {         \
        char msg[128];                                                      \
        sprintf(msg, "%s error: bad rgb value (%d,%d,%d)", cmd, r, g, b);   \
        GollyError(L, msg);                                                 \
    }

#ifdef __WXMAC__
    // use decomposed UTF8 so fopen will work
    #define FILENAME wxString(filename,wxConvUTF8).fn_str()
#else
    #define FILENAME filename
#endif

// use UTF8 for the encoding conversion from Lua string to wxString
#define LUA_ENC wxConvUTF8

#ifdef AUTORELEASE
    /* we need to avoid memory leaks on recent Mac OS versions (probably 10.9+)
       especially if a script command calls Refresh; eg:
    local g = golly()
    for n = 1, math.maxinteger do
        g.show(tostring(n))
    end
    */
    #ifdef __WXMAC__
        #include "wx/osx/private.h" // for wxMacAutoreleasePool
    #endif
    #define AUTORELEASE_POOL wxMacAutoreleasePool pool;
#else
    #define AUTORELEASE_POOL
#endif

// -----------------------------------------------------------------------------

static bool aborted = false;    // stop the current script?

static void CheckEvents(lua_State* L)
{
    // this routine is called at the start of every g_* function so we can
    // detect user events (eg. hitting the stop button or escape key)

    if (allowcheck) wxGetApp().Poller()->checkevents();
    
    if (insideYield > 0) return;
    
    // we're outside Yield so safe to do a longjmp from lua_error
    if (aborted) {
        // AbortLuaScript was called
        lua_pushstring(L, abortmsg);
        lua_error(L);
    }
}

// -----------------------------------------------------------------------------

static void GollyError(lua_State* L, const char* errmsg)
{
    // handle an error detected in a g_* function;
    // note that luaL_error will prepend file path and line number info
    luaL_error(L, "\n%s", errmsg);
}

// -----------------------------------------------------------------------------

static bool CheckBoolean(lua_State* L, int arg)
{
    luaL_checktype(L, arg, LUA_TBOOLEAN);
    return lua_toboolean(L, arg) ? true : false;
}

// -----------------------------------------------------------------------------

static int g_open(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* filename = luaL_checkstring(L, 1);
    bool remember = false;
    if (lua_gettop(L) > 1) remember = CheckBoolean(L, 2);
    const char* err;

    err = GSF_open(wxString(filename, LUA_ENC), remember ? 1 : 0);
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_save(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* filename = luaL_checkstring(L, 1);
    const char* format = luaL_checkstring(L, 2);
    bool remember = false;
    if (lua_gettop(L) > 2) remember = CheckBoolean(L, 3);
    
    const char* err = GSF_save(wxString(filename, LUA_ENC), format, remember ? 1 : 0);
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_opendialog(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* title = "Choose a file";
    const char* filetypes = "All files (*)|*";
    const char* initialdir = "";
    const char* initialfname = "";
    bool mustexist = true;
    
    if (lua_gettop(L) > 0) title = luaL_checkstring(L, 1);
    if (lua_gettop(L) > 1) filetypes = luaL_checkstring(L, 2);
    if (lua_gettop(L) > 2) initialdir = luaL_checkstring(L, 3);
    if (lua_gettop(L) > 3) initialfname = luaL_checkstring(L, 4);
    if (lua_gettop(L) > 4) mustexist = CheckBoolean(L, 5);

    wxString wxs_title(title, LUA_ENC);
    wxString wxs_filetypes(filetypes, LUA_ENC);
    wxString wxs_initialdir(initialdir, LUA_ENC);
    wxString wxs_initialfname(initialfname, LUA_ENC);
    wxString wxs_result = wxEmptyString;
    
    if (wxs_initialdir.IsEmpty()) wxs_initialdir = wxFileName::GetCwd();
    
    if (wxs_filetypes == wxT("dir")) {
        // let user choose a directory
        wxDirDialog dirdlg(NULL, wxs_title, wxs_initialdir, wxDD_NEW_DIR_BUTTON);
        if (dirdlg.ShowModal() == wxID_OK) {
            wxs_result = dirdlg.GetPath();
            if (wxs_result.Last() != wxFILE_SEP_PATH) wxs_result += wxFILE_SEP_PATH;
        }
    } else {
        // let user choose a file
        wxFileDialog opendlg(NULL, wxs_title, wxs_initialdir, wxs_initialfname, wxs_filetypes,
                             wxFD_OPEN | (mustexist ? wxFD_FILE_MUST_EXIST : 0) );
        if (opendlg.ShowModal() == wxID_OK) wxs_result = opendlg.GetPath();
    }
    if (viewptr) viewptr->ResetMouseDown();
    
    lua_pushstring(L, (const char*)wxs_result.mb_str(LUA_ENC));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_savedialog(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* title = "Choose a save location and filename";
    const char* filetypes = "All files (*)|*";
    const char* initialdir = "";
    const char* initialfname = "";
    bool suppressprompt = false;
    
    if (lua_gettop(L) > 0) title = luaL_checkstring(L, 1);
    if (lua_gettop(L) > 1) filetypes = luaL_checkstring(L, 2);
    if (lua_gettop(L) > 2) initialdir = luaL_checkstring(L, 3);
    if (lua_gettop(L) > 3) initialfname = luaL_checkstring(L, 4);
    if (lua_gettop(L) > 4) suppressprompt = CheckBoolean(L, 5);

    wxString wxs_title(title, LUA_ENC);
    wxString wxs_filetypes(filetypes, LUA_ENC);
    wxString wxs_initialdir(initialdir, LUA_ENC);
    wxString wxs_initialfname(initialfname, LUA_ENC);
    
    if (wxs_initialdir.IsEmpty()) wxs_initialdir = wxFileName::GetCwd();
    
    wxFileDialog savedlg(NULL, wxs_title, wxs_initialdir, wxs_initialfname, wxs_filetypes,
                         wxFD_SAVE | (suppressprompt ? 0 : wxFD_OVERWRITE_PROMPT));
    wxString wxs_savefname = wxEmptyString;
    if (savedlg.ShowModal() == wxID_OK) wxs_savefname = savedlg.GetPath();
    if (viewptr) viewptr->ResetMouseDown();
    
    lua_pushstring(L, (const char*)wxs_savefname.mb_str(LUA_ENC));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static const char* ExtractCellArray(lua_State* L, lifealgo* universe, bool shift = false)
{
    // extract cell array from given universe
    lua_newtable(L);
    if ( !universe->isEmpty() ) {
        bigint top, left, bottom, right;
        universe->findedges(&top, &left, &bottom, &right);
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            return "Universe is too big to extract all cells!";
        }
        bool multistate = universe->NumCellStates() > 2;
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        int cx, cy;
        int v = 0;
        int arraylen = 0;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = universe->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;
                    if (shift) {
                        // shift cells so that top left cell of bounding box is at 0,0
                        lua_pushinteger(L, cx - ileft); lua_rawseti(L, -2, ++arraylen);
                        lua_pushinteger(L, cy - itop ); lua_rawseti(L, -2, ++arraylen);
                    } else {
                        lua_pushinteger(L, cx); lua_rawseti(L, -2, ++arraylen);
                        lua_pushinteger(L, cy); lua_rawseti(L, -2, ++arraylen);
                    }
                    if (multistate) {
                        lua_pushinteger(L, v); lua_rawseti(L, -2, ++arraylen);
                    }
                } else {
                    cx = iright;  // done this row
                }
            }
        }
        if (multistate && arraylen > 0 && (arraylen & 1) == 0) {
            // add padding zero so the cell array has an odd number of ints
            // (this is how we distinguish multi-state arrays from one-state arrays;
            // the latter always have an even number of ints)
            lua_pushinteger(L, 0); lua_rawseti(L, -2, ++arraylen);
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------

static int g_load(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* filename = luaL_checkstring(L, 1);
    
    // create temporary universe of same type as current universe
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype, false);
    // readpattern will call setrule
    
    // read pattern into temporary universe
    const char* err = readpattern(FILENAME, *tempalgo);
    if (err) {
        // try all other algos until readpattern succeeds
        for (int i = 0; i < NumAlgos(); i++) {
            if (i != currlayer->algtype) {
                delete tempalgo;
                tempalgo = CreateNewUniverse(i, false);
                err = readpattern(FILENAME, *tempalgo);
                if (!err) break;
            }
        }
    }
    
    if (err) {
        delete tempalgo;
        GollyError(L, err);
    }
    
    // convert pattern into a cell array, shifting cell coords so that the
    // bounding box's top left cell is at 0,0
    err = ExtractCellArray(L, tempalgo, true);
    delete tempalgo;
    if (err) GollyError(L, err);
    
    return 1;   // result is a cell array
}

// -----------------------------------------------------------------------------

static int g_store(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    luaL_checktype(L, 1, LUA_TTABLE);   // cell array
    int len = luaL_len(L, 1);
    
    const char* filename = luaL_checkstring(L, 2);
    
    // create temporary universe of same type as current universe
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype, false);
    const char* err = tempalgo->setrule(currlayer->algo->getrule());
    if (err) tempalgo->setrule(tempalgo->DefaultRule());
    
    if (savexrle) {
        // ensure non-zero generation count will be included in #CXRLE comment
        tempalgo->setGeneration( currlayer->algo->getGeneration() );
    }
    
    // copy cell array into temporary universe
    bool multistate = (len & 1) == 1;
    int ints_per_cell = multistate ? 3 : 2;
    int num_cells = len / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        lua_rawgeti(L, 1, item+1); int x = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, item+2); int y = lua_tointeger(L,-1); lua_pop(L,1);
        
        // check if x,y is outside bounded grid
        err = GSF_checkpos(tempalgo, x, y);
        if (err) {
            delete tempalgo;
            GollyError(L, err);
        }
        
        if (multistate) {
            lua_rawgeti(L, 1, item+3); int state = lua_tointeger(L,-1); lua_pop(L,1);
            if (tempalgo->setcell(x, y, state) < 0) {
                tempalgo->endofpattern();
                delete tempalgo;
                GollyError(L, "store error: state value is out of range.");
            }
        } else {
            tempalgo->setcell(x, y, 1);
        }
    }
    tempalgo->endofpattern();
    
    // write pattern to given file in RLE/XRLE format
    bigint top, left, bottom, right;
    tempalgo->findedges(&top, &left, &bottom, &right);
    pattern_format format = savexrle ? XRLE_format : RLE_format;
    // if grid is bounded then force XRLE_format so that position info is recorded
    if (tempalgo->gridwd > 0 || tempalgo->gridht > 0) format = XRLE_format;
    err = writepattern(FILENAME, *tempalgo, format, no_compression,
                       top.toint(), left.toint(), bottom.toint(), right.toint());
    delete tempalgo;
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_setdir(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* dirname = luaL_checkstring(L, 1);
    const char* newdir = luaL_checkstring(L, 2);
    const char* err;
    
    err = GSF_setdir(dirname, wxString(newdir, LUA_ENC));
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getdir(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* dirname = luaL_checkstring(L, 1);

    const char* dirstring = GSF_getdir(dirname);
    if (dirstring == NULL) GollyError(L, "getdir error: unknown directory name.");

    lua_pushstring(L, dirstring);
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getfiles(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* dirname = luaL_checkstring(L, 1);
    
    wxString dirpath = wxString(dirname, LUA_ENC);

    if (!wxFileName::DirExists(dirpath)) {
        // return nil so scripts can easily detect a non-existent directory
        lua_pushnil(L);
        return 1;   // result is nil
    }

    lua_newtable(L);
    int arraylen = 0;
    
    wxDir dir(dirpath);
    if (dir.IsOpened()) {
        wxString filename;
        // get all files
        bool more = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_HIDDEN);
        while (more) {
            lua_pushstring(L, (const char*)filename.mb_str(LUA_ENC));
            lua_rawseti(L, -2, ++arraylen);
            more = dir.GetNext(&filename);
        }
        // now get all directories and append platform-specific path separator
        more = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS | wxDIR_HIDDEN);
        while (more) {
            filename += wxFILE_SEP_PATH;
            lua_pushstring(L, (const char*)filename.mb_str(LUA_ENC));
            lua_rawseti(L, -2, ++arraylen);
            more = dir.GetNext(&filename);
        }
    }
    
    return 1;   // result is a table of strings
}

// -----------------------------------------------------------------------------

static int g_getpath(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushstring(L, GSF_getpath());
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getinfo(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushstring(L, GSF_getinfo());
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_geturl(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    bool result = mainptr->DownloadURL(wxString(luaL_checkstring(L, 1), LUA_ENC),
                                       wxString(luaL_checkstring(L, 2), LUA_ENC));
    lua_pushboolean(L, result);
    
    return 1;   // result is a bool
}

// -----------------------------------------------------------------------------

static int g_new(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    mainptr->NewPattern(wxString(luaL_checkstring(L, 1), LUA_ENC));
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_cut(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (viewptr->SelectionExists()) {
        viewptr->CutSelection();
        DoAutoUpdate();
    } else {
        GollyError(L, "cut error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_copy(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (viewptr->SelectionExists()) {
        viewptr->CopySelection();
        DoAutoUpdate();
    } else {
        GollyError(L, "copy error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_clear(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int where = luaL_checkinteger(L, 1);

    if (viewptr->SelectionExists()) {
        if (where == 0)
            viewptr->ClearSelection();
        else
            viewptr->ClearOutsideSelection();
        DoAutoUpdate();
    } else {
        GollyError(L, "clear error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_paste(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    const char* mode = luaL_checkstring(L, 3);
    const char* err;
    
    err = GSF_paste(x, y, mode);    
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_shrink(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    bool remove_if_empty = false;
    if (lua_gettop(L) > 0) remove_if_empty = CheckBoolean(L, 1);

    if (viewptr->SelectionExists()) {
        currlayer->currsel.Shrink(false, remove_if_empty);
                               // false == don't fit in viewport
        DoAutoUpdate();
    } else {
        GollyError(L, "shrink error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_randfill(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int perc = luaL_checkinteger(L, 1);

    if (perc < 1 || perc > 100) {
        GollyError(L, "randfill error: percentage must be from 1 to 100.");
    }
    
    if (viewptr->SelectionExists()) {
        int oldperc = randomfill;
        randomfill = perc;
        viewptr->RandomFill();
        randomfill = oldperc;
        DoAutoUpdate();
    } else {
        GollyError(L, "randfill error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_flip(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int direction = luaL_checkinteger(L, 1);

    if (viewptr->SelectionExists()) {
        viewptr->FlipSelection(direction != 0);    // 1 = top-bottom
        DoAutoUpdate();
    } else {
        GollyError(L, "flip error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_rotate(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int direction = luaL_checkinteger(L, 1);

    if (viewptr->SelectionExists()) {
        viewptr->RotateSelection(direction == 0);    // 0 = clockwise
        DoAutoUpdate();
    } else {
        GollyError(L, "rotate error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_parse(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* s = luaL_checkstring(L, 1);
    
    // defaults for optional params
    int x0  = 0;
    int y0  = 0;
    int axx = 1;
    int axy = 0;
    int ayx = 0;
    int ayy = 1;
    
    if (lua_gettop(L) > 1) x0 = luaL_checkinteger(L, 2);
    if (lua_gettop(L) > 2) y0 = luaL_checkinteger(L, 3);
    if (lua_gettop(L) > 3) axx = luaL_checkinteger(L, 4);
    if (lua_gettop(L) > 4) axy = luaL_checkinteger(L, 5);
    if (lua_gettop(L) > 5) ayx = luaL_checkinteger(L, 6);
    if (lua_gettop(L) > 6) ayy = luaL_checkinteger(L, 7);

    lua_newtable(L);
    int arraylen = 0;

    int x = 0;
    int y = 0;
    
    if (strchr(s, '*')) {
        // parsing 'visual' format
        int c = *s++;
        while (c) {
            switch (c) {
                case '\n': if (x) { x = 0; y++; } break;
                case '.': x++; break;
                case '*':
                    lua_pushinteger(L, x0 + x * axx + y * axy); lua_rawseti(L, -2, ++arraylen);
                    lua_pushinteger(L, y0 + x * ayx + y * ayy); lua_rawseti(L, -2, ++arraylen);
                    x++;
                    break;
            }
            c = *s++;
        }
    } else {
        // parsing RLE format; first check if multi-state data is present
        bool multistate = false;
        const char* p = s;
        while (*p) {
            char c = *p++;
            if ((c == '.') || ('p' <= c && c <= 'y') || ('A' <= c && c <= 'X')) {
                multistate = true;
                break;
            }
        }
        int prefix = 0;
        bool done = false;
        int c = *s++;
        while (c && !done) {
            if (isdigit(c))
                prefix = 10 * prefix + (c - '0');
            else {
                prefix += (prefix == 0);
                switch (c) {
                    case '!': done = true; break;
                    case '$': x = 0; y += prefix; break;
                    case 'b': x += prefix; break;
                    case '.': x += prefix; break;
                    case 'o':
                        for (int k = 0; k < prefix; k++, x++) {
                            lua_pushinteger(L, x0 + x * axx + y * axy); lua_rawseti(L, -2, ++arraylen);
                            lua_pushinteger(L, y0 + x * ayx + y * ayy); lua_rawseti(L, -2, ++arraylen);
                            if (multistate) {
                                lua_pushinteger(L, 1); lua_rawseti(L, -2, ++arraylen);
                            }
                        }
                        break;
                    default:
                        if (('p' <= c && c <= 'y') || ('A' <= c && c <= 'X')) {
                            // multistate must be true
                            int state;
                            if (c < 'p') {
                                state = c - 'A' + 1;
                            } else {
                                state = 24 * (c - 'p' + 1);
                                c = *s++;
                                if ('A' <= c && c <= 'X') {
                                    state = state + c - 'A' + 1;
                                } else {
                                    // be forgiving and treat 'p'..'y' like 'o'
                                    state = 1;
                                    s--;
                                }
                            }
                            for (int k = 0; k < prefix; k++, x++) {
                                lua_pushinteger(L, x0 + x * axx + y * axy); lua_rawseti(L, -2, ++arraylen);
                                lua_pushinteger(L, y0 + x * ayx + y * ayy); lua_rawseti(L, -2, ++arraylen);
                                lua_pushinteger(L, state); lua_rawseti(L, -2, ++arraylen);
                            }
                        }
                }
                prefix = 0;
            }
            c = *s++;
        }
        if (multistate && arraylen > 0 && (arraylen & 1) == 0) {
            // add padding zero
            lua_pushinteger(L, 0); lua_rawseti(L, -2, ++arraylen);
        }
    }
    
    return 1;   // result is a cell array
}

// -----------------------------------------------------------------------------

static int g_transform(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    luaL_checktype(L, 1, LUA_TTABLE);   // cell array

    int x0 = luaL_checkinteger(L, 2);
    int y0 = luaL_checkinteger(L, 3);
    
    // defaults for optional params
    int axx = 1;
    int axy = 0;
    int ayx = 0;
    int ayy = 1;

    if (lua_gettop(L) > 3) axx = luaL_checkinteger(L, 4);
    if (lua_gettop(L) > 4) axy = luaL_checkinteger(L, 5);
    if (lua_gettop(L) > 5) ayx = luaL_checkinteger(L, 6);
    if (lua_gettop(L) > 6) ayy = luaL_checkinteger(L, 7);

    lua_newtable(L);
    int arraylen = 0;

    bool multistate = (luaL_len(L, 1) & 1) == 1;
    int ints_per_cell = multistate ? 3 : 2;
    int num_cells = luaL_len(L, 1) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        lua_rawgeti(L, 1, item+1); int x = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, item+2); int y = lua_tointeger(L,-1); lua_pop(L,1);
        
        lua_pushinteger(L, x0 + x * axx + y * axy); lua_rawseti(L, -2, ++arraylen);
        lua_pushinteger(L, y0 + x * ayx + y * ayy); lua_rawseti(L, -2, ++arraylen);
        if (multistate) {
            lua_rawgeti(L, 1, item+3); int state = lua_tointeger(L,-1); lua_pop(L,1);
            lua_pushinteger(L, state); lua_rawseti(L, -2, ++arraylen);
        }
    }
    if (multistate && arraylen > 0 && (arraylen & 1) == 0) {
        // add padding zero
        lua_pushinteger(L, 0); lua_rawseti(L, -2, ++arraylen);
    }
    
    return 1;   // result is a cell array
}

// -----------------------------------------------------------------------------

static int g_evolve(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    luaL_checktype(L, 1, LUA_TTABLE);   // cell array
    
    int ngens = luaL_checkinteger(L, 2);
    if (ngens < 0) {
        GollyError(L, "evolve error: number of generations is negative.");
    }
    
    // create a temporary universe of same type as current universe
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype, false);
    const char* err = tempalgo->setrule(currlayer->algo->getrule());
    if (err) tempalgo->setrule(tempalgo->DefaultRule());
    
    // copy cell array into temporary universe
    bool multistate = (luaL_len(L, 1) & 1) == 1;
    int ints_per_cell = multistate ? 3 : 2;
    int num_cells = luaL_len(L, 1) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        lua_rawgeti(L, 1, item+1); int x = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, item+2); int y = lua_tointeger(L,-1); lua_pop(L,1);
        // check if x,y is outside bounded grid
        err = GSF_checkpos(tempalgo, x, y);
        if (err) {
            delete tempalgo;
            GollyError(L, err);
        }
        if (multistate) {
            lua_rawgeti(L, 1, item+3); int state = lua_tointeger(L,-1); lua_pop(L,1);
            if (tempalgo->setcell(x, y, state) < 0) {
                tempalgo->endofpattern();
                delete tempalgo;
                GollyError(L, "evolve error: state value is out of range.");
            }
        } else {
            tempalgo->setcell(x, y, 1);
        }
    }
    tempalgo->endofpattern();
    
    // advance pattern by ngens
    mainptr->generating = true;
    if (tempalgo->unbounded && (tempalgo->gridwd > 0 || tempalgo->gridht > 0)) {
        // a bounded grid must use an increment of 1 so we can call
        // CreateBorderCells and DeleteBorderCells around each step()
        tempalgo->setIncrement(1);
        while (ngens > 0) {
            if (!tempalgo->CreateBorderCells()) break;
            tempalgo->step();
            if (!tempalgo->DeleteBorderCells()) break;
            ngens--;
        }
    } else {
        tempalgo->setIncrement(ngens);
        tempalgo->step();
    }
    mainptr->generating = false;
    
    // convert new pattern into a cell array    
    err = ExtractCellArray(L, tempalgo);
    delete tempalgo;
    if (err) GollyError(L, err);
    
    return 1;   // result is a cell array
}

// -----------------------------------------------------------------------------

static const char* BAD_STATE = "putcells error: state value is out of range.";

static int g_putcells(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    luaL_checktype(L, 1, LUA_TTABLE);   // cell array

    // defaults for optional params
    int x0  = 0;
    int y0  = 0;
    int axx = 1;
    int axy = 0;
    int ayx = 0;
    int ayy = 1;
    // default for mode is 'or'; 'xor' mode is also supported;
    // for a one-state array 'copy' mode currently has the same effect as 'or' mode
    // because there is no bounding box to set dead cells, but a multi-state array can
    // have dead cells so in that case 'copy' mode is not the same as 'or' mode
    const char* mode = "or";
    
    if (lua_gettop(L) > 1) x0 = luaL_checkinteger(L, 2);
    if (lua_gettop(L) > 2) y0 = luaL_checkinteger(L, 3);
    if (lua_gettop(L) > 3) axx = luaL_checkinteger(L, 4);
    if (lua_gettop(L) > 4) axy = luaL_checkinteger(L, 5);
    if (lua_gettop(L) > 5) ayx = luaL_checkinteger(L, 6);
    if (lua_gettop(L) > 6) ayy = luaL_checkinteger(L, 7);
    if (lua_gettop(L) > 7) mode = luaL_checkstring(L, 8);
        
    wxString modestr = wxString(mode, LUA_ENC);
    if ( !(   modestr.IsSameAs(wxT("or"), false)
           || modestr.IsSameAs(wxT("xor"), false)
           || modestr.IsSameAs(wxT("copy"), false)
           || modestr.IsSameAs(wxT("and"), false)
           || modestr.IsSameAs(wxT("not"), false)) ) {
        GollyError(L, "putcells error: unknown mode.");
    }
    
    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    // use ChangeCell below and combine all changes due to consecutive setcell/putcells
    // if (savecells) SavePendingChanges();
    
    bool multistate = (luaL_len(L, 1) & 1) == 1;
    int ints_per_cell = multistate ? 3 : 2;
    int num_cells = luaL_len(L, 1) / ints_per_cell;
    const char* err = NULL;
    bool pattchanged = false;
    lifealgo* curralgo = currlayer->algo;
    
    if (modestr.IsSameAs(wxT("copy"), false)) {
        // TODO: find bounds of cell array and call ClearRect here (to be added to wxedit.cpp)
    }
    
    if (modestr.IsSameAs(wxT("and"), false)) {
        if (!curralgo->isEmpty()) {
            int newstate = 1;
            for (int n = 0; n < num_cells; n++) {
                int item = ints_per_cell * n;
                lua_rawgeti(L, 1, item+1); int x = lua_tointeger(L,-1); lua_pop(L,1);
                lua_rawgeti(L, 1, item+2); int y = lua_tointeger(L,-1); lua_pop(L,1);
                int newx = x0 + x * axx + y * axy;
                int newy = y0 + x * ayx + y * ayy;
                // check if newx,newy is outside bounded grid
                err = GSF_checkpos(curralgo, newx, newy);
                if (err) break;
                int oldstate = curralgo->getcell(newx, newy);
                if (multistate) {
                    // multi-state arrays can contain dead cells so newstate might be 0
                    lua_rawgeti(L, 1, item+3); newstate = lua_tointeger(L,-1); lua_pop(L,1);
                }
                if (newstate != oldstate && oldstate > 0) {
                    curralgo->setcell(newx, newy, 0);
                    if (savecells) ChangeCell(newx, newy, oldstate, 0);
                    pattchanged = true;
                }
            }
        }
    } else if (modestr.IsSameAs(wxT("xor"), false)) {
        // loop code is duplicated here to allow 'or' case to execute faster
        int numstates = curralgo->NumCellStates();
        for (int n = 0; n < num_cells; n++) {
            int item = ints_per_cell * n;
            lua_rawgeti(L, 1, item+1); int x = lua_tointeger(L,-1); lua_pop(L,1);
            lua_rawgeti(L, 1, item+2); int y = lua_tointeger(L,-1); lua_pop(L,1);
            int newx = x0 + x * axx + y * axy;
            int newy = y0 + x * ayx + y * ayy;
            // check if newx,newy is outside bounded grid
            err = GSF_checkpos(curralgo, newx, newy);
            if (err) break;
            int oldstate = curralgo->getcell(newx, newy);
            int newstate;
            if (multistate) {
                // multi-state arrays can contain dead cells so newstate might be 0
                lua_rawgeti(L, 1, item+3); newstate = lua_tointeger(L,-1); lua_pop(L,1);
                if (newstate == oldstate) {
                    if (oldstate != 0) newstate = 0;
                } else {
                    newstate = newstate ^ oldstate;
                    // if xor overflows then don't change current state
                    if (newstate >= numstates) newstate = oldstate;
                }
                if (newstate != oldstate) {
                    // paste (possibly transformed) cell into current universe
                    if (curralgo->setcell(newx, newy, newstate) < 0) {
                        err = BAD_STATE;
                        break;
                    }
                    if (savecells) ChangeCell(newx, newy, oldstate, newstate);
                    pattchanged = true;
                }
            } else {
                // one-state arrays only contain live cells
                newstate = 1 - oldstate;
                // paste (possibly transformed) cell into current universe
                if (curralgo->setcell(newx, newy, newstate) < 0) {
                    err = BAD_STATE;
                    break;
                }
                if (savecells) ChangeCell(newx, newy, oldstate, newstate);
                pattchanged = true;
            }
        }
    } else {
        bool notmode = modestr.IsSameAs(wxT("not"), false);
        bool ormode = modestr.IsSameAs(wxT("or"), false);
        int newstate = notmode ? 0 : 1;
        int maxstate = curralgo->NumCellStates() - 1;
        for (int n = 0; n < num_cells; n++) {
            int item = ints_per_cell * n;
            lua_rawgeti(L, 1, item+1); int x = lua_tointeger(L,-1); lua_pop(L,1);
            lua_rawgeti(L, 1, item+2); int y = lua_tointeger(L,-1); lua_pop(L,1);
            int newx = x0 + x * axx + y * axy;
            int newy = y0 + x * ayx + y * ayy;
            // check if newx,newy is outside bounded grid
            err = GSF_checkpos(curralgo, newx, newy);
            if (err) break;
            int oldstate = curralgo->getcell(newx, newy);
            if (multistate) {
                // multi-state arrays can contain dead cells so newstate might be 0
                lua_rawgeti(L, 1, item+3); newstate = lua_tointeger(L,-1); lua_pop(L,1);
                if (notmode) newstate = maxstate - newstate;
                if (ormode && newstate == 0) newstate = oldstate;
            }
            if (newstate != oldstate) {
                // paste (possibly transformed) cell into current universe
                if (curralgo->setcell(newx, newy, newstate) < 0) {
                    err = BAD_STATE;
                    break;
                }
                if (savecells) ChangeCell(newx, newy, oldstate, newstate);
                pattchanged = true;
            }
        }
    }
    
    if (pattchanged) {
        curralgo->endofpattern();
        MarkLayerDirty();
        DoAutoUpdate();
    }
    
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getcells(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    luaL_checktype(L, 1, LUA_TTABLE);   // rect array with 0 or 4 ints

    lua_newtable(L);
    int arraylen = 0;
    
    int numints = luaL_len(L, 1);
    if (numints == 0) {
        // return empty cell array
    } else if (numints == 4) {
        lua_rawgeti(L, 1, 1); int ileft = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 2); int itop  = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 3); int wd    = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 4); int ht    = luaL_checkinteger(L,-1); lua_pop(L,1);
        
        const char* err = GSF_checkrect(ileft, itop, wd, ht);
        if (err) GollyError(L, err);
        
        int iright = ileft + wd - 1;
        int ibottom = itop + ht - 1;
        int cx, cy;
        int v = 0;
        lifealgo* curralgo = currlayer->algo;
        bool multistate = curralgo->NumCellStates() > 2;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = curralgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;
                    if (cx <= iright) {
                        lua_pushinteger(L, cx); lua_rawseti(L, -2, ++arraylen);
                        lua_pushinteger(L, cy); lua_rawseti(L, -2, ++arraylen);
                        if (multistate) {
                            lua_pushinteger(L, v); lua_rawseti(L, -2, ++arraylen);
                        }
                    }
                } else {
                    cx = iright;  // done this row
                }
            }
        }
        if (multistate && arraylen > 0 && (arraylen & 1) == 0) {
            // add padding zero
            lua_pushinteger(L, 0); lua_rawseti(L, -2, ++arraylen);
        }
    } else {
        GollyError(L, "getcells error: array must be {} or {x,y,wd,ht}.");
    }
    
    return 1;   // result is a cell array
}

// -----------------------------------------------------------------------------

// maybe only use algo->getcells method if algo is hash-based???!!!
// (needs more thought and more testing)

#if 0

static int g_getcells2(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    luaL_checktype(L, 1, LUA_TTABLE);   // rect array with 0 or 4 ints

    lua_newtable(L);
    int arraylen = 0;
    
    int numints = luaL_len(L, 1);
    if (numints == 0) {
        // return empty cell array
    } else if (numints == 4) {
        lua_rawgeti(L, 1, 1); int ileft = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 2); int itop  = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 3); int wd    = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 4); int ht    = luaL_checkinteger(L,-1); lua_pop(L,1);
        
        const char* err = GSF_checkrect(ileft, itop, wd, ht);
        if (err) GollyError(L, err);
        
        int iright = ileft + wd - 1;
        int ibottom = itop + ht - 1;
        int cx, cy;
        lifealgo* curralgo = currlayer->algo;
        bool multistate = curralgo->NumCellStates() > 2;
        
        unsigned char* cellmem = (unsigned char*) malloc(wd * ht);
        if (cellmem == NULL) {
            GollyError(L, "getcells error: not enough memory.");
        }
        curralgo->getcells(cellmem, ileft, itop, wd, ht);
        unsigned char* cellptr = cellmem;
        
        for ( cy=itop; cy<=ibottom; cy++ ) {
            for ( cx=ileft; cx<=iright; cx++ ) {
                unsigned char state = *cellptr++;
                if (state > 0) {
                    // found next live cell
                    lua_pushinteger(L, cx); lua_rawseti(L, -2, ++arraylen);
                    lua_pushinteger(L, cy); lua_rawseti(L, -2, ++arraylen);
                    if (multistate) {
                        lua_pushinteger(L, state); lua_rawseti(L, -2, ++arraylen);
                    }
                }
            }
        }
        if (multistate && arraylen > 0 && (arraylen & 1) == 0) {
            // add padding zero
            lua_pushinteger(L, 0); lua_rawseti(L, -2, ++arraylen);
        }
        
        free(cellmem);
        
    } else {
        GollyError(L, "getcells error: array must be {} or {x,y,wd,ht}.");
    }
    
    return 1;   // result is a cell array
}

#endif // #if 0

// -----------------------------------------------------------------------------

static int g_join(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);
    
    bool multi1 = (luaL_len(L, 1) & 1) == 1;
    bool multi2 = (luaL_len(L, 2) & 1) == 1;
    bool multiout = multi1 || multi2;
    int ints_per_cell, num_cells;
    int x, y, state;
    
    lua_newtable(L);
    int arraylen = 0;

    // append 1st array
    ints_per_cell = multi1 ? 3 : 2;
    num_cells = luaL_len(L, 1) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        lua_rawgeti(L, 1, item+1); x = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, item+2); y = lua_tointeger(L,-1); lua_pop(L,1);
        if (multi1) {
            lua_rawgeti(L, 1, item+3); state = lua_tointeger(L,-1); lua_pop(L,1);
        } else {
            state = 1;
        }
        lua_pushinteger(L, x); lua_rawseti(L, -2, ++arraylen);
        lua_pushinteger(L, y); lua_rawseti(L, -2, ++arraylen);
        if (multiout) {
            lua_pushinteger(L, state); lua_rawseti(L, -2, ++arraylen);
        }
    }
    
    // append 2nd array
    ints_per_cell = multi2 ? 3 : 2;
    num_cells = luaL_len(L, 2) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        lua_rawgeti(L, 2, item+1); x = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 2, item+2); y = lua_tointeger(L,-1); lua_pop(L,1);
        if (multi2) {
            lua_rawgeti(L, 2, item+3); state = lua_tointeger(L,-1); lua_pop(L,1);
        } else {
            state = 1;
        }
        lua_pushinteger(L, x); lua_rawseti(L, -2, ++arraylen);
        lua_pushinteger(L, y); lua_rawseti(L, -2, ++arraylen);
        if (multiout) {
            lua_pushinteger(L, state); lua_rawseti(L, -2, ++arraylen);
        }
    }
    
    if (multiout && arraylen > 0 && (arraylen & 1) == 0) {
        // add padding zero
        lua_pushinteger(L, 0); lua_rawseti(L, -2, ++arraylen);
    }
    
    return 1;   // result is a cell array
}

// -----------------------------------------------------------------------------

static int g_hash(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    // arg must be a table with 4 ints
    luaL_checktype(L, 1, LUA_TTABLE);
    
    int numints = luaL_len(L, 1);
    if (numints != 4) {
        GollyError(L, "hash error: array must have 4 integers.");
    }
    
    lua_rawgeti(L, 1, 1); int x  = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 2); int y  = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 3); int wd = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 4); int ht = luaL_checkinteger(L,-1); lua_pop(L,1);
    
    const char* err = GSF_checkrect(x, y, wd, ht);
    if (err) GollyError(L, err);

    lua_pushinteger(L, GSF_hash(x, y, wd, ht));
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_getclip(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (!mainptr->ClipboardHasText()) {
        GollyError(L, "getclip error: no pattern in clipboard.");
    }
    
    // create a temporary layer for storing the clipboard pattern
    Layer* templayer = CreateTemporaryLayer();
    if (!templayer) {
        GollyError(L, "getclip error: failed to create temporary layer.");
    }
    
    bool ignorewarning = false;
    if (lua_gettop(L) > 0) {
	int arg = luaL_checkinteger(L, 1);
	if (arg != 0) {
		ignorewarning = true;
	}
    }

    // read clipboard pattern into temporary universe and set edges
    // (not a minimal bounding box if pattern is empty or has empty borders)
    bigint top, left, bottom, right;
    if ( viewptr->GetClipboardPattern(templayer, &top, &left, &bottom, &right, ignorewarning) ) {
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            delete templayer;
            GollyError(L, "getclip error: pattern is too big.");
        }
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        int wd = iright - ileft + 1;
        int ht = ibottom - itop + 1;

        // push pattern's width and height (not necessarily the minimal bounding box
        // because the pattern might have empty borders, or it might even be empty)
        lua_pushinteger(L, wd);
        lua_pushinteger(L, ht);
        
        // now push cell array
        lua_newtable(L);
        int arraylen = 0;
        
        // extract cells from templayer
        lifealgo* tempalgo = templayer->algo;
        bool multistate = tempalgo->NumCellStates() > 2;
        int cx, cy;
        int v = 0;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = tempalgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;

                    // shift cells so that top left cell of bounding box is at 0,0
                    lua_pushinteger(L, cx - ileft); lua_rawseti(L, -2, ++arraylen);
                    lua_pushinteger(L, cy - itop ); lua_rawseti(L, -2, ++arraylen);

                    if (multistate) {
                        lua_pushinteger(L, v); lua_rawseti(L, -2, ++arraylen);
                    }
                } else {
                    cx = iright;  // done this row
                }
            }
        }
        // if no live cells then return {wd,ht} rather than {wd,ht,0}
        if (multistate && arraylen > 2 && (arraylen & 1) == 0) {
            // add padding zero
            lua_pushinteger(L, 0); lua_rawseti(L, -2, ++arraylen);
        }
        delete templayer;
    } else {
        delete templayer;
        GollyError(L, "getclip error: failed to get clipboard pattern.");
    }
    
    return 3;   // result is wd, ht, cell array
}

// -----------------------------------------------------------------------------

static int g_select(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    // arg must be a table with 0 or 4 ints
    luaL_checktype(L, 1, LUA_TTABLE);
    
    int numints = luaL_len(L, 1);
    if (numints == 0) {
        // remove any existing selection
        GSF_select(0, 0, 0, 0);
    
    } else if (numints == 4) {
        lua_rawgeti(L, 1, 1); int x  = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 2); int y  = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 3); int wd = luaL_checkinteger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 4); int ht = luaL_checkinteger(L,-1); lua_pop(L,1);
        
        const char* err = GSF_checkrect(x, y, wd, ht);
        if (err) GollyError(L, err);
    
        GSF_select(x, y, wd, ht);
        
    } else {
        GollyError(L, "select error: array must be {} or {x,y,wd,ht}.");
    }

    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getrect(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_newtable(L);

    if (!currlayer->algo->isEmpty()) {
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if (viewptr->OutsideLimits(top, left, bottom, right)) {
            GollyError(L, "getrect error: pattern is too big.");
        }
        
        int x = left.toint();
        int y = top.toint();
        int wd = right.toint() - x + 1;
        int ht = bottom.toint() - y + 1;
        
        lua_pushinteger(L, x);  lua_rawseti(L, -2, 1);
        lua_pushinteger(L, y);  lua_rawseti(L, -2, 2);
        lua_pushinteger(L, wd); lua_rawseti(L, -2, 3);
        lua_pushinteger(L, ht); lua_rawseti(L, -2, 4);
    }
    
    return 1;   // result is a table (empty or with 4 ints)
}

// -----------------------------------------------------------------------------

static int g_getselrect(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_newtable(L);

    if (viewptr->SelectionExists()) {
        if (currlayer->currsel.TooBig()) {
            GollyError(L, "getselrect error: selection is too big.");
        }
        
        int x, y, wd, ht;
        currlayer->currsel.GetRect(&x, &y, &wd, &ht);
        
        lua_pushinteger(L, x);  lua_rawseti(L, -2, 1);
        lua_pushinteger(L, y);  lua_rawseti(L, -2, 2);
        lua_pushinteger(L, wd); lua_rawseti(L, -2, 3);
        lua_pushinteger(L, ht); lua_rawseti(L, -2, 4);
    }
    
    return 1;   // result is a table (empty or with 4 ints)
}

// -----------------------------------------------------------------------------

static int g_setcell(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int state = luaL_checkinteger(L, 3);
    const char* err;
    
    err = GSF_setcell(x, y, state);    
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getcell(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    
    // check if x,y is outside bounded grid
    const char* err = GSF_checkpos(currlayer->algo, x, y);
    if (err) GollyError(L, err);
    
    lua_pushinteger(L, currlayer->algo->getcell(x, y));
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_setcursor(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* newcursor = luaL_checkstring(L, 1);

    const char* oldcursor = CursorToString(currlayer->curs);
    wxCursor* cursptr = StringToCursor(newcursor);
    if (cursptr) {
        viewptr->SetCursorMode(cursptr);
        // see the cursor change, including button in edit bar
        mainptr->UpdateUserInterface();
    } else {
        GollyError(L, "setcursor error: unknown cursor string.");
    }
    
    // return old cursor (simplifies saving and restoring cursor)
    lua_pushstring(L, oldcursor);
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getcursor(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushstring(L, CursorToString(currlayer->curs));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_empty(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushboolean(L, currlayer->algo->isEmpty());
    
    return 1;   // result is a boolean
}

// -----------------------------------------------------------------------------

static int g_run(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int ngens = luaL_checkinteger(L, 1);

    if (ngens > 0 && !currlayer->algo->isEmpty()) {
        if (ngens > 1) {
            bigint saveinc = currlayer->algo->getIncrement();
            currlayer->algo->setIncrement(ngens);
            mainptr->NextGeneration(true);            // step by ngens
            currlayer->algo->setIncrement(saveinc);
        } else {
            mainptr->NextGeneration(false);           // step 1 gen
        }
        DoAutoUpdate();
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_step(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (!currlayer->algo->isEmpty()) {
        mainptr->NextGeneration(true);      // step by current increment
        DoAutoUpdate();
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_setstep(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int exp = luaL_checkinteger(L, 1);

    mainptr->SetStepExponent(exp);
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getstep(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushinteger(L, currlayer->currexpo);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_setbase(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int base = luaL_checkinteger(L, 1);
    
    if (base < 2) base = 2;
    if (base > MAX_BASESTEP) base = MAX_BASESTEP;
    currlayer->currbase = base;

    mainptr->SetGenIncrement();
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getbase(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushinteger(L, currlayer->currbase);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_advance(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int where = luaL_checkinteger(L, 1);
    int ngens = luaL_checkinteger(L, 2);

    if (ngens > 0) {
        if (viewptr->SelectionExists()) {
            while (ngens > 0) {
                ngens--;
                if (where == 0)
                    currlayer->currsel.Advance();
                else
                    currlayer->currsel.AdvanceOutside();
            }
            DoAutoUpdate();
        } else {
            GollyError(L, "advance error: no selection.");
        }
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_reset(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (currlayer->algo->getGeneration() != currlayer->startgen) {
        mainptr->ResetPattern();
        DoAutoUpdate();
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_setgen(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* genstring = luaL_checkstring(L, 1);
    const char* err;
    
    err = GSF_setgen(genstring);    
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getgen(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    char sepchar = '\0';
    if (lua_gettop(L) > 0) {
        const char* s = luaL_checkstring(L, 1);
        sepchar = s[0];
    }
    
    lua_pushstring(L, currlayer->algo->getGeneration().tostring(sepchar));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getpop(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    char sepchar = '\0';
    if (lua_gettop(L) > 0) {
        const char* s = luaL_checkstring(L, 1);
        sepchar = s[0];
    }
    
    lua_pushstring(L, currlayer->algo->getPopulation().tostring(sepchar));

    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_numstates(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    lua_pushinteger(L, currlayer->algo->NumCellStates());
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_numalgos(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushinteger(L, NumAlgos());
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_setalgo(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* algostring = luaL_checkstring(L, 1);    
    const char* err = GSF_setalgo(algostring);
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getalgo(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int index = currlayer->algtype;
    if (lua_gettop(L) > 0) index = luaL_checkinteger(L, 1);
    
    if (index < 0 || index >= NumAlgos()) {
        char msg[64];
        sprintf(msg, "getalgo error: bad index (%d)", index);
        GollyError(L, msg);
    }
    
    lua_pushstring(L, GetAlgoName(index));

    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_setrule(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* rulestring = luaL_checkstring(L, 1);    
    const char* err = GSF_setrule(rulestring);
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getrule(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    lua_pushstring(L, currlayer->algo->getrule());

    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getwidth(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushinteger(L, currlayer->algo->gridwd);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_getheight(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushinteger(L, currlayer->algo->gridht);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_getgridtype(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    lifealgo::TGridType gridtype = currlayer->algo->getgridtype();
    switch (gridtype) {
        case lifealgo::TRI_GRID:
            lua_pushstring(L, "Triangular");
            break;

        case lifealgo::HEX_GRID:
            lua_pushstring(L, "Hexagonal");
            break;

        case lifealgo::VN_GRID:
            lua_pushstring(L, "von Neumann");
            break;

        default:
            // use Square as default
            lua_pushstring(L, "Square");
            break;
    }

    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_setpos(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* x = luaL_checkstring(L, 1);
    const char* y = luaL_checkstring(L, 2);    
    const char* err = GSF_setpos(x, y);
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getpos(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    char sepchar = '\0';
    if (lua_gettop(L) > 0) {
        const char* s = luaL_checkstring(L, 1);
        sepchar = s[0];
    }
    
    bigint bigx, bigy;
    viewptr->GetPos(bigx, bigy);

    // return position as x,y strings
    lua_pushstring(L, bigx.tostring(sepchar));
    lua_pushstring(L, bigy.tostring(sepchar));
    
    return 2;   // result is 2 strings
}

// -----------------------------------------------------------------------------

// this function will remain undocumented until we find a case where it is necessary!!!

static int g_getscale(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushnumber(L, (lua_Number) scalefactor);
    
    return 1;   // result is a floating point number
}

// -----------------------------------------------------------------------------

static int g_getmag(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int mag = viewptr->GetMag();

    // hack for scripts like NewCA.lua that use an overlay covering the viewport
    // but most pixels are close to transparent
    if (int(scalefactor) > 1 && showoverlay &&
        curroverlay->GetOverlayWidth() >= currlayer->view->getwidth() &&
        curroverlay->GetOverlayHeight() >= currlayer->view->getheight() &&
        strcmp(curroverlay->DoOverlayCommand("get 50 50"), "0 0 0 1") == 0 ) {
        mag--;
    }

    lua_pushinteger(L, mag);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_setmag(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int mag = luaL_checkinteger(L, 1);

    // hack for scripts like NewCA.lua that use an overlay covering the viewport
    // but most pixels are close to transparent
    if (int(scalefactor) > 1 && showoverlay &&
        curroverlay->GetOverlayWidth() >= currlayer->view->getwidth() &&
        curroverlay->GetOverlayHeight() >= currlayer->view->getheight() &&
        strcmp(curroverlay->DoOverlayCommand("get 50 50"), "0 0 0 1") == 0 ) {
        mag++;
    }
    
    viewptr->SetMag(mag);
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_fit(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    viewptr->FitPattern();
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_fitsel(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    if (viewptr->SelectionExists()) {
        viewptr->FitSelection();
        DoAutoUpdate();
    } else {
        GollyError(L, "fitsel error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_visrect(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    // arg must be a table with 4 ints
    luaL_checktype(L, 1, LUA_TTABLE);
    
    int numints = luaL_len(L, 1);
    if (numints != 4) {
        GollyError(L, "visrect error: array must have 4 integers.");
    }
    
    lua_rawgeti(L, 1, 1); int x  = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 2); int y  = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 3); int wd = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 4); int ht = luaL_checkinteger(L,-1); lua_pop(L,1);
    
    const char* err = GSF_checkrect(x, y, wd, ht);
    if (err) GollyError(L, err);

    bigint left = x;
    bigint top = y;
    bigint right = x + wd - 1;
    bigint bottom = y + ht - 1;
    int visible = viewptr->CellVisible(left, top) &&
                  viewptr->CellVisible(right, bottom);
    
    lua_pushboolean(L, visible);
    
    return 1;   // result is a boolean
}

// -----------------------------------------------------------------------------

static int g_setview(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int wd = luaL_checkinteger(L, 1);
    int ht = luaL_checkinteger(L, 2);
    if (wd < 0) wd = 0;
    if (ht < 0) ht = 0;

    int currwd, currht;
    bigview->GetClientSize(&currwd, &currht);
    if (currwd < 0) currwd = 0;
    if (currht < 0) currht = 0;
    
    int mnwd, mnht;
    mainptr->GetSize(&mnwd, &mnht);
    mainptr->SetSize(mnwd + (wd - currwd), mnht + (ht - currht));

    #ifdef __WXGTK__
        // needed on Linux to see size change immediately
        insideYield++;
        wxGetApp().Yield(true);
        insideYield--;
    #endif
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getview(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int index = -1;
    if (lua_gettop(L) > 0) {
        index = luaL_checkinteger(L, 1);
        if (index < 0 || index >= numlayers) {
            char msg[64];
            sprintf(msg, "getview error: bad index (%d)", index);
            GollyError(L, msg);
        }
    }

    int wd, ht;
    if (index == -1) {
        bigview->GetClientSize(&wd, &ht);
    } else {
        if (numlayers > 1 && tilelayers) {
            // tilerect values are only valid when multiple layers are tiled
            wd = GetLayer(index)->tilerect.width / int(scalefactor);
            ht = GetLayer(index)->tilerect.height / int(scalefactor);
        } else {
            bigview->GetClientSize(&wd, &ht);
        }
    }
    if (wd < 0) wd = 0;
    if (ht < 0) ht = 0;
    
    lua_pushinteger(L, wd);
    lua_pushinteger(L, ht);
    
    return 2;   // result is 2 integers (width and height)
}

// -----------------------------------------------------------------------------

static int g_update(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    GSF_update();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_autoupdate(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    autoupdate = CheckBoolean(L, 1);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_addlayer(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (numlayers >= MAX_LAYERS) {
        GollyError(L, "addlayer error: no more layers can be added.");
    } else {
        AddLayer();
        DoAutoUpdate();
    }
    
    // return index of new layer
    lua_pushinteger(L, currindex);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_clone(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (numlayers >= MAX_LAYERS) {
        GollyError(L, "clone error: no more layers can be added.");
    } else {
        CloneLayer();
        DoAutoUpdate();
    }
    
    // return index of new layer
    lua_pushinteger(L, currindex);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_duplicate(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (numlayers >= MAX_LAYERS) {
        GollyError(L, "duplicate error: no more layers can be added.");
    } else {
        DuplicateLayer();
        DoAutoUpdate();
    }
    
    // return index of new layer
    lua_pushinteger(L, currindex);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_dellayer(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    if (numlayers <= 1) {
        GollyError(L, "dellayer error: there is only one layer.");
    } else {
        DeleteLayer();
        DoAutoUpdate();
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_movelayer(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    int fromindex = luaL_checkinteger(L, 1);
    int toindex = luaL_checkinteger(L, 2);

    if (fromindex < 0 || fromindex >= numlayers) {
        char msg[64];
        sprintf(msg, "movelayer error: bad fromindex (%d)", fromindex);
        GollyError(L, msg);
    }
    if (toindex < 0 || toindex >= numlayers) {
        char msg[64];
        sprintf(msg, "movelayer error: bad toindex (%d)", toindex);
        GollyError(L, msg);
    }
    
    MoveLayer(fromindex, toindex);
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_setlayer(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int index = luaL_checkinteger(L, 1);

    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "setlayer error: bad index (%d)", index);
        GollyError(L, msg);
    }
    
    SetLayer(index);
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getlayer(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushinteger(L, currindex);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_numlayers(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushinteger(L, numlayers);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_maxlayers(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushinteger(L, MAX_LAYERS);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_setname(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* name = luaL_checkstring(L, 1);
    int index = currindex;
    if (lua_gettop(L) > 1) index = luaL_checkinteger(L, 2);

    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "setname error: bad index (%d)", index);
        GollyError(L, msg);
    }
    
    GSF_setname(wxString(name, LUA_ENC), index);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getname(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int index = currindex;
    if (lua_gettop(L) > 0) index = luaL_checkinteger(L, 1);
    
    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "getname error: bad index (%d)", index);
        GollyError(L, msg);
    }
    
    lua_pushstring(L, (const char*)GetLayer(index)->currname.mb_str(LUA_ENC));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_setcolors(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    luaL_checktype(L, 1, LUA_TTABLE);    
    int len = luaL_len(L, 1);
    if (len == 0) {
        // restore default colors in current layer and its clones
        UpdateLayerColors();
    } else if (len == 6) {
        // create gradient from r1,g1,b1 to r2,g2,b2
        lua_rawgeti(L, 1, 1); int r1 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 2); int g1 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 3); int b1 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 4); int r2 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 5); int g2 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 6); int b2 = lua_tointeger(L,-1); lua_pop(L,1);
        CHECK_RGB(r1, g1, b1, "setcolors");
        CHECK_RGB(r2, g2, b2, "setcolors");
        currlayer->fromrgb.Set(r1, g1, b1);
        currlayer->torgb.Set(r2, g2, b2);
        CreateColorGradient();
        UpdateIconColors();
        UpdateCloneColors();
    } else if (len % 4 == 0) {
        int i = 0;
        while (i < len) {
            lua_rawgeti(L, 1, ++i); int s = lua_tointeger(L,-1); lua_pop(L,1);
            lua_rawgeti(L, 1, ++i); int r = lua_tointeger(L,-1); lua_pop(L,1);
            lua_rawgeti(L, 1, ++i); int g = lua_tointeger(L,-1); lua_pop(L,1);
            lua_rawgeti(L, 1, ++i); int b = lua_tointeger(L,-1); lua_pop(L,1);
            CHECK_RGB(r, g, b, "setcolors");
            if (s == -1) {
                // set all LIVE states to r,g,b (best not to alter state 0)
                for (s = 1; s < currlayer->algo->NumCellStates(); s++) {
                    currlayer->cellr[s] = r;
                    currlayer->cellg[s] = g;
                    currlayer->cellb[s] = b;
                }
            } else {
                if (s < 0 || s >= currlayer->algo->NumCellStates()) {
                    char msg[64];
                    sprintf(msg, "setcolors error: bad state (%d)", s);
                    GollyError(L, msg);
                } else {
                    currlayer->cellr[s] = r;
                    currlayer->cellg[s] = g;
                    currlayer->cellb[s] = b;
                }
            }
        }
        UpdateIconColors();
        UpdateCloneColors();
    } else {
        GollyError(L, "setcolors error: array length is not a multiple of 4.");
    }
    
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getcolors(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int state = -1;
    if (lua_gettop(L) > 0) state = luaL_checkinteger(L, 1);
    
    lua_newtable(L);
    
    if (state == -1) {
        // return colors for ALL states, including state 0
        int tindex = 0;
        for (state = 0; state < currlayer->algo->NumCellStates(); state++) {
            lua_pushinteger(L, state);                   lua_rawseti(L, -2, ++tindex);
            lua_pushinteger(L, currlayer->cellr[state]); lua_rawseti(L, -2, ++tindex);
            lua_pushinteger(L, currlayer->cellg[state]); lua_rawseti(L, -2, ++tindex);
            lua_pushinteger(L, currlayer->cellb[state]); lua_rawseti(L, -2, ++tindex);
        }
    } else if (state >= 0 && state < currlayer->algo->NumCellStates()) {
        lua_pushinteger(L, state);                   lua_rawseti(L, -2, 1);
        lua_pushinteger(L, currlayer->cellr[state]); lua_rawseti(L, -2, 2);
        lua_pushinteger(L, currlayer->cellg[state]); lua_rawseti(L, -2, 3);
        lua_pushinteger(L, currlayer->cellb[state]); lua_rawseti(L, -2, 4);
    } else {
        char msg[64];
        sprintf(msg, "getcolors error: bad state (%d)", state);
        GollyError(L, msg);
    }
    
    return 1;   // result is a table
}

// -----------------------------------------------------------------------------

static int g_overlaytable(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* result = NULL;
    int nresults = 0;

    // check the argument is a table
    luaL_checktype(L, 1, LUA_TTABLE);
    
    // get the size of the table
    int n = (int)lua_rawlen(L, 1);

    // check if the table contains any elements
    if (n > 0) {
        // get the command name
        lua_rawgeti(L, 1, 1);
        const char* cmd = lua_tostring(L, -1);
        lua_pop(L, 1);

        // check the command name was a string
        if (cmd) {
            result = curroverlay->DoOverlayTable(cmd, L, n, &nresults);
        } else {
            result = "ERR:ovtable command name must be a string";
        }
    } else {
        result = "ERR:missing ovtable command";
    }

    if (result == NULL) return nresults;   // no error so return any results
    
    if (result[0] == 'E' && result[1] == 'R' && result[2] == 'R' ) {
        std::string msg = "ovtable error: ";
        msg += result + 4;  // skip past "ERR:"
        GollyError(L, msg.c_str());
    }
    
    return 0;  // error so return nothing
}

// -----------------------------------------------------------------------------

static int g_overlay(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* cmd = luaL_checkstring(L, 1);
    const char* result;
    
    result = curroverlay->DoOverlayCommand(cmd);
    
    if (result == NULL) return 0;   // no error and no result
    
    if (result[0] == 'E' && result[1] == 'R' && result[2] == 'R' ) {
        std::string msg = "overlay error: ";
        msg += result + 4;  // skip past "ERR:"
        GollyError(L, msg.c_str());
    }
    
    lua_pushstring(L, result);
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_os(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    lua_pushstring(L, GSF_os());
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_millisecs(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    #if wxCHECK_VERSION(2,9,3)
        wxLongLong t = stopwatch->TimeInMicro();
        double d = t.ToDouble() / 1000.0L;
        lua_pushnumber(L, (lua_Number) d);
    #else
        lua_pushnumber(L, (lua_Number) stopwatch->Time());
    #endif
    
    return 1;   // result is a floating point number
}

// -----------------------------------------------------------------------------

static int g_sleep(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    int ms = luaL_checkinteger(L, 1);
    if (ms <= 0) {
        // do nothing (ensures g.sleep(0) is a no-op)
    } else if (ms <= 100) {
        wxMilliSleep(ms);
    } else {
        // do event checking every 100ms (lets user abort sleep if ms value is very large)
        while (ms > 100) {
            CheckEvents(L);
            wxMilliSleep(100);
            ms -= 100;
        }
        // ms is > 0 and <= 100
        wxMilliSleep(ms);
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_sound(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    #ifdef ENABLE_SOUND
        // check for sound
        int numargs = lua_gettop(L);
        if (numargs == 0) {
            if (GSF_SoundEnabled()) {
                // sound is enabled and initialized
                lua_pushinteger(L, 2);
                return 1;
            } else {
                // failed to initialize sound
                lua_pushinteger(L, 1);
                return 1;
            }
        }
        
        // numargs should be 1, 2 or 3
        const char* cmd = luaL_checkstring(L, 1);
        const char* soundfile = "";
        float volume = 1.0;
        if (numargs > 1) soundfile = luaL_checkstring(L, 2);
        if (numargs > 2) volume = luaL_checknumber(L, 3);
        
        // check which sound command is specified
        const char* result = NULL;
        if        (strcmp(cmd, "play") == 0) {      result = GSF_SoundPlay(soundfile, volume, false);
        } else if (strcmp(cmd, "loop") == 0) {      result = GSF_SoundPlay(soundfile, volume, true);
        } else if (strcmp(cmd, "stop") == 0) {      result = GSF_SoundStop(soundfile);
        } else if (strcmp(cmd, "state") == 0) {     result = GSF_SoundState(soundfile);
        } else if (strcmp(cmd, "volume") == 0) {    result = GSF_SoundVolume(soundfile, volume);
        } else if (strcmp(cmd, "pause") == 0) {     result = GSF_SoundPause(soundfile);
        } else if (strcmp(cmd, "resume") == 0) {    result = GSF_SoundResume(soundfile);
        } else {
            GollyError(L, "unknown sound command");
        }
        
        if (result == NULL) {
            return 0;
        } else {
            // check if GSF_SoundPlay etc returned an error msg
            if (result[0] == 'E' && result[1] == 'R' && result[2] == 'R' ) {
                std::string msg = result + 4;  // skip past "ERR:"
                GollyError(L, msg.c_str());
            }
            lua_pushstring(L, result);      // should be from GSF_SoundState
            return 1;
        }
    #else
        // sound support is not enabled
        lua_pushinteger(L, 0);
        return 1;
    #endif
}

// -----------------------------------------------------------------------------

static int g_setoption(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* optname = luaL_checkstring(L, 1);
    int newval = luaL_checkinteger(L, 2);
    int oldval;
    
    if (!GSF_setoption(optname, newval, &oldval)) {
        GollyError(L, "setoption error: unknown option.");
    }
    
    // return old value (simplifies saving and restoring settings)
    lua_pushinteger(L, oldval);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_getoption(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* optname = luaL_checkstring(L, 1);

    int optval;
    if (!GSF_getoption(optname, &optval)) {
        GollyError(L, "getoption error: unknown option.");
    }
    
    lua_pushinteger(L, optval);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_setcolor(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* colname = luaL_checkstring(L, 1);
    int r = luaL_checkinteger(L, 2);
    int g = luaL_checkinteger(L, 3);
    int b = luaL_checkinteger(L, 4);
    
    CHECK_RGB(r, g, b, "setcolor");
    
    wxColor newcol(r, g, b);
    wxColor oldcol;
    
    if (!GSF_setcolor(colname, newcol, oldcol)) {
        GollyError(L, "setcolor error: unknown color.");
    }
    
    // return old r,g,b values (simplifies saving and restoring colors)
    lua_pushinteger(L, oldcol.Red());
    lua_pushinteger(L, oldcol.Green());
    lua_pushinteger(L, oldcol.Blue());

    return 3;   // result is 3 ints
}

// -----------------------------------------------------------------------------

static int g_getcolor(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* colname = luaL_checkstring(L, 1);

    wxColor color;
    if (!GSF_getcolor(colname, color)) {
        GollyError(L, "getcolor error: unknown color.");
    }
    
    // return r,g,b values
    lua_pushinteger(L, color.Red());
    lua_pushinteger(L, color.Green());
    lua_pushinteger(L, color.Blue());

    return 3;   // result is 3 ints
}

// -----------------------------------------------------------------------------

static int g_query(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    wxString query = wxString(luaL_checkstring(L, 1), LUA_ENC);
    wxString msg = wxString(luaL_checkstring(L, 2), LUA_ENC);
    
    wxString labelYes    = _("Yes");
    wxString labelNo     = _("No");
    wxString labelCancel = _("Cancel");
    if (lua_gettop(L) > 2) labelYes    = wxString(luaL_checkstring(L, 3), LUA_ENC);
    if (lua_gettop(L) > 3) labelNo     = wxString(luaL_checkstring(L, 4), LUA_ENC);
    if (lua_gettop(L) > 4) labelCancel = wxString(luaL_checkstring(L, 5), LUA_ENC);
    
    long style = wxICON_QUESTION | wxCENTER | wxYES_NO;
    if (labelCancel.length() > 0) style = style | wxCANCEL; // add Cancel button
    
    wxMessageDialog dialog(wxGetActiveWindow(), msg, query, style);
    
    if (labelCancel.length() > 0) {
        dialog.SetYesNoCancelLabels(labelYes, labelNo, labelCancel);
    } else {
        dialog.SetYesNoLabels(labelYes, labelNo);
    }
    
    wxString label;
    bool finished = false;
    while (!finished) {
        finished = true;
        int button = dialog.ShowModal();
        if (viewptr) viewptr->ResetMouseDown();
        switch (button) {
            case wxID_YES: {
                label = dialog.GetYesLabel();
                break;
            }
            case wxID_NO: {
                label = dialog.GetNoLabel();
                break;
            }
            case wxID_CANCEL: {
                label = dialog.GetCancelLabel();
                if (labelCancel.length() == 0 && label == wxString("gtk-cancel")) {
                    // this happens on Linux if the dialog has no Cancel button
                    // but the user closed it by hitting escape, so we show the
                    // dialog again to force them to select one of the 2 buttons
                    finished = false;
                }
                break;
            }
            default: {
                // should never happen
                GollyError(L, "query bug: unexpected button.");
            }
        }
    }
    lua_pushstring(L, (const char*)label.mb_str(LUA_ENC));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_savechanges(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    wxString query = wxString(luaL_checkstring(L, 1), LUA_ENC);
    wxString msg = wxString(luaL_checkstring(L, 2), LUA_ENC);

    int answer = SaveChanges(query, msg);
    switch (answer) {
        case 2: {
            // user selected Yes (or Save on Mac)
            lua_pushstring(L, "yes");
            break;
        }
        case 1: {
            // user selected No (or Don't Save on Mac)
            lua_pushstring(L, "no");
            break;
        }
        default: {
            // answer == 0 (user selected Cancel)
            lua_pushstring(L, "cancel");
        }
    }
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_settitle(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    // set scripttitle to avoid MainFrame::SetWindowTitle changing title
    scripttitle = wxString(luaL_checkstring(L, 1), LUA_ENC);
    mainptr->SetTitle(scripttitle);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_setclipstr(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    mainptr->CopyTextToClipboard(wxString(luaL_checkstring(L, 1), LUA_ENC));
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getclipstr(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    wxTextDataObject data;
    if (!mainptr->GetTextFromClipboard(&data)) {
        data.SetText(wxEmptyString);
    }
    
    wxString clipstr = data.GetText();
    lua_pushstring(L, (const char*)clipstr.mb_str(LUA_ENC));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getstring(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* prompt = luaL_checkstring(L, 1);
    const char* initial = lua_gettop(L) > 1 ? luaL_checkstring(L, 2) : "";
    const char* title = lua_gettop(L) > 2 ? luaL_checkstring(L, 3) : "";
    
    wxString result;
    if (!GetString(wxString(title, LUA_ENC),
                   wxString(prompt, LUA_ENC),
                   wxString(initial, LUA_ENC),
                   result)) {
        // user hit Cancel button so abort script
        lua_pushstring(L, abortmsg);
        lua_error(L);
    }
    
    lua_pushstring(L, (const char*)result.mb_str(LUA_ENC));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getxy(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    statusptr->CheckMouseLocation(mainptr->infront);   // sets mousepos

    if (viewptr->showcontrols) mousepos = wxEmptyString;
    
    lua_pushstring(L, (const char*)mousepos.mb_str(LUA_ENC));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getevent(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    bool get = true;
    if (lua_gettop(L) > 0) get = CheckBoolean(L, 1);
    
    wxString event;
    GSF_getevent(event, get ? 1 : 0);
    
    lua_pushstring(L, (const char*)event.mb_str(LUA_ENC));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_doevent(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);

    const char* event = luaL_checkstring(L, 1);
    
    if (event[0]) {
        const char* err = GSF_doevent(wxString(event, LUA_ENC));
        if (err) GollyError(L, err);
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_show(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    // do NOT call luaL_checkstring when inscript is false
    // (chaos will ensue if an error is detected)
    const char* msg = luaL_checkstring(L, 1);

    // make sure status bar is visible
    if (!showstatus) mainptr->ToggleStatusBar();

    inscript = false;
    statusptr->DisplayMessage(wxString(msg, LUA_ENC));
    inscript = true;

    #if defined(__WXGTK__)
        // need to see update immediately
        insideYield++;
        wxGetApp().Yield(true);
        insideYield--;
    #endif

    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_error(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    // do NOT call luaL_checkstring when inscript is false
    // (chaos will ensue if an error is detected)
    const char* msg = luaL_checkstring(L, 1);
    
    inscript = false;
    statusptr->ErrorMessage(wxString(msg, LUA_ENC));
    inscript = true;
    // make sure status bar is visible
    if (!showstatus) mainptr->ToggleStatusBar();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_warn(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* msg = luaL_checkstring(L, 1);
    bool showCancel = true;
    if (lua_gettop(L) > 1) showCancel = CheckBoolean(L, 2);
        
    Warning(wxString(msg, LUA_ENC), showCancel);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_note(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    const char* msg = luaL_checkstring(L, 1);
    bool showCancel = true;
    if (lua_gettop(L) > 1) showCancel = CheckBoolean(L, 2);
    
    Note(wxString(msg, LUA_ENC), showCancel);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_help(lua_State* L)
{
    AUTORELEASE_POOL
    CheckEvents(L);
    
    ShowHelp(wxString(luaL_checkstring(L, 1), LUA_ENC));
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_check(lua_State* L)
{
    // no need for AUTORELEASE_POOL here
    // don't call CheckEvents here otherwise we can't safely write code like
    //    if g.getlayer() == target then
    //       g.check(false)
    //       ... do stuff to target layer ...
    //       g.check(true)
    
    allowcheck = CheckBoolean(L, 1);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_continue(lua_State* L)
{
    // do NOT call AUTORELEASE_POOL or CheckEvents here

	aborted = false;	// continue executing any remaining code

	// if not empty, error will be displayed when script ends
	scripterr = wxString(luaL_checkstring(L, 1), LUA_ENC);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_exit(lua_State* L)
{
    // script will terminate so no point calling AUTORELEASE_POOL or CheckEvents here

    if (lua_gettop(L) == 0) {
        GSF_exit(wxEmptyString);
    } else {
        GSF_exit(wxString(luaL_checkstring(L, 1), LUA_ENC));
    }
    
    lua_pushstring(L, abortmsg);
    lua_error(L);
    
    return 0;   // never get here (lua_error does a longjmp)
}

// -----------------------------------------------------------------------------

static const struct luaL_Reg gollyfuncs [] = {
    // filing
    { "open",         g_open },         // open given pattern/script/rule/html file
    { "save",         g_save },         // save pattern in given file using given format
    { "opendialog",   g_opendialog },   // return input path and filename chosen by user
    { "savedialog",   g_savedialog },   // return output path and filename chosen by user
    { "load",         g_load },         // read pattern file and return cell array
    { "store",        g_store },        // write cell array to a file (in RLE format)
    { "setdir",       g_setdir },       // set location of specified directory
    { "getdir",       g_getdir },       // return location of specified directory
    { "getfiles",     g_getfiles },     // return array of files in specified directory
    { "getpath",      g_getpath },      // return the path to the current opened pattern
    { "getinfo",      g_getinfo },      // return comments from pattern file
    { "geturl",       g_geturl },       // download given url and save in given path
    // editing
    { "new",          g_new },          // create new universe and set window title
    { "cut",          g_cut },          // cut selection to clipboard
    { "copy",         g_copy },         // copy selection to clipboard
    { "clear",        g_clear },        // clear inside/outside selection
    { "paste",        g_paste },        // paste clipboard pattern at x,y using given mode
    { "shrink",       g_shrink },       // shrink selection
    { "randfill",     g_randfill },     // randomly fill selection to given percentage
    { "flip",         g_flip },         // flip selection top-bottom or left-right
    { "rotate",       g_rotate },       // rotate selection 90 deg clockwise or anticlockwise
    { "parse",        g_parse },        // parse RLE or Life 1.05 string and return cell array
    { "transform",    g_transform },    // apply an affine transformation to cell array
    { "evolve",       g_evolve },       // generate pattern contained in given cell array
    { "putcells",     g_putcells },     // paste given cell array into current universe
    { "getcells",     g_getcells },     // return cell array in given rectangle
    // { "getcells2",     g_getcells2 },     // experimental version (needs more thought!!!)
    { "join",         g_join },         // return concatenation of given cell arrays
    { "hash",         g_hash },         // return hash value for pattern in given rectangle
    { "getclip",      g_getclip },      // return pattern in clipboard (as wd, ht, cell array)
    { "select",       g_select },       // select {x, y, wd, ht} rectangle or remove if {}
    { "getrect",      g_getrect },      // return pattern rectangle as {} or {x, y, wd, ht}
    { "getselrect",   g_getselrect },   // return selection rectangle as {} or {x, y, wd, ht}
    { "setcell",      g_setcell },      // set given cell to given state
    { "getcell",      g_getcell },      // get state of given cell
    { "setcursor",    g_setcursor },    // set cursor (returns old cursor)
    { "getcursor",    g_getcursor },    // return current cursor
    // control
    { "empty",        g_empty },        // return true if universe is empty
    { "run",          g_run },          // run current pattern for given number of gens
    { "step",         g_step },         // run current pattern for current step
    { "setstep",      g_setstep },      // set step exponent
    { "getstep",      g_getstep },      // return current step exponent
    { "setbase",      g_setbase },      // set base step
    { "getbase",      g_getbase },      // return current base step
    { "advance",      g_advance },      // advance inside/outside selection by given gens
    { "reset",        g_reset },        // restore starting pattern
    { "setgen",       g_setgen },       // set current generation to given string
    { "getgen",       g_getgen },       // return current generation as string
    { "getpop",       g_getpop },       // return current population as string
    { "numstates",    g_numstates },    // return number of cell states in current universe
    { "numalgos",     g_numalgos },     // return number of algorithms
    { "setalgo",      g_setalgo },      // set current algorithm using given string
    { "getalgo",      g_getalgo },      // return name of given or current algorithm
    { "setrule",      g_setrule },      // set current rule using given string
    { "getrule",      g_getrule },      // return current rule
    { "getwidth",     g_getwidth },     // return width of universe (0 if unbounded)
    { "getheight",    g_getheight },    // return height of universe (0 if unbounded)
    { "getgridtype",  g_getgridtype },  // return grid type ("Square", "Triangular", "Hexagonal" or "von Neumann")
    // viewing
    { "setpos",       g_setpos },       // move given cell to middle of viewport
    { "getpos",       g_getpos },       // return x,y position of cell in middle of viewport
    { "setmag",       g_setmag },       // set magnification (0=1:1, 1=1:2, -1=2:1, etc)
    { "getmag",       g_getmag },       // return current magnification
    { "getscale",     g_getscale},      // return screen's scale factor (normally 1.0, but 2.0 for Retina screen)
    { "fit",          g_fit },          // fit entire pattern in viewport
    { "fitsel",       g_fitsel },       // fit selection in viewport
    { "visrect",      g_visrect },      // return true if given rect is completely visible
    { "setview",      g_setview },      // set pixel dimensions of viewport
    { "getview",      g_getview },      // get pixel dimensions of viewport
    { "update",       g_update },       // update display (viewport and status bar)
    { "autoupdate",   g_autoupdate },   // update display after each change to universe?
    // layers
    { "addlayer",     g_addlayer },     // add a new layer
    { "clone",        g_clone },        // add a cloned layer (shares universe)
    { "duplicate",    g_duplicate },    // add a duplicate layer (copies universe)
    { "dellayer",     g_dellayer },     // delete current layer
    { "movelayer",    g_movelayer },    // move given layer to new index
    { "setlayer",     g_setlayer },     // switch to given layer
    { "getlayer",     g_getlayer },     // return index of current layer
    { "numlayers",    g_numlayers },    // return current number of layers
    { "maxlayers",    g_maxlayers },    // return maximum number of layers
    { "setname",      g_setname },      // set name of given layer
    { "getname",      g_getname },      // get name of given layer
    { "setcolors",    g_setcolors },    // set color(s) used in current layer
    { "getcolors",    g_getcolors },    // get color(s) used in current layer
    { "overlay",      g_overlay },      // do an overlay command from a string
    { "ovtable",      g_overlaytable }, // do an overlay command from a table
    // miscellaneous
    { "os",           g_os },           // return the current OS (Windows/Mac/Linux)
    { "millisecs",    g_millisecs },    // return elapsed time since Golly started, in millisecs
    { "sleep",        g_sleep },        // sleep for the given number of millisecs
    { "sound",        g_sound },        // control playing of audio
    { "query",        g_query },        // show a query dialog and return answer
    { "savechanges",  g_savechanges },  // show standard save changes dialog and return answer
    { "settitle",     g_settitle },     // set window title to given string
    { "setoption",    g_setoption },    // set given option to new value (and return old value)
    { "getoption",    g_getoption },    // return current value of given option
    { "setcolor",     g_setcolor },     // set given color to new r,g,b (returns old r,g,b)
    { "getcolor",     g_getcolor },     // return r,g,b values of given color
    { "setclipstr",   g_setclipstr },   // set the clipboard contents to a given string value
    { "getclipstr",   g_getclipstr },   // retrieve the contents of the clipboard as a string
    { "getstring",    g_getstring },    // display dialog box and get string from user
    { "getxy",        g_getxy },        // return current grid location of mouse
    { "getevent",     g_getevent },     // return keyboard/mouse event or empty string if none
    { "doevent",      g_doevent },      // pass given keyboard/mouse event to Golly to handle
    { "show",         g_show },         // show given string in status bar
    { "error",        g_error },        // beep and show given string in status bar
    { "warn",         g_warn },         // show given string in warning dialog
    { "note",         g_note },         // show given string in note dialog
    { "help",         g_help },         // show given HTML file in help window
    { "check",        g_check },        // allow event checking?
    { "continue",     g_continue },     // continue executing code after a pcall error
    { "exit",         g_exit },         // exit script with optional error message
    {NULL, NULL}
};

// -----------------------------------------------------------------------------

static int create_golly_table(lua_State* L)
{
    // create a table with our g_* functions and register them
    luaL_newlib(L, gollyfuncs);
    return 1;
}

// -----------------------------------------------------------------------------

// we want to allow a Lua script to call another Lua script via g_open
// so we use lua_level to detect if a Lua state has already been created
static int lua_level = 0;
static lua_State* L = NULL;

void RunLuaScript(const wxString& filepath)
{
    if (lua_level == 0) {
        aborted = false;
        
        L = luaL_newstate();
        
        luaL_openlibs(L);
    
        // we want our g_* functions to be called from Lua as g.*
        lua_pushcfunction(L, create_golly_table); lua_setglobal(L, "golly");
    
        // It would be nice if we could do this now:
        //
        // luaL_dostring(L, "local g = golly()");
        //
        // But it doesn't work because g goes out of scope, so users
        // will have to start their scripts with that line.
        // Note that we could do this:
        //
        // luaL_dostring(L, "g = golly()");
        //
        // But it's ~10% slower to access functions because g is global.
        
        // append gollydir/Scripts/Lua/?.lua and gollydir/Scripts/Lua/?/init.lua
        // to package.path so scripts can do things like this:
        // local gp = require "gplus"
        // local gpt = require "gplus.text"  ('.' will be changed to path separator)
        wxString luadir = gollydir;
        luadir += wxT("Scripts");
        luadir += wxFILE_SEP_PATH;
        luadir += wxT("Lua");
        luadir += wxFILE_SEP_PATH;
        wxString pstring = wxT("package.path = package.path..';");
        pstring += luadir;
        pstring += wxT("?.lua;");
        pstring += luadir;
        pstring += wxT("?");
        pstring += wxFILE_SEP_PATH;
        pstring += wxT("init.lua'");
        
        // convert any backslashes to "\\" to avoid "\" being treated as escape char
        // (definitely necessary on Windows because wxFILE_SEP_PATH is a backslash)
        pstring.Replace(wxT("\\"), wxT("\\\\"));

        luaL_dostring(L, (const char*)pstring.mb_str(LUA_ENC));
    }
    
    lua_level++;
    
    // don't use wxConvUTF8 in next line because caller has already converted
    // filepath to decomposed UTF8 if on a Mac
    if (luaL_dofile(L, (const char*)filepath.mb_str(wxConvLocal))) {
        scripterr += wxString(lua_tostring(L,-1), LUA_ENC);
        scripterr += wxT("\n");
        // scripterr is checked at the end of RunScript in wxscript.cpp
        lua_pop(L,1);
    }
    
    lua_level--;
    
    if (lua_level == 0) {
        lua_close(L);
    }
}

// -----------------------------------------------------------------------------

void AbortLuaScript()
{
    // calling lua_error here causes nasty problems (presumably due to doing
    // a longjmp from inside Yield) so we set the aborted flag and check it
    // in CheckEvents only after Yield has finished
    
    aborted = true;
}

// -----------------------------------------------------------------------------

void FinishLuaScripting()
{
    // no need to do anything
}

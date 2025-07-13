// This file is part of Golly.
// See docs/License.html for the copyright notice.

// gollybase
#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "generationsalgo.h"
#include "ltlalgo.h"
#include "jvnalgo.h"
#include "superalgo.h"
#include "ruleloaderalgo.h"

// gui-common
#include "algos.h"      // for InitAlgorithms, NumAlgos, etc
#include "prefs.h"      // for GetPrefs, MAX_MAG, tempdir, etc
#include "layer.h"      // for ResizeLayers, currlayer
#include "control.h"    // for SetMinimumStepExponent, NextGeneration, etc
#include "file.h"       // for NewPattern
#include "view.h"       // for fullscreen, TouchBegan, etc
#include "status.h"     // for SetMessage, CheckMouseLocation, etc
#include "utils.h"      // for Beep, etc
#include "undo.h"       // for currlayer->undoredo->...
#include "render.h"     // for InitOGLES2, DrawPattern

#include "webcalls.h"   // for refresh_pattern, UpdateStatus, PatternSaved, etc

#include <stdlib.h>
#include <stdio.h>
#include <GL/glfw.h>
#include <emscripten/emscripten.h>

// -----------------------------------------------------------------------------

// the following JavaScript functions are implemented in jslib.js:

extern "C" {
    extern void jsSetMode(int index);
    extern const char* jsSetRule(const char* oldrule);
    extern void jsShowMenu(const char* id, int x, int y);
    extern int jsTextAreaIsActive();
    extern int jsElementIsVisible(const char* id);
    extern void jsEnableImgButton(const char* id, bool enable);
    extern void jsTickMenuItem(const char* id, bool tick);
    extern void jsSetInputValue(const char* id, int num);
    extern int jsGetInputValue(const char* id);
    extern void jsSetCheckBox(const char* id, bool flag);
    extern bool jsGetCheckBox(const char* id);
    extern void jsSetInnerHTML(const char* id, const char* text);
    extern void jsShowSaveDialog(const char* filename, const char* extensions);
    extern void jsSaveFile(const char* filename);
}

// -----------------------------------------------------------------------------

static int currwd, currht;              // current size of viewport (in pixels)
static double last_time;                // time when NextGeneration was last called

static bool alt_down = false;           // alt/option key is currently pressed?
static bool ctrl_down = false;          // ctrl key is currently pressed?
static bool shift_down = false;         // shift key is currently pressed?
static bool meta_down = false;          // cmd/start/menu key is currently pressed?

static bool ok_to_check_mouse = false;  // ok to call glfwGetMousePos?
static bool mouse_down = false;         // is mouse button down?
static bool over_canvas = false;        // is mouse over canvas?
static bool over_paste = false;         // is mouse over paste image?

static bool paste_menu_visible = false;
static bool selection_menu_visible = false;

// -----------------------------------------------------------------------------

static void InitPaths()
{
    userdir = "";       // webGolly doesn't use this

    // webGolly doesn't use savedir (jsSaveFile will download saved files to the user's computer
    // in a directory specified by the browser)
    savedir = "";

    // webGolly doesn't need to set downloaddir (browser will cache downloaded files)
    downloaddir = "";

    // create a directory for user's rules
    EM_ASM( FS.mkdir('/LocalRules'); );
    userrules = "/LocalRules/";              // WARNING: GetLocalPrefs() assumes this string
    
    // !!! we'll need to provide a way for users to delete .rule files in localStorage
    // to avoid exceeding the localStorage disk quota (implement special "DELETE" links
    // in Set Rule dialog, like we do in iGolly and aGolly)

    // supplied patterns, rules, help are stored in golly.data via --preload-file option in Makefile
    supplieddir = "/";
    patternsdir = supplieddir + "Patterns/";
    rulesdir = supplieddir + "Rules/";
    helpdir = supplieddir + "Help/";
    
    // create a directory for all our temporary files (can't use /tmp as it already exists!)
    EM_ASM( FS.mkdir('/gollytmp'); );
    tempdir = "/gollytmp/";
    clipfile = tempdir + "golly_clipboard";
    
    // WARNING: GetLocalPrefs() and SaveLocalPrefs() assume the user's preferences are
    // temporarily stored in a file with this name
    prefsfile = "GollyPrefs";
}

// -----------------------------------------------------------------------------

static void GetLocalPrefs()
{
    EM_ASM(
        // get GollyPrefs string from local storage (key name must match setItem call)
        var contents = localStorage.getItem('GollyPrefs');
        if (contents) {
            // write contents to virtual file system so GetPrefs() can read it
            // and initialize various global settings
            FS.writeFile('GollyPrefs', contents, {encoding:'utf8'});
        }
    );
    GetPrefs();     // read prefsfile from virtual file system
    
    EM_ASM(
        // re-create any .rule files that were saved in localStorage
        for (var i=0; i<localStorage.length; i++) {
            var key = localStorage.key(i);
            if (key.substr(0,12) == '/LocalRules/') {
                // key is full path name (eg. /LocalRules/Foo.rule)
                var contents = localStorage.getItem(key);
                if (contents) FS.writeFile(key, contents, {encoding:'utf8'});
            }
        }
    );
}

// -----------------------------------------------------------------------------

extern "C" {

void SaveLocalPrefs()
{
    SavePrefs();    // write prefsfile to virtual file system
    EM_ASM(
        // read contents of prefsfile just written
        var contents = FS.readFile('GollyPrefs', {encoding:'utf8'});
        // save contents to local storage (key name must match getItem call)
        localStorage.setItem('GollyPrefs', contents);
    );
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

bool UnsavedChanges()
{
    return (currlayer && currlayer->dirty && asktosave);
}

} // extern "C"

// -----------------------------------------------------------------------------

static int prevwheel = 0;

extern "C" {

void OnMouseWheel(int pos)
{
    int x, y;
    glfwGetMousePos(&x, &y);
    
    // we use wheelsens in below tests to reduce sensitivity
    if (pos + wheelsens < prevwheel) {
        ZoomInPos(x, y);
        prevwheel = pos;
    } else if (pos - wheelsens > prevwheel) {
        ZoomOutPos(x, y);
        prevwheel = pos;
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

static void InitEventHandlers()
{
    EM_ASM(
        var wheelpos = 0;
        function on_mouse_wheel(event) {
            // set delta to a value between -1 and 1
            // (old versions of Firefox use event.deltaY)
            var delta = Math.max(-1, Math.min(1, (event.detail || -event.wheelDelta || -event.deltaY)));
            wheelpos += delta;
            _OnMouseWheel(wheelpos);
            return false;
        };
        // for old versions of Firefox:
        Module['canvas'].addEventListener('DOMMouseScroll', on_mouse_wheel, false);
        // for all(?) modern browsers:
        Module['canvas'].addEventListener('wheel', on_mouse_wheel, false);
    );
    
    EM_ASM(
        // we do our own keyboard event handling because glfw's onKeyChanged always calls
        // event.preventDefault() so browser shortcuts like ctrl-Q/X/C/V don't work and
        // text can't be typed into our clipboard textarea
        function on_key_changed(event, status) {
            var key = event.keyCode;
            // DEBUG: Module.printErr('keycode='+key+' status='+status);
            // DEBUG: Module.printErr('activeElement='+document.activeElement.tagName);

            var prevent = _OnKeyChanged(key, status);
            // we allow default handler in these 2 cases:
            // 1. if ctrl/meta key is down (allows cmd/ctrl-Q/X/C/V/A/etc to work)
            // 2. if a textarea is active (document.activeElement.tagName == 'TEXTAREA')
            
            // on Firefox it seems we NEED to call preventDefault to stop cursor disappearing,
            // but on Safari/Chrome if we call it then cursor disappears (and arrow keys ALWAYS
            // cause disappearance regardless of whether we call it or not)... sheesh!!!
            
            if (prevent) {
                event.preventDefault();
                return false;
            }
        };
        function on_key_down(event) {
            on_key_changed(event, 1);   // GLFW_PRESS
        };
        function on_key_up(event) {
            on_key_changed(event, 0);   // GLFW_RELEASE
        };
        window.addEventListener('keydown', on_key_down, false);
        window.addEventListener('keyup', on_key_up, false);
    );
    
    EM_ASM(
        // detect exit from full screen mode
        document.addEventListener('fullscreenchange', function() {
            if (!document.fullscreenElement) _ExitFullScreen();
        }, false);
        document.addEventListener('msfullscreenchange', function() {
            if (!document.msFullscreenElement) _ExitFullScreen();
        }, false);
        document.addEventListener('mozfullscreenchange', function() {
            if (!document.mozFullScreen) _ExitFullScreen();
        }, false);
        document.addEventListener('webkitfullscreenchange', function() {
            if (!document.webkitIsFullScreen) _ExitFullScreen();
        }, false);
    );

    EM_ASM(
        // give user the option to save any changes
        window.onbeforeunload = function() {
            if (_UnsavedChanges()) return 'You have unsaved changes.';
        };
        
        // save current settings when window is unloaded
        window.addEventListener('unload', function() {
            _SaveLocalPrefs();
        });
    );
}

// -----------------------------------------------------------------------------

static int InitGL()
{
    if (glfwInit() != GL_TRUE) {
        Warning("glfwInit failed!");
        return GL_FALSE;
    }
    InitEventHandlers();

    // initial size doesn't matter -- ResizeCanvas will soon change it
    if (glfwOpenWindow(100, 100, 8, 8, 8, 8, 0, 0, GLFW_WINDOW) != GL_TRUE) {
        Warning("glfwOpenWindow failed!");
        return GL_FALSE;
    }
    glfwSetWindowTitle("Golly");
    
    if (!InitOGLES2()) {
        Warning("InitOGLES2 failed!");
        return GL_FALSE;
    }

    // we only do 2D drawing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(1.0, 1.0, 1.0, 1.0);

    last_time = glfwGetTime();
    return GL_TRUE;
}

// -----------------------------------------------------------------------------

// many of the following routines are declared as C functions to avoid
// C++ name mangling and make it easy to call them from JavaScript code
// (the routine names, along with a leading underscore, must be listed in
// -s EXPORTED_FUNCTIONS in Makefile)

extern "C" {

void ResizeCanvas() {
    // resize canvas based on current window dimensions and fullscreen flag
    if (fullscreen) {
        EM_ASM(
            var wd = window.innerWidth;
            var ht = window.innerHeight;
            var canvas = Module['canvas'];
            canvas.style.top = '0px';
            canvas.style.left = '0px';
            canvas.style.width = wd + 'px';
            canvas.style.height = ht + 'px';
            canvas.width = wd;
            canvas.height = ht;
            _SetViewport(wd, ht);   // call C routine (see below)
        );
    } else {
        EM_ASM(
            var trect = document.getElementById('toolbar').getBoundingClientRect();
            // place canvas immediately under toolbar
            var top = trect.top + trect.height;
            var left = trect.left;
            var wd = window.innerWidth - left - 10;
            var ht = window.innerHeight - top - 10;
            if (wd < 0) wd = 0;
            if (ht < 0) ht = 0;
            var canvas = Module['canvas'];
            canvas.style.top = top + 'px';
            canvas.style.left = left + 'px';
            canvas.style.width = wd + 'px';
            canvas.style.height = ht + 'px';
            canvas.width = wd;
            canvas.height = ht;
            _SetViewport(wd, ht);   // call C routine (see below)
        );
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void SetViewport(int width, int height)
{
    // do NOT call glfwSetWindowSize as it cancels full screen mode
    // glfwSetWindowSize(width, height);
    
    // ResizeCanvas has changed canvas size so we need to change OpenGL's viewport size
    glViewport(0, 0, width, height);
    currwd = width;
    currht = height;
    if (currwd != currlayer->view->getwidth() ||
        currht != currlayer->view->getheight()) {
        // also change size of Golly's viewport
        ResizeLayers(currwd, currht);
        // to avoid seeing lots of black, draw now rather than call UpdatePattern
        DrawPattern(currindex);
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

static void InitClipboard()
{
    // initialize clipboard data to a simple RLE pattern
    EM_ASM(
        document.getElementById('cliptext').value =
            '# To open the following RLE pattern,\n'+
            '# select File > Open Clipboard.\n' +
            '# Or to paste in the pattern, click on\n'+
            '# the Paste button, drag the floating\n' +
            '# image to the desired location, then\n' +
            '# right-click on the image.\n' +
            'x = 9, y = 5, rule = B3/S23\n' +
            '$bo3b3o$b3o2bo$2bo!';
    );
}

// -----------------------------------------------------------------------------

static void UpdateCursorImage()
{
    // note that the 2 numbers after the cursor url are the x and y offsets of the
    // cursor's hotspot relative to the top left corner
    
    if (over_paste) {
        // user can drag paste image so show hand cursor
        EM_ASM( Module['canvas'].style.cursor = 'url(images/cursor_move.png) 7 7, auto'; );
        return;
    }
    
    if (!currlayer) return;     // in case this routine is called very early
    
    if (currlayer->touchmode == drawmode) {
        EM_ASM( Module['canvas'].style.cursor = 'url(images/cursor_draw.png) 4 15, auto'; );
    } else if (currlayer->touchmode == pickmode) {
        EM_ASM( Module['canvas'].style.cursor = 'url(images/cursor_pick.png) 0 15, auto'; );
    } else if (currlayer->touchmode == selectmode) {
        EM_ASM( Module['canvas'].style.cursor = 'crosshair'; );     // all browsers support this???
    } else if (currlayer->touchmode == movemode) {
        EM_ASM( Module['canvas'].style.cursor = 'url(images/cursor_move.png) 7 7, auto'; );
    } else if (currlayer->touchmode == zoominmode) {
        EM_ASM( Module['canvas'].style.cursor = 'url(images/cursor_zoomin.png) 6 6, auto'; );
    } else if (currlayer->touchmode == zoomoutmode) {
        EM_ASM( Module['canvas'].style.cursor = 'url(images/cursor_zoomout.png) 6 6, auto'; );
    } else {
        Warning("Bug detected in UpdateCursorImage!");
    }
}

// -----------------------------------------------------------------------------

static void CheckCursor(int x, int y)
{
    if (mouse_down) {
        // don't check for cursor change if mouse button is pressed
        // (eg. we don't want cursor to change if user drags pencil/crosshair
        // cursor over the paste image)
    } else if (waitingforpaste && PointInPasteImage(x, y) && !over_paste) {
        // change cursor to hand to indicate that paste image can be dragged
        over_paste = true;
        UpdateCursorImage();
    } else if (over_paste && (!waitingforpaste || !PointInPasteImage(x, y))) {
        // change cursor from hand to match currlayer->touchmode
        over_paste = false;
        UpdateCursorImage();
    }
}

// -----------------------------------------------------------------------------

static void StopIfGenerating()
{
    if (generating) {
        StopGenerating();
        // generating flag is now false so change button image
        EM_ASM( document.getElementById('imgstartStop').src = 'images/start.png'; );
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void NewUniverse()
{
    // undo/redo history is about to be cleared so no point calling RememberGenFinish
    // if we're generating a (possibly large) pattern
    bool saveundo = allowundo;
    allowundo = false;
    StopIfGenerating();
    allowundo = saveundo;
    
    if (event_checker > 0) {
        // try again after a short delay
        EM_ASM( window.setTimeout('_NewUniverse()', 10); );
        return;
    }
    
    NewPattern();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

static void Open()
{
    StopIfGenerating();

    // display the open file dialog and start listening for change to selected file
    // (the on_file_change function is in shell.html)
    EM_ASM( 
        document.getElementById('open_overlay').style.visibility = 'visible';
        document.getElementById('upload_file').addEventListener('change', on_file_change, false);
    );
}

// -----------------------------------------------------------------------------

extern "C" {

void CancelOpen()
{
    // close the open file dialog and remove the change event handler
    EM_ASM(
        document.getElementById('open_overlay').style.visibility = 'hidden';
        document.getElementById('upload_file').removeEventListener('change', on_file_change, false);
    );
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void OpenClipboard()
{
    StopIfGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        EM_ASM( window.setTimeout('_OpenClipboard()', 10); );
        return;
    }
    
    // if clipboard text starts with "@RULE rulename" then install rulename.rule and switch to that rule
    if (ClipboardContainsRule()) return;
    
    // load and view pattern data stored in clipboard
    std::string data;
    if (GetTextFromClipboard(data)) {
        // copy clipboard data to tempstart so we can handle all formats supported by readpattern
        FILE* outfile = fopen(currlayer->tempstart.c_str(), "w");
        if (outfile) {
            if (fputs(data.c_str(), outfile) == EOF) {
                ErrorMessage("Could not write clipboard text to tempstart file!");
                fclose(outfile);
                return;
            }
        } else {
            ErrorMessage("Could not open tempstart file for writing!");
            fclose(outfile);
            return;
        }
        fclose(outfile);
        LoadPattern(currlayer->tempstart.c_str(), "clipboard");
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

static void Save()
{
    StopIfGenerating();

    // display the save pattern dialog
    if (currlayer->algo->hyperCapable()) {
        jsShowSaveDialog(currlayer->currname.c_str(), ".mc (default), .mc.gz, .rle, .rle.gz");
    } else {
        jsShowSaveDialog(currlayer->currname.c_str(), ".rle (default), .rle.gz");
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void CancelSave()
{
    // close the save pattern dialog
    EM_ASM( document.getElementById('save_overlay').style.visibility = 'hidden'; );
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void SaveFile(const char* filename)
{
    std::string fname = filename;
    // PatternSaved will append default extension if not supplied
    if (PatternSaved(fname)) {
        CancelSave();           // close dialog
        UpdateStatus();
        // fname successfully created, so download it to user's computer
        jsSaveFile(fname.c_str());
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void StartStop()
{
    if (generating) {
        StopGenerating();
        
        // generating flag is now false so change button image
        EM_ASM( document.getElementById('imgstartStop').src = 'images/start.png'; );
        
        // don't call UpdateButtons here because if event_checker is > 0
        // then StopGenerating hasn't called RememberGenFinish, and so CanUndo/CanRedo
        // might not return correct results
        bool canreset = currlayer->algo->getGeneration() > currlayer->startgen;
        jsEnableImgButton("reset", canreset);
        jsEnableImgButton("undo", allowundo && (canreset || currlayer->undoredo->CanUndo()));
        jsEnableImgButton("redo", false);

    } else if (StartGenerating()) {
        // generating flag is now true so change button image
        EM_ASM( document.getElementById('imgstartStop').src = 'images/stop.png'; );

        // don't call UpdateButtons here because we want user to
        // be able to stop generating by hitting reset/undo buttons
        jsEnableImgButton("reset", true);
        jsEnableImgButton("undo", allowundo);
        jsEnableImgButton("redo", false);
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Next()
{
    StopIfGenerating();
    if (event_checker > 0) {
        // previous NextGeneration() hasn't finished so try again after a short delay
        EM_ASM( window.setTimeout('_Next()', 10); );
        return;
    }

    NextGeneration(false);       // advance by 1
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Step()
{
    StopIfGenerating();
    if (event_checker > 0) {
        // previous NextGeneration() hasn't finished so try again after a short delay
        EM_ASM( window.setTimeout('_Step()', 10); );
        return;
    }

    NextGeneration(true);       // advance by current step size
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void GoSlower()
{
    if (currlayer->currexpo > minexpo) {
        currlayer->currexpo--;
        SetGenIncrement();
        UpdateStatus();
    } else {
        Beep();
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void GoFaster()
{
    currlayer->currexpo++;
    SetGenIncrement();
    UpdateStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void StepBy1()
{
    // reset step exponent to 0
    currlayer->currexpo = 0;
    SetGenIncrement();
    UpdateStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Reset()
{
    if (currlayer->algo->getGeneration() == currlayer->startgen) return;

    StopIfGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        EM_ASM( window.setTimeout('_Reset()', 10); );
        return;
    }
    
    ResetPattern();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Fit()
{
    // no need to call TestAutoFit()
    FitInView(1);
    UpdatePatternAndStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

void Middle()
{
    TestAutoFit();
    if (currlayer->originx == bigint::zero && currlayer->originy == bigint::zero) {
        currlayer->view->center();
    } else {
        // put cell saved by ChangeOrigin (not yet implemented!!!) in middle
        currlayer->view->setpositionmag(currlayer->originx, currlayer->originy, currlayer->view->getmag());
    }
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C" {

void ZoomOut()
{
    TestAutoFit();
    currlayer->view->unzoom();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ZoomIn()
{
    if (currlayer->view->getmag() < MAX_MAG) {
        TestAutoFit();
        currlayer->view->zoom();
        UpdateEverything();
    } else {
        Beep();
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Scale1to1()
{
    if (currlayer->view->getmag() != 0) {
        TestAutoFit();
        currlayer->view->setmag(0);
        UpdateEverything();
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void FullScreen()
{
    fullscreen = true;
    EM_ASM( 
        var canvas = Module['canvas'];
        if (canvas.requestFullscreen) {
            canvas.requestFullscreen();
        } else if (canvas.msRequestFullscreen) {
            canvas.msRequestFullscreen();
        } else if (canvas.mozRequestFullScreen) {
            canvas.mozRequestFullScreen();
        } else if (canvas.webkitRequestFullScreen) {
            canvas.webkitRequestFullScreen();
            // following only works on Chrome??? (but prevents Safari going full screen!!!)
            // canvas.webkitRequestFullScreen(Element.ALLOW_KEYBOARD_INPUT);
        }
    );
    // no need to call ResizeCanvas here (the resize event handler calls it)
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ExitFullScreen()
{
    fullscreen = false;
    ResizeCanvas();
}

} // extern "C"

// -----------------------------------------------------------------------------

static void SetScale(int mag)
{
    if (currlayer->view->getmag() != mag) {
        TestAutoFit();
        currlayer->view->setmag(mag);
        UpdateEverything();
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void Info()
{
    if (currlayer->currfile.length() > 0) {
        // display comments in the last loaded pattern file
        char *commptr = NULL;
        // readcomments will allocate commptr
        const char *err = readcomments(currlayer->currfile.c_str(), &commptr);
        if (err) {
            if (commptr) free(commptr);
            ErrorMessage(err);
            return;
        }

        const char* commfile = "comments";
        FILE* outfile = fopen(commfile, "w");
        if (outfile) {
            int result;
            if (commptr[0] == 0) {
                result = fputs("No comments found.", outfile);
            } else {
                result = fputs(commptr, outfile);
            }
            if (result == EOF) {
                ErrorMessage("Could not write comments to file!");
                fclose(outfile);
                return;
            }
        } else {
            ErrorMessage("Could not open file for comments!");
            fclose(outfile);
            return;
        }
        fclose(outfile);
        
        if (commptr) free(commptr);
        ShowTextFile(commfile);
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

void StopAndHelp(const char* helpfile)
{
    StopIfGenerating();
    ShowHelp(helpfile);
}

// -----------------------------------------------------------------------------

extern "C" {

void Help()
{
    // show most recently loaded help file (or contents page if no such file)
    StopAndHelp("");
}

} // extern "C"

// -----------------------------------------------------------------------------

static void OpenPrefs()
{
    // show a modal dialog that lets user change various settings
    EM_ASM( document.getElementById('prefs_overlay').style.visibility = 'visible'; );
    
    // show current settings
    jsSetInputValue("random_fill", randomfill);
    jsSetInputValue("max_hash", maxhashmem);
    jsSetCheckBox("ask_to_save", asktosave);
    jsSetCheckBox("toggle_beep", allowbeep);
    jsSetInputValue("wheel_sensitivity", wheelsens);
    
    // select random fill percentage (the most likely setting to change)
    EM_ASM(
        var randfill = document.getElementById('random_fill');
        randfill.select();
        randfill.focus();
    );
}

// -----------------------------------------------------------------------------

extern "C" {

void CancelPrefs()
{
    // ignore any changes to the current settings and close the prefs dialog
    EM_ASM( document.getElementById('prefs_overlay').style.visibility = 'hidden'; );
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void StorePrefs()
{
    // get and validate settings
    int newrandfill = jsGetInputValue("random_fill");
    if (newrandfill < 1 || newrandfill > 100) {
        Warning("Random fill percentage must be an integer from 1 to 100.");
        return;
    }
    
    int newmaxhash = jsGetInputValue("max_hash");
    if (newmaxhash < MIN_MEM_MB || newmaxhash > MAX_MEM_MB) {
        char msg[128];
        sprintf(msg, "Maximum hash memory must be an integer from %d to %d.", MIN_MEM_MB, MAX_MEM_MB);
        Warning(msg);
        return;
    }
    
    int newwheelsens = jsGetInputValue("wheel_sensitivity");
    if (newwheelsens < 1 || newwheelsens > MAX_SENSITIVITY) {
        char msg[128];
        sprintf(msg, "Mouse wheel sensitivity must be an integer from %d to %d.", 1, MAX_SENSITIVITY);
        Warning(msg);
        return;
    }

    // all settings are valid
    randomfill = newrandfill;
    wheelsens = newwheelsens;
    if (maxhashmem != newmaxhash) {
        // need to call setMaxMemory if maxhashmem changed
        maxhashmem = newmaxhash;
        for (int i = 0; i < numlayers; i++) {
            Layer* layer = GetLayer(i);
            if (algoinfo[layer->algtype]->canhash) {
                layer->algo->setMaxMemory(maxhashmem);
            }
            // non-hashing algos (QuickLife and LtL) use their default memory setting
        }
    }

    asktosave = jsGetCheckBox("ask_to_save");
    allowbeep = jsGetCheckBox("toggle_beep");
    
    CancelPrefs();      // close the prefs dialog
    SaveLocalPrefs();   // remember settings in local storage
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Paste()
{
    StopIfGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        EM_ASM( window.setTimeout('_Paste()', 10); );
        return;
    }
    
    // remove any existing paste image
    if (waitingforpaste) AbortPaste();

    PasteClipboard();
    UpdatePatternAndStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

static void CancelPaste()
{
    if (waitingforpaste) {
        AbortPaste();
        UpdatePattern();
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void Undo()
{
    StopIfGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        EM_ASM( window.setTimeout('_Undo()', 10); );
        return;
    }
    
    currlayer->undoredo->UndoChange();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Redo()
{
    StopIfGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        EM_ASM( window.setTimeout('_Redo()', 10); );
        return;
    }
    
    currlayer->undoredo->RedoChange();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ToggleIcons()
{
    showicons = !showicons;
    UpdateEditBar();    // updates check box
    UpdatePattern();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ToggleAutoFit()
{
    currlayer->autofit = !currlayer->autofit;
    UpdateEditBar();    // updates check box

    // we only use autofit when generating
    if (generating && currlayer->autofit) {
        FitInView(0);
        UpdateEverything();
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

static void ToggleGrid()
{
    showgridlines = !showgridlines;
    UpdatePattern();
}

// -----------------------------------------------------------------------------

static void ToggleHashInfo()
{
    currlayer->showhashinfo = !currlayer->showhashinfo;
    
    // only show hashing info while generating
    if (generating) lifealgo::setVerbose( currlayer->showhashinfo );
}

// -----------------------------------------------------------------------------

static void ToggleTiming()
{
    showtiming = !showtiming;
}

// -----------------------------------------------------------------------------

static void SetAlgo(int index)
{
    if (index >= 0 && index < NumAlgos()) {
        if (index != currlayer->algtype) ChangeAlgorithm(index, currlayer->algo->getrule());
    } else {
        Warning("Bug detected in SetAlgo!");
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void ModeChanged(int index)
{
    switch (index) {
        case 0:  currlayer->touchmode = drawmode;    break;
        case 1:  currlayer->touchmode = pickmode;    break;
        case 2:  currlayer->touchmode = selectmode;  break;
        case 3:  currlayer->touchmode = movemode;    break;
        case 4:  currlayer->touchmode = zoominmode;  break;
        case 5:  currlayer->touchmode = zoomoutmode; break;
        default: Warning("Bug detected in ModeChanged!"); return;
    }
    UpdateCursorImage();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void StateChanged(int index)
{
    if (index >= 0 && index < currlayer->algo->NumCellStates()) {
        currlayer->drawingstate = index;
    } else {
        Warning("Bug detected in StateChanged!");
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

/* enable next 2 routines if we provide keyboard shortcuts for them!!!

static void DecState()
{
    if (currlayer->drawingstate > 0) {
        currlayer->drawingstate--;
        jsSetState(currlayer->drawingstate);
    }
}

// -----------------------------------------------------------------------------

static void IncState()
{
    if (currlayer->drawingstate < currlayer->algo->NumCellStates() - 1) {
        currlayer->drawingstate++;
        jsSetState(currlayer->drawingstate);
    }
}

*/

// -----------------------------------------------------------------------------

static void ToggleCursorMode()
{
    // state of shift key has changed so may need to toggle cursor mode
    if (currlayer->touchmode == drawmode) {
        currlayer->touchmode = pickmode;
        jsSetMode(currlayer->touchmode);
        UpdateCursorImage();
    } else if (currlayer->touchmode == pickmode) {
        currlayer->touchmode = drawmode;
        jsSetMode(currlayer->touchmode);
        UpdateCursorImage();
    } else if (currlayer->touchmode == zoominmode) {
        currlayer->touchmode = zoomoutmode;
        jsSetMode(currlayer->touchmode);
        UpdateCursorImage();
    } else if (currlayer->touchmode == zoomoutmode) {
        currlayer->touchmode = zoominmode;
        jsSetMode(currlayer->touchmode);
        UpdateCursorImage();
    }
}

// -----------------------------------------------------------------------------

static void SetRule()
{
    StopIfGenerating();
    std::string newrule = jsSetRule(currlayer->algo->getrule());
    // newrule is empty if user hit Cancel
    if (newrule.length() > 0) ChangeRule(newrule);
}

// -----------------------------------------------------------------------------

static void FlipPasteOrSelection(bool direction)
{
    if (waitingforpaste) {
        FlipPastePattern(direction);
    } else if (SelectionExists()) {
        FlipSelection(direction);
    }
    UpdateEverything();
}

// -----------------------------------------------------------------------------

static void RotatePasteOrSelection(bool direction)
{
    if (waitingforpaste) {
        RotatePastePattern(direction);
    } else if (SelectionExists()) {
        RotateSelection(direction);
    }
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C" {

void PasteAction(int item)
{
    if (item == 2 && !SelectionExists()) {
        // pastesel item is grayed, so ignore click
        return;
    }

    // remove menu
    EM_ASM( document.getElementById('pastemenu').style.visibility = 'hidden'; );
    paste_menu_visible = false;

    switch (item) {
        case 0:  AbortPaste(); break;
        case 1:  DoPaste(false); break;
        case 2:  DoPaste(true); break;
        case 3:  FlipPastePattern(true); break;
        case 4:  FlipPastePattern(false); break;
        case 5:  RotatePastePattern(true); break;
        case 6:  RotatePastePattern(false); break;
        default: Warning("Bug detected in PasteAction!");
    }
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void SelectionAction(int item)
{
    // remove menu first
    EM_ASM( document.getElementById('selectionmenu').style.visibility = 'hidden'; );
    selection_menu_visible = false;

    if (generating && item >= 1 && item <= 13 && item != 2 && item != 5 && item != 6) {
        // temporarily stop generating for all actions except Remove, Copy, Shrink, Fit
        PauseGenerating();
    }
    switch (item) {
        case 0:  RemoveSelection(); break;                      // WARNING: above test assumes Remove is item 0
        case 1:  CutSelection(); break;
        case 2:  CopySelection(); break;                        // WARNING: above test assumes Copy is item 2
        case 3:  ClearSelection(); break;
        case 4:  ClearOutsideSelection(); break;
        case 5:  ShrinkSelection(false); break;                 // WARNING: above test assumes Shrink is item 5
        case 6:  FitSelection(); break;                         // WARNING: above test assumes Fit is item 6
        case 7:  RandomFill(); break;
        case 8:  FlipSelection(true); break;
        case 9:  FlipSelection(false); break;
        case 10: RotateSelection(true); break;
        case 11: RotateSelection(false); break;
        case 12: currlayer->currsel.Advance(); break;
        case 13: currlayer->currsel.AdvanceOutside(); break;    // WARNING: above test assumes 13 is last item
        default: Warning("Bug detected in SelectionAction!");
    }
    ResumeGenerating();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ClearStatus()
{
    ClearMessage();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void OpenClickedFile(const char* filepath)
{
    ClearMessage();
    StopIfGenerating();
    OpenFile(filepath);
}

} // extern "C"

// -----------------------------------------------------------------------------

static void ChangePasteMode(paste_mode newmode)
{
    if (pmode != newmode) pmode = newmode;
}

// -----------------------------------------------------------------------------

static void ToggleDisableUndoRedo()
{
    allowundo = !allowundo;
    if (allowundo) {
        if (currlayer->algo->getGeneration() > currlayer->startgen) {
            // undo list is empty but user can Reset, so add a generating change
            // to undo list so user can Undo or Reset (and then Redo if they wish)
            currlayer->undoredo->AddGenChange();
        }
    } else {
        currlayer->undoredo->ClearUndoRedo();
    }
    UpdateEditBar();
}

// -----------------------------------------------------------------------------

extern "C" {

void UpdateMenuItems(const char* id)
{
    // this routine is called just before the given menu is made visible
    std::string menu = id;
    
    if (menu == "File_menu") {
        // items in this menu don't change
    
    } else if (menu == "Edit_menu") {
        if (currlayer->undoredo->CanUndo()) {
            EM_ASM( document.getElementById('edit_undo').className = 'item_normal'; );
        } else {
            EM_ASM( document.getElementById('edit_undo').className = 'item_disabled'; );
        }
        if (currlayer->undoredo->CanRedo()) {
            EM_ASM( document.getElementById('edit_redo').className = 'item_normal'; );
        } else {
            EM_ASM( document.getElementById('edit_redo').className = 'item_disabled'; );
        }
        if (allowundo) {
            EM_ASM( document.getElementById('edit_disable').innerHTML = 'Disable Undo/Redo'; );
        } else {
            EM_ASM( document.getElementById('edit_disable').innerHTML = 'Enable Undo/Redo'; );
        }
        if (SelectionExists()) {
            EM_ASM( document.getElementById('edit_cut').className = 'item_normal'; );
            EM_ASM( document.getElementById('edit_copy').className = 'item_normal'; );
            EM_ASM( document.getElementById('edit_clear').className = 'item_normal'; );
            EM_ASM( document.getElementById('edit_clearo').className = 'item_normal'; );
            EM_ASM( document.getElementById('edit_remove').className = 'item_normal'; );
        } else {
            EM_ASM( document.getElementById('edit_cut').className = 'item_disabled'; );
            EM_ASM( document.getElementById('edit_copy').className = 'item_disabled'; );
            EM_ASM( document.getElementById('edit_clear').className = 'item_disabled'; );
            EM_ASM( document.getElementById('edit_clearo').className = 'item_disabled'; );
            EM_ASM( document.getElementById('edit_remove').className = 'item_disabled'; );
        }
    
    } else if (menu == "PasteMode_menu") {
        jsTickMenuItem("paste_mode_and", pmode == And);
        jsTickMenuItem("paste_mode_copy", pmode == Copy);
        jsTickMenuItem("paste_mode_or", pmode == Or);
        jsTickMenuItem("paste_mode_xor", pmode == Xor);
    
    } else if (menu == "Control_menu") {
        if (generating) {
            EM_ASM( document.getElementById('control_startstop').innerHTML = 'Stop Generating'; );
        } else {
            EM_ASM( document.getElementById('control_startstop').innerHTML = 'Start Generating'; );
        }
        if (currlayer->algo->getGeneration() > currlayer->startgen) {
            EM_ASM( document.getElementById('control_reset').className = 'item_normal'; );
        } else {
            EM_ASM( document.getElementById('control_reset').className = 'item_disabled'; );
        }
        jsTickMenuItem("control_autofit", currlayer->autofit);
        jsTickMenuItem("control_hash", currlayer->showhashinfo);
        jsTickMenuItem("control_timing", showtiming);
    
    } else if (menu == "Algo_menu") {
        jsTickMenuItem("algo0", currlayer->algtype == 0);
        jsTickMenuItem("algo1", currlayer->algtype == 1);
        jsTickMenuItem("algo2", currlayer->algtype == 2);
        jsTickMenuItem("algo3", currlayer->algtype == 3);
        jsTickMenuItem("algo4", currlayer->algtype == 4);
        jsTickMenuItem("algo5", currlayer->algtype == 5);
        jsTickMenuItem("algo6", currlayer->algtype == 6);
    
    } else if (menu == "View_menu") {
        if (SelectionExists()) {
            EM_ASM( document.getElementById('view_fits').className = 'item_normal'; );
        } else {
            EM_ASM( document.getElementById('view_fits').className = 'item_disabled'; );
        }
        jsTickMenuItem("view_xys", showxystate);
        jsTickMenuItem("view_grid", showgridlines);
        jsTickMenuItem("view_icons", showicons);
        if (currlayer->currfile.length() > 0) {
            EM_ASM( document.getElementById('view_info').className = 'item_normal'; );
        } else {
            EM_ASM( document.getElementById('view_info').className = 'item_disabled'; );
        }
    
    } else if (menu == "Scale_menu") {
        jsTickMenuItem("scale0", currlayer->view->getmag() == 0);
        jsTickMenuItem("scale1", currlayer->view->getmag() == 1);
        jsTickMenuItem("scale2", currlayer->view->getmag() == 2);
        jsTickMenuItem("scale3", currlayer->view->getmag() == 3);
        jsTickMenuItem("scale4", currlayer->view->getmag() == 4);
        jsTickMenuItem("scale5", currlayer->view->getmag() == 5);
    
    } else if (menu == "Help_menu") {
        // items in this menu don't change
    
    } else if (menu == "pastemenu") {
        char text[32];
        sprintf(text, "Paste Here (%s)", GetPasteMode());
        jsSetInnerHTML("pastehere", text);
        if (SelectionExists()) {
            EM_ASM( document.getElementById('pastesel').className = 'normal'; );
        } else {
            EM_ASM( document.getElementById('pastesel').className = 'grayed'; );
        }
    
    } else if (menu == "selectionmenu") {
        char text[32];
        sprintf(text, "Random Fill (%d%%)", randomfill);
        jsSetInnerHTML("randfill", text);
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void DoMenuItem(const char* id)
{
    std::string item = id;
    
    // items in File menu:
    if (item == "file_new") NewUniverse(); else
    if (item == "file_open") Open(); else
    if (item == "file_openclip") OpenClipboard(); else
    if (item == "file_save") Save(); else
    if (item == "file_prefs") OpenPrefs(); else
    
    // items in Edit menu:
    if (item == "edit_undo") Undo(); else
    if (item == "edit_redo") Redo(); else
    if (item == "edit_disable") ToggleDisableUndoRedo(); else
    if (item == "edit_cut") CutSelection(); else
    if (item == "edit_copy") CopySelection(); else
    if (item == "edit_clear") ClearSelection(); else
    if (item == "edit_clearo") ClearOutsideSelection(); else
    if (item == "edit_paste") Paste(); else
    if (item == "paste_mode_and") ChangePasteMode(And); else
    if (item == "paste_mode_copy") ChangePasteMode(Copy); else
    if (item == "paste_mode_or") ChangePasteMode(Or); else
    if (item == "paste_mode_xor") ChangePasteMode(Xor); else
    if (item == "edit_all") SelectAll(); else
    if (item == "edit_remove") RemoveSelection(); else
    
    // items in Control menu:
    if (item == "control_startstop") StartStop(); else
    if (item == "control_next") Next(); else
    if (item == "control_step") Step(); else
    if (item == "control_step1") StepBy1(); else
    if (item == "control_slower") GoSlower(); else
    if (item == "control_faster") GoFaster(); else
    if (item == "control_reset") Reset(); else
    if (item == "control_autofit") ToggleAutoFit(); else
    if (item == "control_hash") ToggleHashInfo(); else
    if (item == "control_timing") ToggleTiming(); else
    if (item == "algo0") SetAlgo(0); else
    if (item == "algo1") SetAlgo(1); else
    if (item == "algo2") SetAlgo(2); else
    if (item == "algo3") SetAlgo(3); else
    if (item == "algo4") SetAlgo(4); else
    if (item == "algo5") SetAlgo(5); else
    if (item == "algo6") SetAlgo(6); else
    if (item == "control_setrule") SetRule(); else
    
    // items in View menu:
    if (item == "view_fitp") Fit(); else
    if (item == "view_fits") FitSelection(); else
    if (item == "view_middle") Middle(); else
    if (item == "view_zoomin") ZoomIn(); else
    if (item == "view_zoomout") ZoomOut(); else
    if (item == "scale0") SetScale(0); else
    if (item == "scale1") SetScale(1); else
    if (item == "scale2") SetScale(2); else
    if (item == "scale3") SetScale(3); else
    if (item == "scale4") SetScale(4); else
    if (item == "scale5") SetScale(5); else
    if (item == "view_xys") { showxystate = !showxystate; UpdateStatus(); } else
    if (item == "view_grid") ToggleGrid(); else
    if (item == "view_icons") ToggleIcons(); else
    if (item == "view_info") Info(); else
    
    // items in Help menu:
    if (item == "help_index")    StopAndHelp("/Help/index.html"); else
    if (item == "help_intro")    StopAndHelp("/Help/intro.html"); else
    if (item == "help_tips")     StopAndHelp("/Help/tips.html"); else
    if (item == "help_algos")    StopAndHelp("/Help/algos.html"); else
    if (item == "help_lexicon")  StopAndHelp("/Help/Lexicon/lex.htm"); else
    if (item == "help_archives") StopAndHelp("/Help/archives.html"); else
    if (item == "help_keyboard") StopAndHelp("/Help/keyboard.html"); else
    if (item == "help_refs")     StopAndHelp("/Help/refs.html"); else
    if (item == "help_formats")  StopAndHelp("/Help/formats.html"); else
    if (item == "help_bounded")  StopAndHelp("/Help/bounded.html"); else
    if (item == "help_problems") StopAndHelp("/Help/problems.html"); else
    if (item == "help_credits")  StopAndHelp("/Help/credits.html"); else
    if (item == "help_about")    StopAndHelp("/Help/about.html"); else
    
    Warning("Not yet implemented!!!");
}

} // extern "C"

// -----------------------------------------------------------------------------

static const int META_KEY = 666;  // cmd key on Mac, start/menu key on Windows (latter untested!!!)

static int TranslateKey(int keycode)
{
    // this is a modified version of DOMToGLFWKeyCode in emscripten/src/library_glfw.js
    switch (keycode) {
        // my changes (based on testing and info at http://unixpapa.com/js/key.html)
        case 224 : return META_KEY;     // cmd key on Firefox (Mac)
        case 91  : return META_KEY;     // left cmd key on Safari, Chrome (Mac)
        case 93  : return META_KEY;     // right cmd key on Safari, Chrome (Mac)
        case 92  : return META_KEY;     // right start key on Firefox, IE (Windows)
        case 219 : return '[';          // Firefox, Chrome and Safari (Mac)
        case 220 : return '\\';         // Firefox, Chrome and Safari (Mac)
        case 221 : return ']';          // Firefox, Chrome and Safari (Mac)
        case 173 : return '-';          // Firefox (Mac)
        case 189 : return '-';          // Chrome and Safari (Mac)
        case 187 : return '=';          // Chrome and Safari (Mac)
        case 188 : return '<';          // Firefox, Chrome and Safari (Mac)
        case 190 : return '>';          // Firefox, Chrome and Safari (Mac)
        
        case 0x6A: return 313 ; //DOM_VK_MULTIPLY -> GLFW_KEY_KP_MULTIPLY
        case 0x6B: return 315 ; //DOM_VK_ADD -> GLFW_KEY_KP_ADD
        case 0x6D: return 314 ; //DOM_VK_SUBTRACT -> GLFW_KEY_KP_SUBTRACT
        case 0x6E: return 316 ; //DOM_VK_DECIMAL -> GLFW_KEY_KP_DECIMAL
        case 0x6F: return 312 ; //DOM_VK_DIVIDE -> GLFW_KEY_KP_DIVIDE
        case 0x70: return 258 ; //DOM_VK_F1 -> GLFW_KEY_F1
        case 0x71: return 259 ; //DOM_VK_F2 -> GLFW_KEY_F2
        case 0x72: return 260 ; //DOM_VK_F3 -> GLFW_KEY_F3
        case 0x73: return 261 ; //DOM_VK_F4 -> GLFW_KEY_F4
        case 0x74: return 262 ; //DOM_VK_F5 -> GLFW_KEY_F5
        case 0x75: return 263 ; //DOM_VK_F6 -> GLFW_KEY_F6
        case 0x76: return 264 ; //DOM_VK_F7 -> GLFW_KEY_F7
        case 0x77: return 265 ; //DOM_VK_F8 -> GLFW_KEY_F8
        case 0x78: return 266 ; //DOM_VK_F9 -> GLFW_KEY_F9
        case 0x79: return 267 ; //DOM_VK_F10 -> GLFW_KEY_F10
        case 0x7a: return 268 ; //DOM_VK_F11 -> GLFW_KEY_F11
        case 0x7b: return 269 ; //DOM_VK_F12 -> GLFW_KEY_F12
        case 0x25: return 285 ; //DOM_VK_LEFT -> GLFW_KEY_LEFT
        case 0x26: return 283 ; //DOM_VK_UP -> GLFW_KEY_UP
        case 0x27: return 286 ; //DOM_VK_RIGHT -> GLFW_KEY_RIGHT
        case 0x28: return 284 ; //DOM_VK_DOWN -> GLFW_KEY_DOWN
        case 0x21: return 298 ; //DOM_VK_PAGE_UP -> GLFW_KEY_PAGEUP
        case 0x22: return 299 ; //DOM_VK_PAGE_DOWN -> GLFW_KEY_PAGEDOWN
        case 0x24: return 300 ; //DOM_VK_HOME -> GLFW_KEY_HOME
        case 0x23: return 301 ; //DOM_VK_END -> GLFW_KEY_END
        case 0x2d: return 296 ; //DOM_VK_INSERT -> GLFW_KEY_INSERT
        case 16  : return 287 ; //DOM_VK_SHIFT -> GLFW_KEY_LSHIFT
        case 0x05: return 287 ; //DOM_VK_LEFT_SHIFT -> GLFW_KEY_LSHIFT
        case 0x06: return 288 ; //DOM_VK_RIGHT_SHIFT -> GLFW_KEY_RSHIFT
        case 17  : return 289 ; //DOM_VK_CONTROL -> GLFW_KEY_LCTRL
        case 0x03: return 289 ; //DOM_VK_LEFT_CONTROL -> GLFW_KEY_LCTRL
        case 0x04: return 290 ; //DOM_VK_RIGHT_CONTROL -> GLFW_KEY_RCTRL
        case 18  : return 291 ; //DOM_VK_ALT -> GLFW_KEY_LALT
        case 0x02: return 291 ; //DOM_VK_LEFT_ALT -> GLFW_KEY_LALT
        case 0x01: return 292 ; //DOM_VK_RIGHT_ALT -> GLFW_KEY_RALT
        case 96  : return 302 ; //GLFW_KEY_KP_0
        case 97  : return 303 ; //GLFW_KEY_KP_1
        case 98  : return 304 ; //GLFW_KEY_KP_2
        case 99  : return 305 ; //GLFW_KEY_KP_3
        case 100 : return 306 ; //GLFW_KEY_KP_4
        case 101 : return 307 ; //GLFW_KEY_KP_5
        case 102 : return 308 ; //GLFW_KEY_KP_6
        case 103 : return 309 ; //GLFW_KEY_KP_7
        case 104 : return 310 ; //GLFW_KEY_KP_8
        case 105 : return 311 ; //GLFW_KEY_KP_9
        default  : return keycode;
    };
}

// -----------------------------------------------------------------------------

extern "C" {

int OnKeyChanged(int keycode, int action)
{
    int key = TranslateKey(keycode);
    
    // first check for modifier keys (meta/ctrl/alt/shift);
    // note that we need to set flags for these keys BEFORE testing
    // jsTextAreaIsActive so users can do things like ctrl-click in canvas while
    // a textarea has focus and OnMouseClick will be able to test ctrl_down flag
    
    if (key == META_KEY) {
        meta_down = action == GLFW_PRESS;
        return 0;   // don't call preventDefault
    } else if (key == GLFW_KEY_LCTRL || key == GLFW_KEY_RCTRL) {
        ctrl_down = action == GLFW_PRESS;
        return 0;   // don't call preventDefault
    } else if (key == GLFW_KEY_LALT || key == GLFW_KEY_RALT) {
        alt_down = action == GLFW_PRESS;
        return 1;
    } else if (key == GLFW_KEY_LSHIFT || key == GLFW_KEY_RSHIFT) {
        bool oldshift = shift_down;
        shift_down = action == GLFW_PRESS;
        if (oldshift != shift_down) ToggleCursorMode();
        return 1;
    }
    
    if (meta_down || ctrl_down) {
        // could be a browser shortcut like ctrl/cmd-Q/X/C/V,
        // so don't handle the key here and don't call preventDefault
        return 0;
    }
    
    // check if a modal dialog is visible
    if (jsElementIsVisible("open_overlay") ||
        jsElementIsVisible("save_overlay") ||
        jsElementIsVisible("prefs_overlay") ||
        jsElementIsVisible("info_overlay") ||
        jsElementIsVisible("help_overlay") ) {
        if (action == GLFW_PRESS && (key == 13 || key == 27)) {
            // return key or escape key closes dialog
            if (jsElementIsVisible("open_overlay")) {
                CancelOpen();
            } else if (jsElementIsVisible("save_overlay")) {
                if (key == 13) {
                    EM_ASM( save_pattern(); );  // see shell.html for save_pattern()
                } else {
                    CancelSave();
                }
            } else if (jsElementIsVisible("prefs_overlay")) {
                if (key == 13) {
                    StorePrefs();
                } else {
                    CancelPrefs();
                }
            } else if (jsElementIsVisible("info_overlay")) {
                EM_ASM( _CloseInfo(); );
            } else if (jsElementIsVisible("help_overlay")) {
                // some of the above dialogs can be on top of help dialog, so test this last
                EM_ASM( _CloseHelp(); );
            }
            return 1;   // call preventDefault
        }
        return 0;
    }
    
    if (jsTextAreaIsActive()) {
        // a textarea is active (and probably has focus),
        // so don't handle the key here and don't call preventDefault
        return 0;
    }

    if (action == GLFW_PRESS) ClearMessage();

    if (action == GLFW_RELEASE) return 1;   // non-modifier key was released
    
    // a non-modifer key is down (and meta/ctrl key is NOT currently down)
    
    // check for arrow keys and do panning
    if (key == GLFW_KEY_UP) {
        if (shift_down)
            PanNE();
        else
            PanUp( SmallScroll(currlayer->view->getheight()) );
        return 1;
    } else if (key == GLFW_KEY_DOWN) {
        if (shift_down)
            PanSW();
        else
            PanDown( SmallScroll(currlayer->view->getheight()) );
        return 1;
    } else if (key == GLFW_KEY_LEFT) {
        if (shift_down)
            PanNW();
        else
            PanLeft( SmallScroll(currlayer->view->getwidth()) );
        return 1;
    } else if (key == GLFW_KEY_RIGHT) {
        if (shift_down)
            PanSE();
        else
            PanRight( SmallScroll(currlayer->view->getwidth()) );
        return 1;
    }
    
    int ch = key;
    if (key >= 'A' && key <= 'Z' && !shift_down) {
        // convert to lowercase
        ch = ch + 32;
    }
    
    switch (ch) {
        case 13  : StartStop(); break;
        case ' ' : Next(); break;
        case 9   : Step(); break;
        case 8   : if (shift_down) ClearOutsideSelection(); else ClearSelection(); break;
        case '_' : GoSlower(); break;
        case '-' : GoSlower(); break;
        case '+' : GoFaster(); break;
        case '=' : GoFaster(); break;
        case '0' : StepBy1(); break;
        case '1' : Scale1to1(); break;
        case '5' : RandomFill(); break;
        case '[' : ZoomOut(); break;
        case ']' : ZoomIn(); break;
        case 'a' : SelectAll(); break;
        case 'A' : RemoveSelection(); break;
        case 'f' : Fit(); break;
        case 'F' : FitSelection(); break;
        case 'h' : Help(); break;
        case 'i' : ToggleIcons(); break;
        case 'l' : ToggleGrid(); break;
        case 'm' : Middle(); break;
        case 'o' : Open(); break;
        case 'O' : OpenClipboard(); break;
        case 'p' : OpenPrefs(); break;
        case 'r' : SetRule(); break;
        case 's' : Save(); break;
        case 't' : ToggleAutoFit(); break;
        case 'v' : Paste(); break;
        case 'V' : CancelPaste(); break;
        case 'x' : FlipPasteOrSelection(false); break;
        case 'y' : FlipPasteOrSelection(true); break;
        case '<' : RotatePasteOrSelection(false); break;
        case '>' : RotatePasteOrSelection(true); break;
        case 'z' : Undo(); break;
        case 'Z' : Redo(); break;
    }
    
    return 1;   // call preventDefault
}

} // extern "C"

// -----------------------------------------------------------------------------

static void OnMouseClick(int button, int action)
{
    ok_to_check_mouse = true;
    if (action == GLFW_PRESS) {
        // make sure a textarea element no longer has focus (for testing in on_key_changed);
        // note that 'patterns' is a div with a tabindex, and an outline style that prevents
        // a focus ring appearing
        EM_ASM( document.getElementById('patterns').focus(); );

        int x, y;
        glfwGetMousePos(&x, &y);
        
        ClearMessage();
        
        if (paste_menu_visible) {
            EM_ASM( document.getElementById('pastemenu').style.visibility = 'hidden'; );
            paste_menu_visible = false;
            return;
        }
        if (selection_menu_visible) {
            EM_ASM( document.getElementById('selectionmenu').style.visibility = 'hidden'; );
            selection_menu_visible = false;
            return;
        }
        
        // test for ctrl/right-click in paste image or selection
        if (button == GLFW_MOUSE_BUTTON_RIGHT || button == 2 || ctrl_down) {
            if (waitingforpaste && PointInPasteImage(x, y)) {
                UpdateMenuItems("pastemenu");
                jsShowMenu("pastemenu", x, y);
                paste_menu_visible = true;
            } else if (SelectionExists() && PointInSelection(x, y)) {
                UpdateMenuItems("selectionmenu");
                jsShowMenu("selectionmenu", x, y);
                selection_menu_visible = true;
            }
            return;
        }
        
        // check for click outside viewport
        if (x < 0 || x >= currwd || y < 0 || y >= currht) {
            if (mouse_down) TouchEnded();
            mouse_down = false;
            return;
        }
        
        bool was_auto_fit = currlayer->autofit;
        
        TouchBegan(x, y);
        mouse_down = true;
        
        // TouchBegan might have called TestAutoFit and turned off currlayer->autofit,
        // in which case we need to update the check box
        if (was_auto_fit && !currlayer->autofit) UpdateEditBar();
        
    
    } else if (action == GLFW_RELEASE) {
        if (mouse_down) TouchEnded();
        mouse_down = false;
    }
}

// -----------------------------------------------------------------------------

static void OnMouseMove(int x, int y)
{
    ok_to_check_mouse = true;
    int mousestate = glfwGetMouseButton(GLFW_MOUSE_BUTTON_LEFT);
    if (mousestate == GLFW_PRESS) {
        
        // play safe and ignore move outside viewport
        if (x < 0 || x >= currwd || y < 0 || y >= currht) return;
    
        TouchMoved(x, y);
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void OverCanvas(int entered)
{
    if (entered) {
        // mouse entered canvas so change cursor image depending on currlayer->touchmode
        UpdateCursorImage();
        
        // call CheckMouseLocation in DoFrame
        over_canvas = true;

        // remove focus from select element to avoid problem if an arrow key is pressed
        // (doesn't quite work!!! if selection is NOT changed and arrow key is hit immediately
        // then it can cause the selection to change)
        EM_ASM(
            if (document.activeElement.tagName == 'SELECT') {
                document.getElementById('patterns').focus();
            };
        );
        
    } else {
        // mouse exited canvas; cursor is automatically restored to standard arrow
        // so no need to do this:
        // EM_ASM( Module['canvas'].style.cursor = 'auto'; );
        
        // force XY location to be cleared
        CheckMouseLocation(-1, -1);
        // we also need to prevent CheckMouseLocation being called in DoFrame because
        // it might detect a valid XY location if right/bottom cells are clipped
        over_canvas = false;
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

static void DoFrame()
{
    if (generating && event_checker == 0) {
        if (currlayer->currexpo < 0) {
            // get current delay (in secs)
            float delay = GetCurrentDelay() / 1000.0;
            double current_time = glfwGetTime();
            // check if it's time to call NextGeneration
            if (current_time - last_time >= delay) {
                NextGeneration(true);
                UpdatePatternAndStatus();
                last_time = current_time;
            }
        } else {
            NextGeneration(true);
            UpdatePatternAndStatus();
        }
    }
    
    if (refresh_pattern) {
        refresh_pattern = false;
        DrawPattern(currindex);
    }
    
    glfwSwapBuffers();

    // check the current mouse location continuously, but only after the 1st mouse-click or
    // mouse-move event, because until then glfwGetMousePos returns 0,0
    if (ok_to_check_mouse && over_canvas) {
        int x, y;
        glfwGetMousePos(&x, &y);
        CheckMouseLocation(x, y);
        // also check if cursor image needs to be changed (this can only be done AFTER
        // DrawPattern has set pasterect, which is tested by PointInPasteImage)
        CheckCursor(x, y);
    }
}

// -----------------------------------------------------------------------------

// we need the EMSCRIPTEN_KEEPALIVE flag to avoid our main() routine disappearing
// (a somewhat strange consequence of using -s EXPORTED_FUNCTIONS in Makefile)

int EMSCRIPTEN_KEEPALIVE main()
{
    SetMessage("This is Golly 5.0 for the web (copyright 2005-2025 The Golly Gang).");
    InitPaths();                // init tempdir, prefsfile, etc
    MAX_MAG = 5;                // maximum cell size = 32x32
    maxhashmem = 300;           // enough for caterpillar
    InitAlgorithms();           // must initialize algoinfo first
    GetLocalPrefs();            // load user's preferences from local storage
    SetMinimumStepExponent();   // for slowest speed
    AddLayer();                 // create initial layer (sets currlayer)
    NewPattern();               // create new, empty universe
    UpdateStatus();             // show initial message

    InitClipboard();            // initialize clipboard data
    UpdateEditBar();            // initialize buttons, drawing state, and check boxes

    if (InitGL() == GL_TRUE) {
        ResizeCanvas();
        // we do our own keyboard event handling (see InitEventHandlers)
        // glfwSetKeyCallback(...);
        glfwSetMouseButtonCallback(OnMouseClick);
        glfwSetMousePosCallback(OnMouseMove);
        // we do our own mouse wheel handling (see InitEventHandlers)
        // glfwSetMouseWheelCallback(...);
        emscripten_set_main_loop(DoFrame, 0, 1);
    }

    glfwTerminate();
    return 0;
}

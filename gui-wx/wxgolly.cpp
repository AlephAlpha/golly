// This file is part of Golly.
// See docs/License.html for the copyright notice.

// A GUI for Golly, implemented in wxWidgets (www.wxwidgets.org).
// Unfinished code is flagged by "!!!".
// Uncertain code is flagged by "???".

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "wx/image.h"      // for wxImage
#include "wx/stdpaths.h"   // for wxStandardPaths
#include "wx/sysopt.h"     // for wxSystemOptions
#include "wx/filename.h"   // for wxFileName
#include "wx/fs_inet.h"    // for wxInternetFSHandler
#include "wx/fs_zip.h"     // for wxZipFSHandler

#include "lifepoll.h"
#include "util.h"          // for lifeerrors

#include "wxgolly.h"       // defines GollyApp class
#include "wxmain.h"        // defines MainFrame class
#include "wxstatus.h"      // defines StatusBar class
#include "wxview.h"        // defines PatternView class
#include "wxutils.h"       // for Warning, Fatal, BeginProgress, etc
#include "wxprefs.h"       // for GetPrefs, gollydir, rulesdir, userrules

#ifdef __WXMSW__
    // app icons are loaded via .rc file
#else
    #include "icons/appicon.xpm"
#endif

// -----------------------------------------------------------------------------

// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution and also implements the
// accessor function wxGetApp() which will return the reference of the correct
// type (ie. GollyApp and not wxApp).

IMPLEMENT_APP(GollyApp)

// -----------------------------------------------------------------------------

#define STRINGIFY(arg) STR2(arg)
#define STR2(arg) #arg

MainFrame* mainptr = NULL;      // main window
PatternView* viewptr = NULL;    // current viewport window (possibly a tile)
PatternView* bigview = NULL;    // main viewport window
StatusBar* statusptr = NULL;    // status bar window
wxStopWatch* stopwatch;         // global stopwatch
int insideYield = 0;            // if > 0 then processing an event via Yield()
double scalefactor = 1.0;       // main window's scale factor (2.0 on Retina displays, 1.0 otherwise)

// -----------------------------------------------------------------------------

// let non-wx modules call Fatal, Warning, BeginProgress, etc

class wx_errors : public lifeerrors
{
public:
    virtual void fatal(const char* s) {
        Fatal(wxString(s,wxConvLocal));
    }
    
    virtual void warning(const char* s) {
        Warning(wxString(s,wxConvLocal));
    }
    
    virtual void status(const char* s) {
        statusptr->DisplayMessage(wxString(s,wxConvLocal));
    }
    
    virtual void beginprogress(const char* s) {
        BeginProgress(wxString(s,wxConvLocal));
        // init flag for isaborted() calls in non-wx modules
        aborted = false;
    }
    
    virtual bool abortprogress(double f, const char* s) {
        return AbortProgress(f, wxString(s,wxConvLocal));
    }
    
    virtual void endprogress() {
        EndProgress();
    }
    
    virtual const char* getuserrules() {
        // need to be careful converting Unicode wxString to char*
        #ifdef __WXMAC__
            // we need to convert path to decomposed UTF8 so fopen will work
            dirbuff = userrules.fn_str();
        #else
            dirbuff = userrules.mb_str(wxConvLocal);
        #endif
        return (const char*) dirbuff;
    }
    
    virtual const char* getrulesdir() {
        // need to be careful converting Unicode wxString to char*
        #ifdef __WXMAC__
            // we need to convert path to decomposed UTF8 so fopen will work
            dirbuff = rulesdir.fn_str();
        #else
            dirbuff = rulesdir.mb_str(wxConvLocal);
        #endif
        return (const char*) dirbuff;
    }
    
    virtual const char* gettempdir() {
        // need to be careful converting Unicode wxString to char*
        #ifdef __WXMAC__
            // we need to convert path to decomposed UTF8 so fopen will work
            dirbuff = tempdir.fn_str();
        #else
            dirbuff = tempdir.mb_str(wxConvLocal);
        #endif
        return (const char*) dirbuff;
    }
    
private:
    wxCharBuffer dirbuff;
};

wx_errors wxerrhandler;    // create instance

// -----------------------------------------------------------------------------

// let non-wx modules process events

class wx_poll : public lifepoll
{
public:
    virtual int checkevents();
    virtual void updatePop();
    long nextcheck;
};

int wx_poll::checkevents()
{
    // avoid calling Yield too often
    long t = stopwatch->Time();
    if (t > nextcheck) {
        nextcheck = t + 100;        // call 10 times per sec
        if (mainptr->infront) {
            // make sure viewport window keeps keyboard focus
            viewptr->SetFocus();
        }
        insideYield++;
        wxGetApp().Yield(true);
        insideYield--;
    }
    return isInterrupted();
}

void wx_poll::updatePop()
{
    if (showstatus && !mainptr->IsIconized()) {
        statusptr->Refresh(false);
    }
}

wx_poll wxpoller;    // create instance

lifepoll* GollyApp::Poller()
{
    return &wxpoller;
}

void GollyApp::PollerReset()
{
    wxpoller.resetInterrupted();
    wxpoller.nextcheck = 0;
}

void GollyApp::PollerInterrupt()
{
    wxpoller.setInterrupted();
    wxpoller.nextcheck = 0;
}

// -----------------------------------------------------------------------------

void SetAppDirectory(const char* argv0)
{
#ifdef __WXMSW__
    // on Windows we need to reset current directory to app directory if user
    // dropped file from somewhere else onto app to start it up (otherwise we
    // can't find Help files)
    wxString appdir = wxStandardPaths::Get().GetDataDir();
    wxString currdir = wxGetCwd();
    if ( currdir.CmpNoCase(appdir) != 0 )
        wxSetWorkingDirectory(appdir);
    // avoid VC++ warning
    wxUnusedVar(argv0);
#elif defined(__WXMAC__)
    // wxMac has set current directory to location of .app bundle so no need
    // to do anything
#else // assume Unix
    // first, try to switch to GOLLYDIR if that is set to a sensible value:
    static const char *gd = STRINGIFY(GOLLYDIR);
    if ( *gd == '/' && wxSetWorkingDirectory(wxString(gd,wxConvLocal)) ) {
        return;
    }
    // otherwise, use the executable directory as the application directory.
    // user might have started app from a different directory so find
    // last "/" in argv0 and change cwd if "/" isn't part of "./" prefix
    unsigned int pos = strlen(argv0);
    while (pos > 0) {
        pos--;
        if (argv0[pos] == '/') break;
    }
    if ( pos > 0 && !(pos == 1 && argv0[0] == '.') ) {
        char appdir[2048];
        if (pos < sizeof(appdir)) {
            strncpy(appdir, argv0, pos);
            appdir[pos] = 0;
            wxSetWorkingDirectory(wxString(appdir,wxConvLocal));
        }
    }
#endif
}

// -----------------------------------------------------------------------------

void GollyApp::SetFrameIcon(wxFrame* frame)
{
    // set frame icon
#ifdef __WXMSW__
    // create a bundle with 32x32 and 16x16 icons
    wxIconBundle icb(wxICON(appicon0));
    icb.AddIcon(wxICON(appicon1));
    frame->SetIcons(icb);
#else
    // use appicon.xpm on other platforms (ignored on Mac)
    frame->SetIcon(wxICON(appicon));
#endif
}

// -----------------------------------------------------------------------------

static wxString initdir;    // set to current working directory when app starts

#ifdef __WXMAC__

// open file double-clicked or dropped onto Golly icon

void GollyApp::MacOpenFile(const wxString& path)
{
    mainptr->Raise();
    wxFileName filename(path);
    // convert given path to a full path if not one already
    if (!filename.IsAbsolute()) filename = initdir + path;
    mainptr->pendingfiles.Add(filename.GetFullPath());

    // next OnIdle will call OpenFile (if we call OpenFile here with a script
    // that opens a modal dialog then dialog can't be closed!)
}

#endif

// -----------------------------------------------------------------------------

// app execution starts here

bool GollyApp::OnInit()
{
    SetAppName(_("Golly"));    // for use in Warning/Fatal dialogs
    
    // create a stopwatch so we can use Time() to get elapsed millisecs
    stopwatch = new wxStopWatch();
    
    // set variable seed for later rand() calls
    srand(time(0));
    
#ifdef __WXMAC__
    // remove Show Tab Bar etc from View menu
    OSXEnableAutomaticTabbing(false);
#endif
    
    // get current working directory before calling SetAppDirectory
    initdir = wxFileName::GetCwd();
    if (initdir.Last() != wxFILE_SEP_PATH) initdir += wxFILE_SEP_PATH;
    
    // make sure current working directory contains application otherwise
    // we can't open Help files
    SetAppDirectory( wxString(argv[0]).mb_str(wxConvLocal) );
    
    // now set global gollydir for use in GetPrefs and elsewhere
    gollydir = wxFileName::GetCwd();
    if (gollydir.Last() != wxFILE_SEP_PATH) gollydir += wxFILE_SEP_PATH;
    
    // let non-wx modules call Fatal, Warning, BeginProgress, etc
    lifeerrors::seterrorhandler(&wxerrhandler);
    
    // allow .html files to include common graphic formats
    wxImage::AddHandler(new wxGIFHandler);
    wxImage::AddHandler(new wxPNGHandler);
    wxImage::AddHandler(new wxTIFFHandler);
    
    // wxInternetFSHandler is needed to allow downloading files
    wxFileSystem::AddHandler(new wxInternetFSHandler);
    wxFileSystem::AddHandler(new wxZipFSHandler);
    
    // get main window location and other user preferences
    GetPrefs();
    
    // create main window (also initializes viewptr, bigview, statusptr)
    mainptr = new MainFrame();
    if (mainptr == NULL) Fatal(_("Failed to create main window!"));
    
    // initialize some stuff before showing main window
    mainptr->SetRandomFillPercentage();
    mainptr->SetMinimumStepExponent();
    
    wxString banner = _("This is Golly version ");
    banner +=         _(STRINGIFY(VERSION)); 
    banner +=         _(" (");
#ifdef __WXMAC__
    #ifdef __arm64__
        banner +=     _("Silicon");
    #else
        banner +=     _("Intel");
    #endif
#else
    #ifdef GOLLY64BIT
        banner +=     _("64-bit");
    #else
        banner +=     _("32-bit");
    #endif
#endif
    banner +=         _(").  Copyright 2005-2025 The Golly Gang.");
    if (debuglevel > 0) {
        banner += wxString::Format(_("  *** debuglevel = %d ***"), debuglevel);
    }
    statusptr->SetMessage(banner);
    
    mainptr->NewPattern();
    
    // script/pattern files are stored in the pendingfiles array for later processing
    // in OnIdle; this avoids a crash in Win app if a script is run before showing
    // the main window, and also avoids event problems in Win app with a long-running
    // script (eg. user can't hit escape to abort script)
    const wxString START_LUA = wxT("golly-start.lua");
    const wxString START_PYTHON = wxT("golly-start.py");
    wxString startscript = gollydir + START_LUA;
    if (wxFileExists(startscript)) {
        mainptr->pendingfiles.Add(startscript);
    } else {
        // look in user-specific data directory
        startscript = datadir + START_LUA;
        if (wxFileExists(startscript)) {
            mainptr->pendingfiles.Add(startscript);
        }
    }
    startscript = gollydir + START_PYTHON;
    if (wxFileExists(startscript)) {
        mainptr->pendingfiles.Add(startscript);
    } else {
        // look in user-specific data directory
        startscript = datadir + START_PYTHON;
        if (wxFileExists(startscript)) {
            mainptr->pendingfiles.Add(startscript);
        }
    }
    
    // argc is > 1 if command line has one or more script/pattern files
    for (int n = 1; n < argc; n++) {
        wxFileName filename(argv[n]);
        // convert given path to a full path if not one already
        if (!filename.IsAbsolute()) filename = initdir + argv[n];
        mainptr->pendingfiles.Add(filename.GetFullPath());
    }
    
    // show main window
    if (maximize) mainptr->Maximize(true);
    mainptr->Show(true);
    SetTopWindow(mainptr);
    
    // true means call wxApp::OnRun() which will enter the main event loop;
    // false means exit immediately
    return true;
}

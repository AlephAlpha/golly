// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXMAIN_H_
#define _WXMAIN_H_

#include "wx/splitter.h"   // for wxSplitterWindow, wxSplitterEvent
#include "wx/dirctrl.h"    // for wxGenericDirCtrl
#include "wx/treectrl.h"   // for wxTreeCtrl, wxTreeEvent
#include "wx/dataobj.h"    // for wxTextDataObject
#include "wx/webrequest.h" // for wxWebRequest, wxWebRequestEvent

#include "bigint.h"        // for bigint
#include "lifealgo.h"      // for lifealgo
#include "writepattern.h"  // for pattern_format
#include "wxprefs.h"       // for MAX_RECENT
#include "wxalgos.h"       // for MAX_ALGOS, algo_type
#include "wxlayer.h"       // for MAX_LAYERS

// Golly's main window:
class MainFrame : public wxFrame
{
public:
    MainFrame();
    ~MainFrame();
    
    // update functions
    void UpdateEverything();
    void UpdateUserInterface();
    void UpdateToolBar();
    void EnableAllMenus(bool enable);
    void UpdateMenuItems();
    void UpdatePatternAndStatus(bool update_now = false);
    void UpdateStatus();
    void UpdateMenuAccelerators();
    
    // clipboard functions
    bool ClipboardHasText();
    bool CopyTextToClipboard(const wxString& text);
    bool GetTextFromClipboard(wxTextDataObject* data);
    bool ClipboardContainsRule();
    bool ClipboardContainsRLE3();
    void OpenClipboard();
    void RunClipboard();
    
    // file functions
    void OpenFile(const wxString& path, bool remember = true);
    void LoadPattern(const wxString& path, const wxString& newtitle,
                     bool updatestatus = true, bool updateall = true);
    void NewPattern(const wxString& title = _("untitled"));
    void CreateUniverse();
    void SetWindowTitle(const wxString& filename);
    void OpenPattern();
    void OpenScript();
    void OpenRecentPattern(int id);
    void OpenRecentScript(int id);
    void ToggleShowFiles();
    void SetFileDir(const wxString& newdir);
    void SetFolder();
    void AddFolder();
    void RemoveFolder();
    void AppendDir(const wxString& indir, wxTreeCtrl* treectrl, wxTreeItemId root);
    void BuildTree();
    bool SavePattern();
    bool SaveCurrentLayer();
    const char* SaveFile(const wxString& path, const wxString& format, bool remember);
    const char* WritePattern(const wxString& path, pattern_format format,
                             output_compression compression,
                             int top, int left, int bottom, int right);
    void CheckBeforeRunning(const wxString& scriptpath, bool remember,
                            const wxString& zippath);
    bool ExtractZipEntry(const wxString& zippath,
                         const wxString& entryname,
                         const wxString& outfile);
#if wxUSE_DRAG_AND_DROP
    wxDropTarget* NewDropTarget();
#endif
    
    // edit functions
    void ToggleAllowUndo();
    void RestorePattern(bigint& gen, const wxString& filename,
                        bigint& x, bigint& y, int mag, int base, int expo);
    
    // prefs functions
    void SetRandomFillPercentage();
    void SetMinimumStepExponent();
    void UpdateStepExponent();
    void ShowPrefsDialog(const wxString& page = wxEmptyString);
    
    // control functions
    void StartGenTimer();
    void StartGenerating();
    void StopGenerating();
    void StartOrStop();
    void Stop();
    void FinishUp();
    void GoFaster();
    void GoSlower();
    void DisplayTimingInfo();
    void NextGeneration(bool useinc);
    void ToggleAutoFit();
    void ToggleHyperspeed();
    void ToggleHashInfo();
    void ToggleShowPopulation();
    void SetStepExponent(int newexpo);
    void SetGenIncrement();
    bool SaveStartingPattern();
    void ResetPattern(bool resetundo = true);
    void SetGeneration();
    const char* ChangeGenCount(const char* genstring, bool inundoredo = false);
    void SetBaseStep();
    void ClearOutsideGrid();
    void ReduceCellStates(int newmaxstate);
    void ShowRuleDialog();
    void ChangeAlgorithm(algo_type newalgotype, const wxString& newrule = wxEmptyString,
                         bool inundoredo = false);
    
    // view functions
    void ToggleStatusBar();
    void ToggleExactNumbers();
    void ToggleToolBar();
    void ToggleScrollBars();
    void ToggleFullScreen();
    void ShowPatternInfo();
    void ResizeSplitWindow(int wd, int ht);
    void ResizeStatusBar(int wd, int ht);
    void ResizeBigView();
    wxWindow* RightPane();
    
    // layer functions
    void SaveOverlay();
    void ToggleOverlay();
    void DeleteOverlay();
    void UpdateLayerItem(int index);
    void AppendLayerItem();
    void RemoveLayerItem();
    
    // miscellaneous functions
    void OnTreeClick(wxMouseEvent& event);
    void EditFile(const wxString& filepath);
    void QuitApp();

    bool generating;            // currently generating a pattern?
    bool fullscreen;            // in full screen mode?
    bool showbanner;            // showing banner message?
    bool keepmessage;           // don't clear message created by script?
    bool command_pending;       // user selected a command while generating?
    bool draw_pending;          // user wants to draw while generating?
    wxCommandEvent cmdevent;    // the pending command
    wxMouseEvent mouseevent;    // the pending draw
    
    wxTimer* gentimer;          // timer for generating patterns
    wxTimer* opentimer;         // timer for calling OpenFile from OnIdle
    
    // temporary files
    wxString clipfile;          // name of temporary file for storing clipboard data
    wxString luafile;           // name of temporary Lua script
    wxString pythonfile;        // name of temporary Python script
    
    // store files passed via command line (processed in first OnIdle)
    wxArrayString pendingfiles;

    bool infront;               // main window is active?

    // these scroll bars are needed to avoid bug in wxGLCanvas on Mac
    // and to allow showing/hiding them on all platforms
    wxScrollBar* hbar;
    wxScrollBar* vbar;

    // for downloading files
    void OnWebRequestState(wxWebRequestEvent& evt);
    bool DownloadURL(const wxString& url, const wxString& filepath);
    
private:
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
    
    // event handlers
    void OnMenu(wxCommandEvent& event);
    void OnSetFocus(wxFocusEvent& event);
    void OnActivate(wxActivateEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnDirTreeSelection(wxTreeEvent& event);
    void OnSashDblClick(wxSplitterEvent& event);
    void OnGenTimer(wxTimerEvent& event);
    void OnOpenTimer(wxTimerEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnScroll(wxScrollEvent& event);
    
    // file functions
    bool LoadImage(const wxString& path);
    wxString GetBaseName(const wxString& path);
    void SaveSucceeded(const wxString& path);
    void AddRecentPattern(const wxString& path);
    void ClearMissingPatterns();
    void ClearAllPatterns();
    void AddRecentScript(const wxString& path);
    void ClearMissingScripts();
    void ClearAllScripts();
    wxString GetScriptFileName(const wxString& text);
    void OpenZipFile(const wxString& path);

    // for downloading files
    wxWebRequest webRequest;
    wxString download_file;
    bool download_complete;
    
    // control functions
    void DisplayPattern();
    bool StepPattern();
    
    // miscellaneous functions
    void CreateMenus();
    void CreateToolbar();
    void CreateDirControl();
    void DoPendingAction(bool restart);
    
    // splittable window contains file directory in left pane
    // and layer/edit/timeline bars plus viewport window in right pane
    wxSplitterWindow* splitwin;
    wxGenericDirCtrl* filectrl;

    int hypdown;                    // for hyperspeed
    int minexpo;                    // currexpo at maximum delay (must be <= 0)
    long begintime, endtime;        // for timing info
    double begingen, endgen;        // ditto
};

// ids for menu commands, etc
enum {
    // File menu
    // wxID_NEW,
    // wxID_OPEN,
    ID_OPEN_CLIP = wxID_HIGHEST + 1,
    ID_OPEN_RECENT,
    // last 2 items in Open Recent submenu
    ID_CLEAR_MISSING_PATTERNS = ID_OPEN_RECENT + MAX_RECENT + 1,
    ID_CLEAR_ALL_PATTERNS,
    // wxID_SAVE,
    ID_SAVE_XRLE,
    ID_RUN_SCRIPT,
    ID_RUN_CLIP,
    ID_RUN_RECENT,
    // last 2 items in Run Recent submenu
    ID_CLEAR_MISSING_SCRIPTS = ID_RUN_RECENT + MAX_RECENT + 1,
    ID_CLEAR_ALL_SCRIPTS,
    ID_SHOW_FILES,
    ID_SET_FOLDER,
    ID_ADD_FOLDER,
    ID_REMOVE_FOLDER,
    // wxID_PREFERENCES,
    // wxID_EXIT,
    
    // Edit menu
    // due to wxMac bugs we don't use wxID_UNDO/REDO/CUT/COPY/CLEAR/PASTE/SELECTALL
    // (problems occur when info window or a modal dialog is active, and we can't
    // change the Undo/Redo menu labels)
    ID_UNDO,
    ID_REDO,
    ID_CUT,
    ID_COPY,
    ID_NO_UNDO,
    ID_CLEAR,
    ID_OUTSIDE,
    ID_PASTE,
    ID_PMODE,
    ID_PLOCATION,
    ID_PASTE_SEL,
    ID_SELECTALL,
    ID_REMOVE,
    ID_SHRINK,
    ID_SHRINKFIT,  // there's currently no menu item for "Shrink and Fit"
    ID_RANDOM,
    ID_FLIPTB,
    ID_FLIPLR,
    ID_ROTATEC,
    ID_ROTATEA,
    ID_CMODE,
    
    // Paste Location submenu
    ID_PL_TL,
    ID_PL_TR,
    ID_PL_BR,
    ID_PL_BL,
    ID_PL_MID,
    
    // Paste Mode submenu
    ID_PM_AND,
    ID_PM_COPY,
    ID_PM_OR,
    ID_PM_XOR,
    
    // Cursor Mode submenu
    ID_DRAW,
    ID_PICK,
    ID_SELECT,
    ID_MOVE,
    ID_ZOOMIN,
    ID_ZOOMOUT,
    
    // Control menu
    ID_START,
    ID_NEXT,
    ID_STEP,
    ID_RESET,
    ID_SETGEN,
    ID_FASTER,
    ID_SLOWER,
    ID_SETBASE,
    ID_AUTO,
    ID_HYPER,
    ID_HINFO,
    ID_SHOW_POP,
    ID_AUTOSTOP,
    ID_RECORD,
    ID_DELTIME,
    ID_SETALGO,
    ID_SETRULE,
    
    // Set Algorithm submenu
    ID_ALGO0,
    ID_ALGOMAX = ID_ALGO0 + MAX_ALGOS - 1,
    
    // View menu
    ID_FULL,
    ID_FIT,
    ID_FIT_SEL,
    ID_MIDDLE,
    ID_RESTORE00,
    // wxID_ZOOM_IN,
    // wxID_ZOOM_OUT,
    ID_SET_SCALE,
    ID_TOOL_BAR,
    ID_LAYER_BAR,
    ID_EDIT_BAR,
    ID_ALL_STATES,
    ID_STATUS_BAR,
    ID_EXACT,
    ID_XYSTATE,
    ID_GRID,
    ID_ICONS,
    ID_INVERT,
    ID_SMARTSCALE,
    ID_TIMELINE,
    ID_SCROLL,
    ID_INFO,
    
    // Set Scale submenu
    ID_SCALE_1,
    ID_SCALE_2,
    ID_SCALE_4,
    ID_SCALE_8,
    ID_SCALE_16,
    ID_SCALE_32,
    
    // Layer menu
    ID_SAVE_OVERLAY,
    ID_SHOW_OVERLAY,
    ID_DEL_OVERLAY,
    ID_ADD_LAYER,
    ID_CLONE,
    ID_DUPLICATE,
    ID_DEL_LAYER,
    ID_DEL_OTHERS,
    ID_MOVE_LAYER,
    ID_NAME_LAYER,
    ID_SET_COLORS,
    ID_SYNC_VIEW,
    ID_SYNC_CURS,
    ID_STACK,
    ID_TILE,
    ID_LAYER0,
    ID_LAYERMAX = ID_LAYER0 + MAX_LAYERS - 1,
    
    // Help menu
    ID_HELP_INDEX,
    ID_HELP_INTRO,
    ID_HELP_TIPS,
    ID_HELP_ALGOS,
    ID_HELP_KEYBOARD,
    ID_HELP_MOUSE,
    ID_HELP_LUA,
    ID_HELP_OVERLAY,
    ID_HELP_PYTHON,
    ID_HELP_LEXICON,
    ID_HELP_ARCHIVES,
    ID_HELP_FILE,
    ID_HELP_EDIT,
    ID_HELP_CONTROL,
    ID_HELP_VIEW,
    ID_HELP_LAYER,
    ID_HELP_HELP,
    ID_HELP_REFS,
    ID_HELP_FORMATS,
    ID_HELP_BOUNDED,
    ID_HELP_PROBLEMS,
    ID_HELP_CHANGES,
    ID_HELP_CREDITS,
    
    // these ids aren't associated with any menu item
    ID_LOAD_LEXICON,    // for loading a lexicon pattern
    ID_HELP_BUTT,       // for help button in tool bar
    ID_OPENTIMER,       // for opentimer
    ID_GENTIMER         // for gentimer
};

#endif

// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h" // for wxToolTip
#endif
#include "wx/rawbmp.h"      // for wxAlphaPixelData
#include "wx/filename.h"    // for wxFileName
#include "wx/colordlg.h"    // for wxColourDialog
#include "wx/tglbtn.h"      // for wxToggleButton

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"
#include "util.h"          // for linereader

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, bigview, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxedit.h"        // for ShiftEditBar
#include "wxselect.h"      // for Selection
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, FillRect, CreatePaleBitmap, etc
#include "wxrender.h"      // for DrawOneIcon
#include "wxprefs.h"       // for initrule, swapcolors, userrules, rulesdir, etc
#include "wxscript.h"      // for inscript
#include "wxundo.h"        // for UndoRedo, etc
#include "wxalgos.h"       // for algo_type, initalgo, algoinfo, CreateNewUniverse, etc
#include "wxlayer.h"

#ifdef _MSC_VER
   #pragma warning(disable:4702)   // disable "unreachable code" warnings from MSVC
#endif
#include <map>                     // for std::map
#ifdef _MSC_VER
   #pragma warning(default:4702)   // enable "unreachable code" warnings
#endif

#ifdef __WXMAC__
    // we need to convert filepath to decomposed UTF8 so fopen will work
    #define OPENFILE(filepath) fopen(filepath.fn_str(),"r")
#else
    #define OPENFILE(filepath) fopen(filepath.mb_str(wxConvLocal),"r")
#endif

// -----------------------------------------------------------------------------

const int layerbarht = 32;       // height of layer bar

int numlayers = 0;               // number of existing layers
int numclones = 0;               // number of cloned layers
int currindex = -1;              // index of current layer

Layer* currlayer = NULL;         // pointer to current layer
Layer* layer[MAX_LAYERS];        // array of layers

bool cloneavail[MAX_LAYERS];     // for setting unique cloneid
bool cloning = false;            // adding a cloned layer?
bool duplicating = false;        // adding a duplicated layer?

algo_type oldalgo;               // algorithm in old layer
wxString oldrule;                // rule in old layer
int oldmag;                      // scale in old layer
bigint oldx;                     // X position in old layer
bigint oldy;                     // Y position in old layer
wxCursor* oldcurs;               // cursor mode in old layer

// ids for bitmap buttons in layer bar
enum {
    LAYER_0 = 0,                  // LAYER_0 must be first id
    LAYER_LAST = LAYER_0 + MAX_LAYERS - 1,
    ADD_LAYER,
    CLONE_LAYER,
    DUPLICATE_LAYER,
    DELETE_LAYER,
    STACK_LAYERS,
    TILE_LAYERS,
    NUM_BUTTONS                   // must be last
};

// bitmaps for layer bar buttons
#include "bitmaps/add.xpm"
#include "bitmaps/clone.xpm"
#include "bitmaps/duplicate.xpm"
#include "bitmaps/delete.xpm"
#include "bitmaps/stack.xpm"
#include "bitmaps/stack_down.xpm"
#include "bitmaps/tile.xpm"
#include "bitmaps/tile_down.xpm"

// -----------------------------------------------------------------------------

// Define layer bar window:

// derive from wxPanel so we get current theme's background color on Windows
class LayerBar : public wxPanel
{
public:
    LayerBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
    ~LayerBar() {}
    
    // add a button to layer bar
    void AddButton(int id, const wxString& tip);
    
    // add a horizontal gap between buttons
    void AddSeparator();
    
    // enable/disable button
    void EnableButton(int id, bool enable);
    
    // set state of a toggle button
    void SelectButton(int id, bool select);
    
    // might need to expand/shrink width of layer buttons
    void ResizeLayerButtons();
    
    // detect press and release of button
    void OnButtonDown(wxMouseEvent& event);
    void OnButtonUp(wxMouseEvent& event);
    void OnKillFocus(wxFocusEvent& event);
    
private:
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
    
    // event handlers
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnButton(wxCommandEvent& event);
    
    // bitmaps for normal or down state
    wxBitmap normbutt[NUM_BUTTONS];
    wxBitmap downbutt[NUM_BUTTONS];
    
#ifdef __WXMSW__
    // on Windows we need bitmaps for disabled buttons
    wxBitmap disnormbutt[NUM_BUTTONS];
    wxBitmap disdownbutt[NUM_BUTTONS];
#endif
    
    // positioning data used by AddButton and AddSeparator
    int ypos, xpos, smallgap, biggap;
    
    int downid;          // id of currently pressed layer button
    int currbuttwd;      // current width of each layer button
};

BEGIN_EVENT_TABLE(LayerBar, wxPanel)
EVT_PAINT            (           LayerBar::OnPaint)
EVT_SIZE             (           LayerBar::OnSize)
EVT_LEFT_DOWN        (           LayerBar::OnMouseDown)
EVT_BUTTON           (wxID_ANY,  LayerBar::OnButton)
EVT_TOGGLEBUTTON     (wxID_ANY,  LayerBar::OnButton)
END_EVENT_TABLE()

static LayerBar* layerbarptr = NULL;      // global pointer to layer bar

// layer bar buttons must be global to use Connect/Disconnect on Windows;
// note that bitmapbutt[0..MAX_LAYERS-1] are not used, but it simplifies
// our logic to have those dummy indices
static wxBitmapButton* bitmapbutt[NUM_BUTTONS] = {NULL};
static wxToggleButton* togglebutt[MAX_LAYERS] = {NULL};

// width and height of toggle buttons
const int MAX_TOGGLE_WD = 128;
const int MIN_TOGGLE_WD = 48;
#if defined(__WXMSW__)
    const int TOGGLE_HT = 22;
#elif defined(__WXGTK__)
    const int TOGGLE_HT = 24;
#elif defined(__WXOSX_COCOA__) && !wxCHECK_VERSION(3,0,0)
    const int TOGGLE_HT = 24;
#else
    const int TOGGLE_HT = 20;
#endif

// width and height of bitmap buttons
#if defined(__WXOSX_COCOA__) && wxCHECK_VERSION(3,0,0)
    const int BUTTON_WD = 24;
    const int BUTTON_HT = 24;
#elif defined(__WXOSX_COCOA__) || defined(__WXGTK__)
    const int BUTTON_WD = 28;
    const int BUTTON_HT = 28;
#else
    const int BUTTON_WD = 24;
    const int BUTTON_HT = 24;
#endif

const wxString SWITCH_LAYER = _("Switch to this layer");

// -----------------------------------------------------------------------------

LayerBar::LayerBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
: wxPanel(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
#ifdef __WXMSW__
            // need this to avoid buttons flashing on Windows
            wxNO_FULL_REPAINT_ON_RESIZE
#else
            // better for Mac and Linux
            wxFULL_REPAINT_ON_RESIZE
#endif
            )
{
#ifdef __WXGTK__
    // avoid erasing background on GTK+
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
#endif
    
    // init bitmaps for normal state
    normbutt[ADD_LAYER] =         XPM_BITMAP(add);
    normbutt[ADD_LAYER] =         XPM_BITMAP(add);
    normbutt[CLONE_LAYER] =       XPM_BITMAP(clone);
    normbutt[DUPLICATE_LAYER] =   XPM_BITMAP(duplicate);
    normbutt[DELETE_LAYER] =      XPM_BITMAP(delete);
    normbutt[STACK_LAYERS] =      XPM_BITMAP(stack);
    normbutt[TILE_LAYERS] =       XPM_BITMAP(tile);
    
    // some bitmap buttons also have a down state
    downbutt[STACK_LAYERS] = XPM_BITMAP(stack_down);
    downbutt[TILE_LAYERS] =  XPM_BITMAP(tile_down);
    
#ifdef __WXMSW__
    // create bitmaps for disabled buttons
    CreatePaleBitmap(normbutt[ADD_LAYER],        disnormbutt[ADD_LAYER]);
    CreatePaleBitmap(normbutt[CLONE_LAYER],      disnormbutt[CLONE_LAYER]);
    CreatePaleBitmap(normbutt[DUPLICATE_LAYER],  disnormbutt[DUPLICATE_LAYER]);
    CreatePaleBitmap(normbutt[DELETE_LAYER],     disnormbutt[DELETE_LAYER]);
    CreatePaleBitmap(normbutt[STACK_LAYERS],     disnormbutt[STACK_LAYERS]);
    CreatePaleBitmap(normbutt[TILE_LAYERS],      disnormbutt[TILE_LAYERS]);
    
    // create bitmaps for disabled buttons in down state
    CreatePaleBitmap(downbutt[STACK_LAYERS],     disdownbutt[STACK_LAYERS]);
    CreatePaleBitmap(downbutt[TILE_LAYERS],      disdownbutt[TILE_LAYERS]);
#endif
    
    // init position variables used by AddButton and AddSeparator
    biggap = 16;
#ifdef __WXGTK__
    // buttons are a different size in wxGTK
    xpos = 2;
    ypos = 2;
    smallgap = 6;
#else
    xpos = 4;
    ypos = (32 - BUTTON_HT) / 2;
    smallgap = 4;
#endif
    
    downid = -1;         // no layer button down as yet
    
    currbuttwd = MAX_TOGGLE_WD;
}

// -----------------------------------------------------------------------------

void LayerBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    
    int wd, ht;
    GetClientSize(&wd, &ht);
    if (wd < 1 || ht < 1 || !showlayer) return;
    
#ifdef __WXMSW__
    // needed on Windows
    dc.Clear();
#endif
    
    wxRect r = wxRect(0, 0, wd, ht);
    
#ifdef __WXMAC__
    wxBrush brush(wxColor(202,202,202));
    FillRect(dc, r, brush);
#endif
    
    if (!showedit) {
        // draw gray border line at bottom edge
#if defined(__WXMSW__)
        dc.SetPen(*wxGREY_PEN);
#elif defined(__WXMAC__)
        wxPen linepen(wxColor(140,140,140));
        dc.SetPen(linepen);
#else
        dc.SetPen(*wxLIGHT_GREY_PEN);
#endif
        dc.DrawLine(0, r.GetBottom(), r.width, r.GetBottom());
        dc.SetPen(wxNullPen);
    }
}

// -----------------------------------------------------------------------------

void LayerBar::OnSize(wxSizeEvent& event)
{
    ResizeLayerButtons();
    event.Skip();
}

// -----------------------------------------------------------------------------

void LayerBar::ResizeLayerButtons()
{
    // might need to expand/shrink width of layer button(s)
    if (layerbarptr) {
        int wd, ht;
        GetClientSize(&wd, &ht);
        
        wxRect r1 = togglebutt[0]->GetRect();
        int x = r1.GetLeft();
        int y = r1.GetTop();
        const int rgap = 4;
        int viswidth = wd - x - rgap;
        int oldbuttwd = currbuttwd;
        
        if (numlayers*currbuttwd <= viswidth) {
            // all layer buttons are visible so try to expand widths
            while (currbuttwd < MAX_TOGGLE_WD && numlayers*(currbuttwd+1) <= viswidth) {
                currbuttwd++;
            }
        } else {
            // try to reduce widths until all layer buttons are visible
            while (currbuttwd > MIN_TOGGLE_WD && numlayers*currbuttwd > viswidth) {
                currbuttwd--;
            }
        }
        
        if (currbuttwd != oldbuttwd && currbuttwd >= 0) {
            for (int i = 0; i < MAX_LAYERS; i++) {
                togglebutt[i]->SetSize(x, y, currbuttwd, TOGGLE_HT);
#if defined(__WXOSX_COCOA__) && wxCHECK_VERSION(3,0,0)
                // ensure there is a gap between buttons
                togglebutt[i]->SetSize(x, y, currbuttwd-4, TOGGLE_HT);
#else
                togglebutt[i]->SetSize(x, y, currbuttwd, TOGGLE_HT);
#endif
                x += currbuttwd;
            }
        }
    }
}

// -----------------------------------------------------------------------------

void LayerBar::OnMouseDown(wxMouseEvent& WXUNUSED(event))
{
    // this is NOT called if user clicks a layer bar button;
    // on Windows we need to reset keyboard focus to viewport window
    viewptr->SetFocus();
    
    mainptr->showbanner = false;
    statusptr->ClearMessage();
}

// -----------------------------------------------------------------------------

void LayerBar::OnButton(wxCommandEvent& event)
{
#ifdef __WXMAC__
    // close any open tool tip window (fixes wxMac bug?)
    wxToolTip::RemoveToolTips();
#endif
    
    mainptr->showbanner = false;
    statusptr->ClearMessage();
    
    int id = event.GetId();
    
#ifdef __WXMSW__
    // disconnect focus handler and reset focus to viewptr;
    // we must do latter before button becomes disabled
    if (id < MAX_LAYERS) {
        togglebutt[id]->Disconnect(id, wxEVT_KILL_FOCUS,
                                   wxFocusEventHandler(LayerBar::OnKillFocus));
    } else {
        bitmapbutt[id]->Disconnect(id, wxEVT_KILL_FOCUS,
                                   wxFocusEventHandler(LayerBar::OnKillFocus));
    }
    viewptr->SetFocus();
#endif
    
    switch (id) {
        case ADD_LAYER:         AddLayer(); break;
        case CLONE_LAYER:       CloneLayer(); break;
        case DUPLICATE_LAYER:   DuplicateLayer(); break;
        case DELETE_LAYER:      DeleteLayer(); break;
        case STACK_LAYERS:      ToggleStackLayers(); break;
        case TILE_LAYERS:       ToggleTileLayers(); break;
        default:
            // id < MAX_LAYERS
            if (id == currindex) {
                // make sure toggle button stays in selected state
                togglebutt[id]->SetValue(true);
            } else {
                SetLayer(id);
                if (inscript) {
                    // update window title, viewport and status bar
                    inscript = false;
                    mainptr->SetWindowTitle(wxEmptyString);
                    mainptr->UpdatePatternAndStatus();
                    inscript = true;
                }
            }
    }
    
    // avoid weird bug on Mac where viewport can lose keyboard focus after
    // the user hits DELETE_LAYER button *and* the "All controls" option
    // is ticked in System Prefs > Keyboard & Mouse > Keyboard Shortcuts
    viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void LayerBar::OnKillFocus(wxFocusEvent& event)
{
    int id = event.GetId();
    if (id < MAX_LAYERS) {
        togglebutt[id]->SetFocus();   // don't let button lose focus
    } else {
        bitmapbutt[id]->SetFocus();   // don't let button lose focus
    }
}

// -----------------------------------------------------------------------------

void LayerBar::OnButtonDown(wxMouseEvent& event)
{
    // a layer bar button has been pressed
    int id = event.GetId();
    
    // connect a handler that keeps focus with the pressed button
    if (id < MAX_LAYERS) {
        togglebutt[id]->Connect(id, wxEVT_KILL_FOCUS,
                                wxFocusEventHandler(LayerBar::OnKillFocus));
    } else {
        bitmapbutt[id]->Connect(id, wxEVT_KILL_FOCUS,
                                wxFocusEventHandler(LayerBar::OnKillFocus));
    }
    
    event.Skip();
}

// -----------------------------------------------------------------------------

void LayerBar::OnButtonUp(wxMouseEvent& event)
{
    // a layer bar button has been released
    wxPoint pt;
    int wd, ht;
    int id = event.GetId();
    
    if (id < MAX_LAYERS) {
        pt = togglebutt[id]->ScreenToClient( wxGetMousePosition() );
        togglebutt[id]->GetClientSize(&wd, &ht);
        // disconnect kill-focus handler
        togglebutt[id]->Disconnect(id, wxEVT_KILL_FOCUS,
                                   wxFocusEventHandler(LayerBar::OnKillFocus));
    } else {
        pt = bitmapbutt[id]->ScreenToClient( wxGetMousePosition() );
        bitmapbutt[id]->GetClientSize(&wd, &ht);
        // disconnect kill-focus handler
        bitmapbutt[id]->Disconnect(id, wxEVT_KILL_FOCUS,
                                   wxFocusEventHandler(LayerBar::OnKillFocus));
    }
    
    viewptr->SetFocus();
    
    wxRect r(0, 0, wd, ht);
    if ( r.Contains(pt) ) {
        // call OnButton
        wxCommandEvent buttevt(wxEVT_COMMAND_BUTTON_CLICKED, id);
        if (id < MAX_LAYERS) {
            buttevt.SetEventObject(togglebutt[id]);
            togglebutt[id]->GetEventHandler()->ProcessEvent(buttevt);
        } else {
            buttevt.SetEventObject(bitmapbutt[id]);
            bitmapbutt[id]->GetEventHandler()->ProcessEvent(buttevt);
        }
    }
}

// -----------------------------------------------------------------------------

void LayerBar::AddButton(int id, const wxString& tip)
{
    if (id < MAX_LAYERS) {
        // create toggle button
        int y = (layerbarht - TOGGLE_HT) / 2;
        togglebutt[id] = new wxToggleButton(this, id, wxT("?"), wxPoint(xpos, y),
                                            wxSize(MIN_TOGGLE_WD, TOGGLE_HT) );
        if (togglebutt[id] == NULL) {
            Fatal(_("Failed to create layer bar bitmap button!"));
        } else {
#if defined(__WXGTK__) || defined(__WXOSX_COCOA__)
            // use smaller font on Linux and OS X Cocoa
            togglebutt[id]->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
#endif
            
            // we need to create size using MIN_TOGGLE_WD above and resize now
            // using MAX_TOGGLE_WD, otherwise we can't shrink size later
            // (possibly only needed by wxMac)
#if defined(__WXOSX_COCOA__) && wxCHECK_VERSION(3,0,0)
            // ensure there is a gap between buttons
            togglebutt[id]->SetSize(xpos, y, MAX_TOGGLE_WD-4, TOGGLE_HT);
#else
            togglebutt[id]->SetSize(xpos, y, MAX_TOGGLE_WD, TOGGLE_HT);
#endif
            
            xpos += MAX_TOGGLE_WD;
            
            togglebutt[id]->SetToolTip(SWITCH_LAYER);
            
#ifdef __WXMSW__
            // fix problem with layer bar buttons when generating/inscript
            // due to focus being changed to viewptr
            togglebutt[id]->Connect(id, wxEVT_LEFT_DOWN,
                                    wxMouseEventHandler(LayerBar::OnButtonDown));
            togglebutt[id]->Connect(id, wxEVT_LEFT_UP,
                                    wxMouseEventHandler(LayerBar::OnButtonUp));
#endif
        }
        
    } else {
        // create bitmap button
        bitmapbutt[id] = new wxBitmapButton(this, id, normbutt[id], wxPoint(xpos,ypos),
#if defined(__WXOSX_COCOA__) && wxCHECK_VERSION(3,0,0)
                                            wxSize(BUTTON_WD, BUTTON_HT), wxBORDER_SIMPLE
#else
                                            wxSize(BUTTON_WD, BUTTON_HT)
#endif
                                            );
        if (bitmapbutt[id] == NULL) {
            Fatal(_("Failed to create layer bar bitmap button!"));
        } else {
            xpos += BUTTON_WD + smallgap;
            
            bitmapbutt[id]->SetToolTip(tip);
            
#ifdef __WXMSW__
            // fix problem with layer bar buttons when generating/inscript
            // due to focus being changed to viewptr
            bitmapbutt[id]->Connect(id, wxEVT_LEFT_DOWN,
                                    wxMouseEventHandler(LayerBar::OnButtonDown));
            bitmapbutt[id]->Connect(id, wxEVT_LEFT_UP,
                                    wxMouseEventHandler(LayerBar::OnButtonUp));
#endif
        }
    }
}

// -----------------------------------------------------------------------------

void LayerBar::AddSeparator()
{
    xpos += biggap - smallgap;
}

// -----------------------------------------------------------------------------

void LayerBar::EnableButton(int id, bool enable)
{
    if (id < MAX_LAYERS) {
        // toggle button
        if (enable == togglebutt[id]->IsEnabled()) return;
        
        togglebutt[id]->Enable(enable);
        
    } else {
        // bitmap button
        if (enable == bitmapbutt[id]->IsEnabled()) return;
        
#ifdef __WXMSW__
        if (id == STACK_LAYERS && stacklayers) {
            bitmapbutt[id]->SetBitmapDisabled(disdownbutt[id]);
            
        } else if (id == TILE_LAYERS && tilelayers) {
            bitmapbutt[id]->SetBitmapDisabled(disdownbutt[id]);
            
        } else {
            bitmapbutt[id]->SetBitmapDisabled(disnormbutt[id]);
        }
#endif
        
        bitmapbutt[id]->Enable(enable);
    }
}

// -----------------------------------------------------------------------------

void LayerBar::SelectButton(int id, bool select)
{
    if (id < MAX_LAYERS) {
        // toggle button
        if (select) {
            if (downid >= LAYER_0) {
                // deselect old layer button
                togglebutt[downid]->SetValue(false);      
                togglebutt[downid]->SetToolTip(SWITCH_LAYER);
            }
            downid = id;
            togglebutt[id]->SetToolTip(_("Current layer"));
        }
        togglebutt[id]->SetValue(select);
        
    } else {
        // bitmap button
        if (select) {
            bitmapbutt[id]->SetBitmapLabel(downbutt[id]);
        } else {
            bitmapbutt[id]->SetBitmapLabel(normbutt[id]);
        }
        if (showlayer) bitmapbutt[id]->Refresh(false);
    }
}

// -----------------------------------------------------------------------------

void CreateLayerBar(wxWindow* parent)
{
    int wd, ht;
    parent->GetClientSize(&wd, &ht);
    
    layerbarptr = new LayerBar(parent, 0, 0, wd, layerbarht);
    if (layerbarptr == NULL) Fatal(_("Failed to create layer bar!"));
    
    // create bitmap buttons
    layerbarptr->AddButton(ADD_LAYER,         _("Add new layer"));
    layerbarptr->AddButton(CLONE_LAYER,       _("Clone current layer"));
    layerbarptr->AddButton(DUPLICATE_LAYER,   _("Duplicate current layer"));
    layerbarptr->AddButton(DELETE_LAYER,      _("Delete current layer"));
    layerbarptr->AddSeparator();
    layerbarptr->AddButton(STACK_LAYERS,      _("Stack layers"));
    layerbarptr->AddButton(TILE_LAYERS,       _("Tile layers"));
    layerbarptr->AddSeparator();
    
    // create a toggle button for each layer
    for (int i = 0; i < MAX_LAYERS; i++) {
        layerbarptr->AddButton(i, wxEmptyString);
    }
    
    // hide all toggle buttons except for layer 0
    for (int i = 1; i < MAX_LAYERS; i++) togglebutt[i]->Show(false);
    
    // select STACK_LAYERS or TILE_LAYERS if necessary
    if (stacklayers) layerbarptr->SelectButton(STACK_LAYERS, true);
    if (tilelayers) layerbarptr->SelectButton(TILE_LAYERS, true);
    
    // select LAYER_0 button
    layerbarptr->SelectButton(LAYER_0, true);
    
    layerbarptr->Show(showlayer);
}

// -----------------------------------------------------------------------------

int LayerBarHeight() {
    return (showlayer ? layerbarht : 0);
}

// -----------------------------------------------------------------------------

void ResizeLayerBar(int wd)
{
    if (layerbarptr && showlayer) {
        layerbarptr->SetSize(wd, layerbarht);
    }
}

// -----------------------------------------------------------------------------

void UpdateLayerBar()
{
    if (layerbarptr && showlayer && !mainptr->IsIconized()) {
        bool active = !viewptr->waitingforclick;
        
        layerbarptr->EnableButton(ADD_LAYER,         active && !inscript && numlayers < MAX_LAYERS);
        layerbarptr->EnableButton(CLONE_LAYER,       active && !inscript && numlayers < MAX_LAYERS);
        layerbarptr->EnableButton(DUPLICATE_LAYER,   active && !inscript && numlayers < MAX_LAYERS);
        layerbarptr->EnableButton(DELETE_LAYER,      active && !inscript && numlayers > 1);
        layerbarptr->EnableButton(STACK_LAYERS,      active);
        layerbarptr->EnableButton(TILE_LAYERS,       active);
        for (int i = 0; i < numlayers; i++) {
            layerbarptr->EnableButton(i, active && CanSwitchLayer(i));
        }
        
        // no need to redraw entire bar here if it only contains buttons
        // layerbarptr->Refresh(false);
    }
}

// -----------------------------------------------------------------------------

void UpdateLayerButton(int index, const wxString& name)
{
    // assume caller has replaced any "&" with "&&"
    togglebutt[index]->SetLabel(name);
}

// -----------------------------------------------------------------------------

void RedrawLayerBar()
{
    if (layerbarptr && showlayer && !mainptr->IsIconized()) {
        layerbarptr->Refresh(false);
        #ifdef __WXGTK__
            // avoid bug that can cause buttons to lose their bitmaps
            layerbarptr->Update();
        #endif
    }
}

// -----------------------------------------------------------------------------

void ToggleLayerBar()
{
    showlayer = !showlayer;
    
    if (showlayer) {
        ShiftEditBar(layerbarht);     // move edit bar down
    } else {
        // hide layer bar
        ShiftEditBar(-layerbarht);    // move edit bar up
    }
    
    mainptr->ResizeBigView();
    layerbarptr->Show(showlayer);    // needed on Windows
    mainptr->UpdateMenuItems();
}

// -----------------------------------------------------------------------------

void CalculateTileRects(int bigwd, int bight)
{
    // set tilerect in each layer
    wxRect r;
    bool portrait = (bigwd <= bight);
    int rows, cols;
    
    // try to avoid the aspect ratio of each tile becoming too large
    switch (numlayers) {
        case 4: rows = 2; cols = 2; break;
        case 9: rows = 3; cols = 3; break;
            
        case 3: case 5: case 7:
            rows = portrait ? numlayers / 2 + 1 : 2;
            cols = portrait ? 2 : numlayers / 2 + 1;
            break;
            
        case 6: case 8: case 10:
            rows = portrait ? numlayers / 2 : 2;
            cols = portrait ? 2 : numlayers / 2;
            break;
            
        default:
            // numlayers == 2 or > 10
            rows = portrait ? numlayers : 1;
            cols = portrait ? 1 : numlayers;
    }
    
    int tilewd = bigwd / cols;
    int tileht = bight / rows;
    if ( float(tilewd) > float(tileht) * 2.5 ) {
        rows = 1;
        cols = numlayers;
        tileht = bight;
        tilewd = bigwd / numlayers;
    } else if ( float(tileht) > float(tilewd) * 2.5 ) {
        cols = 1;
        rows = numlayers;
        tilewd = bigwd;
        tileht = bight / numlayers;
    }
    
    for ( int i = 0; i < rows; i++ ) {
        for ( int j = 0; j < cols; j++ ) {
            r.x = j * tilewd;
            r.y = i * tileht;
            r.width = tilewd;
            r.height = tileht;
            if (i == rows - 1) {
                // may need to increase height of bottom-edge tile
                r.height += bight - (rows * tileht);
            }
            if (j == cols - 1) {
                // may need to increase width of right-edge tile
                r.width += bigwd - (cols * tilewd);
            }
            int index = i * cols + j;
            if (index == numlayers) {
                // numlayers == 3,5,7
                layer[index - 1]->tilerect.width += r.width;
            } else {
                layer[index]->tilerect = r;
            }
        }
    }
    
    if (tileborder > 0) {
        // make tilerects smaller to allow for equal-width tile borders
        int border = tileborder * int(scalefactor);
        for ( int i = 0; i < rows; i++ ) {
            for ( int j = 0; j < cols; j++ ) {
                int index = i * cols + j;
                if (index == numlayers) {
                    // numlayers == 3,5,7
                    layer[index - 1]->tilerect.width -= border;
                } else {
                    layer[index]->tilerect.x += border;
                    layer[index]->tilerect.y += border;
                    layer[index]->tilerect.width -= border;
                    layer[index]->tilerect.height -= border;
                    if (j == cols - 1) layer[index]->tilerect.width -= border;
                    if (i == rows - 1) layer[index]->tilerect.height -= border;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ResizeTiles(int bigwd, int bight)
{
    // set tilerect for each layer so they tile bigview's client area
    CalculateTileRects(bigwd, bight);
    
    // set size of each tile window
    for ( int i = 0; i < numlayers; i++ ) {
        if (layer[i]->tilerect.width < 0) layer[i]->tilerect.width = 0;
        if (layer[i]->tilerect.height < 0) layer[i]->tilerect.height = 0;
        layer[i]->tilewin->SetSize( layer[i]->tilerect.x / int(scalefactor),
                                    layer[i]->tilerect.y / int(scalefactor),
                                    layer[i]->tilerect.width / int(scalefactor),
                                    layer[i]->tilerect.height / int(scalefactor) );
    }
    
    // set viewport size for each tile; this is currently the same as the
    // tilerect size because tile windows are created with wxNO_BORDER
    for ( int i = 0; i < numlayers; i++ ) {
        int wd, ht;
        layer[i]->tilewin->GetClientSize(&wd, &ht);
        wd = wd * int(scalefactor);
        ht = ht * int(scalefactor);
        // wd or ht might be < 1 on Windows
        if (wd < 1) wd = 1;
        if (ht < 1) ht = 1;
        layer[i]->view->resize(wd, ht);
    }
}

// -----------------------------------------------------------------------------

void ResizeLayers(int wd, int ht)
{
    // this is called whenever the size of the bigview window changes;
    // wd and ht are the dimensions of bigview's client area
    if (tilelayers && numlayers > 1) {
        ResizeTiles(wd, ht);
    } else {
        // resize viewport in each layer to bigview's client area
        for (int i = 0; i < numlayers; i++) {
            layer[i]->view->resize(wd, ht);
        }
    }
}

// -----------------------------------------------------------------------------

void CreateTiles()
{
    // create tile windows
    for ( int i = 0; i < numlayers; i++ ) {
        layer[i]->tilewin = new PatternView(bigview,
            // correct size will be set below by ResizeTiles
            0, 0, 0, 0,
            wxNO_BORDER | wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS);
        if (layer[i]->tilewin == NULL) Fatal(_("Failed to create tile window!"));
        
        // set tileindex >= 0; this must always match the layer index, so we'll need to
        // destroy and recreate all tiles whenever a tile is added, deleted or moved
        layer[i]->tilewin->tileindex = i;
        
#if wxUSE_DRAG_AND_DROP
        // let user drop file onto any tile (but file will be loaded into current tile)
        layer[i]->tilewin->SetDropTarget(mainptr->NewDropTarget());
#endif
    }
    
    // init tilerects, tile window sizes and their viewport sizes
    int wd, ht;
    bigview->GetClientSize(&wd, &ht);
    wd = wd * int(scalefactor);
    ht = ht * int(scalefactor);
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    ResizeTiles(wd, ht);
    
    // change viewptr to tile window for current layer
    viewptr = currlayer->tilewin;
    if (mainptr->infront) viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void DestroyTiles()
{
    // reset viewptr to main viewport window
    viewptr = bigview;
    if (mainptr->infront) viewptr->SetFocus();
    
    // destroy all tile windows
    for ( int i = 0; i < numlayers; i++ ) {
        delete layer[i]->tilewin;
    }
    
    // resize viewport in each layer to bigview's client area
    int wd, ht;
    bigview->GetClientSize(&wd, &ht);
    wd = wd * int(scalefactor);
    ht = ht * int(scalefactor);
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    for ( int i = 0; i < numlayers; i++ ) {
        layer[i]->view->resize(wd, ht);
    }
}

// -----------------------------------------------------------------------------

void SyncClones()
{
    if (numclones == 0) return;
    
    if (currlayer->cloneid > 0) {
        // make sure clone algo and most other settings are synchronized
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = layer[i];
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                // universe might have been re-created, or algorithm changed
                cloneptr->algo = currlayer->algo;
                cloneptr->algtype = currlayer->algtype;
                cloneptr->rule = currlayer->rule;
                
                // no need to sync undo/redo history
                // cloneptr->undoredo = currlayer->undoredo;
                
                // along with view, don't sync these settings
                // cloneptr->autofit = currlayer->autofit;
                // cloneptr->hyperspeed = currlayer->hyperspeed;
                // cloneptr->showhashinfo = currlayer->showhashinfo;
                // cloneptr->drawingstate = currlayer->drawingstate;
                // cloneptr->curs = currlayer->curs;
                // cloneptr->originx = currlayer->originx;
                // cloneptr->originy = currlayer->originy;
                // cloneptr->currname = currlayer->currname;
                
                // sync various flags
                cloneptr->dirty = currlayer->dirty;
                cloneptr->savedirty = currlayer->savedirty;
                cloneptr->stayclean = currlayer->stayclean;
                
                // sync step size
                cloneptr->currbase = currlayer->currbase;
                cloneptr->currexpo = currlayer->currexpo;
                
                // sync selection info
                cloneptr->currsel = currlayer->currsel;
                cloneptr->savesel = currlayer->savesel;
                
                // sync the stuff needed to reset pattern
                cloneptr->startalgo = currlayer->startalgo;
                cloneptr->savestart = currlayer->savestart;
                cloneptr->startdirty = currlayer->startdirty;
                cloneptr->startrule = currlayer->startrule;
                cloneptr->startgen = currlayer->startgen;
                cloneptr->startsel = currlayer->startsel;
                cloneptr->startfile = currlayer->startfile;
                cloneptr->currfile = currlayer->currfile;
                // clone can have different starting name, pos, scale, step
                // cloneptr->startname = currlayer->startname;
                // cloneptr->startx = currlayer->startx;
                // cloneptr->starty = currlayer->starty;
                // cloneptr->startmag = currlayer->startmag;
                // cloneptr->startbase = currlayer->startbase;
                // cloneptr->startexpo = currlayer->startexpo;
                
                // sync timeline settings
                cloneptr->currframe = currlayer->currframe;
                cloneptr->autoplay = currlayer->autoplay;
                cloneptr->tlspeed = currlayer->tlspeed;
            }
        }
    }
}

// -----------------------------------------------------------------------------

void SaveLayerSettings()
{
    // set oldalgo and oldrule for use in CurrentLayerChanged
    oldalgo = currlayer->algtype;
    oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    
    // we're about to change layer so remember current rule
    // in case we switch back to this layer
    currlayer->rule = oldrule;
    
    // synchronize clone info (do AFTER setting currlayer->rule)
    SyncClones();
    
    if (syncviews) {
        // save scale and location for use in CurrentLayerChanged
        oldmag = currlayer->view->getmag();
        oldx = currlayer->view->x;
        oldy = currlayer->view->y;
    }
    
    if (synccursors) {
        // save cursor mode for use in CurrentLayerChanged
        oldcurs = currlayer->curs;
    }
}

// -----------------------------------------------------------------------------

bool RestoreRule(const wxString& rule)
{
    const char* err = currlayer->algo->setrule( rule.mb_str(wxConvLocal) );
    if (err) {
        // this can happen if a .rule file was deleted
        // or it was edited and some sort of error introduced, so best to
        // use algo's default rule (which should never fail)
        currlayer->algo->setrule( currlayer->algo->DefaultRule() );
        wxString msg = _("The rule \"") + rule;
        msg += _("\" is no longer valid!\nUsing the default rule instead.");
        Warning(msg);
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------

void CurrentLayerChanged()
{
    // currlayer has changed since SaveLayerSettings was called;
    // update rule if the new currlayer uses a different algorithm or rule
    if ( currlayer->algtype != oldalgo || !currlayer->rule.IsSameAs(oldrule,false) ) {
        RestoreRule(currlayer->rule);
    }
    
    if (syncviews) currlayer->view->setpositionmag(oldx, oldy, oldmag);
    if (synccursors) currlayer->curs = oldcurs;
    
    // select current layer button (also deselects old button)
    layerbarptr->SelectButton(currindex, true);
    
    if (tilelayers && numlayers > 1) {
        // switch to new tile
        viewptr = currlayer->tilewin;
        if (mainptr->infront) viewptr->SetFocus();
    }
    
    if (allowundo) {
        // update Undo/Redo items so they show the correct action
        currlayer->undoredo->UpdateUndoRedoItems();
    } else {
        // undo/redo is disabled so clear history;
        // this also removes action from Undo/Redo items
        currlayer->undoredo->ClearUndoRedo();
    }
    
    mainptr->SetStepExponent(currlayer->currexpo);
    // SetStepExponent calls SetGenIncrement
    mainptr->SetWindowTitle(currlayer->currname);
    
    mainptr->UpdateUserInterface();
    mainptr->UpdatePatternAndStatus();
    bigview->UpdateScrollBars();
}

// -----------------------------------------------------------------------------

void UpdateLayerNames()
{
    // update names in all layer items at end of Layer menu
    for (int i = 0; i < numlayers; i++)
        mainptr->UpdateLayerItem(i);
}

// -----------------------------------------------------------------------------

static wxBitmap** CopyIcons(wxBitmap** srcicons, int maxstate)
{
    wxBitmap** iconptr = (wxBitmap**) malloc(256 * sizeof(wxBitmap*));
    if (iconptr) {
        for (int i = 0; i < 256; i++) iconptr[i] = NULL;
        for (int i = 0; i <= maxstate; i++) {
            if (srcicons && srcicons[i]) {
                wxRect rect(0, 0, srcicons[i]->GetWidth(), srcicons[i]->GetHeight());
                iconptr[i] = new wxBitmap(srcicons[i]->GetSubBitmap(rect));
            }
        }
    }
    return iconptr;
}

// -----------------------------------------------------------------------------

static void CopyBuiltinIcons(wxBitmap** i7x7, wxBitmap** i15x15, wxBitmap** i31x31)
{
    int maxstate = currlayer->algo->NumCellStates() - 1;

    if (currlayer->icons7x7) FreeIconBitmaps(currlayer->icons7x7);
    if (currlayer->icons15x15) FreeIconBitmaps(currlayer->icons15x15);
    if (currlayer->icons31x31) FreeIconBitmaps(currlayer->icons31x31);

    currlayer->icons7x7 = CopyIcons(i7x7, maxstate);
    currlayer->icons15x15 = CopyIcons(i15x15, maxstate);
    currlayer->icons31x31 = CopyIcons(i31x31, maxstate);
}

// -----------------------------------------------------------------------------

static unsigned char* CreateIconAtlas(wxBitmap** srcicons, int iconsize)
{
    bool multicolor = currlayer->multicoloricons;
    
    unsigned char deadr = currlayer->cellr[0];
    unsigned char deadg = currlayer->cellg[0];
    unsigned char deadb = currlayer->cellb[0];
    if (swapcolors) {
        deadr = 255 - deadr;
        deadg = 255 - deadg;
        deadb = 255 - deadb;
    }

    // allocate enough memory for texture atlas to store RGBA pixels for a row of icons
    // (note that we use calloc so all alpha bytes are initially 0)
    int rowbytes = currlayer->numicons * iconsize * 4;
    unsigned char* atlasptr = (unsigned char*) calloc(rowbytes * iconsize, 1);
    
    if (atlasptr) {
        for (int state = 1; state <= currlayer->numicons; state++) {
            if (srcicons && srcicons[state]) {
                int wd = srcicons[state]->GetWidth();       // should be iconsize - 1
                int ht = srcicons[state]->GetHeight();      // ditto
                
                wxAlphaPixelData icondata(*srcicons[state]);
                if (icondata) {
                    wxAlphaPixelData::Iterator iconpxl(icondata);
                    
                    unsigned char liver = currlayer->cellr[state];
                    unsigned char liveg = currlayer->cellg[state];
                    unsigned char liveb = currlayer->cellb[state];
                    if (swapcolors) {
                        liver = 255 - liver;
                        liveg = 255 - liveg;
                        liveb = 255 - liveb;
                    }
                    
                    // start at top left byte of icon
                    int tpos = (state-1) * iconsize * 4;
                    for (int row = 0; row < ht; row++) {
                        wxAlphaPixelData::Iterator iconrow = iconpxl;
                        int rowstart = tpos;
                        for (int col = 0; col < wd; col++) {
                            unsigned char r = iconpxl.Red();
                            unsigned char g = iconpxl.Green();
                            unsigned char b = iconpxl.Blue();
                            if (r || g || b) {
                                // non-black pixel
                                if (multicolor) {
                                    // use non-black pixel in multi-colored icon
                                    if (swapcolors) {
                                        atlasptr[tpos]   = 255 - r;
                                        atlasptr[tpos+1] = 255 - g;
                                        atlasptr[tpos+2] = 255 - b;
                                    } else {
                                        atlasptr[tpos]   = r;
                                        atlasptr[tpos+1] = g;
                                        atlasptr[tpos+2] = b;
                                    }
                                } else {
                                    // grayscale icon (r = g = b)
                                    if (r == 255) {
                                        // replace white pixel with live cell color
                                        atlasptr[tpos]   = liver;
                                        atlasptr[tpos+1] = liveg;
                                        atlasptr[tpos+2] = liveb;
                                    } else {
                                        // replace gray pixel with appropriate shade between
                                        // live and dead cell colors
                                        float frac = (float)r / 255.0;
                                        atlasptr[tpos]   = (int)(deadr + frac * (liver - deadr) + 0.5);
                                        atlasptr[tpos+1] = (int)(deadg + frac * (liveg - deadg) + 0.5);
                                        atlasptr[tpos+2] = (int)(deadb + frac * (liveb - deadb) + 0.5);
                                    }
                                }
                                atlasptr[tpos+3] = 255;     // alpha channel is opaque
                            }
                            // move to next pixel
                            tpos += 4;
                            iconpxl++;
                        }
                        // move to next row
                        tpos = rowstart + rowbytes;
                        iconpxl = iconrow;
                        iconpxl.OffsetY(icondata, 1);
                    }
                }
            }
        }
    }
    return atlasptr;
}

// -----------------------------------------------------------------------------

Layer* CreateTemporaryLayer()
{
    Layer* templayer = new Layer();
    if (templayer == NULL) Warning(_("Failed to create temporary layer!"));
    return templayer;
}

// -----------------------------------------------------------------------------

void AddLayer()
{
    if (numlayers >= MAX_LAYERS) return;
    
    // we need to test mainptr here because AddLayer is called from main window's ctor
    if (mainptr && mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_ADD_LAYER);
        mainptr->Stop();
        return;
    }
    
    if (numlayers == 0) {
        // creating the very first layer
        currindex = 0;
    } else {
        if (tilelayers && numlayers > 1) DestroyTiles();
        
        SaveLayerSettings();
        
        // insert new layer after currindex
        currindex++;
        if (currindex < numlayers) {
            // shift right one or more layers
            for (int i = numlayers; i > currindex; i--)
                layer[i] = layer[i-1];
        }
    }
    
    Layer* oldlayer = NULL;
    if (cloning || duplicating) oldlayer = currlayer;
    
    currlayer = new Layer();
    if (currlayer == NULL) Fatal(_("Failed to create new layer!"));
    layer[currindex] = currlayer;
    
    if (cloning || duplicating) {
        // copy old layer's colors to new layer
        currlayer->fromrgb = oldlayer->fromrgb;
        currlayer->torgb = oldlayer->torgb;
        currlayer->multicoloricons = oldlayer->multicoloricons;
        currlayer->numicons = oldlayer->numicons;
        for (int n = 0; n <= currlayer->numicons; n++) {
            currlayer->cellr[n] = oldlayer->cellr[n];
            currlayer->cellg[n] = oldlayer->cellg[n];
            currlayer->cellb[n] = oldlayer->cellb[n];
        }
        if (cloning) {
            // use same icon pointers
            currlayer->icons7x7 = oldlayer->icons7x7;
            currlayer->icons15x15 = oldlayer->icons15x15;
            currlayer->icons31x31 = oldlayer->icons31x31;
            // use same atlas
            currlayer->atlas7x7 = oldlayer->atlas7x7;
            currlayer->atlas15x15 = oldlayer->atlas15x15;
            currlayer->atlas31x31 = oldlayer->atlas31x31;
        } else {
            // duplicate icons from old layer
            currlayer->icons7x7 = CopyIcons(oldlayer->icons7x7, currlayer->numicons);
            currlayer->icons15x15 = CopyIcons(oldlayer->icons15x15, currlayer->numicons);
            currlayer->icons31x31 = CopyIcons(oldlayer->icons31x31, currlayer->numicons);
            // create icon texture atlases
            currlayer->atlas7x7 = CreateIconAtlas(oldlayer->icons7x7, 8);
            currlayer->atlas15x15 = CreateIconAtlas(oldlayer->icons15x15, 16);
            currlayer->atlas31x31 = CreateIconAtlas(oldlayer->icons31x31, 32);
        }
    } else {
        // set new layer's colors+icons to default colors+icons for current algo+rule
        UpdateLayerColors();
    }
    
    numlayers++;
    
    if (numlayers > 1) {
        // add toggle button at end of layer bar
        layerbarptr->ResizeLayerButtons();
        togglebutt[numlayers-1]->Show(true);
        
        // add another item at end of Layer menu
        mainptr->AppendLayerItem();
        
        UpdateLayerNames();
        
        if (tilelayers && numlayers > 1) CreateTiles();
        
        CurrentLayerChanged();
    }
}

// -----------------------------------------------------------------------------

void CloneLayer()
{
    if (numlayers >= MAX_LAYERS) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_CLONE);
        mainptr->Stop();
        return;
    }
    
    cloning = true;
    AddLayer();
    cloning = false;
}

// -----------------------------------------------------------------------------

void DuplicateLayer()
{
    if (numlayers >= MAX_LAYERS) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_DUPLICATE);
        mainptr->Stop();
        return;
    }
    
    duplicating = true;
    AddLayer();
    duplicating = false;
}

// -----------------------------------------------------------------------------

void DeleteLayer()
{
    if (numlayers <= 1) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_DEL_LAYER);
        mainptr->Stop();
        return;
    }
    
    // note that we don't need to ask to delete a clone
    if (askondelete && currlayer->dirty && currlayer->cloneid == 0 && !mainptr->SaveCurrentLayer()) return;
    
    // numlayers > 1
    if (tilelayers) DestroyTiles();
    
    SaveLayerSettings();
    
    delete currlayer;
    numlayers--;
    
    if (currindex < numlayers) {
        // shift left one or more layers
        for (int i = currindex; i < numlayers; i++)
            layer[i] = layer[i+1];
    }
    if (currindex > 0) currindex--;
    currlayer = layer[currindex];
    
    // remove toggle button at end of layer bar
    togglebutt[numlayers]->Show(false);
    layerbarptr->ResizeLayerButtons();
    
    // remove item from end of Layer menu
    mainptr->RemoveLayerItem();
    
    UpdateLayerNames();
    
    if (tilelayers && numlayers > 1) CreateTiles();
    
    CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void DeleteOtherLayers()
{
    if (inscript || numlayers <= 1) return;
    
    if (askondelete) {
        // keep track of which unique clones have been seen;
        // we add 1 below to allow for cloneseen[0] (always false)
        const int maxseen = MAX_LAYERS/2 + 1;
        bool cloneseen[maxseen];
        for (int i = 0; i < maxseen; i++) cloneseen[i] = false;
        
        // for each dirty layer, except current layer and all of its clones,
        // ask user if they want to save changes
        int cid = layer[currindex]->cloneid;
        if (cid > 0) cloneseen[cid] = true;
        int oldindex = currindex;
        for (int i = 0; i < numlayers; i++) {
            // only ask once for each unique clone (cloneid == 0 for non-clone)
            cid = layer[i]->cloneid;
            if (i != oldindex && !cloneseen[cid]) {
                if (cid > 0) cloneseen[cid] = true;
                if (layer[i]->dirty) {
                    // temporarily turn off generating flag for SetLayer
                    bool oldgen = mainptr->generating;
                    mainptr->generating = false;
                    SetLayer(i);
                    if (!mainptr->SaveCurrentLayer()) {
                        // user hit Cancel so restore current layer and generating flag
                        SetLayer(oldindex);
                        mainptr->generating = oldgen;
                        mainptr->UpdateUserInterface();
                        return;
                    }
                    SetLayer(oldindex);
                    mainptr->generating = oldgen;
                }
            }
        }
    }
    
    // numlayers > 1
    if (tilelayers) DestroyTiles();
    
    SyncClones();
    
    // delete all layers except current layer;
    // we need to do this carefully because ~Layer() requires numlayers
    // and the layer array to be correct when deleting a cloned layer
    int i = numlayers;
    while (numlayers > 1) {
        i--;
        if (i != currindex) {
            delete layer[i];     // ~Layer() is called
            numlayers--;
            
            // may need to shift the current layer left one place
            if (i < numlayers) layer[i] = layer[i+1];
            
            // remove toggle button at end of layer bar
            togglebutt[numlayers]->Show(false);
            
            // remove item from end of Layer menu
            mainptr->RemoveLayerItem();
        }
    }
    
    layerbarptr->ResizeLayerButtons();
    
    currindex = 0;
    // currlayer doesn't change
    
    // update the only layer item
    mainptr->UpdateLayerItem(0);
    
    // update window title (may need to remove "=" prefix)
    mainptr->SetWindowTitle(wxEmptyString);
    
    // select LAYER_0 button (also deselects old button)
    layerbarptr->SelectButton(LAYER_0, true);
    
    mainptr->UpdateMenuItems();
    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void SetLayer(int index)
{
    if (currindex == index) return;
    if (index < 0 || index >= numlayers) return;
    
    if (inscript) {
        // always allow a script to switch layers
    } else if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_LAYER0 + index);
        mainptr->Stop();
        return;
    }
    
    SaveLayerSettings();
    currindex = index;
    currlayer = layer[currindex];
    CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

bool CanSwitchLayer(int WXUNUSED(index))
{
    if (inscript) {
        // user can only switch layers if script has set the appropriate option
        return canswitch;
    } else {
        // user can switch to any layer
        return true;
    }
}

// -----------------------------------------------------------------------------

void SwitchToClickedTile(int index)
{
    if (inscript && !CanSwitchLayer(index)) {
        // statusptr->ErrorMessage does nothing if inscript is true
        Warning(_("You cannot switch to another layer while this script is running."));
        return;
    }
    
    // switch current layer to clicked tile
    SetLayer(index);
    
    if (inscript) {
        // update window title, viewport and status bar
        inscript = false;
        mainptr->SetWindowTitle(wxEmptyString);
        mainptr->UpdatePatternAndStatus();
        inscript = true;
    }
}

// -----------------------------------------------------------------------------

void MoveLayer(int fromindex, int toindex)
{
    if (fromindex == toindex) return;
    if (fromindex < 0 || fromindex >= numlayers) return;
    if (toindex < 0 || toindex >= numlayers) return;
    
    SaveLayerSettings();
    
    if (fromindex > toindex) {
        Layer* savelayer = layer[fromindex];
        for (int i = fromindex; i > toindex; i--) layer[i] = layer[i - 1];
        layer[toindex] = savelayer;
    } else {
        // fromindex < toindex
        Layer* savelayer = layer[fromindex];
        for (int i = fromindex; i < toindex; i++) layer[i] = layer[i + 1];
        layer[toindex] = savelayer;
    }
    
    currindex = toindex;
    currlayer = layer[currindex];
    
    UpdateLayerNames();
    
    if (tilelayers && numlayers > 1) {
        DestroyTiles();
        CreateTiles();
    }
    
    CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void MoveLayerDialog()
{
    if (inscript || numlayers <= 1) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_MOVE_LAYER);
        mainptr->Stop();
        return;
    }
    
    wxString msg = _("Move the current layer to a new position:");
    if (currindex > 0) {
        msg += _("\n(enter 0 to make it the first layer)");
    }
    
    int newindex;
    if ( GetInteger(_("Move Layer"), msg,
                    currindex, 0, numlayers - 1, &newindex) ) {
        MoveLayer(currindex, newindex);
    }
}

// -----------------------------------------------------------------------------

void NameLayerDialog()
{
    if (inscript) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_NAME_LAYER);
        mainptr->Stop();
        return;
    }
    
    wxString oldname = currlayer->currname;
    wxString newname;
    if ( GetString(_("Name Layer"), _("Enter a new name for the current layer:"), oldname, newname) &&
        !newname.IsEmpty() && oldname != newname ) {
        
        // inscript is false so no need to call SavePendingChanges
        // if (allowundo) SavePendingChanges();
        
        // show new name in main window's title bar;
        // also sets currlayer->currname and updates menu item
        mainptr->SetWindowTitle(newname);
        
        if (allowundo) {
            // note that startfile and savestart/dirty flags don't change here
            currlayer->undoredo->RememberNameChange(oldname, currlayer->startfile,
                                                    currlayer->savestart, currlayer->dirty);
        }
    }
}

// -----------------------------------------------------------------------------

void MarkLayerDirty()
{
    // need to save starting pattern
    currlayer->savestart = true;
    
    // if script has reset dirty flag then don't change it; this makes sense
    // for scripts that call new() and then construct a pattern
    if (currlayer->stayclean) return;
    
    if (!currlayer->dirty) {
        currlayer->dirty = true;
        
        // pass in currname so UpdateLayerItem(currindex) gets called
        mainptr->SetWindowTitle(currlayer->currname);
        
        if (currlayer->cloneid > 0) {
            // synchronize other clones
            for ( int i = 0; i < numlayers; i++ ) {
                Layer* cloneptr = layer[i];
                if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                    // set dirty flag and display asterisk in layer item
                    cloneptr->dirty = true;
                    mainptr->UpdateLayerItem(i);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void MarkLayerClean(const wxString& title)
{
    currlayer->dirty = false;
    
    // if script is resetting dirty flag -- eg. via new() -- then don't allow
    // dirty flag to be set true for the remainder of the script; this is
    // nicer for scripts that construct a pattern (ie. running such a script
    // is equivalent to loading a pattern file)
    if (inscript) currlayer->stayclean = true;
    
    if (title.IsEmpty()) {
        // pass in currname so UpdateLayerItem(currindex) gets called
        mainptr->SetWindowTitle(currlayer->currname);
    } else {
        // set currlayer->currname to title and call UpdateLayerItem(currindex)
        mainptr->SetWindowTitle(title);
    }
    
    if (currlayer->cloneid > 0) {
        // synchronize other clones
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = layer[i];
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                // reset dirty flag
                cloneptr->dirty = false;
                if (inscript) cloneptr->stayclean = true;
                
                // always allow clones to have different names
                // cloneptr->currname = currlayer->currname;
                
                // remove asterisk from layer item
                mainptr->UpdateLayerItem(i);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ToggleSyncViews()
{
    syncviews = !syncviews;
    
    mainptr->UpdateUserInterface();
    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleSyncCursors()
{
    synccursors = !synccursors;
    
    mainptr->UpdateUserInterface();
    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleStackLayers()
{
    stacklayers = !stacklayers;
    if (stacklayers && tilelayers) {
        tilelayers = false;
        layerbarptr->SelectButton(TILE_LAYERS, false);
        if (numlayers > 1) DestroyTiles();
    }
    layerbarptr->SelectButton(STACK_LAYERS, stacklayers);
    
    mainptr->UpdateUserInterface();
    if (inscript) {
        // always update viewport and status bar
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = true;
    } else {
        mainptr->UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

void ToggleTileLayers()
{
    tilelayers = !tilelayers;
    if (tilelayers && stacklayers) {
        stacklayers = false;
        layerbarptr->SelectButton(STACK_LAYERS, false);
    }
    layerbarptr->SelectButton(TILE_LAYERS, tilelayers);
    
    if (tilelayers) {
        if (numlayers > 1) CreateTiles();
    } else {
        if (numlayers > 1) DestroyTiles();
    }
    
    mainptr->UpdateUserInterface();
    if (inscript) {
        // always update viewport and status bar
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = true;
    } else {
        mainptr->UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

void CreateColorGradient()
{
    int maxstate = currlayer->algo->NumCellStates() - 1;
    unsigned char r1 = currlayer->fromrgb.Red();
    unsigned char g1 = currlayer->fromrgb.Green();
    unsigned char b1 = currlayer->fromrgb.Blue();
    unsigned char r2 = currlayer->torgb.Red();
    unsigned char g2 = currlayer->torgb.Green();
    unsigned char b2 = currlayer->torgb.Blue();
    
    // set cell colors for states 1..maxstate using a color gradient
    // starting with r1,g1,b1 and ending with r2,g2,b2
    currlayer->cellr[1] = r1;
    currlayer->cellg[1] = g1;
    currlayer->cellb[1] = b1;
    if (maxstate > 2) {
        int N = maxstate - 1;
        double rfrac = (double)(r2 - r1) / (double)N;
        double gfrac = (double)(g2 - g1) / (double)N;
        double bfrac = (double)(b2 - b1) / (double)N;
        for (int n = 1; n < N; n++) {
            currlayer->cellr[n+1] = (int)(r1 + n * rfrac + 0.5);
            currlayer->cellg[n+1] = (int)(g1 + n * gfrac + 0.5);
            currlayer->cellb[n+1] = (int)(b1 + n * bfrac + 0.5);
        }
    }
    if (maxstate > 1) {
        currlayer->cellr[maxstate] = r2;
        currlayer->cellg[maxstate] = g2;
        currlayer->cellb[maxstate] = b2;
    }
}

// -----------------------------------------------------------------------------

static FILE* FindRuleFile(const wxString& rulename)
{
    const wxString extn = wxT(".rule");
    wxString path;
    FILE* f;
    
    // first look for rulename.rule in tempdir
    path = tempdir + rulename;
    path += extn;
    f = OPENFILE(path);
    if (f) return f;
    
    // now look for rulename.rule in userrules
    path = userrules + rulename;
    path += extn;
    f = OPENFILE(path);
    if (f) return f;
    
    // now look for rulename.rule in rulesdir
    path = rulesdir + rulename;
    path += extn;
    return OPENFILE(path);
}

// -----------------------------------------------------------------------------

static void CheckRuleHeader(char* linebuf, const wxString& rulename)
{
    // check that 1st line of rulename.rule file contains "@RULE rulename"
    // where rulename must match the file name exactly (to avoid problems on
    // case-sensitive file systems)
    if (strncmp(linebuf, "@RULE ", 6) != 0) {
        wxString msg = _("The first line in ");
        msg += rulename;
        msg += _(".rule does not start with @RULE.");
        Warning(msg);
    } else if (strcmp(linebuf+6, (const char*)rulename.mb_str(wxConvLocal)) != 0) {
        wxString ruleinfile = wxString(linebuf+6, wxConvLocal);
        wxString msg = _("The specified rule (");
        msg += rulename;
        msg += _(") does not match the rule name in the .rule file (");
        msg += ruleinfile;
        msg += _(").\n\nThis will cause problems if you save or copy patterns");
        msg += _(" and try to use them on a case-sensitive file system.");
        Warning(msg);
    }
}

// -----------------------------------------------------------------------------

static void ParseColors(linereader& reader, char* linebuf, int MAXLINELEN, int* linenum, bool* eof)
{
    // parse @COLORS section in currently open .rule file
    int state, r, g, b, r1, g1, b1, r2, g2, b2;
    int maxstate = currlayer->algo->NumCellStates() - 1;
    
    while (reader.fgets(linebuf, MAXLINELEN) != 0) {
        *linenum = *linenum + 1;
        if (linebuf[0] == '#' || linebuf[0] == 0) {
            // skip comment or empty line
        } else if (sscanf(linebuf, "%d%d%d%d%d%d", &r1, &g1, &b1, &r2, &g2, &b2) == 6) {
            // assume line is like this:
            // 255 0 0 0 0 255    use a gradient from red to blue
            currlayer->fromrgb.Set(r1, g1, b1);
            currlayer->torgb.Set(r2, g2, b2);
            CreateColorGradient();
        } else if (sscanf(linebuf, "%d%d%d%d", &state, &r, &g, &b) == 4) {
            // assume line is like this:
            // 1 0 128 255        state 1 is light blue
            if (state >= 0 && state <= maxstate) {
                currlayer->cellr[state] = r;
                currlayer->cellg[state] = g;
                currlayer->cellb[state] = b;
            }
        } else if (linebuf[0] == '@') {
            // found next section, so stop parsing
            *eof = false;
            return;
        }
        // ignore unexpected syntax (better for upward compatibility)
    }
    *eof = true;
}

// -----------------------------------------------------------------------------

static void ParseNames(linereader& reader, char* linebuf, int MAXLINELEN, int* linenum, bool* eof)
{
    // parse @NAMES section in currently open .rule file
    int state;
    int namepos;
    char dummy;
    int maxstate = currlayer->algo->NumCellStates() - 1;
    
    while (reader.fgets(linebuf, MAXLINELEN) != 0) {
        *linenum = *linenum + 1;
        if (linebuf[0] == '#' || linebuf[0] == 0) {
            // skip comment or empty line
        } else if (sscanf(linebuf, "%d %n%c", &state, &namepos, &dummy) == 2) {
            // ignore bad state
            if (state >= 0 && state <= maxstate) {
                currlayer->statenames[state] = wxString(linebuf+namepos,wxConvLocal);
            }
        } else if (linebuf[0] == '@') {
            // found next section, so stop parsing
            *eof = false;
            return;
        }
        // ignore unexpected syntax (better for upward compatibility)
    }
    *eof = true;
}

// -----------------------------------------------------------------------------

static void DeleteXPMData(char** xpmdata, int numstrings)
{
    if (xpmdata) {
        for (int i = 0; i < numstrings; i++)
            if (xpmdata[i]) free(xpmdata[i]);
        free(xpmdata);
    }
}

// -----------------------------------------------------------------------------

static void CreateIcons(const char** xpmdata, int size)
{
    int maxstates = currlayer->algo->NumCellStates();
    
    // we only need to call FreeIconBitmaps if .rule file has duplicate XPM data
    // (unlikely but best to play it safe)
    if (size == 7) {
        if (currlayer->icons7x7) FreeIconBitmaps(currlayer->icons7x7);
        currlayer->icons7x7 = CreateIconBitmaps(xpmdata, maxstates);

    } else if (size == 15) {
        if (currlayer->icons15x15) FreeIconBitmaps(currlayer->icons15x15);
        currlayer->icons15x15 = CreateIconBitmaps(xpmdata, maxstates);

    } else if (size == 31) {
        if (currlayer->icons31x31) FreeIconBitmaps(currlayer->icons31x31);
        currlayer->icons31x31 = CreateIconBitmaps(xpmdata, maxstates);
    }
}

// -----------------------------------------------------------------------------

static void ParseIcons(const wxString& rulename, linereader& reader, char* linebuf, int MAXLINELEN,
                       int* linenum, bool* eof)
{
    // parse @ICONS section in currently open .rule file
    char** xpmdata = NULL;
    int xpmstarted = 0, xpmstrings = 0, maxstrings = 0;
    int wd = 0, ht = 0, numcolors = 0, chars_per_pixel = 0;
    
    std::map<std::string,int> colormap;

    while (true) {
        if (reader.fgets(linebuf, MAXLINELEN) == 0) {
            *eof = true;
            break;
        }
        *linenum = *linenum + 1;
        if (linebuf[0] == '#' || linebuf[0] == '/' || linebuf[0] == 0) {
            // skip comment or empty line
        } else if (linebuf[0] == '"') {
            if (xpmstarted) {
                // we have a "..." string containing XPM data
                if (xpmstrings == 0) {
                    if (sscanf(linebuf, "\"%d%d%d%d\"", &wd, &ht, &numcolors, &chars_per_pixel) == 4 &&
                        wd > 0 && ht > 0 && numcolors > 0 && chars_per_pixel > 0 && ht % wd == 0) {
                        if (wd != 7 && wd != 15 && wd != 31) {
                            // this version of Golly doesn't support the supplied icon size
                            // so silently ignore the rest of this XPM data
                            xpmstarted = 0;
                            continue;
                        }
                        maxstrings = 1 + numcolors + ht;
                        // create and initialize xpmdata
                        xpmdata = (char**) malloc(maxstrings * sizeof(char*));
                        if (xpmdata) {
                            for (int i = 0; i < maxstrings; i++) xpmdata[i] = NULL;
                        } else {
                            Warning(_("Failed to allocate memory for XPM icon data!"));
                            *eof = true;
                            return;
                        }
                    } else {
                        wxString msg;
                        msg.Printf(_("The XPM header string on line %d in "), *linenum);
                        msg += rulename;
                        msg += _(".rule is incorrect");
                        if (wd > 0 && ht > 0 && ht % wd != 0)
                            msg += _(" (height must be a multiple of width).");
                        else if (chars_per_pixel < 1 || chars_per_pixel > 2)
                            msg += _(" (chars_per_pixel must be 1 or 2).");
                        else
                            msg += _(" (4 positive integers are required).");
                        Warning(msg);
                        *eof = true;
                        return;
                    }
                }
                
                // copy data inside "..." to next string in xpmdata
                int len = strlen(linebuf);
                while (linebuf[len] != '"') len--;
                len--;
                if (xpmstrings > 0 && xpmstrings <= numcolors) {
                    // build colormap so we can validate chars in pixel data
                    std::string pixel;
                    char ch1, ch2, ch3;
                    bool badline = false;
                    if (chars_per_pixel == 1) {
                        badline = sscanf(linebuf+1, "%c%c", &ch1, &ch2) != 2 || ch2 != ' ';
                        pixel += ch1;
                    } else {
                        badline = sscanf(linebuf+1, "%c%c%c", &ch1, &ch2, &ch3) != 3 || ch3 != ' ';
                        pixel += ch1;
                        pixel += ch2;
                    }
                    if (badline) {
                        DeleteXPMData(xpmdata, maxstrings);
                        wxString msg;
                        msg.Printf(_("The XPM color info on line %d in "), *linenum);
                        msg += rulename;
                        msg += _(".rule is incorrect.");
                        Warning(msg);
                        *eof = true;
                        return;
                    }
                    colormap[pixel] = xpmstrings;
                } else if (xpmstrings > numcolors) {
                    // check length of string containing pixel data
                    if (len != wd * chars_per_pixel) {
                        DeleteXPMData(xpmdata, maxstrings);
                        wxString msg;
                        msg.Printf(_("The XPM data string on line %d in "), *linenum);
                        msg += rulename;
                        msg += _(".rule has the wrong length.");
                        Warning(msg);
                        *eof = true;
                        return;
                    }
                    // now check that chars in pixel data are valid (ie. in colormap)
                    for (int i = 1; i <= len; i += chars_per_pixel) {
                        std::string pixel;
                        pixel += linebuf[i];
                        if (chars_per_pixel > 1)
                            pixel += linebuf[i+1];
                        if (colormap.find(pixel) == colormap.end()) {
                            DeleteXPMData(xpmdata, maxstrings);
                            wxString msg;
                            msg.Printf(_("The XPM data string on line %d in "), *linenum);
                            msg += rulename;
                            msg += _(".rule has an unknown pixel: ");
                            msg += wxString(pixel.c_str(), wxConvLocal);
                            Warning(msg);
                            *eof = true;
                            return;
                        }
                    }
                }
                char* str = (char*) malloc(len+1);
                if (str) {
                    strncpy(str, linebuf+1, len);
                    str[len] = 0;
                    xpmdata[xpmstrings] = str;
                } else {
                    DeleteXPMData(xpmdata, maxstrings);
                    Warning(_("Failed to allocate memory for XPM string!"));
                    *eof = true;
                    return;
                }
                
                xpmstrings++;
                if (xpmstrings == maxstrings) {
                    // we've got all the data for this icon size
                    CreateIcons((const char**)xpmdata, wd);
                    DeleteXPMData(xpmdata, maxstrings);
                    xpmdata = NULL;
                    xpmstarted = 0;
                    colormap.clear();
                }
            }
        } else if (strcmp(linebuf, "XPM") == 0) {
            // start parsing XPM data on following lines
            if (xpmstarted) break;  // handle error below
            xpmstarted = *linenum;
            xpmstrings = 0;
        } else if (strcmp(linebuf, "circles") == 0) {
            // use circular icons
            CopyBuiltinIcons(circles7x7, circles15x15, circles31x31);
        } else if (strcmp(linebuf, "diamonds") == 0) {
            // use diamond-shaped icons
            CopyBuiltinIcons(diamonds7x7, diamonds15x15, diamonds31x31);
        } else if (strcmp(linebuf, "hexagons") == 0) {
            // use hexagonal icons
            CopyBuiltinIcons(hexagons7x7, hexagons15x15, hexagons31x31);
        } else if (strcmp(linebuf, "triangles") == 0) {
            // use triangular icons
            if (currlayer->algo->NumCellStates() != 4) {
                wxString msg;
                msg.Printf(_("The triangular icons specified on line %d in "), *linenum);
                msg += rulename;
                msg += _(".rule can only be used with a 4-state rule.");
                Warning(msg);
                // don't return 
            } else {
                CopyBuiltinIcons(triangles7x7, triangles15x15, triangles31x31);
            }
        } else if (linebuf[0] == '@') {
            // found next section, so stop parsing
            *eof = false;
            break;
        }
        // ignore unexpected syntax (better for upward compatibility)
    }
    
    if (xpmstarted) {
        // XPM data was incomplete
        DeleteXPMData(xpmdata, maxstrings);
        wxString msg;
        msg.Printf(_("The XPM icon data starting on line %d in "), xpmstarted);
        msg += rulename;
        msg += _(".rule does not have enough strings.");
        Warning(msg);
        *eof = true;
        return;
    }
    
    // create scaled bitmaps if size(s) not supplied
    if (!currlayer->icons7x7) {
        if (currlayer->icons15x15) {
            // scale down 15x15 bitmaps
            currlayer->icons7x7 = ScaleIconBitmaps(currlayer->icons15x15, 7);
        } else if (currlayer->icons31x31) {
            // scale down 31x31 bitmaps
            currlayer->icons7x7 = ScaleIconBitmaps(currlayer->icons31x31, 7);
        }
    }
    if (!currlayer->icons15x15) {
        if (currlayer->icons31x31) {
            // scale down 31x31 bitmaps
            currlayer->icons15x15 = ScaleIconBitmaps(currlayer->icons31x31, 15);
        } else if (currlayer->icons7x7) {
            // scale up 7x7 bitmaps
            currlayer->icons15x15 = ScaleIconBitmaps(currlayer->icons7x7, 15);
        }
    }
    if (!currlayer->icons31x31) {
        if (currlayer->icons15x15) {
            // scale up 15x15 bitmaps
            currlayer->icons31x31 = ScaleIconBitmaps(currlayer->icons15x15, 31);
        } else if (currlayer->icons7x7) {
            // scale up 7x7 bitmaps
            currlayer->icons31x31 = ScaleIconBitmaps(currlayer->icons7x7, 31);
        }
    }
}

// -----------------------------------------------------------------------------

static void LoadRuleInfo(FILE* rulefile, const wxString& rulename,
                         bool* loadedcolors, bool* loadedicons, bool* loadednames)
{
    // load any color/icon/name info from the currently open .rule file
    const int MAXLINELEN = 4095;
    char linebuf[MAXLINELEN + 1];
    int linenum = 0;
    bool eof = false;
    bool skipget = false;

    // the linereader class handles all line endings (CR, CR+LF, LF)
    linereader reader(rulefile);
    
    while (true) {
        if (skipget) {
            // ParseColors/ParseIcons/ParseNames has stopped at next section
            // (ie. linebuf contains @...) so skip fgets call
            skipget = false;
        } else {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            linenum++;
            if (linenum == 1) CheckRuleHeader(linebuf, rulename);
        }
        // look for @COLORS or @ICONS or @NAMES section
        if (strcmp(linebuf, "@COLORS") == 0 && !*loadedcolors) {
            *loadedcolors = true;
            ParseColors(reader, linebuf, MAXLINELEN, &linenum, &eof);
            if (eof) break;
            // otherwise linebuf contains @... so skip next fgets call
            skipget = true;
        } else if (strcmp(linebuf, "@ICONS") == 0 && !*loadedicons) {
            *loadedicons = true;
            ParseIcons(rulename, reader, linebuf, MAXLINELEN, &linenum, &eof);
            if (eof) break;
            // otherwise linebuf contains @... so skip next fgets call
            skipget = true;
        } else if (strcmp(linebuf, "@NAMES") == 0 && !*loadednames) {
            *loadednames = true;
            ParseNames(reader, linebuf, MAXLINELEN, &linenum, &eof);
            if (eof) break;
            // otherwise linebuf contains @... so skip next fgets call
            skipget = true;
        }
    }
    
    reader.close();     // closes rulefile
}

// -----------------------------------------------------------------------------

static void DeleteIcons(Layer* layer)
{
    // delete given layer's existing icons
    if (layer->icons7x7) {
        FreeIconBitmaps(layer->icons7x7);
        layer->icons7x7 = NULL;
    }
    if (layer->icons15x15) {
        FreeIconBitmaps(layer->icons15x15);
        layer->icons15x15 = NULL;
    }
    if (layer->icons31x31) {
        FreeIconBitmaps(layer->icons31x31);
        layer->icons31x31 = NULL;
    }

    // also delete icon texture atlases
    if (layer->atlas7x7) {
        free(layer->atlas7x7);
        layer->atlas7x7 = NULL;
    }
    if (layer->atlas15x15) {
        free(layer->atlas15x15);
        layer->atlas15x15 = NULL;
    }
    if (layer->atlas31x31) {
        free(layer->atlas31x31);
        layer->atlas31x31 = NULL;
    }
}

// -----------------------------------------------------------------------------

static void UseDefaultIcons(int maxstate)
{
    // icons weren't specified so use default icons
    if (currlayer->algo->getgridtype() == lifealgo::HEX_GRID) {
        // use hexagonal icons
        currlayer->icons7x7 = CopyIcons(hexagons7x7, maxstate);
        currlayer->icons15x15 = CopyIcons(hexagons15x15, maxstate);
        currlayer->icons31x31 = CopyIcons(hexagons31x31, maxstate);
    } else if (currlayer->algo->getgridtype() == lifealgo::VN_GRID) {
        // use diamond-shaped icons for 4-neighbor von Neumann neighborhood
        currlayer->icons7x7 = CopyIcons(diamonds7x7, maxstate);
        currlayer->icons15x15 = CopyIcons(diamonds15x15, maxstate);
        currlayer->icons31x31 = CopyIcons(diamonds31x31, maxstate);
    } else {
        // otherwise use default icons from current algo
        AlgoData* ad = algoinfo[currlayer->algtype];
        currlayer->icons7x7 = CopyIcons(ad->icons7x7, maxstate);
        currlayer->icons15x15 = CopyIcons(ad->icons15x15, maxstate);
        currlayer->icons31x31 = CopyIcons(ad->icons31x31, maxstate);
    }
}

// -----------------------------------------------------------------------------

static bool MultiColorBitmaps(wxBitmap** iconmaps, int maxstate)
{
    // return true if any icon contains at least one color that isn't a shade of gray
    for (int n = 1; n <= maxstate; n++) {
        wxBitmap* icon = iconmaps[n];
        if (icon) {
            int wd = icon->GetWidth();
            int ht = icon->GetHeight();
            wxAlphaPixelData icondata(*icon);
            if (icondata) {
                wxAlphaPixelData::Iterator iconpxl(icondata);
                for (int i = 0; i < ht; i++) {
                    wxAlphaPixelData::Iterator iconrow = iconpxl;
                    for (int j = 0; j < wd; j++) {
                        unsigned char r = iconpxl.Red();
                        unsigned char g = iconpxl.Green();
                        unsigned char b = iconpxl.Blue();
                        if (r != g || g != b) return true;
                        iconpxl++;
                    }
                    // move to next row
                    iconpxl = iconrow;
                    iconpxl.OffsetY(icondata, 1);
                }
            }
        }
    }
    return false;   // all icons are grayscale
}

// -----------------------------------------------------------------------------

static void UpdateCurrentColors()
{
    // set current layer's colors, icons and state names according to current algo and rule
    AlgoData* ad = algoinfo[currlayer->algtype];
    lifealgo* curralgo = currlayer->algo;
    int maxstate = curralgo->NumCellStates() - 1;
    
    wxString rulename = wxString(curralgo->getrule(), wxConvLocal);
    // replace any '\' and '/' chars with underscores;
    // ie. given 12/34/6 we look for 12_34_6.rule
    rulename.Replace(wxT("\\"), wxT("_"));
    rulename.Replace(wxT("/"), wxT("_"));
    
    // strip off any suffix like ":T100,200" used to specify a bounded grid
    if (rulename.Find(':') >= 0) rulename = rulename.BeforeFirst(':');

    // copy default colors from current algo
    currlayer->fromrgb = ad->fromrgb;
    currlayer->torgb = ad->torgb;
    if (ad->gradient) {
        CreateColorGradient();
        // state 0 is not part of the gradient
        currlayer->cellr[0] = ad->algor[0];
        currlayer->cellg[0] = ad->algog[0];
        currlayer->cellb[0] = ad->algob[0];
    } else {
        // the Super algo supports three rule families: Super, History and Investigator
        // the History default colors start at index 26 in the list
        // the Investigator default colors start at index 33 in the list
        int o = 0;
        if (strcmp(ad->algoName, "Super") == 0 && rulename.rfind("History") != std::string::npos) o = 26;
        if (strcmp(ad->algoName, "Super") == 0 && rulename.rfind("Investigator") != std::string::npos) o = 33;

        for (int n = 0; n <= maxstate; n++) {
            currlayer->cellr[n] = ad->algor[n + o];
            currlayer->cellg[n] = ad->algog[n + o];
            currlayer->cellb[n] = ad->algob[n + o];
        }
    }
    
    // deallocate current layer's old icons
    DeleteIcons(currlayer);

    // this flag will change if any icon uses a non-grayscale color
    currlayer->multicoloricons = false;
    
    // init state names
    currlayer->statenames.Clear();
    for (int n = 0; n <= maxstate; n++) currlayer->statenames.Add(wxEmptyString);

    bool loadedcolors = false;
    bool loadedicons = false;
    bool loadednames = false;
    
    // look for rulename.rule
    FILE* rulefile = FindRuleFile(rulename);
    if (rulefile) {
        LoadRuleInfo(rulefile, rulename, &loadedcolors, &loadedicons, &loadednames);
        if (!loadedcolors || !loadedicons || !loadednames) {
            // if rulename has the form foo-* then look for foo-shared.rule
            // and load its colors or icons or names
            wxString prefix = rulename.BeforeLast('-');
            if (!prefix.IsEmpty() && !rulename.EndsWith(wxT("-shared"))) {
                rulename = prefix + wxT("-shared");
                rulefile = FindRuleFile(rulename);
                if (rulefile) {
                    LoadRuleInfo(rulefile, rulename, &loadedcolors, &loadedicons, &loadednames);
                }
            }
        }
    }
    
    if (!loadedicons) UseDefaultIcons(maxstate);

    // use the smallest icons to check if they are multi-color
    if (currlayer->icons7x7 && MultiColorBitmaps(currlayer->icons7x7, maxstate)) {
        currlayer->multicoloricons = true;
    }

    // create icon texture atlases (used for rendering)
    currlayer->numicons = maxstate;
    currlayer->atlas7x7 = CreateIconAtlas(currlayer->icons7x7, 8);
    currlayer->atlas15x15 = CreateIconAtlas(currlayer->icons15x15, 16);
    currlayer->atlas31x31 = CreateIconAtlas(currlayer->icons31x31, 32);
    
    if (swapcolors) {
        // invert cell colors in current layer
        for (int n = 0; n <= maxstate; n++) {
            currlayer->cellr[n] = 255 - currlayer->cellr[n];
            currlayer->cellg[n] = 255 - currlayer->cellg[n];
            currlayer->cellb[n] = 255 - currlayer->cellb[n];
        }
    }
}

// -----------------------------------------------------------------------------

void UpdateCloneColors()
{
    if (currlayer->cloneid > 0) {
        for (int i = 0; i < numlayers; i++) {
            Layer* cloneptr = layer[i];
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                cloneptr->fromrgb = currlayer->fromrgb;
                cloneptr->torgb = currlayer->torgb;
                cloneptr->multicoloricons = currlayer->multicoloricons;
                cloneptr->numicons = currlayer->numicons;
                for (int n = 0; n <= currlayer->numicons; n++) {
                    cloneptr->cellr[n] = currlayer->cellr[n];
                    cloneptr->cellg[n] = currlayer->cellg[n];
                    cloneptr->cellb[n] = currlayer->cellb[n];
                }
                
                // use same icon pointers
                cloneptr->icons7x7 = currlayer->icons7x7;
                cloneptr->icons15x15 = currlayer->icons15x15;
                cloneptr->icons31x31 = currlayer->icons31x31;
                
                // use same atlas
                cloneptr->atlas7x7 = currlayer->atlas7x7;
                cloneptr->atlas15x15 = currlayer->atlas15x15;
                cloneptr->atlas31x31 = currlayer->atlas31x31;
            }
        }
    }
}

// -----------------------------------------------------------------------------

void UpdateLayerColors()
{
    UpdateCurrentColors();
    
    // above has created icon texture data so don't call UpdateIconColors here
    
    // if current layer has clones then update their colors
    UpdateCloneColors();
}

// -----------------------------------------------------------------------------

void UpdateIconColors()
{
    // delete current icon texture atlases
    if (currlayer->atlas7x7) {
        free(currlayer->atlas7x7);
    }
    if (currlayer->atlas15x15) {
        free(currlayer->atlas15x15);
    }
    if (currlayer->atlas31x31) {
        free(currlayer->atlas31x31);
    }
    
    // re-create icon texture atlases
    currlayer->atlas7x7 = CreateIconAtlas(currlayer->icons7x7, 8);
    currlayer->atlas15x15 = CreateIconAtlas(currlayer->icons15x15, 16);
    currlayer->atlas31x31 = CreateIconAtlas(currlayer->icons31x31, 32);
}

// -----------------------------------------------------------------------------

void InvertIconColors(unsigned char* atlasptr, int iconsize, int numicons)
{
    if (atlasptr) {
        int numbytes = numicons * iconsize * iconsize * 4;
        int i = 0;
        while (i < numbytes) {
            if (atlasptr[i+3] == 0) {
                // ignore transparent pixel
            } else {
                // invert pixel color
                atlasptr[i]   = 255 - atlasptr[i];
                atlasptr[i+1] = 255 - atlasptr[i+1];
                atlasptr[i+2] = 255 - atlasptr[i+2];
            }
            i += 4;
        }
    }
}

// -----------------------------------------------------------------------------

void InvertCellColors()
{
    bool clone_inverted[MAX_LAYERS] = {false};
    
    // swapcolors has changed so invert cell colors in all layers
    for (int i = 0; i < numlayers; i++) {
        Layer* layerptr = layer[i];
        
        // do NOT use layerptr->algo->... here -- it might not be correct
        // for a non-current layer (but we can use layerptr->algtype)
        int maxstate = algoinfo[layerptr->algtype]->maxstates - 1;
        for (int n = 0; n <= maxstate; n++) {
            layerptr->cellr[n] = 255 - layerptr->cellr[n];
            layerptr->cellg[n] = 255 - layerptr->cellg[n];
            layerptr->cellb[n] = 255 - layerptr->cellb[n];
        }
        
        // clones share icon texture data so we must be careful to only invert them once
        if (layerptr->cloneid == 0 || !clone_inverted[layerptr->cloneid]) {
            // invert colors in icon texture atlases
            // (do NOT use maxstate here -- might be > layerptr->numicons)
            InvertIconColors(layerptr->atlas7x7, 8, layerptr->numicons);
            InvertIconColors(layerptr->atlas15x15, 16, layerptr->numicons);
            InvertIconColors(layerptr->atlas31x31, 32, layerptr->numicons);
            if (layerptr->cloneid > 0) clone_inverted[layerptr->cloneid] = true;
        }
    }
}

// -----------------------------------------------------------------------------

Layer* GetLayer(int index)
{
    if (index < 0 || index >= numlayers) {
        Warning(_("Bad index in GetLayer!"));
        return NULL;
    } else {
        return layer[index];
    }
}

// -----------------------------------------------------------------------------

int GetUniqueCloneID()
{
    // find first available index (> 0) to use as cloneid
    for (int i = 1; i < MAX_LAYERS; i++) {
        if (cloneavail[i]) {
            cloneavail[i] = false;
            return i;
        }
    }
    // bug if we get here
    Warning(_("Bug in GetUniqueCloneID!"));
    return 1;
}

// -----------------------------------------------------------------------------

Layer::Layer()
{
    if (!cloning) {
        // use a unique temporary file for saving starting patterns
        tempstart = wxFileName::CreateTempFileName(tempdir + wxT("golly_start_"));
    }
    
    dirty = false;                // user has not modified pattern
    savedirty = false;            // in case script created layer
    stayclean = inscript;         // if true then keep the dirty flag false
                                  // for the duration of the script
    savestart = false;            // no need to save starting pattern
    currfile.Clear();             // no pattern file has been loaded
    startfile.Clear();            // no starting pattern yet
    startgen = 0;                 // initial starting generation
    currname = _("untitled");     // initial window title
    originx = 0;                  // no X origin offset
    originy = 0;                  // no Y origin offset
    
    icons7x7 = NULL;              // no 7x7 icons
    icons15x15 = NULL;            // no 15x15 icons
    icons31x31 = NULL;            // no 31x31 icons

    atlas7x7 = NULL;              // no texture atlas for 7x7 icons
    atlas15x15 = NULL;            // no texture atlas for 15x15 icons
    atlas31x31 = NULL;            // no texture atlas for 31x31 icons
    
    statenames.Clear();           // no names for cell states
    
    currframe = 0;                // first frame in timeline
    autoplay = 0;                 // not playing
    tlspeed = 0;                  // default speed for autoplay
    
    // create viewport; the initial size is not important because it will soon change
    view = new viewport(100,100);
    if (view == NULL) Fatal(_("Failed to create viewport!"));
    
    if (numlayers == 0) {
        // creating very first layer (can't be a clone)
        cloneid = 0;
        
        // initialize cloneavail array (cloneavail[0] is never used)
        cloneavail[0] = false;
        for (int i = 1; i < MAX_LAYERS; i++) cloneavail[i] = true;
        
        // set some options using initial values stored in prefs file
        algtype = initalgo;
        hyperspeed = inithyperspeed;
        showhashinfo = initshowhashinfo;
        autofit = initautofit;
        
        // initial base step and exponent
        currbase = algoinfo[algtype]->defbase;
        currexpo = 0;
        
        // create empty universe
        algo = CreateNewUniverse(algtype);
        
        // set rule using initrule stored in prefs file
        const char* err = algo->setrule(initrule);
        if (err) {
            // user might have edited rule in prefs file, or deleted .rule file
            algo->setrule( algo->DefaultRule() );
        }
        
        // don't need to remember rule here (SaveLayerSettings will do it)
        rule = wxEmptyString;
        
        // initialize undo/redo history
        undoredo = new UndoRedo();
        if (undoredo == NULL) Fatal(_("Failed to create new undo/redo object!"));
        
        // set cursor in case newcurs/opencurs are set to "No Change"
        curs = curs_pencil;
        drawingstate = 1;
        
    } else {
        // adding a new layer after currlayer (see AddLayer)
        
        // inherit current universe type and other settings
        algtype = currlayer->algtype;
        hyperspeed = currlayer->hyperspeed;
        showhashinfo = currlayer->showhashinfo;
        autofit = currlayer->autofit;
        
        // initial base step and exponent
        currbase = algoinfo[algtype]->defbase;
        currexpo = 0;
        
        if (cloning) {
            if (currlayer->cloneid == 0) {
                // first time this universe is being cloned so need a unique cloneid
                cloneid = GetUniqueCloneID();
                currlayer->cloneid = cloneid;    // current layer also becomes a clone
                numclones += 2;
            } else {
                // we're cloning an existing clone
                cloneid = currlayer->cloneid;
                numclones++;
            }
            
            // clones share the same universe and undo/redo history
            algo = currlayer->algo;
            undoredo = currlayer->undoredo;
            
            // clones also share the same timeline
            currframe = currlayer->currframe;
            autoplay = currlayer->autoplay;
            tlspeed = currlayer->tlspeed;
            
            // clones use same name for starting file
            tempstart = currlayer->tempstart;
            
        } else {
            // this layer isn't a clone
            cloneid = 0;
            
            // create empty universe
            algo = CreateNewUniverse(algtype);
            
            // use current rule
            const char* err = algo->setrule(currlayer->algo->getrule());
            if (err) {
                // .rule file might have been deleted
                algo->setrule( algo->DefaultRule() );
            }
            
            // initialize undo/redo history
            undoredo = new UndoRedo();
            if (undoredo == NULL) Fatal(_("Failed to create new undo/redo object!"));
        }
        
        // inherit current rule
        rule = wxString(currlayer->algo->getrule(), wxConvLocal);
        
        // inherit current viewport's size, scale and location
        view->resize( currlayer->view->getwidth(), currlayer->view->getheight() );
        view->setpositionmag( currlayer->view->x, currlayer->view->y,
                             currlayer->view->getmag() );
        
        // inherit current cursor and drawing state
        curs = currlayer->curs;
        drawingstate = currlayer->drawingstate;
        
        if (cloning || duplicating) {
            // duplicate all the other current settings
            currname = currlayer->currname;
            dirty = currlayer->dirty;
            savedirty = currlayer->savedirty;
            stayclean = currlayer->stayclean;
            currbase = currlayer->currbase;
            currexpo = currlayer->currexpo;
            autofit = currlayer->autofit;
            hyperspeed = currlayer->hyperspeed;
            showhashinfo = currlayer->showhashinfo;
            originx = currlayer->originx;
            originy = currlayer->originy;
            statenames = currlayer->statenames;
            
            // duplicate selection info
            currsel = currlayer->currsel;
            savesel = currlayer->savesel;
            
            // duplicate the stuff needed to reset pattern
            currfile = currlayer->currfile;
            startfile = currlayer->startfile;
            savestart = currlayer->savestart;
            startalgo = currlayer->startalgo;
            startdirty = currlayer->startdirty;
            startrule = currlayer->startrule;
            startx = currlayer->startx;
            starty = currlayer->starty;
            startbase = currlayer->startbase;
            startexpo = currlayer->startexpo;
            startmag = currlayer->startmag;
            startgen = currlayer->startgen;
            startsel = currlayer->startsel;
            if (cloning) {
                // if clone is created after pattern has been generated
                // then we don't want a reset to change its name
                startname = currlayer->currname;
            } else {
                startname = currlayer->startname;
            }
        }
        
        if (duplicating) {
            // first set same gen count
            algo->setGeneration( currlayer->algo->getGeneration() );
            
            // duplicate pattern
            if ( !currlayer->algo->isEmpty() ) {
                bigint top, left, bottom, right;
                currlayer->algo->findedges(&top, &left, &bottom, &right);
                if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
                    Warning(_("Pattern is too big to duplicate."));
                } else {
                    viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                                      currlayer->algo, algo, false, _("Duplicating layer"));
                }
            }
            
            // tempstart file must remain unique in duplicate layer
            if ( wxFileExists(currlayer->tempstart) ) {
                if ( !wxCopyFile(currlayer->tempstart, tempstart, true) ) {
                    Warning(_("Could not copy tempstart file!"));
                }
            }
            if (currlayer->startfile == currlayer->tempstart) {
                startfile = tempstart;
            }
            
            if (allowundo) {
                // duplicate current undo/redo history in new layer
                undoredo->DuplicateHistory(currlayer, this);
            }
        }
    }
}

// -----------------------------------------------------------------------------

Layer::~Layer()
{
    // delete stuff allocated in ctor
    delete view;
    
    if (cloneid > 0) {
        // this layer is a clone, so count how many layers have the same cloneid
        int clonecount = 0;
        for (int i = 0; i < numlayers; i++) {
            if (layer[i]->cloneid == cloneid) clonecount++;
            
            // tell undo/redo which clone is being deleted
            if (this == layer[i]) undoredo->DeletingClone(i);
        }     
        if (clonecount > 2) {
            // only delete this clone
            numclones--;
        } else {
            // first make this cloneid available for the next clone
            cloneavail[cloneid] = true;
            // reset the other cloneid to 0 (should only be one such clone)
            for (int i = 0; i < numlayers; i++) {
                // careful -- layer[i] might be this layer
                if (this != layer[i] && layer[i]->cloneid == cloneid)
                    layer[i]->cloneid = 0;
            }
            numclones -= 2;
        }
        
    } else {
        // this layer is not a clone, so delete universe and undo/redo history
        delete algo;
        delete undoredo;
        
        // delete tempstart file if it exists
        if (wxFileExists(tempstart)) wxRemoveFile(tempstart);
        
        // delete any icons or state names
        DeleteIcons(this);
        statenames.Clear();
    }
}

// -----------------------------------------------------------------------------

// define a window for displaying cell colors/icons:

const int CELLSIZE = 16;      // wd and ht of each cell
const int NUMCOLS = 32;       // number of columns
const int NUMROWS = 8;        // number of rows

class CellPanel : public wxPanel
{
public:
    CellPanel(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxPoint(0,0),
              wxSize(NUMCOLS*CELLSIZE+1,NUMROWS*CELLSIZE+1)) { }
    
    wxStaticText* statebox;    // for showing state of cell under cursor
    wxStaticText* rgbbox;      // for showing color of cell under cursor
    
private:
    void OnEraseBackground(wxEraseEvent& event);
    void OnPaint(wxPaintEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseMotion(wxMouseEvent& event);
    void OnMouseExit(wxMouseEvent& event);
    
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(CellPanel, wxPanel)
EVT_ERASE_BACKGROUND (CellPanel::OnEraseBackground)
EVT_PAINT            (CellPanel::OnPaint)
EVT_LEFT_DOWN        (CellPanel::OnMouseDown)
EVT_LEFT_DCLICK      (CellPanel::OnMouseDown)
EVT_MOTION           (CellPanel::OnMouseMotion)
EVT_ENTER_WINDOW     (CellPanel::OnMouseMotion)
EVT_LEAVE_WINDOW     (CellPanel::OnMouseExit)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

void CellPanel::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
    // do nothing
}

// -----------------------------------------------------------------------------

void CellPanel::OnPaint(wxPaintEvent& WXUNUSED(event))
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
    wxBitmap** iconmaps = currlayer->icons15x15;
    wxRect r = wxRect(0, 0, CELLSIZE+1, CELLSIZE+1);
    int col = 0;
    for (int state = 0; state < 256; state++) {
        if (state < currlayer->algo->NumCellStates()) {
            if (showicons && iconmaps && iconmaps[state]) {
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                DrawRect(dc, r);
                dc.SetBrush(wxNullBrush);               
                DrawOneIcon(dc, r.x + 1, r.y + 1, iconmaps[state],
                            currlayer->cellr[0],
                            currlayer->cellg[0],
                            currlayer->cellb[0],
                            currlayer->cellr[state],
                            currlayer->cellg[state],
                            currlayer->cellb[state],
                            currlayer->multicoloricons);
            } else {
                wxColor color(currlayer->cellr[state],
                              currlayer->cellg[state],
                              currlayer->cellb[state]);
                dc.SetBrush(wxBrush(color));
                DrawRect(dc, r);
                dc.SetBrush(wxNullBrush);
            }
            
        } else {
            // state >= currlayer->algo->NumCellStates()
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

void CellPanel::OnMouseDown(wxMouseEvent& event)
{
    int col = event.GetX() / CELLSIZE;
    int row = event.GetY() / CELLSIZE;
    int state = row * NUMCOLS + col;
    if (state >= 0 && state < currlayer->algo->NumCellStates()) {
        // let user change color of this cell state
        wxColour rgb(currlayer->cellr[state],
                     currlayer->cellg[state],
                     currlayer->cellb[state]);
        wxColourData data;
        data.SetChooseFull(true);    // for Windows
        data.SetColour(rgb);
        
        wxColourDialog dialog(this, &data);
        if ( dialog.ShowModal() == wxID_OK ) {
            wxColourData retData = dialog.GetColourData();
            wxColour c = retData.GetColour();
            if (rgb != c) {
                // change color
                currlayer->cellr[state] = c.Red();
                currlayer->cellg[state] = c.Green();
                currlayer->cellb[state] = c.Blue();
                Refresh(false);
            }
        }
    } 
    event.Skip();
}

// -----------------------------------------------------------------------------

void CellPanel::OnMouseMotion(wxMouseEvent& event)
{
    int col = event.GetX() / CELLSIZE;
    int row = event.GetY() / CELLSIZE;
    int state = row * NUMCOLS + col;
    if (state < 0 || state > 255) {
        statebox->SetLabel(_(" "));
        rgbbox->SetLabel(_(" "));
    } else {
        statebox->SetLabel(wxString::Format(_("%d"),state));
        if (state < currlayer->algo->NumCellStates()) {
            rgbbox->SetLabel(wxString::Format(_("%d,%d,%d"),
                                              currlayer->cellr[state],
                                              currlayer->cellg[state],
                                              currlayer->cellb[state]));
        } else {
            rgbbox->SetLabel(_(" "));
        }
    }
}

// -----------------------------------------------------------------------------

void CellPanel::OnMouseExit(wxMouseEvent& WXUNUSED(event))
{
    statebox->SetLabel(_(" "));
    rgbbox->SetLabel(_(" "));
}

// -----------------------------------------------------------------------------

// define a modal dialog for changing colors

class ColorDialog : public wxDialog
{
public:
    ColorDialog(wxWindow* parent);
    virtual bool TransferDataFromWindow();    // called when user hits OK
    
    enum {
        // control ids
        CELL_PANEL = wxID_HIGHEST + 1,
        ICON_CHECK,
        STATE_BOX,
        RGB_BOX,
        GRADIENT_BUTT,
        FROM_BUTT,
        TO_BUTT,
        DEFAULT_BUTT
    };
    
    void CreateControls();        // initialize all the controls
    
    void AddColorButton(wxWindow* parent, wxBoxSizer* hbox, int id, wxColor* rgb);
    void ChangeButtonColor(int id, wxColor* rgb);
    void UpdateButtonColor(int id, wxColor* rgb);
    
    CellPanel* cellpanel;         // for displaying cell colors/icons
    wxCheckBox* iconcheck;        // show icons?
    
private:
    // event handlers
    void OnCheckBoxClicked(wxCommandEvent& event);
    void OnButton(wxCommandEvent& event);
#if defined(__WXOSX_COCOA__) && wxCHECK_VERSION(3,0,0)
    void OnCharHook(wxKeyEvent& event);
#endif

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ColorDialog, wxDialog)
EVT_CHECKBOX   (wxID_ANY,  ColorDialog::OnCheckBoxClicked)
EVT_BUTTON     (wxID_ANY,  ColorDialog::OnButton)
#if defined(__WXOSX_COCOA__) && wxCHECK_VERSION(3,0,0)
EVT_CHAR_HOOK  (ColorDialog::OnCharHook)
#endif
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

// these consts are used to get nicely spaced controls on each platform:

const int HGAP = 12;    // space left and right of vertically stacked boxes

// following ensures OK/Cancel buttons are better aligned
#ifdef __WXMAC__
    const int STDHGAP = 0;
#elif defined(__WXMSW__)
    const int STDHGAP = 9;
#else
    const int STDHGAP = 10;
#endif

const int BITMAP_WD = 60;     // width of bitmap in color buttons
const int BITMAP_HT = 20;     // height of bitmap in color buttons

// -----------------------------------------------------------------------------

ColorDialog::ColorDialog(wxWindow* parent)
{
    Create(parent, wxID_ANY, _("Set Layer Colors"), wxDefaultPosition, wxDefaultSize);
    CreateControls();
    Centre();
}

// -----------------------------------------------------------------------------

void ColorDialog::CreateControls()
{
    wxString note =
    _("NOTE:  Changes made here are temporary and only affect the current layer and ");
    note += _("its clones.  The colors will be reset to their default values if you open ");
    note += _("a pattern file or create a new pattern, or if you change the current algorithm ");
    note += _("or rule.  If you want to change the default colors, use Preferences > Color.");
    wxStaticText* notebox = new wxStaticText(this, wxID_STATIC, note);
    notebox->Wrap(NUMCOLS * CELLSIZE + 1);
    
    // create bitmap buttons
    wxBoxSizer* frombox = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* tobox = new wxBoxSizer(wxHORIZONTAL);
    AddColorButton(this, frombox, FROM_BUTT, &currlayer->fromrgb);
    AddColorButton(this, tobox, TO_BUTT, &currlayer->torgb);
    
    wxButton* defbutt = new wxButton(this, DEFAULT_BUTT, _("Default Colors"));
    wxButton* gradbutt = new wxButton(this, GRADIENT_BUTT, _("Create Gradient"));
    
    wxBoxSizer* gradbox = new wxBoxSizer(wxHORIZONTAL);
    gradbox->Add(gradbutt, 0, wxALIGN_CENTER_VERTICAL, 0);
    gradbox->Add(new wxStaticText(this, wxID_STATIC, _(" from ")),
                 0, wxALIGN_CENTER_VERTICAL, 0);
    gradbox->Add(frombox, 0, wxALIGN_CENTER_VERTICAL, 0);
    gradbox->Add(new wxStaticText(this, wxID_STATIC, _(" to ")),
                 0, wxALIGN_CENTER_VERTICAL, 0);
    gradbox->Add(tobox, 0, wxALIGN_CENTER_VERTICAL, 0);
    
    // create child window for displaying cell colors/icons
    cellpanel = new CellPanel(this, CELL_PANEL);
    
    iconcheck = new wxCheckBox(this, ICON_CHECK, _("Show icons"));
    iconcheck->SetValue(showicons);
    
    wxStaticText* statebox = new wxStaticText(this, STATE_BOX, _("999"));
    cellpanel->statebox = statebox;
    wxBoxSizer* hbox1 = new wxBoxSizer(wxHORIZONTAL);
    hbox1->Add(statebox, 0, 0, 0);
    hbox1->SetMinSize( hbox1->GetMinSize() );
    
    wxStaticText* rgbbox = new wxStaticText(this, RGB_BOX, _("999,999,999"));
    cellpanel->rgbbox = rgbbox;
    wxBoxSizer* hbox2 = new wxBoxSizer(wxHORIZONTAL);
    hbox2->Add(rgbbox, 0, 0, 0);
    hbox2->SetMinSize( hbox2->GetMinSize() );
    
    statebox->SetLabel(_(" "));
    rgbbox->SetLabel(_(" "));
    
    wxBoxSizer* botbox = new wxBoxSizer(wxHORIZONTAL);
    botbox->Add(new wxStaticText(this, wxID_STATIC, _("State: ")),
                0, wxALIGN_CENTER_VERTICAL, 0);
    botbox->Add(hbox1, 0, wxALIGN_CENTER_VERTICAL, 0);
    botbox->Add(20, 0, 0);
    botbox->Add(new wxStaticText(this, wxID_STATIC, _("RGB: ")),
                0, wxALIGN_CENTER_VERTICAL, 0);
    botbox->Add(hbox2, 0, wxALIGN_CENTER_VERTICAL, 0);
    botbox->AddStretchSpacer();
    botbox->Add(iconcheck, 0, wxALIGN_CENTER_VERTICAL, 0);
    
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    vbox->Add(gradbox, 0, wxALIGN_CENTER, 0);
    vbox->AddSpacer(10);
    vbox->Add(cellpanel, 0, wxLEFT | wxRIGHT, 0);
    vbox->AddSpacer(5);
    vbox->Add(botbox, 1, wxGROW | wxLEFT | wxRIGHT, 0);
    
    wxSizer* stdbutts = CreateButtonSizer(wxOK | wxCANCEL);
    wxBoxSizer* stdhbox = new wxBoxSizer( wxHORIZONTAL );
    stdhbox->Add(defbutt, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, HGAP);
    stdhbox->Add(stdbutts, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, STDHGAP);
    
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    topSizer->AddSpacer(10);
    topSizer->Add(notebox, 0, wxLEFT | wxRIGHT, HGAP);
    topSizer->AddSpacer(20);
    topSizer->Add(vbox, 0, wxGROW | wxLEFT | wxRIGHT, HGAP);
    topSizer->AddSpacer(10);
    topSizer->Add(stdhbox, 1, wxGROW | wxTOP | wxBOTTOM, 10);
    SetSizer(topSizer);
    topSizer->SetSizeHints(this);    // calls Fit
}

// -----------------------------------------------------------------------------

void ColorDialog::OnCheckBoxClicked(wxCommandEvent& event)
{
    if ( event.GetId() == ICON_CHECK ) {
        showicons = iconcheck->GetValue() == 1;
        cellpanel->Refresh(false);
    }
}

// -----------------------------------------------------------------------------

void ColorDialog::AddColorButton(wxWindow* parent, wxBoxSizer* hbox, int id, wxColor* rgb)
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
    if (bb) hbox->Add(bb, 0, wxALIGN_CENTER_VERTICAL, 0);
}

// -----------------------------------------------------------------------------

void ColorDialog::UpdateButtonColor(int id, wxColor* rgb)
{
    wxBitmapButton* bb = (wxBitmapButton*) FindWindow(id);
    if (bb) {
        wxBitmap bitmap(BITMAP_WD, BITMAP_HT);
        wxMemoryDC dc;
        dc.SelectObject(bitmap);
        wxRect rect(0, 0, BITMAP_WD, BITMAP_HT);
        wxBrush brush(*rgb);
        FillRect(dc, rect, brush);
        dc.SelectObject(wxNullBitmap);
        bb->SetBitmapLabel(bitmap);
        bb->Refresh();
    }
}

// -----------------------------------------------------------------------------

void ColorDialog::ChangeButtonColor(int id, wxColor* rgb)
{
    wxColourData data;
    data.SetChooseFull(true);    // for Windows
    data.SetColour(*rgb);
    
    wxColourDialog dialog(this, &data);
    if ( dialog.ShowModal() == wxID_OK ) {
        wxColourData retData = dialog.GetColourData();
        wxColour c = retData.GetColour();
        
        if (*rgb != c) {
            // change given color
            rgb->Set(c.Red(), c.Green(), c.Blue());
            
            // also change color of bitmap in corresponding button
            UpdateButtonColor(id, rgb);
            
            cellpanel->Refresh(false);
        }
    }
}

// -----------------------------------------------------------------------------

#if defined(__WXOSX_COCOA__) && wxCHECK_VERSION(3,0,0)

void ColorDialog::OnCharHook(wxKeyEvent& event)
{
    int key = event.GetKeyCode();
    if (key == WXK_RETURN) {
        // fix wxOSX 3.x bug: allow return key to select OK button
        wxCommandEvent okevent(wxEVT_COMMAND_BUTTON_CLICKED, wxID_OK);
        okevent.SetEventObject(this);
        GetEventHandler()->ProcessEvent(okevent);
        return;
    }
    event.Skip();   // process other keys, like escape
}

#endif

// -----------------------------------------------------------------------------

void ColorDialog::OnButton(wxCommandEvent& event)
{
    int id = event.GetId();
    
    if ( id == FROM_BUTT ) {
        ChangeButtonColor(id, &currlayer->fromrgb);
        
    } else if ( id == TO_BUTT ) {
        ChangeButtonColor(id, &currlayer->torgb);
        
    } else if ( id == GRADIENT_BUTT ) {
        // use currlayer->fromrgb and currlayer->torgb
        CreateColorGradient();
        cellpanel->Refresh(false);
        
    } else if ( id == DEFAULT_BUTT ) {
        // restore current layer's default colors, but don't call UpdateLayerColors
        // here because we don't want to change any clone layers at this stage
        // (in case user cancels dialog)
        UpdateCurrentColors();
        UpdateButtonColor(FROM_BUTT, &currlayer->fromrgb);
        UpdateButtonColor(TO_BUTT, &currlayer->torgb);
        cellpanel->Refresh(false);
        
    } else {
        // process other buttons like Cancel and OK
        event.Skip();
    }
}

// -----------------------------------------------------------------------------

bool ColorDialog::TransferDataFromWindow()
{
    // update icon texture data before calling UpdateCloneColors
    UpdateIconColors();
    
    // if current layer has clones then update their colors
    UpdateCloneColors();
    
    return true;
}

// -----------------------------------------------------------------------------

// class for saving and restoring layer colors
class SaveData {
public:
    SaveData() {
        fromrgb = currlayer->fromrgb;
        torgb = currlayer->torgb;
        for (int i = 0; i < currlayer->algo->NumCellStates(); i++) {
            cellr[i] = currlayer->cellr[i];
            cellg[i] = currlayer->cellg[i];
            cellb[i] = currlayer->cellb[i];
        }
        saveshowicons = showicons;
    }
    
    void RestoreData() {
        currlayer->fromrgb = fromrgb;
        currlayer->torgb = torgb;
        for (int i = 0; i < currlayer->algo->NumCellStates(); i++) {
            currlayer->cellr[i] = cellr[i];
            currlayer->cellg[i] = cellg[i];
            currlayer->cellb[i] = cellb[i];
        }
        showicons = saveshowicons;
    }
    
    // this must match color info in Layer class
    wxColor fromrgb;
    wxColor torgb;
    unsigned char cellr[256];
    unsigned char cellg[256];
    unsigned char cellb[256];
    
    // we also save/restore showicons option
    bool saveshowicons;
};

// -----------------------------------------------------------------------------

void SetLayerColors()
{
    if (inscript || viewptr->waitingforclick) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_SET_COLORS);
        mainptr->Stop();
        return;
    }
    
    bool wastoggled = swapcolors;
    if (swapcolors) viewptr->ToggleCellColors();    // swapcolors is now false
    
    // save current color info so we can restore it if user cancels changes
    SaveData* save_info = new SaveData();
    
    ColorDialog dialog( wxGetApp().GetTopWindow() );
    int button = dialog.ShowModal();
    if (viewptr) viewptr->ResetMouseDown();
    if (button != wxID_OK) {
        // user hit Cancel so restore color info saved above
        save_info->RestoreData();
    }
    
    delete save_info;
    
    if (wastoggled) viewptr->ToggleCellColors();    // swapcolors is now true
    
    mainptr->UpdateEverything();
}

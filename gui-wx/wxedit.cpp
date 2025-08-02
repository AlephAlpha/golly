// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/dcbuffer.h"    // for wxBufferedPaintDC
#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h" // for wxToolTip
#endif

#include "bigint.h"
#include "lifealgo.h"

#include "wxgolly.h"       // for viewptr, statusptr, mainptr
#include "wxmain.h"        // for mainptr->...
#include "wxutils.h"       // for Fatal
#include "wxprefs.h"       // for showedit, showallstates, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"      // for DrawOneIcon
#include "wxlayer.h"       // for currlayer, LayerBarHeight, SetLayerColors
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxtimeline.h"    // for TimelineExists
#include "wxedit.h"

// -----------------------------------------------------------------------------

enum {
    // ids for bitmap buttons in edit bar
    UNDO_BUTT = 0,
    REDO_BUTT,
    DRAW_BUTT,
    PICK_BUTT,
    SELECT_BUTT,
    MOVE_BUTT,
    ZOOMIN_BUTT,
    ZOOMOUT_BUTT,
    ALLSTATES_BUTT,
    NUM_BUTTONS,      // must be after all buttons
    STATE_BAR
};

// bitmaps for edit bar buttons
#include "bitmaps/undo.xpm"
#include "bitmaps/redo.xpm"
#include "bitmaps/draw.xpm"
#include "bitmaps/pick.xpm"
#include "bitmaps/select.xpm"
#include "bitmaps/move.xpm"
#include "bitmaps/zoomin.xpm"
#include "bitmaps/zoomout.xpm"
#include "bitmaps/allstates.xpm"
// bitmaps for down state of toggle buttons
#include "bitmaps/draw_down.xpm"
#include "bitmaps/pick_down.xpm"
#include "bitmaps/select_down.xpm"
#include "bitmaps/move_down.xpm"
#include "bitmaps/zoomin_down.xpm"
#include "bitmaps/zoomout_down.xpm"
#include "bitmaps/allstates_down.xpm"

// width and height of bitmap buttons
const int BUTTON_WD = 24;
const int BUTTON_HT = 24;

// -----------------------------------------------------------------------------

// Define edit bar window:

// derive from wxPanel so we get current theme's background color on Windows
class EditBar : public wxPanel
{
public:
    EditBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
    ~EditBar();
    
    // add a bitmap button to edit bar
    void AddButton(int id, const wxString& tip);
    
    // add gap between buttons
    void AddSeparator();
    
    // enable/disable button
    void EnableButton(int id, bool enable);
    
    // set state of a toggle button
    void SelectButton(int id, bool select);
    
    // update scroll bar
    void UpdateScrollBar();
    
    // detect press and release of a bitmap button
    void OnButtonDown(wxMouseEvent& event);
    void OnButtonUp(wxMouseEvent& event);
    void OnKillFocus(wxFocusEvent& event);
    
private:
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
    
    // event handlers
    void OnPaint(wxPaintEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnScroll(wxScrollEvent& event);
    
    void SetEditFont(wxDC& dc);
    void DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y);
    void DrawAllStates(wxDC& dc, int wd);
    void DrawEditBar(wxDC& dc, int wd, int ht);
    
    // bitmaps for normal or down state
    wxBitmap normbutt[NUM_BUTTONS];
    wxBitmap downbutt[NUM_BUTTONS];
    
#ifdef __WXMSW__
    // on Windows we need bitmaps for disabled buttons
    wxBitmap disnormbutt[NUM_BUTTONS];
    wxBitmap disdownbutt[NUM_BUTTONS];
#endif
    
    // remember state of toggle buttons to avoid unnecessary drawing;
    // 0 = not yet initialized, 1 = selected, -1 = not selected
    int buttstate[NUM_BUTTONS];
    
    // positioning data used by AddButton and AddSeparator
    int ypos, xpos, smallgap, biggap;
    
    wxBitmap* editbitmap;         // edit bar bitmap
    int editbitmapwd;             // width of edit bar bitmap
    int editbitmapht;             // height of edit bar bitmap
    
    wxRect colorbox;              // box showing current color
    wxRect iconbox;               // box showing current icon
    
    wxScrollBar* drawbar;         // scroll bar for changing drawing state
    int firststate;               // first visible state (if showing all states)
    int namepos;                  // horizontal position for state name
    
    int h_col1;                   // horizontal position of labels
    int h_col2;                   // horizontal position of info for state 0
    int digitwd;                  // width of digit in edit bar font
    int digitht;                  // height of digit in edit bar font
    int textascent;               // vertical adjustment used in DrawText calls
    wxFont editfont;              // edit bar font
};

BEGIN_EVENT_TABLE(EditBar, wxPanel)
EVT_PAINT            (              EditBar::OnPaint)
EVT_LEFT_DOWN        (              EditBar::OnMouseDown)
EVT_LEFT_DCLICK      (              EditBar::OnMouseDown)
EVT_BUTTON           (wxID_ANY,     EditBar::OnButton)
EVT_COMMAND_SCROLL   (STATE_BAR,    EditBar::OnScroll)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

EditBar* editbarptr = NULL;         // global pointer to edit bar
const int BIGHT = 80;               // height of edit bar if showallstates
const int SMALLHT = 32;             // height of edit bar if not showallstates
static int editbarht;               // current height (BIGHT or SMALLHT)

const int LINEHT = 14;              // distance between each baseline
const int BASELINE1 = SMALLHT+LINEHT-1;   // baseline of 1st line
const int BASELINE2 = BASELINE1+LINEHT;   // baseline of 2nd line
const int BASELINE3 = BASELINE2+LINEHT;   // baseline of 3rd line
const int COLWD = 22;               // column width of state/color/icon info
const int BOXWD = 9;                // width (and height) of small color/icon boxes
const int BOXSIZE = 17;             // width and height of colorbox and iconbox
const int BOXGAP = 8;               // gap between colorbox and iconbox
const int PAGESIZE = 10;            // scroll amount when paging

// edit bar buttons (must be global to use Connect/Disconnect on Windows)
wxBitmapButton* ebbutt[NUM_BUTTONS];

// -----------------------------------------------------------------------------

EditBar::EditBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
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
    normbutt[UNDO_BUTT] =      XPM_BITMAP(undo);
    normbutt[REDO_BUTT] =      XPM_BITMAP(redo);
    normbutt[DRAW_BUTT] =      XPM_BITMAP(draw);
    normbutt[PICK_BUTT] =      XPM_BITMAP(pick);
    normbutt[SELECT_BUTT] =    XPM_BITMAP(select);
    normbutt[MOVE_BUTT] =      XPM_BITMAP(move);
    normbutt[ZOOMIN_BUTT] =    XPM_BITMAP(zoomin);
    normbutt[ZOOMOUT_BUTT] =   XPM_BITMAP(zoomout);
    normbutt[ALLSTATES_BUTT] = XPM_BITMAP(allstates);
    
    // toggle buttons also have a down state
    downbutt[DRAW_BUTT] =      XPM_BITMAP(draw_down);
    downbutt[PICK_BUTT] =      XPM_BITMAP(pick_down);
    downbutt[SELECT_BUTT] =    XPM_BITMAP(select_down);
    downbutt[MOVE_BUTT] =      XPM_BITMAP(move_down);
    downbutt[ZOOMIN_BUTT] =    XPM_BITMAP(zoomin_down);
    downbutt[ZOOMOUT_BUTT] =   XPM_BITMAP(zoomout_down);
    downbutt[ALLSTATES_BUTT] = XPM_BITMAP(allstates_down);
    
#ifdef __WXMSW__
    for (int i = 0; i < NUM_BUTTONS; i++) {
        CreatePaleBitmap(normbutt[i], disnormbutt[i]);
    }
    CreatePaleBitmap(downbutt[DRAW_BUTT],       disdownbutt[DRAW_BUTT]);
    CreatePaleBitmap(downbutt[PICK_BUTT],       disdownbutt[PICK_BUTT]);
    CreatePaleBitmap(downbutt[SELECT_BUTT],     disdownbutt[SELECT_BUTT]);
    CreatePaleBitmap(downbutt[MOVE_BUTT],       disdownbutt[MOVE_BUTT]);
    CreatePaleBitmap(downbutt[ZOOMIN_BUTT],     disdownbutt[ZOOMIN_BUTT]);
    CreatePaleBitmap(downbutt[ZOOMOUT_BUTT],    disdownbutt[ZOOMOUT_BUTT]);
    CreatePaleBitmap(downbutt[ALLSTATES_BUTT],  disdownbutt[ALLSTATES_BUTT]);
#endif
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
        buttstate[i] = 0;
    }
    
    // init position variables used by AddButton and AddSeparator
    xpos = 4;
    ypos = (32 - BUTTON_HT) / 2;
    smallgap = 4;
    biggap = 16;
    
    // add buttons
    AddButton(UNDO_BUTT,       _("Undo"));
    AddButton(REDO_BUTT,       _("Redo"));
    AddSeparator();
    AddButton(DRAW_BUTT,       _("Draw"));
    AddButton(PICK_BUTT,       _("Pick"));
    AddButton(SELECT_BUTT,     _("Select"));
    AddButton(MOVE_BUTT,       _("Move"));
    AddButton(ZOOMIN_BUTT,     _("Zoom in"));
    AddButton(ZOOMOUT_BUTT,    _("Zoom out"));
    AddSeparator();
    AddButton(ALLSTATES_BUTT,  _("Show/hide all states"));
    
    // create font for text in edit bar and set textascent for use in DisplayText
#ifdef __WXMSW__
    // use smaller, narrower font on Windows
    editfont = wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    int major, minor;
    wxGetOsVersion(&major, &minor);
    if ( major > 5 || (major == 5 && minor >= 1) ) {
        // 5.1+ means XP or later (Vista if major >= 6)
        textascent = 11;
    } else {
        textascent = 10;
    }
#elif defined(__WXGTK__)
    // use smaller font on GTK
    editfont = wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    textascent = 11;
#elif defined(__WXMAC__)
    // we need to specify facename to get Monaco instead of Courier
    editfont = wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxT("Monaco"));
    textascent = 10;
    editfont.SetPointSize(10); // avoid assert error
#else
    editfont = wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    textascent = 10;
#endif
    
    // determine horizontal offsets for info in edit bar
    wxClientDC dc(this);
    int textwd, textht;
    SetEditFont(dc);
    h_col1 = 4;
    dc.GetTextExtent(_("State:"), &textwd, &textht);
    h_col2 = h_col1 + textwd + 4;
    dc.GetTextExtent(_("9"), &digitwd, &digitht);
    digitht -= 4;
    
    editbitmap = NULL;
    editbitmapwd = -1;
    editbitmapht = -1;
    
    // add scroll bar
    int scrollbarwd = 100;
#ifdef __WXMAC__
    int scrollbarht = 15;   // must be this height on Mac
#else
    int scrollbarht = BOXSIZE;
#endif
    int x = xpos + 3*digitwd + BOXGAP + 2*(BOXSIZE + BOXGAP);
    int y = (SMALLHT - (scrollbarht + 1)) / 2;
    drawbar = new wxScrollBar(this, STATE_BAR, wxPoint(x, y),
                              wxSize(scrollbarwd, scrollbarht),
                              wxSB_HORIZONTAL);
    if (drawbar == NULL) Fatal(_("Failed to create scroll bar!"));
    firststate = 0;
    namepos = x + scrollbarwd + BOXGAP;
}

// -----------------------------------------------------------------------------

EditBar::~EditBar()
{
    delete editbitmap;
    delete drawbar;
}

// -----------------------------------------------------------------------------

void EditBar::SetEditFont(wxDC& dc)
{
    dc.SetFont(editfont);
    dc.SetTextForeground(*wxBLACK);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.SetBackgroundMode(wxTRANSPARENT);
}

// -----------------------------------------------------------------------------

void EditBar::DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y)
{
    // DrawText's y parameter is top of text box but we pass in baseline
    // so adjust by textascent which depends on platform and OS version -- yuk!
    dc.DrawText(s, x, y - textascent);
}

// -----------------------------------------------------------------------------

void EditBar::DrawAllStates(wxDC& dc, int wd)
{
    DisplayText(dc, _("State:"), h_col1, BASELINE1);
    DisplayText(dc, _("Color:"), h_col1, BASELINE2);
    DisplayText(dc, _("Icon:"),  h_col1, BASELINE3);
    
    wxBitmap** iconmaps = currlayer->icons7x7;
    
    dc.SetPen(*wxBLACK_PEN);
    
    // calculate number of (completely) visible states
    int visstates = (wd - h_col2) / COLWD;
    if (visstates >= currlayer->algo->NumCellStates()) {
        // all states are visible
        firststate = 0;
        visstates = currlayer->algo->NumCellStates();
    } else {
        // change firststate if necessary so that drawing state is visible
        if (currlayer->drawingstate < firststate) {
            firststate = currlayer->drawingstate;
        } else if (currlayer->drawingstate >= firststate + visstates) {
            firststate = currlayer->drawingstate - visstates + 1;
        }
        // may need to reduce firststate if window width has increased
        if (firststate + visstates >= currlayer->algo->NumCellStates()) {
            firststate = currlayer->algo->NumCellStates() - visstates;
        }
    }
    
    // add 1 to visstates so we see partial box at right edge
    for (int i = firststate; i < firststate + visstates + 1; i++) {
        
        // this test is needed because we added 1 to visstates
        if (i >= currlayer->algo->NumCellStates()) break;
        
        wxString strbuf;
        wxRect r;
        int x;
        
        // draw state value
        strbuf.Printf(_("%d"), i);
        x = (int)(h_col2 + (i - firststate) * COLWD + (COLWD - strbuf.length() * digitwd) / 2);
        #ifdef __WXMAC__
            // digitwd is not accurate on macOS
            x += i < 100 ? 1 : 2;
        #endif
        DisplayText(dc, strbuf, x, BASELINE1);
        
        // draw color box
        x = 1 + h_col2 + (i - firststate) * COLWD + (COLWD - BOXWD) / 2;
        wxColor color(currlayer->cellr[i], currlayer->cellg[i], currlayer->cellb[i]);
        r = wxRect(x, BASELINE2 - BOXWD, BOXWD, BOXWD);
        dc.SetBrush(wxBrush(color));
        DrawRect(dc, r);
        dc.SetBrush(wxNullBrush);
        
        // draw icon box
        r = wxRect(x, BASELINE3 - BOXWD, BOXWD, BOXWD);
        if (iconmaps && iconmaps[i]) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            DrawRect(dc, r);
            dc.SetBrush(wxNullBrush);
            DrawOneIcon(dc, x + 1, BASELINE3 - BOXWD + 1, iconmaps[i],
                        currlayer->cellr[0], currlayer->cellg[0], currlayer->cellb[0],
                        currlayer->cellr[i], currlayer->cellg[i], currlayer->cellb[i],
                        currlayer->multicoloricons);
        } else {
            dc.SetBrush(wxBrush(color));
            DrawRect(dc, r);
            dc.SetBrush(wxNullBrush);
        }
    }
    
    // draw rect around current drawing state
    if (currlayer->drawingstate >= firststate &&
        currlayer->drawingstate <= firststate + visstates) {
        int x = 1 + h_col2 + (currlayer->drawingstate - firststate) * COLWD;
#ifdef __WXGTK__
        wxRect r(x, SMALLHT + 1, COLWD - 1, BIGHT - SMALLHT - 5);
#else
        wxRect r(x, SMALLHT + 2, COLWD - 1, BIGHT - SMALLHT - 5);
#endif
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        DrawRect(dc, r);
        dc.SetBrush(wxNullBrush);
    }
    
    dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void EditBar::DrawEditBar(wxDC& dc, int wd, int ht)
{
    wxRect r = wxRect(0, 0, wd, ht);
    
#ifdef __WXMAC__
    wxBrush brush(wxColor(202,202,202));
    FillRect(dc, r, brush);
#endif

#ifdef __WXMSW__
    // use theme background color on Windows
    wxBrush brush(GetBackgroundColour());
    FillRect(dc, r, brush);
#endif
    
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
    
    // reset drawing state if it's no longer valid (due to algo/rule change)
    if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
        currlayer->drawingstate = 1;
    }
    
    SetEditFont(dc);  // for DisplayText calls
    
    if (showallstates) DrawAllStates(dc, wd);
    
    dc.SetPen(*wxBLACK_PEN);
    
    // draw current drawing state
    int state = currlayer->drawingstate;
    int x = xpos + 2;
    int y = SMALLHT - 8;
    int yoffset = (BOXSIZE - digitht)/2;
    wxString strbuf;
    if (state < 10) x += digitwd;
    if (state < 100) x += digitwd;
    #ifdef __WXMAC__
        // digitwd is not accurate on macOS
        if (state > 10) x += 1;
        if (state > 100) x += 1;
        yoffset += 1;
    #endif
    #ifdef __WXMSW__
        yoffset += 2;
    #endif
    strbuf.Printf(_("%d"), state);
    DisplayText(dc, strbuf, x, y - yoffset);
    
    // draw non-empty state name
    if (currlayer->statenames.GetCount() > 0 && currlayer->statenames[state].Length() > 0) {
        yoffset = (BOXSIZE - digitht)/2;
        #ifdef __WXMAC__
            yoffset += 2;
        #endif
        #ifdef __WXMSW__
            yoffset += 2;
        #endif
        DisplayText(dc, currlayer->statenames[state], namepos, y - yoffset);
    }
    
    wxColor cellcolor(currlayer->cellr[state], currlayer->cellg[state], currlayer->cellb[state]);
    
    // draw color box
    x = xpos + 3*digitwd + BOXGAP;
    colorbox = wxRect(x, y - BOXSIZE, BOXSIZE, BOXSIZE);
    dc.SetBrush(wxBrush(cellcolor));
    DrawRect(dc, colorbox);
    dc.SetBrush(wxNullBrush);
    
    // draw icon box
    wxBitmap** iconmaps = currlayer->icons15x15;
    x += BOXSIZE + BOXGAP;
    iconbox = wxRect(x, y - BOXSIZE, BOXSIZE, BOXSIZE);
    if (iconmaps && iconmaps[state]) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        DrawRect(dc, iconbox);
        dc.SetBrush(wxNullBrush);
        DrawOneIcon(dc, x + 1, y - BOXSIZE + 1, iconmaps[state],
                    currlayer->cellr[0],     currlayer->cellg[0],     currlayer->cellb[0],
                    currlayer->cellr[state], currlayer->cellg[state], currlayer->cellb[state],
                    currlayer->multicoloricons);
    } else {
        dc.SetBrush(wxBrush(cellcolor));
        DrawRect(dc, iconbox);
        dc.SetBrush(wxNullBrush);
    }
    
    // show whether color or icon mode is selected
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    if (showicons) {
        iconbox.Inflate(2,2);
        DrawRect(dc, iconbox);
        iconbox.Inflate(-2,-2);
    } else {
        colorbox.Inflate(2,2);
        DrawRect(dc, colorbox);
        colorbox.Inflate(-2,-2);
    }
    dc.SetBrush(wxNullBrush);
    
    dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void EditBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    int wd, ht;
    GetClientSize(&wd, &ht);
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    
#if defined(__WXMAC__) || defined(__WXGTK__)
    // windows on Mac OS X and GTK+ 2.0 are automatically buffered
    wxPaintDC dc(this);
#else
    // use wxWidgets buffering to avoid flicker
    if (wd != editbitmapwd || ht != editbitmapht) {
        // need to create a new bitmap for edit bar
        delete editbitmap;
        editbitmap = new wxBitmap(wd, ht);
        editbitmapwd = wd;
        editbitmapht = ht;
    }
    if (editbitmap == NULL) Fatal(_("Not enough memory to render edit bar!"));
    wxBufferedPaintDC dc(this, *editbitmap);
#endif
    
    if (showedit) DrawEditBar(dc, wd, ht);
}

// -----------------------------------------------------------------------------

void EditBar::OnMouseDown(wxMouseEvent& event)
{
    // on Win/Linux we need to reset keyboard focus to viewport window
    viewptr->SetFocus();
    
    mainptr->showbanner = false;
    statusptr->ClearMessage();
    
    int x = event.GetX();
    int y = event.GetY();
    
    if (showallstates) {
        // user can change drawing state by clicking in appropriate box
        int right = h_col2 + COLWD * currlayer->algo->NumCellStates();
        int box = -1;
        
        if (x > h_col2 && x < right && y > SMALLHT) {
            box = (x - h_col2) / COLWD + firststate;
        }
        
        if (box >= 0 && box < currlayer->algo->NumCellStates() &&
            currlayer->drawingstate != box) {
            currlayer->drawingstate = box;
            Refresh(false);
            UpdateScrollBar();
            return;
        }
    }
    
    if (event.LeftDClick()) {
        // open Set Layer Colors dialog if user double-clicks in color/icon box
        if (colorbox.Contains(x,y) || iconbox.Contains(x,y)) {
            SetLayerColors();
        }
    } else {
        // user can change color/icon mode by clicking in color/icon box
        if (colorbox.Contains(x,y) && showicons) {
            viewptr->ToggleCellIcons();
        } else if (iconbox.Contains(x,y) && !showicons) {
            viewptr->ToggleCellIcons();
        }
    }
}

// -----------------------------------------------------------------------------

void EditBar::OnButton(wxCommandEvent& event)
{
#ifdef __WXMAC__
    // close any open tool tip window (fixes wxMac bug?)
    wxToolTip::RemoveToolTips();
#endif
    
    mainptr->showbanner = false;
    statusptr->ClearMessage();
    
    int id = event.GetId();
    
    int cmdid;
    switch (id) {
        case UNDO_BUTT:      cmdid = ID_UNDO; break;
        case REDO_BUTT:      cmdid = ID_REDO; break;
        case DRAW_BUTT:      cmdid = ID_DRAW; break;
        case PICK_BUTT:      cmdid = ID_PICK; break;
        case SELECT_BUTT:    cmdid = ID_SELECT; break;
        case MOVE_BUTT:      cmdid = ID_MOVE; break;
        case ZOOMIN_BUTT:    cmdid = ID_ZOOMIN; break;
        case ZOOMOUT_BUTT:   cmdid = ID_ZOOMOUT; break;
        case ALLSTATES_BUTT: cmdid = ID_ALL_STATES; break;
        default:             Warning(_("Unexpected button id!")); return;
    }
    
    // call MainFrame::OnMenu after OnButton finishes
    wxCommandEvent cmdevt(wxEVT_COMMAND_MENU_SELECTED, cmdid);
    wxPostEvent(mainptr->GetEventHandler(), cmdevt);
    
    // avoid possible problems
    viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void EditBar::OnScroll(wxScrollEvent& event)
{
    WXTYPE type = event.GetEventType();
    
    if (type == wxEVT_SCROLL_LINEUP) {
        currlayer->drawingstate--;
        if (currlayer->drawingstate < 0)
            currlayer->drawingstate = 0;
        Refresh(false);
        
    } else if (type == wxEVT_SCROLL_LINEDOWN) {
        currlayer->drawingstate++;
        if (currlayer->drawingstate >= currlayer->algo->NumCellStates())
            currlayer->drawingstate = currlayer->algo->NumCellStates() - 1;
        Refresh(false);
        
    } else if (type == wxEVT_SCROLL_PAGEUP) {
        currlayer->drawingstate -= PAGESIZE;
        if (currlayer->drawingstate < 0)
            currlayer->drawingstate = 0;
        Refresh(false);
        
    } else if (type == wxEVT_SCROLL_PAGEDOWN) {
        currlayer->drawingstate += PAGESIZE;
        if (currlayer->drawingstate >= currlayer->algo->NumCellStates())
            currlayer->drawingstate = currlayer->algo->NumCellStates() - 1;
        Refresh(false);
        
    } else if (type == wxEVT_SCROLL_THUMBTRACK) {
        currlayer->drawingstate = event.GetPosition();
        if (currlayer->drawingstate < 0)
            currlayer->drawingstate = 0;
        if (currlayer->drawingstate >= currlayer->algo->NumCellStates())
            currlayer->drawingstate = currlayer->algo->NumCellStates() - 1;
        Refresh(false);
        
    } else if (type == wxEVT_SCROLL_THUMBRELEASE) {
        UpdateScrollBar();
    }
    
#ifndef __WXMAC__
    viewptr->SetFocus();    // need on Win/Linux
#endif
}

// -----------------------------------------------------------------------------

void EditBar::OnKillFocus(wxFocusEvent& event)
{
    int id = event.GetId();
    ebbutt[id]->SetFocus();   // don't let button lose focus
}

// -----------------------------------------------------------------------------

void EditBar::OnButtonDown(wxMouseEvent& event)
{
    // edit bar button has been pressed
    int id = event.GetId();
    
    // connect a handler that keeps focus with the pressed button
    ebbutt[id]->Connect(id, wxEVT_KILL_FOCUS, wxFocusEventHandler(EditBar::OnKillFocus));
    
    event.Skip();
}

// -----------------------------------------------------------------------------

void EditBar::OnButtonUp(wxMouseEvent& event)
{
    // edit bar button has been released
    int id = event.GetId();
    
    wxPoint pt = ebbutt[id]->ScreenToClient( wxGetMousePosition() );
    
    int wd, ht;
    ebbutt[id]->GetClientSize(&wd, &ht);
    wxRect r(0, 0, wd, ht);
    
    // diconnect kill-focus handler
    ebbutt[id]->Disconnect(id, wxEVT_KILL_FOCUS, wxFocusEventHandler(EditBar::OnKillFocus));
    viewptr->SetFocus();
    
    if (r.Contains(pt)) {
        // call OnButton
        wxCommandEvent buttevt(wxEVT_COMMAND_BUTTON_CLICKED, id);
        buttevt.SetEventObject(ebbutt[id]);
        ebbutt[id]->GetEventHandler()->ProcessEvent(buttevt);
    }
}

// -----------------------------------------------------------------------------

void EditBar::AddButton(int id, const wxString& tip)
{
    ebbutt[id] = new wxBitmapButton(this, id, normbutt[id], wxPoint(xpos,ypos),
                                    wxSize(BUTTON_WD, BUTTON_HT), wxBORDER_NONE);
    if (ebbutt[id] == NULL) {
        Fatal(_("Failed to create edit bar button!"));
    } else {
        xpos += BUTTON_WD + smallgap;
        ebbutt[id]->SetToolTip(tip);
#ifdef __WXMSW__
        // fix problem with edit bar buttons when generating/inscript
        // due to focus being changed to viewptr
        ebbutt[id]->Connect(id, wxEVT_LEFT_DOWN, wxMouseEventHandler(EditBar::OnButtonDown));
        ebbutt[id]->Connect(id, wxEVT_LEFT_UP, wxMouseEventHandler(EditBar::OnButtonUp));
#endif
    }
}

// -----------------------------------------------------------------------------

void EditBar::AddSeparator()
{
    xpos += biggap - smallgap;
}

// -----------------------------------------------------------------------------

void EditBar::EnableButton(int id, bool enable)
{
    if (enable == ebbutt[id]->IsEnabled()) return;
    
#ifdef __WXMSW__
    if (id == DRAW_BUTT && currlayer->curs == curs_pencil) {
        ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
        
    } else if (id == PICK_BUTT && currlayer->curs == curs_pick) {
        ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
        
    } else if (id == SELECT_BUTT && currlayer->curs == curs_cross) {
        ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
        
    } else if (id == MOVE_BUTT && currlayer->curs == curs_hand) {
        ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
        
    } else if (id == ZOOMIN_BUTT && currlayer->curs == curs_zoomin) {
        ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
        
    } else if (id == ZOOMOUT_BUTT && currlayer->curs == curs_zoomout) {
        ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
        
    } else if (id == ALLSTATES_BUTT && showallstates) {
        ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
        
    } else {
        ebbutt[id]->SetBitmapDisabled(disnormbutt[id]);
    }
#endif
    
    ebbutt[id]->Enable(enable);
}

// -----------------------------------------------------------------------------

void EditBar::SelectButton(int id, bool select)
{
    if (select) {
        if (buttstate[id] == 1) return;
        buttstate[id] = 1;
        ebbutt[id]->SetBitmapLabel(downbutt[id]);
    } else {
        if (buttstate[id] == -1) return;
        buttstate[id] = -1;
        ebbutt[id]->SetBitmapLabel(normbutt[id]);
    }
    
    ebbutt[id]->Refresh(false);
}

// -----------------------------------------------------------------------------

void EditBar::UpdateScrollBar()
{
    drawbar->SetScrollbar(currlayer->drawingstate, 1,
                          currlayer->algo->NumCellStates(), PAGESIZE, true);
}

// -----------------------------------------------------------------------------

void CreateEditBar(wxWindow* parent)
{
    // create edit bar underneath layer bar
    int wd, ht;
    parent->GetClientSize(&wd, &ht);
    
    editbarht = showallstates ? BIGHT : SMALLHT;
    editbarptr = new EditBar(parent, 0, LayerBarHeight(), wd, editbarht);
    if (editbarptr == NULL) Fatal(_("Failed to create edit bar!"));
    
    editbarptr->Show(showedit);
}

// -----------------------------------------------------------------------------

int EditBarHeight() {
    return (showedit ? editbarht : 0);
}

// -----------------------------------------------------------------------------

void ResizeEditBar(int wd)
{
    if (editbarptr && showedit) {
        editbarptr->SetSize(wd, editbarht);
    }
}

// -----------------------------------------------------------------------------

void UpdateEditBar()
{
    if (editbarptr && showedit && !mainptr->IsIconized()) {
        bool active = !viewptr->waitingforclick;
        bool timeline = TimelineExists();
        
        // set state of toggle buttons
        editbarptr->SelectButton(DRAW_BUTT,       currlayer->curs == curs_pencil);
        editbarptr->SelectButton(PICK_BUTT,       currlayer->curs == curs_pick);
        editbarptr->SelectButton(SELECT_BUTT,     currlayer->curs == curs_cross);
        editbarptr->SelectButton(MOVE_BUTT,       currlayer->curs == curs_hand);
        editbarptr->SelectButton(ZOOMIN_BUTT,     currlayer->curs == curs_zoomin);
        editbarptr->SelectButton(ZOOMOUT_BUTT,    currlayer->curs == curs_zoomout);
        editbarptr->SelectButton(ALLSTATES_BUTT,  showallstates);
        
        // CanUndo() returns false if drawing/selecting cells so the user can't undo
        // while in those modes (by pressing a key), but we want the Undo button to
        // appear to be active
        bool canundo = (allowundo && (viewptr->drawingcells || viewptr->selectingcells))
                       || currlayer->undoredo->CanUndo();
        editbarptr->EnableButton(UNDO_BUTT,       active && !timeline && canundo);
        editbarptr->EnableButton(REDO_BUTT,       active && !timeline &&
                                                  currlayer->undoredo->CanRedo());
        editbarptr->EnableButton(DRAW_BUTT,       active);
        editbarptr->EnableButton(PICK_BUTT,       active);
        editbarptr->EnableButton(SELECT_BUTT,     active);
        editbarptr->EnableButton(MOVE_BUTT,       active);
        editbarptr->EnableButton(ZOOMIN_BUTT,     active);
        editbarptr->EnableButton(ZOOMOUT_BUTT,    active);
        editbarptr->EnableButton(ALLSTATES_BUTT,  active);
        
        editbarptr->Refresh(false);
        
        // drawing state might have changed
        editbarptr->UpdateScrollBar();
        
        #ifdef __WXGTK__
            // avoid bug that can cause buttons to lose their bitmaps
            editbarptr->Update();
        #endif
    }
}

// -----------------------------------------------------------------------------

void ToggleEditBar()
{
    showedit = !showedit;
    mainptr->ResizeBigView();
    editbarptr->Show(showedit);    // needed on Windows
    if (showlayer) {
        // line at bottom of layer bar may need to be added/removed
        RedrawLayerBar();
    }
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void ToggleAllStates()
{
    showallstates = !showallstates;
    editbarht = showallstates ? BIGHT : SMALLHT;
    if (showedit) {
        mainptr->ResizeBigView();
        mainptr->UpdateEverything();
    } else if (showallstates) {
        // show the edit bar using new height
        ToggleEditBar();
    } else {
        mainptr->UpdateMenuItems();
    }
}

// -----------------------------------------------------------------------------

void ShiftEditBar(int yamount)
{
    int x, y;
    editbarptr->GetPosition(&x, &y);
    editbarptr->Move(x, y + yamount);
}

// -----------------------------------------------------------------------------

void CycleDrawingState(bool higher)
{
    if (viewptr->drawingcells) return;
    
    int maxstate = currlayer->algo->NumCellStates() - 1;
    
    if (higher) {
        if (currlayer->drawingstate == maxstate)
            currlayer->drawingstate = 0;
        else
            currlayer->drawingstate++;
    } else {
        if (currlayer->drawingstate == 0)
            currlayer->drawingstate = maxstate;
        else
            currlayer->drawingstate--;
    }
    
    if (showedit && !mainptr->IsIconized()) {
        editbarptr->Refresh(false);
        editbarptr->UpdateScrollBar();
        #ifdef __WXGTK__
            // avoid bug that can cause buttons to lose their bitmaps
            editbarptr->Update();
        #endif
    }
}

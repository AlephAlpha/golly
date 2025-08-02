// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h" // for wxToolTip
#endif
#include "wx/dcbuffer.h"    // for wxBufferedPaintDC

#include "bigint.h"
#include "lifealgo.h"

#include "wxgolly.h"       // for viewptr, statusptr, mainptr
#include "wxmain.h"        // for mainptr->...
#include "wxutils.h"       // for Warning, etc
#include "wxprefs.h"       // for showtimeline, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxscript.h"      // for inscript
#include "wxview.h"        // for viewptr->...
#include "wxlayer.h"       // for currlayer
#include "wxtimeline.h"

// bitmaps for timeline bar buttons
#include "bitmaps/record.xpm"
#include "bitmaps/stop.xpm"
#include "bitmaps/backwards.xpm"
#include "bitmaps/forwards.xpm"
#include "bitmaps/stopplay.xpm"
#include "bitmaps/deltime.xpm"

// -----------------------------------------------------------------------------

// Define timeline bar window:

enum {
    // ids for bitmap buttons in timeline bar
    RECORD_BUTT = 0,
    STOPREC_BUTT,
    BACKWARDS_BUTT,
    FORWARDS_BUTT,
    STOPPLAY_BUTT,
    DELETE_BUTT,
    NUM_BUTTONS,        // must be after all buttons
    ID_SLIDER,          // for slider
    ID_SCROLLBAR,       // for scroll bar
    ID_AUTOTIMER        // for autotimer
};

// derive from wxPanel so we get current theme's background color on Windows
class TimelineBar : public wxPanel
{
public:
    TimelineBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
    ~TimelineBar();
    
    void AddButton(int id, const wxString& tip);
    void AddSeparator();
    void EnableButton(int id, bool enable);
    void UpdateButtons();
    void UpdateSlider();
    void UpdateScrollBar();
    void DisplayCurrentFrame();
    void StartAutoTimer();
    void StopAutoTimer();

    // detect press and release of a bitmap button
    void OnButtonDown(wxMouseEvent& event);
    void OnButtonUp(wxMouseEvent& event);
    void OnKillFocus(wxFocusEvent& event);
    
    wxTimer* autotimer;             // timer for auto play
    wxSlider* slider;               // slider for controlling autoplay speed
    wxScrollBar* framebar;          // scroll bar for displaying timeline frames
    
    // positioning data used by AddButton and AddSeparator
    int ypos, xpos, smallgap, biggap;
    
private:
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
    
    // event handlers
    void OnPaint(wxPaintEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnSlider(wxScrollEvent& event);
    void OnScroll(wxScrollEvent& event);
    void OnAutoTimer(wxTimerEvent& event);
    
    void SetTimelineFont(wxDC& dc);
    void DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y);
    void DrawTimelineBar(wxDC& dc, int wd, int ht);
    
    // bitmaps for normal buttons
    wxBitmap normbutt[NUM_BUTTONS];
    
#ifdef __WXMSW__
    // on Windows we need bitmaps for disabled buttons
    wxBitmap disnormbutt[NUM_BUTTONS];
#endif
    
    // remember state of buttons to avoid unnecessary updates
    int buttstate[NUM_BUTTONS];
    
    wxBitmap* timelinebitmap;       // timeline bar bitmap
    int timelinebitmapwd;           // width of timeline bar bitmap
    int timelinebitmapht;           // height of timeline bar bitmap
    
    int digitwd;                    // width of digit in timeline bar font
    int digitht;                    // height of digit in timeline bar font
    int textascent;                 // vertical adjustment used in DrawText calls
    wxFont timelinefont;            // timeline bar font
};

BEGIN_EVENT_TABLE(TimelineBar, wxPanel)
EVT_PAINT            (              TimelineBar::OnPaint)
EVT_LEFT_DOWN        (              TimelineBar::OnMouseDown)
EVT_BUTTON           (wxID_ANY,     TimelineBar::OnButton)
EVT_COMMAND_SCROLL   (ID_SLIDER,    TimelineBar::OnSlider)
EVT_COMMAND_SCROLL   (ID_SCROLLBAR, TimelineBar::OnScroll)
EVT_TIMER            (ID_AUTOTIMER, TimelineBar::OnAutoTimer)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

static TimelineBar* tbarptr = NULL;    // global pointer to timeline bar
static int mindelpos;                  // minimum x position of DELETE_BUTT

const int TBARHT = 32;                 // height of timeline bar
const int SCROLLHT = 17;               // height of scroll bar
const int PAGESIZE = 10;               // scroll amount when paging

const int MINSPEED = -10;              // minimum autoplay speed
const int MAXSPEED = 10;               // maximum autoplay speed

// width and height of bitmap buttons
const int BUTTON_WD = 24;
const int BUTTON_HT = 24;

// timeline bar buttons (must be global to use Connect/Disconnect on Windows)
static wxBitmapButton* tlbutt[NUM_BUTTONS];

// -----------------------------------------------------------------------------

TimelineBar::TimelineBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
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
    normbutt[RECORD_BUTT] =    XPM_BITMAP(record);
    normbutt[STOPREC_BUTT] =   XPM_BITMAP(stop);
    normbutt[BACKWARDS_BUTT] = XPM_BITMAP(backwards);
    normbutt[FORWARDS_BUTT] =  XPM_BITMAP(forwards);
    normbutt[STOPPLAY_BUTT] =  XPM_BITMAP(stopplay);
    normbutt[DELETE_BUTT] =    XPM_BITMAP(deltime);
    
#ifdef __WXMSW__
    for (int i = 0; i < NUM_BUTTONS; i++) {
        CreatePaleBitmap(normbutt[i], disnormbutt[i]);
    }
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
    AddButton(RECORD_BUTT, _("Start recording"));
    AddSeparator();
    AddButton(BACKWARDS_BUTT, _("Play backwards"));
    AddButton(FORWARDS_BUTT, _("Play forwards"));
    
    // create font for text in timeline bar and set textascent for use in DisplayText
#ifdef __WXMSW__
    // use smaller, narrower font on Windows
    timelinefont = wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
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
    timelinefont = wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    textascent = 11;
#elif defined(__WXMAC__)
    // we need to specify facename to get Monaco instead of Courier
    timelinefont = wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxT("Monaco"));
    textascent = 10;
    timelinefont.SetPointSize(10); // avoid assert error
#else
    timelinefont = wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    textascent = 10;
#endif
    
    wxClientDC dc(this);
    SetTimelineFont(dc);
    dc.GetTextExtent(_("9"), &digitwd, &digitht);
    digitht -= 4;
    
    timelinebitmap = NULL;
    timelinebitmapwd = -1;
    timelinebitmapht = -1;
    
    // add slider
    int x, y;
    int sliderwd = 80;
#ifdef __WXMAC__
    int sliderht = 15;
#else
    int sliderht = 24;   // for Windows and wxGTK
#endif
    x = xpos + 20 - smallgap;
    y = (TBARHT - (sliderht + 1)) / 2;
    slider = new wxSlider(this, ID_SLIDER, 0, MINSPEED, MAXSPEED, wxPoint(x, y),
                          wxSize(sliderwd, sliderht),
                          wxSL_HORIZONTAL);
    if (slider == NULL) Fatal(_("Failed to create timeline slider!"));
#ifdef __WXMAC__
    slider->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
    slider->Move(x, y-1);
#endif
    xpos = x + sliderwd;
    
    // add scroll bar
    int scrollbarwd = 60;   // minimum width (ResizeTimelineBar will alter it)
#ifdef __WXMAC__
    int scrollbarht = 15;   // must be this height on Mac
#else
    int scrollbarht = SCROLLHT;
#endif
    x = xpos + 20;
    y = (TBARHT - (scrollbarht + 1)) / 2;
    framebar = new wxScrollBar(this, ID_SCROLLBAR, wxPoint(x, y),
                               wxSize(scrollbarwd, scrollbarht),
                               wxSB_HORIZONTAL);
    if (framebar == NULL) Fatal(_("Failed to create timeline scroll bar!"));
    
    xpos = x + scrollbarwd + 4;
    mindelpos = xpos;
    AddButton(DELETE_BUTT, _("Delete timeline"));
    // ResizeTimelineBar will move this button to right end of scroll bar

    // create timer for auto play
    autotimer = new wxTimer(this, ID_AUTOTIMER);
}

// -----------------------------------------------------------------------------

TimelineBar::~TimelineBar()
{
    delete timelinebitmap;
    delete slider;
    delete framebar;
    delete autotimer;
}

// -----------------------------------------------------------------------------

void TimelineBar::SetTimelineFont(wxDC& dc)
{
    dc.SetFont(timelinefont);
    dc.SetTextForeground(*wxBLACK);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.SetBackgroundMode(wxTRANSPARENT);
}

// -----------------------------------------------------------------------------

void TimelineBar::DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y)
{
    // DrawText's y parameter is top of text box but we pass in baseline
    // so adjust by textascent which depends on platform and OS version -- yuk!
    dc.DrawText(s, x, y - textascent);
}

// -----------------------------------------------------------------------------

void TimelineBar::DrawTimelineBar(wxDC& dc, int wd, int ht)
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
    
    // draw gray border line at top edge
#if defined(__WXMSW__)
    dc.SetPen(*wxGREY_PEN);
#elif defined(__WXMAC__)
    wxPen linepen(wxColor(140,140,140));
    dc.SetPen(linepen);
#else
    dc.SetPen(*wxLIGHT_GREY_PEN);
#endif
    dc.DrawLine(0, 0, r.width, 0);
    dc.SetPen(wxNullPen);
    
    if (currlayer->algo->hyperCapable()) {
        bool canplay = TimelineExists() && !currlayer->algo->isrecording();
        tlbutt[RECORD_BUTT]->Show(true);
        tlbutt[BACKWARDS_BUTT]->Show(canplay);
        tlbutt[FORWARDS_BUTT]->Show(canplay);
        tlbutt[DELETE_BUTT]->Show(canplay);
        slider->Show(canplay);
        framebar->Show(canplay);
        
        if (currlayer->algo->isrecording()) {
            // show number of frames recorded so far
            SetTimelineFont(dc);
            dc.SetPen(*wxBLACK_PEN);
            int x = smallgap + BUTTON_WD + 10;
            int y = TBARHT - 8;
            wxString str;
            str.Printf(_("Frames recorded: %d"), currlayer->algo->getframecount());
            DisplayText(dc, str, x, y - (SCROLLHT - digitht)/2);
            dc.SetPen(wxNullPen);
        }
        
    } else {
        tlbutt[RECORD_BUTT]->Show(false);
        tlbutt[BACKWARDS_BUTT]->Show(false);
        tlbutt[FORWARDS_BUTT]->Show(false);
        tlbutt[DELETE_BUTT]->Show(false);
        slider->Show(false);
        framebar->Show(false);
        
        SetTimelineFont(dc);
        dc.SetPen(*wxBLACK_PEN);
        int x = 6;
        int y = TBARHT - 8;
        DisplayText(dc, _("The current algorithm does not support timelines."),
                    x, y - (SCROLLHT - digitht)/2);
        dc.SetPen(wxNullPen);
    }
}

// -----------------------------------------------------------------------------

void TimelineBar::OnPaint(wxPaintEvent& WXUNUSED(event))
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
    if (wd != timelinebitmapwd || ht != timelinebitmapht) {
        // need to create a new bitmap for timeline bar
        delete timelinebitmap;
        timelinebitmap = new wxBitmap(wd, ht);
        timelinebitmapwd = wd;
        timelinebitmapht = ht;
    }
    if (timelinebitmap == NULL) Fatal(_("Not enough memory to render timeline bar!"));
    wxBufferedPaintDC dc(this, *timelinebitmap);
#endif
    
    if (showtimeline) DrawTimelineBar(dc, wd, ht);
}

// -----------------------------------------------------------------------------

void TimelineBar::OnMouseDown(wxMouseEvent& WXUNUSED(event))
{
    // on Win/Linux we need to reset keyboard focus to viewport window
    viewptr->SetFocus();
    
    mainptr->showbanner = false;
    statusptr->ClearMessage();
}

// -----------------------------------------------------------------------------

void TimelineBar::OnButton(wxCommandEvent& event)
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
        case RECORD_BUTT:    cmdid = ID_RECORD; break;
        case BACKWARDS_BUTT: PlayTimeline(-1); return;
        case FORWARDS_BUTT:  PlayTimeline(1); return;
        case DELETE_BUTT:    cmdid = ID_DELTIME; break;
        default:             Warning(_("Unexpected button id!")); return;
    }
    
    // call MainFrame::OnMenu after OnButton finishes
    wxCommandEvent cmdevt(wxEVT_COMMAND_MENU_SELECTED, cmdid);
    wxPostEvent(mainptr->GetEventHandler(), cmdevt);
    
    // avoid possible problems
    viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void TimelineBar::OnSlider(wxScrollEvent& event)
{
    WXTYPE type = event.GetEventType();
    
    if (type == wxEVT_SCROLL_LINEUP) {
        currlayer->tlspeed--;
        if (currlayer->tlspeed < MINSPEED) currlayer->tlspeed = MINSPEED;
        StartAutoTimer();
        
    } else if (type == wxEVT_SCROLL_LINEDOWN) {
        currlayer->tlspeed++;
        if (currlayer->tlspeed > MAXSPEED) currlayer->tlspeed = MAXSPEED;
        StartAutoTimer();
        
    } else if (type == wxEVT_SCROLL_PAGEUP) {
        currlayer->tlspeed -= PAGESIZE;
        if (currlayer->tlspeed < MINSPEED) currlayer->tlspeed = MINSPEED;
        StartAutoTimer();
        
    } else if (type == wxEVT_SCROLL_PAGEDOWN) {
        currlayer->tlspeed += PAGESIZE;
        if (currlayer->tlspeed > MAXSPEED) currlayer->tlspeed = MAXSPEED;
        StartAutoTimer();
        
    } else if (type == wxEVT_SCROLL_THUMBTRACK) {
        currlayer->tlspeed = event.GetPosition();
        if (currlayer->tlspeed < MINSPEED) currlayer->tlspeed = MINSPEED;
        if (currlayer->tlspeed > MAXSPEED) currlayer->tlspeed = MAXSPEED;
        StartAutoTimer();
        
    } else if (type == wxEVT_SCROLL_THUMBRELEASE) {
        UpdateSlider();
        StartAutoTimer();
    }
    
#ifndef __WXMAC__
    viewptr->SetFocus();    // need on Win/Linux
#endif
}

// -----------------------------------------------------------------------------

void TimelineBar::DisplayCurrentFrame()
{
    currlayer->algo->gotoframe(currlayer->currframe);
    
    // FitInView(0) would be less jerky but has the disadvantage that
    // scale won't change if a pattern shrinks when going backwards
    if (currlayer->autofit) viewptr->FitInView(1);
    
    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void TimelineBar::OnScroll(wxScrollEvent& event)
{
    WXTYPE type = event.GetEventType();
    
    // stop autoplay if scroll bar is used
    if (currlayer->autoplay != 0) {
        currlayer->autoplay = 0;
        StopAutoTimer();
        mainptr->UpdateUserInterface();
    }
    
    if (type == wxEVT_SCROLL_LINEUP) {
        currlayer->currframe--;
        if (currlayer->currframe < 0) currlayer->currframe = 0;
        DisplayCurrentFrame();
        
    } else if (type == wxEVT_SCROLL_LINEDOWN) {
        currlayer->currframe++;
        if (currlayer->currframe >= currlayer->algo->getframecount())
            currlayer->currframe = currlayer->algo->getframecount() - 1;
        DisplayCurrentFrame();
        
    } else if (type == wxEVT_SCROLL_PAGEUP) {
        currlayer->currframe -= PAGESIZE;
        if (currlayer->currframe < 0) currlayer->currframe = 0;
        DisplayCurrentFrame();
        
    } else if (type == wxEVT_SCROLL_PAGEDOWN) {
        currlayer->currframe += PAGESIZE;
        if (currlayer->currframe >= currlayer->algo->getframecount())
            currlayer->currframe = currlayer->algo->getframecount() - 1;
        DisplayCurrentFrame();
        
    } else if (type == wxEVT_SCROLL_THUMBTRACK) {
        currlayer->currframe = event.GetPosition();
        if (currlayer->currframe < 0)
            currlayer->currframe = 0;
        if (currlayer->currframe >= currlayer->algo->getframecount())
            currlayer->currframe = currlayer->algo->getframecount() - 1;
        DisplayCurrentFrame();
        
    } else if (type == wxEVT_SCROLL_THUMBRELEASE) {
        UpdateScrollBar();
    }
    
#ifndef __WXMAC__
    viewptr->SetFocus();    // need on Win/Linux
#endif
}

// -----------------------------------------------------------------------------

void TimelineBar::OnKillFocus(wxFocusEvent& event)
{
    int id = event.GetId();
    tlbutt[id]->SetFocus();   // don't let button lose focus
}

// -----------------------------------------------------------------------------

void TimelineBar::OnButtonDown(wxMouseEvent& event)
{
    // timeline bar button has been pressed
    int id = event.GetId();
    
    // connect a handler that keeps focus with the pressed button
    tlbutt[id]->Connect(id, wxEVT_KILL_FOCUS, wxFocusEventHandler(TimelineBar::OnKillFocus));
    
    event.Skip();
}

// -----------------------------------------------------------------------------

void TimelineBar::OnButtonUp(wxMouseEvent& event)
{
    // timeline bar button has been released
    int id = event.GetId();
    
    wxPoint pt = tlbutt[id]->ScreenToClient( wxGetMousePosition() );
    
    int wd, ht;
    tlbutt[id]->GetClientSize(&wd, &ht);
    wxRect r(0, 0, wd, ht);
    
    // diconnect kill-focus handler
    tlbutt[id]->Disconnect(id, wxEVT_KILL_FOCUS, wxFocusEventHandler(TimelineBar::OnKillFocus));
    viewptr->SetFocus();
    
    if (r.Contains(pt)) {
        // call OnButton
        wxCommandEvent buttevt(wxEVT_COMMAND_BUTTON_CLICKED, id);
        buttevt.SetEventObject(tlbutt[id]);
        tlbutt[id]->GetEventHandler()->ProcessEvent(buttevt);
    }
}

// -----------------------------------------------------------------------------

void TimelineBar::AddButton(int id, const wxString& tip)
{
    tlbutt[id] = new wxBitmapButton(this, id, normbutt[id], wxPoint(xpos,ypos),
                                    wxSize(BUTTON_WD, BUTTON_HT), wxBORDER_NONE);
    if (tlbutt[id] == NULL) {
        Fatal(_("Failed to create timeline bar button!"));
    } else {
        xpos += BUTTON_WD + smallgap;
        tlbutt[id]->SetToolTip(tip);
#ifdef __WXMSW__
        // fix problem with timeline bar buttons when generating/inscript
        // due to focus being changed to viewptr
        tlbutt[id]->Connect(id, wxEVT_LEFT_DOWN, wxMouseEventHandler(TimelineBar::OnButtonDown));
        tlbutt[id]->Connect(id, wxEVT_LEFT_UP, wxMouseEventHandler(TimelineBar::OnButtonUp));
#endif
    }
}

// -----------------------------------------------------------------------------

void TimelineBar::AddSeparator()
{
    xpos += biggap - smallgap;
}

// -----------------------------------------------------------------------------

void TimelineBar::EnableButton(int id, bool enable)
{
    if (enable == tlbutt[id]->IsEnabled()) return;
    
#ifdef __WXMSW__
    tlbutt[id]->SetBitmapDisabled(disnormbutt[id]);
#endif
    
    tlbutt[id]->Enable(enable);
}

// -----------------------------------------------------------------------------

void TimelineBar::UpdateButtons()
{
    if (currlayer->algo->isrecording()) {
        if (buttstate[RECORD_BUTT] != -1) {
            buttstate[RECORD_BUTT] = -1;
            tlbutt[RECORD_BUTT]->SetBitmapLabel(normbutt[STOPREC_BUTT]);
            tlbutt[RECORD_BUTT]->SetToolTip(_("Stop recording"));
            if (showtimeline) tlbutt[RECORD_BUTT]->Refresh(false);
        }
    } else {
        if (buttstate[RECORD_BUTT] != 1) {
            buttstate[RECORD_BUTT] = 1;
            tlbutt[RECORD_BUTT]->SetBitmapLabel(normbutt[RECORD_BUTT]);
            tlbutt[RECORD_BUTT]->SetToolTip(_("Start recording"));
            if (showtimeline) tlbutt[RECORD_BUTT]->Refresh(false);
        }
    }
    
    // these buttons are only shown if there is a timeline and we're not recording
    // (see DrawTimelineBar)
    if (TimelineExists() && !currlayer->algo->isrecording()) {
        if (currlayer->autoplay == 0) {
            if (buttstate[BACKWARDS_BUTT] != 1) {
                buttstate[BACKWARDS_BUTT] = 1;
                tlbutt[BACKWARDS_BUTT]->SetBitmapLabel(normbutt[BACKWARDS_BUTT]);
                tlbutt[BACKWARDS_BUTT]->SetToolTip(_("Play backwards"));
                if (showtimeline) tlbutt[BACKWARDS_BUTT]->Refresh(false);
            }
            if (buttstate[FORWARDS_BUTT] != 1) {
                buttstate[FORWARDS_BUTT] = 1;
                tlbutt[FORWARDS_BUTT]->SetBitmapLabel(normbutt[FORWARDS_BUTT]);
                tlbutt[FORWARDS_BUTT]->SetToolTip(_("Play forwards"));
                if (showtimeline) tlbutt[FORWARDS_BUTT]->Refresh(false);
            }
        } else if (currlayer->autoplay > 0) {
            if (buttstate[BACKWARDS_BUTT] != 1) {
                buttstate[BACKWARDS_BUTT] = 1;
                tlbutt[BACKWARDS_BUTT]->SetBitmapLabel(normbutt[BACKWARDS_BUTT]);
                tlbutt[BACKWARDS_BUTT]->SetToolTip(_("Play backwards"));
                if (showtimeline) tlbutt[BACKWARDS_BUTT]->Refresh(false);
            }
            if (buttstate[FORWARDS_BUTT] != -1) {
                buttstate[FORWARDS_BUTT] = -1;
                tlbutt[FORWARDS_BUTT]->SetBitmapLabel(normbutt[STOPPLAY_BUTT]);
                tlbutt[FORWARDS_BUTT]->SetToolTip(_("Stop playing"));
                if (showtimeline) tlbutt[FORWARDS_BUTT]->Refresh(false);
            }
        } else { // currlayer->autoplay < 0
            if (buttstate[BACKWARDS_BUTT] != -1) {
                buttstate[BACKWARDS_BUTT] = -1;
                tlbutt[BACKWARDS_BUTT]->SetBitmapLabel(normbutt[STOPPLAY_BUTT]);
                tlbutt[BACKWARDS_BUTT]->SetToolTip(_("Stop playing"));
                if (showtimeline) tlbutt[BACKWARDS_BUTT]->Refresh(false);
            }
            if (buttstate[FORWARDS_BUTT] != 1) {
                buttstate[FORWARDS_BUTT] = 1;
                tlbutt[FORWARDS_BUTT]->SetBitmapLabel(normbutt[FORWARDS_BUTT]);
                tlbutt[FORWARDS_BUTT]->SetToolTip(_("Play forwards"));
                if (showtimeline) tlbutt[FORWARDS_BUTT]->Refresh(false);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void TimelineBar::UpdateSlider()
{
    slider->SetValue(currlayer->tlspeed);
}

// -----------------------------------------------------------------------------

void TimelineBar::UpdateScrollBar()
{
    framebar->SetScrollbar(currlayer->currframe, 1,
                           currlayer->algo->getframecount(), PAGESIZE, true);
}

// -----------------------------------------------------------------------------

void TimelineBar::OnAutoTimer(wxTimerEvent& WXUNUSED(event))
{
    if (currlayer->autoplay == 0) return;
    if (currlayer->algo->isrecording()) return;
    // assume currlayer->algo->getframecount() > 0
    
    int frameinc = 1;
    if (currlayer->tlspeed > 0) {
        // skip 2^tlspeed frames
        frameinc = 1 << currlayer->tlspeed;
    }
    
    if (currlayer->autoplay > 0) {
        // play timeline forwards
        currlayer->currframe += frameinc;
        if (currlayer->currframe >= currlayer->algo->getframecount() - 1) {
            currlayer->currframe = currlayer->algo->getframecount() - 1;
            currlayer->autoplay = -1;     // reverse direction when we hit last frame
            UpdateButtons();
        }
    } else {
        // currlayer->autoplay < 0 so play timeline backwards
        currlayer->currframe -= frameinc;
        if (currlayer->currframe <= 0) {
            currlayer->currframe = 0;
            currlayer->autoplay = 1;      // reverse direction when we hit first frame
            UpdateButtons();
        }
    }
    
    DisplayCurrentFrame();
    UpdateScrollBar();
}

// -----------------------------------------------------------------------------

void TimelineBar::StartAutoTimer()
{
    if (currlayer->autoplay == 0) return;

    int interval = SIXTY_HERTZ;     // do ~60 calls of OnAutoTimer per sec
    
    // increase interval if user wants a delay between each frame
    if (currlayer->tlspeed < 0) {
        interval = 100 * (-currlayer->tlspeed);
        // if tlspeed is -1 then interval is 100; ie. call OnAutoTimer 10 times per sec
    }
    
    StopAutoTimer();
    autotimer->Start(interval, wxTIMER_CONTINUOUS);
}

// -----------------------------------------------------------------------------

void TimelineBar::StopAutoTimer()
{
    if (autotimer->IsRunning()) autotimer->Stop();
}

// -----------------------------------------------------------------------------

void CreateTimelineBar(wxWindow* parent)
{
    // create timeline bar underneath given window
    int wd, ht;
    parent->GetClientSize(&wd, &ht);
    
    tbarptr = new TimelineBar(parent, 0, ht - TBARHT, wd, TBARHT);
    if (tbarptr == NULL) Fatal(_("Failed to create timeline bar!"));
    
    tbarptr->Show(showtimeline);
}

// -----------------------------------------------------------------------------

int TimelineBarHeight() {
    return (showtimeline ? TBARHT : 0);
}

// -----------------------------------------------------------------------------

void UpdateTimelineBar()
{
    if (tbarptr && showtimeline && !mainptr->IsIconized()) {
        bool active = !inscript;
        
        // may need to change bitmaps in some buttons
        tbarptr->UpdateButtons();
        
        tbarptr->EnableButton(RECORD_BUTT, active && currlayer->algo->hyperCapable());
        
        // note that slider, scroll bar and some buttons are only shown if there is
        // a timeline and we're not recording (see DrawTimelineBar)
        if (TimelineExists() && !currlayer->algo->isrecording()) {
            tbarptr->EnableButton(BACKWARDS_BUTT, active);
            tbarptr->EnableButton(FORWARDS_BUTT, active);
            tbarptr->EnableButton(DELETE_BUTT, active);
            tbarptr->UpdateSlider();
            tbarptr->UpdateScrollBar();
        }
        
        if (currlayer->algo->isrecording()) {
            // don't refresh RECORD_BUTT (otherwise button flickers on Windows)
            int wd, ht;
            tbarptr->GetClientSize(&wd, &ht);
            wxRect r(BUTTON_WD + tbarptr->smallgap * 2, 0, wd, ht);
            tbarptr->RefreshRect(r, false);
        } else {
            tbarptr->Refresh(false);
        }
        #ifdef __WXGTK__
            // avoid bug that can cause buttons to lose their bitmaps
            tbarptr->Update();
        #endif
    }
}

// -----------------------------------------------------------------------------

void ResizeTimelineBar(int y, int wd)
{
    if (tbarptr && showtimeline) {
        tbarptr->SetSize(0, y, wd, TBARHT);
        // change width of scroll bar to nearly fill timeline bar
        wxRect r = tbarptr->framebar->GetRect();
        r.width = wd - r.x - 20 - BUTTON_WD - 20;
        if (r.width < 0) r.width = 0;
        tbarptr->framebar->SetSize(r);
        
        // move DELETE_BUTT to right edge of timeline bar
        r = tlbutt[DELETE_BUTT]->GetRect();
        r.x = wd - 20 - BUTTON_WD;
        if (r.x < mindelpos && TimelineExists()) r.x = mindelpos;
        tlbutt[DELETE_BUTT]->SetSize(r);
    }
}

// -----------------------------------------------------------------------------

void ToggleTimelineBar()
{
    showtimeline = !showtimeline;
    mainptr->ResizeBigView();
    tbarptr->Show(showtimeline);    // needed on Windows
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void StartStopRecording()
{
    if (!inscript && currlayer->algo->hyperCapable()) {
        if (currlayer->algo->isrecording()) {
            mainptr->Stop();
            // StopGenerating() has called currlayer->algo->stoprecording()
        
        } else {
            tbarptr->StopAutoTimer();
            
            if (currlayer->algo->isEmpty()) {
                statusptr->ErrorMessage(_("There is no pattern to record."));
                return;
            }
            
            if (!showtimeline) ToggleTimelineBar();
            
            if (currlayer->algo->getframecount() == MAX_FRAME_COUNT) {
                wxString msg;
                msg.Printf(_("The timeline can't be extended any further (max frames = %d)."),
                           MAX_FRAME_COUNT);
                statusptr->ErrorMessage(msg);
                return;
            }
            
            // record a new timeline, or extend the existing one
            if (currlayer->algo->startrecording(currlayer->currbase, currlayer->currexpo) > 0) {
                if (currlayer->algo->getGeneration() == currlayer->startgen) {
                    // ensure the SaveStartingPattern call in DeleteTimeline will
                    // create a new temporary .mc file (with only one frame)
                    currlayer->savestart = true;
                }
                
                mainptr->StartGenerating();
                
            } else {
                // this should never happen
                Warning(_("Bug: could not start recording!"));
            }
        }
    }
}

// -----------------------------------------------------------------------------

void DeleteTimeline()
{
    if (!inscript && TimelineExists() && !currlayer->algo->isrecording()) {

        tbarptr->StopAutoTimer();
        
        if (currlayer->currframe > 0) {
            // tell writeNativeFormat to only save the current frame
            // so that the temporary .mc files created by SaveStartingPattern and
            // RememberGenStart/Finish won't store the entire timeline
            currlayer->algo->savetimelinewithframe(0);
            
            // do stuff so user can select Reset/Undo to go back to 1st frame
            currlayer->algo->gotoframe(0);
            if (currlayer->autofit) viewptr->FitInView(1);
            if (currlayer->algo->getGeneration() == currlayer->startgen) {
                mainptr->SaveStartingPattern();
            }
            if (allowundo) currlayer->undoredo->RememberGenStart();
            
            // return to the current frame
            currlayer->algo->gotoframe(currlayer->currframe);
            if (currlayer->autofit) viewptr->FitInView(1);
            if (allowundo) currlayer->undoredo->RememberGenFinish();
            
            // restore flag that tells writeNativeFormat to save entire timeline
            currlayer->algo->savetimelinewithframe(1);
        }
        
        currlayer->algo->destroytimeline();
        mainptr->UpdateUserInterface();
    }
}

// -----------------------------------------------------------------------------

void InitTimelineFrame()
{
    // the user has just loaded a .mc file with a timeline,
    // so prepare to display the first frame
    currlayer->algo->gotoframe(0);
    currlayer->currframe = 0;
    currlayer->autoplay = 0;
    currlayer->tlspeed = 0;
    tbarptr->StopAutoTimer();
    
    // first frame is starting gen (needed for DeleteTimeline)
    currlayer->startgen = currlayer->algo->getGeneration();
    
    // ensure SaveStartingPattern call in DeleteTimeline will create
    // a new temporary .mc file with one frame
    currlayer->savestart = true;
}

// -----------------------------------------------------------------------------

bool TimelineExists()
{
    return currlayer->algo->getframecount() > 0;
}

// -----------------------------------------------------------------------------

void PlayTimeline(int direction)
{
    if (currlayer->algo->isrecording()) return;
    if (direction > 0 && currlayer->autoplay > 0) {
        currlayer->autoplay = 0;
    } else if (direction < 0 && currlayer->autoplay < 0) {
        currlayer->autoplay = 0;
    } else {
        currlayer->autoplay = direction;
    }
    
    if (currlayer->autoplay == 0) {
        tbarptr->StopAutoTimer();
    } else {
        tbarptr->StartAutoTimer();
    }
    
    mainptr->UpdateUserInterface();
}

// -----------------------------------------------------------------------------

void PlayTimelineFaster()
{
    if (currlayer->algo->isrecording()) return;
    if (currlayer->tlspeed < MAXSPEED) {
        currlayer->tlspeed++;
        if (showtimeline) tbarptr->UpdateSlider();
        tbarptr->StartAutoTimer();
    }
}

// -----------------------------------------------------------------------------

void PlayTimelineSlower()
{
    if (currlayer->algo->isrecording()) return;
    if (currlayer->tlspeed > MINSPEED) {
        currlayer->tlspeed--;
        if (showtimeline) tbarptr->UpdateSlider();
        tbarptr->StartAutoTimer();
    }
}

// -----------------------------------------------------------------------------

void ResetTimelineSpeed()
{
    if (currlayer->algo->isrecording()) return;
    currlayer->tlspeed = 0;
    if (showtimeline) tbarptr->UpdateSlider();
    tbarptr->StartAutoTimer();
}

// -----------------------------------------------------------------------------

bool TimelineIsPlaying()
{
    return currlayer->autoplay != 0;
}

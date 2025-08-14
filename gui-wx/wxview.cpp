// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h" // for wxToolTip
#endif
#include "wx/file.h"        // for wxFile

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for mainptr, statusptr
#include "wxutils.h"       // for Warning, Fatal, Beep
#include "wxprefs.h"       // for showgridlines, canchangerule, etc
#include "wxhelp.h"        // for ShowHelp
#include "wxmain.h"        // for mainptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxrender.h"      // for InitPaste, DrawView
#include "wxscript.h"      // for inscript, PassKeyToScript, PassClickToScript, etc
#include "wxselect.h"      // for Selection
#include "wxedit.h"        // for UpdateEditBar, ToggleEditBar, etc
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxalgos.h"       // for algo_type, *_ALGO, CreateNewUniverse, etc
#include "wxlayer.h"       // for currlayer, ResizeLayers, etc
#include "wxoverlay.h"     // for curroverlay
#include "wxtimeline.h"    // for StartStopRecording, DeleteTimeline, etc
#include "wxview.h"

// -----------------------------------------------------------------------------

static bool stopdrawing = false;    // terminate a draw done while generating?

static Layer* pastelayer = NULL;    // temporary layer with pattern to be pasted
static wxRect pastebox;             // bounding box for paste pattern
static wxString oldrule;            // rule before readclipboard is called
static wxString newrule;            // rule after readclipboard is called

// remember which translucent button was clicked, and when
static control_id clickedcontrol = NO_CONTROL;
static long clicktime;

// panning buttons are treated differently
#define PANNING_CONTROL (clickedcontrol >= NW_CONTROL && \
                         clickedcontrol <= SE_CONTROL && \
                         clickedcontrol != MIDDLE_CONTROL)

// this determines the rate at which OnDragTimer will be called after the mouse
// is dragged outside the viewport but then kept still (note that OnMouseMotion
// calls OnDragTimer when the mouse is moved, inside or outside the viewport)
const int TEN_HERTZ = 100;

// OpenGL parameters
int glMajor = 0;                     // major version
int glMinor = 0;                     // minor version
int glMaxTextureSize = 1024;         // maximum texture size

// -----------------------------------------------------------------------------

// event table and handlers:

BEGIN_EVENT_TABLE(PatternView, wxGLCanvas)
EVT_PAINT            (           PatternView::OnPaint)
EVT_SIZE             (           PatternView::OnSize)
#ifdef __WXMAC__
EVT_CHAR_HOOK        (           PatternView::OnCharHook)
#endif
EVT_KEY_DOWN         (           PatternView::OnKeyDown)
EVT_KEY_UP           (           PatternView::OnKeyUp)
EVT_CHAR             (           PatternView::OnChar)
EVT_LEFT_DOWN        (           PatternView::OnMouseDown)
EVT_LEFT_DCLICK      (           PatternView::OnMouseDown)
EVT_RIGHT_DOWN       (           PatternView::OnMouseDown)
EVT_RIGHT_DCLICK     (           PatternView::OnMouseDown)
EVT_MIDDLE_DOWN      (           PatternView::OnMouseDown)
EVT_MIDDLE_DCLICK    (           PatternView::OnMouseDown)
EVT_LEFT_UP          (           PatternView::OnMouseUp)
EVT_RIGHT_UP         (           PatternView::OnMouseUp)
EVT_MIDDLE_UP        (           PatternView::OnMouseUp)
EVT_MOUSE_CAPTURE_LOST (         PatternView::OnMouseCaptureLost)
EVT_MOTION           (           PatternView::OnMouseMotion)
EVT_ENTER_WINDOW     (           PatternView::OnMouseEnter)
EVT_LEAVE_WINDOW     (           PatternView::OnMouseExit)
EVT_MOUSEWHEEL       (           PatternView::OnMouseWheel)
EVT_TIMER            (wxID_ANY,  PatternView::OnDragTimer)
EVT_SCROLLWIN        (           PatternView::OnScroll)
EVT_ERASE_BACKGROUND (           PatternView::OnEraseBackground)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

static void RefreshView()
{
    // refresh main viewport window, including all tile windows if they exist
    // (tile windows are children of bigview)
    if (!mainptr->IsIconized()) bigview->Refresh(false);
}

// -----------------------------------------------------------------------------

bool PatternView::OutsideLimits(bigint& t, bigint& l, bigint& b, bigint& r)
{
    return ( t < bigint::min_coord || l < bigint::min_coord ||
             b > bigint::max_coord || r > bigint::max_coord );
}

// -----------------------------------------------------------------------------

bool PatternView::SelectionExists()
{
    return currlayer->currsel.Exists();
}

// -----------------------------------------------------------------------------

bool PatternView::CopyRect(int itop, int ileft, int ibottom, int iright,
                           lifealgo* srcalgo, lifealgo* destalgo,
                           bool erasesrc, const wxString& progmsg)
{
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    int cx, cy;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    int v = 0;
    bool abort = false;
    
    // copy (and erase if requested) live cells from given rect
    // in source universe to same rect in destination universe
    BeginProgress(progmsg);
    for ( cy=itop; cy<=ibottom; cy++ ) {
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = srcalgo->nextcell(cx, cy, v);
            if (skip + cx > iright)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                destalgo->setcell(cx, cy, v);
                if (erasesrc) srcalgo->setcell(cx, cy, 0);
            } else {
                cx = iright + 1;     // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0) {
                double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                               (cx - ileft)) / maxcount;
                abort = AbortProgress(prog, wxEmptyString);
                if (abort) break;
            }
        }
        if (abort) break;
    }
    if (erasesrc) srcalgo->endofpattern();
    destalgo->endofpattern();
    EndProgress();
    
    return !abort;
}

// -----------------------------------------------------------------------------

void PatternView::CopyAllRect(int itop, int ileft, int ibottom, int iright,
                              lifealgo* srcalgo, lifealgo* destalgo,
                              const wxString& progmsg)
{
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    int cx, cy;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;
    
    // copy all cells from given rect in srcalgo to same rect in destalgo
    BeginProgress(progmsg);
    for ( cy=itop; cy<=ibottom; cy++ ) {
        for ( cx=ileft; cx<=iright; cx++ ) {
            destalgo->setcell(cx, cy, srcalgo->getcell(cx, cy));
            cntr++;
            if ((cntr % 4096) == 0) {
                abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
                if (abort) break;
            }
        }
        if (abort) break;
    }
    destalgo->endofpattern();
    EndProgress();
}

// -----------------------------------------------------------------------------

void PatternView::ClearSelection()
{
    currlayer->currsel.Clear();
}

// -----------------------------------------------------------------------------

void PatternView::ClearOutsideSelection()
{
    currlayer->currsel.ClearOutside();
}

// -----------------------------------------------------------------------------

void PatternView::CutSelection()
{
    if (!SelectionExists()) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_CUT);
        mainptr->Stop();
        return;
    }
    
    currlayer->currsel.CopyToClipboard(true);
}

// -----------------------------------------------------------------------------

void PatternView::CopySelection()
{
    if (!SelectionExists()) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_COPY);
        mainptr->Stop();
        return;
    }
    
    currlayer->currsel.CopyToClipboard(false);
}

// -----------------------------------------------------------------------------

bool PatternView::CellInGrid(const bigint& x, const bigint& y)
{
    // return true if cell at x,y is within bounded grid
    if (currlayer->algo->gridwd > 0 &&
        (x < currlayer->algo->gridleft ||
         x > currlayer->algo->gridright)) return false;
    
    if (currlayer->algo->gridht > 0 &&
        (y < currlayer->algo->gridtop ||
         y > currlayer->algo->gridbottom)) return false;
    
    return true;
}

// -----------------------------------------------------------------------------

bool PatternView::PointInGrid(int x, int y)
{
    // is given viewport location also in grid?
    if (currlayer->algo->gridwd == 0 && currlayer->algo->gridht == 0) {
        // unbounded grid
        return true;
    }
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    return CellInGrid(cellpos.first, cellpos.second);
}

// -----------------------------------------------------------------------------

bool PatternView::RectOutsideGrid(const wxRect& rect)
{
    if (currlayer->algo->gridwd == 0 && currlayer->algo->gridht == 0) {
        // unbounded grid
        return false;
    }
    
    pair<bigint, bigint> lt = currlayer->view->at(rect.x, rect.y);
    pair<bigint, bigint> rb = currlayer->view->at(rect.x+rect.width-1, rect.y+rect.height-1);
    
    return (currlayer->algo->gridwd > 0 &&
                (lt.first > currlayer->algo->gridright ||
                 rb.first < currlayer->algo->gridleft)) ||
           (currlayer->algo->gridht > 0 &&
                (lt.second > currlayer->algo->gridbottom ||
                 rb.second < currlayer->algo->gridtop));
}

// -----------------------------------------------------------------------------

void PatternView::SetPasteRect(wxRect& rect, bigint& wd, bigint& ht)
{
    int x, y, pastewd, pasteht;
    int mag = currlayer->view->getmag();
    
    // find cell coord of current paste cursor position
    pair<bigint, bigint> pcell = currlayer->view->at(pastex, pastey);
    
    // determine bottom right cell
    bigint right = pcell.first;     right += wd;    right -= 1;
    bigint bottom = pcell.second;   bottom += ht;   bottom -= 1;
    
    // best to use same method as in Selection::Visible
    pair<int,int> lt = currlayer->view->screenPosOf(pcell.first, pcell.second, currlayer->algo);
    pair<int,int> rb = currlayer->view->screenPosOf(right, bottom, currlayer->algo);
    
    if (mag > 0) {
        // move rb to pixel at bottom right corner of cell
        rb.first += (1 << mag) - 1;
        rb.second += (1 << mag) - 1;
        if (mag > 1) {
            // avoid covering gaps at scale 1:4 and above
            rb.first--;
            rb.second--;
        }
    }
    
    x = lt.first;
    y = lt.second;
    pastewd = rb.first - lt.first + 1;
    pasteht = rb.second - lt.second + 1;
    
    // this should never happen but play safe
    if (pastewd <= 0) pastewd = 1;
    if (pasteht <= 0) pasteht = 1;
    
    rect = wxRect(x, y, pastewd, pasteht);
    int xoffset, yoffset;
    int cellsize = 1 << mag;      // only used if mag > 0
    int gap = 1;                  // ditto
    if (mag == 1) gap = 0;        // but no gap between cells at scale 1:2
    switch (plocation) {
        case TopLeft:
            break;
        case TopRight:
            xoffset = mag > 0 ? -(pastewd - cellsize + gap) : -pastewd + 1;
            rect.Offset(xoffset, 0);
            break;
        case BottomRight:
            xoffset = mag > 0 ? -(pastewd - cellsize + gap) : -pastewd + 1;
            yoffset = mag > 0 ? -(pasteht - cellsize + gap) : -pasteht + 1;
            rect.Offset(xoffset, yoffset);
            break;
        case BottomLeft:
            yoffset = mag > 0 ? -(pasteht - cellsize + gap) : -pasteht + 1;
            rect.Offset(0, yoffset);
            break;
        case Middle:
            xoffset = mag > 0 ? -(pastewd / cellsize / 2) * cellsize : -pastewd / 2;
            yoffset = mag > 0 ? -(pasteht / cellsize / 2) * cellsize : -pasteht / 2;
            rect.Offset(xoffset, yoffset);
            break;
    }
}

// -----------------------------------------------------------------------------

void PatternView::PasteTemporaryToCurrent(bool toselection,
                                          bigint top, bigint left, bigint bottom, bigint right)
{
    // make sure given edges are within getcell/setcell limits
    if ( OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(_("Clipboard pattern is too big."));
        return;
    }
    int itop = top.toint();
    int ileft = left.toint();
    int ibottom = bottom.toint();
    int iright = right.toint();
    bigint wd = iright - ileft + 1;
    bigint ht = ibottom - itop + 1;
    
    if ( toselection ) {
        if ( !currlayer->currsel.CanPaste(wd, ht, top, left) ) {
            statusptr->ErrorMessage(_("Clipboard pattern is bigger than selection."));
            return;
        }
        // top and left have been set to the selection's top left corner
        
    } else {
        // ask user where to paste the clipboard pattern
        statusptr->DisplayMessage(_("Click where you want to paste..."));
        
        // temporarily change cursor to cross
        wxCursor* savecurs = currlayer->curs;
        currlayer->curs = curs_cross;
        CheckCursor(true);
        
        // pastelayer contains the pattern to be pasted; note that pastebox
        // is not necessarily the minimal bounding box because clipboard pattern
        // might have blank borders (in fact it could be empty)
        pastebox = wxRect(ileft, itop, wd.toint(), ht.toint());
        InitPaste(pastelayer, pastebox);
        
        waitingforclick = true;
        mainptr->UpdateMenuAccelerators();  // remove all accelerators so keyboard shortcuts can be used
        mainptr->EnableAllMenus(false);     // disable all menu items
        mainptr->UpdateToolBar();           // disable all tool bar buttons
        UpdateLayerBar();                   // disable all layer bar buttons
        UpdateEditBar();                    // disable all edit bar buttons
        if (!HasCapture()) CaptureMouse();  // get mouse down event even if outside view
        pasterect = wxRect(-1,-1,0,0);
        
        while (waitingforclick) {
            wxPoint pt = ScreenToClient( wxGetMousePosition() );
            pt.x *= int(scalefactor);
            pt.y *= int(scalefactor);
            pastex = pt.x;
            pastey = pt.y;
            if (PointInView(pt.x, pt.y)) {
                // determine new paste rectangle
                wxRect newrect;
                if (wd.toint() != pastebox.width || ht.toint() != pastebox.height) {
                    // RotatePastePattern was called
                    itop = pastebox.y;
                    ileft = pastebox.x;
                    ibottom = itop + pastebox.height - 1;
                    iright = ileft + pastebox.width - 1;
                    wd = pastebox.width;
                    ht = pastebox.height;
                }
                SetPasteRect(newrect, wd, ht);
                if (newrect != pasterect) {
                    // draw new pasterect
                    pasterect = newrect;
                    Refresh(false);
                    // don't update immediately
                }
            } else {
                // mouse outside viewport so erase old pasterect if necessary
                if ( pasterect.width > 0 ) {
                    pasterect = wxRect(-1,-1,0,0);
                    Refresh(false);
                    // don't update immediately
                }
            }
            wxMilliSleep(10);             // don't hog CPU
            insideYield++;
            wxGetApp().Yield(true);
            insideYield--;
            // make sure viewport retains focus so we can use keyboard shortcuts
            SetFocus();
            // waitingforclick becomes false if OnMouseDown is called
        }

        #ifdef __WXMAC__
            // avoid problem on Mac due to OnMouseUp not being called
            mouseisdown = false;
        #endif

        if (HasCapture()) ReleaseMouse();
        mainptr->EnableAllMenus(true);
        mainptr->UpdateMenuAccelerators();  // restore accelerators
        
        // restore cursor
        currlayer->curs = savecurs;
        CheckCursor(mainptr->infront);
        
        if ( pasterect.width > 0 ) {
            // erase old pasterect
            Refresh(false);
        }
        
        // only allow paste if some part of pasterect is within the (possibly bounded) grid
        if ( !PointInView(pastex, pastey) || RectOutsideGrid(pasterect) ) {
            statusptr->DisplayMessage(_("Paste aborted."));
            return;
        }
        
        // set paste rectangle's top left cell coord
        pair<bigint, bigint> clickpos = currlayer->view->at(pastex, pastey);
        top = clickpos.second;
        left = clickpos.first;
        bigint halfht = ht;
        bigint halfwd = wd;
        halfht.div2();
        halfwd.div2();
        if (currlayer->view->getmag() > 1) {
            if (ht.even()) halfht -= 1;
            if (wd.even()) halfwd -= 1;
        }
        switch (plocation) {
            case TopLeft:     /* no change*/ break;
            case TopRight:    left -= wd; left += 1; break;
            case BottomRight: left -= wd; left += 1; top -= ht; top += 1; break;
            case BottomLeft:  top -= ht; top += 1; break;
            case Middle:      left -= halfwd; top -= halfht; break;
        }
    }
    
    // check that paste rectangle is within edit limits
    bottom = top;   bottom += ht;   bottom -= 1;
    right = left;   right += wd;    right -= 1;
    if ( OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(_("Pasting is not allowed outside +/- 10^9 boundary."));
        return;
    }
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    SaveCurrentSelection();
    
    // pasting clipboard pattern can cause a rule change
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;
    if (canchangerule > 0 && oldrule != newrule) {
        const char* err = currlayer->algo->setrule( newrule.mb_str(wxConvLocal) );
        // setrule can fail if readclipboard loaded clipboard pattern into
        // a different type of algo
        if (err) {
            // ChangeAlgorithm will call currlayer->algo->getrule() which can return an empty string
            // for some algos after currlayer->algo->setrule fails, so we need to restore oldrule now
            currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
            // allow rule change to cause algo change
            mainptr->ChangeAlgorithm(pastelayer->algtype, newrule);
        } else {
            // show new rule in title bar
            mainptr->SetWindowTitle(wxEmptyString);

            // if pattern exists and is at starting gen then ensure savestart is true
            // so that SaveStartingPattern will save pattern to suitable file
            // (and thus undo/reset will work correctly)
            if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
                currlayer->savestart = true;
            }

            // if grid is bounded then remove any live cells outside grid edges
            if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
                mainptr->ClearOutsideGrid();
            }
            
            // rule change might have changed the number of cell states;
            // if there are fewer states then pattern might change
            int newmaxstate = currlayer->algo->NumCellStates() - 1;
            if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
                mainptr->ReduceCellStates(newmaxstate);
            }
            
            // use colors for new rule
            UpdateLayerColors();
            
            if (allowundo && !currlayer->stayclean) {
                currlayer->undoredo->RememberRuleChange(oldrule);
            }
        }
    }
    
    // set pastex,pastey to top left cell of paste rectangle
    pastex = left.toint();
    pastey = top.toint();
    
    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    if (savecells && inscript) SavePendingChanges();
    
    // don't paste cells outside bounded grid
    int gtop = currlayer->algo->gridtop.toint();
    int gleft = currlayer->algo->gridleft.toint();
    int gbottom = currlayer->algo->gridbottom.toint();
    int gright = currlayer->algo->gridright.toint();
    if (currlayer->algo->gridwd == 0) {
        // grid has infinite width
        gleft = INT_MIN;
        gright = INT_MAX;
    }
    if (currlayer->algo->gridht == 0) {
        // grid has infinite height
        gtop = INT_MIN;
        gbottom = INT_MAX;
    }
    
    // copy pattern from temporary universe to current universe
    int tx, ty, cx, cy;
    double maxcount = wd.todouble() * ht.todouble();
    int cntr = 0;
    bool abort = false;
    bool pattchanged = false;
    bool reduced = false;
    lifealgo* pastealgo = pastelayer->algo;
    lifealgo* curralgo = currlayer->algo;
    int maxstate = curralgo->NumCellStates() - 1;
    
    BeginProgress(_("Pasting pattern"));
    
    // we can speed up pasting sparse patterns by using nextcell in these cases:
    // - if using Or mode
    // - if current universe is empty
    // - if paste rect is outside current pattern edges
    bool usenextcell;
    if ( pmode == Or || curralgo->isEmpty() ) {
        usenextcell = true;
    } else {
        bigint ctop, cleft, cbottom, cright;
        curralgo->findedges(&ctop, &cleft, &cbottom, &cright);
        usenextcell = top > cbottom || bottom < ctop || left > cright || right < cleft;
    }
    
    if ( usenextcell && pmode == And ) {
        // current universe is empty or paste rect is outside current pattern edges
        // so don't change any cells
    } else if ( usenextcell ) {
        int newstate = 0;
        cy = pastey;
        for ( ty=itop; ty<=ibottom; ty++ ) {
            cx = pastex;
            for ( tx=ileft; tx<=iright; tx++ ) {
                int skip = pastealgo->nextcell(tx, ty, newstate);
                if (skip + tx > iright)
                    skip = -1;           // pretend we found no more live cells
                if (skip >= 0) {
                    // found next live cell so paste it into current universe
                    tx += skip;
                    cx += skip;
                    if (cx >= gleft && cx <= gright && cy >= gtop && cy <= gbottom) {
                        int currstate = curralgo->getcell(cx, cy);
                        if (currstate != newstate) {
                            if (newstate > maxstate) {
                                newstate = maxstate;
                                reduced = true;
                            }
                            curralgo->setcell(cx, cy, newstate);
                            pattchanged = true;
                            if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, newstate);
                        }
                    }
                    cx++;
                } else {
                    tx = iright + 1;     // done this row
                }
                cntr++;
                if ((cntr % 4096) == 0) {
                    double prog = ((ty - itop) * (double)(iright - ileft + 1) +
                                   (tx - ileft)) / maxcount;
                    abort = AbortProgress(prog, wxEmptyString);
                    if (abort) break;
                }
            }
            if (abort) break;
            cy++;
        }
    } else {
        // have to use slower getcell/setcell calls
        int tempstate, currstate;
        int numstates = curralgo->NumCellStates();
        cy = pastey;
        for ( ty=itop; ty<=ibottom; ty++ ) {
            cx = pastex;
            for ( tx=ileft; tx<=iright; tx++ ) {
                tempstate = pastealgo->getcell(tx, ty);
                currstate = curralgo->getcell(cx, cy);
                if (cx >= gleft && cx <= gright && cy >= gtop && cy <= gbottom) {
                    switch (pmode) {
                        case And:
                            if (tempstate != currstate && currstate > 0) {
                                curralgo->setcell(cx, cy, 0);
                                pattchanged = true;
                                if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, 0);
                            }
                            break;
                        case Copy:
                            if (tempstate != currstate) {
                                if (tempstate > maxstate) {
                                    tempstate = maxstate;
                                    reduced = true;
                                }
                                curralgo->setcell(cx, cy, tempstate);
                                pattchanged = true;
                                if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, tempstate);
                            }
                            break;
                        case Or:
                            // Or mode is done using above nextcell loop;
                            // we only include this case to avoid compiler warning
                            break;
                        case Xor:
                            if (tempstate == currstate) {
                                if (currstate != 0) {
                                    curralgo->setcell(cx, cy, 0);
                                    pattchanged = true;
                                    if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, 0);
                                }
                            } else {
                                // tempstate != currstate
                                int newstate = tempstate ^ currstate;
                                // if xor overflows then don't change current state
                                if (newstate >= numstates) newstate = currstate;
                                if (currstate != newstate) {
                                    curralgo->setcell(cx, cy, newstate);
                                    pattchanged = true;
                                    if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, newstate);
                                }
                            }
                            break;
                    }
                }
                cx++;
                cntr++;
                if ( (cntr % 4096) == 0 ) {
                    abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
                    if (abort) break;
                }
            }
            if (abort) break;
            cy++;
        }
    }
    
    if (pattchanged) curralgo->endofpattern();
    EndProgress();
    
    // tidy up and display result
    statusptr->ClearMessage();
    if (pattchanged) {
        if (savecells) currlayer->undoredo->RememberCellChanges(_("Paste"), currlayer->dirty);
        MarkLayerDirty();    // calls SetWindowTitle
        mainptr->UpdatePatternAndStatus();
    }
    
    if (reduced) statusptr->ErrorMessage(_("Some cell states were reduced."));
}

// -----------------------------------------------------------------------------

bool PatternView::GetClipboardPattern(Layer* templayer, bigint* t, bigint* l, bigint* b, bigint* r, bool nowarning)
{
    wxTextDataObject data;
    if ( !mainptr->GetTextFromClipboard(&data) ) return false;
    
    // copy clipboard data to temporary file so we can handle all formats supported by readclipboard
    wxFile tmpfile(mainptr->clipfile, wxFile::write);
    if ( !tmpfile.IsOpened() ) {
        Warning(_("Could not create temporary file for clipboard data!"));
        return false;
    }
    if ( !tmpfile.Write(data.GetText()) ) {
        Warning(_("Could not write clipboard data to temporary file!  Maybe disk is full?"));
        tmpfile.Close();
        return false;
    }
    tmpfile.Close();
    
    // remember current rule
    oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    
    const char* err = readclipboard(mainptr->clipfile.mb_str(wxConvLocal), *templayer->algo, t, l, b, r);
    if (err) {
        // cycle thru all other algos until readclipboard succeeds
        for (int i = 0; i < NumAlgos(); i++) {
            if (i != currlayer->algtype) {
                delete templayer->algo;
                templayer->algo = CreateNewUniverse(i);
                err = readclipboard(mainptr->clipfile.mb_str(wxConvLocal), *templayer->algo, t, l, b, r);
                if (!err) {
                    templayer->algtype = i;
                    break;
                }
            }
        }
    }
    
    if (!err && canchangerule > 0) {
        // set newrule for later use in PasteTemporaryToCurrent
        if (canchangerule == 1 && !currlayer->algo->isEmpty()) {
            // don't change rule if universe isn't empty
            newrule = oldrule;
        } else {
            // remember rule set by readclipboard
            newrule = wxString(templayer->algo->getrule(), wxConvLocal);
        }
    }
    
    wxRemoveFile(mainptr->clipfile);
    
    if (err) {
        // error probably due to bad rule string in clipboard data
	  if (!nowarning) {
		Warning(_("Could not load clipboard pattern\n(probably due to unknown rule)."));
	  }
        return false;
    }
    
    return true;
}

// -----------------------------------------------------------------------------

static bool doing_paste = false;    // inside PasteTemporaryToCurrent?

void PatternView::PasteClipboard(bool toselection)
{
    // prevent re-entrancy in PasteTemporaryToCurrent due to rapid pasting of large clipboard pattern
    if (doing_paste) return;

    if (waitingforclick || !mainptr->ClipboardHasText()) return;
    if (toselection && !SelectionExists()) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(toselection ? ID_PASTE_SEL : ID_PASTE);
        mainptr->Stop();
        return;
    }
    
    // if clipboard text starts with "@RULE rulename" then install rulename.rule
    // and switch to that rule
    if (mainptr->ClipboardContainsRule()) return;
    
    // if clipboard text starts with "3D version" then start up 3D.lua
    // and load the RLE3 pattern
    if (mainptr->ClipboardContainsRLE3()) return;

    // create a temporary layer for storing the clipboard pattern
    pastelayer = CreateTemporaryLayer();
    if (pastelayer) {
        // read clipboard pattern into pastelayer
        bigint top, left, bottom, right;
        if ( GetClipboardPattern(pastelayer, &top, &left, &bottom, &right, false) ) {
            // temporarily set currlayer to pastelayer so we can update the paste pattern's colors and icons
            Layer* savelayer = currlayer;
            currlayer = pastelayer;
            UpdateLayerColors();
            currlayer = savelayer;
        
            doing_paste = true;
            PasteTemporaryToCurrent(toselection, top, left, bottom, right);
            doing_paste = false;
        }
        delete pastelayer;
        pastelayer = NULL;
    }
}

// -----------------------------------------------------------------------------

void PatternView::AbortPaste()
{
    pastex = -1;
    pastey = -1;
    waitingforclick = false;    // terminate while (waitingforclick) loop
}

// -----------------------------------------------------------------------------

void PatternView::CyclePasteLocation()
{
    if (plocation == TopLeft) {
        plocation = TopRight;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Top Right."));
    } else if (plocation == TopRight) {
        plocation = BottomRight;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Bottom Right."));
    } else if (plocation == BottomRight) {
        plocation = BottomLeft;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Bottom Left."));
    } else if (plocation == BottomLeft) {
        plocation = Middle;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Middle."));
    } else {
        plocation = TopLeft;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Top Left."));
    }
    if (waitingforclick) {
        // force redraw of paste rectangle if mouse is inside viewport
        pasterect = wxRect(-1,-1,0,0);
    }
}

// -----------------------------------------------------------------------------

void PatternView::CyclePasteMode()
{
    if (pmode == And) {
        pmode = Copy;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Copy."));
    } else if (pmode == Copy) {
        pmode = Or;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Or."));
    } else if (pmode == Or) {
        pmode = Xor;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Xor."));
    } else {
        pmode = And;
        if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is And."));
    }
    if (waitingforclick) {
        // force redraw of paste rectangle if mouse is inside viewport
        pasterect = wxRect(-1,-1,0,0);
    }
}

// -----------------------------------------------------------------------------

void PatternView::DisplaySelectionSize()
{
    if (waitingforclick || inscript || currlayer->undoredo->doingscriptchanges)
        return;
    
    currlayer->currsel.DisplaySize();
}

// -----------------------------------------------------------------------------

void PatternView::SaveCurrentSelection()
{
    if (allowundo && !currlayer->stayclean) {
        currlayer->savesel = currlayer->currsel;
    }
}

// -----------------------------------------------------------------------------

void PatternView::RememberNewSelection(const wxString& action)
{
    if (TimelineExists()) {
        // we allow selections while a timeline exists but we can't
        // remember them in the undo/redo history
        return;
    }
    if (allowundo && !currlayer->stayclean) {
        if (inscript) SavePendingChanges();
        currlayer->undoredo->RememberSelection(action);
    }
}

// -----------------------------------------------------------------------------

void PatternView::SelectAll()
{
    SaveCurrentSelection();
    if (SelectionExists()) {
        currlayer->currsel.Deselect();
        mainptr->UpdatePatternAndStatus();
    }
    
    if (currlayer->algo->isEmpty()) {
        statusptr->ErrorMessage(empty_pattern);
        RememberNewSelection(_("Deselection"));
        return;
    }
    
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    currlayer->currsel.SetEdges(top, left, bottom, right);
    
    RememberNewSelection(_("Select All"));
    DisplaySelectionSize();
    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void PatternView::RemoveSelection()
{
    if (SelectionExists()) {
        SaveCurrentSelection();
        currlayer->currsel.Deselect();
        RememberNewSelection(_("Deselection"));
        mainptr->UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

void PatternView::ShrinkSelection(bool fit)
{
    currlayer->currsel.Shrink(fit);
}

// -----------------------------------------------------------------------------

void PatternView::RandomFill()
{
    currlayer->currsel.RandomFill();
}

// -----------------------------------------------------------------------------

bool PatternView::FlipPastePattern(bool topbottom)
{
    bool result;
    Selection pastesel(pastebox.GetTop(), pastebox.GetLeft(),
                       pastebox.GetBottom(), pastebox.GetRight());
    
    // flip the pattern in pastelayer
    lifealgo* savealgo = currlayer->algo;
    int savetype = currlayer->algtype;
    currlayer->algo = pastelayer->algo;
    currlayer->algtype = pastelayer->algtype;
    // pass in true for inundoredo parameter so flip won't be remembered
    // and layer won't be marked as dirty; also set inscript temporarily
    // so that viewport won't be updated
    inscript = true;
    result = pastesel.Flip(topbottom, true);
    // currlayer->algo might point to a *different* universe
    pastelayer->algo = currlayer->algo;
    currlayer->algo = savealgo;
    currlayer->algtype = savetype;
    inscript = false;
    
    if (result) {
        InitPaste(pastelayer, pastebox);
        RefreshView();
    }
    
    return result;
}

// -----------------------------------------------------------------------------

bool PatternView::RotatePastePattern(bool clockwise)
{
    Selection pastesel(pastebox.GetTop(), pastebox.GetLeft(),
                       pastebox.GetBottom(), pastebox.GetRight());
    
    // check if pastelayer->algo uses a finite grid
    if (!pastelayer->algo->unbounded) {
        // readclipboard has loaded the pattern into top left corner of grid,
        // so if pastebox isn't square we need to expand the grid to avoid the
        // rotated pattern being clipped (WARNING: this assumes the algo won't
        // change the pattern's cell coordinates when setrule expands the grid)
        int x, y, wd, ht;
        pastesel.GetRect(&x, &y, &wd, &ht);
        if (wd != ht) {
        
            // better solution would be to check if pastebox is small enough for
            // pattern to be safely rotated after shifting to center of grid and only
            // expand grid if it can't??? (must also update pastesel edges)
            
            int newwd, newht;
            if (wd > ht) {
                // expand grid vertically
                newht = pastelayer->algo->gridht + wd;
                newwd = pastelayer->algo->gridwd;
            } else {
                // wd < ht so expand grid horizontally
                newwd = pastelayer->algo->gridwd + ht;
                newht = pastelayer->algo->gridht;
            }
            char rule[MAXRULESIZE];
            sprintf(rule, "%s", pastelayer->algo->getrule());
            char topology = 'T';
            char *suffix = strchr(rule, ':');
            if (suffix) {
                topology = suffix[1];
                suffix[0] = 0;
            }
            sprintf(rule+strlen(rule), ":%c%d,%d", topology, newwd, newht);
            if (pastelayer->algo->setrule(rule)) {
                // unlikely, but could happen if the new grid size is too big
                Warning(_("Sorry, but the clipboard pattern could not be rotated."));
                return false;
            }
        }
    }
    
    // rotate the pattern in pastelayer
    lifealgo* savealgo = currlayer->algo;
    int savetype = currlayer->algtype;
    currlayer->algo = pastelayer->algo;
    currlayer->algtype = pastelayer->algtype;
    
    // pass in true for inundoredo parameter so rotate won't be remembered
    // and layer won't be marked as dirty; also set inscript temporarily
    // so that viewport won't be updated
    inscript = true;
    bool result = pastesel.Rotate(clockwise, true);
    
    // currlayer->algo might point to a *different* universe
    pastelayer->algo = currlayer->algo;
    currlayer->algo = savealgo;
    currlayer->algtype = savetype;
    inscript = false;
    
    if (result) {
        // get rotated selection and update pastebox
        int x, y, wd, ht;
        pastesel.GetRect(&x, &y, &wd, &ht);
        pastebox = wxRect(x, y, wd, ht);
        InitPaste(pastelayer, pastebox);
        if (wd == ht) RefreshView();
        // if wd != ht then PasteTemporaryToCurrent will call Refresh
    }
    
    return result;
}

// -----------------------------------------------------------------------------

bool PatternView::FlipSelection(bool topbottom, bool inundoredo)
{
    if (waitingforclick) {
        // more useful to flip the pattern about to be pasted
        return FlipPastePattern(topbottom);
    } else {
        return currlayer->currsel.Flip(topbottom, inundoredo);
    }
}

// -----------------------------------------------------------------------------

bool PatternView::RotateSelection(bool clockwise, bool inundoredo)
{
    if (waitingforclick) {
        // more useful to rotate the pattern about to be pasted
        return RotatePastePattern(clockwise);
    } else {
        return currlayer->currsel.Rotate(clockwise, inundoredo);
    }
}

// -----------------------------------------------------------------------------

void PatternView::SetCursorMode(wxCursor* cursor)
{
    currlayer->curs = cursor;
}

// -----------------------------------------------------------------------------

void PatternView::CycleCursorMode()
{
    if (drawingcells || selectingcells || movingview || waitingforclick)
        return;
    
    if (currlayer->curs == curs_pencil)
        currlayer->curs = curs_pick;
    else if (currlayer->curs == curs_pick)
        currlayer->curs = curs_cross;
    else if (currlayer->curs == curs_cross)
        currlayer->curs = curs_hand;
    else if (currlayer->curs == curs_hand)
        currlayer->curs = curs_zoomin;
    else if (currlayer->curs == curs_zoomin)
        currlayer->curs = curs_zoomout;
    else
        currlayer->curs = curs_pencil;
}

// -----------------------------------------------------------------------------

void PatternView::ZoomOut()
{
    TestAutoFit();
    currlayer->view->unzoom();
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ZoomIn()
{
    TestAutoFit();
    if (currlayer->view->getmag() < MAX_MAG) {
        currlayer->view->zoom();
        mainptr->UpdateEverything();
    } else {
        Beep();
    }
}

// -----------------------------------------------------------------------------

void PatternView::SetPixelsPerCell(int pxlspercell)
{
    int mag = 0;
    while (pxlspercell > 1) {
        mag++;
        pxlspercell >>= 1;
    }
    if (mag == currlayer->view->getmag()) return;
    TestAutoFit();
    currlayer->view->setmag(mag);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::FitPattern()
{
    currlayer->algo->fit(*currlayer->view, 1);
    // best not to call TestAutoFit
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::FitSelection()
{
    if (!SelectionExists()) return;
    
    currlayer->currsel.Fit();
    
    TestAutoFit();
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ViewOrigin()
{
    // put 0,0 cell in middle of view
    if ( currlayer->originx == bigint::zero && currlayer->originy == bigint::zero ) {
        currlayer->view->center();
    } else {
        // put cell saved by ChangeOrigin in middle
        currlayer->view->setpositionmag(currlayer->originx, currlayer->originy,
                                        currlayer->view->getmag());
    }
    TestAutoFit();
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ChangeOrigin()
{
    if (waitingforclick) return;
    // change cell under cursor to 0,0
    wxPoint pt = ScreenToClient( wxGetMousePosition() );
    pt.x *= int(scalefactor);
    pt.y *= int(scalefactor);
    if (pt.x < 0 || pt.x > currlayer->view->getxmax() ||
        pt.y < 0 || pt.y > currlayer->view->getymax() ) {
        statusptr->ErrorMessage(_("Origin not changed."));
    } else {
        pair<bigint, bigint> cellpos = currlayer->view->at(pt.x, pt.y);
        currlayer->originx = cellpos.first;
        currlayer->originy = cellpos.second;
        statusptr->DisplayMessage(_("Origin changed."));
        if ( GridVisible() )
            mainptr->UpdatePatternAndStatus();
        else
            statusptr->UpdateXYLocation();
    }
}

// -----------------------------------------------------------------------------

void PatternView::RestoreOrigin()
{
    if (waitingforclick) return;
    if (currlayer->originx != bigint::zero || currlayer->originy != bigint::zero) {
        currlayer->originx = 0;
        currlayer->originy = 0;
        statusptr->DisplayMessage(origin_restored);
        if ( GridVisible() )
            mainptr->UpdatePatternAndStatus();
        else
            statusptr->UpdateXYLocation();
    }
}

// -----------------------------------------------------------------------------

bool PatternView::GridVisible()
{
    return ( showgridlines && currlayer->view->getmag() >= mingridmag );
}

// -----------------------------------------------------------------------------

void PatternView::ToggleGridLines()
{
    showgridlines = !showgridlines;
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ToggleCellIcons()
{
    showicons = !showicons;
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ToggleCellColors()
{
    swapcolors = !swapcolors;
    InvertCellColors();
    
    if (pastelayer) {
        // invert colors used to draw paste pattern
        for (int n = 0; n <= pastelayer->numicons; n++) {
            pastelayer->cellr[n] = 255 - pastelayer->cellr[n];
            pastelayer->cellg[n] = 255 - pastelayer->cellg[n];
            pastelayer->cellb[n] = 255 - pastelayer->cellb[n];
        }
        InvertIconColors(pastelayer->atlas7x7, 8, pastelayer->numicons);
        InvertIconColors(pastelayer->atlas15x15, 16, pastelayer->numicons);
        InvertIconColors(pastelayer->atlas31x31, 32, pastelayer->numicons);
    }
    
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ToggleSmarterScaling()
{
    smartscale = !smartscale;
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

bool PatternView::GetCellPos(bigint& xpos, bigint& ypos)
{
    if (mainptr == NULL || !mainptr->IsShown()) return false;

    wxPoint pt = ScreenToClient( wxGetMousePosition() );
    pt.x *= int(scalefactor);
    pt.y *= int(scalefactor);
    if (PointInView(pt.x, pt.y)) {
        // get mouse location in cell coords
        pair<bigint, bigint> cellpos = currlayer->view->at(pt.x, pt.y);
        xpos = cellpos.first;
        ypos = cellpos.second;
        
        // check if xpos,ypos is outside bounded grid
        if (!CellInGrid(xpos, ypos)) return false;
        
        return true;
    } else {
        // mouse not in viewport
        return false;
    }
}

// -----------------------------------------------------------------------------

bool PatternView::PointInView(int x, int y)
{
    return ( x >= 0 && x <= currlayer->view->getxmax() &&
             y >= 0 && y <= currlayer->view->getymax() );
}

// -----------------------------------------------------------------------------

#ifdef __WXMAC__
    #define RefreshControls() RefreshRect(controlsrect,false)
#else
    // safer to redraw entire viewport on Windows and Linux
    // otherwise we see partial drawing in some cases
    #define RefreshControls() Refresh(false)
#endif

void PatternView::CheckCursor(bool active)
{
    if (!active) return;    // main window is not active so don't change cursor
    
    if (mainptr->IsIconized() || !mainptr->IsShown()) return;
    
    // make sure cursor is up to date
    wxPoint pt = ScreenToClient( wxGetMousePosition() );
    pt.x *= int(scalefactor);
    pt.y *= int(scalefactor);
    
    if (PointInView(pt.x, pt.y)) {
        int ox, oy;
        if (numlayers > 1 && tilelayers && tileindex != currindex) {
            // show arrow cursor if over tile border (ie. bigview) or non-current tile
            SetCursor(*wxSTANDARD_CURSOR);
            if (showcontrols) {
                showcontrols = false;
                RefreshControls();
            }
        
        } else if (showoverlay && curroverlay->PointInOverlay(pt.x, pt.y, &ox, &oy)
                               && !curroverlay->TransparentPixel(ox, oy)) {
            // cursor is over non-transparent pixel in overlay
            curroverlay->SetOverlayCursor();
            if (showcontrols) {
                showcontrols = false;
                RefreshControls();
            }
        
        } else if ((controlsrect.Contains(pt) || clickedcontrol > NO_CONTROL) &&
                   !(drawingcells || selectingcells || movingview || waitingforclick) ) {
            // cursor is over translucent controls, or user clicked in a control
            // and hasn't released mouse button yet
            SetCursor(*wxSTANDARD_CURSOR);
            if (!showcontrols) {
                showcontrols = true;
                RefreshControls();
            }
            
        } else {
            // show current cursor mode
            SetCursor(*currlayer->curs);
            if (showcontrols) {
                showcontrols = false;
                RefreshControls();
            }
        }
        
    } else {
        // cursor is not in viewport
        SetCursor(*wxSTANDARD_CURSOR);
        if (showcontrols) {
            showcontrols = false;
            RefreshControls();
        }
    }
}

// -----------------------------------------------------------------------------

int PatternView::GetMag()
{
    return currlayer->view->getmag();
}

// -----------------------------------------------------------------------------

void PatternView::SetMag(int mag)
{
    TestAutoFit();
    if (mag > MAX_MAG) mag = MAX_MAG;
    currlayer->view->setmag(mag);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::SetPosMag(const bigint& x, const bigint& y, int mag)
{
    currlayer->view->setpositionmag(x, y, mag);
}

// -----------------------------------------------------------------------------

void PatternView::GetPos(bigint& x, bigint& y)
{
    x = currlayer->view->x;
    y = currlayer->view->y;
}

// -----------------------------------------------------------------------------

void PatternView::FitInView(int force)
{
    currlayer->algo->fit(*currlayer->view, force);
}

// -----------------------------------------------------------------------------

bool PatternView::CellVisible(const bigint& x, const bigint& y)
{
    return currlayer->view->contains(x, y) != 0;
}

// -----------------------------------------------------------------------------

// scrolling functions:

void PatternView::PanUp(int amount)
{
    TestAutoFit();
    currlayer->view->move(0, -amount);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanDown(int amount)
{
    TestAutoFit();
    currlayer->view->move(0, amount);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanLeft(int amount)
{
    TestAutoFit();
    currlayer->view->move(-amount, 0);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanRight(int amount)
{
    TestAutoFit();
    currlayer->view->move(amount, 0);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanNE()
{
    TestAutoFit();
    int xamount = SmallScroll(currlayer->view->getwidth());
    int yamount = SmallScroll(currlayer->view->getheight());
    int amount = (xamount < yamount) ? xamount : yamount;
    currlayer->view->move(amount, -amount);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanNW()
{
    TestAutoFit();
    int xamount = SmallScroll(currlayer->view->getwidth());
    int yamount = SmallScroll(currlayer->view->getheight());
    int amount = (xamount < yamount) ? xamount : yamount;
    currlayer->view->move(-amount, -amount);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanSE()
{
    TestAutoFit();
    int xamount = SmallScroll(currlayer->view->getwidth());
    int yamount = SmallScroll(currlayer->view->getheight());
    int amount = (xamount < yamount) ? xamount : yamount;
    currlayer->view->move(amount, amount);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanSW()
{
    TestAutoFit();
    int xamount = SmallScroll(currlayer->view->getwidth());
    int yamount = SmallScroll(currlayer->view->getheight());
    int amount = (xamount < yamount) ? xamount : yamount;
    currlayer->view->move(-amount, amount);
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

int PatternView::SmallScroll(int xysize)
{
    int amount;
    int mag = currlayer->view->getmag();
    if (mag > 0) {
        // scroll an integral number of cells (1 cell = 2^mag pixels)
        if (mag < 5) {
            amount = ((xysize >> mag) / 20) << mag;
            if (amount == 0) amount = 1 << mag;
            return amount;
        } else {
            // zoomed right in so scroll by only 1 cell
            return 1 << mag;
        }
    } else {
        // scroll by approx 5% of current wd/ht
        amount = xysize / 20;
        if (amount == 0) amount = 1;
        return amount;
    }
}

// -----------------------------------------------------------------------------

int PatternView::BigScroll(int xysize)
{
    int amount;
    int mag = currlayer->view->getmag();
    if (mag > 0) {
        // scroll an integral number of cells (1 cell = 2^mag pixels)
        amount = ((xysize >> mag) * 9 / 10) << mag;
        if (amount == 0) amount = 1 << mag;
        return amount;
    } else {
        // scroll by approx 90% of current wd/ht
        amount = xysize * 9 / 10;
        if (amount == 0) amount = 1;
        return amount;
    }
}

// -----------------------------------------------------------------------------

void PatternView::UpdateScrollBars()
{
    if (mainptr->fullscreen) return;
    
    int viewwd, viewht;
    int mag = currlayer->view->getmag();
    if (mag > 0) {
        // scroll by cells, so determine number of cells visible in viewport
        viewwd = currlayer->view->getwidth() >> mag;
        viewht = currlayer->view->getheight() >> mag;
    } else {
        // scroll by pixels, so get pixel dimensions of viewport
        viewwd = currlayer->view->getwidth();
        viewht = currlayer->view->getheight();
    }
    if (viewwd < 1) viewwd = 1;
    if (viewht < 1) viewht = 1;
    
    if (currlayer->algo->gridwd > 0) {
        // restrict scrolling to left/right edges of grid if its width is finite
        int range = currlayer->algo->gridwd;
        // avoid scroll bar disappearing
        if (range < 3) range = 3;
        hthumb = currlayer->view->x.toint() + range / 2;
        mainptr->hbar->SetScrollbar(hthumb, 1, range, 1, true);
    } else {
        // keep thumb box in middle of scroll bar if grid width is infinite
        hthumb = (thumbrange - 1) * viewwd / 2;
        mainptr->hbar->SetScrollbar(hthumb, viewwd, thumbrange * viewwd, viewwd, true);
    }
    
    if (currlayer->algo->gridht > 0) {
        // restrict scrolling to top/bottom edges of grid if its height is finite
        int range = currlayer->algo->gridht;
        // avoid scroll bar disappearing
        if (range < 3) range = 3;
        vthumb = currlayer->view->y.toint() + range / 2;
        mainptr->vbar->SetScrollbar(vthumb, 1, range, 1, true);
    } else {
        // keep thumb box in middle of scroll bar if grid height is infinite
        vthumb = (thumbrange - 1) * viewht / 2;
        mainptr->vbar->SetScrollbar(vthumb, viewht, thumbrange * viewht, viewht, true);
    }
}

// -----------------------------------------------------------------------------

void PatternView::ProcessKey(int key, int modifiers)
{
    mainptr->showbanner = false;
    
    // WARNING: ProcessKey can be called while running a script, or reading
    // a large pattern file, or waiting for a paste click etc, so we must avoid
    // doing any actions that could cause havoc at such times.
    bool busy = nopattupdate || waitingforclick || dragtimer->IsRunning();
    bool timeline = TimelineExists();
    
    action_info action = FindAction(key, modifiers);
    switch (action.id) {
        case DO_NOTHING:
            // any unassigned key turns off full screen mode
            if (mainptr->fullscreen) mainptr->ToggleFullScreen();
            break;
            
        case DO_OPENFILE:
            if (IsHTMLFile(action.file)) {
                // show HTML file in help window
                if (!busy) ShowHelp(action.file);
            } else {
                // load pattern or run script
                if (!inscript && !busy) mainptr->OpenFile(action.file, true);
            }
            break;
            
        // File menu actions
        case DO_NEWPATT:     if (!inscript && !busy) mainptr->NewPattern(); break;
        case DO_OPENPATT:    if (!inscript && !busy) mainptr->OpenPattern(); break;
        case DO_OPENCLIP:    if (!inscript && !busy) mainptr->OpenClipboard(); break;
        case DO_OPENRECENT:  if (!inscript && !busy) mainptr->OpenRecentPattern(ID_OPEN_RECENT+1); break;
        case DO_SAVE:        if (!inscript && !busy) mainptr->SavePattern(); break;
        case DO_SAVEXRLE:    if (!inscript) savexrle = !savexrle; break;
        case DO_RUNSCRIPT:   if (!inscript && !timeline && !busy) mainptr->OpenScript(); break;
        case DO_RUNCLIP:     if (!inscript && !timeline && !busy) mainptr->RunClipboard(); break;
        case DO_RUNRECENT:   if (!inscript && !timeline && !busy) mainptr->OpenRecentScript(ID_RUN_RECENT+1); break;
        case DO_PREFS:       if (!busy) mainptr->ShowPrefsDialog(); break;
        case DO_SETFOLDER:   if (!busy) mainptr->SetFolder(); break;
        case DO_SHOWFILES:   mainptr->ToggleShowFiles(); break;
        case DO_QUIT:        mainptr->QuitApp(); break;
            
        // Edit menu actions
        case DO_UNDO:        if (!inscript && !timeline && !busy) currlayer->undoredo->UndoChange(); break;
        case DO_REDO:        if (!inscript && !timeline && !busy) currlayer->undoredo->RedoChange(); break;
        case DO_DISABLE:     if (!inscript) mainptr->ToggleAllowUndo(); break;
        case DO_CUT:         if (!inscript && !timeline) CutSelection(); break;
        case DO_COPY:        if (!inscript) CopySelection(); break;
        case DO_CLEAR:       if (!inscript && !timeline) ClearSelection(); break;
        case DO_CLEAROUT:    if (!inscript && !timeline) ClearOutsideSelection(); break;
        case DO_PASTE:
            if (!inscript && !timeline && !busy) {
                // PasteClipboard(false) has a Yield loop so we do the following to avoid
                // calling ProcessKey re-entrantly as it causes problems on Mac OS X
                // and possibly the other platforms
                wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, ID_PASTE);
                wxPostEvent(mainptr->GetEventHandler(), evt);
                return;
            }
            break;
        // case DO_PASTE:    if (!inscript && !timeline && !busy) PasteClipboard(false); break;
        case DO_PASTESEL:    if (!inscript && !timeline && !busy) PasteClipboard(true); break;
        case DO_SELALL:      if (!inscript) SelectAll(); break;
        case DO_REMOVESEL:   if (!inscript) RemoveSelection(); break;
        case DO_SHRINK:      if (!inscript) ShrinkSelection(false); break;
        case DO_SHRINKFIT:   if (!inscript) ShrinkSelection(true); break;
        case DO_RANDFILL:    if (!inscript && !timeline) RandomFill(); break;
        case DO_FLIPTB:      if (!inscript && !timeline) FlipSelection(true); break;
        case DO_FLIPLR:      if (!inscript && !timeline) FlipSelection(false); break;
        case DO_ROTATECW:    if (!inscript && !timeline) RotateSelection(true); break;
        case DO_ROTATEACW:   if (!inscript && !timeline) RotateSelection(false); break;
        case DO_ADVANCE:     if (!inscript && !timeline) currlayer->currsel.Advance(); break;
        case DO_ADVANCEOUT:  if (!inscript && !timeline) currlayer->currsel.AdvanceOutside(); break;
        case DO_CURSDRAW:    SetCursorMode(curs_pencil); break;
        case DO_CURSPICK:    SetCursorMode(curs_pick); break;
        case DO_CURSSEL:     SetCursorMode(curs_cross); break;
        case DO_CURSMOVE:    SetCursorMode(curs_hand); break;
        case DO_CURSIN:      SetCursorMode(curs_zoomin); break;
        case DO_CURSOUT:     SetCursorMode(curs_zoomout); break;
        case DO_CURSCYCLE:   CycleCursorMode(); break;
        case DO_PASTEMODE:   CyclePasteMode(); break;
        case DO_PASTELOC:    CyclePasteLocation(); break;
        case DO_NEXTHIGHER:  CycleDrawingState(true); break;
        case DO_NEXTLOWER:   CycleDrawingState(false); break;
            
        // Control menu actions
        case DO_STARTSTOP:   if (!inscript) mainptr->StartOrStop(); break;
        case DO_NEXTGEN:     if (!inscript && !timeline) mainptr->NextGeneration(false); break;
        case DO_NEXTSTEP:    if (!inscript && !timeline) mainptr->NextGeneration(true); break;
        case DO_RESET:       if (!inscript && !timeline && !busy) mainptr->ResetPattern(); break;
        case DO_SETGEN:      if (!inscript && !timeline && !busy) mainptr->SetGeneration(); break;
        case DO_SETBASE:     if (!inscript && !timeline && !busy) mainptr->SetBaseStep(); break;
        case DO_FASTER:      mainptr->GoFaster(); break;
        case DO_SLOWER:      mainptr->GoSlower(); break;
        case DO_AUTOFIT:     mainptr->ToggleAutoFit(); break;
        case DO_HYPER:       if (!timeline) mainptr->ToggleHyperspeed(); break;
        case DO_HASHINFO:    mainptr->ToggleHashInfo(); break;
        case DO_SHOWPOP:     mainptr->ToggleShowPopulation(); break;
        case DO_AUTOSTOP:    autostop = !autostop; break;
        case DO_RECORD:      StartStopRecording(); break;
        case DO_DELTIME:     DeleteTimeline(); break;
        case DO_PLAYBACK:    if (!inscript && timeline) PlayTimeline(-1); break;
        case DO_SETRULE:     if (!inscript && !timeline && !busy) mainptr->ShowRuleDialog(); break;
        case DO_TIMING:      if (!inscript && !timeline) mainptr->DisplayTimingInfo(); break;
        case DO_HASHING:
            if (!inscript && !timeline && !busy) {
                if (currlayer->algtype != HLIFE_ALGO)
                    mainptr->ChangeAlgorithm(HLIFE_ALGO);
                else
                    mainptr->ChangeAlgorithm(QLIFE_ALGO);
            }
            break;
            
        // View menu actions
        case DO_LEFT:        PanLeft( SmallScroll(currlayer->view->getwidth()) ); break;
        case DO_RIGHT:       PanRight( SmallScroll(currlayer->view->getwidth()) ); break;
        case DO_UP:          PanUp( SmallScroll(currlayer->view->getheight()) ); break;
        case DO_DOWN:        PanDown( SmallScroll(currlayer->view->getheight()) ); break;
        case DO_NE:          PanNE(); break;
        case DO_NW:          PanNW(); break;
        case DO_SE:          PanSE(); break;
        case DO_SW:          PanSW(); break;
        case DO_FULLSCREEN:  mainptr->ToggleFullScreen(); break;
        case DO_FIT:         FitPattern(); break;
        case DO_FITSEL:      FitSelection(); break;
        case DO_MIDDLE:      ViewOrigin(); break;
        case DO_CHANGE00:    ChangeOrigin(); break;
        case DO_RESTORE00:   RestoreOrigin(); break;
        case DO_ZOOMIN:      ZoomIn(); break;
        case DO_ZOOMOUT:     ZoomOut(); break;
        case DO_SCALE1:      SetPixelsPerCell(1); break;
        case DO_SCALE2:      SetPixelsPerCell(2); break;
        case DO_SCALE4:      SetPixelsPerCell(4); break;
        case DO_SCALE8:      SetPixelsPerCell(8); break;
        case DO_SCALE16:     SetPixelsPerCell(16); break;
        case DO_SCALE32:     SetPixelsPerCell(32); break;
        case DO_SHOWTOOL:    mainptr->ToggleToolBar(); break;
        case DO_SHOWLAYER:   ToggleLayerBar(); break;
        case DO_SHOWEDIT:    ToggleEditBar(); break;
        case DO_SHOWSTATES:  ToggleAllStates(); break;
        case DO_SHOWSCROLL:  mainptr->ToggleScrollBars(); break;
        case DO_SHOWSTATUS:  mainptr->ToggleStatusBar(); break;
        case DO_SHOWEXACT:   mainptr->ToggleExactNumbers(); break;
        case DO_SHOWXYSTATE: showxystate = !showxystate; statusptr->UpdateXYLocation(); break;
        case DO_SHOWICONS:   ToggleCellIcons(); break;
        case DO_INVERT:      ToggleCellColors(); break;
        case DO_SMARTSCALE:  ToggleSmarterScaling(); break;
        case DO_SHOWGRID:    ToggleGridLines(); break;
        case DO_SHOWTIME:    ToggleTimelineBar(); break;
        case DO_INFO:        if (!busy) mainptr->ShowPatternInfo(); break;
            
        // Layer menu actions
        case DO_SAVEOVERLAY: mainptr->SaveOverlay(); break;
        case DO_SHOWOVERLAY: mainptr->ToggleOverlay(); break;
        case DO_DELOVERLAY:  if (!inscript) mainptr->DeleteOverlay(); break;
        case DO_ADD:         if (!inscript) AddLayer(); break;
        case DO_CLONE:       if (!inscript) CloneLayer(); break;
        case DO_DUPLICATE:   if (!inscript) DuplicateLayer(); break;
        case DO_DELETE:      if (!inscript) DeleteLayer(); break;
        case DO_DELOTHERS:   if (!inscript) DeleteOtherLayers(); break;
        case DO_MOVELAYER:   if (!inscript && !busy) MoveLayerDialog(); break;
        case DO_NAMELAYER:   if (!inscript && !busy) NameLayerDialog(); break;
        case DO_SETCOLORS:   if (!inscript && !busy) SetLayerColors(); break;
        case DO_SYNCVIEWS:   if (!inscript) ToggleSyncViews(); break;
        case DO_SYNCCURS:    if (!inscript) ToggleSyncCursors(); break;
        case DO_STACK:       if (!inscript) ToggleStackLayers(); break;
        case DO_TILE:        if (!inscript) ToggleTileLayers(); break;
            
        // Help menu actions
        case DO_HELP:
            if (!busy) {
                // if help window is open then bring it to the front,
                // otherwise open it and display most recent help file
                ShowHelp(wxEmptyString);
            }
            break;
        case DO_ABOUT:       if (!inscript && !busy) ShowAboutBox(); break;
            
        default:             Warning(_("Bug detected in ProcessKey!"));
    }
    
    // note that we avoid updating viewport if inscript and action.id is DO_OPENFILE
    // otherwise problem can occur when rapidly repeating a keyboard shortcut that
    // runs a script
    if (inscript && action.id != DO_NOTHING && action.id != DO_OPENFILE) {
        // update viewport, status bar, scroll bars
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        bigview->UpdateScrollBars();
        inscript = true;
    }
    
    mainptr->UpdateUserInterface();
}

// -----------------------------------------------------------------------------

void PatternView::RememberOneCellChange(int cx, int cy, int oldstate, int newstate)
{
    if (allowundo) {
        // remember this cell change for later undo/redo
        currlayer->undoredo->SaveCellChange(cx, cy, oldstate, newstate);
    }
}

// -----------------------------------------------------------------------------

void PatternView::StartDrawingCells(int x, int y)
{
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    // check that cellpos is within getcell/setcell limits
    if ( OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
        statusptr->ErrorMessage(_("Drawing is not allowed outside +/- 10^9 boundary."));
        return;
    }

    drawingcells = true;
    
    // save dirty state now for later use by RememberCellChanges
    if (allowundo) currlayer->savedirty = currlayer->dirty;
    
    cellx = cellpos.first.toint();
    celly = cellpos.second.toint();
    int currstate = currlayer->algo->getcell(cellx, celly);
    
    // reset drawing state in case it's no longer valid (due to algo/rule change)
    if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
        currlayer->drawingstate = 1;
    }
    
    if (currstate == currlayer->drawingstate) {
        drawstate = 0;
    } else {
        drawstate = currlayer->drawingstate;
    }
    if (currstate != drawstate) {
        currlayer->algo->setcell(cellx, celly, drawstate);
        currlayer->algo->endofpattern();

        // remember this cell change for later undo/redo
        RememberOneCellChange(cellx, celly, currstate, drawstate);

        MarkLayerDirty();
        if (showstatus) statusptr->Refresh(false);
        RefreshView();
    }
    
    CaptureMouse();                 // get mouse up event even if outside view
    dragtimer->Start(TEN_HERTZ);
    
    if (stopdrawing) {
        // mouse up event has already been seen so terminate drawing immediately
        stopdrawing = false;
        StopDraggingMouse();
    }
}

// -----------------------------------------------------------------------------

void PatternView::DrawCells(int x, int y)
{
    // make sure x,y is within viewport
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
    if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
    
    // make sure x,y is within bounded grid
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if (currlayer->algo->gridwd > 0) {
        if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
        if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
    }
    if (currlayer->algo->gridht > 0) {
        if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
        if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
    }
    
    if ( currlayer->view->getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
        return;
    }
    
    int currstate;
    int numchanged = 0;
    int newx = cellpos.first.toint();
    int newy = cellpos.second.toint();
    if ( newx != cellx || newy != celly ) {
        
        // draw a line of cells using Bresenham's algorithm
        int d, ii, jj, di, ai, si, dj, aj, sj;
        di = newx - cellx;
        ai = abs(di) << 1;
        si = (di < 0)? -1 : 1;
        dj = newy - celly;
        aj = abs(dj) << 1;
        sj = (dj < 0)? -1 : 1;
        
        ii = cellx;
        jj = celly;
        
        lifealgo* curralgo = currlayer->algo;
        if (ai > aj) {
            d = aj - (ai >> 1);
            while (ii != newx) {
                currstate = curralgo->getcell(ii, jj);
                if (currstate != drawstate) {
                    curralgo->setcell(ii, jj, drawstate);
                    RememberOneCellChange(ii, jj, currstate, drawstate);
                    numchanged++;
                }
                if (d >= 0) {
                    jj += sj;
                    d  -= ai;
                }
                ii += si;
                d  += aj;
            }
        } else {
            d = ai - (aj >> 1);
            while (jj != newy) {
                currstate = curralgo->getcell(ii, jj);
                if (currstate != drawstate) {
                    curralgo->setcell(ii, jj, drawstate);
                    RememberOneCellChange(ii, jj, currstate, drawstate);
                    numchanged++;
                }
                if (d >= 0) {
                    ii += si;
                    d  -= aj;
                }
                jj += sj;
                d  += ai;
            }
        }
        
        cellx = newx;
        celly = newy;
        
        currstate = curralgo->getcell(cellx, celly);
        if (currstate != drawstate) {
            curralgo->setcell(cellx, celly, drawstate);
            RememberOneCellChange(cellx, celly, currstate, drawstate);
            numchanged++;
        }
    }
        
    if (numchanged > 0) {
        currlayer->algo->endofpattern();
        MarkLayerDirty();
        if (showstatus) statusptr->Refresh(false);
        RefreshView();
    }
}

// -----------------------------------------------------------------------------

void PatternView::PickCell(int x, int y)
{
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if ( currlayer->view->getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
        return;
    }
    
    int cellx = cellpos.first.toint();
    int celly = cellpos.second.toint();
    currlayer->drawingstate = currlayer->algo->getcell(cellx, celly);
    UpdateEditBar();
}

// -----------------------------------------------------------------------------

void PatternView::StartSelectingCells(int x, int y, bool shiftdown)
{
    // make sure anchor cell is within bounded grid (x,y can be outside grid)
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if (currlayer->algo->gridwd > 0) {
        if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
        if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
    }
    if (currlayer->algo->gridht > 0) {
        if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
        if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
    }
    anchorx = cellpos.first;
    anchory = cellpos.second;
    
    // save original selection so it can be restored if user hits escape;
    // also used by RememberNewSelection
    currlayer->savesel = currlayer->currsel;
    
    // reset previous selection
    prevsel.Deselect();
    
    // for avoiding 1x1 selection if mouse doesn't move much
    initselx = x;
    initsely = y;
    
    // allow changing size in any direction
    forceh = false;
    forcev = false;
    
    if (SelectionExists()) {
        if (shiftdown) {
            // modify current selection
            currlayer->currsel.Modify(cellpos.first, cellpos.second,
                                      anchorx, anchory, &forceh, &forcev);
            DisplaySelectionSize();
        } else {
            // remove current selection
            currlayer->currsel.Deselect();
        }
        // allow mouse interaction if script is running
        bool saveinscript = inscript;
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = saveinscript;
    }
    
    selectingcells = true;
    CaptureMouse();                 // get mouse up event even if outside view
    dragtimer->Start(TEN_HERTZ);
}

// -----------------------------------------------------------------------------

void PatternView::SelectCells(int x, int y)
{
    // last time selection was changed
    static long lastupdate = -1;

    // only select cells within view
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
    if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
    
    if ( abs(initselx - x) < 2 && abs(initsely - y) < 2 && !SelectionExists() ) {
        // avoid 1x1 selection if mouse hasn't moved much
        return;
    }
    
    // make sure x,y is within bounded grid
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if (currlayer->algo->gridwd > 0) {
        if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
        if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
    }
    if (currlayer->algo->gridht > 0) {
        if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
        if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
    }
    
    if (!forcev) currlayer->currsel.SetLeftRight(cellpos.first, anchorx);
    if (!forceh) currlayer->currsel.SetTopBottom(cellpos.second, anchory);
    
    if (currlayer->currsel != prevsel) {
        // selection has changed
        DisplaySelectionSize();
        prevsel = currlayer->currsel;
        
        // allow mouse interaction if script is running
        bool saveinscript = inscript;
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = saveinscript;

        // save last update time
        lastupdate = stopwatch->Time();
    } else {
        // selection has not changed so check how long it has been static
        if  (lastupdate >= 0 && (stopwatch->Time() - lastupdate > 100)) {
            // selection static for 100ms so compute selection population
            // temporarily disable selectingcells since this will cause
            // the long timeout to be used in the population calculation
            selectingcells = false;
            DisplaySelectionSize();

            // enable selecting cells again
            selectingcells = true;
            mainptr->UpdatePatternAndStatus(false);

            // mark no update pending
            lastupdate = -1;
        }
    }
}

// -----------------------------------------------------------------------------

void PatternView::StartMovingView(int x, int y)
{
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    bigcellx = cellpos.first;
    bigcelly = cellpos.second;
    movingview = true;
    if (waitingforclick) {
        // avoid calling CaptureMouse again (middle button was pressed)
    } else {
        CaptureMouse();             // get mouse up event even if outside view
    }
    dragtimer->Start(TEN_HERTZ);
}

// -----------------------------------------------------------------------------

void PatternView::MoveView(int x, int y)
{
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    bigint newx = cellpos.first;
    bigint newy = cellpos.second;
    bigint xdelta = bigcellx;
    bigint ydelta = bigcelly;
    xdelta -= newx;
    ydelta -= newy;
    
    int xamount, yamount;
    int mag = currlayer->view->getmag();
    if (mag >= 0) {
        // move an integral number of cells
        xamount = xdelta.toint() << mag;
        yamount = ydelta.toint() << mag;
    } else {
        // convert cell deltas to screen pixels
        xdelta >>= -mag;
        ydelta >>= -mag;
        xamount = xdelta.toint();
        yamount = ydelta.toint();
    }
    
    if ( xamount != 0 || yamount != 0 ) {
        currlayer->view->move(xamount, yamount);
        
        // allow mouse interaction if script is running
        bool saveinscript = inscript;
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = saveinscript;
        
        cellpos = currlayer->view->at(x, y);
        bigcellx = cellpos.first;
        bigcelly = cellpos.second;
    }
    
    // need to update scroll bars if grid is bounded
    if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
        UpdateScrollBars();
    }
}

// -----------------------------------------------------------------------------

void PatternView::StopDraggingMouse()
{
    if ( HasCapture() ) {
        if (movingview && waitingforclick) {
            // don't release mouse capture here (paste loop won't detect click outside view)
        } else {
            ReleaseMouse();
        }
    }
    
    if ( dragtimer->IsRunning() ) dragtimer->Stop();
    
    if (selectingcells) {
        if (allowundo) RememberNewSelection(_("Selection"));
        selectingcells = false;                // tested by CanUndo
        mainptr->UpdateMenuItems();            // enable various Edit menu items
        if (allowundo) UpdateEditBar();        // update Undo/Redo buttons

        if (SelectionExists()) {
            DisplaySelectionSize();            // display selection population if enabled
            statusptr->Refresh(false);
        }
    }
    
    if (drawingcells && allowundo) {
        // MarkLayerDirty has set dirty flag, so we need to
        // pass in the flag state saved before drawing started
        currlayer->undoredo->RememberCellChanges(_("Drawing"), currlayer->savedirty);
        drawingcells = false;                  // tested by CanUndo
        mainptr->UpdateMenuItems();            // enable Undo item
        UpdateEditBar();                       // update Undo/Redo buttons
    }
    
    if (clickedcontrol > NO_CONTROL) {
        if (currcontrol == clickedcontrol && !PANNING_CONTROL) {
            // only do non-panning function when button is released
            ProcessClickedControl();
        }
        clickedcontrol = NO_CONTROL;
        currcontrol = NO_CONTROL;
        RefreshRect(controlsrect, false);
        Update();
    }
    
    if (movingview && restorecursor != NULL) {
        // restore cursor temporarily changed to curs_hand due to middle button click
        SetCursorMode(restorecursor);
        restorecursor = NULL;
        mainptr->UpdateMenuItems();            // enable Edit > Cursor Mode
        UpdateEditBar();                       // update cursor mode buttons
    }
    
    drawingcells = false;
    selectingcells = false;
    movingview = false;
    
    CheckCursor(true);
}

// -----------------------------------------------------------------------------

void PatternView::RestoreSelection()
{
    currlayer->currsel = currlayer->savesel;
    StopDraggingMouse();
    
    // allow mouse interaction if script is running
    bool saveinscript = inscript;
    inscript = false;
    mainptr->UpdatePatternAndStatus();
    inscript = saveinscript;
    
    statusptr->DisplayMessage(_("New selection aborted."));
}

// -----------------------------------------------------------------------------

void PatternView::TestAutoFit()
{
    if (currlayer->autofit && mainptr->generating) {
        // assume user no longer wants us to do autofitting
        currlayer->autofit = false;
    }
}

// -----------------------------------------------------------------------------

void PatternView::ZoomInPos(int x, int y)
{
    // zoom in so that clicked cell stays under cursor
    TestAutoFit();
    if (currlayer->view->getmag() < MAX_MAG) {
        currlayer->view->zoom(x, y);
        // allow mouse interaction if script is running
        bool saveinscript = inscript;
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        bigview->UpdateScrollBars();
        inscript = saveinscript;
    } else {
        Beep();   // can't zoom in any further
    }
}

// -----------------------------------------------------------------------------

void PatternView::ZoomOutPos(int x, int y)
{
    // zoom out so that clicked cell stays under cursor
    TestAutoFit();
    currlayer->view->unzoom(x, y);
    // allow mouse interaction if script is running
    bool saveinscript = inscript;
    inscript = false;
    mainptr->UpdatePatternAndStatus();
    bigview->UpdateScrollBars();
    inscript = saveinscript;
}

// -----------------------------------------------------------------------------

void PatternView::SetViewSize(int wd, int ht)
{
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    
    if (tileindex < 0) {
        // use main viewport window's size to reset viewport in each layer
        ResizeLayers(wd, ht);
    }
    
    // only autofit when generating
    if (currlayer->autofit && mainptr && mainptr->generating)
        currlayer->algo->fit(*currlayer->view, 0);
    
    // update position of translucent controls
    switch (controlspos) {
        case 1:
            // top left corner
            controlsrect = wxRect(0, 0, controlswd, controlsht);
            break;
        case 2:
            // top right corner
            controlsrect = wxRect(wd - controlswd, 0, controlswd, controlsht);
            break;
        case 3:
            // bottom right corner
            controlsrect = wxRect(wd - controlswd, ht - controlsht, controlswd, controlsht);
            break;
        case 4:
            // bottom left corner
            controlsrect = wxRect(0, ht - controlsht, controlswd, controlsht);
            break;
        default:
            // controlspos should be 0 (controls are disabled)
            controlsrect = wxRect(0, 0, 0, 0);
    }
}

// -----------------------------------------------------------------------------

void PatternView::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    // OnPaint handlers must always create a wxPaintDC
    wxPaintDC dc(this);
    
    int wd, ht;
    GetClientSize(&wd, &ht);
    wd = wd * int(scalefactor);     // matches intscale in wxoverlay.cpp
    ht = ht * int(scalefactor);     // ditto

    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    
    if ( numclones > 0 && numlayers > 1 && (stacklayers || tilelayers) )
        SyncClones();
    
    if ( numlayers > 1 && tilelayers ) {
        if ( tileindex >= 0 && ( wd != GetLayer(tileindex)->view->getwidth() ||
                                 ht != GetLayer(tileindex)->view->getheight() ) ) {
            // might happen on Win/GTK???
            GetLayer(tileindex)->view->resize(wd, ht);
        }
    } else if ( wd != currlayer->view->getwidth() || ht != currlayer->view->getheight() ) {
        // need to change viewport size;
        // can happen on Windows when resizing/maximizing main window
        SetViewSize(wd, ht);
    }
    
    SetCurrent(*glcontext);
    
    if (initgl) {
        // do these gl calls once (and only after the window has been created)
        initgl = false;
    
        glDisable(GL_DEPTH_TEST);       // we only do 2D drawing
        glDisable(GL_DITHER);
        // glDisable(GL_MULTISAMPLE);   // unknown on Windows
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_FOG);
    
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
    
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // initialize GL matrix to match our preferred coordinate system
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, wd, ht, 0, -1, 1);   // origin is top left and y increases down
        glViewport(0, 0, wd, ht);
        glMatrixMode(GL_MODELVIEW);

        // get OpenGL version
        const char* version = NULL;
        version = (const char*)glGetString(GL_VERSION);
        if (version) {
            sscanf(version, "%d.%d", &glMajor, &glMinor);
        }

        // get maximum texture size
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMaxTextureSize);
        if (glMaxTextureSize < 1024) glMaxTextureSize = 1024;
    }
    
    DrawView(tileindex);
    
    SwapBuffers();
    
    // this is used to ensure viewport is updated immediately on Linux and Mac OS
    // when a script calls g.update or ov("update")
    view_painted = true;
}

// -----------------------------------------------------------------------------

void PatternView::OnSize(wxSizeEvent& event)
{
    if (!IsShownOnScreen()) return;     // must not call SetCurrent (on Linux at least)

    int wd, ht;
    GetClientSize(&wd, &ht);
    wd = wd * int(scalefactor);         // matches intscale in wxoverlay.cpp
    ht = ht * int(scalefactor);         // ditto
        
    // resize this viewport
    SetViewSize(wd, ht);
    
    SetCurrent(*glcontext);

    // update GL matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, wd, ht, 0, -1, 1);       // origin is top left and y increases down
    glViewport(0, 0, wd, ht);
    glMatrixMode(GL_MODELVIEW);
    
    event.Skip();
}

// -----------------------------------------------------------------------------

#ifdef __WXMAC__

// we need this hook handler to avoid problems seeing ctrl-Q and option-EINU`

void PatternView::OnCharHook(wxKeyEvent& event)
{
    int key = event.GetKeyCode();
    int mods = event.GetModifiers();

    // close any open tool tip window and clear any status bar message
    wxToolTip::RemoveToolTips();
    statusptr->ClearMessage();

    // avoid ctrl-Q not being seen until hit twice
    if (mods == wxMOD_RAW_CONTROL && key == 'Q') {
        // convert Q to lower case
        key = 'q';
        if (inscript && pass_key_events) {
            PassKeyToScript(key, mods);
        } else {
            ProcessKey(key, mods);
        }
        // don't call event.Skip()
        return;
    }

    // avoid option-EINU` not being seen and causing a beep
    if (mods == wxMOD_ALT && (key == 'E' || key == 'I' || key == 'N' ||
                              key == 'U' || key == '`')) {
        // convert key to lower case
        if (key >= 'A' && key <= 'Z') key += 32;
        if (inscript && pass_key_events) {
            PassKeyToScript(key, mods);
        } else {
            ProcessKey(key, mods);
        }
        // don't call event.Skip()
        return;
    }

    event.Skip();
}

#endif // __WXMAC__

// -----------------------------------------------------------------------------

#if defined(__WXMAC__) && wxCHECK_VERSION(2,9,0)
    // wxMOD_CONTROL has been changed to mean Command key down (sheesh!)
    #define wxMOD_CONTROL wxMOD_RAW_CONTROL
    #define ControlDown RawControlDown
#endif

void PatternView::OnKeyDown(wxKeyEvent& event)
{
    realkey = event.GetKeyCode();
    int mods = event.GetModifiers();
    
    #ifdef __WXMSW__
        // avoid selection size message being cleared
        if (selectingcells && realkey == WXK_SHIFT) return;
    #endif

    statusptr->ClearMessage();
    
    // wxGTK 2.8.12 does not set modifier flag if shift key pressed by itself
    // so best to play safe and test wxMOD_SHIFT or wxMOD_NONE
    if (realkey == WXK_SHIFT && (mods == wxMOD_SHIFT || mods == wxMOD_NONE)) {
        // pressing unmodified shift key temporarily toggles the draw/pick cursors or
        // the zoom in/out cursors; note that Windows sends multiple key-down events
        // while shift key is pressed so we must be careful to toggle only once
        if (currlayer->curs == curs_pencil && oldcursor == NULL) {
            oldcursor = curs_pencil;
            SetCursorMode(curs_pick);
            mainptr->UpdateUserInterface();
        } else if (currlayer->curs == curs_pick && oldcursor == NULL) {
            oldcursor = curs_pick;
            SetCursorMode(curs_pencil);
            mainptr->UpdateUserInterface();
        } else if (currlayer->curs == curs_zoomin && oldcursor == NULL) {
            oldcursor = curs_zoomin;
            SetCursorMode(curs_zoomout);
            mainptr->UpdateUserInterface();
        } else if (currlayer->curs == curs_zoomout && oldcursor == NULL) {
            oldcursor = curs_zoomout;
            SetCursorMode(curs_zoomin);
            mainptr->UpdateUserInterface();
        }
    } else if (oldcursor) {
        // for any other key combo we restore the cursor immediately rather than
        // wait for the shift key to be released; this avoids problems with OnKeyUp
        // not being called if the shift key is used in a keyboard shortcut that
        // adds a new layer or opens another window
        SetCursorMode(oldcursor);
        oldcursor = NULL;
        mainptr->UpdateUserInterface();
    }
    
    if (debuglevel == 1) {
        // set debugkey now but don't show it until OnChar
        debugkey = wxString::Format(_("OnKeyDown: key=%d (%c) mods=%d"),
                                    realkey, realkey < 128 ? wxChar(realkey) : wxChar('?'), mods);
    }
    
    // WARNING: logic must match that in KeyComboCtrl::OnKeyDown in wxprefs.cpp
    if (mods == wxMOD_NONE || realkey == WXK_ESCAPE || realkey > 127) {
        // tell OnChar handler to ignore realkey
        realkey = 0;
    }
    
#ifdef __WXMAC__
    // pass ctrl/cmd-key combos directly to OnChar
    if (realkey > 0 && ((mods & wxMOD_CONTROL) || (mods & wxMOD_CMD))) {
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
    
#ifdef __WXGTK__
    if (realkey == ' ' && mods == wxMOD_SHIFT) {
        // fix wxGTK bug (curiously, the bug isn't seen in the prefs dialog);
        // OnChar won't see the shift modifier, so set realkey to a special
        // value to tell OnChar that shift-space was pressed
        realkey = -666;
    }
#endif
    
    event.Skip();
}

// -----------------------------------------------------------------------------

void PatternView::OnKeyUp(wxKeyEvent& event)
{
    int key = event.GetKeyCode();
    
    if (key == WXK_SHIFT) {
        // releasing shift key sets cursor back to original state
        if (oldcursor) {
            SetCursorMode(oldcursor);
            oldcursor = NULL;
            mainptr->UpdateUserInterface();
        }
    }
    
    if (inscript && pass_key_events) {
        // let script decide what to do with key-up events
        PassKeyUpToScript(key);
    }

    // no need to call event.Skip() here
}

// -----------------------------------------------------------------------------

void PatternView::OnChar(wxKeyEvent& event)
{
    // get translated keyboard event
    int key = event.GetKeyCode();
    int mods = event.GetModifiers();
    
    if (debuglevel == 1) {
        debugkey += wxString::Format(_("\nOnChar: key=%d (%c) mods=%d"),
                                     key, key < 128 ? wxChar(key) : wxChar('?'), mods);
        Warning(debugkey);
    }
    
    // WARNING: logic must match that in KeyComboCtrl::OnChar in wxprefs.cpp
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
    
#ifdef __WXGTK__
    if (realkey == -666) {
        // OnKeyDown saw that shift-space was pressed but for some reason
        // OnChar doesn't see the modifier (ie. mods is wxMOD_NONE)
        key = ' ';
        mods = wxMOD_SHIFT;
    }
#endif
    
    // do this check first because we allow user to make a selection while
    // generating a pattern or running a script
    if ( selectingcells && key == WXK_ESCAPE ) {
        RestoreSelection();
        return;
    }
    
    if ( inscript && (pass_key_events || key == WXK_ESCAPE) ) {
        // let script decide what to do with the key
        PassKeyToScript(key, mods);
        return;
    }
    
    // test waitingforclick before mainptr->generating so user can cancel
    // a paste operation while generating
    if ( waitingforclick && key == WXK_ESCAPE ) {
        AbortPaste();
        return;
    }
    
    if ( TimelineExists() && key == WXK_ESCAPE ) {
        if (currlayer->algo->isrecording()) {
            StartStopRecording();   // stop recording
        } else {
            PlayTimeline(0);        // stop autoplay
        }
        return;
    }
    
    if ( mainptr->generating && key == WXK_ESCAPE ) {
        mainptr->Stop();
        return;
    }
    
    ProcessKey(key, mods);
}

// -----------------------------------------------------------------------------

void PatternView::ProcessClickedControl()
{
    switch (clickedcontrol) {
        case STEP1_CONTROL:
            if (TimelineExists()) {
                // reset autoplay speed to 0 (no delay, no frame skipping)
                ResetTimelineSpeed();
            } else if (currlayer->currexpo != 0) {
                mainptr->SetStepExponent(0);
                statusptr->Refresh(false);
            }
            break;
            
        case SLOWER_CONTROL:
            mainptr->GoSlower();
            break;
            
        case FASTER_CONTROL:
            mainptr->GoFaster();
            break;
            
        case FIT_CONTROL:
            FitPattern();
            break;
            
        case ZOOMIN_CONTROL:
            ZoomIn();
            break;
            
        case ZOOMOUT_CONTROL:
            ZoomOut();
            break;
            
        case NW_CONTROL:
            PanNW();
            break;
            
        case UP_CONTROL:
            PanUp( SmallScroll(currlayer->view->getheight()) );
            break;
            
        case NE_CONTROL:
            PanNE();
            break;
            
        case LEFT_CONTROL:
            PanLeft( SmallScroll(currlayer->view->getwidth()) );
            break;
            
        case MIDDLE_CONTROL:
            ViewOrigin();
            break;
            
        case RIGHT_CONTROL:
            PanRight( SmallScroll(currlayer->view->getwidth()) );
            break;
            
        case SW_CONTROL:
            PanSW();
            break;
            
        case DOWN_CONTROL:
            PanDown( SmallScroll(currlayer->view->getheight()) );
            break;
            
        case SE_CONTROL:
            PanSE();
            break;
            
        default:
            // should never happen
            Warning(_("Bug detected in ProcessClickedControl!"));
    }
    
    // need to update viewport and status bar if script is running
    if (inscript) {
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = true;
    }
}

// -----------------------------------------------------------------------------

void PatternView::ProcessClick(int x, int y, int button, int modifiers)
{
    // user has clicked x,y pixel in viewport
    if (button == wxMOUSE_BTN_LEFT) {
        if (currlayer->curs == curs_pencil) {
            if (!PointInGrid(x, y)) {
                // best not to clobber any status bar message displayed by script
                Warning(_("Drawing is not allowed outside grid."));
                return;
            }
            if (inscript) {
                // best not to clobber any status bar message displayed by script
                Warning(_("Drawing is not allowed while a script is running."));
                return;
            }
            if (TimelineExists()) {
                statusptr->ErrorMessage(_("Drawing is not allowed if there is a timeline."));
                return;
            }
            if (currlayer->view->getmag() < 0) {
                statusptr->ErrorMessage(_("Drawing is not allowed at scales greater than 1 cell per pixel."));
                return;
            }
            if (mainptr->generating) {
                // we allow drawing while generating
                mainptr->draw_pending = true;
                // x and y have been multiplied by scalefactor in OnMouseDown
                mainptr->mouseevent.m_x = x / int(scalefactor);
                mainptr->mouseevent.m_y = y / int(scalefactor);
                mainptr->Stop();
                return;
            }
            StartDrawingCells(x, y);
            
        } else if (currlayer->curs == curs_pick) {
            if (!PointInGrid(x, y)) {
                // best not to clobber any status bar message displayed by script
                Warning(_("Picking is not allowed outside grid."));
                return;
            }
            if (inscript) {
                // best not to clobber any status bar message displayed by script
                Warning(_("Picking is not allowed while a script is running."));
                return;
            }
            if (currlayer->view->getmag() < 0) {
                statusptr->ErrorMessage(_("Picking is not allowed at scales greater than 1 cell per pixel."));
                return;
            }
            PickCell(x, y);
            
        } else if (currlayer->curs == curs_cross) {
            // note that we allow starting a selection outside the grid to make it
            // easier to select all of a bounded grid, but not when a script is running
            // because there's currently no way for the script to detect such a click
            if (inscript && !PointInGrid(x, y)) {
                Warning(_("Selecting is not allowed outside grid."));
                return;
            }
            TestAutoFit();
            StartSelectingCells(x, y, (modifiers & wxMOD_SHIFT) != 0);
            
        } else if (currlayer->curs == curs_hand) {
            TestAutoFit();
            StartMovingView(x, y);
            
        } else if (currlayer->curs == curs_zoomin) {
            ZoomInPos(x, y);
            
        } else if (currlayer->curs == curs_zoomout) {
            ZoomOutPos(x, y);
        }
        
    } else if (button == wxMOUSE_BTN_RIGHT) {
        // reverse the usual zoom direction
        if (currlayer->curs == curs_zoomin) {
            ZoomOutPos(x, y);
        } else if (currlayer->curs == curs_zoomout) {
            ZoomInPos(x, y);
        }
        
    } else if (button == wxMOUSE_BTN_MIDDLE) {
        // start panning, regardless of current cursor mode
        if (currlayer->curs != curs_hand) {
            restorecursor = currlayer->curs;
            SetCursorMode(curs_hand);
        }
        TestAutoFit();
        StartMovingView(x, y);
    }
    
    mainptr->UpdateUserInterface();
}

// -----------------------------------------------------------------------------

static int GetMouseModifiers(wxMouseEvent& event)
{
    int modbits = wxMOD_NONE;
    if (event.AltDown())       modbits |= wxMOD_ALT;
    if (event.CmdDown())       modbits |= wxMOD_CMD;
    if (event.ControlDown())   modbits |= wxMOD_CONTROL;
    if (event.MetaDown())      modbits |= wxMOD_META;
    if (event.ShiftDown())     modbits |= wxMOD_SHIFT;
    return modbits;
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseDown(wxMouseEvent& event)
{
    int x = event.GetX() * int(scalefactor);
    int y = event.GetY() * int(scalefactor);
    int button = event.GetButton();
    int modifiers = GetMouseModifiers(event);
    
    // ignore if a mouse button is already down
    if (mouseisdown) return;

    // flag that a mouse button is down
    mouseisdown = true;
    whichbuttondown = button;

    if (waitingforclick && button == wxMOUSE_BTN_LEFT) {
        // save paste location
        pastex = x;
        pastey = y;
        waitingforclick = false;    // terminate while (waitingforclick) loop
        return;
    }
    
    statusptr->ClearMessage();
    mainptr->showbanner = false;
    
    if (numlayers > 1 && tilelayers && tileindex < 0) {
        // ignore click in tile border
        return;
    }
    
    if (tileindex >= 0 && tileindex != currindex) {
        // switch current layer to clicked tile
        SwitchToClickedTile(tileindex);
        return;
    }
    
    // tileindex == currindex
    
    int ox, oy;
    if (showoverlay && curroverlay->PointInOverlay(x, y, &ox, &oy)
                    && !curroverlay->TransparentPixel(ox, oy)) {
        if (inscript && pass_mouse_events) {
            // let script decide what to do with click in non-transparent pixel in overlay
            PassOverlayClickToScript(ox, oy, button, modifiers);
        }
        // otherwise just ignore click in overlay
        return;
    }
    
    if (showcontrols) {
        currcontrol = WhichControl(x - controlsrect.x, y - controlsrect.y);
        if (currcontrol > NO_CONTROL) {
            clickedcontrol = currcontrol;       // remember which control was clicked
            clicktime = stopwatch->Time();      // remember when clicked (in millisecs)
            CaptureMouse();                     // get mouse up event even if outside view
            dragtimer->Start(SIXTY_HERTZ);      // call OnDragTimer ~60 times per sec
            RefreshRect(controlsrect, false);   // redraw clicked button
#ifdef __WXGTK__
            // nicer to see change immediately on Linux
            Update();
#endif
            if (PANNING_CONTROL) {
                // scroll immediately
                ProcessClickedControl();
            }
        }
        return;
    }
    
    if (inscript && pass_mouse_events && PointInGrid(x, y)) {
        // let script decide what to do with click in grid
        pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
        PassClickToScript(cellpos.first, cellpos.second, button, modifiers);
        return;
    }
    
    ProcessClick(x, y, button, modifiers);
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseUp(wxMouseEvent& event)
{
    // if the button released was not the first held down then ignore
    int button = event.GetButton();
    if (button != whichbuttondown) return;

    // same button released so process
    mouseisdown = false;

    if (drawingcells || selectingcells || movingview || clickedcontrol > NO_CONTROL) {
        StopDraggingMouse();
    } else if (mainptr->draw_pending) {
        // this can happen if user does a quick click while pattern is generating,
        // so set a special flag to force drawing to terminate
        stopdrawing = true;
    }
    
    if (inscript && pass_mouse_events) {
        // let script decide what to do with mouse up event
        PassMouseUpToScript(event.GetButton());
        return;
    }
}

// -----------------------------------------------------------------------------

void PatternView::ResetMouseDown()
{
    // mouseisdown needs to be reset if a right-click causes a modal dialog
    // to appear which then fails to send a mouse-up event (bug only in wxMac???)
    mouseisdown = false;
}

// -----------------------------------------------------------------------------

// mouse capture can be lost on Windows before mouse-up event
void PatternView::OnMouseCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
{
    mouseisdown = false;
    if (drawingcells || selectingcells || movingview || clickedcontrol > NO_CONTROL) {
        StopDraggingMouse();
    }
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseMotion(wxMouseEvent& event)
{
    statusptr->CheckMouseLocation(mainptr->infront);
    
    // check if translucent controls need to be shown/hidden
    // or if the cursor has moved out of or into the overlay
    if (mainptr->infront) {
        wxPoint pt(event.GetX() * int(scalefactor),
                   event.GetY() * int(scalefactor));
        bool active_tile = !(numlayers > 1 && tilelayers && tileindex != currindex);
        bool busy = drawingcells || selectingcells || movingview || waitingforclick;
        bool show = active_tile && !busy && (controlsrect.Contains(pt) || clickedcontrol > NO_CONTROL);
        if (showcontrols != show) {
            // let CheckCursor set showcontrols and call RefreshRect
            CheckCursor(true);
        } else if (showoverlay && active_tile && !busy) {
            // cursor might need to change
            CheckCursor(true);
        }
    }

    if (drawingcells || selectingcells || movingview || clickedcontrol > NO_CONTROL) {
        if (event.Dragging()) {
            wxTimer timer;
            wxTimerEvent unused(timer);
            OnDragTimer(unused);
        } else {
            // no mouse buttons are being pressed so ensure ReleaseMouse gets called
            // (in case OnMouseUp doesn't get called when it should)
            StopDraggingMouse();
        }
    }
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseEnter(wxMouseEvent& WXUNUSED(event))
{
    // wx bug??? we don't get this event if CaptureMouse has been called
    CheckCursor(mainptr->infront);
    // no need to call CheckMouseLocation here (OnMouseMotion will be called)
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseExit(wxMouseEvent& WXUNUSED(event))
{
    // Win only bug??? we don't get this event if CaptureMouse has been called
    CheckCursor(mainptr->infront);
    statusptr->CheckMouseLocation(mainptr->infront);
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseWheel(wxMouseEvent& event)
{
    // wheelpos should be persistent, because in theory we should keep track of
    // the remainder if the amount scrolled was not an even number of deltas
    static int wheelpos = 0;
    int delta, rot, x, y;
    
    if (mousewheelmode == 0) {
        // ignore wheel, according to user preference
        event.Skip();
        return;
    }
    
    // delta is the amount that represents one "step" of rotation.
    // Normally 120 on Win/Linux but 10 on Mac.
    // If wheelsens < MAX_SENSITIVITY then mouse wheel will be less sensitive.
    delta = event.GetWheelDelta() * (MAX_SENSITIVITY + 1 - wheelsens);
    rot = event.GetWheelRotation();
    x = event.GetX() * int(scalefactor);
    y = event.GetY() * int(scalefactor);
    
    if (mousewheelmode == 2)
        wheelpos -= rot;
    else
        wheelpos += rot;
    
    // DEBUG:
    // statusptr->DisplayMessage(wxString::Format(_("delta=%d rot=%d wheelpos=%d"), delta, rot, wheelpos));
    
    while (wheelpos >= delta) {
        wheelpos -= delta;
        if (inscript && pass_mouse_events) {
            // let script decide what to do with wheel event
            PassZoomOutToScript(x, y);
        } else {
            TestAutoFit();
            currlayer->view->unzoom(x, y);
        }
    }
    
    while (wheelpos <= -delta) {
        wheelpos += delta;
        if (inscript && pass_mouse_events) {
            // let script decide what to do with wheel event
            PassZoomInToScript(x, y);
        } else {
            TestAutoFit();
            if (currlayer->view->getmag() < MAX_MAG) {
                currlayer->view->zoom(x, y);
            } else {
                Beep();
                wheelpos = 0;
                break;         // best not to beep lots of times
            }
        }
    }
    
    if (inscript && pass_mouse_events) return;
    
    // allow mouse interaction if script is running
    bool saveinscript = inscript;
    inscript = false;
    mainptr->UpdatePatternAndStatus();
    bigview->UpdateScrollBars();
    inscript = saveinscript;
    // do following after restoring inscript so we don't change stop button if inscript
    mainptr->UpdateUserInterface();
}

// -----------------------------------------------------------------------------

// this flag is used to avoid re-entrancy in OnDragTimer
static bool in_timer = false;

void PatternView::OnDragTimer(wxTimerEvent& WXUNUSED(event))
{
    // called periodically while drawing/selecting/moving,
    // or if user has clicked a translucent control and button is still down
    if (in_timer) return;
    in_timer = true;

    wxPoint pt = ScreenToClient( wxGetMousePosition() );
    int x = pt.x * int(scalefactor);
    int y = pt.y * int(scalefactor);
    
    if (clickedcontrol > NO_CONTROL) {
        control_id oldcontrol = currcontrol;
        currcontrol = WhichControl(x - controlsrect.x, y - controlsrect.y);
        if (currcontrol == clickedcontrol) {
            if (PANNING_CONTROL && stopwatch->Time() - clicktime > 300) {
                // panning can be repeated while button is pressed, but only after
                // a short pause (0.3 secs) from the time the button was clicked
                // (this matches the way scroll buttons work on Mac/Windows)
                ProcessClickedControl();
            }
        } else {
            currcontrol = NO_CONTROL;
        }
        if (currcontrol != oldcontrol) RefreshRect(controlsrect, false);
        in_timer = false;
        return;
    }
    
    // don't test "!PointInView(x, y)" here -- we want to allow scrolling
    // in full screen mode when mouse is at outer edge of view
    if ( x <= 0 || x >= currlayer->view->getxmax() ||
         y <= 0 || y >= currlayer->view->getymax() ) {
        
        // user can disable scrolling
        if ( drawingcells && !scrollpencil ) {
            DrawCells(x, y);
            in_timer = false;
            return;
        }
        if ( selectingcells && !scrollcross ) {
            SelectCells(x, y);
            in_timer = false;
            return;
        }
        if ( movingview && !scrollhand ) {
            // make sure x,y is within viewport
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
            if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
            MoveView(x, y);
            in_timer = false;
            return;
        }
        
        // scroll view
        int xamount = 0;
        int yamount = 0;
        if (x <= 0) xamount = -SmallScroll( currlayer->view->getwidth() );
        if (y <= 0) yamount = -SmallScroll( currlayer->view->getheight() );
        if (x >= currlayer->view->getxmax())
            xamount = SmallScroll( currlayer->view->getwidth() );
        if (y >= currlayer->view->getymax())
            yamount = SmallScroll( currlayer->view->getheight() );
        
        if ( drawingcells ) {
            currlayer->view->move(xamount, yamount);
            mainptr->UpdatePatternAndStatus();
            
        } else if ( selectingcells ) {
            currlayer->view->move(xamount, yamount);
            // no need to call UpdatePatternAndStatus() here because
            // it will be called soon in SelectCells, except in this case:
            if (forceh || forcev || currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
                // selection might not change so must update pattern
                RefreshView();
                // need to update now if script is running
                if (inscript) {
                    inscript = false;
                    mainptr->UpdatePatternAndStatus();
                    inscript = true;
                }
            }
            
        } else if ( movingview ) {
            // scroll in opposite direction, and if both amounts are non-zero then
            // set both to same (larger) absolute value so user can scroll at 45 degrees
            if ( xamount != 0 && yamount != 0 ) {
                if ( abs(xamount) > abs(yamount) ) {
                    yamount = yamount < 0 ? -abs(xamount) : abs(xamount);
                } else {
                    xamount = xamount < 0 ? -abs(yamount) : abs(yamount);
                }
            }
            currlayer->view->move(-xamount, -yamount);
            
            // allow mouse interaction if script is running
            bool saveinscript = inscript;
            inscript = false;
            mainptr->UpdatePatternAndStatus();
            inscript = saveinscript;
            
            // adjust x,y and bigcellx,bigcelly for MoveView call below
            x += xamount;
            y += yamount;
            pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
            bigcellx = cellpos.first;
            bigcelly = cellpos.second;
        }
        
        // need to update scroll bars if grid is bounded
        if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
            UpdateScrollBars();
        }
    }
    
    if ( drawingcells ) {
        DrawCells(x, y);
        
    } else if ( selectingcells ) {
        SelectCells(x, y);
        
    } else if ( movingview ) {
        MoveView(x, y);
    }
    
    in_timer = false;
}

// -----------------------------------------------------------------------------

void PatternView::OnScroll(wxScrollWinEvent& event)
{   
    WXTYPE type = event.GetEventType();
    int orient = event.GetOrientation();
    
    if (type == wxEVT_SCROLLWIN_LINEUP) {
        if (orient == wxHORIZONTAL) {
            PanLeft( SmallScroll(currlayer->view->getwidth()) );
        } else {
            PanUp( SmallScroll(currlayer->view->getheight()) );
        }
        
    } else if (type == wxEVT_SCROLLWIN_LINEDOWN) {
        if (orient == wxHORIZONTAL) {
            PanRight( SmallScroll(currlayer->view->getwidth()) );
        } else {
            PanDown( SmallScroll(currlayer->view->getheight()) );
        }
        
    } else if (type == wxEVT_SCROLLWIN_PAGEUP) {
        if (orient == wxHORIZONTAL) {
            PanLeft( BigScroll(currlayer->view->getwidth()) );
        } else {
            PanUp( BigScroll(currlayer->view->getheight()) );
        }
        
    } else if (type == wxEVT_SCROLLWIN_PAGEDOWN) {
        if (orient == wxHORIZONTAL) {
            PanRight( BigScroll(currlayer->view->getwidth()) );
        } else {
            PanDown( BigScroll(currlayer->view->getheight()) );
        }
        
    } else if (type == wxEVT_SCROLLWIN_THUMBTRACK) {
        int newpos = event.GetPosition();
        int amount = newpos - (orient == wxHORIZONTAL ? hthumb : vthumb);
        if (amount != 0) {
            TestAutoFit();
            if (currlayer->view->getmag() > 0) {
                // amount is in cells so convert to pixels
                amount = amount << currlayer->view->getmag();
            }
            if (orient == wxHORIZONTAL) {
                hthumb = newpos;
                currlayer->view->move(amount, 0);
                // don't call UpdateEverything here because it calls UpdateScrollBars
                RefreshView();
            } else {
                vthumb = newpos;
                currlayer->view->move(0, amount);
                // don't call UpdateEverything here because it calls UpdateScrollBars
                RefreshView();
            }
        }
    
    } else if (type == wxEVT_SCROLLWIN_THUMBRELEASE) {
        // now we can call UpdateScrollBars
        mainptr->UpdateEverything();
    }
    
    // need an update if script is running
    if (inscript && type != wxEVT_SCROLLWIN_THUMBTRACK) {
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        bigview->UpdateScrollBars();
        inscript = true;
    }
}

// -----------------------------------------------------------------------------

void PatternView::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
    // do nothing because we'll be painting the entire viewport
    
    // why does this get called even though we always call Refresh(false)???
    // and why does bg still get erased (on Mac and GTK, but not Windows)???
    // note that eraseBack parameter in wxWindowMac::Refresh in window.cpp is never used!
}

// -----------------------------------------------------------------------------

/* using this doesn't seem to change anything
static int attributes[5] = {
    WX_GL_DOUBLEBUFFER,
    WX_GL_RGBA,
    WX_GL_DEPTH_SIZE, 0,    // Golly only does 2D drawing
    0
};
*/

// create the viewport canvas

PatternView::PatternView(wxWindow* parent, wxCoord x, wxCoord y, int wd, int ht, long style)
: wxGLCanvas(parent, wxID_ANY, NULL /*attributes*/, wxPoint(x,y), wxSize(wd,ht), style)
{
    // create a new rendering context instance for this canvas
    glcontext = new wxGLContext(this);
    if (glcontext == NULL) Fatal(_("Failed to create OpenGL context!"));

    dragtimer = new wxTimer(this, wxID_ANY);
    if (dragtimer == NULL) Fatal(_("Failed to create drag timer!"));
    
    // avoid erasing background on Linux -- doesn't work
    // SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    
    // avoid resizing problems on Mac/Linux -- doesn't work
    // SetBackgroundStyle(wxBG_STYLE_PAINT);
    
    initgl = true;             // need to initialize GL state
    drawingcells = false;      // not drawing cells
    selectingcells = false;    // not selecting cells
    movingview = false;        // not moving view
    waitingforclick = false;   // not waiting for user to click
    mouseisdown = false;       // mouse button is not down
    nopattupdate = false;      // enable pattern updates
    showcontrols = false;      // not showing translucent controls
    oldcursor = NULL;          // for toggling cursor via shift key
    restorecursor = NULL;      // for restoring cursor changed by middle button click
}

// -----------------------------------------------------------------------------

PatternView::~PatternView()
{
    delete glcontext;
    delete dragtimer;
}

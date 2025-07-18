// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr, insideYield
#include "wxutils.h"       // for Warning, golly_random
#include "wxprefs.h"       // for randomfill, etc
#include "wxmain.h"        // for mainptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxscript.h"      // for inscript
#include "wxview.h"        // for viewptr->...
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxalgos.h"       // for *_ALGO, CreateNewUniverse
#include "wxlayer.h"       // for currlayer, MarkLayerDirty, etc
#include "wxselect.h"

// This module implements operations on selections.

// -----------------------------------------------------------------------------

Selection::Selection()
{
    exists = false;
}

// -----------------------------------------------------------------------------

Selection::Selection(int t, int l, int b, int r)
{
    // create rectangular selection if given edges are valid
    exists = (t <= b && l <= r);
    if (exists) {
        seltop = t;
        selleft = l;
        selbottom = b;
        selright = r;
    }
}

// -----------------------------------------------------------------------------

Selection::~Selection()
{
    // no need to do anything at the moment
}

// -----------------------------------------------------------------------------

bool Selection::operator==(const Selection& s) const
{
    if (!exists && !s.exists) {
        // neither selection exists
        return true;
    } else if (exists && s.exists) {
        // check if edges match
        return (seltop == s.seltop && selleft == s.selleft &&
                selbottom == s.selbottom && selright == s.selright);
    } else {
        // one selection exists but not the other
        return false;
    }
}

// -----------------------------------------------------------------------------

bool Selection::operator!=(const Selection& s) const
{
    return !(*this == s);
}

// -----------------------------------------------------------------------------

bool Selection::Exists()
{
    return exists;
}

// -----------------------------------------------------------------------------

void Selection::Deselect()
{
    exists = false;
}

// -----------------------------------------------------------------------------

bool Selection::TooBig()
{
    return viewptr->OutsideLimits(seltop, selleft, selbottom, selright);
}

// -----------------------------------------------------------------------------

bool Selection::CalculatePopulation(long timeout)
{
    // compute selection population
    int leftx = selleft.toint();
    int rightx = selright.toint();
    int bottomy = selbottom.toint();
    int topy = seltop.toint();
    lifealgo* curralgo = currlayer->algo;
    int nstates = curralgo->NumCellStates();
    int v = 0;

    // bail out if population count takes longer than supplied timeout (in milliseconds)
    long starttime = stopwatch->Time();
    long maxtime = starttime + timeout;

    // clear previous results
    for (int i = 0; i < nstates; i++) {
        selpop[i] = 0;
    }

    // count live cells
    for (int cy = topy; cy <= bottomy; cy++) {
        for (int cx = leftx; cx <= rightx; cx++) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip + cx > rightx)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                selpop[v]++;
            } else {
                cx = rightx + 1;     // done this row
            }
        }

        // bail out if calculation time exceeded
        if (stopwatch->Time() > maxtime) return false;
    }

    // calculation completed successfully on time
    return true;
}

// -----------------------------------------------------------------------------

void Selection::DisplaySize()
{
    bigint wd = selright;    wd -= selleft;   wd += bigint::one;
    bigint ht = selbottom;   ht -= seltop;    ht += bigint::one;
    wxString msg = _("Selection wd x ht = ");
    msg += statusptr->Stringify(wd);
    msg += _(" x ");
    msg += statusptr->Stringify(ht);

    // check if the selection population is enabled and is small enough to calculate population
    if (showselpop && !TooBig()) {
        // set a smaller timeout if currently selecting cells to keep UI responsive
        int timeout = viewptr->selectingcells ? 5 : 500;

        // attempt to calculate the selection population
        if (CalculatePopulation(timeout)) {
            // if calculation was successful then update the status message
            msg += _("    Inside=");
            int nstates = currlayer->algo->NumCellStates();
            int nactive = 0;
            int totalpop = 0;

            if (nstates == 2) {
                // for 2 state patterns just output the population
                totalpop = selpop[1];
                nactive = 1;
            } else {
                // for >2 state patterns compute and display the total population from the states
                for (int i = 1; i < nstates; i++) {
                    int pop = selpop[i];
                    if (pop) {
                        totalpop += pop;
                        nactive++;
                    }
                }
            }

            msg += statusptr->Stringify(totalpop);

            // if there is more than one active state display the population for each active state
            if (nactive > 1) {
                // output the individual state populations
                for (int i = 1; i < nstates; i++) {
                    int pop = selpop[i];
                    if (pop > 0) {
                        msg += _("  ");
                        msg += statusptr->Stringify(i);
                        msg += _("=");
                        msg += statusptr->Stringify(pop);
                    }
                }
            }

            // check if total population is available
            bigint popcount = currlayer->algo->getPopulation();
            if (popcount.sign() >= 0) {
                popcount -= totalpop;

                // display the population outside the selection if non-zero
                if (popcount > 0) {
                    msg += _("    Outside=");
                    msg += wxString(popcount.tostring(), wxConvLocal);
                }
            }
        }
    }

    statusptr->SetMessage(msg);
}

// -----------------------------------------------------------------------------

void Selection::SetRect(int x, int y, int wd, int ht)
{
    exists = (wd > 0 && ht > 0);
    if (exists) {
        seltop = y;
        selleft = x;
        // avoid int overflow
        ht--;
        wd--;
        selbottom = y;
        selbottom += ht;
        selright = x;
        selright += wd;
    }
}

// -----------------------------------------------------------------------------

void Selection::GetRect(int* x, int* y, int* wd, int* ht)
{
    *x = selleft.toint();
    *y = seltop.toint();
    *wd = selright.toint() - *x + 1;
    *ht = selbottom.toint() - *y + 1;
}

// -----------------------------------------------------------------------------

void Selection::SetEdges(bigint& t, bigint& l, bigint& b, bigint& r)
{
    exists = true;
    seltop = t;
    selleft = l;
    selbottom = b;
    selright = r;
    CheckGridEdges();
}

// -----------------------------------------------------------------------------

void Selection::CheckGridEdges()
{
    if (exists) {
        // change selection edges if necessary to ensure they are inside a bounded grid
        if (currlayer->algo->gridwd > 0) {
            if (selleft > currlayer->algo->gridright || selright < currlayer->algo->gridleft) {
                exists = false;   // selection is outside grid
                return;
            }
            if (selleft < currlayer->algo->gridleft) selleft = currlayer->algo->gridleft;
            if (selright > currlayer->algo->gridright) selright = currlayer->algo->gridright;
        }
        if (currlayer->algo->gridht > 0) {
            if (seltop > currlayer->algo->gridbottom || selbottom < currlayer->algo->gridtop) {
                exists = false;   // selection is outside grid
                return;
            }
            if (seltop < currlayer->algo->gridtop) seltop = currlayer->algo->gridtop;
            if (selbottom > currlayer->algo->gridbottom) selbottom = currlayer->algo->gridbottom;
        }
    }
}

// -----------------------------------------------------------------------------

bool Selection::Contains(bigint& t, bigint& l, bigint& b, bigint& r)
{
    return ( seltop <= t && selleft <= l && selbottom >= b && selright >= r );
}

// -----------------------------------------------------------------------------

bool Selection::Outside(bigint& t, bigint& l, bigint& b, bigint& r)
{
    return ( seltop > b || selleft > r || selbottom < t || selright < l );
}

// -----------------------------------------------------------------------------

bool Selection::ContainsCell(int x, int y)
{
    return ( x >= selleft.toint() && x <= selright.toint() &&
            y >= seltop.toint() && y <= selbottom.toint() );
}

// -----------------------------------------------------------------------------

bool Selection::SaveDifferences(lifealgo* oldalgo, lifealgo* newalgo,
                                int itop, int ileft, int ibottom, int iright)
{
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    int cx, cy;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;

    // compare patterns in given algos and call SaveCellChange for each different cell
    BeginProgress(_("Saving cell changes"));
    for ( cy=itop; cy<=ibottom; cy++ ) {
        for ( cx=ileft; cx<=iright; cx++ ) {
            int oldstate = oldalgo->getcell(cx, cy);
            int newstate = newalgo->getcell(cx, cy);
            if ( oldstate != newstate ) {
                // assume this is only called if allowundo && !currlayer->stayclean
                currlayer->undoredo->SaveCellChange(cx, cy, oldstate, newstate);
            }
            cntr++;
            if ((cntr % 4096) == 0) {
                abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
                if (abort) break;
            }
        }
        if (abort) break;
    }
    EndProgress();

    return !abort;
}

// -----------------------------------------------------------------------------

void Selection::Advance()
{
    if (insideYield > 0) return; // avoid recursion

    if (mainptr->generating || viewptr->drawingcells || viewptr->waitingforclick) return;

    if (!exists) {
        statusptr->ErrorMessage(no_selection);
        return;
    }

    if (currlayer->algo->isEmpty()) {
        statusptr->ErrorMessage(empty_selection);
        return;
    }

    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);

    // check if selection is completely outside pattern edges
    if (Outside(top, left, bottom, right)) {
        statusptr->ErrorMessage(empty_selection);
        return;
    }

    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    if (savecells && inscript) SavePendingChanges();

    bool boundedgrid = currlayer->algo->unbounded && (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0);

    // check if selection encloses entire pattern;
    // can't do this if qlife because it uses gen parity to decide which bits to draw;
    // also avoid this if undo/redo is enabled (too messy to remember cell changes)
    if ( currlayer->algtype != QLIFE_ALGO && !savecells && Contains(top, left, bottom, right) ) {
        mainptr->generating = true;
        wxGetApp().PollerReset();

        // step by one gen without changing gen count
        bigint savegen = currlayer->algo->getGeneration();
        bigint saveinc = currlayer->algo->getIncrement();
        currlayer->algo->setIncrement(1);
        if (boundedgrid) currlayer->algo->CreateBorderCells();
        currlayer->algo->step();
        if (boundedgrid) currlayer->algo->DeleteBorderCells();
        currlayer->algo->setIncrement(saveinc);
        currlayer->algo->setGeneration(savegen);

        mainptr->generating = false;

        // clear 1-cell thick strips just outside selection
        ClearOutside();
        MarkLayerDirty();
        mainptr->UpdateEverything();
        return;
    }

    // find intersection of selection and pattern to minimize work
    if (seltop > top) top = seltop;
    if (selleft > left) left = selleft;
    if (selbottom < bottom) bottom = selbottom;
    if (selright < right) right = selright;

    // check that intersection is within setcell/getcell limits
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(selection_too_big);
        return;
    }

    // create a temporary universe of same type as current universe
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype);
    if (tempalgo->setrule(currlayer->algo->getrule()))
        tempalgo->setrule(tempalgo->DefaultRule());

    // copy live cells in selection to temporary universe
    if ( !viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                            currlayer->algo, tempalgo, false, _("Saving selection")) ) {
        delete tempalgo;
        return;
    }

    if ( tempalgo->isEmpty() ) {
        statusptr->ErrorMessage(empty_selection);
        delete tempalgo;
        return;
    }

    // advance temporary universe by one gen
    mainptr->generating = true;
    wxGetApp().PollerReset();
    tempalgo->setIncrement(1);
    if (boundedgrid) tempalgo->CreateBorderCells();
    tempalgo->step();
    if (boundedgrid) tempalgo->DeleteBorderCells();
    mainptr->generating = false;

    if ( !tempalgo->isEmpty() ) {
        // temporary pattern might have expanded
        bigint temptop, templeft, tempbottom, tempright;
        tempalgo->findedges(&temptop, &templeft, &tempbottom, &tempright);
        if (temptop < top) top = temptop;
        if (templeft < left) left = templeft;
        if (tempbottom > bottom) bottom = tempbottom;
        if (tempright > right) right = tempright;

        // but ignore live cells created outside selection edges
        if (top < seltop) top = seltop;
        if (left < selleft) left = selleft;
        if (bottom > selbottom) bottom = selbottom;
        if (right > selright) right = selright;
    }

    if (savecells) {
        // compare selection rect in currlayer->algo and tempalgo and call SaveCellChange
        // for each cell that has a different state
        if ( SaveDifferences(currlayer->algo, tempalgo,
                             top.toint(), left.toint(), bottom.toint(), right.toint()) ) {
            if ( !currlayer->undoredo->RememberCellChanges(_("Advance Selection"), currlayer->dirty) ) {
                // pattern inside selection didn't change
                delete tempalgo;
                return;
            }
        } else {
            currlayer->undoredo->ForgetCellChanges();
            delete tempalgo;
            return;
        }
    }

    // copy all cells in new selection from tempalgo to currlayer->algo
    viewptr->CopyAllRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                         tempalgo, currlayer->algo, _("Copying advanced selection"));

    delete tempalgo;
    MarkLayerDirty();
    mainptr->UpdateEverything();   
}

// -----------------------------------------------------------------------------

void Selection::AdvanceOutside()
{
    if (insideYield > 0) return; // avoid recursion

    if (mainptr->generating || viewptr->drawingcells || viewptr->waitingforclick) return;

    if (!exists) {
        statusptr->ErrorMessage(no_selection);
        return;
    }

    if (currlayer->algo->isEmpty()) {
        statusptr->ErrorMessage(empty_outside);
        return;
    }

    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);

    // check if selection encloses entire pattern
    if (Contains(top, left, bottom, right)) {
        statusptr->ErrorMessage(empty_outside);
        return;
    }

    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    if (savecells && inscript) SavePendingChanges();

    bool boundedgrid = currlayer->algo->unbounded && (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0);

    // check if selection is completely outside pattern edges;
    // can't do this if qlife because it uses gen parity to decide which bits to draw;
    // also avoid this if undo/redo is enabled (too messy to remember cell changes)
    if ( currlayer->algtype != QLIFE_ALGO && !savecells && Outside(top, left, bottom, right) ) {
        mainptr->generating = true;
        wxGetApp().PollerReset();

        // step by one gen without changing gen count
        bigint savegen = currlayer->algo->getGeneration();
        bigint saveinc = currlayer->algo->getIncrement();
        currlayer->algo->setIncrement(1);
        if (boundedgrid) currlayer->algo->CreateBorderCells();
        currlayer->algo->step();
        if (boundedgrid) currlayer->algo->DeleteBorderCells();
        currlayer->algo->setIncrement(saveinc);
        currlayer->algo->setGeneration(savegen);

        mainptr->generating = false;

        // clear selection in case pattern expanded into it
        Clear();
        MarkLayerDirty();
        mainptr->UpdateEverything();
        return;
    }

    // check that pattern is within setcell/getcell limits
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(_("Pattern is outside +/- 10^9 boundary."));
        return;
    }

    lifealgo* oldalgo = NULL;
    if (savecells) {
        // copy current pattern to oldalgo, using same type and gen count
        // so we can switch to oldalgo if user decides to abort below
        oldalgo = CreateNewUniverse(currlayer->algtype);
        if (oldalgo->setrule(currlayer->algo->getrule()))
            oldalgo->setrule(oldalgo->DefaultRule());
        oldalgo->setGeneration( currlayer->algo->getGeneration() );
        if ( !viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                                currlayer->algo, oldalgo, false, _("Saving pattern")) ) {
            delete oldalgo;
            return;
        }
    }

    // create a new universe of same type
    lifealgo* newalgo = CreateNewUniverse(currlayer->algtype);
    if (newalgo->setrule(currlayer->algo->getrule()))
        newalgo->setrule(newalgo->DefaultRule());
    newalgo->setGeneration( currlayer->algo->getGeneration() );

    // copy (and kill) live cells in selection to new universe
    int iseltop    = seltop.toint();
    int iselleft   = selleft.toint();
    int iselbottom = selbottom.toint();
    int iselright  = selright.toint();
    if ( !viewptr->CopyRect(iseltop, iselleft, iselbottom, iselright,
                            currlayer->algo, newalgo, true,
                            _("Saving and erasing selection")) ) {
        // aborted, so best to restore selection
        if ( !newalgo->isEmpty() ) {
            newalgo->findedges(&top, &left, &bottom, &right);
            viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                              newalgo, currlayer->algo, false,
                              _("Restoring selection"));
        }
        delete newalgo;
        if (savecells) delete oldalgo;
        mainptr->UpdateEverything();
        return;
    }

    // advance current universe by 1 generation
    mainptr->generating = true;
    wxGetApp().PollerReset();
    currlayer->algo->setIncrement(1);
    if (boundedgrid) currlayer->algo->CreateBorderCells();
    currlayer->algo->step();
    if (boundedgrid) currlayer->algo->DeleteBorderCells();
    mainptr->generating = false;

    if ( !currlayer->algo->isEmpty() ) {
        // find new edges and copy current pattern to new universe,
        // except for any cells that were created in selection
        // (newalgo contains the original selection)
        bigint t, l, b, r;
        currlayer->algo->findedges(&t, &l, &b, &r);
        int itop = t.toint();
        int ileft = l.toint();
        int ibottom = b.toint();
        int iright = r.toint();
        int ht = ibottom - itop + 1;
        int cx, cy;

        // for showing accurate progress we need to add pattern height to pop count
        // in case this is a huge pattern with many blank rows
        double maxcount = currlayer->algo->getPopulation().todouble() + ht;
        double accumcount = 0;
        int currcount = 0;
        int v = 0;
        bool abort = false;
        BeginProgress(_("Copying advanced pattern"));

        lifealgo* curralgo = currlayer->algo;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            currcount++;
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = curralgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;

                    // only copy cell if outside selection
                    if ( cx < iselleft || cx > iselright ||
                        cy < iseltop || cy > iselbottom ) {
                        newalgo->setcell(cx, cy, v);
                    }

                    currcount++;
                } else {
                    cx = iright;  // done this row
                }
                if (currcount > 1024) {
                    accumcount += currcount;
                    currcount = 0;
                    abort = AbortProgress(accumcount / maxcount, wxEmptyString);
                    if (abort) break;
                }
            }
            if (abort) break;
        }

        newalgo->endofpattern();
        EndProgress();

        if (abort && savecells) {
            // revert back to pattern saved in oldalgo
            delete newalgo;
            delete currlayer->algo;
            currlayer->algo = oldalgo;
            mainptr->SetGenIncrement();
            mainptr->UpdateEverything();
            return;
        }
    }

    // switch to new universe (best to do this even if aborted)
    delete currlayer->algo;
    currlayer->algo = newalgo;
    mainptr->SetGenIncrement();

    if (savecells) {
        // compare patterns in oldalgo and currlayer->algo and call SaveCellChange
        // for each cell that has a different state; note that we need to compare
        // the union of the original pattern's rect and the new pattern's rect
        int otop = top.toint();
        int oleft = left.toint();
        int obottom = bottom.toint();
        int oright = right.toint();
        if (!currlayer->algo->isEmpty()) {
            currlayer->algo->findedges(&top, &left, &bottom, &right);
            int ntop = top.toint();
            int nleft = left.toint();
            int nbottom = bottom.toint();
            int nright = right.toint();
            if (ntop < otop) otop = ntop;
            if (nleft < oleft) oleft = nleft;
            if (nbottom > obottom) obottom = nbottom;
            if (nright > oright) oright = nright;
        }
        if ( SaveDifferences(oldalgo, currlayer->algo, otop, oleft, obottom, oright) ) {
            delete oldalgo;
            if ( !currlayer->undoredo->RememberCellChanges(_("Advance Outside"), currlayer->dirty) ) {
                // pattern outside selection didn't change
                mainptr->UpdateEverything();
                return;
            }
        } else {
            // revert back to pattern saved in oldalgo
            currlayer->undoredo->ForgetCellChanges();
            delete currlayer->algo;
            currlayer->algo = oldalgo;
            mainptr->SetGenIncrement();
            mainptr->UpdateEverything();
            return;
        }
    }

    MarkLayerDirty();
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void Selection::Modify(const bigint& xclick, const bigint& yclick,
                       bigint& anchorx, bigint& anchory,
                       bool* forceh, bool* forcev)
{
    // note that we include "=" in following tests to get sensible
    // results when modifying small selections (ht or wd <= 3)
    if ( yclick <= seltop && xclick <= selleft ) {
        // click is in or outside top left corner
        seltop = yclick;
        selleft = xclick;
        anchory = selbottom;
        anchorx = selright;

    } else if ( yclick <= seltop && xclick >= selright ) {
        // click is in or outside top right corner
        seltop = yclick;
        selright = xclick;
        anchory = selbottom;
        anchorx = selleft;

    } else if ( yclick >= selbottom && xclick >= selright ) {
        // click is in or outside bottom right corner
        selbottom = yclick;
        selright = xclick;
        anchory = seltop;
        anchorx = selleft;

    } else if ( yclick >= selbottom && xclick <= selleft ) {
        // click is in or outside bottom left corner
        selbottom = yclick;
        selleft = xclick;
        anchory = seltop;
        anchorx = selright;

    } else if (yclick <= seltop) {
        // click is in or above top edge
        *forcev = true;
        seltop = yclick;
        anchory = selbottom;

    } else if (yclick >= selbottom) {
        // click is in or below bottom edge
        *forcev = true;
        selbottom = yclick;
        anchory = seltop;

    } else if (xclick <= selleft) {
        // click is in or left of left edge
        *forceh = true;
        selleft = xclick;
        anchorx = selright;

    } else if (xclick >= selright) {
        // click is in or right of right edge
        *forceh = true;
        selright = xclick;
        anchorx = selleft;

    } else {
        // click is somewhere inside selection
        double wd = selright.todouble() - selleft.todouble() + 1.0;
        double ht = selbottom.todouble() - seltop.todouble() + 1.0;
        double onethirdx = selleft.todouble() + wd / 3.0;
        double twothirdx = selleft.todouble() + wd * 2.0 / 3.0;
        double onethirdy = seltop.todouble() + ht / 3.0;
        double twothirdy = seltop.todouble() + ht * 2.0 / 3.0;
        double midy = seltop.todouble() + ht / 2.0;
        double x = xclick.todouble();
        double y = yclick.todouble();

        if ( y < onethirdy && x < onethirdx ) {
            // click is near top left corner
            seltop = yclick;
            selleft = xclick;
            anchory = selbottom;
            anchorx = selright;

        } else if ( y < onethirdy && x > twothirdx ) {
            // click is near top right corner
            seltop = yclick;
            selright = xclick;
            anchory = selbottom;
            anchorx = selleft;

        } else if ( y > twothirdy && x > twothirdx ) {
            // click is near bottom right corner
            selbottom = yclick;
            selright = xclick;
            anchory = seltop;
            anchorx = selleft;

        } else if ( y > twothirdy && x < onethirdx ) {
            // click is near bottom left corner
            selbottom = yclick;
            selleft = xclick;
            anchory = seltop;
            anchorx = selright;

        } else if ( x < onethirdx ) {
            // click is near middle of left edge
            *forceh = true;
            selleft = xclick;
            anchorx = selright;

        } else if ( x > twothirdx ) {
            // click is near middle of right edge
            *forceh = true;
            selright = xclick;
            anchorx = selleft;

        } else if ( y < midy ) {
            // click is below middle section of top edge
            *forcev = true;
            seltop = yclick;
            anchory = selbottom;

        } else {
            // click is above middle section of bottom edge
            *forcev = true;
            selbottom = yclick;
            anchory = seltop;
        }
    }
}

// -----------------------------------------------------------------------------

void Selection::SetLeftRight(const bigint& x, const bigint& anchorx)
{
    if (x <= anchorx) {
        selleft = x;
        selright = anchorx;
    } else {
        selleft = anchorx;
        selright = x;
    }
    exists = true;
}

// -----------------------------------------------------------------------------

void Selection::SetTopBottom(const bigint& y, const bigint& anchory)
{
    if (y <= anchory) {
        seltop = y;
        selbottom = anchory;
    } else {
        seltop = anchory;
        selbottom = y;
    }
    exists = true;
}

// -----------------------------------------------------------------------------

void Selection::Fit()
{
    bigint newx = selright;
    newx -= selleft;
    newx += bigint::one;
    newx.div2();
    newx += selleft;

    bigint newy = selbottom;
    newy -= seltop;
    newy += bigint::one;
    newy.div2();
    newy += seltop;

    int mag = MAX_MAG;
    while (true) {
        currlayer->view->setpositionmag(newx, newy, mag);
        if (currlayer->view->contains(selleft, seltop) &&
            currlayer->view->contains(selright, selbottom) )
            break;
        mag--;
    }
}

// -----------------------------------------------------------------------------

void Selection::Shrink(bool fit, bool remove_if_empty)
{
    if (insideYield > 0) return; // avoid recursion

    if (!exists) return;

    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(fit ? ID_SHRINKFIT : ID_SHRINK);
        mainptr->Stop();
        return;
    }

    // check if there is no pattern
    if (currlayer->algo->isEmpty()) {
        if (remove_if_empty) {
            viewptr->RemoveSelection();
        } else {
            statusptr->ErrorMessage(empty_selection);
            if (fit) viewptr->FitSelection();
        }
        return;
    }

    // check if selection encloses entire pattern
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if (Contains(top, left, bottom, right)) {
        // shrink edges
        viewptr->SaveCurrentSelection();
        seltop = top;
        selleft = left;
        selbottom = bottom;
        selright = right;
        viewptr->RememberNewSelection(_("Shrink Selection"));
        viewptr->DisplaySelectionSize();
        if (fit)
            viewptr->FitSelection();
        else
            mainptr->UpdatePatternAndStatus();
        return;
    }

    // check if selection is completely outside pattern edges
    if (Outside(top, left, bottom, right)) {
        if (remove_if_empty) {
            viewptr->RemoveSelection();
        } else {
            statusptr->ErrorMessage(empty_selection);
            if (fit) viewptr->FitSelection();
        }
        return;
    }

    // find intersection of selection and pattern to minimize work
    if (seltop > top) top = seltop;
    if (selleft > left) left = selleft;
    if (selbottom < bottom) bottom = selbottom;
    if (selright < right) right = selright;

    // check that selection is small enough to save
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(selection_too_big);
        if (fit) viewptr->FitSelection();
        return;
    }

    // the easy way to shrink selection is to create a new temporary universe,
    // copy selection into new universe and then call findedges;
    // if only 2 cell states then use qlife because its findedges call is faster
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algo->NumCellStates() > 2 ?
                                           currlayer->algtype :
                                           QLIFE_ALGO);
    // make sure temporary universe has same # of cell states
    if (currlayer->algo->NumCellStates() > 2)
        if (tempalgo->setrule(currlayer->algo->getrule()))
            tempalgo->setrule(tempalgo->DefaultRule());

    // copy live cells in selection to temporary universe
    if ( viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                           currlayer->algo, tempalgo, false, _("Shrinking selection")) ) {
        if (tempalgo->isEmpty()) {
            if (remove_if_empty) {
                viewptr->RemoveSelection();
                delete tempalgo;
                return;
            } else {
                statusptr->ErrorMessage(empty_selection);
            }
        } else {
            viewptr->SaveCurrentSelection();
            tempalgo->findedges(&seltop, &selleft, &selbottom, &selright);
            viewptr->RememberNewSelection(_("Shrink Selection"));
            viewptr->DisplaySelectionSize();
            if (!fit) mainptr->UpdatePatternAndStatus();
        }
    }

    delete tempalgo;
    if (fit) viewptr->FitSelection();
}

// -----------------------------------------------------------------------------

bool Selection::Visible(wxRect* visrect)
{
    if (!exists) return false;

    pair<int,int> lt = currlayer->view->screenPosOf(selleft, seltop, currlayer->algo);
    pair<int,int> rb = currlayer->view->screenPosOf(selright, selbottom, currlayer->algo);

    if (lt.first > currlayer->view->getxmax() || rb.first < 0 ||
        lt.second > currlayer->view->getymax() || rb.second < 0) {
        // no part of selection is visible
        return false;
    }

    // all or some of selection is visible in viewport;
    // only set visible rectangle if requested
    if (visrect) {
        // first we must clip coords to viewport
        if (lt.first < 0) lt.first = 0;
        if (lt.second < 0) lt.second = 0;
        if (rb.first > currlayer->view->getxmax()) rb.first = currlayer->view->getxmax();
        if (rb.second > currlayer->view->getymax()) rb.second = currlayer->view->getymax();

        if (currlayer->view->getmag() > 0) {
            // move rb to pixel at bottom right corner of cell
            rb.first += (1 << currlayer->view->getmag()) - 1;
            rb.second += (1 << currlayer->view->getmag()) - 1;
            if (currlayer->view->getmag() > 1) {
                // avoid covering gaps at scale 1:4 and above
                rb.first--;
                rb.second--;
            }
            // clip to viewport again
            if (rb.first > currlayer->view->getxmax()) rb.first = currlayer->view->getxmax();
            if (rb.second > currlayer->view->getymax()) rb.second = currlayer->view->getymax();
        }

        visrect->SetX(lt.first);
        visrect->SetY(lt.second);
        visrect->SetWidth(rb.first - lt.first + 1);
        visrect->SetHeight(rb.second - lt.second + 1);
    }
    return true;
}

// -----------------------------------------------------------------------------

void Selection::EmptyUniverse()
{
    // save current step, scale, position and gen count
    int savebase = currlayer->currbase;
    int saveexpo = currlayer->currexpo;
    int savemag = currlayer->view->getmag();
    bigint savex = currlayer->view->x;
    bigint savey = currlayer->view->y;
    bigint savegen = currlayer->algo->getGeneration();

    // kill all live cells by replacing the current universe with a
    // new, empty universe which also uses the same rule
    mainptr->CreateUniverse();

    // restore step, scale, position and gen count
    currlayer->currbase = savebase;
    mainptr->SetStepExponent(saveexpo);
    // SetStepExponent calls SetGenIncrement
    currlayer->view->setpositionmag(savex, savey, savemag);
    currlayer->algo->setGeneration(savegen);

    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void Selection::Clear()
{
    if (insideYield > 0) return; // avoid recursion

    if (!exists) return;

    // no need to do anything if there is no pattern
    if (currlayer->algo->isEmpty()) return;

    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_CLEAR);
        mainptr->Stop();
        return;
    }

    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    if (savecells && inscript) SavePendingChanges();

    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);

    if ( !savecells && Contains(top, left, bottom, right) ) {
        // selection encloses entire pattern so just create empty universe
        EmptyUniverse();
        MarkLayerDirty();
        return;
    }

    // no need to do anything if selection is completely outside pattern edges
    if (Outside(top, left, bottom, right)) {
        return;
    }

    // find intersection of selection and pattern to minimize work
    if (seltop > top) top = seltop;
    if (selleft > left) left = selleft;
    if (selbottom < bottom) bottom = selbottom;
    if (selright < right) right = selright;

    // can only use setcell in limited domain
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(selection_too_big);
        return;
    }

    // clear all live cells in selection
    int itop = top.toint();
    int ileft = left.toint();
    int ibottom = bottom.toint();
    int iright = right.toint();
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    int cx, cy;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    int v = 0;
    bool abort = false;
    bool selchanged = false;
    BeginProgress(_("Clearing selection"));
    lifealgo* curralgo = currlayer->algo;
    for ( cy=itop; cy<=ibottom; cy++ ) {
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip + cx > iright)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                curralgo->setcell(cx, cy, 0);
                selchanged = true;
                if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
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
    if (selchanged) curralgo->endofpattern();
    EndProgress();

    if (selchanged) {
        if (savecells) currlayer->undoredo->RememberCellChanges(_("Clear"), currlayer->dirty);
        MarkLayerDirty();
        mainptr->UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

bool Selection::SaveOutside(bigint& t, bigint& l, bigint& b, bigint& r)
{
    if ( viewptr->OutsideLimits(t, l, b, r) ) {
        statusptr->ErrorMessage(pattern_too_big);
        return false;
    }

    int itop = t.toint();
    int ileft = l.toint();
    int ibottom = b.toint();
    int iright = r.toint();

    // save ALL cells if selection is completely outside pattern edges
    bool saveall = Outside(t, l, b, r);

    // integer selection edges must not be outside pattern edges
    int stop = itop;
    int sleft = ileft;
    int sbottom = ibottom;
    int sright = iright;
    if (!saveall) {
        if (seltop > t)    stop = seltop.toint();
        if (selleft > l)   sleft = selleft.toint();
        if (selbottom < b) sbottom = selbottom.toint();
        if (selright < r)  sright = selright.toint();
    }

    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    int cx, cy;
    int v = 0;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;
    BeginProgress(_("Saving outside selection"));
    lifealgo* curralgo = currlayer->algo;
    for ( cy=itop; cy<=ibottom; cy++ ) {
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip + cx > iright)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                if (saveall || cx < sleft || cx > sright || cy < stop || cy > sbottom) {
                    // cell is outside selection edges
                    currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
                }
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
    EndProgress();

    if (abort) currlayer->undoredo->ForgetCellChanges();
    return !abort;
}

// -----------------------------------------------------------------------------

void Selection::ClearOutside()
{
    if (insideYield > 0) return; // avoid recursion

    if (!exists) return;

    // no need to do anything if there is no pattern
    if (currlayer->algo->isEmpty()) return;

    // no need to do anything if selection encloses entire pattern
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if (Contains(top, left, bottom, right)) {
        return;
    }

    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_OUTSIDE);
        mainptr->Stop();
        return;
    }

    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    if (savecells && inscript) SavePendingChanges();

    if (savecells) {
        // save live cells outside selection
        if ( !SaveOutside(top, left, bottom, right) ) {
            return;
        }
    } else {
        // create empty universe if selection is completely outside pattern edges
        if (Outside(top, left, bottom, right)) {
            EmptyUniverse();
            MarkLayerDirty();
            return;
        }
    }

    // find intersection of selection and pattern to minimize work
    if (seltop > top) top = seltop;
    if (selleft > left) left = selleft;
    if (selbottom < bottom) bottom = selbottom;
    if (selright < right) right = selright;

    // check that selection is small enough to save
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(selection_too_big);
        return;
    }

    // create a new universe of same type
    lifealgo* newalgo = CreateNewUniverse(currlayer->algtype);
    if (newalgo->setrule(currlayer->algo->getrule()))
        newalgo->setrule(newalgo->DefaultRule());

    // set same gen count
    newalgo->setGeneration( currlayer->algo->getGeneration() );

    // copy live cells in selection to new universe
    if ( viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                           currlayer->algo, newalgo, false, _("Saving selection")) ) {
        // delete old universe and point currlayer->algo at new universe
        delete currlayer->algo;
        currlayer->algo = newalgo;
        mainptr->SetGenIncrement();
        if (savecells) currlayer->undoredo->RememberCellChanges(_("Clear Outside"), currlayer->dirty);
        MarkLayerDirty();
        mainptr->UpdatePatternAndStatus();
    } else {
        // CopyRect was aborted, so don't change current universe
        delete newalgo;
        if (savecells) currlayer->undoredo->ForgetCellChanges();
    }
}

// -----------------------------------------------------------------------------

void Selection::AddEOL(char* &chptr)
{
#ifdef __WXMSW__
    // use DOS line ending (CR+LF) on Windows
    *chptr = '\r';
    chptr += 1;
    *chptr = '\n';
    chptr += 1;
#else
    // use LF on Linux or Mac
    *chptr = '\n';
    chptr += 1;
#endif
}

// -----------------------------------------------------------------------------

const int WRLE_NONE = -3;
const int WRLE_EOP = -2;
const int WRLE_NEWLINE = -1;

void Selection::AddRun(int state,                // in: state of cell to write
                       int multistate,           // true if #cell states > 2
                       unsigned int &run,        // in and out
                       unsigned int &linelen,    // ditto
                       char* &chptr)             // ditto
{
    // output of RLE pattern data is channelled thru here to make it easier to
    // ensure all lines have <= maxrleline characters
    const unsigned int maxrleline = 70;
    unsigned int i, numlen;
    char numstr[32];

    if ( run > 1 ) {
        sprintf(numstr, "%u", run);
        numlen = (unsigned int)strlen(numstr);
    } else {
        numlen = 0;                      // no run count shown if 1
    }
    // keep linelen <= maxrleline
    if ( linelen + numlen + 1 + multistate > maxrleline ) {
        AddEOL(chptr);
        linelen = 0;
    }
    i = 0;
    while (i < numlen) {
        *chptr = numstr[i];
        chptr += 1;
        i++;
    }
    if (multistate) {
        if (state <= 0)
            *chptr = ".$!"[-state];
        else {
            if (state > 24) {
                int hi = (state - 25) / 24;
                *chptr = hi + 'p';
                chptr += 1;
                linelen += 1;
                state -= (hi + 1) * 24;
            }
            *chptr = 'A' + state - 1;
        }
    } else
        *chptr = "!$bo"[state+2];
    chptr += 1;
    linelen += numlen + 1;
    run = 0;                           // reset run count
}

// -----------------------------------------------------------------------------

void Selection::CopyToClipboard(bool cut)
{
    if (insideYield > 0) return; // avoid recursion

    // can only use getcell/setcell in limited domain
    if (TooBig()) {
        statusptr->ErrorMessage(selection_too_big);
        return;
    }

    int itop = seltop.toint();
    int ileft = selleft.toint();
    int ibottom = selbottom.toint();
    int iright = selright.toint();
    unsigned int wd = iright - ileft + 1;
    unsigned int ht = ibottom - itop + 1;

    // convert cells in selection to RLE data in textptr
    char* textptr;
    char* etextptr;
    int cursize = 4096;

    textptr = (char*)malloc(cursize);
    if (textptr == NULL) {
        statusptr->ErrorMessage(_("Not enough memory for clipboard data!"));
        return;
    }
    etextptr = textptr + cursize;

    // add RLE header line
    sprintf(textptr, "x = %u, y = %u, rule = %s", wd, ht, currlayer->algo->getrule());
    char* chptr = textptr;
    chptr += strlen(textptr);
    AddEOL(chptr);
    // save start of data in case livecount is zero
    int datastart = chptr - textptr;

    // add RLE pattern data
    unsigned int livecount = 0;
    unsigned int linelen = 0;
    unsigned int brun = 0;
    unsigned int orun = 0;
    unsigned int dollrun = 0;
    int laststate;
    int cx, cy;
    int v = 0;

    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    if (savecells && inscript) SavePendingChanges();

    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;
    if (cut)
        BeginProgress(_("Cutting selection"));
    else
        BeginProgress(_("Copying selection"));

    lifealgo* curralgo = currlayer->algo;
    int multistate = curralgo->NumCellStates() > 2;
    for ( cy=itop; cy<=ibottom; cy++ ) {
        laststate = WRLE_NONE;
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip + cx > iright)
                skip = -1;           // pretend we found no more live cells
            if (skip > 0) {
                // have exactly "skip" empty cells here
                if (laststate == 0) {
                    brun += skip;
                } else {
                    if (orun > 0) {
                        // output current run of live cells
                        AddRun(laststate, multistate, orun, linelen, chptr);
                    }
                    laststate = 0;
                    brun = skip;
                }
            }
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                livecount++;
                if (cut) {
                    curralgo->setcell(cx, cy, 0);
                    if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
                }
                if (laststate == v) {
                    orun++;
                } else {
                    if (dollrun > 0)
                        // output current run of $ chars
                        AddRun(WRLE_NEWLINE, multistate, dollrun, linelen, chptr);
                    if (brun > 0)
                        // output current run of dead cells
                        AddRun(0, multistate, brun, linelen, chptr);
                    if (orun > 0)
                        // output current run of other live cells
                        AddRun(laststate, multistate, orun, linelen, chptr);
                    laststate = v;
                    orun = 1;
                }
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
            if (chptr + 60 >= etextptr) {
                // nearly out of space; try to increase allocation
                ptrdiff_t delta = chptr - textptr;
                char* ntxtptr = (char*) realloc(textptr, 2*cursize);
                if (ntxtptr == 0) {
                    statusptr->ErrorMessage(_("No more memory for clipboard data!"));
                    // don't return here -- best to set abort flag and break so that
                    // partially cut/copied portion gets saved to clipboard
                    abort = true;
                    break;
                }
                chptr = ntxtptr + delta;
                cursize *= 2;
                etextptr = ntxtptr + cursize;
                textptr = ntxtptr;
            }
        }
        if (abort) break;
        // end of current row
        if (laststate == 0)
            // forget dead cells at end of row
            brun = 0;
        else if (laststate >= 0)
            // output current run of live cells
            AddRun(laststate, multistate, orun, linelen, chptr);
        dollrun++;
    }

    if (livecount == 0) {
        // no live cells in selection so simplify RLE data to "!"
        chptr = textptr + datastart;
        *chptr = '!';
        chptr++;
    } else {
        // terminate RLE data
        dollrun = 1;
        AddRun(WRLE_EOP, multistate, dollrun, linelen, chptr);
        if (cut) currlayer->algo->endofpattern();
    }
    AddEOL(chptr);
    *chptr = 0;

    EndProgress();

    if (cut && livecount > 0) {
        if (savecells) currlayer->undoredo->RememberCellChanges(_("Cut"), currlayer->dirty);
        // update currlayer->dirty AFTER RememberCellChanges
        MarkLayerDirty();
        mainptr->UpdatePatternAndStatus();
    }

    wxString text = wxString(textptr,wxConvLocal);
    mainptr->CopyTextToClipboard(text);
    free(textptr);
}

// -----------------------------------------------------------------------------

bool Selection::CanPaste(const bigint& wd, const bigint& ht, bigint& top, bigint& left)
{
    bigint selht = selbottom;  selht -= seltop;   selht += 1;
    bigint selwd = selright;   selwd -= selleft;  selwd += 1;
    if ( ht > selht || wd > selwd ) return false;

    // set paste rectangle's top left cell coord
    top = seltop;
    left = selleft;

    return true;
}

// -----------------------------------------------------------------------------

void Selection::RandomFill()
{
    if (insideYield > 0) return; // avoid recursion (eg. due to setcell calling checkevents)

    if (!exists) return;

    // can only use getcell/setcell in limited domain
    if (TooBig()) {
        statusptr->ErrorMessage(selection_too_big);
        return;
    }

    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_RANDOM);
        mainptr->Stop();
        return;
    }

    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    if (savecells && inscript) SavePendingChanges();

    // no need to kill cells if selection is empty
    bool killcells = !currlayer->algo->isEmpty();
    if ( killcells ) {
        // find pattern edges and compare with selection edges
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if (Contains(top, left, bottom, right)) {
            // selection encloses entire pattern so create empty universe
            if (savecells) {
                // don't kill pattern otherwise we can't use SaveCellChange below
            } else {
                EmptyUniverse();
                killcells = false;
            }
        } else if (Outside(top, left, bottom, right)) {
            // selection is completely outside pattern edges
            killcells = false;
        }
    }

    int itop = seltop.toint();
    int ileft = selleft.toint();
    int ibottom = selbottom.toint();
    int iright = selright.toint();
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;
    BeginProgress(_("Randomly filling selection"));
    int cx, cy;
    lifealgo* curralgo = currlayer->algo;
    int livestates = curralgo->NumRandomizedCellStates() - 1;    // don't count dead state

    for ( cy=itop; cy<=ibottom; cy++ ) {
        for ( cx=ileft; cx<=iright; cx++ ) {
            // randomfill is from 1..100
            if (savecells) {
                // remember cell change only if state changes
                int oldstate = curralgo->getcell(cx, cy);
                if ((golly_rand() % 100) < randomfill) {
                    int newstate = livestates < 2 ? 1 : 1 + (golly_rand() % livestates);
                    if (oldstate != newstate) {
                        curralgo->setcell(cx, cy, newstate);
                        currlayer->undoredo->SaveCellChange(cx, cy, oldstate, newstate);
                    }
                } else if (killcells && oldstate > 0) {
                    curralgo->setcell(cx, cy, 0);
                    currlayer->undoredo->SaveCellChange(cx, cy, oldstate, 0);
                }
            } else {
                if ((golly_rand() % 100) < randomfill) {
                    if (livestates < 2) {
                        curralgo->setcell(cx, cy, 1);
                    } else {
                        curralgo->setcell(cx, cy, 1 + (golly_rand() % livestates));
                    }
                } else if (killcells) {
                    curralgo->setcell(cx, cy, 0);
                }
            }
            cntr++;
            if ((cntr % 4096) == 0) {
                abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
                if (abort) break;
            }
        }
        if (abort) break;
    }

    currlayer->algo->endofpattern();
    EndProgress();

    if (savecells) currlayer->undoredo->RememberCellChanges(_("Random Fill"), currlayer->dirty);

    // update currlayer->dirty AFTER RememberCellChanges
    MarkLayerDirty();
    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

// only called by Selection::Flip
bool Selection::FlipRect(bool topbottom, lifealgo* srcalgo, lifealgo* destalgo, bool erasesrc,
                         int itop, int ileft, int ibottom, int iright)
{
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;
    int v = 0;
    int cx, cy, newx, newy, newxinc, newyinc;

    if (topbottom) {
        BeginProgress(_("Flipping top-bottom"));
        newy = ibottom;
        newyinc = -1;
        newxinc = 1;
    } else {
        BeginProgress(_("Flipping left-right"));
        newy = itop;
        newyinc = 1;
        newxinc = -1;
    }

    for ( cy=itop; cy<=ibottom; cy++ ) {
        newx = topbottom ? ileft : iright;
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = srcalgo->nextcell(cx, cy, v);
            if (skip + cx > iright)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                if (erasesrc) srcalgo->setcell(cx, cy, 0);
                newx += newxinc * skip;
                destalgo->setcell(newx, newy, v);
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
            newx += newxinc;
        }
        if (abort) break;
        newy += newyinc;
    }

    if (erasesrc) srcalgo->endofpattern();
    destalgo->endofpattern();
    EndProgress();

    return !abort;
}

// -----------------------------------------------------------------------------

bool Selection::Flip(bool topbottom, bool inundoredo)
{
    // we need to allow flipping a selection when waiting for paste click
    if (insideYield > 0 && !viewptr->waitingforclick) return false; // avoid recursion

    if (!exists) return false;

    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(topbottom ? ID_FLIPTB : ID_FLIPLR);
        mainptr->Stop();
        return true;
    }

    if (topbottom) {
        if (seltop == selbottom) return true;
    } else {
        if (selleft == selright) return true;
    }

    if (currlayer->algo->isEmpty()) return true;

    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);

    bigint stop = seltop;
    bigint sleft = selleft;
    bigint sbottom = selbottom;
    bigint sright = selright;

    bool simpleflip;
    if (Contains(top, left, bottom, right)) {
        // selection encloses entire pattern so we may only need to flip a smaller rectangle
        if (topbottom) {
            bigint tdiff = top; tdiff -= stop;
            bigint bdiff = sbottom; bdiff -= bottom;
            bigint mindiff = tdiff;
            if (bdiff < mindiff) mindiff = bdiff;
            stop += mindiff;
            sbottom -= mindiff;
            sleft = left;
            sright = right;
        } else {
            bigint ldiff = left; ldiff -= sleft;
            bigint rdiff = sright; rdiff -= right;
            bigint mindiff = ldiff;
            if (rdiff < mindiff) mindiff = rdiff;
            sleft += mindiff;
            sright -= mindiff;
            stop = top;
            sbottom = bottom;
        }
        simpleflip = true;
    } else {
        // selection encloses part of pattern so we can clip some selection edges
        // if they are outside the pattern edges
        if (topbottom) {
            if (sleft < left) sleft = left;
            if (sright > right) sright = right;
        } else {
            if (stop < top) stop = top;
            if (sbottom > bottom) sbottom = bottom;
        }
        simpleflip = false;
    }

    // can only use getcell/setcell in limited domain
    if ( viewptr->OutsideLimits(stop, sbottom, sleft, sright) ) {
        statusptr->ErrorMessage(selection_too_big);
        return false;
    }

    int itop = stop.toint();
    int ileft = sleft.toint();
    int ibottom = sbottom.toint();
    int iright = sright.toint();

    if (simpleflip) {
        // selection encloses all of pattern so we can flip into new universe
        // (must be same type) without killing live cells in selection
        lifealgo* newalgo = CreateNewUniverse(currlayer->algtype);
        if (newalgo->setrule(currlayer->algo->getrule()))
            newalgo->setrule(newalgo->DefaultRule());
        newalgo->setGeneration( currlayer->algo->getGeneration() );

        if ( FlipRect(topbottom, currlayer->algo, newalgo, false, itop, ileft, ibottom, iright) ) {
            // switch to newalgo
            delete currlayer->algo;
            currlayer->algo = newalgo;
            mainptr->SetGenIncrement();
        } else {
            // user aborted flip
            delete newalgo;
            return false;
        }
    } else {
        // flip into temporary universe and kill all live cells in selection;
        // if only 2 cell states then use qlife because its setcell/getcell calls are faster
        lifealgo* tempalgo = CreateNewUniverse(currlayer->algo->NumCellStates() > 2 ?
                                               currlayer->algtype :
                                               QLIFE_ALGO);
        // make sure temporary universe has same # of cell states
        if (currlayer->algo->NumCellStates() > 2)
            if (tempalgo->setrule(currlayer->algo->getrule()))
                tempalgo->setrule(tempalgo->DefaultRule());

        if ( FlipRect(topbottom, currlayer->algo, tempalgo, true, itop, ileft, ibottom, iright) ) {
            // find pattern edges in temporary universe (could be much smaller)
            // and copy temporary pattern into current universe
            tempalgo->findedges(&top, &left, &bottom, &right);
            viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                              tempalgo, currlayer->algo, false, _("Adding flipped selection"));
            delete tempalgo;
        } else {
            // user aborted flip so flip tempalgo pattern back into current universe
            FlipRect(topbottom, tempalgo, currlayer->algo, false, itop, ileft, ibottom, iright);
            delete tempalgo;
            return false;
        }
    }

    // flips are always reversible so no need to use SaveCellChange and RememberCellChanges
    if (allowundo && !currlayer->stayclean && !inundoredo) {
        if (inscript) SavePendingChanges();
        currlayer->undoredo->RememberFlip(topbottom, currlayer->dirty);
    }

    // update currlayer->dirty AFTER RememberFlip
    if (!inundoredo) MarkLayerDirty();
    mainptr->UpdatePatternAndStatus();

    return true;
}

// -----------------------------------------------------------------------------

const wxString rotate_clockwise =      _("Rotating selection +90 degrees");
const wxString rotate_anticlockwise =  _("Rotating selection -90 degrees");

// only called by Selection::Rotate
bool Selection::RotateRect(bool clockwise,
                           lifealgo* srcalgo, lifealgo* destalgo, bool erasesrc,
                           int itop, int ileft, int ibottom, int iright,
                           int ntop, int nleft, int nbottom, int nright)
{
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;
    int cx, cy, newx, newy, newxinc, newyinc, v=0;

    if (clockwise) {
        BeginProgress(rotate_clockwise);
        newx = nright;
        newyinc = 1;
        newxinc = -1;
    } else {
        BeginProgress(rotate_anticlockwise);
        newx = nleft;
        newyinc = -1;
        newxinc = 1;
    }

    for ( cy=itop; cy<=ibottom; cy++ ) {
        newy = clockwise ? ntop : nbottom;
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = srcalgo->nextcell(cx, cy, v);
            if (skip + cx > iright)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                if (erasesrc) srcalgo->setcell(cx, cy, 0);
                newy += newyinc * skip;
                destalgo->setcell(newx, newy, v);
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
            newy += newyinc;
        }
        if (abort) break;
        newx += newxinc;
    }

    if (erasesrc) srcalgo->endofpattern();
    destalgo->endofpattern();
    EndProgress();

    return !abort;
}

// -----------------------------------------------------------------------------

// only called by Selection::Rotate
bool Selection::RotatePattern(bool clockwise,
                              bigint& newtop, bigint& newbottom,
                              bigint& newleft, bigint& newright,
                              bool inundoredo)
{
    // create new universe of same type as current universe
    lifealgo* newalgo = CreateNewUniverse(currlayer->algtype);
    if (newalgo->setrule(currlayer->algo->getrule()))
        newalgo->setrule(newalgo->DefaultRule());

    // set same gen count
    newalgo->setGeneration( currlayer->algo->getGeneration() );

    // copy all live cells to new universe, rotating the coords by +/- 90 degrees
    int itop    = seltop.toint();
    int ileft   = selleft.toint();
    int ibottom = selbottom.toint();
    int iright  = selright.toint();
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;
    int cx, cy, newx, newy, newxinc, newyinc, firstnewy, v=0;

    if (clockwise) {
        BeginProgress(rotate_clockwise);
        firstnewy = newtop.toint();
        newx = newright.toint();
        newyinc = 1;
        newxinc = -1;
    } else {
        BeginProgress(rotate_anticlockwise);
        firstnewy = newbottom.toint();
        newx = newleft.toint();
        newyinc = -1;
        newxinc = 1;
    }

    lifealgo* curralgo = currlayer->algo;
    for ( cy=itop; cy<=ibottom; cy++ ) {
        newy = firstnewy;
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip + cx > iright)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                newy += newyinc * skip;
                newalgo->setcell(newx, newy, v);
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
            newy += newyinc;
        }
        if (abort) break;
        newx += newxinc;
    }

    newalgo->endofpattern();
    EndProgress();

    if (abort) {
        delete newalgo;
    } else {
        // rotate the selection edges
        seltop    = newtop;
        selbottom = newbottom;
        selleft   = newleft;
        selright  = newright;

        // switch to new universe and display results
        delete currlayer->algo;
        currlayer->algo = newalgo;
        mainptr->SetGenIncrement();
        viewptr->DisplaySelectionSize();

        // rotating entire pattern is easily reversible so no need to use
        // SaveCellChange and RememberCellChanges in this case
        if (allowundo && !currlayer->stayclean && !inundoredo) {
            if (inscript) SavePendingChanges();
            currlayer->undoredo->RememberRotation(clockwise, currlayer->dirty);
        }

        // update currlayer->dirty AFTER RememberRotation
        if (!inundoredo) MarkLayerDirty();
        mainptr->UpdatePatternAndStatus();
    }

    return !abort;
}

// -----------------------------------------------------------------------------

bool Selection::Rotate(bool clockwise, bool inundoredo)
{
    // we need to allow rotation of a selection when waiting for paste click
    if (insideYield > 0 && !viewptr->waitingforclick) return false; // avoid recursion

    if (!exists) return false;

    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(clockwise ? ID_ROTATEC : ID_ROTATEA);
        mainptr->Stop();
        return true;
    }

    // determine rotated selection edges
    bigint halfht = selbottom;   halfht -= seltop;    halfht.div2();
    bigint halfwd = selright;    halfwd -= selleft;   halfwd.div2();
    bigint midy = seltop;    midy += halfht;
    bigint midx = selleft;   midx += halfwd;
    bigint newtop    = midy;   newtop    += selleft;     newtop    -= midx;
    bigint newbottom = midy;   newbottom += selright;    newbottom -= midx;
    bigint newleft   = midx;   newleft   += seltop;      newleft   -= midy;
    bigint newright  = midx;   newright  += selbottom;   newright  -= midy;

    if (!inundoredo) {
        // check if rotated selection edges are outside bounded grid
        if ( (currlayer->algo->gridwd > 0 &&
                (newleft < currlayer->algo->gridleft || newright > currlayer->algo->gridright)) ||
             (currlayer->algo->gridht > 0 &&
                (newtop < currlayer->algo->gridtop || newbottom > currlayer->algo->gridbottom)) ) {
            statusptr->ErrorMessage(_("New selection would be outside grid boundary."));
            return false;
        }
    }

    // if there is no pattern then just rotate the selection edges
    if (currlayer->algo->isEmpty()) {
        viewptr->SaveCurrentSelection();
        seltop    = newtop;
        selbottom = newbottom;
        selleft   = newleft;
        selright  = newright;
        viewptr->RememberNewSelection(_("Rotation"));
        viewptr->DisplaySelectionSize();
        mainptr->UpdatePatternAndStatus();
        return true;
    }

    // if the current selection and the rotated selection are both outside the
    // pattern edges (ie. both are empty) then just rotate the selection edges
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if ( (seltop > bottom || selbottom < top || selleft > right || selright < left) &&
         (newtop > bottom || newbottom < top || newleft > right || newright < left) ) {
        viewptr->SaveCurrentSelection();
        seltop    = newtop;
        selbottom = newbottom;
        selleft   = newleft;
        selright  = newright;
        viewptr->RememberNewSelection(_("Rotation"));
        viewptr->DisplaySelectionSize();
        mainptr->UpdatePatternAndStatus();
        return true;
    }

    // can only use nextcell/getcell/setcell in limited domain
    if (TooBig()) {
        statusptr->ErrorMessage(selection_too_big);
        return false;
    }

    // make sure rotated selection edges are also within limits
    if ( viewptr->OutsideLimits(newtop, newbottom, newleft, newright) ) {
        statusptr->ErrorMessage(_("New selection would be outside +/- 10^9 boundary."));
        return false;
    }

    // use faster method if selection encloses entire pattern
    if (Contains(top, left, bottom, right)) {
        return RotatePattern(clockwise, newtop, newbottom, newleft, newright, inundoredo);
    }

    int itop    = seltop.toint();
    int ileft   = selleft.toint();
    int ibottom = selbottom.toint();
    int iright  = selright.toint();

    int ntop    = newtop.toint();
    int nleft   = newleft.toint();
    int nbottom = newbottom.toint();
    int nright  = newright.toint();

    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    // and we're not undoing/redoing an earlier rotation
    bool savecells = allowundo && !currlayer->stayclean && !inundoredo;
    if (savecells && inscript) SavePendingChanges();

    lifealgo* oldalgo = NULL;
    int otop = itop;
    int oleft = ileft;
    int obottom = ibottom;
    int oright = iright;

    if (savecells) {
        // copy current pattern to oldalgo using union of old and new selection rects
        if (otop > ntop) otop = ntop;
        if (oleft > nleft) oleft = nleft;
        if (obottom < nbottom) obottom = nbottom;
        if (oright < nright) oright = nright;
        oldalgo = CreateNewUniverse(currlayer->algo->NumCellStates() > 2 ?
                                    currlayer->algtype :
                                    QLIFE_ALGO);
        // make sure universe has same # of cell states
        if (currlayer->algo->NumCellStates() > 2)
            if (oldalgo->setrule(currlayer->algo->getrule()))
                oldalgo->setrule(oldalgo->DefaultRule());
        if ( !viewptr->CopyRect(otop, oleft, obottom, oright, currlayer->algo, oldalgo,
                                false, _("Saving part of pattern")) ) {
            delete oldalgo;
            return false;
        }
    }

    // create temporary universe; doesn't need to match current universe so
    // if only 2 cell states then use qlife because its setcell/getcell calls are faster
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algo->NumCellStates() > 2 ?
                                           currlayer->algtype :
                                           QLIFE_ALGO);
    // make sure temporary universe has same # of cell states
    if (currlayer->algo->NumCellStates() > 2)
        if (tempalgo->setrule(currlayer->algo->getrule()))
            tempalgo->setrule(tempalgo->DefaultRule());

    // copy (and kill) live cells in selection to temporary universe,
    // rotating the new coords by +/- 90 degrees
    if ( !RotateRect(clockwise, currlayer->algo, tempalgo, true,
                     itop, ileft, ibottom, iright,
                     ntop, nleft, nbottom, nright) ) {
        // user aborted rotation
        if (savecells) {
            // use oldalgo to restore erased selection
            viewptr->CopyRect(itop, ileft, ibottom, iright, oldalgo, currlayer->algo,
                              false, _("Restoring selection"));
            delete oldalgo;
        } else {
            // restore erased selection by rotating tempalgo in opposite direction
            // back into the current universe
            RotateRect(!clockwise, tempalgo, currlayer->algo, false,
                       ntop, nleft, nbottom, nright,
                       itop, ileft, ibottom, iright);
        }
        delete tempalgo;
        mainptr->UpdatePatternAndStatus();
        return false;
    }

    // copy rotated selection from temporary universe to current universe;
    // check if new selection rect is outside modified pattern edges
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if ( newtop > bottom || newbottom < top || newleft > right || newright < left ) {
        // safe to use fast nextcell calls
        viewptr->CopyRect(ntop, nleft, nbottom, nright,
                          tempalgo, currlayer->algo, false, _("Adding rotated selection"));
    } else {
        // have to use slow getcell calls
        viewptr->CopyAllRect(ntop, nleft, nbottom, nright,
                             tempalgo, currlayer->algo, _("Pasting rotated selection"));
    }
    // don't need temporary universe any more
    delete tempalgo;

    // rotate the selection edges
    seltop    = newtop;
    selbottom = newbottom;
    selleft   = newleft;
    selright  = newright;

    if (savecells) {
        // compare patterns in oldalgo and currlayer->algo and call SaveCellChange
        // for each cell that has a different state
        if ( SaveDifferences(oldalgo, currlayer->algo, otop, oleft, obottom, oright) ) {
            Selection oldsel(itop, ileft, ibottom, iright);
            Selection newsel(ntop, nleft, nbottom, nright);
            currlayer->undoredo->RememberRotation(clockwise, oldsel, newsel, currlayer->dirty);
        } else {
            currlayer->undoredo->ForgetCellChanges();
            Warning(_("You can't undo this change!"));
        }
        delete oldalgo;
    }

    // display results
    viewptr->DisplaySelectionSize();
    if (!inundoredo) MarkLayerDirty();
    mainptr->UpdatePatternAndStatus();

    return true;
}

// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "util.h"

#include "wxgolly.h"        // for wxGetApp, statusptr, viewptr, bigview
#include "wxutils.h"        // for BeginProgress, GetString, etc
#include "wxprefs.h"        // for allowundo, etc
#include "wxrule.h"         // for ChangeRule
#include "wxhelp.h"         // for LoadLexiconPattern
#include "wxstatus.h"       // for statusptr->...
#include "wxselect.h"       // for Selection
#include "wxview.h"         // for viewptr->...
#include "wxscript.h"       // for inscript, PassKeyToScript
#include "wxmain.h"         // for MainFrame
#include "wxundo.h"         // for undoredo->...
#include "wxalgos.h"        // for *_ALGO, algo_type, CreateNewUniverse, etc
#include "wxlayer.h"        // for currlayer, etc
#include "wxtimeline.h"     // for TimelineExists, UpdateTimelineBar, etc

// This module implements Control menu functions.

// -----------------------------------------------------------------------------

bool MainFrame::SaveStartingPattern()
{
    if ( currlayer->algo->getGeneration() > currlayer->startgen ) {
        // don't do anything if current gen count > starting gen
        return true;
    }
    
    // save current name, rule, dirty flag, scale, location, etc
    currlayer->startname = currlayer->currname;
    currlayer->startrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    currlayer->startdirty = currlayer->dirty;
    currlayer->startmag = viewptr->GetMag();
    viewptr->GetPos(currlayer->startx, currlayer->starty);
    currlayer->startbase = currlayer->currbase;
    currlayer->startexpo = currlayer->currexpo;
    currlayer->startalgo = currlayer->algtype;
    
    // if this layer is a clone then save some settings in other clones
    if (currlayer->cloneid > 0) {
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = GetLayer(i);
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                cloneptr->startname = cloneptr->currname;
                cloneptr->startx = cloneptr->view->x;
                cloneptr->starty = cloneptr->view->y;
                cloneptr->startmag = cloneptr->view->getmag();
                cloneptr->startbase = cloneptr->currbase;
                cloneptr->startexpo = cloneptr->currexpo;
            }
        }
    }
    
    // save current selection
    currlayer->startsel = currlayer->currsel;
    
    if ( !currlayer->savestart ) {
        // no need to save pattern (use currlayer->startfile as the starting pattern)
        if (currlayer->startfile.IsEmpty())
            Warning(_("Bug in SaveStartingPattern: startfile is empty!"));
        return true;
    }

    currlayer->startfile = currlayer->tempstart;     // ResetPattern will load tempstart

    // save starting pattern in tempstart file
    if ( currlayer->algo->hyperCapable() ) {
        // much faster to save pattern in a macrocell file
        const char* err = WritePattern(currlayer->tempstart, MC_format,
                                       no_compression, 0, 0, 0, 0);
        if (err) {
            statusptr->ErrorMessage(wxString(err,wxConvLocal));
            // don't allow user to continue generating
            return false;
        }
    } else {
        // can only save as RLE if edges are within getcell/setcell limits
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);      
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            statusptr->ErrorMessage(_("Starting pattern is outside +/- 10^9 boundary."));
            // don't allow user to continue generating
            return false;
        }
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();      
        // use XRLE format so the pattern's top left location and the current
        // generation count are stored in the file
        const char* err = WritePattern(currlayer->tempstart, XRLE_format, no_compression,
                                       itop, ileft, ibottom, iright);
        if (err) {
            statusptr->ErrorMessage(wxString(err,wxConvLocal));
            // don't allow user to continue generating
            return false;
        }
    }
    
    return true;
}

// -----------------------------------------------------------------------------

void MainFrame::ResetPattern(bool resetundo)
{
    if (currlayer->algo->getGeneration() == currlayer->startgen) return;
    if (viewptr->drawingcells || viewptr->selectingcells) return;
    
    if (generating) {
        command_pending = true;
        cmdevent.SetId(ID_RESET);
        Stop();
        return;
    }
    
    if (inscript) stop_after_script = true;
    
    if (currlayer->algo->getGeneration() < currlayer->startgen) {
        // if this happens then startgen logic is wrong
        Warning(_("Current gen < starting gen!"));
        return;
    }
    
    if (currlayer->startfile.IsEmpty()) {
        // if this happens then savestart or startfile logic is wrong
        Warning(_("Starting pattern cannot be restored!"));
        return;
    }
    
    if (allowundo && !currlayer->stayclean && inscript) {
        // script called reset()
        SavePendingChanges();
        currlayer->undoredo->RememberGenStart();
    }
    
    // restore pattern and settings saved by SaveStartingPattern;
    // first restore algorithm
    currlayer->algtype = currlayer->startalgo;
    
    // restore starting pattern
    LoadPattern(currlayer->startfile, wxEmptyString);
    
    if (currlayer->algo->getGeneration() != currlayer->startgen) {
        // LoadPattern failed to reset the gen count to startgen
        // (probably because the user deleted the starting pattern)
        // so best to clear the pattern and reset the gen count
        CreateUniverse();
        currlayer->algo->setGeneration(currlayer->startgen);
        Warning(_("Failed to reset pattern from this file:\n") + currlayer->startfile);
    }
    
    // restore settings saved by SaveStartingPattern
    RestoreRule(currlayer->startrule);
    currlayer->currname = currlayer->startname;
    currlayer->dirty = currlayer->startdirty;
    if (restoreview) {
        viewptr->SetPosMag(currlayer->startx, currlayer->starty, currlayer->startmag);
    }
    
    // restore step size and set increment
    currlayer->currbase = currlayer->startbase;
    currlayer->currexpo = currlayer->startexpo;
    SetGenIncrement();
    
    // if this layer is a clone then restore some settings in other clones
    if (currlayer->cloneid > 0) {
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = GetLayer(i);
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                cloneptr->currname = cloneptr->startname;
                if (restoreview) {
                    cloneptr->view->setpositionmag(cloneptr->startx, cloneptr->starty,
                                                   cloneptr->startmag);
                }
                cloneptr->currbase = cloneptr->startbase;
                cloneptr->currexpo = cloneptr->startexpo;
                // also synchronize dirty flags and update items in Layer menu
                cloneptr->dirty = currlayer->dirty;
                UpdateLayerItem(i);
            }
        }
    }
    
    // restore selection
    currlayer->currsel = currlayer->startsel;
    
    // switch to default colors and icons in case algo/rule changed
    // (even if rule didn't change user might have edited .rule file before doing reset)
    UpdateLayerColors();
    
    // update window title in case currname, rule or dirty flag changed;
    // note that UpdateLayerItem(currindex) gets called
    SetWindowTitle(currlayer->currname);
    UpdateEverything();
    
    if (allowundo && !currlayer->stayclean) {
        if (inscript) {
            // script called reset() so remember gen change (RememberGenStart was called above)
            currlayer->undoredo->RememberGenFinish();
        } else if (resetundo) {
            // wind back the undo history to the starting pattern
            currlayer->undoredo->SyncUndoHistory();
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::RestorePattern(bigint& gen, const wxString& filename,
                               bigint& x, bigint& y, int mag, int base, int expo)
{
    // called to undo/redo a generating change
    if (gen == currlayer->startgen) {
        // restore starting pattern (false means don't call SyncUndoHistory)
        ResetPattern(false);
    } else {
        // restore pattern in given filename;
        // false means don't update status bar (algorithm should NOT change)
        LoadPattern(filename, wxEmptyString, false);
        
        if (currlayer->algo->getGeneration() != gen) {
            // best to clear the pattern and set the expected gen count
            CreateUniverse();
            currlayer->algo->setGeneration(gen);
            Warning(_("Could not restore pattern from this file:\n") + filename);
        }
        
        // restore step size and set increment
        currlayer->currbase = base;
        currlayer->currexpo = expo;
        SetGenIncrement();
        
        // restore position and scale, if allowed
        if (restoreview) viewptr->SetPosMag(x, y, mag);
        
        if (strcmp(algoinfo[currlayer->algtype]->algoName, "RuleLoader") == 0) {
            // update colors and icons in case user edited .rule file before an undo/redo
            UpdateLayerColors();
        }
        
        UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

const char* MainFrame::ChangeGenCount(const char* genstring, bool inundoredo)
{
    // disallow alphabetic chars in genstring
    for (unsigned int i = 0; i < strlen(genstring); i++)
        if ( (genstring[i] >= 'a' && genstring[i] <= 'z') ||
             (genstring[i] >= 'A' && genstring[i] <= 'Z') )
            return "Alphabetic character is not allowed in generation string.";
    
    bigint oldgen = currlayer->algo->getGeneration();
    bigint newgen(genstring);
    
    if (genstring[0] == '+' || genstring[0] == '-') {
        // leading +/- sign so make newgen relative to oldgen
        bigint relgen = newgen;
        newgen = oldgen;
        newgen += relgen;
        if (newgen < bigint::zero) newgen = bigint::zero;
    }
    
    // set stop_after_script BEFORE testing newgen == oldgen so scripts
    // can call setgen("+0") to prevent further generating
    if (inscript) stop_after_script = true;
    
    if (newgen == oldgen) return NULL;
    
    if (!inundoredo && allowundo && !currlayer->stayclean && inscript) {
        // script called setgen()
        SavePendingChanges();
    }
    
    // need IsParityShifted() method???
    if (currlayer->algtype == QLIFE_ALGO && newgen.odd() != oldgen.odd()) {
        // qlife stores pattern in different bits depending on gen parity,
        // so we need to create a new qlife universe, set its gen, copy the
        // current pattern to the new universe, then switch to that universe
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            return "Pattern is too big to copy.";
        }
        // create a new universe of same type and same rule
        lifealgo* newalgo = CreateNewUniverse(currlayer->algtype);
        const char* err = newalgo->setrule(currlayer->algo->getrule());
        if (err) {
            delete newalgo;
            return "Current rule is no longer valid!";
        }
        newalgo->setGeneration(newgen);
        // copy pattern
        if ( !viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                                currlayer->algo, newalgo, false, _("Copying pattern")) ) {
            delete newalgo;
            return "Failed to copy pattern.";
        }
        // switch to new universe
        delete currlayer->algo;
        currlayer->algo = newalgo;
        SetGenIncrement();
    } else {
        currlayer->algo->setGeneration(newgen);
    }
    
    if (!inundoredo) {
        // save some settings for RememberSetGen below
        bigint oldstartgen = currlayer->startgen;
        bool oldsave = currlayer->savestart;
        
        // may need to change startgen and savestart
        if (oldgen == currlayer->startgen || newgen <= currlayer->startgen) {
            currlayer->startgen = newgen;
            currlayer->savestart = true;
        }
        
        if (allowundo && !currlayer->stayclean) {
            currlayer->undoredo->RememberSetGen(oldgen, newgen, oldstartgen, oldsave);
        }
    }
    
    UpdateStatus();
    return NULL;
}

// -----------------------------------------------------------------------------

void MainFrame::SetGeneration()
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(ID_SETGEN);
        Stop();
        return;
    }
    
    bigint oldgen = currlayer->algo->getGeneration();
    wxString result;
    wxString prompt = _("Enter a new generation count:");
    prompt += _("\n(+n/-n is relative to current count)");
    if ( GetString(_("Set Generation"), prompt,
                   wxString(oldgen.tostring(), wxConvLocal), result) ) {
        
        const char* err = ChangeGenCount(result.mb_str(wxConvLocal));
        
        if (err) {
            Warning(wxString(err,wxConvLocal));
        } else {
            // Reset/Undo/Redo items might become enabled or disabled
            // (we need to do this if user clicked "Generation=..." text)
            UpdateMenuItems();
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::SetGenIncrement()
{
    if (currlayer->currexpo > 0) {
        bigint inc = 1;
        int maxexpo = 1 ;
        if (currlayer->currbase <= 10000) {
           int mantissa = currlayer->currbase ;
           int himantissa = 0x7fffffff ;
           while (mantissa > 1 && 0 == (mantissa & 1))
              mantissa >>= 1 ;
           if (mantissa == 1) {
              maxexpo = 0x7fffffff ;
           } else {
              int p = mantissa ;
              while (p <= himantissa / mantissa) {
                 p *= mantissa ;
                 maxexpo++ ;
              }
           }
        }
        if (currlayer->currexpo > maxexpo)
           currlayer->currexpo = maxexpo ;
        // set increment to currbase^currexpo
        int i = currlayer->currexpo;
        while (i > 0) {
            if (currlayer->currbase > 10000) {
               inc = currlayer->currbase ;
            } else {
               inc.mul_smallint(currlayer->currbase);
            }
            i--;
        }
        currlayer->algo->setIncrement(inc);
    } else {
        currlayer->algo->setIncrement(1);
    }
}

// -----------------------------------------------------------------------------

void MainFrame::StartGenTimer()
{
    if (inscript) return;       // do NOT call OnGenTimer if a script is running!

    int interval = SIXTY_HERTZ; // do ~60 calls of OnGenTimer per sec
    
    // increase interval if user wants a delay
    if (currlayer->currexpo < 0) {
        interval = statusptr->GetCurrentDelay();
        if (interval < SIXTY_HERTZ) interval += SIXTY_HERTZ;
    }
    
    if (gentimer->IsRunning()) gentimer->Stop();
    gentimer->Start(interval, wxTIMER_CONTINUOUS);
}

// -----------------------------------------------------------------------------

void MainFrame::GoFaster()
{
    if (TimelineExists()) {
        PlayTimelineFaster();
    } else {
        currlayer->currexpo++;
        SetGenIncrement();
        // only need to refresh status bar
        UpdateStatus();
        if (generating && currlayer->currexpo <= 0) {
            // decrease gentimer interval
            StartGenTimer();
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::GoSlower()
{
    if (TimelineExists()) {
        PlayTimelineSlower();
    } else {
        if (currlayer->currexpo > minexpo) {
            currlayer->currexpo--;
            SetGenIncrement();
            // only need to refresh status bar
            UpdateStatus();
            if (generating && currlayer->currexpo < 0) {
                // increase gentimer interval
                StartGenTimer();
            }
        } else {
            Beep();
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::SetMinimumStepExponent()
{
    // set minexpo depending on mindelay and maxdelay
    minexpo = 0;
    if (mindelay > 0) {
        int d = mindelay;
        minexpo--;
        while (d < maxdelay) {
            d *= 2;
            minexpo--;
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::UpdateStepExponent()
{
    SetMinimumStepExponent();
    if (currlayer->currexpo < minexpo) currlayer->currexpo = minexpo;
    SetGenIncrement();

    if (generating && currlayer->currexpo <= 0) {
        // update gentimer interval
        StartGenTimer();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::SetStepExponent(int newexpo)
{
    currlayer->currexpo = newexpo;
    if (currlayer->currexpo < minexpo) currlayer->currexpo = minexpo;
    SetGenIncrement();

    if (generating && currlayer->currexpo <= 0) {
        // update gentimer interval
        StartGenTimer();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::SetBaseStep()
{
    int i;
    if ( GetInteger(_("Set Base Step"),
                    _("Temporarily change the current base step:"),
                    currlayer->currbase, 2, MAX_BASESTEP, &i) ) {
        currlayer->currbase = i;
        SetGenIncrement();
        UpdateStatus();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::DisplayPattern()
{
    // this routine is similar to UpdatePatternAndStatus() but if tiled windows
    // exist it only updates the current tile if possible; ie. it's not a clone
    // and tile views aren't synchronized
    
    if (IsIconized()) return;
    
    if (tilelayers && numlayers > 1 && !syncviews && currlayer->cloneid == 0) {
        // only update the current tile
        viewptr->Refresh(false);
    } else {
        // update main viewport window, possibly including all tile windows
        // (tile windows are children of bigview)
        if (numlayers > 1 && (stacklayers || tilelayers)) {
            bigview->Refresh(false);
        } else {
            viewptr->Refresh(false);
        }
    }
    
    if (showstatus) {
        statusptr->CheckMouseLocation(infront);
        statusptr->Refresh(false);
    }
}

// -----------------------------------------------------------------------------

bool MainFrame::StepPattern()
{
    lifealgo* curralgo = currlayer->algo;
    if (curralgo->unbounded && (curralgo->gridwd > 0 || curralgo->gridht > 0)) {
        // bounded grid, so temporarily set the increment to 1 so we can call
        // CreateBorderCells() and DeleteBorderCells() around each step()
        int savebase = currlayer->currbase;
        int saveexpo = currlayer->currexpo;
        bigint inc = curralgo->getIncrement();
        curralgo->setIncrement(1);
        while (inc > 0) {
            if (wxGetApp().Poller()->checkevents()) {
                SetGenIncrement();         // restore correct increment
                return false;
            }
            if (savebase != currlayer->currbase || saveexpo != currlayer->currexpo) {
                // user changed step base/exponent, so best to simply exit loop
                break;
            }
            if (!curralgo->CreateBorderCells()) {
                SetGenIncrement();         // restore correct increment
                return false;
            }
            curralgo->step();
            if (!curralgo->DeleteBorderCells()) {
                SetGenIncrement();         // restore correct increment
                return false;
            }
            if (curralgo->isrecording()) curralgo->extendtimeline();
            inc -= 1;
        }
        // safe way to restore correct increment in case user altered step base/exponent
        SetGenIncrement();
    } else {
        if (wxGetApp().Poller()->checkevents()) return false;
        curralgo->step();
        if (curralgo->isrecording()) curralgo->extendtimeline();
    }
    
    if (currlayer->autofit) viewptr->FitInView(0);
    
    if (!IsIconized()) DisplayPattern();
    
    if (autostop && curralgo->isEmpty()) {
        if (curralgo->getIncrement() > bigint::one) {
            statusptr->DisplayMessage(_("Pattern died at or before this generation."));
        } else {
            statusptr->DisplayMessage(_("Pattern died at this generation."));
        }
        return false;
    }
    
    return true;
}

// -----------------------------------------------------------------------------

void MainFrame::DoPendingAction(bool restart)
{
    if (command_pending) {
        command_pending = false;
        
        int id = cmdevent.GetId();
        switch (id) {
            // don't restart the generating loop after some commands
            case wxID_NEW:          NewPattern(); break;
            case wxID_OPEN:         OpenPattern(); break;
            case ID_OPEN_CLIP:      OpenClipboard(); break;
            case ID_RESET:          ResetPattern(); break;
            case ID_SETGEN:         SetGeneration(); break;
            case ID_UNDO:           currlayer->undoredo->UndoChange(); break;
            case ID_ADD_LAYER:      AddLayer(); break;
            case ID_DUPLICATE:      DuplicateLayer(); break;
            case ID_LOAD_LEXICON:   LoadLexiconPattern(); break;
            default:
                if ( id > ID_OPEN_RECENT && id <= ID_OPEN_RECENT + numpatterns ) {
                    OpenRecentPattern(id);
                    
                } else if ( id > ID_RUN_RECENT && id <= ID_RUN_RECENT + numscripts ) {
                    OpenRecentScript(id);
                    if (restart && !stop_after_script) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                        // avoid clearing status message due to script like density.py
                        keepmessage = true;
                    }
                    
                } else if ( id == ID_RUN_SCRIPT ) {
                    OpenScript();
                    if (restart && !stop_after_script) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                        // avoid clearing status message due to script like density.py
                        keepmessage = true;
                    }
                    
                } else if ( id == ID_RUN_CLIP ) {
                    RunClipboard();
                    if (restart && !stop_after_script) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                        // avoid clearing status message due to script like density.py
                        keepmessage = true;
                    }
                    
                } else if ( id >= ID_LAYER0 && id <= ID_LAYERMAX ) {
                    int oldcloneid = currlayer->cloneid;
                    SetLayer(id - ID_LAYER0);
                    // continue generating if new layer is a clone of old layer
                    if (restart && currlayer->cloneid > 0 && currlayer->cloneid == oldcloneid) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                    }
                    
                } else if ( id == ID_DEL_LAYER ) {
                    int wasclone = currlayer->cloneid > 0 &&
                    ((currindex == 0 && currlayer->cloneid == GetLayer(1)->cloneid) ||
                     (currindex > 0 && currlayer->cloneid == GetLayer(currindex-1)->cloneid));
                    DeleteLayer();
                    // continue generating if new layer is/was a clone of old layer
                    if (restart && wasclone) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                    }
                    
                } else {
                    // temporarily pretend the tool/layer/edit bars are not showing
                    // to avoid Update[Tool/Layer/Edit]Bar changing button states
                    bool saveshowtool = showtool;    showtool = false;
                    bool saveshowlayer = showlayer;  showlayer = false;
                    bool saveshowedit = showedit;    showedit = false;
                    
                    // process the pending command
                    cmdevent.SetEventType(wxEVT_COMMAND_MENU_SELECTED);
                    cmdevent.SetEventObject(this);
                    this->GetEventHandler()->ProcessEvent(cmdevent);
                    
                    // restore tool/layer/edit bar flags
                    showtool = saveshowtool;
                    showlayer = saveshowlayer;
                    showedit = saveshowedit;
                    
                    if (restart) {
                        // call StartGenerating again
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                    }
                }
        }
    }
    
    if (draw_pending) {
        draw_pending = false;
        
        // temporarily pretend the tool/layer/edit bars are not showing
        // to avoid Update[Tool/Layer/Edit]Bar changing button states
        bool saveshowtool = showtool;    showtool = false;
        bool saveshowlayer = showlayer;  showlayer = false;
        bool saveshowedit = showedit;    showedit = false;
        
        UpdateEverything();
        
        // do the drawing by creating a mouse down event so PatternView::OnMouseDown is called again,
        // but we need to reset mouseisdown flag which is currently true
        viewptr->mouseisdown = false;
        mouseevent.SetEventType(wxEVT_LEFT_DOWN);
        mouseevent.SetEventObject(viewptr);
        viewptr->GetEventHandler()->ProcessEvent(mouseevent);
        while (viewptr->drawingcells) {
            insideYield++;
            wxGetApp().Yield(true);
            insideYield--;
            wxMilliSleep(5);             // don't hog CPU
        }
        
        // restore tool/layer/edit bar flags
        showtool = saveshowtool;
        showlayer = saveshowlayer;
        showedit = saveshowedit;
        
        if (restart) {
            // call StartGenerating again
            wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
            wxPostEvent(this->GetEventHandler(), goevt);
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::DisplayTimingInfo()
{
    if (viewptr->waitingforclick) return;
    if (generating) {
        endtime = stopwatch->Time();
        endgen = currlayer->algo->getGeneration().todouble();
    }
    if (endtime > begintime) {
        double secs = (double)(endtime - begintime) / 1000.0;
        double gens = endgen - begingen;
        wxString s;
        s.Printf(_("%g gens in %g secs (%g gens/sec)."), gens, secs, gens/secs);
        statusptr->DisplayMessage(s);
    }
}

// -----------------------------------------------------------------------------

void MainFrame::StartGenerating()
{
    if (insideYield > 0 || viewptr->drawingcells || viewptr->waitingforclick) {
        return;
    }
    
    if (currlayer->algo->isEmpty()) {
        statusptr->ErrorMessage(empty_pattern);
        return;
    }
    
    if (currlayer->algo->isrecording()) {
        // don't attempt to save starting pattern here (let DeleteTimeline do it)
    } else if (!SaveStartingPattern()) {
        return;
    }
    
    // no need to test inscript or currlayer->stayclean
    if (allowundo && !currlayer->algo->isrecording()) currlayer->undoredo->RememberGenStart();

    // for DisplayTimingInfo
    begintime = stopwatch->Time();
    begingen = currlayer->algo->getGeneration().todouble();

    // for hyperspeed
    hypdown = 64;

    generating = true;
    wxGetApp().PollerReset();
    UpdateUserInterface();

    // only show hashing info while generating
    lifealgo::setVerbose(currlayer->showhashinfo);

    StartGenTimer();
}

// -----------------------------------------------------------------------------

void MainFrame::FinishUp()
{
    // display the final pattern
    if (currlayer->autofit) viewptr->FitInView(0);
    if (command_pending || draw_pending) {
        // let the pending command/draw do the update below
    } else {
        UpdateEverything();
    }
    
    // note that we must call RememberGenFinish BEFORE processing any pending command
    if (allowundo && !currlayer->algo->isrecording()) currlayer->undoredo->RememberGenFinish();
    
    // stop recording any timeline before processing any pending command
    if (currlayer->algo->isrecording()) {
        currlayer->algo->stoprecording();
        if (currlayer->algo->getframecount() > 0) {
            // probably best to go to last frame
            currlayer->currframe = currlayer->algo->getframecount() - 1;
            currlayer->autoplay = 0;
            currlayer->tlspeed = 0;
            currlayer->algo->gotoframe(currlayer->currframe);
            if (currlayer->autofit) viewptr->FitInView(1);
        }
        if (!showtimeline) ToggleTimelineBar();
        UpdateUserInterface();
    }
    
    DoPendingAction(true);     // true means we can restart generating
}

// -----------------------------------------------------------------------------

void MainFrame::StopGenerating()
{
    if (gentimer->IsRunning()) gentimer->Stop();
    generating = false;
    wxGetApp().PollerInterrupt();
    lifealgo::setVerbose(0);
    
    // for DisplayTimingInfo
    endtime = stopwatch->Time();
    endgen = currlayer->algo->getGeneration().todouble();

    if (insideYield > 0) {
        // we're currently in the event poller somewhere inside step(), so we must let
        // step() complete and only call FinishUp after StepPattern has finished
    } else {
        FinishUp();
    }
}

// -----------------------------------------------------------------------------

// this flag is used to avoid re-entrancy in OnGenTimer (note that on Windows
// the timer can fire while a wxMessageBox dialog is open)
static bool in_timer = false;

void MainFrame::OnGenTimer(wxTimerEvent& WXUNUSED(event))
{
    if (in_timer) return;
    in_timer = true;
    
    if (!StepPattern()) {
        if (generating) {
            // call StopGenerating() to stop gentimer
            Stop();
        } else {
            // StopGenerating() was called while in Yield()
            FinishUp();
        }
        in_timer = false;
        return;
    }
    
    if (currlayer->algo->isrecording()) {
        if (showtimeline) UpdateTimelineBar();
        if (currlayer->algo->getframecount() == MAX_FRAME_COUNT) {
            if (generating) {
                // call StopGenerating() to stop gentimer
                Stop();
            } else {
                // StopGenerating() was called while in Yield()
                FinishUp();
            }
            wxString msg;
            msg.Printf(_("No more frames can be recorded (maximum = %d)."), MAX_FRAME_COUNT);
            Warning(msg);
            in_timer = false;
            return;
        }
    } else if (currlayer->hyperspeed && currlayer->algo->hyperCapable()) {
        hypdown--;
        if (hypdown == 0) {
            hypdown = 64;
            GoFaster();
        }
    }
    
    if (!generating) {
        // StopGenerating() was called while in Yield()
        FinishUp();
    }
    
    in_timer = false;
}

// -----------------------------------------------------------------------------

void MainFrame::Stop()
{
    if (inscript) {
        PassKeyToScript(WXK_ESCAPE);
    } else if (generating) {
        StopGenerating();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::StartOrStop()
{
    if (inscript || generating) {
        Stop();
    } else if (TimelineExists()) {
        if (currlayer->algo->isrecording()) {
            // should never happen if generating is false
            Warning(_("Bug: recording but not generating!"));
        } else {
            PlayTimeline(1);        // play forwards or stop if already playing
        }
    } else {
        StartGenerating();
    }
}

// -----------------------------------------------------------------------------

// this global flag is used to avoid re-entrancy in NextGeneration()
// due to holding down the space/tab key
static bool inNextGen = false;

void MainFrame::NextGeneration(bool useinc)
{
    if (inNextGen) return;
    inNextGen = true;
    
    if (!inscript && generating) {
        Stop();
        inNextGen = false;
        return;
    }
    
    if (insideYield > 0) {
        // avoid calling step() recursively
        inNextGen = false;
        return;
    }
    
    if (viewptr->drawingcells || viewptr->waitingforclick) {
        Beep();
        inNextGen = false;
        return;
    }
    
    // best if generating stops after running a script like oscar.py or goto.py
    if (inscript) stop_after_script = true;
    
    lifealgo* curralgo = currlayer->algo;
    if (curralgo->isEmpty()) {
        statusptr->ErrorMessage(empty_pattern);
        inNextGen = false;
        return;
    }
    
    if (!SaveStartingPattern()) {
        inNextGen = false;
        return;
    }
    
    if (allowundo) {
        if (currlayer->stayclean) {
            // script has called run/step after a new/open command has set
            // stayclean true by calling MarkLayerClean
            if (curralgo->getGeneration() == currlayer->startgen) {
                // starting pattern has just been saved so we need to remember
                // this gen change in case user does a Reset after script ends
                // (RememberGenFinish will be called at the end of RunScript)
                if (currlayer->undoredo->savegenchanges) {
                    // script must have called reset command, so we need to call
                    // RememberGenFinish to match earlier RememberGenStart
                    currlayer->undoredo->savegenchanges = false;
                    currlayer->undoredo->RememberGenFinish();
                }
                currlayer->undoredo->RememberGenStart();
            }
        } else {
            // !currlayer->stayclean
            if (inscript) {
                // pass in false so we don't test savegenchanges flag;
                // ie. we only want to save pending cell changes here
                SavePendingChanges(false);
            }
            currlayer->undoredo->RememberGenStart();
        }
    }
    
    // curralgo->step() calls checkevents() so set generating flag
    generating = true;
    
    // only show hashing info while generating
    lifealgo::setVerbose( currlayer->showhashinfo );
    
    // avoid doing some things if NextGeneration is called from a script;
    // ie. by a run/step command
    if (!inscript) {
        if (useinc) {
            // for DisplayTimingInfo
            begintime = stopwatch->Time();
            begingen = curralgo->getGeneration().todouble();
        }
        wxGetApp().PollerReset();
        viewptr->CheckCursor(infront);
    }
    
    bool boundedgrid = curralgo->unbounded && (curralgo->gridwd > 0 || curralgo->gridht > 0);
    
    if (useinc) {
        // step by current increment
        if (curralgo->getIncrement() > bigint::one && !inscript) {
            UpdateToolBar();
            UpdateMenuItems();
        }
        if (boundedgrid) {
            // temporarily set the increment to 1 so we can call CreateBorderCells()
            // and DeleteBorderCells() around each step()
            int savebase = currlayer->currbase;
            int saveexpo = currlayer->currexpo;
            bigint inc = curralgo->getIncrement();
            curralgo->setIncrement(1);
            while (inc > 0) {
                if (wxGetApp().Poller()->checkevents()) break;
                if (savebase != currlayer->currbase || saveexpo != currlayer->currexpo) {
                    // user changed step base/exponent, so reset increment to 1
                    inc = curralgo->getIncrement();
                    curralgo->setIncrement(1);
                }
                if (!curralgo->CreateBorderCells()) break;
                curralgo->step();
                if (!curralgo->DeleteBorderCells()) break;
                inc -= 1;
            }
            // safe way to restore correct increment in case user altered base/expo in above loop
            SetGenIncrement();
        } else {
            curralgo->step();
        }
    } else {
        // step by 1 gen
        bigint saveinc = curralgo->getIncrement();
        curralgo->setIncrement(1);
        if (boundedgrid) curralgo->CreateBorderCells();
        curralgo->step();
        if (boundedgrid) curralgo->DeleteBorderCells();
        curralgo->setIncrement(saveinc);
    }
    
    generating = false;
    
    lifealgo::setVerbose(0);
    
    if (!inscript) {
        if (useinc) {
            // for DisplayTimingInfo (we add 1 millisec here in case it took < 1 millisec)
            endtime = stopwatch->Time() + 1;
            endgen = curralgo->getGeneration().todouble();
        }
        // autofit is only used when doing many gens
        if (currlayer->autofit && useinc && curralgo->getIncrement() > bigint::one)
            viewptr->FitInView(0);
        UpdateEverything();
    }
    
    // we must call RememberGenFinish BEFORE processing any pending command
    if (allowundo && !currlayer->stayclean)
        currlayer->undoredo->RememberGenFinish();
    
    // process any pending command seen via checkevents() in curralgo->step()
    if (!inscript)
        DoPendingAction(false);     // false means don't restart generating loop
    
    inNextGen = false;
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleAutoFit()
{
    currlayer->autofit = !currlayer->autofit;
    
    // we only use autofit when generating; that's why the Auto Fit item
    // is in the Control menu and not in the View menu
    if (generating && currlayer->autofit) {
        viewptr->FitInView(0);
        UpdateEverything();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleHyperspeed()
{
    currlayer->hyperspeed = !currlayer->hyperspeed;
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleHashInfo()
{
    currlayer->showhashinfo = !currlayer->showhashinfo;
    
    // only show hashing info while generating
    if (generating) lifealgo::setVerbose( currlayer->showhashinfo );
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleShowPopulation()
{
    showpopulation = !showpopulation;
    
    if (generating && showstatus && !IsIconized()) statusptr->Refresh(false);
}

// -----------------------------------------------------------------------------

void MainFrame::ClearOutsideGrid()
{
    // check current pattern and clear any live cells outside bounded grid
    bool patternchanged = false;
    bool savechanges = allowundo && !currlayer->stayclean;
    
    // might also need to truncate selection
    currlayer->currsel.CheckGridEdges();

    if (!currlayer->algo->unbounded) {
        if (currlayer->algo->clipped_cells.size() > 0) {
            // cells outside the grid were clipped
            if (savechanges) {
                for (size_t i = 0; i < currlayer->algo->clipped_cells.size(); i += 3) {
                    int x = currlayer->algo->clipped_cells[i];
                    int y = currlayer->algo->clipped_cells[i+1];
                    int s = currlayer->algo->clipped_cells[i+2];
                    currlayer->undoredo->SaveCellChange(x, y, s, 0);
                }
            }
            currlayer->algo->clipped_cells.clear();
            patternchanged = true;
        }

    } else {
        // algo uses an unbounded grid
        if (currlayer->algo->isEmpty()) return;
        
        // check if current pattern is too big to use nextcell/setcell
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            statusptr->ErrorMessage(_("Pattern too big to check (outside +/- 10^9 boundary)."));
            return;
        }
        
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        
        // no need to do anything if pattern is entirely within grid
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
        if (itop >= gtop && ileft >= gleft && ibottom <= gbottom && iright <= gright) {
            return;
        }
        
        int ht = ibottom - itop + 1;
        int cx, cy;
        
        // for showing accurate progress we need to add pattern height to pop count
        // in case this is a huge pattern with many blank rows
        double maxcount = currlayer->algo->getPopulation().todouble() + ht;
        double accumcount = 0;
        int currcount = 0;
        bool abort = false;
        int v = 0;
        BeginProgress(_("Checking cells outside grid"));
        
        lifealgo* curralgo = currlayer->algo;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            currcount++;
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = curralgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;
                    if (cx < gleft || cx > gright || cy < gtop || cy > gbottom) {
                        // clear cell outside grid
                        if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
                        curralgo->setcell(cx, cy, 0);
                        patternchanged = true;
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
        
        curralgo->endofpattern();
        EndProgress();
    }
    
    if (patternchanged) {
        statusptr->ErrorMessage(_("Pattern was truncated (live cells were outside grid)."));
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ReduceCellStates(int newmaxstate)
{
    // check current pattern and reduce any cell states > newmaxstate
    bool patternchanged = false;
    bool savechanges = allowundo && !currlayer->stayclean;
    
    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(_("Pattern too big to check (outside +/- 10^9 boundary)."));
        return;
    }
    
    int itop = top.toint();
    int ileft = left.toint();
    int ibottom = bottom.toint();
    int iright = right.toint();
    int ht = ibottom - itop + 1;
    int cx, cy;
    
    // for showing accurate progress we need to add pattern height to pop count
    // in case this is a huge pattern with many blank rows
    double maxcount = currlayer->algo->getPopulation().todouble() + ht;
    double accumcount = 0;
    int currcount = 0;
    bool abort = false;
    int v = 0;
    BeginProgress(_("Checking cell states"));
    
    lifealgo* curralgo = currlayer->algo;
    for ( cy=itop; cy<=ibottom; cy++ ) {
        currcount++;
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip >= 0) {
                // found next live cell in this row
                cx += skip;
                if (v > newmaxstate) {
                    // reduce cell's current state to largest state
                    if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, newmaxstate);
                    curralgo->setcell(cx, cy, newmaxstate);
                    patternchanged = true;
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
    
    curralgo->endofpattern();
    EndProgress();
    
    if (patternchanged) {
        statusptr->ErrorMessage(_("Pattern has changed (new rule has fewer states)."));
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ShowRuleDialog()
{
    if (inscript || viewptr->waitingforclick) return;
    
    if (generating) {
        command_pending = true;
        cmdevent.SetId(ID_SETRULE);
        Stop();
        return;
    }
    
    algo_type oldalgo = currlayer->algtype;
    wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    viewptr->SaveCurrentSelection();
    
    if (ChangeRule()) {
        if (currlayer->algtype != oldalgo) {
        	// ChangeAlgorithm was called so we're almost done;
            // we just have to call UpdateEverything now that the main window is active
            UpdateEverything();
            return;
        }
        
        // show new rule in window title (but don't change file name);
        // even if the rule didn't change we still need to do this because
        // the user might have simply added/deleted a named rule
        SetWindowTitle(wxEmptyString);
        
        // check if the rule string changed, or the number of states changed
        // (the latter might happen if user modified .rule file)
        wxString newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
        int newmaxstate = currlayer->algo->NumCellStates() - 1;
        if (oldrule != newrule || oldmaxstate != newmaxstate) {
            
            // if pattern exists and is at starting gen then ensure savestart is true
            // so that SaveStartingPattern will save pattern to suitable file
            // (and thus undo/reset will work correctly)
            if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
                currlayer->savestart = true;
            }

            // if grid is bounded then remove any live cells outside grid edges
            if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
                ClearOutsideGrid();
            }
            
            // rule change might have changed the number of cell states;
            // if there are fewer states then pattern might change
            if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
                ReduceCellStates(newmaxstate);
            }
            
            if (allowundo) {
                currlayer->undoredo->RememberRuleChange(oldrule);
            }
        }
        
        // switch to default colors and icons for new rule (we need to do this even if
        // oldrule == newrule in case colors or icons changed)
        UpdateLayerColors();
        
        // pattern or colors or icons might have changed
        UpdateEverything();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ChangeAlgorithm(algo_type newalgotype, const wxString& newrule, bool inundoredo)
{
    if (newalgotype == currlayer->algtype) return;
    
    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    if ( !currlayer->algo->isEmpty() ) {
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            statusptr->ErrorMessage(_("Pattern cannot be converted (outside +/- 10^9 boundary)."));
            return;
        }
    }
    
    if (generating) {
        command_pending = true;
        cmdevent.SetId(ID_ALGO0 + newalgotype);
        Stop();
        return;
    }
    
    // save changes if undo/redo is enabled and script isn't constructing a pattern
    // and we're not undoing/redoing an earlier algo change
    bool savechanges = allowundo && !currlayer->stayclean && !inundoredo;
    if (savechanges && inscript) {
        // note that we must save pending gen changes BEFORE changing algo type
        // otherwise temporary files won't be the correct type (mc or rle)
        SavePendingChanges();
    }
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberAlgoChange
    if (savechanges) viewptr->SaveCurrentSelection();
    
    bool rulechanged = false;
    wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    
    // change algorithm type, reset step size, and update status bar immediately
    algo_type oldalgo = currlayer->algtype;
    currlayer->algtype = newalgotype;
    currlayer->currbase = algoinfo[newalgotype]->defbase;
    currlayer->currexpo = 0;
    UpdateStatus();
    
    // create a new universe of the requested flavor
    lifealgo* newalgo = CreateNewUniverse(newalgotype);
    
    if (inundoredo) {
        // switch to given newrule
        const char* err = newalgo->setrule( newrule.mb_str(wxConvLocal) );
        if (err) newalgo->setrule( newalgo->DefaultRule() );
    } else {
        const char* err;
        if (newrule.IsEmpty()) {
            // try to use same rule
            err = newalgo->setrule( currlayer->algo->getrule() );
        } else {
            // switch to newrule
            err = newalgo->setrule( newrule.mb_str(wxConvLocal) );
            rulechanged = true;
        }
        if (err) {
            wxString defrule = wxString(newalgo->DefaultRule(), wxConvLocal);
            if (newrule.IsEmpty() && oldrule.Find(':') >= 0) {
                // switch to new algo's default rule, but preserve the topology in oldrule
                // so we can do things like switch from "LifeHistory:T30,20" in RuleLoader
                // to "B3/S23:T30,20" in QuickLife
                if (defrule.Find(':') >= 0) {
                    // default rule shouldn't have a suffix but play safe and remove it
                    defrule = defrule.BeforeFirst(':');
                }
                defrule += wxT(":");
                defrule += oldrule.AfterFirst(':');
            }
            err = newalgo->setrule( defrule.mb_str(wxConvLocal) );
            // shouldn't ever fail but play safe
            if (err) newalgo->setrule( newalgo->DefaultRule() );
            rulechanged = true;
        }
    }
    
    // set same gen count
    newalgo->setGeneration( currlayer->algo->getGeneration() );
    
    bool patternchanged = false;
    if ( !currlayer->algo->isEmpty() ) {
        // copy pattern in current universe to new universe
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        int ht = ibottom - itop + 1;
        int cx, cy;
        
        // for showing accurate progress we need to add pattern height to pop count
        // in case this is a huge pattern with many blank rows
        double maxcount = currlayer->algo->getPopulation().todouble() + ht;
        double accumcount = 0;
        int currcount = 0;
        bool abort = false;
        int v = 0;
        BeginProgress(_("Converting pattern"));
        
        // set newalgo's grid edges so we can save cells that are outside grid
        int gtop = newalgo->gridtop.toint();
        int gleft = newalgo->gridleft.toint();
        int gbottom = newalgo->gridbottom.toint();
        int gright = newalgo->gridright.toint();
        if (newalgo->gridwd == 0) {
            // grid has infinite width
            gleft = INT_MIN;
            gright = INT_MAX;
        }
        if (newalgo->gridht == 0) {
            // grid has infinite height
            gtop = INT_MIN;
            gbottom = INT_MAX;
        }
        
        // need to check for state change if new algo has fewer states than old algo
        int newmaxstate = newalgo->NumCellStates() - 1;
        
        lifealgo* curralgo = currlayer->algo;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            currcount++;
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = curralgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;
                    if (cx < gleft || cx > gright || cy < gtop || cy > gbottom) {
                        // cx,cy is outside grid
                        if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
                        // no need to clear cell from curralgo (that universe will soon be deleted)
                        patternchanged = true;
                    } else {
                        if (v > newmaxstate) {
                            // reduce v to largest state in new algo
                            if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, newmaxstate);
                            v = newmaxstate;
                            patternchanged = true;
                        }
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
    }
    
    // delete old universe and point current universe to new universe
    delete currlayer->algo;
    currlayer->algo = newalgo;   
    SetGenIncrement();
    
    // if new grid is bounded then we might need to truncate the selection
    if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
        currlayer->currsel.CheckGridEdges();
    }
    
    // switch to default colors for new algo+rule
    UpdateLayerColors();
    
    if (!inundoredo) {
        // if pattern exists and is at starting gen then set savestart true
        // so that SaveStartingPattern will save pattern to suitable file
        // (and thus ResetPattern will work correctly)
        if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
            currlayer->savestart = true;
        }
        
        if (rulechanged) {
            // show new rule in window title (but don't change file name)
            SetWindowTitle(wxEmptyString);
            if (newrule.IsEmpty()) {
                if (patternchanged) {
                    statusptr->ErrorMessage(_("Rule has changed and pattern has changed."));
                } else {
                    // don't beep
                    statusptr->DisplayMessage(_("Rule has changed."));
                }
            } else {
                if (patternchanged) {
                    statusptr->ErrorMessage(_("Algorithm has changed and pattern has changed."));
                } else {
                    // don't beep
                    statusptr->DisplayMessage(_("Algorithm has changed."));
                }
            }
        } else if (patternchanged) {
            statusptr->ErrorMessage(_("Pattern has changed."));
        }
        
        if (!inscript) {
            UpdateEverything();
        }
    }
    
    if (savechanges) {
        currlayer->undoredo->RememberAlgoChange(oldalgo, oldrule);
    }
}

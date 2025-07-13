// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"
#include "util.h"           // for linereader

#include "utils.h"          // for gRect, Warning, etc
#include "prefs.h"          // for initrule, swapcolors, userrules, rulesdir, etc
#include "algos.h"          // for algo_type, initalgo, algoinfo, CreateNewUniverse, etc
#include "control.h"        // for generating
#include "select.h"         // for Selection
#include "file.h"           // for SetPatternTitle
#include "view.h"           // for OutsideLimits, CopyRect
#include "undo.h"           // for UndoRedo
#include "layer.h"

#include <map>              // for std::map
#include <algorithm>        // for std::replace

// -----------------------------------------------------------------------------

bool inscript = false;          // move to script.cpp if we ever support scripting!!!

int numlayers = 0;              // number of existing layers
int numclones = 0;              // number of cloned layers
int currindex = -1;             // index of current layer

Layer* currlayer = NULL;        // pointer to current layer
Layer* layer[MAX_LAYERS];       // array of layers

bool cloneavail[MAX_LAYERS];    // for setting unique cloneid
bool cloning = false;           // adding a cloned layer?
bool duplicating = false;       // adding a duplicated layer?

algo_type oldalgo;              // algorithm in old layer
std::string oldrule;            // rule in old layer
int oldmag;                     // scale in old layer
bigint oldx;                    // X position in old layer
bigint oldy;                    // Y position in old layer
TouchModes oldmode;             // touch mode in old layer

// -----------------------------------------------------------------------------

void CalculateTileRects(int bigwd, int bight)
{
    // set tilerect in each layer
    gRect r;
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
            break;
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
        for ( int i = 0; i < rows; i++ ) {
            for ( int j = 0; j < cols; j++ ) {
                int index = i * cols + j;
                if (index == numlayers) {
                    // numlayers == 3,5,7
                    layer[index - 1]->tilerect.width -= tileborder;
                } else {
                    layer[index]->tilerect.x += tileborder;
                    layer[index]->tilerect.y += tileborder;
                    layer[index]->tilerect.width -= tileborder;
                    layer[index]->tilerect.height -= tileborder;
                    if (j == cols - 1) layer[index]->tilerect.width -= tileborder;
                    if (i == rows - 1) layer[index]->tilerect.height -= tileborder;
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

    /*!!!
    // set size of each tile window
    for ( int i = 0; i < numlayers; i++ )
        layer[i]->tilewin->SetSize( layer[i]->tilerect );

    // set viewport size for each tile; this is currently the same as the
    // tilerect size because tile windows are created with wxNO_BORDER
    for ( int i = 0; i < numlayers; i++ ) {
        int wd, ht;
        layer[i]->tilewin->GetClientSize(&wd, &ht);
        // wd or ht might be < 1 on Windows
        if (wd < 1) wd = 1;
        if (ht < 1) ht = 1;
        layer[i]->view->resize(wd, ht);
    }
    */
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
        for (int i = 0; i < numlayers; i++)
            layer[i]->view->resize(wd, ht);
    }
}

// -----------------------------------------------------------------------------

/*!!!

void CreateTiles()
{
    // create tile windows
    for ( int i = 0; i < numlayers; i++ ) {
        layer[i]->tilewin = new PatternView(bigview,
                                            // correct size will be set below by ResizeTiles
                                            0, 0, 0, 0,
                                            // we draw our own tile borders
                                            wxNO_BORDER |
                                            // needed for wxGTK
                                            wxFULL_REPAINT_ON_RESIZE |
                                            wxWANTS_CHARS);
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
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    ResizeTiles(wd, ht);

    // change viewptr to tile window for current layer
    viewptr = currlayer->tilewin;
    if (mainptr->IsActive()) viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void DestroyTiles()
{
    // reset viewptr to main viewport window
    viewptr = bigview;
    if (mainptr->IsActive()) viewptr->SetFocus();

    // destroy all tile windows
    for ( int i = 0; i < numlayers; i++ )
        delete layer[i]->tilewin;

    // resize viewport in each layer to bigview's client area
    int wd, ht;
    bigview->GetClientSize(&wd, &ht);
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    for ( int i = 0; i < numlayers; i++ )
        layer[i]->view->resize(wd, ht);
}
!!!*/

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
                // cloneptr->touchmode = currlayer->touchmode;
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
    oldrule = currlayer->algo->getrule();

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

    if (syncmodes) {
        // save touch mode for use in CurrentLayerChanged
        oldmode = currlayer->touchmode;
    }
}

// -----------------------------------------------------------------------------

bool RestoreRule(const char* rule)
{
    const char* err = currlayer->algo->setrule(rule);
    if (err) {
        // this can happen if a .rule file was deleted
        // or it was edited and some sort of error introduced, so best to
        // use algo's default rule (which should never fail)
        currlayer->algo->setrule( currlayer->algo->DefaultRule() );
        std::string msg = "The rule \"";
        msg += rule;
        msg += "\" is no longer valid!\nUsing the default rule instead.";
        Warning(msg.c_str());
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------

/*!!!

void CurrentLayerChanged()
{
    // currlayer has changed since SaveLayerSettings was called;
    // update rule if the new currlayer uses a different algorithm or rule
    if ( currlayer->algtype != oldalgo || !currlayer->rule.IsSameAs(oldrule,false) ) {
        RestoreRule(currlayer->rule);
    }

    if (syncviews) currlayer->view->setpositionmag(oldx, oldy, oldmag);
    if (syncmodes) currlayer->touchmode = oldmode;

    // select current layer button (also deselects old button)
    layerbarptr->SelectButton(currindex, true);

    if (tilelayers && numlayers > 1) {
        // switch to new tile
        viewptr = currlayer->tilewin;
        if (mainptr->IsActive()) viewptr->SetFocus();
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
    mainptr->SetPatternTitle(currlayer->currname);

    mainptr->UpdateUserInterface(mainptr->IsActive());
    mainptr->UpdatePatternAndStatus();
}

!!!*/

// -----------------------------------------------------------------------------

static gBitmapPtr* CopyIcons(gBitmapPtr* srcicons, int maxstate)
{
    gBitmapPtr* iconptr = (gBitmapPtr*) malloc(256 * sizeof(gBitmapPtr));
    if (iconptr) {
        for (int i = 0; i < 256; i++) iconptr[i] = NULL;
        for (int i = 0; i <= maxstate; i++) {
            if (srcicons && srcicons[i]) {
                gBitmapPtr icon = (gBitmapPtr) malloc(sizeof(gBitmap));
                if (icon) {
                    icon->wd = srcicons[i]->wd;
                    icon->ht = srcicons[i]->ht;
                    if (srcicons[i]->pxldata == NULL) {
                        icon->pxldata = NULL;
                    } else {
                        int iconbytes = icon->wd * icon->ht * 4;
                        icon->pxldata = (unsigned char*) malloc(iconbytes);
                        if (icon->pxldata) {
                            memcpy(icon->pxldata, srcicons[i]->pxldata, iconbytes);
                        }
                    }
                }
                iconptr[i] = icon;
            }
        }
    }
    return iconptr;
}

// -----------------------------------------------------------------------------

static unsigned char* CreateIconAtlas(gBitmapPtr* srcicons, int iconsize)
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
                int wd = srcicons[state]->wd;       // should be iconsize - 1
                int ht = srcicons[state]->ht;       // ditto
                
                unsigned char* icondata = srcicons[state]->pxldata;
                if (icondata) {
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
                    int ipos = 0;
                    for (int row = 0; row < ht; row++) {
                        int rowstart = tpos;
                        for (int col = 0; col < wd; col++) {
                            unsigned char r = icondata[ipos];
                            unsigned char g = icondata[ipos+1];
                            unsigned char b = icondata[ipos+2];
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
                            ipos += 4;
                        }
                        // move to next row
                        tpos = rowstart + rowbytes;
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
    if (templayer == NULL) Warning("Failed to create temporary layer!");
    return templayer;
}

// -----------------------------------------------------------------------------

void AddLayer()
{
    if (numlayers >= MAX_LAYERS) return;

    if (generating) Warning("Bug detected in AddLayer!");

    if (numlayers == 0) {
        // creating the very first layer
        currindex = 0;
    } else {
        //!!! if (tilelayers && numlayers > 1) DestroyTiles();

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
    if (currlayer == NULL) Fatal("Failed to create new layer!");
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
            int maxstate = currlayer->algo->NumCellStates() - 1;
            currlayer->icons7x7 = CopyIcons(oldlayer->icons7x7, maxstate);
            currlayer->icons15x15 = CopyIcons(oldlayer->icons15x15, maxstate);
            currlayer->icons31x31 = CopyIcons(oldlayer->icons31x31, maxstate);
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
        //!!! if (tilelayers && numlayers > 1) CreateTiles();
        //!!! CurrentLayerChanged();
    }
}

// -----------------------------------------------------------------------------

void CloneLayer()
{
    if (numlayers >= MAX_LAYERS) return;

    if (generating) Warning("Bug detected in CloneLayer!");

    cloning = true;
    AddLayer();
    cloning = false;
}

// -----------------------------------------------------------------------------

void DuplicateLayer()
{
    if (numlayers >= MAX_LAYERS) return;

    if (generating) Warning("Bug detected in DuplicateLayer!");

    duplicating = true;
    AddLayer();
    duplicating = false;
}

// -----------------------------------------------------------------------------

/*!!!

void DeleteLayer()
{
    if (numlayers <= 1) return;

    if (generating) Warning("Bug detected in DeleteLayer!");

    // note that we don't need to ask to delete a clone
    if (currlayer->dirty && currlayer->cloneid == 0 && asktosave && !SaveCurrentLayer()) return;

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

    if (tilelayers && numlayers > 1) CreateTiles();

    CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void SetLayer(int index)
{
    if (currindex == index) return;
    if (index < 0 || index >= numlayers) return;

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
        Warning("You cannot switch to another layer while this script is running.");
        return;
    }

    // switch current layer to clicked tile
    SetLayer(index);

    if (inscript) {
        // update window title, viewport and status bar
        inscript = false;
        mainptr->SetPatternTitle(wxEmptyString);
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

    if (tilelayers && numlayers > 1) {
        DestroyTiles();
        CreateTiles();
    }

    CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void MoveLayerDialog()
{
    if (numlayers <= 1) return;

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
    wxString oldname = currlayer->currname;
    wxString newname;
    if ( GetString("Name Layer", "Enter a new name for the current layer:",
                   oldname, newname) &&
        !newname.IsEmpty() && oldname != newname ) {

        // inscript is false so no need to call SavePendingChanges
        // if (allowundo) SavePendingChanges();

        // show new name in main window's title bar;
        // also sets currlayer->currname and updates menu item
        mainptr->SetPatternTitle(newname);

        if (allowundo) {
            // note that startfile and savestart/dirty flags don't change here
            currlayer->undoredo->RememberNameChange(oldname, currlayer->startfile,
                                                    currlayer->savestart, currlayer->dirty);
        }
    }
}

!!!*/

// -----------------------------------------------------------------------------

void DeleteOtherLayers()
{
    if (inscript || numlayers <= 1) return;

    /*!!!
    if (asktosave) {
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
                    if (!SaveCurrentLayer()) {
                        // user hit Cancel so restore current layer and generating flag
                        SetLayer(oldindex);
                        mainptr->generating = oldgen;
                        mainptr->UpdateUserInterface(mainptr->IsActive());
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
    mainptr->SetPatternTitle(wxEmptyString);

    // select LAYER_0 button (also deselects old button)
    layerbarptr->SelectButton(LAYER_0, true);

    mainptr->UpdateMenuItems(mainptr->IsActive());
    mainptr->UpdatePatternAndStatus();
    */
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
        SetPatternTitle(currlayer->currname.c_str());

        if (currlayer->cloneid > 0) {
            // synchronize other clones
            for ( int i = 0; i < numlayers; i++ ) {
                Layer* cloneptr = layer[i];
                if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                    // set dirty flag and display asterisk in layer item
                    cloneptr->dirty = true;
                    //!!! UpdateLayerItem(i);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void MarkLayerClean(const char* title)
{
    currlayer->dirty = false;

    // if script is resetting dirty flag -- eg. via new() -- then don't allow
    // dirty flag to be set true for the remainder of the script; this is
    // nicer for scripts that construct a pattern (ie. running such a script
    // is equivalent to loading a pattern file)
    if (inscript) currlayer->stayclean = true;

    if (title[0] == 0) {
        // pass in currname so UpdateLayerItem(currindex) gets called
        SetPatternTitle(currlayer->currname.c_str());
    } else {
        // set currlayer->currname to title and call UpdateLayerItem(currindex)
        SetPatternTitle(title);
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

                // remove asterisk from layer name
                //!!! UpdateLayerItem(i);
            }
        }
    }
}

// -----------------------------------------------------------------------------

/*!!!

void ToggleSyncViews()
{
    syncviews = !syncviews;

    mainptr->UpdateUserInterface(mainptr->IsActive());
    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleSyncModes()
{
    syncmodes = !syncmodes;

    mainptr->UpdateUserInterface(mainptr->IsActive());
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

    mainptr->UpdateUserInterface(mainptr->IsActive());
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

    mainptr->UpdateUserInterface(mainptr->IsActive());
    if (inscript) {
        // always update viewport and status bar
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = true;
    } else {
        mainptr->UpdatePatternAndStatus();
    }
}

!!!*/

// -----------------------------------------------------------------------------

static FILE* FindRuleFile(const std::string& rulename)
{
    const std::string extn = ".rule";
    std::string path;
    FILE* f;
    
    // first look for rulename.rule in tempdir
    path = tempdir + rulename;
    path += extn;
    f = fopen(path.c_str(), "r");
    if (f) return f;

    // now look for rulename.rule in userrules
    path = userrules + rulename;
    path += extn;
    f = fopen(path.c_str(), "r");
    if (f) return f;

    // now look for rulename.rule in rulesdir
    path = rulesdir + rulename;
    path += extn;
    return fopen(path.c_str(), "r");
}

// -----------------------------------------------------------------------------

static void CheckRuleHeader(char* linebuf, const std::string& rulename)
{
    // check that 1st line of rulename.rule file contains "@RULE rulename"
    // where rulename must match the file name exactly (to avoid problems on
    // case-sensitive file systems)
    if (strncmp(linebuf, "@RULE ", 6) != 0) {
        std::string msg = "The first line in ";
        msg += rulename;
        msg += ".rule does not start with @RULE.";
        Warning(msg.c_str());
    } else if (strcmp(linebuf+6, rulename.c_str()) != 0) {
        std::string msg = "The rule name on the first line in ";
        msg += rulename;
        msg += ".rule does not match the specified rule: ";
        msg += rulename;
        Warning(msg.c_str());
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
            SetColor(currlayer->fromrgb, r1, g1, b1);
            SetColor(currlayer->torgb, r2, g2, b2);
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
                currlayer->statenames[state] = linebuf+namepos;
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

static void CopyBuiltinIcons(gBitmapPtr* i7x7, gBitmapPtr* i15x15, gBitmapPtr* i31x31)
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

static void ParseIcons(const std::string& rulename, linereader& reader, char* linebuf, int MAXLINELEN,
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
                        wd > 0 && ht > 0 && numcolors > 0 && ht % wd == 0 &&
                        chars_per_pixel > 0 && chars_per_pixel < 3) {
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
                            Warning("Failed to allocate memory for XPM icon data!");
                            *eof = true;
                            return;
                        }
                    } else {
                        char s[128];
                        sprintf(s, "The XPM header string on line %d in ", *linenum);
                        std::string msg(s);
                        msg += rulename;
                        msg += ".rule is incorrect";
                        if (wd > 0 && ht > 0 && ht % wd != 0)
                            msg += " (height must be a multiple of width).";
                        else if (chars_per_pixel < 1 || chars_per_pixel > 2)
                            msg += " (chars_per_pixel must be 1 or 2).";
                        else
                            msg += " (4 positive integers are required).";
                        Warning(msg.c_str());
                        *eof = true;
                        return;
                    }
                }

                // copy data inside "..." to next string in xpmdata
                int len = (int)strlen(linebuf);
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
                        char s[128];
                        sprintf(s, "The XPM data string on line %d in ", *linenum);
                        std::string msg(s);
                        msg += rulename;
                        msg += ".rule is incorrect.";
                        Warning(msg.c_str());
                        *eof = true;
                        return;
                    }
                    colormap[pixel] = xpmstrings;
                } else if (xpmstrings > numcolors) {
                    // check length of string containing pixel data
                    if (len != wd * chars_per_pixel) {
                        DeleteXPMData(xpmdata, maxstrings);
                        char s[128];
                        sprintf(s, "The XPM data string on line %d in ", *linenum);
                        std::string msg(s);
                        msg += rulename;
                        msg += ".rule has the wrong length.";
                        Warning(msg.c_str());
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
                            char s[128];
                            sprintf(s, "The XPM data string on line %d in ", *linenum);
                            std::string msg(s);
                            msg += rulename;
                            msg += ".rule has an unknown pixel: ";
                            msg += pixel;
                            Warning(msg.c_str());
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
                    Warning("Failed to allocate memory for XPM string!");
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
                char s[128];
                sprintf(s, "The triangular icons specified on line %d in ", *linenum);
                std::string msg(s);
                msg += rulename;
                msg += ".rule can only be used with a 4-state rule.";
                Warning(msg.c_str());
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
        char s[128];
        sprintf(s, "The XPM icon data starting on line %d in ", xpmstarted);
        std::string msg(s);
        msg += rulename;
        msg += ".rule does not have enough strings.";
        Warning(msg.c_str());
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

static void LoadRuleInfo(FILE* rulefile, const std::string& rulename,
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
            // ParseColors/ParseIcons has stopped at next section
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

void CreateColorGradient()
{
    int maxstate = currlayer->algo->NumCellStates() - 1;
    unsigned char r1 = currlayer->fromrgb.r;
    unsigned char g1 = currlayer->fromrgb.g;
    unsigned char b1 = currlayer->fromrgb.b;
    unsigned char r2 = currlayer->torgb.r;
    unsigned char g2 = currlayer->torgb.g;
    unsigned char b2 = currlayer->torgb.b;

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

void UpdateCurrentColors()
{
    // set current layer's colors, icons and state names according to current algo and rule
    AlgoData* ad = algoinfo[currlayer->algtype];
    int maxstate = currlayer->algo->NumCellStates() - 1;

    std::string rulename = currlayer->algo->getrule();
    // replace any '\' and '/' chars with underscores;
    // ie. given 12/34/6 we look for 12_34_6.{colors|icons}
    std::replace(rulename.begin(), rulename.end(), '\\', '_');
    std::replace(rulename.begin(), rulename.end(), '/', '_');

    // strip off any suffix like ":T100,200" used to specify a bounded grid
    size_t colonpos = rulename.find(':');
    if (colonpos != std::string::npos) rulename = rulename.substr(0, colonpos);

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
    currlayer->statenames.clear();
    for (int n = 0; n <= maxstate; n++) currlayer->statenames.push_back("");

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
            size_t hyphenpos = rulename.rfind('-');
            if (hyphenpos != std::string::npos && rulename.rfind("-shared") == std::string::npos) {
                rulename = rulename.substr(0, hyphenpos) + "-shared";
                rulefile = FindRuleFile(rulename);
                if (rulefile) {
                    LoadRuleInfo(rulefile, rulename, &loadedcolors, &loadedicons, &loadednames);
                }
            }
        }
    }
    
    if (!loadedicons) UseDefaultIcons(maxstate);

    // use the smallest icons to check if they are multi-color
    if (currlayer->icons7x7) {
        for (int n = 1; n <= maxstate; n++) {
            if (MultiColorImage(currlayer->icons7x7[n])) {
                currlayer->multicoloricons = true;
                break;
            }
        }
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
        Warning("Bad index in GetLayer!");
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
    Warning("Bug in GetUniqueCloneID!");
    return 1;
}

// -----------------------------------------------------------------------------

Layer::Layer()
{
    if (!cloning) {
        // use a unique temporary file for saving starting patterns
        tempstart = CreateTempFileName("golly_start_");
    }

    dirty = false;              // user has not modified pattern
    savedirty = false;          // in case script created layer
    stayclean = inscript;       // if true then keep the dirty flag false
                                // for the duration of the script
    savestart = false;          // no need to save starting pattern
    startgen = 0;               // initial starting generation
    currname = "untitled";      // initial window title
    currfile.clear();           // no pattern file has been loaded
    startfile.clear();          // no starting pattern yet
    originx = 0;                // no X origin offset
    originy = 0;                // no Y origin offset

    icons7x7 = NULL;            // no 7x7 icons
    icons15x15 = NULL;          // no 15x15 icons
    icons31x31 = NULL;          // no 31x31 icons

    atlas7x7 = NULL;            // no texture atlas for 7x7 icons
    atlas15x15 = NULL;          // no texture atlas for 15x15 icons
    atlas31x31 = NULL;          // no texture atlas for 31x31 icons
    
    statenames.clear();         // no names for cell states

    currframe = 0;              // first frame in timeline
    autoplay = 0;               // not playing
    tlspeed = 0;                // default speed for autoplay

    // create viewport; the initial size is not important because it will soon change
    view = new viewport(100,100);
    if (view == NULL) Fatal("Failed to create viewport!");

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
        rule.clear();

        // initialize undo/redo history
        undoredo = new UndoRedo();
        if (undoredo == NULL) Fatal("Failed to create new undo/redo object!");

        touchmode = drawmode;
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
            if (undoredo == NULL) Fatal("Failed to create new undo/redo object!");
        }

        // inherit current rule
        rule = currlayer->algo->getrule();

        // inherit current viewport's size, scale and location
        view->resize( currlayer->view->getwidth(), currlayer->view->getheight() );
        view->setpositionmag( currlayer->view->x, currlayer->view->y,
                             currlayer->view->getmag() );

        // inherit current touch mode and drawing state
        touchmode = currlayer->touchmode;
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
            startname = currlayer->startname;
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
                if ( OutsideLimits(top, left, bottom, right) ) {
                    Warning("Pattern is too big to duplicate.");
                } else {
                    CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                             currlayer->algo, algo, false, "Duplicating layer");
                }
            }

            // tempstart file must remain unique in duplicate layer
            if ( FileExists(currlayer->tempstart) ) {
                if ( !CopyFile(currlayer->tempstart, tempstart) ) {
                    Warning("Could not copy tempstart file!");
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
        if (FileExists(tempstart)) RemoveFile(tempstart);

        // delete any icons
        DeleteIcons(this);
        statenames.clear();
    }
}

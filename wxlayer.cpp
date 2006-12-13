                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com

                        / ***/

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#if wxUSE_TOOLTIPS
   #include "wx/tooltip.h" // for wxToolTip
#endif

#include "wx/numdlg.h"     // for wxGetNumberFromUser
#include "wx/dcbuffer.h"   // for wxBufferedPaintDC

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, bigview, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, FillRect
#include "wxprefs.h"       // for gollydir, inithash, initrule, etc
#include "wxscript.h"      // for inscript
#include "wxlayer.h"

// -----------------------------------------------------------------------------

int numlayers = 0;               // number of existing layers
int numclones = 0;               // number of cloned layers
int currindex = -1;              // index of current layer

Layer* currlayer;                // pointer to current layer
Layer* layer[maxlayers];         // array of layers

bool suffavail[maxlayers];       // for setting unique tempstart suffix
bool cloneavail[maxlayers/2];    // for setting unique cloneid

bool cloning = false;            // adding a cloned layer?
bool duplicating = false;        // adding a duplicated layer?

bool oldhash;                    // hash setting in old layer
wxString oldrule;                // rule string in old layer
int oldmag;                      // scale in old layer
bigint oldx;                     // X position in old layer
bigint oldy;                     // Y position in old layer
wxCursor* oldcurs;               // cursor mode in old layer

// ids for bitmap buttons in layer bar;
// also used as indices for bitbutt/normbitmap/toggbitmap arrays
enum {
   ADD_LAYER = 0,
   DELETE_LAYER,
   STACK_LAYERS,
   TILE_LAYERS,
   LAYER_0,
   LAYER_LAST = LAYER_0 + maxlayers - 1
};

// array of bitmap buttons (set in LayerBar::AddButton)
wxBitmapButton* bitbutt[LAYER_LAST + 1];

// array of bitmaps for normal or toggled state (set in LayerBar::AddButton)
wxBitmap* normbitmap[LAYER_LAST + 1];
wxBitmap* toggbitmap[LAYER_LAST + 1];

int toggid = -1;                 // id of currently toggled layer button

const int BUTTON_WD = 24;        // nominal width of bitmap buttons
const int BITMAP_WD = 16;        // width of bitmaps
const int BITMAP_HT = 16;        // height of bitmaps

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
   
   if (tileframewd > 0) {
      // make tilerects smaller to allow for equal-width tile borders
      for ( int i = 0; i < rows; i++ ) {
         for ( int j = 0; j < cols; j++ ) {
            int index = i * cols + j;
            if (index == numlayers) {
               // numlayers == 3,5,7
               layer[index - 1]->tilerect.width -= tileframewd;
            } else {
               layer[index]->tilerect.x += tileframewd;
               layer[index]->tilerect.y += tileframewd;
               layer[index]->tilerect.width -= tileframewd;
               layer[index]->tilerect.height -= tileframewd;
               if (j == cols - 1) layer[index]->tilerect.width -= tileframewd;
               if (i == rows - 1) layer[index]->tilerect.height -= tileframewd;
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
      layer[i]->tilewin->SetSize( layer[i]->tilerect );
   }
   
   // set viewport size for each tile; this is currently the same as the
   // tilerect size because tile windows are created with wxNO_BORDER
   for ( int i = 0; i < numlayers; i++ ) {
      int wd, ht;
      layer[i]->tilewin->GetClientSize(&wd, &ht);
      // wd or ht might be < 1 on Win/X11 platforms
      if (wd < 1) wd = 1;
      if (ht < 1) ht = 1;
      layer[i]->view->resize(wd, ht);
   }
}

// -----------------------------------------------------------------------------

void ResizeLayers(int wd, int ht)
{
   // this is called whenever the size of the bigview window changes;
   // wd and ht are the dimens of bigview's client area
   if (tilelayers && numlayers > 1) {
      ResizeTiles(wd, ht);
   } else {
      // resize viewport in each layer to bigview's client area
      for (int i = 0; i < numlayers; i++)
         layer[i]->view->resize(wd, ht);
   }
}

// -----------------------------------------------------------------------------

void CreateTiles()
{
   //!!! debug
   if (viewptr != bigview) Fatal(_("Bug in CreateTiles!"));
   
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
   }
   
   // init tilerects, tile window sizes and their viewport sizes
   int wd, ht;
   bigview->GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
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
   //!!! debug
   if (viewptr == bigview) Fatal(_("Bug in DestroyTiles!"));

   // reset viewptr to main viewport window
   viewptr = bigview;
   if (mainptr->IsActive()) viewptr->SetFocus();

   // destroy all tile windows
   for ( int i = 0; i < numlayers; i++ )
      delete layer[i]->tilewin;

   // resize viewport in each layer to bigview's client area
   int wd, ht;
   bigview->GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   for ( int i = 0; i < numlayers; i++ )
      layer[i]->view->resize(wd, ht);
}

// -----------------------------------------------------------------------------

void UpdateView()
{
   if (tilelayers && numlayers > 1) {
      //!!! make this more efficient??? ie. in many cases we only need to
      // update the currently selected tile
      
      // update tile borders and all tiles (child windows of bigview)
      bigview->Refresh(false);
      //!!! only need to refresh bigview upon creating/resizing???
      /*
      // update all tile windows
      for ( int i = 0; i < numlayers; i++ ) {
         layer[i]->tilewin->Refresh(false);
         layer[i]->tilewin->Update();
      }
      */
      bigview->Update();
      
   } else {
      // update main viewport window
      viewptr->Refresh(false);
      viewptr->Update();
   }
}

// -----------------------------------------------------------------------------

void RefreshView()
{
   if (tilelayers && numlayers > 1) {
      // refresh tile borders and all tiles (child windows of bigview)
      bigview->Refresh(false);
   } else {
      // refresh main viewport window
      viewptr->Refresh(false);
   }
}

// -----------------------------------------------------------------------------

void SelectButton(int id, bool select)
{
   if (select && id >= LAYER_0 && id <= LAYER_LAST) {
      if (toggid >= LAYER_0) {
         // deselect old layer button
         bitbutt[toggid]->SetBitmapLabel(*normbitmap[toggid]);
         if (showlayer) bitbutt[toggid]->Refresh(false);
      }
      toggid = id;
   }
   
   if (select) {
      bitbutt[id]->SetBitmapLabel(*toggbitmap[id]);
   } else {
      bitbutt[id]->SetBitmapLabel(*normbitmap[id]);
   }
   if (showlayer) bitbutt[id]->Refresh(false);
}

// -----------------------------------------------------------------------------

void SyncClones()
{
   //!!! debug
   if (numclones < 0) Fatal(_("Bug in SyncClones!"));
   
   if (numclones == 0) return;
   
   if (currlayer->cloneid > 0) {
      // make sure clone algo and most other settings are synchronized
      for ( int i = 0; i < numlayers; i++ ) {
         Layer* cloneptr = layer[i];
         if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
            // universe might have been re-created, or hashing changed
            cloneptr->algo = currlayer->algo;
            cloneptr->hash = currlayer->hash;
            cloneptr->rule = currlayer->rule;

            // don't sync curs or currname
            // cloneptr->curs = currlayer->curs;
            // cloneptr->currname = currlayer->currname;
            
            // sync speed and origin offset
            cloneptr->warp = currlayer->warp;
            cloneptr->originx = currlayer->originx;
            cloneptr->originy = currlayer->originy;
            
            // sync selection
            cloneptr->seltop = currlayer->seltop;
            cloneptr->selbottom = currlayer->selbottom;
            cloneptr->selleft = currlayer->selleft;
            cloneptr->selright = currlayer->selright;
   
            // sync the stuff needed to reset pattern
            cloneptr->starthash = currlayer->starthash;
            cloneptr->startrule = currlayer->startrule;
            cloneptr->startx = currlayer->startx;
            cloneptr->starty = currlayer->starty;
            cloneptr->startwarp = currlayer->startwarp;
            cloneptr->startmag = currlayer->startmag;
            cloneptr->savestart = currlayer->savestart;
            cloneptr->startfile = currlayer->startfile;
            cloneptr->startgen = currlayer->startgen;
            cloneptr->currfile = currlayer->currfile;
         }
      }
   }
}

// -----------------------------------------------------------------------------

void SaveLayerSettings()
{
   // a good place to synchronize clone info
   SyncClones();

   // set oldhash and oldrule for use in CurrentLayerChanged
   oldhash = currlayer->hash;
   oldrule = wxString(global_liferules.getrule(), wxConvLocal);
   
   // we're about to change layer so remember current rule
   // in case we switch back to this layer
   currlayer->rule = oldrule;
   
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

void CurrentLayerChanged()
{
   // currlayer has changed since SaveLayerSettings was called;
   // need to update global rule table if the new currlayer has a
   // different hash setting or different rule
   if ( currlayer->hash != oldhash || !currlayer->rule.IsSameAs(oldrule,false) ) {
      currlayer->algo->setrule( currlayer->rule.mb_str(wxConvLocal) );
   }
   
   if (syncviews) currlayer->view->setpositionmag(oldx, oldy, oldmag);
   if (synccursors) currlayer->curs = oldcurs;

   // select current layer button (also deselects old button)
   SelectButton(LAYER_0 + currindex, true);
   
   if (tilelayers && numlayers > 1) {
      // switch to new tile
      viewptr = currlayer->tilewin;
      if (mainptr->IsActive()) viewptr->SetFocus();
   }

   mainptr->SetWarp(currlayer->warp);
   mainptr->SetWindowTitle(currlayer->currname);

   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void AddLayer()
{
   if (mainptr && mainptr->generating) return;
   if (numlayers >= maxlayers) return;
   
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

   currlayer = new Layer();
   layer[currindex] = currlayer;
   
   numlayers++;

   if (numlayers > 1) {
      // add bitmap button at end of layer bar
      bitbutt[LAYER_0 + numlayers - 1]->Show(true);
      
      // add another item at end of Layer menu
      mainptr->AppendLayerItem();

      // update names in any items after currindex
      for (int i = currindex + 1; i < numlayers; i++)
         mainptr->UpdateLayerItem(i);
      
      if (tilelayers && numlayers > 1) CreateTiles();
      
      CurrentLayerChanged();
   }
}

// -----------------------------------------------------------------------------

void CloneLayer()
{
   cloning = true;
   AddLayer();
   cloning = false;
}

// -----------------------------------------------------------------------------

void DuplicateLayer()
{
   duplicating = true;
   AddLayer();
   duplicating = false;
}

// -----------------------------------------------------------------------------

void DeleteLayer()
{
   if (mainptr->generating || numlayers <= 1) return;

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

   // remove bitmap button at end of layer bar
   bitbutt[LAYER_0 + numlayers]->Show(false);
      
   // remove item from end of Layer menu
   mainptr->RemoveLayerItem();

   // update names in any items after currindex
   for (int i = currindex + 1; i < numlayers; i++)
      mainptr->UpdateLayerItem(i);
      
   if (tilelayers && numlayers > 1) CreateTiles();
   
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void DeleteOtherLayers()
{
   if (inscript || numlayers <= 1) return;

   // numlayers > 1
   if (tilelayers) DestroyTiles();

   SyncClones();
   
   // delete all layers except current layer
   for (int i = 0; i < numlayers; i++)
      if (i != currindex) delete layer[i];

   layer[0] = layer[currindex];
   currindex = 0;
   // currlayer doesn't change

   // remove all items except layer 0
   while (numlayers > 1) {
      numlayers--;

      // remove bitmap button at end of layer bar
      bitbutt[LAYER_0 + numlayers]->Show(false);
      
      // remove item from end of Layer menu
      mainptr->RemoveLayerItem();
   }

   // update name in currindex item
   mainptr->UpdateLayerItem(currindex);

   // select LAYER_0 button (also deselects old button)
   SelectButton(LAYER_0, true);

   mainptr->UpdateMenuItems(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void SetLayer(int index)
{
   if (mainptr->generating || currindex == index) return;
   if (index < 0 || index >= numlayers) return;
   
   SaveLayerSettings();
   currindex = index;
   currlayer = layer[currindex];
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void MoveLayer(int fromindex, int toindex)
{
   if (mainptr->generating || fromindex == toindex) return;
   if (fromindex < 0 || fromindex >= numlayers) return;
   if (toindex < 0 || toindex >= numlayers) return;

   SaveLayerSettings();
   
   if (fromindex > toindex) {
      Layer* savelayer = layer[fromindex];
      for (int i = fromindex; i > toindex; i--)
         layer[i] = layer[i - 1];
      layer[toindex] = savelayer;
      
      // update item names for layers that moved
      for (int i = toindex; i <= fromindex; i++)
         mainptr->UpdateLayerItem(i);

   } else {
      // fromindex < toindex
      Layer* savelayer = layer[fromindex];
      for (int i = fromindex; i < toindex; i++)
         layer[i] = layer[i + 1];
      layer[toindex] = savelayer;
      
      // update item names for layers that moved
      for (int i = fromindex; i <= toindex; i++)
         mainptr->UpdateLayerItem(i);
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

//!!! remove this eventually if user can drag layer buttons???
void MoveLayerDialog()
{
   if (mainptr->generating || inscript || numlayers <= 1) return;
   
   long n = wxGetNumberFromUser(_("Move current layer to new position."),
                                _("Enter new index:"),
                                _("Move Layer"),
                                currindex, 0, numlayers - 1,   // default, min, max
                                wxGetActiveWindow());
                                //!!!??? calc offset from main win top left
                                //!!! wxPoint(100,100));    ignored on Mac -- try 2.8???
   
   if (n >= 0 && n < numlayers) MoveLayer(currindex, n);
}

// -----------------------------------------------------------------------------

void NameLayerDialog()
{
   if (inscript) return;

   wxTextEntryDialog dialog(wxGetActiveWindow(),
                            _("Enter a name for the current layer:"),
                            _("Name Layer"),
                            currlayer->currname,
                            #ifdef __WXMAC__
                               //!!! without this dlg appears in top left corner;
                               // ditto in dialogs sample
                               wxCENTRE |
                            #endif
                            wxOK | wxCANCEL);

   if (dialog.ShowModal() == wxID_OK) {
      wxString newname = dialog.GetValue();
      if ( !newname.IsEmpty() ) {
         // show new name in main window's title;
         // also sets currlayer->currname and updates menu item
         mainptr->SetWindowTitle(newname);
      }
   }
}

// -----------------------------------------------------------------------------

void ToggleSyncViews()
{
   syncviews = !syncviews;

   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleSyncCursors()
{
   synccursors = !synccursors;

   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleStackLayers()
{
   stacklayers = !stacklayers;
   if (stacklayers && tilelayers) {
      tilelayers = false;
      SelectButton(TILE_LAYERS, false);
      if (numlayers > 1) DestroyTiles();
   }
   SelectButton(STACK_LAYERS, stacklayers);

   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleTileLayers()
{
   tilelayers = !tilelayers;
   if (tilelayers && stacklayers) {
      stacklayers = false;
      SelectButton(STACK_LAYERS, false);
   }
   SelectButton(TILE_LAYERS, tilelayers);
   
   if (tilelayers) {
      if (numlayers > 1) CreateTiles();
   } else {
      if (numlayers > 1) DestroyTiles();
   }

   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
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
   for (int i = 1; i < maxlayers/2; i++) {
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

int GetUniqueSuffix()
{
   // find first available index to use as tempstart suffix
   for (int i = 0; i < maxlayers; i++) {
      if (suffavail[i]) {
         suffavail[i] = false;
         return i;
      }
   }
   // bug if we get here
   Warning(_("Bug in GetUniqueSuffix!"));
   return 0;
}

// -----------------------------------------------------------------------------

Layer::Layer()
{
   // set tempstart prefix (unique suffix will be added below);
   // WARNING: ~Layer() assumes prefix ends with '_'
   tempstart = gollydir + wxT(".golly_start_");

   savestart = false;            // no need to save starting pattern just yet
   startfile.Clear();            // no starting pattern
   startgen = 0;                 // initial starting generation
   currname = _("untitled");
   currfile = wxEmptyString;
   warp = 0;                     // initial speed setting
   originx = 0;                  // no X origin offset
   originy = 0;                  // no Y origin offset
   
   // no selection
   seltop = 1;
   selbottom = 0;
   selleft = 0;
   selright = 0;

   if (numlayers == 0) {
      // creating very first layer
      
      // set hash using inithash stored in prefs file
      hash = inithash;
      
      // create empty universe
      if (hash) {
         algo = new hlifealgo();
         algo->setMaxMemory(maxhashmem);
      } else {
         algo = new qlifealgo();
      }
      algo->setpoll(wxGetApp().Poller());

      // set rule using initrule stored in prefs file;
      // errors can only occur if someone has edited the prefs file
      const char *err = algo->setrule(initrule);
      if (err) {
         Warning(wxString(err,wxConvLocal));
         // user will see offending rule string in window title
      } else if (global_liferules.hasB0notS8 && hash) {
         // silently turn off hashing
         hash = false;
         delete algo;
         algo = new qlifealgo();
         algo->setpoll(wxGetApp().Poller());
         algo->setrule(initrule);
      }
   
      // don't need to remember rule here (SaveLayerSettings will do it)
      rule = wxEmptyString;
      
      // create viewport; the initial size is not important because
      // ResizeLayers will soon be called
      view = new viewport(100,100);
      
      // set cursor in case newcurs/opencurs are set to "No Change"
      curs = curs_pencil;
      
      // add suffix to tempstart and initialize suffavail array
      tempstart += wxT("0");
      suffavail[0] = false;
      for (int i = 1; i < maxlayers; i++) suffavail[i] = true;
      
      // first layer can't be a clone
      cloneid = 0;
      
      // initialize cloneavail array (cloneavail[0] is never used)
      cloneavail[0] = false;
      for (int i = 1; i < maxlayers/2; i++) cloneavail[i] = true;

   } else {
      // adding a new layer after currlayer (see AddLayer)

      // inherit current universe type
      hash = currlayer->hash;
      
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

         // clones share the same universe
         algo = currlayer->algo;

         // clones use same name for starting file
         tempstart = currlayer->tempstart;

      } else {
         // this layer isn't a clone
         cloneid = 0;
         
         // create empty universe
         if (hash) {
            algo = new hlifealgo();
            algo->setMaxMemory(maxhashmem);
         } else {
            algo = new qlifealgo();
         }
         algo->setpoll(wxGetApp().Poller());

         // add unique suffix to tempstart
         tempstart += wxString::Format("%d", GetUniqueSuffix());
      }
      
      // inherit current rule in global_liferules (NOT in currlayer->rule)
      rule = wxString(global_liferules.getrule(), wxConvLocal);
      
      // inherit current viewport's size, scale and location
      view = new viewport(100,100);
      view->resize( currlayer->view->getwidth(),
                    currlayer->view->getheight() );
      view->setpositionmag( currlayer->view->x, currlayer->view->y,
                            currlayer->view->getmag() );
      
      // inherit current cursor
      curs = currlayer->curs;

      if (cloning || duplicating) {
         // duplicate all the other current settings
         currname = currlayer->currname;
         warp = currlayer->warp;
         originx = currlayer->originx;
         originy = currlayer->originy;
         
         // duplicate selection
         seltop = currlayer->seltop;
         selbottom = currlayer->selbottom;
         selleft = currlayer->selleft;
         selright = currlayer->selright;

         // we'll even duplicate the stuff needed to reset pattern
         starthash = currlayer->starthash;
         startrule = currlayer->startrule;
         startx = currlayer->startx;
         starty = currlayer->starty;
         startwarp = currlayer->startwarp;
         startmag = currlayer->startmag;
         savestart = currlayer->savestart;
         startfile = currlayer->startfile;
         startgen = currlayer->startgen;
         currfile = currlayer->currfile;
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
         
         // tempstart must remain unique
         if (currlayer->startfile == currlayer->tempstart)
            startfile = tempstart;
         
         // if currlayer->tempstart exists then copy it to this layer's unique tempstart
         if ( wxFileExists(currlayer->tempstart) ) {
            if ( wxCopyFile(currlayer->tempstart, tempstart, true) ) {
               if (currlayer->currfile == currlayer->tempstart)
                  currfile = tempstart;   // starting pattern came from clipboard
            } else {
               Warning(_("Could not copy tempstart file!"));
            }
         }
      }
   }
}

// -----------------------------------------------------------------------------

Layer::~Layer()
{
   if (view) delete view;

   if (cloneid > 0) {
      // count how many layers have the same cloneid
      int clonecount = 0;
      for (int i = 0; i < numlayers; i++) {
         if (layer[i]->cloneid == cloneid) clonecount++;
      }
      if (clonecount > 2) {
         // only delete this clone
         numclones--;
      } else {
         // reset all (two) cloneids to 0
         for (int i = 0; i < numlayers; i++) {
            if (layer[i]->cloneid == cloneid) {
               layer[i]->cloneid = 0;
               numclones--;
            }
         }
         if (clonecount < 2 || numclones < 0) {
            Warning(_("Bug detected deleting clone!"));
         }
         // make this cloneid available for the next clone
         cloneavail[cloneid] = true;
      }
      
   } else {
      // not a clone so safe to delete algo and tempstart file
      if (algo) delete algo;
      
      // delete tempstart file if it exists
      if (wxFileExists(tempstart)) wxRemoveFile(tempstart);
      
      // make tempstart suffix available for new layers
      wxString suffix = tempstart.AfterLast('_');
      long val;
      if (suffix.ToLong(&val) && val >= 0 && val < maxlayers) {
         suffavail[val] = true;
      } else {
         Warning(_("Problem with tempstart suffix: ") + tempstart);
      }
   }
}

// -----------------------------------------------------------------------------

// Define layer bar window:

class LayerBar : public wxWindow
{
public:
   LayerBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
   ~LayerBar();

   // add a bitmap button to layer bar
   void AddButton(int id, char label, int x, int y);

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnButton(wxCommandEvent& event);
   void OnEraseBackground(wxEraseEvent& event);

   void DrawLayerBar(wxDC& dc, wxRect& updaterect);

   #ifndef __WXMAC__
      wxBitmap* lbarbitmap;      // layer bar bitmap
      int lbarbitmapwd;          // width of layer bar bitmap
      int lbarbitmapht;          // height of layer bar bitmap
   #endif
};

BEGIN_EVENT_TABLE(LayerBar, wxWindow)
   EVT_PAINT            (           LayerBar::OnPaint)
   EVT_LEFT_DOWN        (           LayerBar::OnMouseDown)
   EVT_BUTTON           (wxID_ANY,  LayerBar::OnButton)
//!!!??? EVT_ERASE_BACKGROUND (           LayerBar::OnEraseBackground)
END_EVENT_TABLE()

LayerBar* layerbarptr = NULL;

// -----------------------------------------------------------------------------

LayerBar::LayerBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxWindow(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht))
{
   #ifdef __WXGTK__
      // avoid erasing background on GTK+
      SetBackgroundStyle(wxBG_STYLE_CUSTOM);
   #endif

   #ifdef __WXMSW__
      // use current theme's background colour
      SetBackgroundColour(wxNullColour);
   #endif

   #ifndef __WXMAC__
      lbarbitmap = NULL;
      lbarbitmapwd = -1;
      lbarbitmapht = -1;
   #endif
}

// -----------------------------------------------------------------------------

LayerBar::~LayerBar()
{
   #ifndef __WXMAC__
      if (lbarbitmap) delete lbarbitmap;
   #endif
}

// -----------------------------------------------------------------------------

void LayerBar::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
   // do nothing because we'll be painting the entire viewport
}

// -----------------------------------------------------------------------------

void LayerBar::DrawLayerBar(wxDC& dc, wxRect& updaterect)
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   if (wd < 1 || ht < 1 || !showlayer) return;
      
   #ifdef __WXMSW__
      // needed on Windows
      //!!!??? dc.Clear();
      wxBrush brush = dc.GetBackground();
      FillRect(dc, updaterect, brush);
   #else
      wxUnusedVar(updaterect);
   #endif
   
   // draw some border lines
   wxRect r = wxRect(0, 0, wd, ht);
   #ifdef __WXMSW__
      // draw gray line at bottom edge
      dc.SetPen(*wxGREY_PEN);
      dc.DrawLine(0, r.GetBottom(), r.width, r.GetBottom());
   #else
      // draw light gray line at bottom edge
      dc.SetPen(*wxLIGHT_GREY_PEN);
      // left edge??? dc.DrawLine(0, 0, 0, r.height);
      dc.DrawLine(0, r.GetBottom(), r.width, r.GetBottom());
   #endif
   dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void LayerBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
   #ifdef __WXMAC__
      // windows on Mac OS X are automatically buffered
      wxPaintDC dc(this);
   #else
      wxPaintDC dc(this);//!!!

      /* doesn't solve prob on Windows
      //!!!??? use wxWidgets buffering to avoid buttons flashing
      int wd, ht;
      GetClientSize(&wd, &ht);
      // wd or ht might be < 1 on Win/X11 platforms
      if (wd < 1) wd = 1;
      if (ht < 1) ht = 1;
      if (wd != lbarbitmapwd || ht != lbarbitmapht) {
         // need to create a new bitmap for layer bar
         if (lbarbitmap) delete lbarbitmap;
         lbarbitmap = new wxBitmap(wd, ht);
         lbarbitmapwd = wd;
         lbarbitmapht = ht;
      }
      if (lbarbitmap == NULL) Fatal(_("Not enough memory to render layer bar!"));
      wxBufferedPaintDC dc(this, *lbarbitmap);
      */
   #endif

   wxRect updaterect = GetUpdateRegion().GetBox();
   DrawLayerBar(dc, updaterect);
}

// -----------------------------------------------------------------------------

void LayerBar::OnMouseDown(wxMouseEvent& WXUNUSED(event))
{
   // this is NOT called if user clicks a layer bar button;
   // on Windows we need to reset keyboard focus to viewport window
   viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void LayerBar::OnButton(wxCommandEvent& event)
{
   mainptr->showbanner = false;
   statusptr->ClearMessage();

   int id = event.GetId();
   switch (id)
   {
      case ADD_LAYER:      AddLayer(); break;
      case DELETE_LAYER:   DeleteLayer(); break;
      case STACK_LAYERS:   ToggleStackLayers(); break;
      case TILE_LAYERS:    ToggleTileLayers(); break;
      default:             SetLayer(id - LAYER_0);
   }
   
   // needed on Windows to clear button focus
   viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void LayerBar::AddButton(int id, char label, int x, int y)
{
   wxMemoryDC dc;
   wxFont* font = wxFont::New(10, wxMODERN, wxFONTSTYLE_NORMAL, wxBOLD);
   wxString str;
   str.Printf(_("%c"), label);

   // create bitmap for normal button state
   normbitmap[id] = new wxBitmap(BITMAP_WD, BITMAP_HT);
   dc.SelectObject(*normbitmap[id]);
   dc.SetFont(*font);
   dc.SetTextForeground(*wxBLACK);
   dc.SetBrush(*wxBLACK_BRUSH);
   #ifndef __WXMAC__
      dc.Clear();   // needed on Windows and Linux
   #endif
   dc.SetBackgroundMode(wxTRANSPARENT);
   #ifdef __WXMAC__
      dc.DrawText(str, 3, 2);
   #else
      dc.DrawText(str, 4, 0);
   #endif
   dc.SelectObject(wxNullBitmap);
   
   // create bitmap for toggled state
   toggbitmap[id] = new wxBitmap(BITMAP_WD, BITMAP_HT);
   dc.SelectObject(*toggbitmap[id]);
   wxRect r = wxRect(0, 0, BITMAP_WD, BITMAP_HT);
   wxColor gray(64,64,64);
   wxBrush brush(gray);
   FillRect(dc, r, brush);
   dc.SetFont(*font);
   dc.SetTextForeground(*wxWHITE);
   dc.SetBrush(*wxWHITE_BRUSH);
   dc.SetBackgroundMode(wxTRANSPARENT);
   #ifdef __WXMAC__
      dc.DrawText(str, 5, 1);             //!!! why diff to above???
   #else
      dc.DrawText(str, 4, 0);
   #endif
   dc.SelectObject(wxNullBitmap);
   
   bitbutt[id] = new wxBitmapButton(this, id, *normbitmap[id], wxPoint(x,y));
   if (bitbutt[id] == NULL) Fatal(_("Failed to create layer button!"));
}

// -----------------------------------------------------------------------------

void CreateLayerBar(wxWindow* parent)
{
   int wd, ht;
   parent->GetClientSize(&wd, &ht);

   layerbarptr = new LayerBar(parent, 0, 0, wd, layerbarht);
   if (layerbarptr == NULL) Fatal(_("Failed to create layer bar!"));

   // create bitmap buttons
   int x = 4;
   #ifdef __WXGTK__
      int y = 3;
      int sgap = 6;
   #else
      int y = 4;
      int sgap = 4;
   #endif
   int bgap = 16;
   layerbarptr->AddButton(ADD_LAYER,    '+', x, y);   x += BUTTON_WD + sgap;
   layerbarptr->AddButton(DELETE_LAYER, '-', x, y);   x += BUTTON_WD + bgap;
   layerbarptr->AddButton(STACK_LAYERS, 'S', x, y);   x += BUTTON_WD + sgap;
   layerbarptr->AddButton(TILE_LAYERS,  'T', x, y);   x += BUTTON_WD + bgap;
   for (int i = 0; i < maxlayers; i++) {
      layerbarptr->AddButton(LAYER_0 + i, '0' + i, x, y);
      x += BUTTON_WD + sgap;
   }

   // add tool tips to buttons
   bitbutt[ADD_LAYER]->SetToolTip(_("Add new layer"));
   bitbutt[DELETE_LAYER]->SetToolTip(_("Delete current layer"));
   bitbutt[STACK_LAYERS]->SetToolTip(_("Toggle stacked layers"));
   bitbutt[TILE_LAYERS]->SetToolTip(_("Toggle tiled layers"));
   for (int i = 0; i < maxlayers; i++) {
      wxString tip;
      tip.Printf(_("Switch to layer %d"), i);
      bitbutt[LAYER_0 + i]->SetToolTip(tip);
   }
   #if wxUSE_TOOLTIPS
      wxToolTip::Enable(showtips);  // fix wxGTK bug
   #endif
  
   // hide all layer buttons except layer 0
   for (int i = 1; i < maxlayers; i++) {
      bitbutt[LAYER_0 + i]->Show(false);
   }
   
   // select STACK_LAYERS or TILE_LAYERS if necessary
   if (stacklayers) SelectButton(STACK_LAYERS, true);
   if (tilelayers) SelectButton(TILE_LAYERS, true);
   
   // select LAYER_0 button
   SelectButton(LAYER_0, true);
   
   // disable DELETE_LAYER button
   bitbutt[DELETE_LAYER]->Enable(false);
      
   layerbarptr->Show(showlayer);    // needed on Windows
}

// -----------------------------------------------------------------------------

void ResizeLayerBar(int wd)
{
   if (layerbarptr) {
      layerbarptr->SetSize(wd, layerbarht);
   }
}

// -----------------------------------------------------------------------------

void UpdateLayerBar(bool active)
{
   if (layerbarptr && showlayer) {
      if (viewptr->waitingforclick) active = false;
      bool busy = mainptr->generating || inscript;
      
      bitbutt[ADD_LAYER]->Enable(active && !busy && numlayers < maxlayers);
      bitbutt[DELETE_LAYER]->Enable(active && !busy && numlayers > 1);
      bitbutt[STACK_LAYERS]->Enable(active);
      bitbutt[TILE_LAYERS]->Enable(active);
      for (int i = LAYER_0; i < LAYER_0 + numlayers; i++)
         bitbutt[i]->Enable(active && !busy);
   }
}

// -----------------------------------------------------------------------------

void ToggleLayerBar()
{
   showlayer = !showlayer;
   wxRect r = bigview->GetRect();
   
   if (showlayer) {
      // show layer bar at top of viewport window
      r.y += layerbarht;
      r.height -= layerbarht;
   } else {
      // hide layer bar
      r.y -= layerbarht;
      r.height += layerbarht;
   }
   
   bigview->SetSize(r);
   layerbarptr->Show(showlayer);    // needed on Windows
}

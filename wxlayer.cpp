                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.

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
#include "wx/rawbmp.h"     // for wxAlphaPixelData
#include "wx/filename.h"   // for wxFileName
#include "wx/colordlg.h"   // for wxColourDialog
#include "wx/tglbtn.h"     // for wxToggleButton

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

#ifdef __WXMSW__
   // bitmaps are loaded via .rc file
#else
   // bitmaps for some layer bar buttons
   #include "bitmaps/add.xpm"
   #include "bitmaps/clone.xpm"
   #include "bitmaps/duplicate.xpm"
   #include "bitmaps/delete.xpm"
   #include "bitmaps/stack.xpm"
   #include "bitmaps/stack_down.xpm"
   #include "bitmaps/tile.xpm"
   #include "bitmaps/tile_down.xpm"
#endif

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
   void OnMouseMotion(wxMouseEvent& event);
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
#elif defined(__WXOSX_COCOA__)
   const int TOGGLE_HT = 24;
#else
   const int TOGGLE_HT = 20;
#endif

// width and height of bitmap buttons
#if defined(__WXOSX_COCOA__) || defined(__WXGTK__)
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
             wxNO_FULL_REPAINT_ON_RESIZE)
{
   #ifdef __WXGTK__
      // avoid erasing background on GTK+
      SetBackgroundStyle(wxBG_STYLE_CUSTOM);
   #endif

   // init bitmaps for normal state
   normbutt[ADD_LAYER] =         wxBITMAP(add);
   normbutt[CLONE_LAYER] =       wxBITMAP(clone);
   normbutt[DUPLICATE_LAYER] =   wxBITMAP(duplicate);
   normbutt[DELETE_LAYER] =      wxBITMAP(delete);
   normbutt[STACK_LAYERS] =      wxBITMAP(stack);
   normbutt[TILE_LAYERS] =       wxBITMAP(tile);
   
   // some bitmap buttons also have a down state
   downbutt[STACK_LAYERS] = wxBITMAP(stack_down);
   downbutt[TILE_LAYERS] =  wxBITMAP(tile_down);

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

      if (currbuttwd != oldbuttwd) {
         for (int i = 0; i < MAX_LAYERS; i++) {
            togglebutt[i]->SetSize(x, y, currbuttwd, TOGGLE_HT);
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

// global flag is not used at the moment (probably need later for dragging button)
static bool buttdown = false;

void LayerBar::OnButtonDown(wxMouseEvent& event)
{
   // a layer bar button has been pressed
   buttdown = true;
   
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
   buttdown = false;

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

// not used at the moment (probably need later for button dragging)
void LayerBar::OnMouseMotion(wxMouseEvent& event)
{   
   if (buttdown) {
      //???
   }
   event.Skip();
}

// -----------------------------------------------------------------------------

void LayerBar::AddButton(int id, const wxString& tip)
{
   if (id < MAX_LAYERS) {
      // create toggle button
      int y = (layerbarht - TOGGLE_HT) / 2;
      togglebutt[id] = new wxToggleButton(this, id, wxT("?"),
                                          wxPoint(xpos, y),
                                          wxSize(MIN_TOGGLE_WD, TOGGLE_HT));
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
         togglebutt[id]->SetSize(xpos, y, MAX_TOGGLE_WD, TOGGLE_HT);
         
         xpos += MAX_TOGGLE_WD;
         
         togglebutt[id]->SetToolTip(SWITCH_LAYER);
         
         #ifdef __WXMSW__
            // fix problem with layer bar buttons when generating/inscript
            // due to focus being changed to viewptr
            togglebutt[id]->Connect(id, wxEVT_LEFT_DOWN,
                                    wxMouseEventHandler(LayerBar::OnButtonDown));
            togglebutt[id]->Connect(id, wxEVT_LEFT_UP,
                                    wxMouseEventHandler(LayerBar::OnButtonUp));
            /* don't need this handler at the moment
            togglebutt[id]->Connect(id, wxEVT_MOTION,
                                    wxMouseEventHandler(LayerBar::OnMouseMotion));
            */
         #endif
      }

   } else {
      // create bitmap button
      bitmapbutt[id] = new wxBitmapButton(this, id, normbutt[id], wxPoint(xpos,ypos),
                                          wxSize(BUTTON_WD, BUTTON_HT));
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
            /* don't need this handler at the moment
            bitmapbutt[id]->Connect(id, wxEVT_MOTION,
                                    wxMouseEventHandler(LayerBar::OnMouseMotion));
            */
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
   if (layerbarptr) {
      layerbarptr->SetSize(wd, layerbarht);
   }
}

// -----------------------------------------------------------------------------

void UpdateLayerBar(bool active)
{
   if (layerbarptr && showlayer) {
      if (viewptr->waitingforclick) active = false;

      layerbarptr->EnableButton(ADD_LAYER,         active && !inscript && numlayers < MAX_LAYERS);
      layerbarptr->EnableButton(CLONE_LAYER,       active && !inscript && numlayers < MAX_LAYERS);
      layerbarptr->EnableButton(DUPLICATE_LAYER,   active && !inscript && numlayers < MAX_LAYERS);
      layerbarptr->EnableButton(DELETE_LAYER,      active && !inscript && numlayers > 1);
      layerbarptr->EnableButton(STACK_LAYERS,      active);
      layerbarptr->EnableButton(TILE_LAYERS,       active);
      for (int i = 0; i < numlayers; i++)
         layerbarptr->EnableButton(i, active && CanSwitchLayer(i));

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
   layerbarptr->Refresh(false);
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
      ShiftEditBar(layerbarht);     // move edit bar down
   } else {
      // hide layer bar
      r.y -= layerbarht;
      r.height += layerbarht;
      ShiftEditBar(-layerbarht);    // move edit bar up
   }
   
   bigview->SetSize(r);
   layerbarptr->Show(showlayer);    // needed on Windows

   mainptr->UpdateMenuItems(mainptr->IsActive());
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
            cloneptr->startfile = currlayer->startfile;
            cloneptr->startgen = currlayer->startgen;
            cloneptr->currfile = currlayer->currfile;
            cloneptr->startsel = currlayer->startsel;
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
            cloneptr->lastframe = currlayer->lastframe;
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
      // this can happen if the given rule's table/tree file was deleted
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
   mainptr->SetWindowTitle(currlayer->currname);

   mainptr->UpdateUserInterface(mainptr->IsActive());
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

static wxBitmap** CopyIcons(wxBitmap** srcicons, int size, int maxstate)
{
   wxBitmap** iconptr = (wxBitmap**) malloc(256 * sizeof(wxBitmap*));
   if (iconptr) {
      wxRect rect(0, 0, size, size);
      for (int i = 0; i < 256; i++) iconptr[i] = NULL;
      for (int i = 1; i <= maxstate; i++) {
         if (srcicons && srcicons[i])
            iconptr[i] = new wxBitmap(srcicons[i]->GetSubBitmap(rect));
      }
   }
   return iconptr;
}

// -----------------------------------------------------------------------------

void AddLayer()
{
   if (numlayers >= MAX_LAYERS) return;

   // we need to test mainptr here because AddLayer is called from main window's ctor
   if (mainptr && mainptr->generating) {
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_ADD_LAYER);
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
      for (int n = 0; n < currlayer->algo->NumCellStates(); n++) {
         currlayer->cellr[n] = oldlayer->cellr[n];
         currlayer->cellg[n] = oldlayer->cellg[n];
         currlayer->cellb[n] = oldlayer->cellb[n];
      }
      currlayer->deadbrush->SetColour( oldlayer->deadbrush->GetColour() );
      currlayer->gridpen->SetColour( oldlayer->gridpen->GetColour() );
      currlayer->boldpen->SetColour( oldlayer->boldpen->GetColour() );
      if (cloning) {
         // use same icon pointers
         currlayer->icons15x15 = oldlayer->icons15x15;
         currlayer->icons7x7 = oldlayer->icons7x7;
      } else {
         // duplicate icons from old layer
         int maxstate = currlayer->algo->NumCellStates() - 1;
         currlayer->icons15x15 = CopyIcons(oldlayer->icons15x15, 15, maxstate);
         currlayer->icons7x7 = CopyIcons(oldlayer->icons7x7, 7, maxstate);
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
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_CLONE);
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
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_DUPLICATE);
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
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_DEL_LAYER);
      return;
   }

   // note that we don't need to ask to delete a clone
   if (!inscript && currlayer->dirty && currlayer->cloneid == 0 &&
       askondelete && !mainptr->SaveCurrentLayer()) return;

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
   mainptr->SetWindowTitle(wxEmptyString);

   // select LAYER_0 button (also deselects old button)
   layerbarptr->SelectButton(LAYER_0, true);

   mainptr->UpdateMenuItems(mainptr->IsActive());
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
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_LAYER0 + index);
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

// remove this eventually if user can drag layer buttons???
void MoveLayerDialog()
{
   if (inscript || numlayers <= 1) return;

   if (mainptr->generating) {
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_MOVE_LAYER);
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
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_NAME_LAYER);
      return;
   }

   wxString oldname = currlayer->currname;
   wxString newname;
   if ( GetString(_("Name Layer"), _("Enter a new name for the current layer:"),
                  oldname, newname) &&
        !newname.IsEmpty() && oldname != newname ) {

      // inscript is false so no need to call SavePendingChanges
      // if (allowundo) SavePendingChanges();

      // show new name in main window's title bar;
      // also sets currlayer->currname and updates menu item
      mainptr->SetWindowTitle(newname);

      if (allowundo) {
         // note that currfile and savestart/dirty flags don't change here
         currlayer->undoredo->RememberNameChange(oldname, currlayer->currfile,
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

void UpdateBrushAndPens(Layer* layerptr)
{
   // update deadbrush, gridpen, boldpen in given layer
   int r = layerptr->cellr[0];
   int g = layerptr->cellg[0];
   int b = layerptr->cellb[0];
   layerptr->deadbrush->SetColour(r, g, b);

   // no need to use this standard grayscale conversion???
   // gray = (int) (0.299*r + 0.587*g + 0.114*b);
   int gray = (int) ((r + g + b) / 3.0);
   if (gray > 127) {
      // use darker grid
      layerptr->gridpen->SetColour(r > 32 ? r - 32 : 0,
                                   g > 32 ? g - 32 : 0,
                                   b > 32 ? b - 32 : 0);
      layerptr->boldpen->SetColour(r > 64 ? r - 64 : 0,
                                   g > 64 ? g - 64 : 0,
                                   b > 64 ? b - 64 : 0);
   } else {
      // use lighter grid
      layerptr->gridpen->SetColour(r + 32 < 256 ? r + 32 : 255,
                                   g + 32 < 256 ? g + 32 : 255,
                                   b + 32 < 256 ? b + 32 : 255);
      layerptr->boldpen->SetColour(r + 64 < 256 ? r + 64 : 255,
                                   g + 64 < 256 ? g + 64 : 255,
                                   b + 64 < 256 ? b + 64 : 255);
   }
}

// -----------------------------------------------------------------------------

void UpdateCloneColors()
{
   // first update deadbrush, gridpen, boldpen
   UpdateBrushAndPens(currlayer);

   if (currlayer->cloneid > 0) {
      int maxstate = currlayer->algo->NumCellStates() - 1;
      for (int i = 0; i < numlayers; i++) {
         Layer* cloneptr = layer[i];
         if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
            cloneptr->fromrgb = currlayer->fromrgb;
            cloneptr->torgb = currlayer->torgb;
            for (int n = 0; n <= maxstate; n++) {
               cloneptr->cellr[n] = currlayer->cellr[n];
               cloneptr->cellg[n] = currlayer->cellg[n];
               cloneptr->cellb[n] = currlayer->cellb[n];
            }
            
            // use same icon pointers
            cloneptr->icons15x15 = currlayer->icons15x15;
            cloneptr->icons7x7 = currlayer->icons7x7;
            
            // use same colors in deadbrush, gridpen and boldpen
            cloneptr->deadbrush->SetColour( currlayer->deadbrush->GetColour() );
            cloneptr->gridpen->SetColour( currlayer->gridpen->GetColour() );
            cloneptr->boldpen->SetColour( currlayer->boldpen->GetColour() );
         }
      }
   }
}

// -----------------------------------------------------------------------------

static FILE* FindColorFile(const wxString& rule, const wxString& dir)
{
   const wxString extn = wxT(".colors");
   wxString path;
   
   // first look for rule.colors in given directory
   path = dir + rule;
   path += extn;
   FILE* f = fopen(path.mb_str(wxConvLocal), "r");
   if (f) return f;
   
   // if rule has the form foo-* then look for foo.colors in dir;
   // this allows related rules to share a single .colors file
   wxString prefix = rule.BeforeLast('-');
   if (!prefix.IsEmpty()) {
      path = dir + prefix;
      path += extn;
      f = fopen(path.mb_str(wxConvLocal), "r");
      if (f) return f;
   }
   
   return NULL;
}

// -----------------------------------------------------------------------------

static bool LoadRuleColors(const wxString& rule, int maxstate)
{
   // if rule.colors file exists in userrules or rulesdir then
   // change colors according to info in file
   FILE* f = FindColorFile(rule, userrules);
   if (!f) f = FindColorFile(rule, rulesdir);
   if (f) {
      // the linereader class handles all line endings (CR, CR+LF, LF)
      linereader reader(f);
      // not needed here, but useful if we ever return early due to error
      // reader.setcloseonfree();
      const int MAXLINELEN = 512;
      char buf[MAXLINELEN + 1];
      while (reader.fgets(buf, MAXLINELEN) != 0) {
         if (buf[0] == '#' || buf[0] == 0) {
            // skip comment or empty line
         } else {
            // look for "color" or "gradient" keyword at start of line
            char* keyword = buf;
            char* value;
            while (*keyword == ' ') keyword++;
            value = keyword;
            while (*value >= 'a' && *value <= 'z') value++;
            while (*value == ' ' || *value == '=') value++;
            if (strncmp(keyword, "color", 5) == 0) {
               int state, r, g, b;
               if (sscanf(value, "%d%d%d%d", &state, &r, &g, &b) == 4) {
                  if (state >= 0 && state <= maxstate) {
                     currlayer->cellr[state] = r;
                     currlayer->cellg[state] = g;
                     currlayer->cellb[state] = b;
                  }
               };
            } else if (strncmp(keyword, "gradient", 8) == 0) {
               int r1, g1, b1, r2, g2, b2;
               if (sscanf(value, "%d%d%d%d%d%d", &r1, &g1, &b1, &r2, &g2, &b2) == 6) {
                  currlayer->fromrgb.Set(r1, g1, b1);
                  currlayer->torgb.Set(r2, g2, b2);
                  CreateColorGradient();
               };
            }
         }
      }
      reader.close();
      return true;
   }
   return false;
}

// -----------------------------------------------------------------------------

static bool FindIconFile(const wxString& rule, const wxString& dir, wxString& path)
{
   const wxString extn = wxT(".icons");

   // first look for rule.icons in given directory
   path = dir + rule;
   path += extn;
   if (wxFileName::FileExists(path)) return true;
   
   // if rule has the form foo-* then look for foo.icons in dir;
   // this allows related rules to share a single .icons file
   wxString prefix = rule.BeforeLast('-');
   if (!prefix.IsEmpty()) {
      path = dir + prefix;
      path += extn;
      if (wxFileName::FileExists(path)) return true;
   }
   
   return false;
}

// -----------------------------------------------------------------------------

static bool LoadRuleIcons(const wxString& rule, int maxstate)
{
   // deallocate current layer's old icons if they exist
   if (currlayer->icons15x15) {
      for (int i = 0; i < 256; i++) delete currlayer->icons15x15[i];
      free(currlayer->icons15x15);
      currlayer->icons15x15 = NULL;
   }
   if (currlayer->icons7x7) {
      for (int i = 0; i < 256; i++) delete currlayer->icons7x7[i];
      free(currlayer->icons7x7);
      currlayer->icons7x7 = NULL;
   }

   // if rule.icons file exists in userrules or rulesdir then
   // load icons for current layer
   wxString path;
   return (FindIconFile(rule, userrules, path) ||
           FindIconFile(rule, rulesdir, path)) &&
          LoadIconFile(path, maxstate, &currlayer->icons15x15, &currlayer->icons7x7);
}

// -----------------------------------------------------------------------------

void SetAverageColor(int state, wxBitmap* icon)
{
   // set non-icon color to average color of non-black pixels in given icon
   if (icon) {
      int wd = icon->GetWidth();
      int ht = icon->GetHeight();
      
      #ifdef __WXMSW__
         // must use wxNativePixelData for bitmaps with no alpha channel
         wxNativePixelData icondata(*icon);
      #else
         wxAlphaPixelData icondata(*icon);
      #endif

      if (icondata) {
         #ifdef __WXMSW__
            wxNativePixelData::Iterator iconpxl(icondata);
         #else
            wxAlphaPixelData::Iterator iconpxl(icondata);
         #endif
         
         int nbcount = 0;  // # of non-black pixels
         int totalr = 0;
         int totalg = 0;
         int totalb = 0;

         for (int i = 0; i < ht; i++) {
            #ifdef __WXMSW__
               wxNativePixelData::Iterator iconrow = iconpxl;
            #else
               wxAlphaPixelData::Iterator iconrow = iconpxl;
            #endif
            for (int j = 0; j < wd; j++) {
               if (iconpxl.Red() || iconpxl.Green() || iconpxl.Blue()) {
                  // non-black pixel
                  totalr += iconpxl.Red();
                  totalg += iconpxl.Green();
                  totalb += iconpxl.Blue();
                  nbcount++;
               }
               iconpxl++;
            }
            // move to next row of icon bitmap
            iconpxl = iconrow;
            iconpxl.OffsetY(icondata, 1);
         }
         
         if (nbcount>0) {
            currlayer->cellr[state] = int(totalr / nbcount);
            currlayer->cellg[state] = int(totalg / nbcount);
            currlayer->cellb[state] = int(totalb / nbcount);
         }
         else { // avoid div0
            currlayer->cellr[state] = 0;
            currlayer->cellg[state] = 0;
            currlayer->cellb[state] = 0;
         }
      }
   }
}

// -----------------------------------------------------------------------------

void UpdateCurrentColors()
{
   // set current layer's colors and icons according to current algo and rule
   AlgoData* ad = algoinfo[currlayer->algtype];
   int maxstate = currlayer->algo->NumCellStates() - 1;

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
      for (int n = 0; n <= maxstate; n++) {
         currlayer->cellr[n] = ad->algor[n];
         currlayer->cellg[n] = ad->algog[n];
         currlayer->cellb[n] = ad->algob[n];
      }
   }
   
   wxString rule = wxString(currlayer->algo->getrule(), wxConvLocal);
   // replace any '\' and '/' chars with underscores;
   // ie. given 12/34/6 we look for 12_34_6.{colors|icons}
   rule.Replace(wxT("\\"), wxT("_"));
   rule.Replace(wxT("/"), wxT("_"));
   
   // strip off any suffix like ":T100,200" used to specify a bounded grid
   if (rule.Find(':') >= 0) rule = rule.BeforeFirst(':');
   
   // if rule.colors file exists then override default colors
   bool loadedcolors = LoadRuleColors(rule, maxstate);
   
   // if rule.icons file exists then use those icons
   if ( !LoadRuleIcons(rule, maxstate) ) {
      if (currlayer->algo->getgridtype() == lifealgo::HEX_GRID) {
         // use hexagonal icons
         currlayer->icons15x15 = CopyIcons(hexicons15x15, 15, maxstate);
         currlayer->icons7x7 = CopyIcons(hexicons7x7, 7, maxstate);
      } else if (currlayer->algo->getgridtype() == lifealgo::VN_GRID) {
         // use diamond-shaped icons for 4-neighbor von Neumann neighborhood
         currlayer->icons15x15 = CopyIcons(vnicons15x15, 15, maxstate);
         currlayer->icons7x7 = CopyIcons(vnicons7x7, 7, maxstate);
      } else {
         // otherwise copy default icons from current algo
         currlayer->icons15x15 = CopyIcons(ad->icons15x15, 15, maxstate);
         currlayer->icons7x7 = CopyIcons(ad->icons7x7, 7, maxstate);
      }
   }   
   // if rule.colors file wasn't loaded and icons are multi-color then we
   // set non-icon colors to the average of the non-black pixels in each icon
   // (note that we use the 7x7 icons because they are faster to scan)
   wxBitmap** iconmaps = currlayer->icons7x7;
   if (!loadedcolors && iconmaps && iconmaps[1] && iconmaps[1]->GetDepth() > 1) {
      for (int n = 1; n <= maxstate; n++) {
         SetAverageColor(n, iconmaps[n]);
      }
      // if extra 15x15 icon was supplied then use it to set state 0 color
      iconmaps = currlayer->icons15x15;
      if (iconmaps && iconmaps[0]) {
         #ifdef __WXMSW__
            // must use wxNativePixelData for bitmaps with no alpha channel
            wxNativePixelData icondata(*iconmaps[0]);
         #else
            wxAlphaPixelData icondata(*iconmaps[0]);
         #endif
         if (icondata) {
            #ifdef __WXMSW__
               wxNativePixelData::Iterator iconpxl(icondata);
            #else
               wxAlphaPixelData::Iterator iconpxl(icondata);
            #endif
            // iconpxl is the top left pixel
            currlayer->cellr[0] = iconpxl.Red();
            currlayer->cellg[0] = iconpxl.Green();
            currlayer->cellb[0] = iconpxl.Blue();
         }
      }
   }
   
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

void UpdateLayerColors()
{
   UpdateCurrentColors();
   
   // if current layer has clones then update their colors
   UpdateCloneColors();
}

// -----------------------------------------------------------------------------

void InvertCellColors()
{
   // invert cell colors in all layers
   for (int i = 0; i < numlayers; i++) {
      Layer* layerptr = layer[i];
      // do NOT use layerptr->algo->... here -- it might not be correct
      // for a non-current layer (but we can use layerptr->algtype)
      for (int n = 0; n < algoinfo[layerptr->algtype]->maxstates; n++) {
         layerptr->cellr[n] = 255 - layerptr->cellr[n];
         layerptr->cellg[n] = 255 - layerptr->cellg[n];
         layerptr->cellb[n] = 255 - layerptr->cellb[n];
      }
      UpdateBrushAndPens(layerptr);
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
   startfile.Clear();            // no starting pattern
   startgen = 0;                 // initial starting generation
   currname = _("untitled");     // initial window title
   currfile.Clear();             // no pattern file has been loaded
   originx = 0;                  // no X origin offset
   originy = 0;                  // no Y origin offset
   icons15x15 = NULL;            // no 15x15 icons
   icons7x7 = NULL;              // no 7x7 icons

   currframe = 0;                // first frame in timeline
   autoplay = 0;                 // not playing
   tlspeed = 0;                  // default speed for autoplay
   lastframe = 0;                // no frame displayed

   deadbrush = new wxBrush(*wxBLACK);
   gridpen = new wxPen(*wxBLACK);
   boldpen = new wxPen(*wxBLACK);
   if (deadbrush == NULL) Fatal(_("Failed to create deadbrush!"));
   if (gridpen == NULL) Fatal(_("Failed to create gridpen!"));
   if (boldpen == NULL) Fatal(_("Failed to create boldpen!"));
   
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
         // user might have edited rule in prefs file, or deleted table/tree file
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
         lastframe = currlayer->lastframe;

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
            // table/tree file might have been deleted
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
         
         // duplicate selection info
         currsel = currlayer->currsel;
         savesel = currlayer->savesel;

         // duplicate the stuff needed to reset pattern
         savestart = currlayer->savestart;
         startalgo = currlayer->startalgo;
         startdirty = currlayer->startdirty;
         startname = currlayer->startname;
         startrule = currlayer->startrule;
         startx = currlayer->startx;
         starty = currlayer->starty;
         startbase = currlayer->startbase;
         startexpo = currlayer->startexpo;
         startmag = currlayer->startmag;
         startfile = currlayer->startfile;
         startgen = currlayer->startgen;
         currfile = currlayer->currfile;
         startsel = currlayer->startsel;
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
         if (currlayer->currfile == currlayer->tempstart) {
            // starting pattern came from clipboard or lexicon pattern
            currfile = tempstart;
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
   delete deadbrush;
   delete gridpen;
   delete boldpen;

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
      
      // delete any icons
      if (icons15x15) {
         for (int i = 0; i < 256; i++) delete icons15x15[i];
         free(icons15x15);
      }
      if (icons7x7) {
         for (int i = 0; i < 256; i++) delete icons7x7[i];
         free(icons7x7);
      }
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
            dc.DrawRectangle(r);
            dc.SetBrush(wxNullBrush);               
            DrawOneIcon(dc, r.x + 1, r.y + 1, iconmaps[state],
                        currlayer->cellr[0],
                        currlayer->cellg[0],
                        currlayer->cellb[0],
                        currlayer->cellr[state],
                        currlayer->cellg[state],
                        currlayer->cellb[state]);
         } else {
            wxColor color(currlayer->cellr[state],
                          currlayer->cellg[state],
                          currlayer->cellb[state]);
            dc.SetBrush(wxBrush(color));
            dc.DrawRectangle(r);
            dc.SetBrush(wxNullBrush);
         }

      } else {
         // state >= currlayer->algo->NumCellStates()
         dc.SetBrush(bgbrush);
         dc.DrawRectangle(r);
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

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ColorDialog, wxDialog)
   EVT_CHECKBOX   (wxID_ANY,  ColorDialog::OnCheckBoxClicked)
   EVT_BUTTON     (wxID_ANY,  ColorDialog::OnButton)
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
   stdhbox->Add(stdbutts, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxRIGHT, STDHGAP);

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
      deadbrushrgb = currlayer->deadbrush->GetColour();
      gridpenrgb = currlayer->gridpen->GetColour();
      boldpenrgb = currlayer->boldpen->GetColour();
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
      currlayer->deadbrush->SetColour(deadbrushrgb);
      currlayer->gridpen->SetColour(gridpenrgb);
      currlayer->boldpen->SetColour(boldpenrgb);
      showicons = saveshowicons;
   }
   
   // this must match color info in Layer class
   wxColor fromrgb;
   wxColor torgb;
   unsigned char cellr[256];
   unsigned char cellg[256];
   unsigned char cellb[256];
   wxColor deadbrushrgb;
   wxColor gridpenrgb;
   wxColor boldpenrgb;
   
   // we also save/restore showicons option
   bool saveshowicons;
};

// -----------------------------------------------------------------------------

void SetLayerColors()
{
   if (inscript || viewptr->waitingforclick) return;

   if (mainptr->generating) {
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_SET_COLORS);
      return;
   }
   
   bool wastoggled = swapcolors;
   if (swapcolors) viewptr->ToggleCellColors();
   
   // save current color info so we can restore it if user cancels changes
   SaveData* save_info = new SaveData();

   ColorDialog dialog( wxGetApp().GetTopWindow() );
   if ( dialog.ShowModal() != wxID_OK ) {
      // user hit Cancel so restore color info saved above
      save_info->RestoreData();
   }

   delete save_info;
   
   if (wastoggled) viewptr->ToggleCellColors();
   
   mainptr->UpdateEverything();
}

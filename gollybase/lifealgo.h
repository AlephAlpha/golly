// This file is part of Golly.
// See docs/License.html for the copyright notice.

/**
 *   This is the pure abstract class any life calculation algorithm
 *   must support.  As long as a life algorithm implements this
 *   interface, it can be invoked by our driver code.
 */
#ifndef LIFEALGO_H
#define LIFEALGO_H
#include "bigint.h"
#include "viewport.h"
#include "liferender.h"
#include "lifepoll.h"
#include "readpattern.h"
#include "platform.h"
#include <cstdio>
// moving the include vector *before* platform.h breaks compilation
#ifdef _MSC_VER
   #pragma warning(disable:4702)   // disable "unreachable code" warnings from MSVC
#endif
#include <vector>
#ifdef _MSC_VER
   #pragma warning(default:4702)   // enable "unreachable code" warnings
#endif
using std::vector;
#include <iostream>

// this must not be increased beyond 32767, because we use a bigint
// multiply that only supports multiplicands up to that size.
const int MAX_FRAME_COUNT = 32000 ;

/**
 *   Timeline support is pretty generic.
 */
class timeline_t {
public:
   timeline_t() : recording(0), framecount(0), savetimeline(1),
                  start(0), inc(0), next(0), end(0), frames() {}
   int recording, framecount, base, expo, savetimeline ;
   bigint start, inc, next, end ;
   vector<void *> frames ;
} ;

class lifealgo {
public:
   lifealgo() : generation(0), increment(0), timeline(), grid_type(SQUARE_GRID)
      {  poller = &default_poller ;
         gridwd = gridht = 0 ;      // default is an unbounded universe
         unbounded = true ;         // most algorithms use an unbounded universe
      }
   virtual ~lifealgo() ;
   // returns <0 if error
   virtual int setcell(int x, int y, int newstate) = 0 ;
   virtual int getcell(int x, int y) = 0 ;
   virtual int nextcell(int x, int y, int &v) = 0 ;
   void getcells(unsigned char *buf, int x, int y, int w, int h) ;
   // call after setcell calls
   virtual void endofpattern() = 0 ;
   virtual void setIncrement(bigint inc) = 0 ;
   virtual void setIncrement(int inc) = 0 ;
   virtual void setGeneration(bigint gen) = 0 ;
   const bigint &getIncrement() { return increment ; }
   const bigint &getGeneration() { return generation ; }
   virtual const bigint &getPopulation() = 0 ;
   virtual int isEmpty() = 0 ;
   // can we do the gen count doubling? only hashlife
   virtual int hyperCapable() = 0 ;
   virtual void setMaxMemory(int m) = 0 ;          // never alloc more than this
   virtual int getMaxMemory() = 0 ;
   virtual const char *setrule(const char *) = 0 ; // new rules; returns err msg
   virtual const char *getrule() = 0 ;             // get current rule set
   virtual void step() = 0 ;                       // do inc gens
   virtual void draw(viewport &view, liferender &renderer) = 0 ;
   virtual void fit(viewport &view, int force) = 0 ;
   virtual void findedges(bigint *t, bigint *l, bigint *b, bigint *r) = 0 ;
   virtual void lowerRightPixel(bigint &x, bigint &y, int mag) = 0 ;
   virtual const char *writeNativeFormat(std::ostream &os, char *comments) = 0 ;
   void setpoll(lifepoll *pollerarg) { poller = pollerarg ; }
   virtual const char *readmacrocell(char *) { return "Cannot read macrocell format." ; }
   
   // Verbosity crosses algorithms.  We need to embed this sort of option
   // into some global shared thing or something rather than use static.
   static void setVerbose(int v) { verbose = v ; }
   static int getVerbose() { return verbose ; }

   virtual const char* DefaultRule() { return "B3/S23"; }
   // return number of cell states in this universe (2..256)
   virtual int NumCellStates() { return 2; }
   // return number of states to use when setting random cells
   virtual int NumRandomizedCellStates() { return NumCellStates() ; }

   // timeline support
   virtual void* getcurrentstate() = 0 ;
   virtual void setcurrentstate(void *) = 0 ;
   int startrecording(int base, int expo) ;
   pair<int, int> stoprecording() ;
   pair<int, int> getbaseexpo()
                       { return make_pair(timeline.base, timeline.expo) ; }
   void extendtimeline() ;
   void pruneframes() ;
   const bigint &gettimelinestart() { return timeline.start ; }
   const bigint &gettimelineend() { return timeline.end ; }
   const bigint &gettimelineinc() { return timeline.inc ; }
   int getframecount() { return timeline.framecount ; }
   int isrecording() { return timeline.recording ; }
   int gotoframe(int i) ;
   void destroytimeline() ;
   void savetimelinewithframe(int yesno) { timeline.savetimeline = yesno ; }

   // support for a bounded universe with various topologies:
   // plane, cylinder, torus, Klein bottle, cross-surface, sphere
   unsigned int gridwd, gridht ;    // bounded universe if either is > 0
   bigint gridleft, gridright ;     // undefined if gridwd is 0
   bigint gridtop, gridbottom ;     // undefined if gridht is 0
   bool boundedplane ;              // topology is a bounded plane?
   bool sphere ;                    // topology is a sphere?
   bool htwist, vtwist ;            // Klein bottle if either is true,
                                    // or cross-surface if both are true
   int hshift, vshift ;             // torus with horizontal or vertical shift

   const char* setgridsize(const char* suffix) ;
   // use in setrule() to parse a suffix like ":T100,200" and set
   // the above parameters

   const char* canonicalsuffix() ;
   // use in setrule() to return the canonical version of suffix;
   // eg. ":t0020" would be converted to ":T20,0"

   bool CreateBorderCells() ;
   bool DeleteBorderCells() ;
   // the above routines can be called around step() to create the
   // illusion of a bounded universe (note that increment must be 1);
   // they return false if the pattern exceeds the editing limits

   bool unbounded;
   // algorithms that uses a finite universe should set this flag false
   // so the GUI code won't call CreateBorderCells or DeleteBorderCells

   vector<int> clipped_cells;
   // algorithms that uses a finite universe need to save live cells
   // that might be clipped when a setrule call reduces the size of
   // the universe (this allows the GUI code to restore the cells
   // if the rule change is undone)

   enum TGridType { SQUARE_GRID, TRI_GRID, HEX_GRID, VN_GRID } ;
   TGridType getgridtype() const { return grid_type ; }

   // for RuleLoader algo
   std::string local_file;      // name of RLE file with local @RULE data
   std::string local_rule;      // name of local @RULE
   size_t RULE_offset;          // offset in RLE file of @RULE line

protected:
   lifepoll *poller ;
   static int verbose ;
   int maxCellStates ; // keep up to date; setcell depends on it
   bigint generation ;
   bigint increment ;
   timeline_t timeline ;
   TGridType grid_type ;

private:
   // following are called by CreateBorderCells() to join edges in various ways
   void JoinTwistedEdges() ;
   void JoinTwistedAndShiftedEdges() ;
   void JoinShiftedEdges() ;
   void JoinAdjacentEdges(int pt, int pl, int pb, int pr) ;
   void JoinEdges(int pt, int pl, int pb, int pr) ;
   // following is called by DeleteBorderCells()
   void ClearRect(int top, int left, int bottom, int right) ;
} ;

/**
 *   If you need any static information from a lifealgo, this class can be
 *   called (or overridden) to set up all that data.  Right now the
 *   functions do nothing; override if you need that info.  These are
 *   called one by one by a static method in the algorithm itself,
 *   if that information is available.  The ones marked optional need
 *   not be called.
 */
class staticAlgoInfo {
public:
   staticAlgoInfo() ;
   virtual ~staticAlgoInfo() { } ;

   // mandatory
   void setAlgorithmName(const char *s) { algoName = s ; }
   void setAlgorithmCreator(lifealgo *(*f)()) { creator = f ; }
   
   // optional; override if you want to retain this data
   virtual void setDefaultBaseStep(int) {}
   virtual void setDefaultMaxMem(int) {}
   
   // minimum and maximum number of cell states supported by this algorithm;
   // both must be within 2..256
   int minstates;
   int maxstates;
   
   // default color scheme
   bool defgradient;                      // use color gradient?
   unsigned char defr1, defg1, defb1;     // color at start of gradient
   unsigned char defr2, defg2, defb2;     // color at end of gradient
   // if defgradient is false then use these colors for each cell state
   unsigned char defr[256], defg[256], defb[256];
   
   // default icon data (in XPM format)
   const char **defxpm7x7;                // 7x7 icons
   const char **defxpm15x15;              // 15x15 icons
   const char **defxpm31x31;              // 31x31 icons
   
   // basic data
   const char *algoName ;
   lifealgo *(*creator)() ;
   int id ; // my index
   staticAlgoInfo *next ;
   
   // support:  give me sequential algorithm IDs
   static int getNumAlgos() { return nextAlgoId ; }
   static int nextAlgoId ;
   static staticAlgoInfo &tick() ;
   static staticAlgoInfo *head ;
   static staticAlgoInfo *byName(const char *s) ;
   static int nameToIndex(const char *s) ;
} ;
#endif

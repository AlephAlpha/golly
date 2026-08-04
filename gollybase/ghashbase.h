// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef GHASHBASE_H
#define GHASHBASE_H
#include "lifealgo.h"
#include "liferules.h"
#include "util.h"
/*
 *   This class forms the basis of all hashlife-type algorithms except
 *   the highly-optimized hlifealgo (which is most appropriate for
 *   simple two-state automata).  This more generalized class is used
 *   for multi-state algorithms.
 */
/**
 *   The size of a state.  Unsigned char works for now.
 */
typedef unsigned char state ;
/**
 *   Nodes, like the standard hlifealgo nodes.
 */
struct ghnode {
   ghnode *next ;              /* hash link */
   ghnode *nw, *ne, *sw, *se ; /* constant; nw != 0 means nonjleaf */
   ghnode *res ;               /* cache */
} ;
/*
 *   Leaves, like the standard hlifealgo leaves.
 */
struct ghleaf {
   ghnode *next ;              /* hash link */
   ghnode *isghnode ;          /* must always be zero for leaves */
   state nw, ne, sw, se ;      /* constant */
   bigint leafpop ;            /* how many set bits */
} ;
/*
 *   If it is a struct ghnode, this returns a non-zero value, otherwise it
 *   returns a zero value.
 */
#define is_ghnode(n) (((ghnode *)(n))->nw)
/*
 *   For explicit prefetching we retain some state for our lookup
 *   routines.
 */
#ifdef USEPREFETCH
struct ghsetup_t { 
   g_uintptr_t h ;
   struct ghnode *nw, *ne, *sw, *se ;
   void prefetch(struct ghnode **addr) const { PREFETCH(addr) ; }
} ;
#endif

/**
 *   Our ghashbase class.  Note that this is an abstract class; you need
 *   to expand specific methods to specialize it for a particular multi-state
 *   automata.
 */
class ghashbase : public lifealgo {
public:
   ghashbase() ;
   virtual ~ghashbase() ;
   //  This is the method that computes the next generation, slowly.
   //  This should be overridden by a deriving class.
   virtual state slowcalc(state nw, state n, state ne, state w, state c,
                          state e, state sw, state s, state se) = 0 ;
   virtual int setcell(int x, int y, int newstate) ;
   virtual int getcell(int x, int y) ;
   virtual int nextcell(int x, int y, int &v) ;
   virtual void endofpattern() ;
   virtual void setIncrement(bigint inc) ;
   virtual void setIncrement(int inc) { setIncrement(bigint(inc)) ; }
   virtual void setGeneration(bigint gen) { generation = gen ; }
   virtual const bigint &getPopulation() ;
   virtual int isEmpty() ;
   virtual int hyperCapable() { return 1 ; }
   virtual void setMaxMemory(int m) ;
   virtual int getMaxMemory() { return (int)(maxmem >> 20) ; }
   virtual const char *setrule(const char *) ;
   virtual const char *getrule() { return "" ; }
   virtual void step() ;
   virtual void* getcurrentstate() { return root ; }
   virtual void setcurrentstate(void *n) ;
   /*
    *   The contract of draw() is that it render every pixel in the
    *   viewport precisely once.  This allows us to eliminate all
    *   flashing.  Later we'll make this be damage-specific.
    */
   virtual void draw(viewport &view, liferender &renderer) ;
   virtual void fit(viewport &view, int force) ;
   virtual void lowerRightPixel(bigint &x, bigint &y, int mag) ;
   virtual void findedges(bigint *t, bigint *l, bigint *b, bigint *r) ;
   virtual const char *readmacrocell(char *line) ;
   virtual const char *writeNativeFormat(std::ostream &os, char *comments) ;
   static void doInitializeAlgoInfo(staticAlgoInfo &) ;
   
private:
/*
 *   Some globals representing our universe.  The root is the
 *   real root of the universe, and the depth is the depth of the
 *   tree where 2 means that root is a ghleaf, and 3 means that the
 *   children of root are leaves, and so on.  The center of the
 *   root is always coordinate position (0,0), so at startup the
 *   x and y coordinates range from -4..3; in general,
 *   -(2**depth)..(2**depth)-1.  The zeroghnodea is an
 *   array of canonical `empty-space' ghnodes at various depths.
 *   The ngens is an input parameter which is the second power of
 *   the number of generations to run.
 */
   ghnode *root ;
   int depth ;
   ghnode **zeroghnodea ;
   int nzeros ;
/*
 *   Finally, our gc routine.  We keep a `stack' of all the `roots'
 *   we want to preserve.  Nodes not reachable from here, we allow to
 *   be freed.  Same with leaves.
 */
   ghnode **stack ;
   int stacksize ;
   g_uintptr_t hashpop, hashlimit, hashprime ;
#ifndef PRIMEMOD
   g_uintptr_t hashmask ;
#endif
   static double maxloadfactor ;
   ghnode **hashtab ;
   int halvesdone ;
   int gsp ;
   g_uintptr_t alloced, maxmem ;
   ghnode *freeghnodes ;
   int okaytogc ;
   g_uintptr_t totalthings ;
   ghnode *ghnodeblocks ;
   bigint population ;
   bigint setincrement ;
   bigint pow2step ; // greatest power of two in increment
   int nonpow2 ; // increment / pow2step
   int ngens ; // log2(pow2step)
   int popValid, needPop, inGC ;
   /*
    *   When rendering we store the relevant bits here rather than
    *   passing them deep into recursive subroutines.
    */
   liferender *renderer ;
   viewport *view ;
   int uviewh, uvieww, viewh, vieww, mag, pmag ;
   int llbits, llsize ;
   char *llxb, *llyb ;
   int hashed ;
   int cacheinvalid ;
   g_uintptr_t cellcounter ; // used when writing
   g_uintptr_t writecells ; // how many to write
   int gccount ; // how many gcs total this pattern
   int gcstep ; // how many gcs this step
   hperf running_hperf, step_hperf, inc_hperf ;
   int softinterrupt ;
   static char statusline[] ;
//
   void resize() ;
   ghnode *find_ghnode(ghnode *nw, ghnode *ne, ghnode *sw, ghnode *se) ;
#ifdef USEPREFETCH
   ghnode *find_ghnode(ghsetup_t &su) ;
   void setupprefetch(ghsetup_t &su, ghnode *nw, ghnode *ne, ghnode *sw, ghnode *se) ;
#endif
   void unhash_ghnode(ghnode *n) ;
   void unhash_ghnode2(ghnode *n) ;
   void rehash_ghnode(ghnode *n) ;
   ghleaf *find_ghleaf(state nw, state ne, state sw, state se) ;
   ghnode *getres(ghnode *n, int depth) ;
   ghnode *dorecurs(ghnode *n, ghnode *ne, ghnode *t, ghnode *e, int depth) ;
   ghnode *dorecurs_half(ghnode *n, ghnode *ne, ghnode *t, ghnode *e, int depth) ;
   ghleaf *dorecurs_ghleaf(ghleaf *n, ghleaf *ne, ghleaf *t, ghleaf *e) ;
   ghnode *newghnode() ;
   ghleaf *newghleaf() ;
   ghnode *newclearedghnode() ;
   ghleaf *newclearedghleaf() ;
   void repurpose(void *mem, g_uintptr_t bytes) ;
   void pushroot_1() ;
   int ghnode_depth(ghnode *n) ;
   ghnode *zeroghnode(int depth) ;
   ghnode *pushroot(ghnode *n) ;
   ghnode *gsetbit(ghnode *n, int x, int y, int newstate, int depth) ;
   int getbit(ghnode *n, int x, int y, int depth) ;
   int nextbit(ghnode *n, int x, int y, int depth, int &v) ;
   ghnode *hashpattern(ghnode *root, int depth) ;
   ghnode *popzeros(ghnode *n) ;
   const bigint &calcpop(ghnode *root, int depth) ;
   void aftercalcpop2(ghnode *root, int depth) ;
   void afterwritemc(ghnode *root, int depth) ;
   void calcPopulation() ;
   ghnode *save(ghnode *n) ;
   void pop(int n) ;
   void clearstack() ;
   void clearcache() ;
   void gc_mark(ghnode *root, int invalidate) ;
   void do_gc(int invalidate) ;
   void clearcache(ghnode *n, int depth, int clearto) ;
   void clearcache_p1(ghnode *n, int depth, int clearto) ;
   void clearcache_p2(ghnode *n, int depth, int clearto) ;
   void new_ngens(int newval) ;
   int log2(unsigned int n) ;
   ghnode *runpattern() ;
   void renderbm(int x, int y) ;
   void fill_ll(int d) ;
   void drawghnode(ghnode *n, int llx, int lly, int depth, ghnode *z) ;
   void ensure_hashed() ;
   g_uintptr_t writecell(std::ostream &os, ghnode *root, int depth) ;
   g_uintptr_t writecell_2p1(ghnode *root, int depth) ;
   g_uintptr_t writecell_2p2(std::ostream &os, ghnode *root, int depth) ;
   void drawpixel(int x, int y);
   void draw4x4_1(state sw, state se, state nw, state ne, int llx, int lly) ;
   void draw4x4_1(ghnode *n, ghnode *z, int llx, int lly) ;
   // AKT: set all pixels to background color
   void killpixels();
} ;
#endif

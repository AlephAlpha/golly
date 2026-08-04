// This file is part of Golly.
// See docs/License.html for the copyright notice.

/*
 * jvn 0.99 by Radical Eye Software.
 *
 *   All good ideas here were originated by Gosper or Bell or others, I'm
 *   sure, and all bad ones by yours truly.
 *
 *   The main reason I wrote this program was to attempt to push out the
 *   evaluation of metacatacryst as far as I could.  So this program
 *   really does very little other than compute life as far into the
 *   future as possible, using as little memory as possible (and reusing
 *   it if necessary).  No UI, few options.
 */
#include "ghashbase.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
using namespace std ;
/*
 *   Power of two hash sizes work fine.
 */
#ifdef PRIMEMOD
#define HASHMOD(a) ((a)%hashprime)
static g_uintptr_t nexthashsize(g_uintptr_t i) {
   g_uintptr_t j ;
   i |= 1 ;
   for (;; i+=2) {
      for (j=3; j*j<=i; j+=2)
         if (i % j == 0)
            break ;
      if (j*j > i)
         return i ;
   }
}
#else
#define HASHMOD(a) ((a)&(hashmask))
static g_uintptr_t nexthashsize(g_uintptr_t i) {
   while ((i & (i - 1)))
      i += (i & (1 + ~i)) ; // i & - i is more idiomatic but generates warning
   return i ;
}
#endif
/*
 *   We do now support garbage collection, but there are some routines we
 *   call frequently to help us.
 */
#ifdef PRIMEMOD
#define ghnode_hash(a,b,c,d) (65537*(g_uintptr_t)(d)+257*(g_uintptr_t)(c)+17*(g_uintptr_t)(b)+5*(g_uintptr_t)(a))
#else
g_uintptr_t ghnode_hash(void *a, void *b, void *c, void *d) {
   g_uintptr_t r = (65537*(g_uintptr_t)(d)+257*(g_uintptr_t)(c)+17*(g_uintptr_t)(b)+5*(g_uintptr_t)(a)) ;
   r += (r >> 11) ;
   return r ;
}
#endif
#define ghleaf_hash(a,b,c,d) (65537*(d)+257*(c)+17*(b)+5*(a))
/*
 *   Resize the hash.  The max load factor defined here does not actually
 *   yield the maximum load factor the hash will see, because when we
 *   do the last resize before exhausting memory, we may find we are
 *   not permitted (while keeping total memory consumption below the
 *   limit) to do the resize, so the actual max load factor may be
 *   somewhat higher.  Conversely, because we double the hash size
 *   each time, the actual final max load factor may be less than this.
 *   Additional code can be added to manage this, but after some
 *   experimentation, it has been found that the impact is tiny, so
 *   we are keeping the code simple.  Nonetheless, this factor can be
 *   tweaked in the case where you absolutely want as many nodes as
 *   possible in memory, and are willing to use a large load factor to
 *   permit this; with the move-to-front heuristic, the code actually
 *   handles a large load factor fairly well.
 */
double ghashbase::maxloadfactor = 0.7 ;
void ghashbase::resize() {
#ifndef NOGCBEFORERESIZE
   if (okaytogc) {
      do_gc(0) ;
   }
#endif
   g_uintptr_t i, nhashprime = nexthashsize(2 * hashprime) ;
   ghnode *p, **nhashtab ;
   if (hashprime > (totalthings >> 2)) {
      if (alloced > maxmem ||
          nhashprime * sizeof(ghnode *) > (maxmem - alloced)) {
         hashlimit = G_MAX ;
         return ;
      }
   }
   if (verbose) {
     sprintf(statusline, "Resizing hash to %" PRIuPTR "...", nhashprime) ;
     lifestatus(statusline) ;
   }
   nhashtab = (ghnode **)calloc(nhashprime, sizeof(ghnode *)) ;
   if (nhashtab == 0) {
     lifewarning("Out of memory; running in a somewhat slower mode; "
                 "try reducing the hash memory limit after restarting.") ;
     hashlimit = G_MAX ;
     return ;
   }
   alloced += sizeof(ghnode *) * nhashprime ;
   g_uintptr_t ohashprime = hashprime ;
   hashprime = nhashprime ;
#ifndef PRIMEMOD
   hashmask = hashprime - 1 ;
#endif
   for (i=0; i<ohashprime; i++) {
      for (p=hashtab[i]; p;) {
         ghnode *np = p->next ;
         g_uintptr_t h ;
         if (is_ghnode(p)) {
            h = ghnode_hash(p->nw, p->ne, p->sw, p->se) ;
         } else {
            ghleaf *l = (ghleaf *)p ;
            h = ghleaf_hash(l->nw, l->ne, l->sw, l->se) ;
         }
         h = HASHMOD(h) ;
         p->next = nhashtab[h] ;
         nhashtab[h] = p ;
         p = np ;
      }
   }
   repurpose(hashtab, sizeof(ghnode *) * ohashprime) ;
   hashtab = nhashtab ;
   hashlimit = (g_uintptr_t)(maxloadfactor * hashprime) ;
   if (verbose) {
     strcpy(statusline+strlen(statusline), " done.") ;
     lifestatus(statusline) ;
   }
}
/*
 *   These next two routines are (nearly) our only hash table access
 *   routines; we simply look up the passed in information.  If we
 *   find it in the hash table, we return it; otherwise, we build a
 *   new ghnode and store it in the hash table, and return that.
 */
ghnode *ghashbase::find_ghnode(ghnode *nw, ghnode *ne, ghnode *sw, ghnode *se) {
   ghnode *p ;
   g_uintptr_t h = ghnode_hash(nw,ne,sw,se) ;
   ghnode *pred = 0 ;
   h = HASHMOD(h) ;
   for (p=hashtab[h]; p; p = p->next) { /* make sure to compare nw *first* */
      if (nw == p->nw && ne == p->ne && sw == p->sw && se == p->se) {
         if (pred) { /* move this one to the front */
            pred->next = p->next ;
            p->next = hashtab[h] ;
            hashtab[h] = p ;
         }
         return save(p) ;
      }
      pred = p ;
   }
   p = newghnode() ;
   p->nw = nw ;
   p->ne = ne ;
   p->sw = sw ;
   p->se = se ;
   p->res = 0 ;
   p->next = hashtab[h] ;
   hashtab[h] = p ;
   hashpop++ ;
   save(p) ;
   if (hashpop > hashlimit)
      resize() ;
   return p ;
}
ghleaf *ghashbase::find_ghleaf(state nw, state ne, state sw, state se) {
   ghleaf *p ;
   ghleaf *pred = 0 ;
   g_uintptr_t h = ghleaf_hash(nw, ne, sw, se) ;
   h = HASHMOD(h) ;
   for (p=(ghleaf *)hashtab[h]; p; p = (ghleaf *)p->next) {
      if (nw == p->nw && ne == p->ne && sw == p->sw && se == p->se &&
          !is_ghnode(p)) {
         if (pred) {
            pred->next = p->next ;
            p->next = hashtab[h] ;
            hashtab[h] = (ghnode *)p ;
         }
         return (ghleaf *)save((ghnode *)p) ;
      }
      pred = p ;
   }
   p = newghleaf() ;
   p->nw = nw ;
   p->ne = ne ;
   p->sw = sw ;
   p->se = se ;
   p->leafpop = bigint((short)((nw != 0) + (ne != 0) + (sw != 0) + (se != 0))) ;
   p->isghnode = 0 ;
   p->next = hashtab[h] ;
   hashtab[h] = (ghnode *)p ;
   hashpop++ ;
   save((ghnode *)p) ;
   if (hashpop > hashlimit)
      resize() ;
   return p ;
}
/*
 *   The following routine does the same, but first it checks to see if
 *   the cached result is any good.  If it is, it directly returns that.
 *   Otherwise, it figures out whether to call the ghleaf routine or the
 *   non-ghleaf routine by whether two ghnodes down is a ghleaf ghnode or not.
 *   (We'll understand why this is a bit later.)  All the sp stuff is
 *   stack pointer and garbage collection stuff.
 */
ghnode *ghashbase::getres(ghnode *n, int depth) {
   if (n->res)
     return n->res ;
   ghnode *res = 0 ;
   /**
    *   This routine be the only place we assign to res.  We use
    *   the fact that the poll routine is *sticky* to allow us to
    *   manage unwinding the stack without munging our data
    *   structures.  Note that there may be many find_ghnodes
    *   and getres called before we finally actually exit from
    *   here, because the stack is deep and we don't want to
    *   put checks throughout the code.  Instead we need two
    *   calls here, one to prevent us going deeper, and another
    *   to prevent us from destroying the cache field.
    */
   if (poller->poll() || softinterrupt) return zeroghnode(depth-1) ;
   int sp = gsp ;
   if (running_hperf.fastinc(depth, ngens < depth))
      running_hperf.report(inc_hperf, verbose) ;
   depth-- ;
   if (ngens >= depth) {
     if (is_ghnode(n->nw)) {
       res = dorecurs(n->nw, n->ne, n->sw, n->se, depth) ;
     } else {
       res = (ghnode *)dorecurs_ghleaf((ghleaf *)n->nw, (ghleaf *)n->ne,
                                       (ghleaf *)n->sw, (ghleaf *)n->se) ;
     }
   } else {
     if (is_ghnode(n->nw)) {
       res = dorecurs_half(n->nw, n->ne, n->sw, n->se, depth) ;
     } else {
       lifefatal("! can't happen") ;
     }
   }
   pop(sp) ;
   if (softinterrupt || poller->isInterrupted()) // don't assign this to the cache field!
     res = zeroghnode(depth) ;
   else {
     if (ngens < depth && halvesdone < 1000)
       halvesdone++ ;
     n->res = res ;
   }
   return res ;
}
#ifdef USEPREFETCH
void ghashbase::setupprefetch(ghsetup_t &su, ghnode *nw, ghnode *ne, ghnode *sw, ghnode *se) {
   su.h = ghnode_hash(nw,ne,sw,se) ;
   su.nw = nw ;
   su.ne = ne ;
   su.sw = sw ;
   su.se = se ;
   su.prefetch(hashtab + HASHMOD(su.h)) ;
}
ghnode *ghashbase::find_ghnode(ghsetup_t &su) {
   ghnode *p ;
   ghnode *pred = 0 ;
   g_uintptr_t h = HASHMOD(su.h) ;
   for (p=hashtab[h]; p; p = p->next) { /* make sure to compare nw *first* */
      if (su.nw == p->nw && su.ne == p->ne && su.sw == p->sw && su.se == p->se) {
         if (pred) { /* move this one to the front */
            pred->next = p->next ;
            p->next = hashtab[h] ;
            hashtab[h] = p ;
         }
         return save(p) ;
      }
      pred = p ;
   }
   p = newghnode() ;
   p->nw = su.nw ;
   p->ne = su.ne ;
   p->sw = su.sw ;
   p->se = su.se ;
   p->res = 0 ;
   p->next = hashtab[h] ;
   hashtab[h] = p ;
   hashpop++ ;
   save(p) ;
   if (hashpop > hashlimit)
      resize() ;
   return p ;
}
ghnode *ghashbase::dorecurs(ghnode *n, ghnode *ne, ghnode *t, ghnode *e, int depth) {
   int sp = gsp ;
   ghsetup_t su[5] ;
   setupprefetch(su[2], n->se, ne->sw, t->ne, e->nw) ;
   setupprefetch(su[0], n->ne, ne->nw, n->se, ne->sw) ;
   setupprefetch(su[1], ne->sw, ne->se, e->nw, e->ne) ;
   setupprefetch(su[3], n->sw, n->se, t->nw, t->ne) ;
   setupprefetch(su[4], t->ne, e->nw, t->se, e->sw) ;
   ghnode
   *t00 = getres(n, depth),
   *t01 = getres(find_ghnode(su[0]), depth),
   *t02 = getres(ne, depth),
   *t12 = getres(find_ghnode(su[1]), depth),
   *t11 = getres(find_ghnode(su[2]), depth),
   *t10 = getres(find_ghnode(su[3]), depth),
   *t20 = getres(t, depth),
   *t21 = getres(find_ghnode(su[4]), depth),
   *t22 = getres(e, depth) ;
   setupprefetch(su[0], t11, t12, t21, t22) ;
   setupprefetch(su[1], t10, t11, t20, t21) ;
   setupprefetch(su[2], t00, t01, t10, t11) ;
   setupprefetch(su[3], t01, t02, t11, t12) ;
   ghnode
   *t44 = getres(find_ghnode(su[0]), depth),
   *t43 = getres(find_ghnode(su[1]), depth),
   *t33 = getres(find_ghnode(su[2]), depth),
   *t34 = getres(find_ghnode(su[3]), depth) ;
   n = find_ghnode(t33, t34, t43, t44) ;
   pop(sp) ;
   return save(n) ;
}
#else
/*
 *   So let's say the cached way failed.  How do we do it the slow way?
 *   Recursively, of course.  For an n-square (composed of the four
 *   n/2-squares passed in, compute the n/2-square that is n/4
 *   generations ahead.
 *
 *   This routine works exactly the same as the ghleafres() routine, only
 *   instead of working on an 8-square, we're working on an n-square,
 *   returning an n/2-square, and we build that n/2-square by first building
 *   9 n/4-squares, use those to calculate 4 more n/4-squares, and
 *   then put these together into a new n/2-square.  Simple, eh?
 */
ghnode *ghashbase::dorecurs(ghnode *n, ghnode *ne, ghnode *t,
                            ghnode *e, int depth) {
   int sp = gsp ;
   ghnode
   *t11 = getres(find_ghnode(n->se, ne->sw, t->ne, e->nw), depth),
   *t00 = getres(n, depth),
   *t01 = getres(find_ghnode(n->ne, ne->nw, n->se, ne->sw), depth),
   *t02 = getres(ne, depth),
   *t12 = getres(find_ghnode(ne->sw, ne->se, e->nw, e->ne), depth),
   *t10 = getres(find_ghnode(n->sw, n->se, t->nw, t->ne), depth),
   *t20 = getres(t, depth),
   *t21 = getres(find_ghnode(t->ne, e->nw, t->se, e->sw), depth),
   *t22 = getres(e, depth),
   *t44 = getres(find_ghnode(t11, t12, t21, t22), depth),
   *t43 = getres(find_ghnode(t10, t11, t20, t21), depth),
   *t33 = getres(find_ghnode(t00, t01, t10, t11), depth),
   *t34 = getres(find_ghnode(t01, t02, t11, t12), depth) ;
   n = find_ghnode(t33, t34, t43, t44) ;
   pop(sp) ;
   return save(n) ;
}
#endif
/*
 *   Same as above, but we only do one step instead of 2.
 */
ghnode *ghashbase::dorecurs_half(ghnode *n, ghnode *ne, ghnode *t,
                               ghnode *e, int depth) {
   int sp = gsp ;
   if (depth > 1) {
      ghnode
      *t00 = find_ghnode(n->nw->se, n->ne->sw, n->sw->ne, n->se->nw),
      *t01 = find_ghnode(n->ne->se, ne->nw->sw, n->se->ne, ne->sw->nw),
      *t02 = find_ghnode(ne->nw->se, ne->ne->sw, ne->sw->ne, ne->se->nw),
      *t10 = find_ghnode(n->sw->se, n->se->sw, t->nw->ne, t->ne->nw),
      *t11 = find_ghnode(n->se->se, ne->sw->sw, t->ne->ne, e->nw->nw),
      *t12 = find_ghnode(ne->sw->se, ne->se->sw, e->nw->ne, e->ne->nw),
      *t20 = find_ghnode(t->nw->se, t->ne->sw, t->sw->ne, t->se->nw),
      *t21 = find_ghnode(t->ne->se, e->nw->sw, t->se->ne, e->sw->nw),
      *t22 = find_ghnode(e->nw->se, e->ne->sw, e->sw->ne, e->se->nw) ;
      n = find_ghnode(getres(find_ghnode(t00, t01, t10, t11), depth),
                      getres(find_ghnode(t01, t02, t11, t12), depth),
                      getres(find_ghnode(t10, t11, t20, t21), depth),
                      getres(find_ghnode(t11, t12, t21, t22), depth)) ;
   } else {
      ghnode
      *t00 = getres(n, depth),
      *t01 = getres(find_ghnode(n->ne, ne->nw, n->se, ne->sw), depth),
      *t10 = getres(find_ghnode(n->sw, n->se, t->nw, t->ne), depth),
      *t11 = getres(find_ghnode(n->se, ne->sw, t->ne, e->nw), depth),
      *t02 = getres(ne, depth),
      *t12 = getres(find_ghnode(ne->sw, ne->se, e->nw, e->ne), depth),
      *t20 = getres(t, depth),
      *t21 = getres(find_ghnode(t->ne, e->nw, t->se, e->sw), depth),
      *t22 = getres(e, depth) ;
      n = find_ghnode((ghnode *)find_ghleaf(((ghleaf *)t00)->se,
                                             ((ghleaf *)t01)->sw,
                                             ((ghleaf *)t10)->ne,
                                             ((ghleaf *)t11)->nw),
                    (ghnode *)find_ghleaf(((ghleaf *)t01)->se,
                                             ((ghleaf *)t02)->sw,
                                             ((ghleaf *)t11)->ne,
                                             ((ghleaf *)t12)->nw),
                    (ghnode *)find_ghleaf(((ghleaf *)t10)->se,
                                             ((ghleaf *)t11)->sw,
                                             ((ghleaf *)t20)->ne,
                                             ((ghleaf *)t21)->nw),
                    (ghnode *)find_ghleaf(((ghleaf *)t11)->se,
                                             ((ghleaf *)t12)->sw,
                                             ((ghleaf *)t21)->ne,
                                             ((ghleaf *)t22)->nw)) ;
   }
   pop(sp) ;
   return save(n) ;
}
/*
 *   If the ghnode is a 16-ghnode, then the constituents are leaves, so we
 *   need a very similar but still somewhat different subroutine.  Since
 *   we do not (yet) garbage collect leaves, we don't need all that
 *   save/pop mumbo-jumbo.
 */
ghleaf *ghashbase::dorecurs_ghleaf(ghleaf *nw, ghleaf *ne, ghleaf *sw,
                                   ghleaf *se) {
   return find_ghleaf(
             slowcalc(nw->nw, nw->ne, ne->nw,
                      nw->sw, nw->se, ne->sw,
                      sw->nw, sw->ne, se->nw),
             slowcalc(nw->ne, ne->nw, ne->ne,
                      nw->se, ne->sw, ne->se,
                      sw->ne, se->nw, se->ne),
             slowcalc(nw->sw, nw->se, ne->sw,
                      sw->nw, sw->ne, se->nw,
                      sw->sw, sw->se, se->sw),
             slowcalc(nw->se, ne->sw, ne->se,
                      sw->ne, se->nw, se->ne,
                      sw->se, se->sw, se->se)) ;
}
/*
 *   We keep free ghnodes in a linked list for allocation, and we allocate
 *   them 1360 at a time.  This gives allocations close to, but not quite,
 *   a power of two, to make memory allocators happy.
 */
static const int NODECHUNKCOUNT = 1360 ;
ghnode *ghashbase::newghnode() {
   ghnode *r ;
   if (freeghnodes == 0) {
      int i ;
      freeghnodes = (ghnode *)calloc(NODECHUNKCOUNT+1, sizeof(ghnode)) ;
      if (freeghnodes == 0)
         lifefatal("Out of memory; try reducing the hash memory limit.") ;
      alloced += (NODECHUNKCOUNT + 1) * sizeof(ghnode) ;
      freeghnodes->next = ghnodeblocks ;
      freeghnodes->res = (struct ghnode *)NODECHUNKCOUNT ;
      ghnodeblocks = freeghnodes++ ;
      for (i=0; i<(NODECHUNKCOUNT-1); i++) {
         freeghnodes[1].next = freeghnodes ;
         freeghnodes++ ;
      }
      totalthings += NODECHUNKCOUNT ;
   }
   if (freeghnodes->next == 0 && alloced + NODECHUNKCOUNT * sizeof(ghnode) > maxmem &&
       okaytogc) {
      do_gc(0) ;
   }
   r = freeghnodes ;
   freeghnodes = freeghnodes->next ;
   return r ;
}
/*          
 *   Instead of freeing old hashtable arrays (which can be large), in hopes
 *   the system will reuse that memory for nodes, we instead chop up the
 *   arrays ourself into node blocks.  This is because modern zoned memory
 *   allocators may often never return large blocks to the system, or reuse
 *   freed large blocks for smaller allocations, leading to memory use that
 *   is much larger than the user set as the limit.
 */
void ghashbase::repurpose(void *mem, g_uintptr_t bytes) {
   g_uintptr_t nodecount = bytes / sizeof(struct ghnode) ;
   if (nodecount < 2) {
      free(mem) ;
      alloced -= bytes ;
      return ;
   }        
   struct ghnode *newblock = (struct ghnode *)mem ;
   newblock->next = ghnodeblocks ;
   ghnodeblocks = newblock ;
   nodecount-- ;
   newblock->res = (struct ghnode *)nodecount ;
   newblock++ ; 
   g_uintptr_t i ;
   for (i=0; i<nodecount; i++)
      newblock[i].next = newblock + i + 1 ;
   newblock[nodecount-1].next = freeghnodes ;
   freeghnodes = newblock ;
   totalthings += nodecount ;
}  
/*
 *   Leaves are the same.
 */
ghleaf *ghashbase::newghleaf() {
   ghleaf *r = (ghleaf *)newghnode() ;
   new(&(r->leafpop))bigint ;
   return r ;
}
/*
 *   Sometimes we want the new ghnode or ghleaf to be automatically cleared
 *   for us.
 */
ghnode *ghashbase::newclearedghnode() {
   return (ghnode *)memset(newghnode(), 0, sizeof(ghnode)) ;
}
ghleaf *ghashbase::newclearedghleaf() {
   ghleaf *r = (ghleaf *)newclearedghnode() ;
   new(&(r->leafpop))bigint ;
   return r ;
}
ghashbase::ghashbase() {
   hashprime = nexthashsize(1000) ;
#ifndef PRIMEMOD
   hashmask = hashprime - 1 ;
#endif
   hashlimit = (g_uintptr_t)(maxloadfactor * hashprime) ;
   hashpop = 0 ;
   hashtab = (ghnode **)calloc(hashprime, sizeof(ghnode *)) ;
   if (hashtab == 0)
     lifefatal("Out of memory (1).") ;
   alloced = hashprime * sizeof(ghnode *) ;
   ngens = 0 ;
   stacksize = 0 ;
   halvesdone = 0 ;
   nzeros = 0 ;
   stack = 0 ;
   gsp = 0 ;
   maxmem = 256 * 1024 * 1024 ;
   freeghnodes = 0 ;
   okaytogc = 0 ;
   totalthings = 0 ;
   ghnodeblocks = 0 ;
   zeroghnodea = 0 ;
/*
 *   We initialize our universe to be a 16-square.  We are in drawing
 *   mode at this point.
 */
   root = (ghnode *)newclearedghnode() ;
   population = 0 ;
   generation = 0 ;
   increment = 1 ;
   setincrement = 1 ;
   nonpow2 = 1 ;
   pow2step = 1 ;
   llsize = 0 ;
   depth = 1 ;
   hashed = 0 ;
   popValid = 0 ;
   needPop = 0 ;
   inGC = 0 ;
   cacheinvalid = 0 ;
   gccount = 0 ;
   gcstep = 0 ;
   running_hperf.clear() ;
   inc_hperf = running_hperf ;
   step_hperf = running_hperf ;
   softinterrupt = 0 ;
}
/**
 *   Destructor frees memory.
 */
ghashbase::~ghashbase() {
   free(hashtab) ;
   while (ghnodeblocks) {
      ghnode *r = ghnodeblocks ;
      ghnodeblocks = ghnodeblocks->next ;
      free(r) ;
   }
   if (zeroghnodea)
      free(zeroghnodea) ;
   if (stack)
      free(stack) ;
   if (llsize) {
      delete [] llxb ;
      delete [] llyb ;
   }
}
/**
 *   Set increment.
 */
void ghashbase::setIncrement(bigint inc) {
   if (inc < increment)
      softinterrupt = 1 ;
   increment = inc ;
}
/**
 *   Do a step.
 */
void ghashbase::step() {
   poller->bailIfCalculating() ;
   // we use while here because the increment may be changed while we are
   // doing the hashtable sweep; if that happens, we may need to sweep
   // again.
   while (1) {
      int cleareddownto = 1000000000 ;
      softinterrupt = 0 ;
      while (increment != setincrement) {
         bigint pendingincrement = increment ;
         int newpow2 = 0 ;
         bigint t = pendingincrement ;
         while (t > 0 && t.even()) {
            newpow2++ ;
            t.div2() ;
         }
         nonpow2 = t.low31() ;
         if (t != nonpow2)
            lifefatal("bad increment") ;
         int downto = newpow2 ;
         if (ngens < newpow2)
            downto = ngens ;
         if (newpow2 != ngens && cleareddownto > downto) {
            new_ngens(newpow2) ;
            cleareddownto = downto ;
         } else {
            ngens = newpow2 ;
         }
         setincrement = pendingincrement ;
         pow2step = 1 ;
         while (newpow2--)
            pow2step += pow2step ;
      }
      gcstep = 0 ;
      running_hperf.genval = generation.todouble() ;
      for (int i=0; i<nonpow2; i++) {
         ghnode *newroot = runpattern() ;
         if (newroot == 0 || softinterrupt || poller->isInterrupted()) // we *were* interrupted
            break ;
         popValid = 0 ;
         root = newroot ;
         depth = ghnode_depth(root) ;
      }
      running_hperf.reportStep(step_hperf, inc_hperf, generation.todouble(), verbose) ;
      if (poller->isInterrupted() || !softinterrupt)
         break ;
   }
}
void ghashbase::setcurrentstate(void *n) {
   if (root != (ghnode *)n) {
      root = (ghnode *)n ;
      depth = ghnode_depth(root) ;
      popValid = 0 ;
   }
}
/*
 *   Set the max memory
 */
void ghashbase::setMaxMemory(int newmemlimit) {
   if (newmemlimit < 10)
     newmemlimit = 10 ;
#ifndef GOLLY64BIT
   else if (newmemlimit > 4000)
     newmemlimit = 4000 ;
#endif
   g_uintptr_t newlimit = ((g_uintptr_t)newmemlimit) << 20 ;
   if (alloced > newlimit) {
      lifewarning("Sorry, more memory currently used than allowed.") ;
      return ;
   }
   maxmem = newlimit ;
   hashlimit = (g_uintptr_t)(maxloadfactor * hashprime) ;
}
/*
 *   This routine expands our universe by a factor of two, maintaining
 *   centering.  We use four new ghnodes, and *reuse* the root so this cannot
 *   be called after we've started hashing.
 */
void ghashbase::pushroot_1() {
   ghnode *t ;
   t = newclearedghnode() ;
   t->se = root->nw ;
   root->nw = t ;
   t = newclearedghnode() ;
   t->sw = root->ne ;
   root->ne = t ;
   t = newclearedghnode() ;
   t->ne = root->sw ;
   root->sw = t ;
   t = newclearedghnode() ;
   t->nw = root->se ;
   root->se = t ;
   depth++ ;
}
/*
 *   Return the depth of this ghnode (2 is 8x8).
 */
int ghashbase::ghnode_depth(ghnode *n) {
   int depth = 0 ;
   while (is_ghnode(n)) {
      depth++ ;
      n = n->nw ;
   }
   return depth ;
}
/*
 *   This routine returns the canonical clear space ghnode at a particular
 *   depth.
 */
ghnode *ghashbase::zeroghnode(int depth) {
   while (depth >= nzeros) {
      int nnzeros = 2 * nzeros + 10 ;
      zeroghnodea = (ghnode **)realloc(zeroghnodea,
                                          nnzeros * sizeof(ghnode *)) ;
      if (zeroghnodea == 0)
        lifefatal("Out of memory (2).") ;
      alloced += (nnzeros - nzeros) * sizeof(ghnode *) ;
      while (nzeros < nnzeros)
         zeroghnodea[nzeros++] = 0 ;
   }
   if (zeroghnodea[depth] == 0) {
      if (depth == 0) {
         zeroghnodea[depth] = (ghnode *)find_ghleaf(0, 0, 0, 0) ;
      } else {
         ghnode *z = zeroghnode(depth-1) ;
         zeroghnodea[depth] = find_ghnode(z, z, z, z) ;
      }
   }
   return zeroghnodea[depth] ;
}
/*
 *   Same, but with hashed ghnodes.
 */
ghnode *ghashbase::pushroot(ghnode *n) {
   int depth = ghnode_depth(n) ;
   zeroghnode(depth+1) ; // ensure zeros are deep enough
   ghnode *z = zeroghnode(depth-1) ;
   return find_ghnode(find_ghnode(z, z, z, n->nw),
                    find_ghnode(z, z, n->ne, z),
                    find_ghnode(z, n->sw, z, z),
                    find_ghnode(n->se, z, z, z)) ;
}
/*
 *   Here is our recursive routine to set a bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.  We allocate new ghnodes and
 *   leaves on our way down.
 *
 *   Note that at this point our universe lives outside the hash table
 *   and has not been canonicalized, and that many of the pointers in
 *   the ghnodes can be null.  We'll patch this up in due course.
 */
ghnode *ghashbase::gsetbit(ghnode *n, int x, int y, int newstate, int depth) {
   if (depth == 0) {
      ghleaf *l = (ghleaf *)n ;
      if (hashed) {
         state nw = l->nw ;
         state sw = l->sw ;
         state ne = l->ne ;
         state se = l->se ;
         if (x < 0)
           if (y < 0)
             sw = (state)newstate ;
           else
             nw = (state)newstate ;
         else
           if (y < 0)
             se = (state)newstate ;
           else
             ne = (state)newstate ;
         return save((ghnode *)find_ghleaf(nw, ne, sw, se)) ;
      }
      if (x < 0)
        if (y < 0)
          l->sw = (state)newstate ;
        else
          l->nw = (state)newstate ;
      else
        if (y < 0)
          l->se = (state)newstate ;
        else
          l->ne = (state)newstate ;
      return (ghnode *)l ;
   } else {
      unsigned int w = 0, wh = 0 ;
      if (depth > 31) {
         if (depth == 32)
            wh = 0x80000000 ;
	 w = 0 ;
      } else {
         w = 1 << depth ;
         wh = 1 << (depth - 1) ;
      }
      depth-- ;
      ghnode **nptr ;
      if (depth+1 == this->depth || depth < 31) {
         if (x < 0) {
            if (y < 0)
               nptr = &(n->sw) ;
            else
               nptr = &(n->nw) ;
         } else {
            if (y < 0)
               nptr = &(n->se) ;
            else
               nptr = &(n->ne) ;
         }
      } else {
         if (x >= 0) {
            if (y >= 0)
               nptr = &(n->sw) ;
            else
               nptr = &(n->nw) ;
         } else {
            if (y >= 0)
               nptr = &(n->se) ;
            else
               nptr = &(n->ne) ;
         }
      }
      if (*nptr == 0) {
         if (depth == 0)
            *nptr = (ghnode *)newclearedghleaf() ;
         else
            *nptr = newclearedghnode() ;
      }
      ghnode *s = gsetbit(*nptr, (x & (w - 1)) - wh,
                                 (y & (w - 1)) - wh, newstate, depth) ;
      if (hashed) {
         ghnode *nw = (nptr == &(n->nw) ? s : n->nw) ;
         ghnode *sw = (nptr == &(n->sw) ? s : n->sw) ;
         ghnode *ne = (nptr == &(n->ne) ? s : n->ne) ;
         ghnode *se = (nptr == &(n->se) ? s : n->se) ;
         n = save(find_ghnode(nw, ne, sw, se)) ;
      } else {
         *nptr = s ;
      }
      return n ;
   }
}
/*
 *   Here is our recursive routine to get a bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.
 */
int ghashbase::getbit(ghnode *n, int x, int y, int depth) {
   struct ghnode tnode ;
   while (depth >= 32) {
      tnode.nw = n->nw->se ;
      tnode.ne = n->ne->sw ;
      tnode.sw = n->sw->ne ;
      tnode.se = n->se->nw ;
      n = &tnode ;
      depth-- ;
   }
   if (depth == 0) {
      ghleaf *l = (ghleaf *)n ;
      if (x < 0)
         if (y < 0)
           return l->sw ;
         else
           return l->nw ;
      else
         if (y < 0)
           return l->se ;
         else
           return l->ne ;
   } else {
      unsigned int w = 0, wh = 0 ;
      if (depth >= 32) {
         if (depth == 32)
            wh = 0x80000000 ;
      } else {
         w = 1 << depth ;
         wh = 1 << (depth - 1) ;
      }
      ghnode *nptr ;
      depth-- ;
      if (x < 0) {
         if (y < 0)
            nptr = n->sw ;
         else
            nptr = n->nw ;
      } else {
         if (y < 0)
            nptr = n->se ;
         else
            nptr = n->ne ;
      }
      if (nptr == 0 || nptr == zeroghnode(depth))
         return 0 ;
      return getbit(nptr, (x & (w - 1)) - wh, (y & (w - 1)) - wh,
                    depth) ;
   }
}
/*
 *   Here is our recursive routine to get the next bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.
 */
int ghashbase::nextbit(ghnode *n, int x, int y, int depth, int &v) {
   if (n == 0 || n == zeroghnode(depth))
      return -1 ;
   if (depth == 0) {
      ghleaf *l = (ghleaf *)n ;
      if (y < 0) {
        if (x < 0 && l->sw) {
          v = l->sw ;
          return 0 ;
        }
        if (l->se) {
          v = l->se ;
          return -x ;
        }
      } else {
        if (x < 0 && l->nw) {
          v = l->nw ;
          return 0 ;
        }
        if (l->ne) {
          v = l->ne ;
          return -x ;
        }
      }
      return -1 ; // none found
   } else {
      unsigned int w = 1 << depth ;
      unsigned int wh = w >> 1 ;
      ghnode *lft, *rght ;
      depth-- ;
      if (y < 0) {
        lft = n->sw ;
        rght = n->se ;
      } else {
        lft = n->nw ;
        rght = n->ne ;
      }
      int r = 0 ;
      if (x < 0) {
        int t = nextbit(lft, (x & (w-1)) - wh,
                        (y & (w - 1)) - wh, depth, v) ;
        if (t >= 0)
          return t ;
        r = -x ;
        x = 0 ;
      }
      int t = nextbit(rght, (x & (w-1)) - wh,
                      (y & (w - 1)) - wh, depth, v) ;
      if (t >= 0)
        return r + t ;
      return -1 ;
   }
}
/*
 *   Our nonrecurse top-level bit setting routine simply expands the
 *   universe as necessary to encompass the passed-in coordinates, and
 *   then invokes the recursive setbit.  Right now it works hashed or
 *   unhashed (but it's faster when unhashed).  We also turn on the inGC
 *   flag to inhibit popcount.
 */
int ghashbase::setcell(int x, int y, int newstate) {
   if (newstate < 0 || newstate >= maxCellStates)
     return -1 ;
   if (hashed) {
      clearstack() ;
      save(root) ;
      okaytogc = 1 ;
   }
   inGC = 1 ;
   y = - y ;
   int sx = x ;
   int sy = y ;
   if (depth <= 31) {
     sx >>= depth ;
     sy >>= depth ;
   } else {
     sx >>= 31 ;
     sy >>= 31 ;
   }
   while (sx > 0 || sx < -1 || sy > 0 || sy < -1) {
      if (hashed) {
         root = save(pushroot(root)) ;
         depth++ ;
      } else {
         pushroot_1() ;
      }
      sx >>= 1 ;
      sy >>= 1 ;
   }
   root = gsetbit(root, x, y, newstate, depth) ;
   if (hashed) {
      okaytogc = 0 ;
   }
   return 0 ;
}
/*
 *   Our nonrecurse top-level bit getting routine.
 */
int ghashbase::getcell(int x, int y) {
   y = - y ;
   int sx = x ;
   int sy = y ;
   if (depth <= 31) {
     sx >>= depth ;
     sy >>= depth ;
   } else {
     sx >>= 31 ;
     sy >>= 31 ;
   }
   if (sx > 0 || sx < -1 || sy > 0 || sy < -1)
      return 0 ;
   return getbit(root, x, y, depth) ;
}
/*
 *   A recursive bit getting routine, but this one returns the
 *   number of pixels to the right to the next set cell in the
 *   current universe, or -1 if none set to the right, or if
 *   the next set pixel is out of range.
 */
int ghashbase::nextcell(int x, int y, int &v) {
   y = - y ;
   int sx = x ;
   int sy = y ;
   if (depth <= 31) {
     sx >>= depth ;
     sy >>= depth ;
   } else {
     sx >>= 31 ;
     sy >>= 31 ;
   }
   while (sx > 0 || sx < -1 || sy > 0 || sy < -1) {
      if (hashed) {
         root = save(pushroot(root)) ;
         depth++ ;
      } else {
         pushroot_1() ;
      }
      sx >>= 1 ;
      sy >>= 1 ;
   }
   if (depth > 30) {
      struct ghnode tghnode = *root ;
      int mdepth = depth ;
      while (mdepth > 30) {
         tghnode.nw = tghnode.nw->se ;
         tghnode.ne = tghnode.ne->sw ;
         tghnode.sw = tghnode.sw->ne ;
         tghnode.se = tghnode.se->nw ;
         mdepth-- ;
      }
      return nextbit(&tghnode, x, y, mdepth, v) ;
   }
   return nextbit(root, x, y, depth, v) ;
}
/*
 *   Canonicalize a universe by filling in the null pointers and then
 *   invoking find_ghnode on each ghnode.  Drops the original universe on
 *   the floor [big deal, it's probably small anyway].
 */
ghnode *ghashbase::hashpattern(ghnode *root, int depth) {
   ghnode *r ;
   if (root == 0) {
      r = zeroghnode(depth) ;
   } else if (depth == 0) {
      ghleaf *n = (ghleaf *)root ;
      r = (ghnode *)find_ghleaf(n->nw, n->ne, n->sw, n->se) ;
      n->next = freeghnodes ;
      freeghnodes = root ;
   } else {
      depth-- ;
      r = find_ghnode(hashpattern(root->nw, depth),
                    hashpattern(root->ne, depth),
                    hashpattern(root->sw, depth),
                    hashpattern(root->se, depth)) ;
      root->next = freeghnodes ;
      freeghnodes = root ;
   }
   return r ;
}
void ghashbase::endofpattern() {
   poller->bailIfCalculating() ;
   if (!hashed) {
      root = hashpattern(root, depth) ;
      zeroghnode(depth) ;
      hashed = 1 ;
   }
   popValid = 0 ;
   needPop = 0 ;
   inGC = 0 ;
}
void ghashbase::ensure_hashed() {
   if (!hashed)
      endofpattern() ;
}
/*
 *   Pop off any levels we don't need.
 */
ghnode *ghashbase::popzeros(ghnode *n) {
   int depth = ghnode_depth(n) ;
   while (depth > 1) {
      ghnode *z = zeroghnode(depth-2) ;
      if (n->nw->nw == z && n->nw->ne == z && n->nw->sw == z &&
          n->ne->nw == z && n->ne->ne == z && n->ne->se == z &&
          n->sw->nw == z && n->sw->sw == z && n->sw->se == z &&
          n->se->ne == z && n->se->sw == z && n->se->se == z) {
         depth-- ;
         n = find_ghnode(n->nw->se, n->ne->sw, n->sw->ne, n->se->nw) ;
      } else {
         break ;
      }
   }
   return n ;
}
/*
 *   A lot of the routines from here on down traverse the universe, hanging
 *   information off the ghnodes.  The way they generally do so is by using
 *   (or abusing) the cache (res) field, and the least significant bit of
 *   the hash next field (as a visited bit).
 */
#define marked(n) (1 & (g_uintptr_t)(n)->next)
#define mark(n) ((n)->next = (ghnode *)(1 | (g_uintptr_t)(n)->next))
#define clearmark(n) ((n)->next = (ghnode *)(~1 & (g_uintptr_t)(n)->next))
#define clearmarkbit(p) ((ghnode *)(~1 & (g_uintptr_t)(p)))
/*
 *   Sometimes we want to use *res* instead of next to mark.  You cannot
 *   do this to leaves, though.
 */
#define marked2(n) (3 & (g_uintptr_t)(n)->res)
#define mark2(n) ((n)->res = (ghnode *)(1 | (g_uintptr_t)(n)->res))
#define mark2v(n, v) ((n)->res = (ghnode *)(v | (g_uintptr_t)(n)->res))
#define clearmark2(n) ((n)->res = (ghnode *)(~3 & (g_uintptr_t)(n)->res))
void ghashbase::unhash_ghnode(ghnode *n) {
   ghnode *p ;
   g_uintptr_t h = ghnode_hash(n->nw,n->ne,n->sw,n->se) ;
   ghnode *pred = 0 ;
   h = HASHMOD(h) ;
   for (p=hashtab[h]; (!is_ghnode(p) || !marked2(p)) && p; p = p->next) {
      if (p == n) {
         if (pred)
            pred->next = p->next ;
         else
            hashtab[h] = p->next ;
         return ;
      }
      pred = p ;
   }
   lifefatal("Didn't find ghnode to unhash") ;
}
void ghashbase::unhash_ghnode2(ghnode *n) {
   ghnode *p ;
   g_uintptr_t h = ghnode_hash(n->nw,n->ne,n->sw,n->se) ;
   ghnode *pred = 0 ;
   h = HASHMOD(h) ;
   for (p=hashtab[h]; p; p = p->next) {
      if (p == n) {
         if (pred)
            pred->next = p->next ;
         else
            hashtab[h] = p->next ;
         return ;
      }
      pred = p ;
   }
   lifefatal("Didn't find ghnode to unhash") ;
}
void ghashbase::rehash_ghnode(ghnode *n) {
   g_uintptr_t h = ghnode_hash(n->nw,n->ne,n->sw,n->se) ;
   h = HASHMOD(h) ;
   n->next = hashtab[h] ;
   hashtab[h] = n ;
}
/*
 *   This recursive routine calculates the population by hanging the
 *   population on marked ghnodes.
 */
const bigint &ghashbase::calcpop(ghnode *root, int depth) {
   if (root == zeroghnode(depth))
      return bigint::zero ;
   if (depth == 0)
      return ((ghleaf *)root)->leafpop ;
   if (marked2(root))
      return *(bigint*)&(root->next) ;
   depth-- ;
   if (root->next == 0)
      mark2v(root, 3) ;
   else {
      unhash_ghnode(root) ;
      mark2(root) ;
   }
/**
 *   We use the memory in root->next as a value bigint.  But we want to
 *   make sure the copy constructor doesn't "clean up" something that
 *   doesn't exist.  So we clear it to zero here.
 */
   new(&(root->next))bigint(
        calcpop(root->nw, depth), calcpop(root->ne, depth),
        calcpop(root->sw, depth), calcpop(root->se, depth)) ;
   return *(bigint *)&(root->next) ;
}
/*
 *   Call this after doing something that unhashes ghnodes in order to
 *   use the next field as a temp pointer.
 */
void ghashbase::aftercalcpop2(ghnode *root, int depth) {
   if (depth == 0 || root == zeroghnode(depth))
      return ;
   int v = marked2(root) ;
   if (v) {
      clearmark2(root) ;
      depth-- ;
      if (depth > 0) {
         aftercalcpop2(root->nw, depth) ;
         aftercalcpop2(root->ne, depth) ;
         aftercalcpop2(root->sw, depth) ;
         aftercalcpop2(root->se, depth) ;
      }
      ((bigint *)&(root->next))->~bigint() ;
      if (v == 3)
         root->next = 0 ;
      else
         rehash_ghnode(root) ;
   }
}
/*
 *   Call this after doing something that unhashes ghnodes in order to
 *   use the next field as a temp pointer.
 */
void ghashbase::afterwritemc(ghnode *root, int depth) {
   if (root == zeroghnode(depth))
      return ;
   if (depth == 0) {
      root->nw = 0 ; // all these bigints are guaranteed to be small
      return ;
   }
   if (marked2(root)) {
      clearmark2(root) ;
      depth-- ;
      afterwritemc(root->nw, depth) ;
      afterwritemc(root->ne, depth) ;
      afterwritemc(root->sw, depth) ;
      afterwritemc(root->se, depth) ;
      rehash_ghnode(root) ;
   }
}
/*
 *   This top level routine calculates the population of a universe.
 */
void ghashbase::calcPopulation() {
   int depth ;
   ensure_hashed() ;
   depth = ghnode_depth(root) ;
   population = calcpop(root, depth) ;
   aftercalcpop2(root, depth) ;
}
/*
 *   Is the universe empty?
 */
int ghashbase::isEmpty() {
   ensure_hashed() ;
   return root == zeroghnode(depth) ;
}
/*
 *   This routine marks a ghnode as needed to be saved.
 */
ghnode *ghashbase::save(ghnode *n) {
   if (gsp >= stacksize) {
      int nstacksize = stacksize * 2 + 100 ;
      alloced += sizeof(ghnode *)*(nstacksize-stacksize) ;
      stack = (ghnode **)realloc(stack, nstacksize * sizeof(ghnode *)) ;
      if (stack == 0)
        lifefatal("Out of memory (3).") ;
      stacksize = nstacksize ;
   }
   stack[gsp++] = n ;
   return n ;
}
/*
 *   This routine pops the stack back to a previous depth.
 */
void ghashbase::pop(int n) {
   gsp = n ;
}
/*
 *   This routine clears the stack altogether.
 */
void ghashbase::clearstack() {
   gsp = 0 ;
}
/*
 *   Do a gc.  Walk down from all ghnodes reachable on the stack, saveing
 *   them by setting the odd bit on the next link.  Then, walk the hash,
 *   eliminating the res from everything that's not saveed, and moving
 *   the ghnodes from the hash to the freelist as appropriate.  Finally,
 *   walk the hash again, clearing the low order bits in the next pointers.
 */
void ghashbase::gc_mark(ghnode *root, int invalidate) {
   if (!marked(root)) {
      mark(root) ;
      if (is_ghnode(root)) {
         gc_mark(root->nw, invalidate) ;
         gc_mark(root->ne, invalidate) ;
         gc_mark(root->sw, invalidate) ;
         gc_mark(root->se, invalidate) ;
         if (root->res) {
            if (invalidate)
              root->res = 0 ;
            else
              gc_mark(root->res, invalidate) ;
         }
      }
   }
}
/**
 *   If the invalidate flag is set, we want to kill *all* cache entries
 *   and recalculate all leaves.
 */
void ghashbase::do_gc(int invalidate) {
   int i ;
   g_uintptr_t freed_ghnodes=0 ;
   ghnode *p, *pp ;
   inGC = 1 ;
   gccount++ ;
   gcstep++ ;
   if (verbose) {
     if (gcstep > 1)
       sprintf(statusline, "GC #%d(%d)", gccount, gcstep) ;
     else
       sprintf(statusline, "GC #%d", gccount) ;
     lifestatus(statusline) ;
   }
   for (i=nzeros-1; i>=0; i--)
      if (zeroghnodea[i] != 0)
         break ;
   if (i >= 0)
      gc_mark(zeroghnodea[i], 0) ; // never invalidate zeroghnode
   if (root != 0)
      gc_mark(root, invalidate) ; // pick up the root
   for (i=0; i<gsp; i++) {
      poller->poll() ;
      gc_mark((ghnode *)stack[i], invalidate) ;
   }
   for (i=0; i<timeline.framecount; i++)
      gc_mark((ghnode *)timeline.frames[i], invalidate) ;
   hashpop = 0 ;
   memset(hashtab, 0, sizeof(ghnode *) * hashprime) ;
   freeghnodes = 0 ;
   for (p=ghnodeblocks; p; p=p->next) {
      poller->poll() ;
      for (pp=p+1, i=1; i<((g_uintptr_t)(p->res))+1; i++, pp++) {
         if (marked(pp)) {
            g_uintptr_t h = 0 ;
            if (pp->nw) { /* yes, it's a ghnode */
               h = HASHMOD(ghnode_hash(pp->nw, pp->ne, pp->sw, pp->se)) ;
            } else {
               ghleaf *lp = (ghleaf *)pp ;
               h = HASHMOD(ghleaf_hash(lp->nw, lp->ne, lp->sw, lp->se)) ;
            }
            pp->next = hashtab[h] ;
            hashtab[h] = pp ;
            hashpop++ ;
         } else {
            pp->next = freeghnodes ;
            freeghnodes = pp ;
            freed_ghnodes++ ;
         }
      }
   }
   inGC = 0 ;
   if (verbose) {
     double perc = (double)freed_ghnodes / (double)totalthings * 100.0 ;
     sprintf(statusline+strlen(statusline), " freed %g percent (%" PRIuPTR ").",
                                                  perc, freed_ghnodes) ;
     lifestatus(statusline) ;
   }
   if (needPop) {
      calcPopulation() ;
      popValid = 1 ;
      needPop = 0 ;
      poller->updatePop() ;
   }
}
/*
 *   Clear the cache bits down to the appropriate level, marking the
 *   ghnodes we've handled.
 */
void ghashbase::clearcache(ghnode *n, int depth, int clearto) {
   if (!marked(n)) {
      mark(n) ;
      if (depth > 1) {
         depth-- ;
         poller->poll() ;
         clearcache(n->nw, depth, clearto) ;
         clearcache(n->ne, depth, clearto) ;
         clearcache(n->sw, depth, clearto) ;
         clearcache(n->se, depth, clearto) ;
         if (n->res)
            clearcache(n->res, depth, clearto) ;
      }
      if (depth >= clearto)
         n->res = 0 ;
   }
}
/*
 *   Mark the nodes we need to clear the result from.
 */
void ghashbase::clearcache_p1(ghnode *n, int depth, int clearto) {
   if (depth < clearto || marked(n))
      return ;
   mark(n) ;
   if (depth > clearto) {
      depth-- ;
      poller->poll() ;
      clearcache_p1(n->nw, depth, clearto) ;
      clearcache_p1(n->ne, depth, clearto) ;
      clearcache_p1(n->sw, depth, clearto) ;
      clearcache_p1(n->se, depth, clearto) ;
      if (n->res)
         clearcache_p1(n->res, depth, clearto) ;
   }
}
/*
 *   Unmark the nodes and clear the cached result.
 */
void ghashbase::clearcache_p2(ghnode *n, int depth, int clearto) {
   if (depth < clearto || !marked(n))
      return ;
   clearmark(n) ;
   if (depth > clearto) {
      depth-- ;
      poller->poll() ;
      clearcache_p2(n->nw, depth, clearto) ;
      clearcache_p2(n->ne, depth, clearto) ;
      clearcache_p2(n->sw, depth, clearto) ;
      clearcache_p2(n->se, depth, clearto) ;
      if (n->res)
         clearcache_p2(n->res, depth, clearto) ;
   }
   if (n->res)
      n->res = 0 ;
}
/*
 *   Clear the entire cache of everything, and recalculate all leaves.
 *   This can be very expensive.
 */
void ghashbase::clearcache() {
   cacheinvalid = 1 ;
}
/*
 *   Change the ngens value.  Requires us to walk the hash, clearing
 *   the cache fields of any ghnodes that do not have the appropriate
 *   values.
 */
void ghashbase::new_ngens(int newval) {
   g_uintptr_t i ;
   ghnode *p, *pp ;
   int clearto = ngens ;
   if (newval > ngens && halvesdone == 0) {
      ngens = newval ;
      return ;
   }
#ifndef NOGCBEFOREINC
   do_gc(0) ;
#endif
   if (verbose) {
     strcpy(statusline, "Changing increment...") ;
     lifestatus(statusline) ;
   }
   if (newval < clearto)
      clearto = newval ;
   clearto++ ; /* clear this depth and above */
   if (clearto < 1)
      clearto = 1 ;
   ngens = newval ;
   inGC = 1 ;
   for (i=0; i<hashprime; i++)
      for (p=hashtab[i]; p; p=clearmarkbit(p->next))
         if (is_ghnode(p) && !marked(p))
            clearcache(p, ghnode_depth(p), clearto) ;
   for (p=ghnodeblocks; p; p=p->next) {
      poller->poll() ;
      for (pp=p+1, i=1; i<((g_uintptr_t)(p->res))+1; i++, pp++)
         clearmark(pp) ;
   }
   halvesdone = 0 ;
   inGC = 0 ;
   if (needPop) {
      calcPopulation() ;
      popValid = 1 ;
      needPop = 0 ;
      poller->updatePop() ;
   }
   if (verbose) {
     strcpy(statusline+strlen(statusline), " done.") ;
     lifestatus(statusline) ;
   }
}
/*
 *   Return log2.
 */
int ghashbase::log2(unsigned int n) {
   int r = 0 ;
   while ((n & 1) == 0) {
      n >>= 1 ;
      r++ ;
   }
   if (n != 1) {
      lifefatal("Expected power of two!") ;
   }
   return r ;
}
static bigint negone = -1 ;
const bigint &ghashbase::getPopulation() {
   // note:  if called during gc, then we cannot call calcPopulation
   // since that will mess up the gc.
   if (!popValid) {
      if (inGC) {
        needPop = 1 ;
        return negone ;
      } else if (poller->isCalculating()) {
        // AKT: avoid calling poller->bailIfCalculating
        return negone ;
      } else {
        calcPopulation() ;
        popValid = 1 ;
        needPop = 0 ;
      }
   }
   return population ;
}
/*
 *   Finally, we get to run the pattern.  We first ensure that all
 *   clearspace ghnodes and the input pattern is never garbage
 *   collected; we turn on garbage collection, and then we invoke our
 *   magic top-level routine passing in clearspace borders that are
 *   guaranteed large enough.
 */
ghnode *ghashbase::runpattern() {
   ghnode *n = root ;
   save(root) ; // do this in case we interrupt generation
   ensure_hashed() ;
   okaytogc = 1 ;
   if (cacheinvalid) {
      do_gc(1) ; // invalidate the entire cache and recalc leaves
      cacheinvalid = 0 ;
   }
   int depth = ghnode_depth(n) ;
   ghnode *n2 ;
   n = pushroot(n) ;
   depth++ ;
   n = pushroot(n) ;
   depth++ ;
   while (ngens + 2 > depth) {
      n = pushroot(n) ;
      depth++ ;
   }
   save(zeroghnode(nzeros-1)) ;
   save(n) ;
   n2 = getres(n, depth) ;
   okaytogc = 0 ;
   clearstack() ;
   if (halvesdone == 1 && n->res != 0) {
      n->res = 0 ;
      halvesdone = 0 ;
   }
   if (poller->isInterrupted() || softinterrupt)
      return 0 ; // indicate it was interrupted
   n = popzeros(n2) ;
   generation += pow2step ;
   return n ;
}
const char *ghashbase::readmacrocell(char *line) {
   int n=0 ;
   g_uintptr_t i=1, nw=0, ne=0, sw=0, se=0, indlen=0 ;
   int r, d ;
   ghnode **ind = 0 ;
   root = 0 ;
   while (getline(line, 10000)) {
      if (i >= indlen) {
         g_uintptr_t nlen = i + indlen + 10 ;
         ind = (ghnode **)realloc(ind, sizeof(ghnode*) * nlen) ;
         if (ind == 0)
           lifefatal("Out of memory (4).") ;
         while (indlen < nlen)
            ind[indlen++] = 0 ;
      }
      if (line[0] == '#') {
         char *p, *pp ;
         const char *err ;
         switch (line[1]) {
         case 'R':
            p = line + 2 ;
            while (*p && *p <= ' ') p++ ;
            pp = p ;
            while (*pp > ' ') pp++ ;
            *pp = 0 ;
            err = setrule(p);
            if (err) return err;
            break ;

         case 'G':
            p = line + 2 ;
            while (*p && *p <= ' ') p++ ;
            pp = p ;
            while (*pp >= '0' && *pp <= '9') pp++ ;
            *pp = 0 ;
            generation = bigint(p) ;
            break ;
	    // either:
	    //   #FRAMES count base inc
	    // or
	    //   #FRAME index node
	 case 'F':
	    if (strncmp(line, "#FRAMES ", 8) == 0) {
	       p = line + 8 ;
	       while (*p && *p <= ' ')
		 p++ ;
	       long cnt = atol(p) ;
	       if (cnt < 0 || cnt > MAX_FRAME_COUNT)
		  return "Bad FRAMES line" ;
	       destroytimeline() ;
	       while ('0' <= *p && *p <= '9')
		 p++ ;
	       while (*p && *p <= ' ')
		 p++ ;
	       pp = p ;
	       while ((*pp >= '0' && *pp <= '9') || *pp == ',') pp++ ;
	       if (*pp == 0)
		  return "Bad FRAMES line" ;
	       *pp = 0 ;
	       timeline.start = bigint(p) ;
	       timeline.end = timeline.start ;
	       timeline.next = timeline.end ;
	       p = pp + 1 ;
	       while (*p && *p <= ' ')
		 p++ ;
	       pp = p ;
	       while (*pp > ' ')
                  pp++ ;
	       *pp = 0 ;
               if (strchr(p, '^')) {
                  int tbase=0, texpo=0 ;
                  if (sscanf(p, "%d^%d", &tbase, &texpo) != 2 ||
                      tbase < 2 || texpo < 0)
                     return "Bad FRAMES line" ;
                  timeline.base = tbase ;
                  timeline.expo = texpo ;
                  timeline.inc = 1 ;
                  while (texpo--)
                     timeline.inc.mul_smallint(tbase) ;
               } else {
                  timeline.inc = bigint(p) ;
                  // if it's a power of two, we're good
                  int texpo = timeline.inc.lowbitset() ;
                  int tbase = 2 ;
                  bigint test = 1 ;
                  for (int i=0; i<texpo; i++)
                     test += test ;
                  if (test != timeline.inc)
                     return "Bad increment (missing ^) in FRAMES" ;
                  timeline.base = tbase ;
                  timeline.expo = texpo ;
               }
	    } else if (strncmp(line, "#FRAME ", 7) == 0) {
	       int frameind = 0 ;
	       g_uintptr_t nodeind = 0 ;
	       n = sscanf(line+7, "%d %" PRIuPTR, &frameind, &nodeind) ;
	       if (n != 2 || frameind > MAX_FRAME_COUNT || frameind < 0 ||
		   nodeind > i || timeline.framecount != frameind)
		  return "Bad FRAME line" ;
	       timeline.frames.push_back(ind[nodeind]) ;
	       timeline.framecount++ ;
	       timeline.end = timeline.next ;
	       timeline.next += timeline.inc ;
	    }
	    break ;
         }
      } else {
         n = sscanf(line, "%d %" PRIuPTR " %" PRIuPTR " %" PRIuPTR " %" PRIuPTR " %d", &d, &nw, &ne, &sw, &se, &r) ;
         if (n < 0) // blank line; permit
            continue ;
         if (n == 0) {
            // conversion error in first argument; we allow only if the only
            // content on the line is whitespace.
            char *ws = line ;
            while (*ws && *ws <= ' ')
               ws++ ;
            if (*ws > 0)
               return "Parse error in macrocell format." ;
            continue ;
         }
         if (n < 5)
            // best not to use lifefatal here because user won't see any
            // error message when reading clipboard data starting with "[..."
            return "Parse error in readmacrocell." ;
         if (d < 1)
            return "Oops; bad depth in readmacrocell." ;
         if (d == 1) {
           if (nw >= (g_uintptr_t)maxCellStates || ne >= (g_uintptr_t)maxCellStates ||
               sw >= (g_uintptr_t)maxCellStates || se >= (g_uintptr_t)maxCellStates)
              return "Cell state values too high for this algorithm." ;
           root = ind[i++] = (ghnode *)find_ghleaf((state)nw, (state)ne,
                                                   (state)sw, (state)se) ;
           depth = d - 1 ;
         } else {
           ind[0] = zeroghnode(d-2) ; /* allow zeros to work right */
           if (nw >= i || ind[nw] == 0 || ne >= i || ind[ne] == 0 ||
               sw >= i || ind[sw] == 0 || se >= i || ind[se] == 0) {
             return "Node out of range in readmacrocell." ;
           }
           clearstack() ;
           root = ind[i++] = find_ghnode(ind[nw], ind[ne], ind[sw], ind[se]) ;
           depth = d - 1 ;
         }
      }
   }
   if (ind)
      free(ind) ;
   if (root == 0) {
      // allow empty macrocell pattern; note that endofpattern()
      // will be called soon so don't set hashed here
      // return "Invalid macrocell file: no ghnodes." ;
      return 0 ;
   }
   hashed = 1 ;
   return 0 ;
}
const char *ghashbase::setrule(const char *) {
   poller->bailIfCalculating() ;
   clearcache() ;
   return 0 ;
}
/**
 *   Write out the native macrocell format.  This is the one we use when
 *   we're not interactive and displaying a progress dialog.
 */
g_uintptr_t ghashbase::writecell(std::ostream &os, ghnode *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zeroghnode(depth))
      return 0 ;
   if (depth == 0) {
      if (root->nw != 0)
         return (g_uintptr_t)(root->nw) ;
   } else {
      if (marked2(root))
         return (g_uintptr_t)(root->next) ;
      unhash_ghnode2(root) ;
      mark2(root) ;
   }
   thiscell = ++cellcounter ;
   if (depth == 0) {
      ghleaf *n = (ghleaf *)root ;
      root->nw = (ghnode *)thiscell ;
      os << 1 << ' ' << int(n->nw) << ' ' << int(n->ne)
              << ' ' << int(n->sw) << ' ' << int(n->se) << '\n' ;
   } else {
      g_uintptr_t nw = writecell(os, root->nw, depth-1) ;
      g_uintptr_t ne = writecell(os, root->ne, depth-1) ;
      g_uintptr_t sw = writecell(os, root->sw, depth-1) ;
      g_uintptr_t se = writecell(os, root->se, depth-1) ;
      root->next = (ghnode *)thiscell ;
      os << depth+1 << ' ' << nw << ' ' << ne
                    << ' ' << sw << ' ' << se << '\n' ;
   }
   return thiscell ;
}
/**
 *   This new two-pass method works by doing a prepass that numbers the
 *   ghnodes and counts the number of ghnodes that should be sent, so we can
 *   display an accurate progress dialog.
 */
g_uintptr_t ghashbase::writecell_2p1(ghnode *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zeroghnode(depth))
      return 0 ;
   if (depth == 0) {
      if (root->nw != 0)
         return (g_uintptr_t)(root->nw) ;
   } else {
      if (marked2(root))
         return (g_uintptr_t)(root->next) ;
      unhash_ghnode2(root) ;
      mark2(root) ;
   }
   if (depth == 0) {
      thiscell = ++cellcounter ;
      // note:  we *must* not abort this prescan
      if ((cellcounter & 4095) == 0)
         lifeabortprogress(0, "Scanning tree") ;
      root->nw = (ghnode *)thiscell ;
   } else {
      writecell_2p1(root->nw, depth-1) ;
      writecell_2p1(root->ne, depth-1) ;
      writecell_2p1(root->sw, depth-1) ;
      writecell_2p1(root->se, depth-1) ;
      thiscell = ++cellcounter ;
      // note:  we *must* not abort this prescan
      if ((cellcounter & 4095) == 0)
         lifeabortprogress(0, "Scanning tree") ;
      root->next = (ghnode *)thiscell ;
   }
   return thiscell ;
}
/**
 *   This one writes the cells, but assuming they've already been
 *   numbered, and displaying a progress dialog.
 */
static char progressmsg[80] ;
g_uintptr_t ghashbase::writecell_2p2(std::ostream &os, ghnode *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zeroghnode(depth))
      return 0 ;
   if (depth == 0) {
      if (cellcounter + 1 != (g_uintptr_t)(root->nw))
         return (g_uintptr_t)(root->nw) ;
      thiscell = ++cellcounter ;
      if ((cellcounter & 4095) == 0) {
         std::streampos siz = os.tellp() ;
         sprintf(progressmsg, "File size: %.2f MB", double(siz) / 1048576.0) ;
         lifeabortprogress(thiscell/(double)writecells, progressmsg) ;
      }
      ghleaf *n = (ghleaf *)root ;
      root->nw = (ghnode *)thiscell ;
      os << 1 << ' ' << int(n->nw) << ' ' << int(n->ne)
              << ' ' << int(n->sw) << ' ' << int(n->se) << '\n';
   } else {
      if (cellcounter + 1 > (g_uintptr_t)(root->next) || isaborted())
         return (g_uintptr_t)(root->next) ;
      g_uintptr_t nw = writecell_2p2(os, root->nw, depth-1) ;
      g_uintptr_t ne = writecell_2p2(os, root->ne, depth-1) ;
      g_uintptr_t sw = writecell_2p2(os, root->sw, depth-1) ;
      g_uintptr_t se = writecell_2p2(os, root->se, depth-1) ;
      if (!isaborted() &&
          cellcounter + 1 != (g_uintptr_t)(root->next)) { // this should never happen
         lifefatal("Internal in writecell_2p2") ;
         return (g_uintptr_t)(root->next) ;
      }
      thiscell = ++cellcounter ;
      if ((cellcounter & 4095) == 0) {
         std::streampos siz = os.tellp() ;
         sprintf(progressmsg, "File size: %.2f MB", double(siz) / 1048576.0) ;
         lifeabortprogress(thiscell/(double)writecells, progressmsg) ;
      }
      root->next = (ghnode *)thiscell ;
      os << depth+1 << ' ' << nw << ' ' << ne
                    << ' ' << sw << ' ' << se << '\n' ;
   }
   return thiscell ;
}
#define STRINGIFY(arg) STR2(arg)
#define STR2(arg) #arg
const char *ghashbase::writeNativeFormat(std::ostream &os, char *comments) {
   int depth = ghnode_depth(root) ;
   os << "[M2] (golly " STRINGIFY(VERSION) ")\n" ;
   
   // AKT: always write out explicit rule
   os << "#R " << getrule() << '\n' ;

   if (generation > bigint::zero) {
      // write non-zero gen count
      os << "#G " << generation.tostring('\0') << '\n' ;
   }

    if (comments) {
        // write given comment line(s), but we can't just do "os << comments" because the
        // lines might not start with #C (eg. if they came from the end of a .rle file),
        // so we must ensure that each comment line in the .mc file starts with #C
        char *p = comments;
        while (*p != '\0') {
            char *line = p;
            // note that readcomments() in readpattern.cpp ensures each line ends with \n
            while (*p != '\n') p++;
            if (line[0] != '#' || line[1] != 'C') {
                os << "#C ";
            }
            if (line != p) {
                *p = '\0';
                os << line;
                *p = '\n';
            }
            os << '\n';
            p++;
        }
    }

   inGC = 1 ;
   /* this is the old way:
   cellcounter = 0 ;
   writecell(os, root, depth) ;
   */
   /* this is the new two-pass way */
   cellcounter = 0 ;
   vector<int> depths(timeline.framecount) ;
   int framestosave = timeline.framecount ;
   if (timeline.savetimeline == 0)
     framestosave = 0 ;
   if (framestosave) {
     for (int i=0; i<timeline.framecount; i++) {
       ghnode *frame = (ghnode*)timeline.frames[i] ;
       depths[i] = ghnode_depth(frame) ;
     }
     for (int i=0; i<timeline.framecount; i++) {
       ghnode *frame = (ghnode*)timeline.frames[i] ;
       writecell_2p1(frame, depths[i]) ;
     }
   }
   writecell_2p1(root, depth) ;
   writecells = cellcounter ;
   cellcounter = 0 ;
   if (framestosave) {
      os << "#FRAMES"
         << ' ' << timeline.framecount
         << ' ' << timeline.start.tostring()
         << ' ' << timeline.base << '^' << timeline.expo << '\n' ;
      for (int i=0; i<timeline.framecount; i++) {
         ghnode *frame = (ghnode*)timeline.frames[i] ;
         writecell_2p2(os, frame, depths[i]) ;
         os << "#FRAME " << i << ' ' << (g_uintptr_t)frame->next << '\n' ;
      }
   }
   writecell_2p2(os, root, depth) ;
   /* end new two-pass way */
   if (framestosave) {
     for (int i=0; i<timeline.framecount; i++) {
       ghnode *frame = (ghnode*)timeline.frames[i] ;
       afterwritemc(frame, depths[i]) ;
     }
   }
   afterwritemc(root, depth) ;
   inGC = 0 ;
   return 0 ;
}
char ghashbase::statusline[120] ;
void ghashbase::doInitializeAlgoInfo(staticAlgoInfo &ai) {
   ai.setDefaultBaseStep(8) ;
   ai.setDefaultMaxMem(500) ; // MB
}

// This file is part of Golly.
// See docs/License.html for the copyright notice.

/*
 * hlife 0.99 by Radical Eye Software.
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
#include "hlifealgo.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
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
 *   Note that all the places we represent 4-squares by short, we use
 *   unsigned shorts; this is so we can directly index into these arrays.
 */
static unsigned char shortpop[65536] ;
/*
 *   The cached result of an 8-square is a new 4-square representing
 *   two generations into the future.  This subroutine calculates that
 *   future, assuming ruletable is calculated (see below).  The code
 *   that it uses is similar to code you'll see again, so we explain
 *   what's going on in some detail.
 *
 *   Each time we build a leaf node, we compute the result, because it
 *   is reasonably quick.
 *
 *   The first generation result of an 8-square is a 6-square, which
 *   we represent as nine 2-squares.  The nine 2-squares are called
 *   t00 through t22, and are arranged in a matrix:
 *
 *      t00   t01   t02
 *      t10   t11   t12
 *      t20   t21   t22
 *
 *   To compute each of these, we need to extract the relevant bits
 *   from the four 4-square values n->nw, n->ne, n->sw, and n->ne.
 *   We can use these values to directly index into the ruletable
 *   array.
 *
 *   Then, given the nine values, we can compute a resulting 4-square
 *   by computing four 2-square results, and combining these into a
 *   single 4-square.
 *
 *   It's a bit intricate, but it's not really overwhelming.
 */
#define combine9(t00,t01,t02,t10,t11,t12,t20,t21,t22) \
       ((t00) << 15) | ((t01) << 13) | (((t02) << 11) & 0x1000) | \
       (((t10) << 7) & 0x880) | ((t11) << 5) | (((t12) << 3) & 0x110) | \
       (((t20) >> 1) & 0x8) | ((t21) >> 3) | ((t22) >> 5)
void hlifealgo::leafres(leaf *n) {
   unsigned short
   t00 = ruletable[n->nw],
   t01 = ruletable[((n->nw << 2) & 0xcccc) | ((n->ne >> 2) & 0x3333)],
   t02 = ruletable[n->ne],
   t10 = ruletable[((n->nw << 8) & 0xff00) | ((n->sw >> 8) & 0x00ff)],
   t11 = ruletable[((n->nw << 10) & 0xcc00) | ((n->ne << 6) & 0x3300) |
                   ((n->sw >> 6) & 0x00cc) | ((n->se >> 10) & 0x0033)],
   t12 = ruletable[((n->ne << 8) & 0xff00) | ((n->se >> 8) & 0x00ff)],
   t20 = ruletable[n->sw],
   t21 = ruletable[((n->sw << 2) & 0xcccc) | ((n->se >> 2) & 0x3333)],
   t22 = ruletable[n->se] ;
   n->res1 = combine9(t00,t01,t02,t10,t11,t12,t20,t21,t22) ;
   n->res2 =
   (ruletable[(t00 << 10) | (t01 << 8) | (t10 << 2) | t11] << 10) |
   (ruletable[(t01 << 10) | (t02 << 8) | (t11 << 2) | t12] << 8) |
   (ruletable[(t10 << 10) | (t11 << 8) | (t20 << 2) | t21] << 2) |
    ruletable[(t11 << 10) | (t12 << 8) | (t21 << 2) | t22] ;
   n->leafpop = bigint((short)(shortpop[n->nw] + shortpop[n->ne] +
                               shortpop[n->sw] + shortpop[n->se])) ;
}
/*
 *   We do now support garbage collection, but there are some routines we
 *   call frequently to help us.
 */
#ifdef PRIMEMOD
#define node_hash(a,b,c,d) (65537*(g_uintptr_t)(d)+257*(g_uintptr_t)(c)+17*(g_uintptr_t)(b)+5*(g_uintptr_t)(a))
#else
g_uintptr_t node_hash(void *a, void *b, void *c, void *d) {
   g_uintptr_t r = (65537*(g_uintptr_t)(d)+257*(g_uintptr_t)(c)+17*(g_uintptr_t)(b)+5*(g_uintptr_t)(a)) ;
   r += (r >> 11) ;
   return r ;
}
#endif
#define leaf_hash(a,b,c,d) (65537*(d)+257*(c)+17*(b)+5*(a))
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
double hlifealgo::maxloadfactor = 0.7 ;
void hlifealgo::resize() {
#ifndef NOGCBEFORERESIZE
   if (okaytogc) {
      do_gc(0) ; // faster resizes if we do a gc first
   }
#endif
   g_uintptr_t i, nhashprime = nexthashsize(2 * hashprime) ;
   node *p, **nhashtab ;
   if (hashprime > (totalthings >> 2)) {
      if (alloced > maxmem ||
          nhashprime * sizeof(node *) > (maxmem - alloced)) {
         hashlimit = G_MAX ;
         return ;
      }
   }
   if (verbose) {
     sprintf(statusline, "Resizing hash to %" PRIuPTR "...", nhashprime) ;
     lifestatus(statusline) ;
   }
   nhashtab = (node **)calloc(nhashprime, sizeof(node *)) ;
   if (nhashtab == 0) {
     lifewarning("Out of memory; running in a somewhat slower mode; "
                 "try reducing the hash memory limit after restarting.") ;
     hashlimit = G_MAX ;
     return ;
   }
   alloced += sizeof(node *) * nhashprime ;
   g_uintptr_t ohashprime = hashprime ;
   hashprime = nhashprime ;
#ifndef PRIMEMOD
   hashmask = hashprime - 1 ;
#endif
   for (i=0; i<ohashprime; i++) {
      for (p=hashtab[i]; p;) {
         node *np = p->next ;
         g_uintptr_t h ;
         if (is_node(p)) {
            h = node_hash(p->nw, p->ne, p->sw, p->se) ;
         } else {
            leaf *l = (leaf *)p ;
            h = leaf_hash(l->nw, l->ne, l->sw, l->se) ;
         }
         h = HASHMOD(h) ;
         p->next = nhashtab[h] ;
         nhashtab[h] = p ;
         p = np ;
      }
   }
   repurpose(hashtab, ohashprime * sizeof(node*)) ;
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
 *   new node and store it in the hash table, and return that.
 */
node *hlifealgo::find_node(node *nw, node *ne, node *sw, node *se) {
   node *p ;
   g_uintptr_t h = node_hash(nw,ne,sw,se) ;
   node *pred = 0 ;
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
   p = newnode() ;
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
leaf *hlifealgo::find_leaf(unsigned short nw, unsigned short ne,
                                  unsigned short sw, unsigned short se) {
   leaf *p ;
   leaf *pred = 0 ;
   g_uintptr_t h = leaf_hash(nw, ne, sw, se) ;
   h = HASHMOD(h) ;
   for (p=(leaf *)hashtab[h]; p; p = (leaf *)p->next) {
      if (nw == p->nw && ne == p->ne && sw == p->sw && se == p->se &&
          !is_node(p)) {
         if (pred) {
            pred->next = p->next ;
            p->next = hashtab[h] ;
            hashtab[h] = (node *)p ;
         }
         return (leaf *)save((node *)p) ;
      }
      pred = p ;
   }
   p = newleaf() ;
   p->nw = nw ;
   p->ne = ne ;
   p->sw = sw ;
   p->se = se ;
   leafres(p) ;
   p->isnode = 0 ;
   p->next = hashtab[h] ;
   hashtab[h] = (node *)p ;
   hashpop++ ;
   save((node *)p) ;
   if (hashpop > hashlimit)
      resize() ;
   return p ;
}
/*
 *   The following routine does the same, but first it checks to see if
 *   the cached result is any good.  If it is, it directly returns that.
 *   Otherwise, it figures out whether to call the leaf routine or the
 *   non-leaf routine by whether two nodes down is a leaf node or not.
 *   (We'll understand why this is a bit later.)  All the sp stuff is
 *   stack pointer and garbage collection stuff.
 */
node *hlifealgo::getres(node *n, int depth) {
   if (n->res)
     return n->res ;
   node *res = 0 ;
   /**
    *   This routine be the only place we assign to res.  We use
    *   the fact that the poll routine is *sticky* to allow us to
    *   manage unwinding the stack without munging our data
    *   structures.  Note that there may be many find_nodes
    *   and getres called before we finally actually exit from
    *   here, because the stack is deep and we don't want to
    *   put checks throughout the code.  Instead we need two
    *   calls here, one to prevent us going deeper, and another
    *   to prevent us from destroying the cache field.
    */
   if (poller->poll() || softinterrupt)
     return zeronode(depth-1) ;
   int sp = gsp ;
   if (running_hperf.fastinc(depth, ngens < depth))
      running_hperf.report(inc_hperf, verbose) ;
   depth-- ;
   if (ngens >= depth) {
     if (is_node(n->nw)) {
       res = dorecurs(n->nw, n->ne, n->sw, n->se, depth) ;
     } else {
       res = (node *)dorecurs_leaf((leaf *)n->nw, (leaf *)n->ne,
                                   (leaf *)n->sw, (leaf *)n->se) ;
     }
   } else {
     if (is_node(n->nw)) {
       res = dorecurs_half(n->nw, n->ne, n->sw, n->se, depth) ;
     } else if (ngens == 0) {
       res = (node *)dorecurs_leaf_quarter((leaf *)n->nw, (leaf *)n->ne,
                                           (leaf *)n->sw, (leaf *)n->se) ;
     } else {
       res = (node *)dorecurs_leaf_half((leaf *)n->nw, (leaf *)n->ne,
                                        (leaf *)n->sw, (leaf *)n->se) ;
     }
   }
   pop(sp) ;
   if (softinterrupt ||
       poller->isInterrupted()) // don't assign this to the cache field!
     res = zeronode(depth) ;
   else {
     if (ngens < depth && halvesdone < 1000)
       halvesdone++ ;
     n->res = res ;
   }
   return res ;
}
#ifdef USEPREFETCH
void hlifealgo::setupprefetch(setup_t &su, node *nw, node *ne, node *sw, node *se) {
   su.h = node_hash(nw,ne,sw,se) ;
   su.nw = nw ;
   su.ne = ne ;
   su.sw = sw ;
   su.se = se ;
   su.prefetch(hashtab + HASHMOD(su.h)) ;
}
node *hlifealgo::find_node(setup_t &su) {
   node *p ;
   node *pred = 0 ;
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
   p = newnode() ;
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
node *hlifealgo::dorecurs(node *n, node *ne, node *t, node *e, int depth) {
   int sp = gsp ;
   setup_t su[5] ;
   setupprefetch(su[2], n->se, ne->sw, t->ne, e->nw) ;
   setupprefetch(su[0], n->ne, ne->nw, n->se, ne->sw) ;
   setupprefetch(su[1], ne->sw, ne->se, e->nw, e->ne) ;
   setupprefetch(su[3], n->sw, n->se, t->nw, t->ne) ;
   setupprefetch(su[4], t->ne, e->nw, t->se, e->sw) ;
   node
   *t00 = getres(n, depth),
   *t01 = getres(find_node(su[0]), depth),
   *t02 = getres(ne, depth),
   *t12 = getres(find_node(su[1]), depth),
   *t11 = getres(find_node(su[2]), depth),
   *t10 = getres(find_node(su[3]), depth),
   *t20 = getres(t, depth),
   *t21 = getres(find_node(su[4]), depth),
   *t22 = getres(e, depth) ;
   setupprefetch(su[0], t11, t12, t21, t22) ;
   setupprefetch(su[1], t10, t11, t20, t21) ;
   setupprefetch(su[2], t00, t01, t10, t11) ;
   setupprefetch(su[3], t01, t02, t11, t12) ;
   node
   *t44 = getres(find_node(su[0]), depth),
   *t43 = getres(find_node(su[1]), depth),
   *t33 = getres(find_node(su[2]), depth),
   *t34 = getres(find_node(su[3]), depth) ;
   n = find_node(t33, t34, t43, t44) ;
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
 *   This routine works exactly the same as the leafres() routine, only
 *   instead of working on an 8-square, we're working on an n-square,
 *   returning an n/2-square, and we build that n/2-square by first building
 *   9 n/4-squares, use those to calculate 4 more n/4-squares, and
 *   then put these together into a new n/2-square.  Simple, eh?
 */
node *hlifealgo::dorecurs(node *n, node *ne, node *t, node *e, int depth) {
   int sp = gsp ;
   node
   *t11 = getres(find_node(n->se, ne->sw, t->ne, e->nw), depth),
   *t00 = getres(n, depth),
   *t01 = getres(find_node(n->ne, ne->nw, n->se, ne->sw), depth),
   *t02 = getres(ne, depth),
   *t12 = getres(find_node(ne->sw, ne->se, e->nw, e->ne), depth),
   *t10 = getres(find_node(n->sw, n->se, t->nw, t->ne), depth),
   *t20 = getres(t, depth),
   *t21 = getres(find_node(t->ne, e->nw, t->se, e->sw), depth),
   *t22 = getres(e, depth),
   *t44 = getres(find_node(t11, t12, t21, t22), depth),
   *t43 = getres(find_node(t10, t11, t20, t21), depth),
   *t33 = getres(find_node(t00, t01, t10, t11), depth),
   *t34 = getres(find_node(t01, t02, t11, t12), depth) ;
   n = find_node(t33, t34, t43, t44) ;
   pop(sp) ;
   return save(n) ;
}
#endif
/*
 *   Same as above, but we only do one step instead of 2.
 */
node *hlifealgo::dorecurs_half(node *n, node *ne, node *t,
                               node *e, int depth) {
   int sp = gsp ;
   node
   *t00 = getres(n, depth),
   *t01 = getres(find_node(n->ne, ne->nw, n->se, ne->sw), depth),
   *t10 = getres(find_node(n->sw, n->se, t->nw, t->ne), depth),
   *t11 = getres(find_node(n->se, ne->sw, t->ne, e->nw), depth),
   *t02 = getres(ne, depth),
   *t12 = getres(find_node(ne->sw, ne->se, e->nw, e->ne), depth),
   *t20 = getres(t, depth),
   *t21 = getres(find_node(t->ne, e->nw, t->se, e->sw), depth),
   *t22 = getres(e, depth) ;
   if (depth > 3) {
      n = find_node(find_node(t00->se, t01->sw, t10->ne, t11->nw),
                    find_node(t01->se, t02->sw, t11->ne, t12->nw),
                    find_node(t10->se, t11->sw, t20->ne, t21->nw),
                    find_node(t11->se, t12->sw, t21->ne, t22->nw)) ;
   } else {
      n = find_node((node *)find_leaf(((leaf *)t00)->se,
                                             ((leaf *)t01)->sw,
                                             ((leaf *)t10)->ne,
                                             ((leaf *)t11)->nw),
                    (node *)find_leaf(((leaf *)t01)->se,
                                             ((leaf *)t02)->sw,
                                             ((leaf *)t11)->ne,
                                             ((leaf *)t12)->nw),
                    (node *)find_leaf(((leaf *)t10)->se,
                                             ((leaf *)t11)->sw,
                                             ((leaf *)t20)->ne,
                                             ((leaf *)t21)->nw),
                    (node *)find_leaf(((leaf *)t11)->se,
                                             ((leaf *)t12)->sw,
                                             ((leaf *)t21)->ne,
                                             ((leaf *)t22)->nw)) ;
   }
   pop(sp) ;
   return save(n) ;
}
/*
 *   If the node is a 16-node, then the constituents are leaves, so we
 *   need a very similar but still somewhat different subroutine.  Since
 *   we do not (yet) garbage collect leaves, we don't need all that
 *   save/pop mumbo-jumbo.
 */
leaf *hlifealgo::dorecurs_leaf(leaf *n, leaf *ne, leaf *t, leaf *e) {
   unsigned short
   t00 = n->res2,
   t01 = find_leaf(n->ne, ne->nw, n->se, ne->sw)->res2,
   t02 = ne->res2,
   t10 = find_leaf(n->sw, n->se, t->nw, t->ne)->res2,
   t11 = find_leaf(n->se, ne->sw, t->ne, e->nw)->res2,
   t12 = find_leaf(ne->sw, ne->se, e->nw, e->ne)->res2,
   t20 = t->res2,
   t21 = find_leaf(t->ne, e->nw, t->se, e->sw)->res2,
   t22 = e->res2 ;
   return find_leaf(find_leaf(t00, t01, t10, t11)->res2,
                    find_leaf(t01, t02, t11, t12)->res2,
                    find_leaf(t10, t11, t20, t21)->res2,
                    find_leaf(t11, t12, t21, t22)->res2) ;
}
/*
 *   Same as above but we only do two generations.
 */
#define combine4(t00,t01,t10,t11) (unsigned short)\
((((t00)<<10)&0xcc00)|(((t01)<<6)&0x3300)|(((t10)>>6)&0xcc)|(((t11)>>10)&0x33))
leaf *hlifealgo::dorecurs_leaf_half(leaf *n, leaf *ne, leaf *t, leaf *e) {
   unsigned short
   t00 = n->res2,
   t01 = find_leaf(n->ne, ne->nw, n->se, ne->sw)->res2,
   t02 = ne->res2,
   t10 = find_leaf(n->sw, n->se, t->nw, t->ne)->res2,
   t11 = find_leaf(n->se, ne->sw, t->ne, e->nw)->res2,
   t12 = find_leaf(ne->sw, ne->se, e->nw, e->ne)->res2,
   t20 = t->res2,
   t21 = find_leaf(t->ne, e->nw, t->se, e->sw)->res2,
   t22 = e->res2 ;
   return find_leaf(combine4(t00, t01, t10, t11),
                    combine4(t01, t02, t11, t12),
                    combine4(t10, t11, t20, t21),
                    combine4(t11, t12, t21, t22)) ;
}
/*
 *   Same as above but we only do one generation.
 */
leaf *hlifealgo::dorecurs_leaf_quarter(leaf *n, leaf *ne,
                                   leaf *t, leaf *e) {
   unsigned short
   t00 = n->res1,
   t01 = find_leaf(n->ne, ne->nw, n->se, ne->sw)->res1,
   t02 = ne->res1,
   t10 = find_leaf(n->sw, n->se, t->nw, t->ne)->res1,
   t11 = find_leaf(n->se, ne->sw, t->ne, e->nw)->res1,
   t12 = find_leaf(ne->sw, ne->se, e->nw, e->ne)->res1,
   t20 = t->res1,
   t21 = find_leaf(t->ne, e->nw, t->se, e->sw)->res1,
   t22 = e->res1 ;
   return find_leaf(combine4(t00, t01, t10, t11),
                    combine4(t01, t02, t11, t12),
                    combine4(t10, t11, t20, t21),
                    combine4(t11, t12, t21, t22)) ;
}
/*
 *   We keep free nodes in a linked list for allocation, and we allocate
 *   them 1360 at a time.  We used to do 1000, but this was interacting
 *   with certain (MacOS) memory allocators and instead of allocating the
 *   requested 48,000 bytes, they were allocating 65,536 bytes and
 *   wasting a good fraction of the memory available.  Using 1360 gets us
 *   closer to 65536, while still leaving some memory for any headers the
 *   allocator might need.
 */
static const int NODECHUNKCOUNT = 1360 ;
node *hlifealgo::newnode() {
   node *r ;
   if (freenodes == 0) {
      int i ;
      freenodes = (node *)calloc(NODECHUNKCOUNT+1, sizeof(node)) ;
      if (freenodes == 0)
         lifefatal("Out of memory; try reducing the hash memory limit.") ;
      alloced += NODECHUNKCOUNT * sizeof(node) ;
      freenodes->next = nodeblocks ;
      freenodes->res = (struct node *)NODECHUNKCOUNT ;
      nodeblocks = freenodes++ ;
      for (i=0; i<(NODECHUNKCOUNT-1); i++) {
         freenodes[1].next = freenodes ;
         freenodes++ ;
      }
      totalthings += NODECHUNKCOUNT ;
   }
   if (freenodes->next == 0 && alloced + NODECHUNKCOUNT * sizeof(node) > maxmem &&
       okaytogc) {
      do_gc(0) ;
   }
   r = freenodes ;
   freenodes = freenodes->next ;
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
void hlifealgo::repurpose(void *mem, g_uintptr_t bytes) {
   g_uintptr_t nodecount = bytes / sizeof(struct node) ;
   if (nodecount < 2) {
      free(mem) ;
      alloced -= bytes ;
      return ;
   }
   struct node *newblock = (struct node *)mem ;
   newblock->next = nodeblocks ;
   nodeblocks = newblock ;
   nodecount-- ;
   newblock->res = (struct node *)nodecount ;
   newblock++ ;
   g_uintptr_t i ;
   for (i=0; i<nodecount; i++)
      newblock[i].next = newblock + i + 1 ;
   newblock[nodecount-1].next = freenodes ;
   freenodes = newblock ;
   totalthings += nodecount ;
}
/*
 *   Leaves are the same.
 */
leaf *hlifealgo::newleaf() {
   leaf *r = (leaf *)newnode() ;
   new(&(r->leafpop))bigint ;
   return r ;
}
/*
 *   Sometimes we want the new node or leaf to be automatically cleared
 *   for us.
 */
node *hlifealgo::newclearednode() {
   return (node *)memset(newnode(), 0, sizeof(node)) ;
}
leaf *hlifealgo::newclearedleaf() {
   leaf *r = (leaf *)newclearednode() ;
   new(&(r->leafpop))bigint ;
   return r ;
}
hlifealgo::hlifealgo() {
   int i ;
/*
 *   The population of one-bits in an integer is one more than the
 *   population of one-bits in the integer with one fewer bit set,
 *   and we can turn off a bit by anding an integer with the next
 *   lower integer.
 */
   if (shortpop[1] == 0)
      for (i=1; i<65536; i++)
         shortpop[i] = shortpop[i & (i - 1)] + 1 ;
   hashprime = nexthashsize(1000) ;
#ifndef PRIMEMOD
   hashmask = hashprime - 1 ;
#endif
   hashlimit = (g_uintptr_t)(maxloadfactor * hashprime) ;
   hashpop = 0 ;
   hashtab = (node **)calloc(hashprime, sizeof(node *)) ;
   if (hashtab == 0)
     lifefatal("Out of memory (1).") ;
   alloced = hashprime * sizeof(node *) ;
   ngens = 0 ;
   stacksize = 0 ;
   halvesdone = 0 ;
   nzeros = 0 ;
   stack = 0 ;
   gsp = 0 ;
   maxmem = 256 * 1024 * 1024 ;
   freenodes = 0 ;
   okaytogc = 0 ;
   totalthings = 0 ;
   nodeblocks = 0 ;
   zeronodea = 0 ;
   ruletable = hliferules.rule0 ;
/*
 *   We initialize our universe to be a 16-square.  We are in drawing
 *   mode at this point.
 */
   root = (node *)newclearednode() ;
   population = 0 ;
   generation = 0 ;
   increment = 1 ;
   setincrement = 1 ;
   nonpow2 = 1 ;
   pow2step = 1 ;
   llsize = 0 ;
   depth = 3 ;
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
hlifealgo::~hlifealgo() {
   free(hashtab) ;
   while (nodeblocks) {
      node *r = nodeblocks ;
      nodeblocks = nodeblocks->next ;
      free(r) ;
   }
   if (zeronodea)
      free(zeronodea) ;
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
void hlifealgo::setIncrement(bigint inc) {
   if (inc < increment)
      softinterrupt = 1 ;
   increment = inc ;
}
/**
 *   Do a step.
 */
void hlifealgo::step() {
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
         node *newroot = runpattern() ;
         if (newroot == 0 || softinterrupt || poller->isInterrupted()) // we *were* interrupted
            break ;
         popValid = 0 ;
         root = newroot ;
         depth = node_depth(root) ;
      }
      running_hperf.reportStep(step_hperf, inc_hperf, generation.todouble(), verbose) ;
      if (poller->isInterrupted() || !softinterrupt)
         break ;
   }
}
void hlifealgo::setcurrentstate(void *n) {
   if (root != (node *)n) {
      root = (node *)n ;
      depth = node_depth(root) ;
      popValid = 0 ;
   }
}
/*
 *   Set the max memory
 */
void hlifealgo::setMaxMemory(int newmemlimit) {
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
 *   centering.  We use four new nodes, and *reuse* the root so this cannot
 *   be called after we've started hashing.
 */
void hlifealgo::pushroot_1() {
   node *t ;
   t = newclearednode() ;
   t->se = root->nw ;
   root->nw = t ;
   t = newclearednode() ;
   t->sw = root->ne ;
   root->ne = t ;
   t = newclearednode() ;
   t->ne = root->sw ;
   root->sw = t ;
   t = newclearednode() ;
   t->nw = root->se ;
   root->se = t ;
   depth++ ;
}
/*
 *   Return the depth of this node (2 is 8x8).
 */
int hlifealgo::node_depth(node *n) {
   int depth = 2 ;
   while (is_node(n)) {
      depth++ ;
      n = n->nw ;
   }
   return depth ;
}
/*
 *   This routine returns the canonical clear space node at a particular
 *   depth.
 */
node *hlifealgo::zeronode(int depth) {
   while (depth >= nzeros) {
      int nnzeros = 2 * nzeros + 10 ;
      zeronodea = (node **)realloc(zeronodea,
                                          nnzeros * sizeof(node *)) ;
      if (zeronodea == 0)
        lifefatal("Out of memory (2).") ;
      alloced += (nnzeros - nzeros) * sizeof(node *) ;
      while (nzeros < nnzeros)
         zeronodea[nzeros++] = 0 ;
   }
   if (zeronodea[depth] == 0) {
      if (depth == 2) {
         zeronodea[depth] = (node *)find_leaf(0, 0, 0, 0) ;
      } else {
         node *z = zeronode(depth-1) ;
         zeronodea[depth] = find_node(z, z, z, z) ;
      }
   }
   return zeronodea[depth] ;
}
/*
 *   Same, but with hashed nodes.
 */
node *hlifealgo::pushroot(node *n) {
   int depth = node_depth(n) ;
   zeronode(depth+1) ; // ensure enough zero nodes for rendering
   node *z = zeronode(depth-1) ;
   return find_node(find_node(z, z, z, n->nw),
                    find_node(z, z, n->ne, z),
                    find_node(z, n->sw, z, z),
                    find_node(n->se, z, z, z)) ;
}
/* Returns an internal (i.e. non-leaf) node containing the given node. */
node *hlifealgo::make_internal_node(node *n) {
   if (is_node(n)) return n ;
   leaf *l=(leaf *)n ;
   return find_node((node *)find_leaf(0, 0, 0, l->nw),
                    (node *)find_leaf(0, 0, l->ne, 0),
                    (node *)find_leaf(0, l->sw, 0, 0),
                    (node *)find_leaf(l->se, 0, 0, 0));
}
/*
 *   Here is our recursive routine to set a bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.  We allocate new nodes and
 *   leaves on our way down.
 *
 *   Note that at this point our universe lives outside the hash table
 *   and has not been canonicalized, and that many of the pointers in
 *   the nodes can be null.  We'll patch this up in due course.
 */
node *hlifealgo::gsetbit(node *n, int x, int y, int newstate, int depth) {
   if (depth == 2) {
      leaf *l = (leaf *)n ;
      if (hashed) {
         unsigned short nw = l->nw ;
         unsigned short sw = l->sw ;
         unsigned short ne = l->ne ;
         unsigned short se = l->se ;
         if (newstate) {
            if (x < 0)
               if (y < 0)
                  sw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
               else
                  nw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
            else
               if (y < 0)
                  se |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
               else
                  ne |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
         } else {
            if (x < 0)
               if (y < 0)
                  sw &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
               else
                  nw &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
            else
               if (y < 0)
                  se &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
               else
                  ne &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
         }
         return save((node *)find_leaf(nw, ne, sw, se)) ;
      }
      if (newstate) {
         if (x < 0)
            if (y < 0)
               l->sw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
            else
               l->nw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
         else
            if (y < 0)
               l->se |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
            else
               l->ne |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
      } else {
         if (x < 0)
            if (y < 0)
               l->sw &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
            else
               l->nw &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
         else
            if (y < 0)
               l->se &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
            else
               l->ne &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
      }
      return (node *)l ;
   } else {
      unsigned int w = 0, wh = 0 ;
      if (depth >= 32) {
         if (depth == 32)
            wh = 0x80000000 ;
      } else {
         w = 1 << depth ;
         wh = 1 << (depth - 1) ;
      }
      depth-- ;
      node **nptr ;
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
         if (depth == 2)
            *nptr = (node *)newclearedleaf() ;
         else
            *nptr = newclearednode() ;
      }
      node *s = gsetbit(*nptr, (x & (w - 1)) - wh,
                               (y & (w - 1)) - wh, newstate, depth) ;
      if (hashed) {
         node *nw = (nptr == &(n->nw) ? s : n->nw) ;
         node *sw = (nptr == &(n->sw) ? s : n->sw) ;
         node *ne = (nptr == &(n->ne) ? s : n->ne) ;
         node *se = (nptr == &(n->se) ? s : n->se) ;
         n = save(find_node(nw, ne, sw, se)) ;
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
int hlifealgo::getbit(node *n, int x, int y, int depth) {
   struct node tnode ;
   while (depth >= 32) {
      tnode.nw = n->nw->se ;
      tnode.ne = n->ne->sw ;
      tnode.sw = n->sw->ne ;
      tnode.se = n->se->nw ;
      n = &tnode ;
      depth-- ;
   }
   if (depth == 2) {
      leaf *l = (leaf *)n ;
      int test = 0 ;
      if (x < 0)
         if (y < 0)
            test = (l->sw & (1 << (3 - (x & 3) + 4 * (y & 3)))) ;
         else
            test = (l->nw & (1 << (3 - (x & 3) + 4 * (y & 3)))) ;
      else
         if (y < 0)
            test = (l->se & (1 << (3 - (x & 3) + 4 * (y & 3)))) ;
         else
            test = (l->ne & (1 << (3 - (x & 3) + 4 * (y & 3)))) ;
      if (test)
         return 1 ;
      return 0 ;
   } else {
      unsigned int w = 0, wh = 0 ;
      if (depth >= 32) {
         if (depth == 32)
            wh = 0x80000000 ;
      } else {
         w = 1 << depth ;
         wh = 1 << (depth - 1) ;
      }
      depth-- ;
      node *nptr ;
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
      if (nptr == 0 || nptr == zeronode(depth))
         return 0 ;
      return getbit(nptr, (x & (w - 1)) - wh, (y & (w - 1)) - wh, depth) ;
   }
}
/*
 *   Here is our recursive routine to get the next bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.
 */
int hlifealgo::nextbit(node *n, int x, int y, int depth) {
   if (n == 0 || n == zeronode(depth))
      return -1 ;
   if (depth == 2) {
      leaf *l = (leaf *)n ;
      int test = 0 ;
      if (y < 0)
        test = (((l->sw >> (4 * (y & 3))) & 15) << 4) |
                ((l->se >> (4 * (y & 3))) & 15) ;
      else
        test = (((l->nw >> (4 * (y & 3))) & 15) << 4) |
                ((l->ne >> (4 * (y & 3))) & 15) ;
      test &= (1 << (4 - x)) - 1 ;
      if (test) {
        int r = 0 ;
        int b = 1 << (3 - x) ;
        while ((test & b) == 0) {
          r++ ;
          b >>= 1 ;
        }
        return r ;
      }
      return -1 ; // none found
   } else {
      unsigned int w = 0, wh = 0 ;
      w = 1 << depth ;
      wh = 1 << (depth - 1) ;
      node *lft, *rght ;
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
                        (y & (w - 1)) - wh, depth) ;
        if (t >= 0)
          return t ;
        r = -x ;
        x = 0 ;
      }
      int t = nextbit(rght, (x & (w-1)) - wh,
                      (y & (w - 1)) - wh, depth) ;
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
int hlifealgo::setcell(int x, int y, int newstate) {
   if (newstate & ~1)
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
int hlifealgo::getcell(int x, int y) {
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
int hlifealgo::nextcell(int x, int y, int &v) {
   v = 1 ;
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
      struct node tnode = *root ;
      int mdepth = depth ;
      while (mdepth > 30) {
         tnode.nw = tnode.nw->se ;
         tnode.ne = tnode.ne->sw ;
         tnode.sw = tnode.sw->ne ;
         tnode.se = tnode.se->nw ;
         mdepth-- ;
      }
      return nextbit(&tnode, x, y, mdepth) ;
   }
   return nextbit(root, x, y, depth) ;
}
/*
 *   Canonicalize a universe by filling in the null pointers and then
 *   invoking find_node on each node.  Drops the original universe on
 *   the floor [big deal, it's probably small anyway].
 */
node *hlifealgo::hashpattern(node *root, int depth) {
   node *r ;
   if (root == 0) {
      r = zeronode(depth) ;
   } else if (depth == 2) {
      leaf *n = (leaf *)root ;
      r = (node *)find_leaf(n->nw, n->ne, n->sw, n->se) ;
      n->next = freenodes ;
      freenodes = root ;
   } else {
      depth-- ;
      r = find_node(hashpattern(root->nw, depth),
                    hashpattern(root->ne, depth),
                    hashpattern(root->sw, depth),
                    hashpattern(root->se, depth)) ;
      root->next = freenodes ;
      freenodes = root ;
   }
   return r ;
}
void hlifealgo::endofpattern() {
   poller->bailIfCalculating() ;
   if (!hashed) {
      root = hashpattern(root, depth) ;
      zeronode(depth) ;
      hashed = 1 ;
   }
   popValid = 0 ;
   needPop = 0 ;
   inGC = 0 ;
}
void hlifealgo::ensure_hashed() {
   if (!hashed)
      endofpattern() ;
}
/*
 *   Pop off any levels we don't need.
 */
node *hlifealgo::popzeros(node *n) {
   int depth = node_depth(n) ;
   while (depth > 3) {
      node *z = zeronode(depth-2) ;
      if (n->nw->nw == z && n->nw->ne == z && n->nw->sw == z &&
          n->ne->nw == z && n->ne->ne == z && n->ne->se == z &&
          n->sw->nw == z && n->sw->sw == z && n->sw->se == z &&
          n->se->ne == z && n->se->sw == z && n->se->se == z) {
         depth-- ;
         n = find_node(n->nw->se, n->ne->sw, n->sw->ne, n->se->nw) ;
      } else {
         break ;
      }
   }
   return n ;
}
/*
 *   A lot of the routines from here on down traverse the universe, hanging
 *   information off the nodes.  The way they generally do so is by using
 *   (or abusing) the cache (res) field, and the least significant bit of
 *   the hash next field (as a visited bit).
 */
#define marked(n) (1 & (g_uintptr_t)(n)->next)
#define mark(n) ((n)->next = (node *)(1 | (g_uintptr_t)(n)->next))
#define clearmark(n) ((n)->next = (node *)(~1 & (g_uintptr_t)(n)->next))
#define clearmarkbit(p) ((node *)(~1 & (g_uintptr_t)(p)))
/*
 *   Sometimes we want to use *res* instead of next to mark.  You cannot
 *   do this to leaves, though.
 */
#define marked2(n) (3 & (g_uintptr_t)(n)->res)
#define mark2(n) ((n)->res = (node *)(1 | (g_uintptr_t)(n)->res))
#define mark2v(n,v) ((n)->res = (node *)(v | (g_uintptr_t)(n)->res))
#define clearmark2(n) ((n)->res = (node *)(~3 & (g_uintptr_t)(n)->res))
void hlifealgo::unhash_node(node *n) {
   node *p ;
   g_uintptr_t h = node_hash(n->nw,n->ne,n->sw,n->se) ;
   node *pred = 0 ;
   h = HASHMOD(h) ;
   for (p=hashtab[h]; (!is_node(p) || !marked2(p)) && p; p = p->next) {
      if (p == n) {
         if (pred)
            pred->next = p->next ;
         else
            hashtab[h] = p->next ;
         return ;
      }
      pred = p ;
   }
   lifefatal("Didn't find node to unhash") ;
}
void hlifealgo::unhash_node2(node *n) {
   node *p ;
   g_uintptr_t h = node_hash(n->nw,n->ne,n->sw,n->se) ;
   node *pred = 0 ;
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
   lifefatal("Didn't find node to unhash 2") ;
}
void hlifealgo::rehash_node(node *n) {
   g_uintptr_t h = node_hash(n->nw,n->ne,n->sw,n->se) ;
   h = HASHMOD(h) ;
   n->next = hashtab[h] ;
   hashtab[h] = n ;
}
/*
 *   This recursive routine calculates the population by hanging the
 *   population on marked nodes.
 */
const bigint &hlifealgo::calcpop(node *root, int depth) {
   if (root == zeronode(depth))
      return bigint::zero ;
   if (depth == 2)
      return ((leaf *)root)->leafpop ;
   if (marked2(root))
      return *(bigint*)&(root->next) ;
   depth-- ;
   if (root->next == 0)
      mark2v(root, 3) ;
   else {
      unhash_node(root) ;
      mark2(root) ;
   }
/**
 *   We use allocate-in-place bigint constructor here to initialize the
 *   node.  This should compile to a single instruction.
 */
   new(&(root->next))bigint(
        calcpop(root->nw, depth), calcpop(root->ne, depth),
        calcpop(root->sw, depth), calcpop(root->se, depth)) ;
   return *(bigint *)&(root->next) ;
}
/*
 *   Call this after doing something that unhashes nodes in order to
 *   use the next field as a temp pointer.
 */
void hlifealgo::aftercalcpop2(node *root, int depth) {
   if (depth == 2 || root == zeronode(depth))
      return ;
   int v = marked2(root) ;
   if (v) {
      clearmark2(root) ;
      depth-- ;
      if (depth > 2) {
         aftercalcpop2(root->nw, depth) ;
         aftercalcpop2(root->ne, depth) ;
         aftercalcpop2(root->sw, depth) ;
         aftercalcpop2(root->se, depth) ;
      }
      ((bigint *)&(root->next))->~bigint() ;
      if (v == 3)
         root->next = 0 ;
      else
         rehash_node(root) ;
   }
}
/*
 *   Call this after writing macrocell.
 */
void hlifealgo::afterwritemc(node *root, int depth) {
   if (root == zeronode(depth))
      return ;
   if (depth == 2) {
      root->nw = 0 ;
      return ;
   }
   if (marked2(root)) {
      clearmark2(root) ;
      depth-- ;
      afterwritemc(root->nw, depth) ;
      afterwritemc(root->ne, depth) ;
      afterwritemc(root->sw, depth) ;
      afterwritemc(root->se, depth) ;
      rehash_node(root) ;
   }
}
/*
 *   This top level routine calculates the population of a universe.
 */
void hlifealgo::calcPopulation() {
   int depth ;
   ensure_hashed() ;
   depth = node_depth(root) ;
   population = calcpop(root, depth) ;
   aftercalcpop2(root, depth) ;
}
/*
 *   Is the universe empty?
 */
int hlifealgo::isEmpty() {
   ensure_hashed() ;
   return root == zeronode(depth) ;
}
/*
 *   This routine marks a node as needed to be saved.
 */
node *hlifealgo::save(node *n) {
   if (gsp >= stacksize) {
      int nstacksize = stacksize * 2 + 100 ;
      alloced += sizeof(node *)*(nstacksize-stacksize) ;
      stack = (node **)realloc(stack, nstacksize * sizeof(node *)) ;
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
void hlifealgo::pop(int n) {
   gsp = n ;
}
/*
 *   This routine clears the stack altogether.
 */
void hlifealgo::clearstack() {
   gsp = 0 ;
}
/*
 *   Do a gc.  Walk down from all nodes reachable on the stack, saveing
 *   them by setting the odd bit on the next link.  Then, walk the hash,
 *   eliminating the res from everything that's not saveed, and moving
 *   the nodes from the hash to the freelist as appropriate.  Finally,
 *   walk the hash again, clearing the low order bits in the next pointers.
 */
void hlifealgo::gc_mark(node *root, int invalidate) {
   if (!marked(root)) {
      mark(root) ;
      if (is_node(root)) {
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
void hlifealgo::do_gc(int invalidate) {
   int i ;
   g_uintptr_t freed_nodes=0 ;
   node *p, *pp ;
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
      if (zeronodea[i] != 0)
         break ;
   if (i >= 0)
      gc_mark(zeronodea[i], 0) ; // never invalidate zeronode
   if (root != 0)
      gc_mark(root, invalidate) ; // pick up the root
   for (i=0; i<gsp; i++) {
      poller->poll() ;
      gc_mark(stack[i], invalidate) ;
   }
   for (i=0; i<timeline.framecount; i++)
      gc_mark((node *)timeline.frames[i], invalidate) ;
   hashpop = 0 ;
   memset(hashtab, 0, sizeof(node *) * hashprime) ;
   freenodes = 0 ;
   for (p=nodeblocks; p; p=p->next) {
      poller->poll() ;
      for (pp=p+1, i=1; i<((g_uintptr_t)(p->res)+1); i++, pp++) {
         if (marked(pp)) {
            g_uintptr_t h = 0 ;
            if (pp->nw) { /* yes, it's a node */
               h = HASHMOD(node_hash(pp->nw, pp->ne, pp->sw, pp->se)) ;
            } else {
               leaf *lp = (leaf *)pp ;
               if (invalidate)
                  leafres(lp) ;
               h = HASHMOD(leaf_hash(lp->nw, lp->ne, lp->sw, lp->se)) ;
            }
            pp->next = hashtab[h] ;
            hashtab[h] = pp ;
            hashpop++ ;
         } else {
            pp->next = freenodes ;
            freenodes = pp ;
            freed_nodes++ ;
         }
      }
   }
   inGC = 0 ;
   if (verbose) {
     double perc = (double)freed_nodes / (double)totalthings * 100.0 ;
     sprintf(statusline+strlen(statusline), " freed %g percent (%" PRIuPTR ").",
                                                   perc, freed_nodes) ;
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
 *   nodes we've handled.
 */
void hlifealgo::clearcache(node *n, int depth, int clearto) {
   if (!marked(n)) {
      mark(n) ;
      if (depth > 3) {
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
 *   Clear the entire cache of everything, and recalculate all leaves.
 *   This can be very expensive.
 */
void hlifealgo::clearcache() {
   cacheinvalid = 1 ;
}
/*
 *   Change the ngens value.  Requires us to walk the hash, clearing
 *   the cache fields of any nodes that do not have the appropriate
 *   values.
 */
void hlifealgo::new_ngens(int newval) {
   g_uintptr_t i ;
   node *p, *pp ;
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
   if (clearto < 3)
      clearto = 3 ;
   ngens = newval ;
   inGC = 1 ;
   for (i=0; i<hashprime; i++)
      for (p=hashtab[i]; p; p=clearmarkbit(p->next))
         if (is_node(p) && !marked(p))
            clearcache(p, node_depth(p), clearto) ;
   for (p=nodeblocks; p; p=p->next) {
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
int hlifealgo::log2(unsigned int n) {
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
const bigint &hlifealgo::getPopulation() {
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
 *   clearspace nodes and the input pattern is never garbage
 *   collected; we turn on garbage collection, and then we invoke our
 *   magic top-level routine passing in clearspace borders that are
 *   guaranteed large enough.
 */
node *hlifealgo::runpattern() {
   node *n = root ;
   save(root) ; // do this in case we interrupt generation
   ensure_hashed() ;
   okaytogc = 1 ;
   if (cacheinvalid) {
      do_gc(1) ; // invalidate the entire cache and recalc leaves
      cacheinvalid = 0 ;
   }
   int depth = node_depth(n) ;
   node *n2 ;
   n = pushroot(n) ;
   depth++ ;
   n = pushroot(n) ;
   depth++ ;
   while (ngens + 2 > depth) {
      n = pushroot(n) ;
      depth++ ;
   }
   save(zeronode(nzeros-1)) ;
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
/* Returns the center 4-square of an 8x8 leaf node. */
static unsigned short unpack4x4center(leaf *leaf) {
   return combine4(leaf->nw, leaf->ne, leaf->sw, leaf->se);
}
const char *hlifealgo::readmacrocell(char *line) {
   int n=0 ;
   g_uintptr_t i=1, nw=0, ne=0, sw=0, se=0, indlen=0 ;
   int r, d ;
   node **ind = 0 ;
   root = 0 ;
   while (getline(line, 10000)) {
      if (i >= indlen) {
         g_uintptr_t nlen = i + indlen + 10 ;
         ind = (node **)realloc(ind, sizeof(node*) * nlen) ;
         if (ind == 0)
           lifefatal("Out of memory (4).") ;
         while (indlen < nlen)
            ind[indlen++] = 0 ;
      }
      if (line[0] == '.' || line[0] == '*' || line[0] == '$') {
         int x=0, y=7 ;
         unsigned short lnw=0, lne=0, lsw=0, lse=0 ;
         char *p = 0 ;
         for (p=line; *p > ' '; p++) {
            switch(*p) {
case '*':      if (x > 7 || y < 0)
                  return "Illegal coordinates in readmacrocell." ;
               if (x < 4)
                  if (y < 4)
                     lsw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
                  else
                     lnw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
               else
                  if (y < 4)
                     lse |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
                  else
                     lne |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
               /* note: fall through here */
case '.':      x++ ;
               break ;
case '$':      x = 0 ;
               y-- ;
               break ;
default:       return "Illegal character in readmacrocell." ;
            }
         }
         clearstack() ;
         root = ind[i++] = (node *)find_leaf(lnw, lne, lsw, lse) ;
         depth = 2;
      } else if (line[0] == '#') {
         char *p, *pp ;
         const char *err ;
         switch (line[1]) {
         case 'R':
            p = line + 2 ;
            while (*p && *p <= ' ') p++ ;
            pp = p ;
            while (*pp > ' ') pp++ ;
            *pp = 0 ;
            
            // AKT: need to check for B0-not-Smax rule
            err = setrule(p);
            if (err)
               return err;
            if (hliferules.alternate_rules)
               return "B0-not-Smax rules are not allowed in HashLife.";
            
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
               timeline.next = timeline.start ;
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
	       timeline.frames.push_back(make_internal_node(ind[nodeind])) ;
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
            // AKT: best not to use lifefatal here because user won't see any
            // error message when reading clipboard data starting with "[..."
            return "Parse error in readmacrocell." ;
         if (d < 1)
            return "Oops; bad depth in readmacrocell." ;
         if (d > 1) {
            ind[0] = zeronode(d <= 4 ? 2 : d-2) ; /* allow zeros to work right */
            if (nw >= i || ind[nw] == 0 || ne >= i || ind[ne] == 0 ||
               sw >= i || ind[sw] == 0 || se >= i || ind[se] == 0) {
               return "Node out of range in readmacrocell." ;
            }
         }
         if (d < 4) {
            /* Support macrocell nodes in multicell format (i.e. with 2-square
               leaf nodes) for compatibility. Cell values must be 0 or 1! */
            unsigned short lnw=0, lne=0, lsw=0, lse=0 ;
            if (d == 1) {
               if (nw > 1 || ne > 1 || sw > 1 || se > 1) {
                  return "Cell value out of range in readmacrocell." ;
               }
               lnw = nw ? 1 <<  0 : 0 ;
               lne = ne ? 1 <<  3 : 0 ;
               lsw = sw ? 1 << 12 : 0 ;
               lse = se ? 1 << 15 : 0 ;
            } else { // d == 2 || d == 3
               node *pnw=ind[nw], *pne=ind[ne], *psw=ind[sw], *pse=ind[se] ;
               if (is_node(pnw) || is_node(pne) || is_node(psw) || is_node(pse)) {
                  return "Invalid leaf node reference in readmacrocell." ;
               }
               lnw = unpack4x4center((leaf *)pnw) ;
               lne = unpack4x4center((leaf *)pne) ;
               lsw = unpack4x4center((leaf *)psw) ;
               lse = unpack4x4center((leaf *)pse) ;
               if (d == 2) {
                  lnw >>= 5 ;
                  lne >>= 3 ;
                  lsw <<= 3 ;
                  lse <<= 5 ;
               }
            }
            clearstack() ;
            root = ind[i++] = (node *)find_leaf(lnw, lne, lsw, lse) ;
         } else {  // d >= 4
            clearstack() ;
            root = ind[i++] = find_node(ind[nw], ind[ne], ind[sw], ind[se]) ;
         }
         depth = d - 1 ;
      }
   }
   if (ind)
      free(ind) ;
   if (root == 0) {
      // AKT: allow empty macrocell pattern; note that endofpattern()
      // will be called soon so don't set hashed here
      // return "Invalid macrocell file: no nodes." ;
      return 0 ;
   }
   if (depth < 3) {
      root = make_internal_node(root) ;
      depth = 3;
   }
   hashed = 1 ;
   return 0 ;
}

// Flip bits in given rule table.
static void fliprule(char *rptr) {
   for (int i=0; i<65536; i++) {
      int j = ((i & 0xf) << 12) +
               ((i & 0xf0) << 4) + ((i & 0xf00) >> 4) + ((i & 0xf000) >> 12) ;
      if (i <= j) {
         char fi = rptr[i] ;
         char fj = rptr[j] ;
         fi = ((fi & 0x30) >> 4) + ((fi & 0x3) << 4) ;
         fj = ((fj & 0x30) >> 4) + ((fj & 0x3) << 4) ;
         rptr[i] = fj ;
         rptr[j] = fi ;
      }
   }
}

const char *hlifealgo::setrule(const char *s) {
   poller->bailIfCalculating() ;
   const char* err = hliferules.setrule(s, this);
   if (err) return err;

   // invert orientation if not hex or Wolfram
   if (!(hliferules.isHexagonal() || hliferules.isWolfram())) {
      fliprule(hliferules.rule0);
   }

   clearcache() ;
   
   if (hliferules.alternate_rules)
      return "B0-not-Smax rules are not allowed in HashLife.";
      
   if (hliferules.isHexagonal())
      grid_type = HEX_GRID;
   else if (hliferules.isVonNeumann())
      grid_type = VN_GRID;
   else
      grid_type = SQUARE_GRID;
      
   return 0 ;
}
void hlifealgo::unpack8x8(unsigned short nw, unsigned short ne,
                          unsigned short sw, unsigned short se,
                          unsigned int *top, unsigned int *bot) {
   *top = ((nw & 0xf000) << 16) | (((ne & 0xf000) | (nw & 0xf00)) << 12) |
          (((ne & 0xf00) | (nw & 0xf0)) << 8) |
          (((ne & 0xf0) | (nw & 0xf)) << 4) | (ne & 0xf) ;
   *bot = ((sw & 0xf000) << 16) | (((se & 0xf000) | (sw & 0xf00)) << 12) |
          (((se & 0xf00) | (sw & 0xf0)) << 8) |
          (((se & 0xf0) | (sw & 0xf)) << 4) | (se & 0xf) ;
}
/**
 *   Write out the native macrocell format.  This is the one we use when
 *   we're not interactive and displaying a progress dialog.
 */
g_uintptr_t hlifealgo::writecell(std::ostream &os, node *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zeronode(depth))
      return 0 ;
   if (depth == 2) {
      if (root->nw != 0)
         return (g_uintptr_t)(root->nw) ;
   } else {
      if (marked2(root))
         return (g_uintptr_t)(root->next) ;
      unhash_node2(root) ;
      mark2(root) ;
   }
   if (depth == 2) {
      int i, j ;
      unsigned int top, bot ;
      leaf *n = (leaf *)root ;
      thiscell = ++cellcounter ;
      root->nw = (node *)thiscell ;
      unpack8x8(n->nw, n->ne, n->sw, n->se, &top, &bot) ;
      for (j=7; (top | bot) && j>=0; j--) {
         int bits = (top >> 24) ;
         top = (top << 8) | (bot >> 24) ;
         bot = (bot << 8) ;
         for (i=0; bits && i<8; i++, bits = (bits << 1) & 255)
            if (bits & 128)
               os << '*' ;
            else
               os << '.' ;
         os << '$' ;
      }
      os << '\n' ;
   } else {
      g_uintptr_t nw = writecell(os, root->nw, depth-1) ;
      g_uintptr_t ne = writecell(os, root->ne, depth-1) ;
      g_uintptr_t sw = writecell(os, root->sw, depth-1) ;
      g_uintptr_t se = writecell(os, root->se, depth-1) ;
      thiscell = ++cellcounter ;
      root->next = (node *)thiscell ;
      os << depth+1 << ' ' << nw << ' ' << ne << ' ' << sw << ' ' << se << '\n';
   }
   return thiscell ;
}
/**
 *   This new two-pass method works by doing a prepass that numbers the
 *   nodes and counts the number of nodes that should be sent, so we can
 *   display an accurate progress dialog.
 */
g_uintptr_t hlifealgo::writecell_2p1(node *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zeronode(depth))
      return 0 ;
   if (depth == 2) {
      if (root->nw != 0)
         return (g_uintptr_t)(root->nw) ;
   } else {
      if (marked2(root))
         return (g_uintptr_t)(root->next) ;
      unhash_node2(root) ;
      mark2(root) ;
   }
   if (depth == 2) {
      thiscell = ++cellcounter ;
      // note:  we *must* not abort this prescan
      if ((cellcounter & 4095) == 0)
         lifeabortprogress(0, "Scanning tree") ;
      root->nw = (node *)thiscell ;
   } else {
      writecell_2p1(root->nw, depth-1) ;
      writecell_2p1(root->ne, depth-1) ;
      writecell_2p1(root->sw, depth-1) ;
      writecell_2p1(root->se, depth-1) ;
      thiscell = ++cellcounter ;
      // note:  we *must* not abort this prescan
      if ((cellcounter & 4095) == 0)
         lifeabortprogress(0, "Scanning tree") ;
      root->next = (node *)thiscell ;
   }
   return thiscell ;
}
/**
 *   This one writes the cells, but assuming they've already been
 *   numbered, and displaying a progress dialog.
 */
static char progressmsg[80] ;
g_uintptr_t hlifealgo::writecell_2p2(std::ostream &os, node *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zeronode(depth))
      return 0 ;
   if (depth == 2) {
      if (cellcounter + 1 != (g_uintptr_t)(root->nw))
         return (g_uintptr_t)(root->nw) ;
      thiscell = ++cellcounter ;
      if ((cellcounter & 4095) == 0) {
         std::streampos siz = os.tellp();
         sprintf(progressmsg, "File size: %.2f MB", double(siz) / 1048576.0) ;
         lifeabortprogress(thiscell/(double)writecells, progressmsg) ;
      }
      int i, j ;
      unsigned int top, bot ;
      leaf *n = (leaf *)root ;
      root->nw = (node *)thiscell ;
      unpack8x8(n->nw, n->ne, n->sw, n->se, &top, &bot) ;
      for (j=7; (top | bot) && j>=0; j--) {
         int bits = (top >> 24) ;
         top = (top << 8) | (bot >> 24) ;
         bot = (bot << 8) ;
         for (i=0; bits && i<8; i++, bits = (bits << 1) & 255)
            if (bits & 128)
               os << '*' ;
            else
               os << '.' ;
         os << '$' ;
      }
      os << '\n' ;
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
         std::streampos siz = os.tellp();
         sprintf(progressmsg, "File size: %.2f MB", double(siz) / 1048576.0) ;
         lifeabortprogress(thiscell/(double)writecells, progressmsg) ;
      }
      root->next = (node *)thiscell ;
      os << depth+1 << ' ' << nw << ' ' << ne << ' ' << sw << ' ' << se << '\n';
   }
   return thiscell ;
}
#define STRINGIFY(arg) STR2(arg)
#define STR2(arg) #arg
const char *hlifealgo::writeNativeFormat(std::ostream &os, char *comments) {
   int depth = node_depth(root) ;
   os << "[M2] (golly " STRINGIFY(VERSION) ")\n" ;

   // AKT: always write out explicit rule
   os << "#R " << hliferules.getrule() << '\n' ;

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
   writecell(f, root, depth) ;
   */
   /* this is the new two-pass way */
   cellcounter = 0 ;
   vector<int> depths(timeline.framecount) ;
   int framestosave = timeline.framecount ;
   if (timeline.savetimeline == 0)
     framestosave = 0 ;
   if (framestosave) {
     for (int i=0; i<timeline.framecount; i++) {
       node *frame = (node*)timeline.frames[i] ;
       depths[i] = node_depth(frame) ;
     }
     for (int i=0; i<timeline.framecount; i++) {
       node *frame = (node*)timeline.frames[i] ;
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
       node *frame = (node*)timeline.frames[i] ;
       writecell_2p2(os, frame, depths[i]) ;
       os << "#FRAME " << i << ' ' << (g_uintptr_t)frame->next << '\n' ;
     }
   }
   writecell_2p2(os, root, depth) ;
   /* end new two-pass way */
   if (framestosave) {
     for (int i=0; i<timeline.framecount; i++) {
       node *frame = (node*)timeline.frames[i] ;
       afterwritemc(frame, depths[i]) ;
     }
   }
   afterwritemc(root, depth) ;
   inGC = 0 ;
   return 0 ;
}
char hlifealgo::statusline[200] ;
static lifealgo *creator() { return new hlifealgo() ; }
void hlifealgo::doInitializeAlgoInfo(staticAlgoInfo &ai) {
   ai.setAlgorithmName("HashLife") ;
   ai.setAlgorithmCreator(&creator) ;
   ai.setDefaultBaseStep(8) ;
   ai.setDefaultMaxMem(500) ; // MB
   ai.minstates = 2 ;
   ai.maxstates = 2 ;
   // init default color scheme
   ai.defgradient = false;
   ai.defr1 = ai.defg1 = ai.defb1 = 255;        // start color = white
   ai.defr2 = ai.defg2 = ai.defb2 = 255;        // end color = white
   ai.defr[0] = ai.defg[0] = ai.defb[0] = 48;   // 0 state = dark gray
   ai.defr[1] = ai.defg[1] = ai.defb[1] = 255;  // 1 state = white
}

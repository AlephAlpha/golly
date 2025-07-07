#ifdef _MSC_VER
   #pragma warning(disable:4702)   // disable "unreachable code" warnings from MSVC
#endif
#include <set>
#ifdef _MSC_VER
   #pragma warning(default:4702)   // enable "unreachable code" warnings
#endif
#include <map>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <stdlib.h>

#include "util.h"
#include "ruletable_algo.h"
using namespace std ;

const int MAXPARAMS = 9 ;
const int MAXSTATES = 256 ;
struct ndd {
   int level, index ;
   vector<int> vals ;
   bool operator<(const ndd &n) const {
      if (level != n.level)
         return level < n.level ;
      return vals < n.vals ;
   }
   bool operator==(const ndd &n) const {
      return level == n.level && vals == n.vals ;
   }
} ;
set<ndd> world ;
int nodeseq ;
int shrinksize = 100 ;
vector<ndd> seq ;
int n_states, neighborhood_size ;
int curndd = 0 ;
typedef unsigned char state ;
static int getnode(ndd &n) {
   n.index = nodeseq ;
   set<ndd>::iterator it = world.find(n) ;
   if (it != world.end())
      return it->index ;
   seq.push_back(n) ;
   world.insert(n) ;
   return nodeseq++ ;
}
void initndd() {
   curndd = -1 ;
   for (int i=0; i<neighborhood_size; i++) {
      ndd n ;
      for (int j=0; j<n_states; j++)
         n.vals.push_back(curndd) ;
      n.level = i + 1 ;
      curndd = getnode(n) ;
   }
}
map<int, int> cache ;
int remap5[5] = {0, 3, 2, 4, 1} ;
int remap9[9] = {0, 5, 3, 7, 1, 4, 6, 2, 8} ;
int *remap ;
int addndd(const vector<vector<state> > &inputs, const state output,
           int nddr, int at) {
   if (at == 0)
      return nddr < 0 ? output : nddr ;
   map<int,int>::iterator it = cache.find(nddr) ;
   if (it != cache.end())
      return it->second ;
   ndd n = seq[nddr] ;
   const vector<state> &inset = inputs[remap[at-1]] ;
   for (unsigned int i=0; i<inset.size(); i++)
      n.vals[inset[i]] = addndd(inputs, output, n.vals[inset[i]], at-1) ;
   int r = getnode(n) ;
   cache[nddr] = r ;
   return r ;
}
void shrink() ;
void addndd(const vector<vector<state> > &inputs, const state output) {
   if (neighborhood_size == 5)
      remap = remap5 ;
   else
      remap = remap9 ;
   cache.clear() ;
   curndd = addndd(inputs, output, curndd, neighborhood_size) ;
   if (nodeseq > shrinksize)
       shrink() ;
}
int setdefaults(int nddr, int off, int at) {
   if (at == 0)
      return nddr < 0 ? off : nddr ;
   map<int,int>::iterator it = cache.find(nddr) ;
   if (it != cache.end())
      return it->second ;
   ndd n = seq[nddr] ;
   for (int i=0; i<n_states; i++)
      n.vals[i] = setdefaults(n.vals[i], i, at-1) ;
   int r = getnode(n) ;
   cache[nddr] = r ;
   return r ;
}
void setdefaults() {
   cache.clear() ;
   curndd = setdefaults(curndd, -1, neighborhood_size) ;
}
/**
 *   Rebuilds the ndd from scratch.
 */
int recreate(const vector<ndd> &oseq, int nddr, int lev) {
   if (lev == 0)
      return nddr ;
   map<int, int>::iterator it = cache.find(nddr) ;
   if (it != cache.end())
      return it->second ;
   ndd n = oseq[nddr] ;
   for (int i=0; i<n_states; i++)
      n.vals[i] = recreate(oseq, n.vals[i], lev-1) ;
   int r = getnode(n) ;
   cache[nddr] = r ;
   return r ;
}
void shrink() {
   world.clear() ;
   vector<ndd> oseq = seq ;
   seq.clear() ;
   cache.clear() ;
   nodeseq = 0 ;
   curndd = recreate(oseq, curndd, neighborhood_size) ;
   //cerr << "Shrunk from " << oseq.size() << " to " << seq.size() << endl ;
   shrinksize = (int)(seq.size() * 2) ;
}
void write_ndd() {
   shrink() ;
   printf("num_states=%d\n", n_states) ;
   printf("num_neighbors=%d\n", neighborhood_size-1) ;
   printf("num_nodes=%d\n", (int)seq.size()) ;
   for (unsigned int i=0; i<seq.size(); i++) {
      printf("%d", seq[i].level) ;
      for (unsigned int j=0; j<seq[i].vals.size(); j++)
         printf(" %d", seq[i].vals[j]) ;
      printf("\n") ;
   }
}
class mylifeerrors : public lifeerrors {
public:
   virtual void fatal(const char *s) {
      fprintf(stderr, "%s\n", s) ;
      exit(10) ;
   }
   virtual void warning(const char *s) {
      fprintf(stderr, "%s\n", s) ;
   }
   virtual void status(const char *s) {
      fprintf(stderr, "%s\n", s) ;
   }
   virtual void beginprogress(const char *) {
      aborted = false ;
   }
   virtual bool abortprogress(double, const char *) {
      return false ;
   }
   virtual void endprogress() {
      // do nothing
   }
   virtual const char *getuserrules() {
      return "" ;
   }
   virtual const char *getrulesdir() {
      return "Rules/" ;
   }
   virtual const char *gettempdir() {
      return "" ;
   }
} mylifeerrors ;
class my_ruletable_algo : public ruletable_algo {
public:
   my_ruletable_algo() : ruletable_algo() {}
   string loadrule(string filename) {
      return LoadRuleTable(filename) ;
   }
   void buildndd() ;
} ;
void my_ruletable_algo::buildndd() {
   unsigned int iRule;
   ::n_states = n_states ;
   ::neighborhood_size = (neighborhood==vonNeumann ? 5 : 9) ;
   initndd() ;
   for(iRule=0;iRule<this->n_compressed_rules;iRule++) {
     for (unsigned int bitno=0; bitno<sizeof(TBits)*8; bitno++) {
       TBits bit = ((TBits)1)<<bitno ;
       vector<vector<state> > in ;
       int ok = 1 ;
       for (int i=0; i<neighborhood_size; i++) {
         vector<state> nv ;
         for (unsigned int j=0; j<n_states; j++)
           if (lut[i][j][iRule] & bit)
             nv.push_back((state)j) ;
         if (nv.size() == 0) {
           ok = 0 ;
           break ;
         }
         in.push_back(nv) ;
       }
       if (ok) {
         state out = output[iRule*sizeof(TBits)*8+bitno] ;
         addndd(in, out) ;
       }
     }
   }
   setdefaults() ;
   shrink() ;
}
int main(int argc, char *argv[]) {
   if (argc < 2) {
      cerr << "Usage: RuleTableToTree rule >Rules/rule.tree" << endl ;
      exit(1) ;
   }
   lifeerrors::seterrorhandler(&mylifeerrors) ;
   my_ruletable_algo *rta = new my_ruletable_algo() ;
   string err = rta->loadrule(argv[1]) ;
   if (err.size() > 0) {
      cerr << "Error: " << err << endl ;
      exit(1) ;
   }
   rta->buildndd() ;
   write_ndd() ;
   delete rta ;
}

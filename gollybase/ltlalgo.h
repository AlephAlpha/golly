// This file is part of Golly.
// See docs/License.html for the copyright notice.

// This is the code for the "Larger than Life" family of rules.

#ifndef LTLALGO_H
#define LTLALGO_H

#include "lifealgo.h"
#include "liferules.h"  // for MAXRULESIZE
#include <vector>

class ltlalgo : public lifealgo {
public:
    ltlalgo();
    virtual ~ltlalgo();
    virtual int setcell(int x, int y, int newstate);
    virtual int getcell(int x, int y);
    virtual int nextcell(int x, int y, int& v);
    virtual void endofpattern();
    virtual void setIncrement(bigint inc) { increment = inc; }
    virtual void setIncrement(int inc) { increment = inc; }
    virtual void setGeneration(bigint gen) { generation = gen; }
    virtual const bigint& getPopulation();
    virtual int isEmpty();
    virtual int hyperCapable() { return 0; }
    virtual void setMaxMemory(int m) {}
    virtual int getMaxMemory() { return 0; }
    virtual const char* setrule(const char* s);
    virtual const char* getrule();
    virtual const char* DefaultRule();
    virtual int NumCellStates();
    virtual int NumRandomizedCellStates() { return 2 ; }
    virtual void step();
    virtual void* getcurrentstate() { return 0; }
    virtual void setcurrentstate(void*) {}
    virtual void draw(viewport& view, liferender& renderer);
    virtual void fit(viewport& view, int force);
    virtual void lowerRightPixel(bigint& x, bigint& y, int mag);
    virtual void findedges(bigint* t, bigint* l, bigint* b, bigint* r);
    virtual const char* writeNativeFormat(std::ostream&, char*) {
        return "No native format for ltlalgo.";
    }
    static void doInitializeAlgoInfo(staticAlgoInfo&);

private:
    char canonrule[MAXRULESIZE];        // canonical version of valid rule passed into setrule
    int population;                     // number of non-zero cells in current generation
    int gwd, ght;                       // width and height of grid (in cells)
    int gwdm1, ghtm1;                   // gwd-1, ght-1 (bottom right corner of grid)
    unsigned char* currgrid;            // points to gwd*ght cells for current generation
    unsigned char* nextgrid;            // points to gwd*ght cells for next generation
    int minx, miny, maxx, maxy;         // boundary of live cells (in grid coordinates)
    int gtop, gleft, gbottom, gright;   // cell coordinates of grid edges
    vector<int> cell_list;              // used by save_cells and restore_cells
    bool show_warning;                  // flag used to avoid multiple warning dialogs
    int* colcounts;                     // cumulative column counts of state-1 cells
    
    // bounded grids are surrounded by a border of cells (with thickness = range+1)
    // so we can calculate neighborhood counts without checking for edge conditions;
    // note that in an unbounded universe outerwd = gwd, outerht = ght,
    // currgrid = outergrid1, nextgrid = outergrid2
    
    int border;                         // border thickness in cells (depends on range)
    int outerwd, outerht;               // width and height of bounded grids (including border)
    int outerbytes;                     // outerwd*outerht
    unsigned char* outergrid1;          // points to outerwd*outerht cells for current generation
    unsigned char* outergrid2;          // points to outerwd*outerht cells for next generation
    int *shape ;                        // for shaped neighborhoods, this is the shape

    // these variables are used in getcount and faster_Neumann_*
    int ccht;                           // height of colcounts array when ntype = N
    int halfccwd;                       // half width of colcounts array when ntype = N
    int nrows, ncols;                   // size of rectangle being processed
    
    // rule parameters (set by setrule)
    int range;                          // neighborhood radius
    char ntype;                         // extended neighborhood type (M = Moore, N = von Neumann, C = shaped (circle))
    char topology;                      // grid topology (T = torus, P = plane)
    unsigned char* births;              // flag for birth at each neighbour count
    unsigned char* survivals;           // flag for survival at each neighbour count
    unsigned char* altbirths;           // flag for birth on odd gens for B0 emulation
    unsigned char* altsurvivals;        // flag for suvival on odd gens for B0 emulation
    int customcount;                    // neighborhood count for custom neighborhoods
    bool b0;                            // whether B0 is specified
    int* weights;                       // neighborhood weights
    int* stateweights;                  // state weights
    int* customneighborhood;            // custom neighborhood list
    int customlength;                   // custom neighborhood list length
    
    const char* read_custom(const char *n, int r, int &c, TGridType &gt, const char *&nbrend); // read custom neighborhood
    const char* read_weighted(const char *n, int r, int states, int &c, TGridType &gt, const char *&nbrend); // read weighted neighborhood
    char* flags_string(const unsigned char* flags, int len); // convert birth or survival flags into string
    int max_neighbors(int range, const char neighborhood, int customcount, int *tshape); // compute max neighbors for range and neighborhood
    void setup_b0_emulation(int maxn);  // setup B0 emulation
    void create_grids(int wd, int ht);  // create a bounded universe of given width and height
    void allocate_colcounts();          // allocate the colcounts array
    void empty_boundaries();            // set minx, miny, maxx, maxy when population is 0
    void save_cells();                  // save current pattern in cell_list
    void restore_cells();               // restore pattern from cell_list
    void do_gen(int mincol, int minrow, int maxcol, int maxrow); // calculate next generation
    void do_bounded_gen();              // calculate the next generation in a bounded universe
    bool do_unbounded_gen();            // calculate the next generation in an unbounded universe
    int getcount(int i, int j);         // used in faster_Neumann_*

    const char* resize_grids(int up, int down, int left, int right);
    // try to resize an unbounded universe by the given amounts (possibly -ve);
    // if it fails then return a suitable error message
    
    void fast_Moore(int mincol, int minrow, int maxcol, int maxrow);
    void faster_Moore_bounded(int mincol, int minrow, int maxcol, int maxrow);
    void faster_Moore_bounded2(int mincol, int minrow, int maxcol, int maxrow);
    void faster_Moore_unbounded(int mincol, int minrow, int maxcol, int maxrow);
    void faster_Moore_unbounded2(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Neumann(int mincol, int minrow, int maxcol, int maxrow);
    void faster_Neumann_bounded(int mincol, int minrow, int maxcol, int maxrow);
    void faster_Neumann_unbounded(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Shaped(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Asterisk(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Tripod(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Weighted(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Custom(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Hash(int mincol, int minrow, int maxcol, int maxrow);
    void fast_CheckerBoth(int mincol, int minrow, int maxcol, int maxrow, int start);
    void fast_Aligned(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Checker(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Hex(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Saltire(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Star(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Cross(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Triangular(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Gaussian(int mincol, int minrow, int maxcol, int maxrow);
    // these routines are called from do_gen to process a rectangular region of cells
    
    void update_current_grid(unsigned char &state, int ncount);
    void update_next_grid(int x, int y, int xyoffset, int ncount);
    // called from each of the fast* routines to set the state of the x,y cell
    // in nextgrid based on the given neighborhood count
};

#endif

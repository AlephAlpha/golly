// This file is part of Golly.
// See docs/License.html for the copyright notice.

// Implementation code for the "Larger than Life" family of rules.
// See Help/Algorithms/Larger_than_Life.html for more info.

#include "ltlalgo.h"
#include "liferules.h"
#include "util.h"
#include <stdlib.h>     // for malloc, free, etc
#include <limits.h>     // for INT_MIN and INT_MAX
#include <string.h>     // for memset and strchr
#include <cstddef>      // for ptrdiff_t

// -----------------------------------------------------------------------------

// set default rule to match Life
static const char *DEFAULTRULE = "R1,C0,M0,S2..3,B3..3,NM";

#define MAXRANGE 500
#define MAXGAUSSIANRANGE 50
#define DEFAULTSIZE 400     // must be >= 2

// maximum number of columns in a cell's neighborhood (used in fast_Moore)
static const int MAXNCOLS = 2 * MAXRANGE + 1;

// maximum number of cells in grid must be < 2^31 so population can't overflow
#define MAXCELLS 1000000000.0 // 1 billion

// faster_Neumann_* calls are much slower than fast_Neumann when the
// range is 1 or 2, similar when 5, but much faster when 10 or above
#define SMALL_NN_RANGE 4

// valid neighborhoods (upper case)
static const char *VALIDNEIGHBORHOODS = "MNC+X*2HBD#@3ALGW";

// hex digits (upper case)
static const char *HEXCHARACTERS = "0123456789ABCDEF";

// -----------------------------------------------------------------------------

// Create a new empty universe.

ltlalgo::ltlalgo()
{
    shape = NULL ;
    // create a bounded universe with the default grid size, range and neighborhood
    unbounded = false;
    range = 1;
    ntype = 'M';
    colcounts = NULL;
    create_grids(DEFAULTSIZE, DEFAULTSIZE);
    generation = 0;
    increment = 1;
    show_warning = true;
    births = NULL;
    survivals = NULL;
    altbirths = NULL;
    altsurvivals = NULL;
    b0 = false;
    weights = NULL;
    stateweights = NULL;
    customneighborhood = NULL;
    customlength = 0;
}

// -----------------------------------------------------------------------------

// Destroy the universe.

ltlalgo::~ltlalgo()
{
    free(outergrid1);
    if (outergrid2) free(outergrid2);
    if (colcounts) free(colcounts);
    if (shape) free(shape);
    if (births) free(births);
    if (survivals) free(survivals);
    if (altbirths) free(altbirths);
    if (altsurvivals) free(altsurvivals);
    if (weights) free(weights);
    if (stateweights) free(stateweights);
    if (customneighborhood) free(customneighborhood);
}

// -----------------------------------------------------------------------------

void ltlalgo::allocate_colcounts()
{
    // allocate the array used for cumulative column counts of state-1 cells
    if (colcounts) free(colcounts);
    if (ntype == 'M') {
        colcounts = (int*) malloc(outerbytes * sizeof(int));
        // if NULL then use fast_Moore, otherwise faster_Moore_*
    } else if (ntype == 'N') {
        if (range <= SMALL_NN_RANGE) {
            // use fast_Neumann (faster than faster_Neumann_* for small ranges)
            colcounts = NULL;
        } else {
            // additional rows are needed to calculate counts in faster_Neumann_*
            colcounts = (int*) malloc(outerwd * (outerht + (outerwd-1)/2) * sizeof(int));
            // if NULL then use fast_Neumann
        }
    } else if (strchr(VALIDNEIGHBORHOODS, ntype) != NULL) {
        colcounts = NULL ;
        // use fast_<neighborhood>
    } else {
        lifefatal("Unexpected ntype!");
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::create_grids(int wd, int ht)
{
    // create a bounded universe of given width and height
    gwd = wd;
    ght = ht;
    border = range + 1;                 // the extra 1 is needed by faster_Moore_*
    outerwd = gwd + border * 2;         // add left and right border
    outerht = ght + border * 2;         // add top and bottom border
    outerbytes = outerwd * outerht;
    
    allocate_colcounts();

    // allocate memory for grid
    int offset = border * outerwd + border;
    outergrid1 = (unsigned char*) calloc(outerbytes, sizeof(unsigned char));
    if (outergrid1 == NULL) lifefatal("Not enough memory for LtL grid!");
    // point currgrid to top left non-border cells within outergrid1
    currgrid = outergrid1 + offset;

    // if using fast_Moore or fast_Neumann we need to allocate outergrid2
    if (colcounts == NULL) {
        outergrid2 = (unsigned char*) calloc(outerbytes, sizeof(unsigned char));
        if (outergrid2 == NULL) lifefatal("Not enough memory for LtL grids!");
        // point nextgrid to top left non-border cells within outergrid2
        nextgrid = outergrid2 + offset;
    } else {
        // faster_* calls don't use outergrid2
        outergrid2 = NULL;
        nextgrid = NULL;
    }

    // set grid coordinates of cell at bottom right corner of inner grid
    gwdm1 = gwd - 1;
    ghtm1 = ght - 1;

    // set cell coordinates of inner grid edges (middle of grid is 0,0)
    gtop = -int(ght / 2);
    gleft = -int(gwd / 2);
    gbottom = gtop + ghtm1;
    gright = gleft + gwdm1;
    
    // set bigint versions of inner grid edges (used by GUI code)
    gridtop = gtop;
    gridleft = gleft;
    gridbottom = gbottom;
    gridright = gright;
    
    // the universe is empty
    population = 0;
    
    // init boundaries so next birth will change them
    empty_boundaries();
}

// -----------------------------------------------------------------------------

void ltlalgo::empty_boundaries()
{
    minx = INT_MAX;
    miny = INT_MAX;
    maxx = INT_MIN;
    maxy = INT_MIN;
}

// -----------------------------------------------------------------------------

int ltlalgo::NumCellStates()
{
    return maxCellStates;
}

// -----------------------------------------------------------------------------

void ltlalgo::endofpattern()
{
    show_warning = true;
}

// -----------------------------------------------------------------------------

const char* ltlalgo::resize_grids(int up, int down, int left, int right)
{
    // for triangular rules ensure polarity doesn't change
    if (grid_type == TRI_GRID) {
        if (up & 1) up++;
        if (left & 1) left++;
    }

    // try to resize an unbounded universe by given amounts (possibly -ve)
    int newwd = gwd + left + right;
    int newht = ght + up + down;
    if ((float)newwd * (float)newht > MAXCELLS) {
        return "Sorry, but the universe can't be expanded any further.";
    }

    // check if new grid edges would be outside editing limits
    int newtop = gtop - up;
    int newleft = gleft - left;
    int newbottom = newtop + newht - 1;
    int newright = newleft + newwd - 1;
    if (newtop <   -1000000000 || newleft < -1000000000 ||
        newbottom > 1000000000 || newright > 1000000000) {
        return "Sorry, but the grid edges can't be outside the editing limits.";
    }
    
    int newbytes = newwd * newht;
    unsigned char* newcurr = (unsigned char*) calloc(newbytes, sizeof(unsigned char));
    unsigned char* newnext = (unsigned char*) calloc(newbytes, sizeof(unsigned char));
    if (newcurr == NULL || newnext == NULL) {
        if (newcurr) free(newcurr);
        if (newnext) free(newnext);
        return "Not enough memory to resize universe!";
    }
    
    // resize succeeded so copy pattern from currgrid into newcurr
    if (population > 0) {
        unsigned char* src = currgrid + miny * outerwd + minx;
        unsigned char* dest = newcurr + (miny + up) * newwd + minx + left;
        int xbytes = maxx - minx + 1;
        for (int row = miny; row <= maxy; row++) {
            memcpy(dest, src, xbytes);
            src += outerwd;
            dest += newwd;
        }
        // shift pattern boundaries
        minx += left;
        maxx += left;
        miny += up;
        maxy += up;
    }
    
    free(outergrid1);
    if (outergrid2) free(outergrid2);
    outergrid1 = currgrid = newcurr;
    outergrid2 = nextgrid = newnext;

    outerwd = gwd = newwd;
    outerht = ght = newht;
    outerbytes = newbytes;
    
    // set grid coordinates of cell at bottom right corner of grid
    gwdm1 = gwd - 1;
    ghtm1 = ght - 1;

    // adjust cell coordinates of grid edges
    gtop -= up;
    gleft -= left;
    gbottom = gtop + ghtm1;
    gright = gleft + gwdm1;
    
    // set bigint versions of grid edges (used by GUI code)
    gridtop = gtop;
    gridleft = gleft;
    gridbottom = gbottom;
    gridright = gright;

    allocate_colcounts();

    if (colcounts) {
        // faster_* calls don't use outergrid2
        free(outergrid2);
        outergrid2 = NULL;
        nextgrid = NULL;
    }

    return NULL;    // success
}

// -----------------------------------------------------------------------------

// Set the cell at the given location to the given state.

int ltlalgo::setcell(int x, int y, int newstate)
{
    if (newstate < 0 || newstate >= maxCellStates) return -1;
    
    if (unbounded) {
        // check if universe needs to be expanded
        if (x < gleft || x > gright || y < gtop || y > gbottom) {
            if (population == 0) {
                // no need to resize empty grids;
                // just adjust grid edges so that x,y is in middle of grid
                gtop = y - int(ght / 2);
                gleft = x - int(gwd / 2);

                // for triangular type rules ensure pattern placement is on a 2x2 grid
                if (grid_type == TRI_GRID) {
                    gtop &= ~1;
                    gleft &= ~1;
                }

                gbottom = gtop + ghtm1;
                gright = gleft + gwdm1;
                // set bigint versions of grid edges (used by GUI code)
                gridtop = gtop;
                gridleft = gleft;
                gridbottom = gbottom;
                gridright = gright;
            } else {
                int up = y < gtop ? gtop - y : 0;
                int down = y > gbottom ? y - gbottom : 0;
                int left = x < gleft ? gleft - x : 0;
                int right = x > gright ? x - gright : 0;
                
                // if the down or right amount is 1 then it's likely a pattern file
                // is being loaded, so increase the amount to reduce the number of
                // resize_grids calls and speed up the loading time
                if (down == 1) down = 10;
                if (right == 1) right = 10;
                
                const char* errmsg = resize_grids(up, down, left, right);
                if (errmsg) {
                    if (show_warning) lifewarning(errmsg);
                    // prevent further warning messages until endofpattern is called
                    // (this avoids user having to close thousands of dialog boxes
                    // if they attempted to paste a large pattern)
                    show_warning = false;
                    return -1;
                }
            }
        }
    } else {
        // check if x,y is outside bounded universe
        if (x < gleft || x > gright) return -1;
        if (y < gtop || y > gbottom) return -1;
    }

    // set x,y cell in currgrid
    int gx = x - gleft;
    int gy = y - gtop;
    unsigned char* cellptr = currgrid + gy * outerwd + gx;
    int oldstate = *cellptr;
    if (newstate != oldstate) {
        *cellptr = (unsigned char)newstate;
        // population might change
        if (oldstate == 0 && newstate > 0) {
            population++;
            if (gx < minx) minx = gx;
            if (gx > maxx) maxx = gx;
            if (gy < miny) miny = gy;
            if (gy > maxy) maxy = gy;
        } else if (oldstate > 0 && newstate == 0) {
            population--;
            if (population == 0) empty_boundaries();
        }
    }
    
    return 0;
}

// -----------------------------------------------------------------------------

// Get the state of the cell at the given location.

int ltlalgo::getcell(int x, int y)
{
    if (unbounded) {
        // cell outside grid is dead
        if (x < gleft || x > gright) return 0;
        if (y < gtop || y > gbottom) return 0;
    } else {
        // error if x,y is outside bounded universe
        if (x < gleft || x > gright) return -1;
        if (y < gtop || y > gbottom) return -1;
    }

    // get x,y cell in currgrid
    unsigned char* cellptr = currgrid + (y - gtop) * outerwd + (x - gleft);
    return *cellptr;
}

// -----------------------------------------------------------------------------

// Return the distance to the next non-zero cell in the given row,
// or -1 if there is none.

int ltlalgo::nextcell(int x, int y, int& v)
{
    if (population == 0) return -1;

    // check if y is outside grid
    if (y < gtop || y > gbottom) return -1;
    
    // check if x is outside right edge
    if (x > gright) return -1;

    // init distance
    int d = 0;
    
    // if x is outside left edge then set it to gleft and increase d
    // (this is necessary in case the user makes a selection outside
    // gleft when the universe is unbounded)
    if (x < gleft) {
        d = gleft - x;
        x = gleft;
    }
    
    // get x,y cell in currgrid
    unsigned char* cellptr = currgrid + (y - gtop) * outerwd + (x - gleft);
    
    do {
        v = *cellptr;
        if (v > 0) return d;    // found a non-zero cell
        d++;
        cellptr++;
        x++;
    } while (x <= gright);
    
    return -1;
}

// -----------------------------------------------------------------------------

static bigint bigpop;

const bigint& ltlalgo::getPopulation()
{
    bigpop = population;
    return bigpop;
}

// -----------------------------------------------------------------------------

int ltlalgo::isEmpty()
{
    return population == 0 ? 1 : 0;
}

// -----------------------------------------------------------------------------

void ltlalgo::update_current_grid(unsigned char &state, int ncount)
{
    // return the state of the cell based on the neighbor count
    if (state == 0) {
        // this cell is dead
        if (births[ncount]) {
            // new cell is born
            state = 1;
            population++;
        }
    } else if (state == 1) {
        // this cell is alive
        if (!survivals[ncount]) {
            // this cell doesn't survive
            if (maxCellStates > 2) {
                // cell decays to state 2
                state = 2;
            } else {
                // cell dies
                state = 0;
                population--;
                if (population == 0) empty_boundaries();
            }
        }
    } else {
        // state is > 1 so this cell will eventually die
        if (state + 1 < maxCellStates) {
            state++;
        } else {
            // cell dies
            state = 0;
            population--;
            if (population == 0) empty_boundaries();
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::update_next_grid(int x, int y, int xyoffset, int ncount)
{
    // x,y cell in nextgrid might change based on the given neighborhood count
    unsigned char state = *(currgrid + xyoffset);
    if (state == 0) {
        // this cell is dead
        if (ncount >= 0 && births[ncount]) {
            // new cell is born in nextgrid
            unsigned char* nextcell = nextgrid + xyoffset;
            *nextcell = 1;
            population++;
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    } else if (state == 1) {
        // this cell is alive
        if (ncount >= 0 && survivals[ncount]) {
            // cell survives so copy into nextgrid
            unsigned char* nextcell = nextgrid + xyoffset;
            *nextcell = 1;
            // population doesn't change but pattern limits in nextgrid might
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        } else if (maxCellStates > 2) {
            // cell decays to state 2
            unsigned char* nextcell = nextgrid + xyoffset;
            *nextcell = 2;
            // population doesn't change but pattern limits in nextgrid might
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        } else {
            // cell dies
            population--;
            if (population == 0) empty_boundaries();
        }
    } else {
        // state is > 1 so this cell will eventually die
        if (state + 1 < maxCellStates) {
            unsigned char* nextcell = nextgrid + xyoffset;
            *nextcell = state + 1;
            // population doesn't change but pattern limits in nextgrid might
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        } else {
            // cell dies
            population--;
            if (population == 0) empty_boundaries();
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::faster_Moore_bounded(int mincol, int minrow, int maxcol, int maxrow)
{
    // use Adam P. Goucher's algorithm to calculate Moore neighborhood counts
    // in a bounded universe; note that currgrid is surrounded by a border that
    // might contain live cells (the border is range+1 cells thick and the
    // outermost cells are always dead)
    
    // the given limits are relative to currgrid so we need to add border
    // so they are relative to outergrid1, and then expand them by range
    int bmr = border - range;
    int bpr = border + range;
    minrow += bmr;
    mincol += bmr;
    maxrow += bpr;
    maxcol += bpr;

    // calculate cumulative counts for each column and store in colcounts

    unsigned char* cellptr = outergrid1 + minrow * outerwd + mincol;
    int* ccptr = colcounts + minrow * outerwd + mincol;
    int *prevptr = NULL;
    int width = (maxcol - mincol + 1);
    int nextrow = outerwd - width;
    int rowcount = 0;

    for (int j = mincol; j <= maxcol; j++) {
        if (*cellptr == 1) rowcount++;
        *ccptr = rowcount;
        cellptr++;
        ccptr++;
    }
    cellptr += nextrow;
    ccptr += nextrow;
    prevptr = ccptr - outerwd;
    for (int i = minrow + 1; i <= maxrow; i++) {
        rowcount = 0;
        for (int j = mincol; j <= maxcol; j++) {
            if (*cellptr == 1) rowcount++;
            *ccptr = *prevptr + rowcount;
            cellptr++;
            ccptr++;
            prevptr++;
        }
        cellptr += nextrow;
        ccptr += nextrow;
        prevptr += nextrow;
    }
    
    // restore given limits (necessary for update_current_grid calls)
    minrow -= bmr;
    mincol -= bmr;
    maxrow -= bpr;
    maxcol -= bpr;

    // calculate final neighborhood counts using values in colcounts
    // and update the corresponding cells in current grid
    
    int* colptr = colcounts + (minrow + bpr) * outerwd;
    ccptr = colptr + mincol + bpr;
    unsigned char* stateptr = currgrid + minrow*outerwd+mincol;
    unsigned char state = *stateptr;
    update_current_grid(state, *ccptr);
    *stateptr = state;
    if (state) {
        if (mincol < minx) minx = mincol;
        if (mincol > maxx) maxx = mincol;
        if (minrow < miny) miny = minrow;
        if (minrow > maxy) maxy = minrow;
    }

    bool rowchanged = false;
    int bmrm1 = border - range - 1;
    stateptr = currgrid + minrow*outerwd + mincol+1;
    for (int j = mincol+1; j <= maxcol; j++) {
        // do i == minrow
        int* ccptr1 = colptr + (j + bpr);
        int* ccptr2 = colptr + (j + bmrm1);
        state = *stateptr;
        update_current_grid(state, *ccptr1 - *ccptr2);
        *stateptr++ = state;
        if (state) {
            if (j < minx) minx = j;
            if (j > maxx) maxx = j;
            rowchanged = true;
        }
    }
    if (rowchanged) {
        if (minrow < miny) miny = minrow;
        if (minrow > maxy) maxy = minrow;
    }
    
    bool colchanged = false;
    colptr = colcounts + mincol + bpr;
    stateptr = currgrid + (minrow+1)*outerwd + mincol;
    for (int i = minrow+1; i <= maxrow; i++) {
        // do j == mincol
        int* ccptr1 = colptr + (i + bpr) * outerwd;
        int* ccptr2 = colptr + (i + bmrm1) * outerwd;
        state = *stateptr;
        update_current_grid(state, *ccptr1 - *ccptr2);
        *stateptr = state;
        stateptr += outerwd;
        if (state) {
            if (i < miny) miny = i;
            if (i > maxy) maxy = i;
            colchanged = true;
        }
    }
    if (colchanged) {
        if (mincol < minx) minx = mincol;
        if (mincol > maxx) maxx = mincol;
    }
    
    rowchanged = false;
    for (int i = minrow+1; i <= maxrow; i++) {
        int* ipr = colcounts + (i + bpr) * outerwd;
        int* imrm1 = colcounts + (i + bmrm1) * outerwd;
        stateptr = currgrid + i*outerwd + mincol+1;
        for (int j = mincol+1; j <= maxcol; j++) {
            int jpr = j + bpr;
            int jmrm1 = j + bmrm1;
            int* ccptr1 = ipr + jpr;
            int* ccptr2 = imrm1 + jmrm1;
            int* ccptr3 = ipr + jmrm1;
            int* ccptr4 = imrm1 + jpr;
            state = *stateptr;
            update_current_grid(state, *ccptr1 + *ccptr2 - *ccptr3 - *ccptr4);
            *stateptr++ = state;
            if (state) {
                if (j < minx) minx = j;
                if (j > maxx) maxx = j;
                rowchanged = true;
            }
        }
        if (rowchanged) {
            if (i < miny) miny = i;
            if (i > maxy) maxy = i;
            rowchanged = false;
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::faster_Moore_bounded2(int mincol, int minrow, int maxcol, int maxrow)
{
    // use Adam P. Goucher's algorithm to calculate Moore neighborhood counts
    // in a bounded universe; note that currgrid is surrounded by a border that
    // might contain live cells (the border is range+1 cells thick and the
    // outermost cells are always dead)
    
    // the given limits are relative to currgrid so we need to add border
    // so they are relative to outergrid1, and then expand them by range
    int bmr = border - range;
    int bpr = border + range;
    minrow += bmr;
    mincol += bmr;
    maxrow += bpr;
    maxcol += bpr;

    // calculate cumulative counts for each column and store in colcounts

    unsigned char* cellptr = outergrid1 + minrow * outerwd + mincol;
    int* ccptr = colcounts + minrow * outerwd + mincol;
    int *prevptr = NULL;
    int width = (maxcol - mincol + 1);
    int nextrow = outerwd - width;
    int rowcount = 0;

    // compute 4 cell offset
    int offset = (4 - ((g_uintptr_t)cellptr & 3)) & 3;
    if (offset > width) offset = width;

    // process in 4 cell chunks
    int chunks = (width - offset) >> 2;
    int remainder = (width - offset) - (chunks << 2);
    int j = 0;

    // process cells in the first row

    // process cells up to the first 4 cell chunk
    for (j = 0; j < offset; j++) {
        rowcount += *cellptr++;
        *ccptr++ = rowcount;
    }

    // process any 4 cell chunks
    unsigned int *lcellptr = (unsigned int*)cellptr;
    for (j = 0; j < chunks; j++) {
        if (*lcellptr++) {
            rowcount += *cellptr++;
            *ccptr++ = rowcount;
            rowcount += *cellptr++;
            *ccptr++ = rowcount;
            rowcount += *cellptr++;
            *ccptr++ = rowcount;
            rowcount += *cellptr++;
            *ccptr++ = rowcount;
        } else {
            *ccptr++ = rowcount;
            *ccptr++ = rowcount;
            *ccptr++ = rowcount;
            *ccptr++ = rowcount;
            cellptr += 4;
        }
    }

    // process any remaining cells
    for (j = 0; j < remainder; j++) {
        rowcount += *cellptr++;
        *ccptr++ = rowcount;
    }
    cellptr += nextrow;
    ccptr += nextrow;
    prevptr = ccptr - outerwd;

    // process the remaining rows of cells

    for (int i = minrow + 1; i <= maxrow; i++) {
        rowcount = 0;

        // compute 4 cell offset
        offset = (4 - ((g_uintptr_t)cellptr & 3)) & 3;
        if (offset > width) offset = width;

        // process in 4 cell chunks
        chunks = (width - offset) >> 2;
        remainder = (width - offset) - (chunks << 2);

        // process cells up to the first 4 cell chunk
        for (j = 0; j < offset; j++) {
            rowcount += *cellptr++;
            *ccptr++ = *prevptr++ + rowcount;
        }
        lcellptr = (unsigned int*)cellptr;

        // process any 4 cell chunks
        for (j = 0; j < chunks; j++) {
            // check if any of the cells are alive
            if (*lcellptr++) {
                rowcount += *cellptr++;
                *ccptr++ = *prevptr++ + rowcount;
                rowcount += *cellptr++;
                *ccptr++ = *prevptr++ + rowcount;
                rowcount += *cellptr++;
                *ccptr++ = *prevptr++ + rowcount;
                rowcount += *cellptr++;
                *ccptr++ = *prevptr++ + rowcount;
           } else {
                *ccptr++ = *prevptr++ + rowcount;
                *ccptr++ = *prevptr++ + rowcount;
                *ccptr++ = *prevptr++ + rowcount;
                *ccptr++ = *prevptr++ + rowcount;
                cellptr += 4;
            }
        }

        // process any remaining cells
        for (j = 0; j < remainder; j++) {
            rowcount += *cellptr++;
            *ccptr++ = *prevptr++ + rowcount;
        }
        // next row
        cellptr += nextrow;
        ccptr += nextrow;
        prevptr += nextrow;
    }
    
    // restore given limits (necessary for update_current_grid calls)
    minrow -= bmr;
    mincol -= bmr;
    maxrow -= bpr;
    maxcol -= bpr;

    // calculate final neighborhood counts using values in colcounts
    // and update the corresponding cells in current grid
    
    int* colptr = colcounts + (minrow + bpr) * outerwd;
    ccptr = colptr + mincol + bpr;
    unsigned char* stateptr = currgrid + minrow*outerwd+mincol;
    int ncount = *ccptr;
    if (*stateptr == 0) {
        if (births[ncount]) {
            *stateptr = 1;
            population++;
            minx = mincol;
            maxx = mincol;
            miny = minrow;
            maxy = minrow;
        }
    } else {
        if (!survivals[ncount]) {
            *stateptr = 0;
            population--;
        }
        else {
            minx = mincol;
            maxx = maxcol;
            miny = minrow;
            maxy = maxrow;
        }
    }

    bool rowchanged = false;
    int bmrm1 = border - range - 1;
    stateptr = currgrid + minrow*outerwd + mincol+1;
    int* ccptr1 = colptr + (mincol+1 + bpr);
    int* ccptr2 = colptr + (mincol+1 + bmrm1);
    for (j = mincol+1; j <= maxcol; j++) {
        // do i == minrow
        ncount = *ccptr1++ - *ccptr2++;
        if (*stateptr == 0) {
            if (births[ncount]) {
                *stateptr = 1;
                population++;
                if (j < minx) minx = j;
                if (j > maxx) maxx = j;
                rowchanged = true;
            }
        } else {
            if (!survivals[ncount]) {
                *stateptr = 0;
                population--;
            }
            else {
                if (j < minx) minx = j;
                if (j > maxx) maxx = j;
                rowchanged = true;
            }
        }
        stateptr++;
    }
    if (rowchanged) {
        if (minrow < miny) miny = minrow;
        if (minrow > maxy) maxy = minrow;
    }
    
    bool colchanged = false;
    colptr = colcounts + mincol + bpr;
    stateptr = currgrid + (minrow+1)*outerwd + mincol;
    ccptr1 = colptr + (minrow+1 + bpr) * outerwd;
    ccptr2 = colptr + (minrow+1 + bmrm1) * outerwd;
    for (int i = minrow+1; i <= maxrow; i++) {
        // do j == mincol
        ncount = *ccptr1 - *ccptr2;
        if (*stateptr == 0) {
            if (births[ncount]) {
                *stateptr = 1;
                population++;
                if (i < miny) miny = i;
                if (i > maxy) maxy = i;
                colchanged = true;
            }
        } else {
            if (!survivals[ncount]) {
                *stateptr = 0;
                population--;
            }
            else {
                if (i < miny) miny = i;
                if (i > maxy) maxy = i;
                colchanged = true;
            }
        }
        stateptr += outerwd;
        ccptr1 += outerwd;
        ccptr2 += outerwd;
    }
    if (colchanged) {
        if (mincol < minx) minx = mincol;
        if (mincol > maxx) maxx = mincol;
    }
    
    rowchanged = false;
    for (int i = minrow+1; i <= maxrow; i++) {
        int* ipr = colcounts + (i + bpr) * outerwd;
        int* imrm1 = colcounts + (i + bmrm1) * outerwd;
        int jpr = mincol+1 + bpr;
        int jmrm1 = mincol+1 + bmrm1;
        ccptr1 = ipr + jpr;
        ccptr2 = imrm1 + jmrm1;
        int* ccptr3 = ipr + jmrm1;
        int* ccptr4 = imrm1 + jpr;
        stateptr = currgrid + i*outerwd + mincol+1;
        for (j = mincol+1; j <= maxcol; j++) {
            ncount = *ccptr1++ + *ccptr2++ - *ccptr3++ - *ccptr4++;
            if (*stateptr == 0) {
                if (births[ncount]) {
                    *stateptr = 1;
                    population++;
                    if (j < minx) minx = j;
                    if (j > maxx) maxx = j;
                    rowchanged = true;
                }
            } else {
                if (!survivals[ncount]) {
                    *stateptr = 0;
                    population--;
                }
                else {
                    if (j < minx) minx = j;
                    if (j > maxx) maxx = j;
                    rowchanged = true;
                }
            }
            stateptr++;
        }
        if (rowchanged) {
            if (i < miny) miny = i;
            if (i > maxy) maxy = i;
            rowchanged = false;
        }
    }
    if (population == 0) empty_boundaries();
}

// -----------------------------------------------------------------------------

void ltlalgo::faster_Moore_unbounded(int mincol, int minrow, int maxcol, int maxrow)
{
    // use Adam P. Goucher's algorithm to calculate Moore neighborhood counts
    // in an unbounded universe; note that we can safely assume there is at least
    // a 2*range border of dead cells surrounding the pattern
    
    // temporarily expand the given limits
    minrow -= range;
    mincol -= range;
    maxrow += range;
    maxcol += range;
    
    int r2 = range * 2;
    int minrowpr2 = minrow+r2;
    int mincolpr2 = mincol+r2;
    
    // put zeros in top 2*range rows of colcounts
    for (int i = minrow; i < minrowpr2; i++) {
        int* ccptr = colcounts + i * outerwd + mincol;
        for (int j = mincol; j <= maxcol; j++) {
            *ccptr++ = 0;
        }
    }

    // put zeros in left 2*range columns of colcounts
    for (int j = mincol; j < mincolpr2; j++) {
        int* ccptr = colcounts + minrowpr2 * outerwd + j;
        for (int i = minrowpr2; i <= maxrow; i++) {
            *ccptr = 0;
            ccptr += outerwd;
        }
    }

    unsigned char* cellptr = currgrid + minrowpr2 * outerwd + mincolpr2;
    int* ccptr = colcounts + minrowpr2 * outerwd + mincolpr2;
    int* prevptr = ccptr - outerwd;
    int width = (maxcol - mincolpr2 + 1);
    int nextrow = outerwd - width;
    int rowcount = 0;
    int j = 0;

    for (int i = minrowpr2; i <= maxrow; i++) {
        rowcount = 0;
        for (j = mincolpr2; j <= maxcol; j++) {
            if (*cellptr == 1) rowcount++;
            *ccptr = *prevptr + rowcount;
            cellptr++;
            ccptr++;
            prevptr++;
        }
        cellptr += nextrow;
        ccptr += nextrow;
        prevptr += nextrow;
    }

    // restore given limits
    minrow += range;
    mincol += range;
    maxrow -= range;
    maxcol -= range;

    // calculate final neighborhood counts using values in colcounts
    // and update the corresponding cells in current grid
    
    int* colptr = colcounts + (minrow + range) * outerwd;
    ccptr = colptr + mincol + range;
    unsigned char* stateptr = currgrid + minrow*outerwd+mincol;
    unsigned char state = *stateptr;
    update_current_grid(state, *ccptr);
    *stateptr = state;
    if (state) {
        if (mincol < minx) minx = mincol;
        if (mincol > maxx) maxx = mincol;
        if (minrow < miny) miny = minrow;
        if (minrow > maxy) maxy = minrow;
    }

    bool rowchanged = false;
    int rangep1 = range + 1;
    stateptr = currgrid + minrow*outerwd + mincol+1;
    for (j = mincol+1; j <= maxcol; j++) {
        // do i == minrow
        int* ccptr1 = colptr + (j+range);
        int* ccptr2 = colptr + (j-rangep1);
        state = *stateptr;
        update_current_grid(state, *ccptr1 - *ccptr2);
        *stateptr++ = state;
        if (state) {
            if (j < minx) minx = j;
            if (j > maxx) maxx = j;
            rowchanged = true;
        }
    }
    if (rowchanged) {
        if (minrow < miny) miny = minrow;
        if (minrow > maxy) maxy = minrow;
    }

    bool colchanged = false;
    colptr = colcounts + mincol + range;
    stateptr = currgrid + (minrow+1)*outerwd + mincol;
    for (int i = minrow+1; i <= maxrow; i++) {
        // do j == mincol
        int* ccptr1 = colptr + (i+range) * outerwd;
        int* ccptr2 = colptr + (i-rangep1) * outerwd;
        state = *stateptr;
        update_current_grid(state, *ccptr1 - *ccptr2);
        *stateptr = state;
        stateptr += outerwd;
        if (state) {
            if (i < miny) miny = i;
            if (i > maxy) maxy = i;
            colchanged = true;
        }
    }
    if (colchanged) {
        if (mincol < minx) minx = mincol;
        if (mincol > maxx) maxx = mincol;
    }
    
    rowchanged = false;
    for (int i = minrow+1; i <= maxrow; i++) {
        int* ipr = colcounts + (i+range) * outerwd;
        int* imrm1 = colcounts + (i-rangep1) * outerwd;
        stateptr = currgrid + i*outerwd + mincol+1;
        for (j = mincol+1; j <= maxcol; j++) {
            int jpr = j+range;
            int jmrm1 = j-rangep1;
            int* ccptr1 = ipr + jpr;
            int* ccptr2 = imrm1 + jmrm1;
            int* ccptr3 = ipr + jmrm1;
            int* ccptr4 = imrm1 + jpr;
            state = *stateptr;
            update_current_grid(state, *ccptr1 + *ccptr2 - *ccptr3 - *ccptr4);
            *stateptr++ = state;
            if (state) {
                if (j < minx) minx = j;
                if (j > maxx) maxx = j;
                rowchanged = true;
            }
        }
        if (rowchanged) {
            if (i < miny) miny = i;
            if (i > maxy) maxy = i;
            rowchanged = false;
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::faster_Moore_unbounded2(int mincol, int minrow, int maxcol, int maxrow)
{
    // use Adam P. Goucher's algorithm to calculate Moore neighborhood counts
    // in an unbounded universe; note that we can safely assume there is at least
    // a 2*range border of dead cells surrounding the pattern
    
    // temporarily expand the given limits
    minrow -= range;
    mincol -= range;
    maxrow += range;
    maxcol += range;
    
    int r2 = range * 2;
    int minrowpr2 = minrow+r2;
    int mincolpr2 = mincol+r2;
    
    // put zeros in top 2*range rows of colcounts
    for (int i = minrow; i < minrowpr2; i++) {
        int* ccptr = colcounts + i * outerwd + mincol;
        for (int j = mincol; j <= maxcol; j++) {
            *ccptr++ = 0;
        }
    }

    // put zeros in left 2*range columns of colcounts
    for (int j = mincol; j < mincolpr2; j++) {
        int* ccptr = colcounts + minrowpr2 * outerwd + j;
        for (int i = minrowpr2; i <= maxrow; i++) {
            *ccptr = 0;
            ccptr += outerwd;
        }
    }

    // calculate cumulative counts for each column and store in colcounts

    unsigned char* cellptr = currgrid + minrowpr2 * outerwd + mincolpr2;
    int* ccptr = colcounts + minrowpr2 * outerwd + mincolpr2;
    int* prevptr = ccptr - outerwd;
    int width = (maxcol - mincolpr2 + 1);
    int nextrow = outerwd - width;
    int rowcount = 0;

    // compute 4 cell offset
    int offset = (4 - ((g_uintptr_t)cellptr & 3)) & 3;
    if (offset > width) offset = width;

    // process in 4 cell chunks
    int chunks = (width - offset) >> 2;
    int remainder = (width - offset) - (chunks << 2);
    int j = 0;

    // process cells in the first row

    // process cells up to the first 4 cell chunk
    for (j = 0; j < offset; j++) {
        rowcount += *cellptr++;
        *ccptr++ = rowcount;
    }

    // process any 4 cell chunks
    unsigned int *lcellptr = (unsigned int*)cellptr;
    for (j = 0; j < chunks; j++) {
        if (*lcellptr++) {
            rowcount += *cellptr++;
            *ccptr++ = rowcount;
            rowcount += *cellptr++;
            *ccptr++ = rowcount;
            rowcount += *cellptr++;
            *ccptr++ = rowcount;
            rowcount += *cellptr++;
            *ccptr++ = rowcount;
        } else {
            *ccptr++ = rowcount;
            *ccptr++ = rowcount;
            *ccptr++ = rowcount;
            *ccptr++ = rowcount;
            cellptr += 4;
        }
    }

    // process any remaining cells
    for (j = 0; j < remainder; j++) {
        rowcount += *cellptr++;
        *ccptr++ = rowcount;
    }
    cellptr += nextrow;
    ccptr += nextrow;
    prevptr = ccptr - outerwd;

    // process the remaining rows of cells

    for (int i = minrowpr2 + 1; i <= maxrow; i++) {
        rowcount = 0;

        // compute 4 cell offset
        offset = (4 - ((g_uintptr_t)cellptr & 3)) & 3;
        if (offset > width) offset = width;

        // process in 4 cell chunks
        chunks = (width - offset) >> 2;
        remainder = (width - offset) - (chunks << 2);

        // process cells up to the first 4 cell chunk
        for (j = 0; j < offset; j++) {
            rowcount += *cellptr++;
            *ccptr++ = *prevptr++ + rowcount;
        }
        lcellptr = (unsigned int*)cellptr;

        // process any 4 cell chunks
        for (j = 0; j < chunks; j++) {
            // check if any of the cells are alive
            if (*lcellptr++) {
                rowcount += *cellptr++;
                *ccptr++ = *prevptr++ + rowcount;
                rowcount += *cellptr++;
                *ccptr++ = *prevptr++ + rowcount;
                rowcount += *cellptr++;
                *ccptr++ = *prevptr++ + rowcount;
                rowcount += *cellptr++;
                *ccptr++ = *prevptr++ + rowcount;
           } else {
                *ccptr++ = *prevptr++ + rowcount;
                *ccptr++ = *prevptr++ + rowcount;
                *ccptr++ = *prevptr++ + rowcount;
                *ccptr++ = *prevptr++ + rowcount;
                cellptr += 4;
            }
        }

        // process any remaining cells
        for (j = 0; j < remainder; j++) {
            rowcount += *cellptr++;
            *ccptr++ = *prevptr++ + rowcount;
        }
        // next row
        cellptr += nextrow;
        ccptr += nextrow;
        prevptr += nextrow;
    }

    // restore given limits
    minrow += range;
    mincol += range;
    maxrow -= range;
    maxcol -= range;

    // calculate final neighborhood counts using values in colcounts
    // and update the corresponding cells in current grid
    
    int* colptr = colcounts + (minrow + range) * outerwd;
    ccptr = colptr + mincol + range;
    unsigned char* stateptr = currgrid + minrow*outerwd+mincol;
    int ncount = *ccptr;
    if (*stateptr == 0) {
        if (births[ncount]) {
            *stateptr = 1;
            population++;
            minx = mincol;
            maxx = mincol;
            miny = minrow;
            maxy = minrow;
        }
    } else {
        if (!survivals[ncount]) {
            *stateptr = 0;
            population--;
        }
        else {
            minx = mincol;
            maxx = maxcol;
            miny = minrow;
            maxy = maxrow;
        }
    }

    bool rowchanged = false;
    int rangep1 = range + 1;
    stateptr = currgrid + minrow*outerwd + mincol+1;
    int* ccptr1 = colptr + (mincol+1 + range);
    int* ccptr2 = colptr + (mincol+1 - rangep1);
    for (j = mincol+1; j <= maxcol; j++) {
        // do i == minrow
        ncount = *ccptr1++ - *ccptr2++;
        if (*stateptr == 0) {
            if (births[ncount]) {
                *stateptr = 1;
                population++;
                if (j < minx) minx = j;
                if (j > maxx) maxx = j;
                rowchanged = true;
            }
        } else {
            if (!survivals[ncount]) {
                *stateptr = 0;
                population--;
            }
            else {
                if (j < minx) minx = j;
                if (j > maxx) maxx = j;
                rowchanged = true;
            }
        }
        stateptr++;
    }
    if (rowchanged) {
        if (minrow < miny) miny = minrow;
        if (minrow > maxy) maxy = minrow;
    }

    bool colchanged = false;
    colptr = colcounts + mincol + range;
    stateptr = currgrid + (minrow+1)*outerwd + mincol;
    ccptr1 = colptr + (minrow+1+range) * outerwd;
    ccptr2 = colptr + (minrow+1-rangep1) * outerwd;
    for (int i = minrow+1; i <= maxrow; i++) {
        // do j == mincol
        ncount = *ccptr1 - *ccptr2;
        if (*stateptr == 0) {
            if (births[ncount]) {
                *stateptr = 1;
                population++;
                if (i < miny) miny = i;
                if (i > maxy) maxy = i;
                colchanged = true;
            }
        } else {
            if (!survivals[ncount]) {
                *stateptr = 0;
                population--;
            }
            else {
                if (i < miny) miny = i;
                if (i > maxy) maxy = i;
                colchanged = true;
            }
        }
        stateptr += outerwd;
        ccptr1 += outerwd;
        ccptr2 += outerwd;
    }
    if (colchanged) {
        if (mincol < minx) minx = mincol;
        if (mincol > maxx) maxx = mincol;
    }
    
    rowchanged = false;
    for (int i = minrow+1; i <= maxrow; i++) {
        int* ipr = colcounts + (i+range) * outerwd;
        int* imrm1 = colcounts + (i-rangep1) * outerwd;
        int jpr = mincol+1+range;
        int jmrm1 = mincol+1-rangep1;
        ccptr1 = ipr + jpr;
        ccptr2 = imrm1 + jmrm1;
        int* ccptr3 = ipr + jmrm1;
        int* ccptr4 = imrm1 + jpr;
        stateptr = currgrid + i*outerwd + mincol+1;
        for (j = mincol+1; j <= maxcol; j++) {
            ncount = *ccptr1++ + *ccptr2++ - *ccptr3++ - *ccptr4++;
            if (*stateptr == 0) {
                if (births[ncount]) {
                    *stateptr = 1;
                    population++;
                    if (j < minx) minx = j;
                    if (j > maxx) maxx = j;
                    rowchanged = true;
                }
            } else {
                if (!survivals[ncount]) {
                    *stateptr = 0;
                    population--;
                }
                else {
                    if (j < minx) minx = j;
                    if (j > maxx) maxx = j;
                    rowchanged = true;
                }
            }
            stateptr++;
        }
        if (rowchanged) {
            if (i < miny) miny = i;
            if (i > maxy) maxy = i;
            rowchanged = false;
        }
    }
    if (population == 0) empty_boundaries();
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Moore(int mincol, int minrow, int maxcol, int maxrow)
{
    if (range == 1) {
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * outerwd;
            unsigned char* topy = currgrid + (y - 1) * outerwd;
            for (int x = mincol; x <= maxcol; x++) {
                // count the state-1 neighbors within the current range
                // using the extended Moore neighborhood with no edge checks
                int ncount = 0;
                unsigned char* cellptr = topy + (x - 1);
                if (*cellptr++ == 1) ncount++;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr   == 1) ncount++;
                cellptr += outerwd;
                if (*cellptr   == 1) ncount++;
                if (*--cellptr == 1) ncount++;
                if (*--cellptr == 1) ncount++;
                cellptr += outerwd;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr   == 1) ncount++;
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    } else {
        // range > 1
        int rightcol = 2 * range;
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * outerwd;
            int ymrange = y - range;
            int yprange = y + range;
            unsigned char* topy = currgrid + ymrange * outerwd;
            
            // for the 1st cell in this row we count the state-1 cells in the
            // extended Moore neighborhood and remember the column counts
            int colcount[MAXNCOLS];
            int xmrange = mincol - range;
            int xprange = mincol + range;
            int ncount = 0;
            for (int i = xmrange; i <= xprange; i++) {
                unsigned char* cellptr = topy + i;
                int col = i - xmrange;
                colcount[col] = 0;
                for (int j = ymrange; j <= yprange; j++) {
                    if (*cellptr == 1) colcount[col]++;
                    cellptr += outerwd;
                }
                ncount += colcount[col];
            }
            
            // we now have the neighborhood counts in each column;
            // eg. 7 columns if range == 3:
            //   ---------------
            //   | | | | | | | |
            //   | | | | | | | |
            //   |0|1|2|3|4|5|6|
            //   | | | | | | | |
            //   | | | | | | | |
            //   ---------------
            
            update_next_grid(mincol, y, yoffset+mincol, ncount);
            
            // for the remaining cells in this row we only need to update
            // the count in the right column of the new neighborhood
            // and shift the other column counts to the left
            topy += range;
            for (int x = mincol+1; x <= maxcol; x++) {
                // get count in right column
                int rcount = 0;
                unsigned char* cellptr = topy + x;
                for (int j = ymrange; j <= yprange; j++) {
                    if (*cellptr == 1) rcount++;
                    cellptr += outerwd;
                }
                
                ncount = rcount;
                for (int i = 1; i <= rightcol; i++) {
                    ncount += colcount[i];
                    colcount[i-1] = colcount[i];
                }
                colcount[rightcol] = rcount;
                
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Shaped(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        int ymrange = y - range;
        int yprange = y + range;
        
        // for the 1st cell in this row we count the state-1 cells in the
        // shaped neighborhood and remember the column counts
        int ncount = 0;
        unsigned char* cellptr = currgrid + ymrange * outerwd ;
        for (int j = ymrange; j <= yprange; j++, cellptr += outerwd) {
            int xmrange = mincol - shape[j-ymrange] ;
            int xprange = mincol + shape[j-ymrange] ;
            for (int i = xmrange; i <= xprange; i++)
                if (cellptr[i] == 1) ncount++ ;
        }
           
        update_next_grid(mincol, y, yoffset+mincol, ncount);
        
        // for the remaining cells in this row we only need subtract
        // points in relevant rows and add points in other relevant
        // rows according to the shape.
        cellptr = currgrid + ymrange * outerwd ;
        for (int x = mincol+1; x <= maxcol; x++) {
            unsigned char* cp = cellptr ;
            for (int j = ymrange; j <= yprange; j++, cp += outerwd) {
               int xmrange = x - shape[j-ymrange] ;
               int xprange = x + shape[j-ymrange] ;
               if (cp[xmrange-1] == 1)
                  ncount-- ;
               if (cp[xprange] == 1)
                  ncount++ ;
            }
            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

int ltlalgo::getcount(int i, int j)
{
    // From Dean Hickerson:
    // C[i][j] is the sum of G[i'][j'] for all cells between northwest and northeast from
    // (i,j) with i'+j' == i+j (mod 2).  I.e. the sum of these:
    //
    // ...                                    ...                                    ...
    //    G[i-3][j-3]           G[i-3][j-1]         G[i-3][j+1]           G[i-3][j+3]
    //               G[i-2][j-2]           G[i-2][j]           G[i-2][j+2]
    //                          G[i-1][j-1]         G[i-1][j+1]
    //                                      G[i][j]
    //
    // We only need to compute and store C[i][j] for  0 <= i < ncols,  0 <= j < nrows + floor((ncols-1)/2);
    // other values that we need are equal to one of these, as given by this function.
    //
    if (i < 0 || i+j < 0 || j-i >= ncols) {
        return 0;
    }
    if (j < 0 && i+j < ccht) {
        // return C[i+j][0]
        return *(colcounts + (i+j) * outerwd);
    }
    if (j >= ncols && j-i >= ncols-ccht) {
        // return C[i+ncols-1-j][ncols-1]
        return *(colcounts + (i+ncols-1-j) * outerwd + (ncols-1));
    }
    if (i < ccht) {
        // return C[i][j]
        return *(colcounts + i * outerwd + j);
    }
    if ((i-ccht+1)+j <= halfccwd) {
        // return C[ccht-1][i-ccht+1+j]
        return *(colcounts + (ccht-1) * outerwd + (i-ccht+1+j));
    }
    if (j-(i-ccht+1) >= halfccwd) {
        // return C[ccht-1][j-(i-ccht+1)]
        return *(colcounts + (ccht-1) * outerwd + (j-(i-ccht+1)));
    }
    // return C[ccht-1][halfccwd + ((i+j+ccht+halfccwd+1) % 2)]
    return *(colcounts + (ccht-1) * outerwd + (halfccwd + ((i+j+ccht+halfccwd+1) % 2)));
}

// -----------------------------------------------------------------------------

void ltlalgo::faster_Neumann_bounded(int mincol, int minrow, int maxcol, int maxrow)
{
    // use Dean Hickerson's algorithm (based on Adam P. Goucher's algorithm for the
    // Moore neighborhood) to calculate extended von Neumann neighborhood counts
    // in a bounded universe; note that currgrid is surrounded by a border that
    // might contain live cells (the border is range+1 cells thick and the
    // outermost cells are always dead)
    
    // the given limits are relative to currgrid so we need to add border
    // so they are relative to outergrid1, and then expand them by range
    int bmr = border - range;
    int bpr = border + range;
    minrow += bmr;
    mincol += bmr;
    maxrow += bpr;
    maxcol += bpr;
    
    // set variables used below and in getcount
    nrows = maxrow - minrow + 1;
    ncols = maxcol - mincol + 1;
    ccht = nrows + (ncols-1)/2;
    halfccwd = ncols/2;

    // calculate cumulative counts in top left corner of colcounts
    for (int i = 0; i < ccht; i++) {
        int* Coffset = colcounts + i * outerwd;
        unsigned char* Goffset = outergrid1 + (i + minrow) * outerwd;
        int im1 = i - 1;
        int im2 = im1 - 1;
        for (int j = 0; j < ncols; j++) {
            int* Cij = Coffset + j;
            *Cij = getcount(im1,j-1) + getcount(im1,j+1) - getcount(im2,j);
            if (i < nrows) {
                unsigned char* Gij = Goffset + j + mincol;
                if (*Gij == 1) *Cij += *Gij;
            }
        }
    }
    
    // set minrow and mincol for update_current_grid calls
    minrow -= border;
    mincol -= border;

    // calculate final neighborhood counts and update the corresponding cells in the grid
    bool rowchanged = false;
    for (int i = range; i < nrows-range; i++) {
        int im1 = i - 1;
        int ipr = i + range;
        int iprm1 = ipr - 1;
        int imrm1 = i - range - 1;
        int imrm2 = imrm1 - 1;
        int ipminrow = i + minrow;
        unsigned char* stateptr = currgrid + ipminrow*outerwd + range + mincol;
        for (int j = range; j < ncols-range; j++) {
            int jpr = j + range;
            int jmr = j - range;
            int n = getcount(ipr,j)   - getcount(im1,jpr+1) - getcount(im1,jmr-1) + getcount(imrm2,j) +
                    getcount(iprm1,j) - getcount(im1,jpr)   - getcount(im1,jmr)   + getcount(imrm1,j);
            unsigned char state = *stateptr;
            update_current_grid(state, n);
            *stateptr++ = state;
            if (state) {
                int jpmincol = j + mincol;
                if (jpmincol < minx) minx = jpmincol;
                if (jpmincol > maxx) maxx = jpmincol;
                rowchanged = true;
            }
        }
        if (rowchanged) {
            if (ipminrow < miny) miny = ipminrow;
            if (ipminrow > maxy) maxy = ipminrow;
            rowchanged = false;
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::faster_Neumann_unbounded(int mincol, int minrow, int maxcol, int maxrow)
{
    // use Dean Hickerson's algorithm (based on Adam P. Goucher's algorithm for the
    // Moore neighborhood) to calculate extended von Neumann neighborhood counts
    // in an unbounded universe; note that we can safely assume there is at least
    // a 2*range border of dead cells surrounding the pattern
    
    // set variables used below and in getcount
    nrows = maxrow - minrow + 1;
    ncols = maxcol - mincol + 1;
    ccht = nrows + (ncols-1)/2;
    halfccwd = ncols/2;

    // calculate cumulative counts in top left corner of colcounts
    for (int i = 0; i < ccht; i++) {
        int* Coffset = colcounts + i * outerwd;
        unsigned char* Goffset = outergrid1 + (i + minrow) * outerwd;
        int im1 = i - 1;
        int im2 = im1 - 1;
        for (int j = 0; j < ncols; j++) {
            int* Cij = Coffset + j;
            *Cij = getcount(im1,j-1) + getcount(im1,j+1) - getcount(im2,j);
            if (i < nrows) {
                unsigned char* Gij = Goffset + j + mincol;
                if (*Gij == 1) *Cij += *Gij;
            }
        }
    }

    // calculate final neighborhood counts and update the corresponding cells in the grid
    bool rowchanged = false;
    for (int i = 0; i < nrows; i++) {
        int im1 = i - 1;
        int ipr = i + range;
        int iprm1 = ipr - 1;
        int imrm1 = i - range - 1;
        int imrm2 = imrm1 - 1;
        int ipminrow = i + minrow;
        unsigned char* stateptr = currgrid + ipminrow*outerwd + mincol;
        for (int j = 0; j < ncols; j++) {
            int jpr = j + range;
            int jmr = j - range;
            int n = getcount(ipr,j)   - getcount(im1,jpr+1) - getcount(im1,jmr-1) + getcount(imrm2,j) +
                    getcount(iprm1,j) - getcount(im1,jpr)   - getcount(im1,jmr)   + getcount(imrm1,j);
            unsigned char state = *stateptr;
            update_current_grid(state, n);
            *stateptr++ = state;
            if (state) {
                int jpmincol = j + mincol;
                if (jpmincol < minx) minx = jpmincol;
                if (jpmincol > maxx) maxx = jpmincol;
                rowchanged = true;
            }
        }
        if (rowchanged) {
            if (ipminrow < miny) miny = ipminrow;
            if (ipminrow > maxy) maxy = ipminrow;
            rowchanged = false;
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Asterisk(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + (y - range) * outerwd;
        
        for (int x = mincol; x <= maxcol; x++) {
            int ncount = 0;
            unsigned char* cp1 = cellptr;
            for (int j = -range; j < 0; j++, cp1 += outerwd) {
                if (cp1[x] == 1)     ncount++;
                if (cp1[x + j] == 1) ncount++;
            }
            for (int j = -range; j <= range; j++) {
                if (cp1[x + j] == 1) ncount++;
            }
            cp1 += outerwd;
            for (int j = 1; j <= range; j++, cp1 += outerwd) {
                if (cp1[x] == 1)     ncount++;
                if (cp1[x + j] == 1) ncount++;
            }

            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Tripod(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + (y - range) * outerwd;
        
        for (int x = mincol; x <= maxcol; x++) {
            int ncount = 0;
            unsigned char* cp1 = cellptr;
            for (int j = -range; j < 0; j++, cp1 += outerwd) {
                if (cp1[x] == 1) ncount++;
            }
            for (int j = -range; j <= 0; j++) {
                if (cp1[x + j] == 1) ncount++;
            }
            cp1 += outerwd;
            for (int j = 1; j <= range; j++, cp1 += outerwd) {
                if (cp1[x + j] == 1) ncount++;
            }

            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Weighted(int mincol, int minrow, int maxcol, int maxrow)
{
    const int nsize = (range + range + 1);
    const int brow = (nsize - 1) * nsize;

    // check for weighted states
    if (stateweights == NULL) {
        // just weighted neighborhood
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * outerwd;
            unsigned char* cellptr = currgrid + (y - range) * outerwd;
            
            for (int x = mincol; x <= maxcol; x++) {
                int ncount = 0;
                int k, l;
                if (grid_type == TRI_GRID && (((x + y) & 1) != 0)) {
                    l = -nsize;
                    k = brow;
                    l += l;
                } else {
                    k = 0;
                    l = 0;
                }
                unsigned char* cp1 = cellptr;
                for (int j = -range; j <= range; j++, cp1 += outerwd) {
                    for (int i = -range; i <= range; i++) {
                        if (cp1[x + i] == 1) ncount += weights[k];
                        k++;
                    }
                    k += l;
                }
    
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    } else {
        // weighted states and neighborhood
        int deadweight = stateweights[0];
        int state = 0;

        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * outerwd;
            unsigned char* cellptr = currgrid + (y - range) * outerwd;
            
            for (int x = mincol; x <= maxcol; x++) {
                int ncount = 0;
                int k, l;
                if (grid_type == TRI_GRID && (((x + y) & 1) != 0)) {
                    l = -nsize;
                    k = brow;
                    l += l;
                } else {
                    k = 0;
                    l = 0;
                }
                unsigned char* cp1 = cellptr;
                for (int j = -range; j <= range; j++, cp1 += outerwd) {
                    for (int i = -range; i <= range; i++) {
                        state = cp1[x + i];
                        if (state > 0) {
                            ncount += weights[k] * stateweights[state];
                        } else {
                            if (deadweight > 0) {
                                ncount += weights[k] * deadweight;
                            }
                        }
                        k++;
                    }
                    k += l;
                }
    
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Custom(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + y * outerwd;
        
        for (int x = mincol; x <= maxcol; x++) {
            int ncount = 0;
            int j = 0;
            while (j < customlength) {
                // get the row number
                int i = customneighborhood[j++];
                if (grid_type == TRI_GRID && (((x + y) & 1) != 0)) {
                    i = -i;
                }
                unsigned char* cp1 = cellptr + i * outerwd;

                // get the count of items in the row
                int k = customneighborhood[j++];
                for (int l = j; l < j + k; l++) {
                    if (cp1[x + customneighborhood[l]] == 1) ncount++;
                }
                j += k;
            }

            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Hash(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + y * outerwd;
        
        // for the first cell count the entire neighborhood
        int rowcount1 = 0;
        int rowcount2 = 0;
        int x = mincol;
        int ncount = 0;
        unsigned char* cp1 = cellptr - outerwd;
        unsigned char *cp2 = cellptr + outerwd;
        // row -1 and + 1
        for (int i = -range; i <= range; i++) {
            if (cp1[x + i] == 1) rowcount1++;
            if (cp2[x + i] == 1) rowcount2++;
        }

        // row 0
        if (cellptr[x - 1] == 1) ncount++;
        if (cellptr[x] == 1)     ncount++;
        if (cellptr[x + 1] == 1) ncount++;

        // remaining rows
        cp1 -= outerwd;
        cp2 += outerwd;
        for (int j = 2; j <= range; j++, cp1 -= outerwd, cp2 += outerwd) {
            if (cp1[x - 1] == 1) ncount++;
            if (cp1[x + 1] == 1) ncount++;
            if (cp2[x - 1] == 1) ncount++;
            if (cp2[x + 1] == 1) ncount++;
        }
        ncount += rowcount1 + rowcount2;
        update_next_grid(x, y, yoffset+x, ncount);

        // for remaining columns subtract the left and add the right cells
        for (int x = mincol + 1; x <= maxcol; x++) {
            int ncount = 0;
            cp1 = cellptr - outerwd;
            cp2 = cellptr + outerwd;
            // row -1 and + 1
            if (cp1[x - range - 1] == 1) rowcount1--;
            if (cp1[x + range] == 1)     rowcount1++;
            if (cp2[x - range - 1] == 1) rowcount2--;
            if (cp2[x + range] == 1)     rowcount2++;

            // row 0
            if (cellptr[x - 1] == 1) ncount++;
            if (cellptr[x] == 1)     ncount++;
            if (cellptr[x + 1] == 1) ncount++;

            // remaining rows
            cp1 -= outerwd;
            cp2 += outerwd;
            for (int j = 2; j <= range; j++, cp1 -= outerwd, cp2 += outerwd) {
                if (cp1[x - 1] == 1) ncount++;
                if (cp1[x + 1] == 1) ncount++;
                if (cp2[x - 1] == 1) ncount++;
                if (cp2[x + 1] == 1) ncount++;
            }
            ncount += rowcount1 + rowcount2;

            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_CheckerBoth(int mincol, int minrow, int maxcol, int maxrow, int start)
{
    int topoffset = range * outerwd;
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + y * outerwd;
        
        // for the first two cells in the row count the entire neighbourhood
        int x = mincol;
        int ncount = 0;
        int ncount2 = 0;
        int offset = start;
        unsigned char* cp1 = cellptr - topoffset;
        for (int j = -range; j <= range; j++, cp1 += outerwd) {
            for (int i = -range + offset; i <= range - offset; i += 2) {
                if (cp1[x + i] == 1) ncount++;
            }
            offset = 1 - offset;
        }

	  // check for survival
	  if (start == 1) {
            if (cellptr[x] == 1) ncount++;
	  }

        update_next_grid(x, y, yoffset+x, ncount);
        x++;

        // check if there are two cells in the row
        if (x <= maxcol) {
            ncount2 = 0;
            offset = start;
            unsigned char* cp1 = cellptr - topoffset;
            for (int j = -range; j <= range; j++, cp1 += outerwd) {
                for (int i = -range + offset; i <= range - offset; i += 2) {
                    if (cp1[x + i] == 1) ncount2++;
                }
                offset = 1 - offset;
            }

		// check for survival
		if (start == 1) {
                if (cellptr[x] == 1) ncount2++;
		}

            update_next_grid(x, y, yoffset+x, ncount2);
            x++;

            // for the remaining cell pairs on the row subtract the left and add the right cells
            while (x <= maxcol) {
                offset = start;
                unsigned char* cp1 = cellptr - topoffset;
                for (int j = -range; j <= range; j++, cp1 += outerwd) {
                    if (cp1[x - range + offset - 2] == 1) ncount--;
                    if (cp1[x + range - offset] == 1) ncount++;
                    offset = 1 - offset;
                }

                // check for survival
		    if (start == 1) {
                    if (cellptr[x - 2] == 1) ncount--;
                    if (cellptr[x] == 1) ncount++;
		    }
                update_next_grid(x, y, yoffset+x, ncount);
                x += 1;

                if (x <= maxcol) {
                    offset = start;
                    unsigned char* cp1 = cellptr - topoffset;
                    for (int j = -range; j <= range; j++, cp1 += outerwd) {
                        if (cp1[x - range + offset - 2] == 1) ncount2--;
                        if (cp1[x + range - offset] == 1) ncount2++;
                        offset = 1 - offset;
                    }

                    // check for survival
			  if (start == 1) {
                        if (cellptr[x - 2] == 1) ncount2--;
                        if (cellptr[x] == 1) ncount2++;
			  }
                    update_next_grid(x, y, yoffset+x, ncount2);
                    x += 1;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Aligned(int mincol, int minrow, int maxcol, int maxrow)
{
	fast_CheckerBoth(mincol, minrow, maxcol, maxrow, 0);
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Checker(int mincol, int minrow, int maxcol, int maxrow)
{
	fast_CheckerBoth(mincol, minrow, maxcol, maxrow, 1);
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Hex(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + (y - range) * outerwd;
        
        // for the first cell count the entire neighborhood
        int ncount = 0;
        unsigned char* cp1 = cellptr;
        int x = mincol;
        for (int j = -range; j < 0; j++, cp1 += outerwd) {
            for (int i = -range; i <= range + j; i++) {
                if (cp1[x + i] == 1) ncount++;
            }
        }
        for (int j = 0; j <= range; j++, cp1 += outerwd) {
            for (int i = -range + j; i <= range; i++) {
                if (cp1[x + i] == 1) ncount++;
            }
        }
        update_next_grid(x, y, yoffset+x, ncount);

        // for remaining columns subtract the left and add the right cells
        for (int x = mincol + 1; x <= maxcol; x++) {
            cp1 = cellptr;
            for (int j = -range; j < 0; j++, cp1 += outerwd) {
                if (cp1[x - range - 1] == 1) ncount--;
                if (cp1[x + range + j] == 1) ncount++;
            }
            for (int j = 0; j <= range; j++, cp1 += outerwd) {
                if (cp1[x - range + j - 1] == 1) ncount--;
                if (cp1[x + range] == 1)         ncount++;
            }
            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Saltire(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + y * outerwd;
        
        for (int x = mincol; x <= maxcol; x++) {
            int ncount = 0;
            if (cellptr[x] == 1) ncount++;
            unsigned char* cp1 = cellptr - outerwd;
            unsigned char* cp2 = cellptr + outerwd;
            for (int j = 1; j <= range; j++, cp1 -= outerwd, cp2 += outerwd) {
                if (cp1[x - j] == 1) ncount++;
                if (cp1[x + j] == 1) ncount++;
                if (cp2[x - j] == 1) ncount++;
                if (cp2[x + j] == 1) ncount++;
            }
            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Star(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + y * outerwd;
        
        // for the first cell count the entire neighborhood
        int rowcount = 0;
        int ncount = 0;
        unsigned char* cp1 = cellptr - outerwd;
        unsigned char* cp2 = cellptr + outerwd;
        int x = mincol;
        for (int j = 1; j <= range; j++, cp1 -= outerwd, cp2 += outerwd) {
            if (cp1[x - j] == 1) ncount++;
            if (cp1[x] == 1)     ncount++;
            if (cp1[x + j] == 1) ncount++;
            if (cp2[x - j] == 1) ncount++;
            if (cp2[x] == 1)     ncount++;
            if (cp2[x + j] == 1) ncount++;
        }
        for (int j = -range; j <= range; j++) {
            if (cellptr[x + j] == 1) rowcount++;
        }
        ncount += rowcount;
        update_next_grid(x, y, yoffset+x, ncount);

        // for remaining columns subtract the left and add the right cells
        for (int x = mincol + 1; x <= maxcol; x++) {
            int ncount = 0;
            cp1 = cellptr - outerwd;
            cp2 = cellptr + outerwd;
            for (int j = 1; j <= range; j++, cp1 -= outerwd, cp2 += outerwd) {
                if (cp1[x - j] == 1) ncount++;
                if (cp1[x] == 1)     ncount++;
                if (cp1[x + j] == 1) ncount++;
                if (cp2[x - j] == 1) ncount++;
                if (cp2[x] == 1)     ncount++;
                if (cp2[x + j] == 1) ncount++;
            }
            if (cellptr[x - range - 1] == 1) rowcount--;
            if (cellptr[x + range] == 1)     rowcount++;
            ncount += rowcount;

            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Cross(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + y * outerwd;
        
        // for the first cell count the entire neighborhood
        int rowcount = 0;
        int ncount = 0;
        unsigned char* cp1 = cellptr - outerwd;
        unsigned char* cp2 = cellptr + outerwd;
        int x = mincol;
        for (int j = 1; j <= range; j++, cp1 -= outerwd, cp2 += outerwd) {
            if (cp1[x] == 1) ncount++;
            if (cp2[x] == 1) ncount++;
        }
        for (int j = -range; j <= range; j++) {
            if (cellptr[x + j] == 1) rowcount++;
        }
        ncount += rowcount;
        update_next_grid(x, y, yoffset+x, ncount);

        // for remaining columns subtract the left and add the right cells
        for (int x = mincol + 1; x <= maxcol; x++) {
            int ncount = 0;
            cp1 = cellptr - outerwd;
            cp2 = cellptr + outerwd;
            for (int j = 1; j <= range; j++, cp1 -= outerwd, cp2 += outerwd) {
                if (cp1[x] == 1) ncount++;
                if (cp2[x] == 1) ncount++;
            }
            if (cellptr[x - range - 1] == 1) rowcount--;
            if (cellptr[x + range] == 1)     rowcount++;
            ncount += rowcount;

            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Triangular(int mincol, int minrow, int maxcol, int maxrow)
{
    // vertical range is half range
    int halfr = range >> 1;

    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + (y - halfr) * outerwd;
        
        // for the first cell compute the whole neighbourhood
        int x = mincol;
        int ncount = 0;
        int k = (x + y) & 1;
        if (k == 0) {
            int width = halfr + 1;
            unsigned char* cp1 = cellptr;
            for (int j = -halfr; j < 0; j++, cp1 += outerwd) {
                for (int i = -width; i <= width; i++) {
                    if (cp1[x + i] == 1) ncount++;
                }
                width++;
            }
            for (int j = 0; j <= halfr; j++, cp1 += outerwd) {
                width--;
                for (int i = -width; i <= width; i++) {
                    if (cp1[x + i] == 1) ncount++;
                }
            }
        } else {
            int width = halfr;
            unsigned char* cp1 = cellptr;
            for (int j = -halfr; j <= 0; j++, cp1 += outerwd) {
                for (int i = -width; i <= width; i++) {
                    if (cp1[x + i] == 1) ncount++;
                }
                width++;
            }
            for (int j = 1; j <= halfr; j++, cp1 += outerwd) {
                width--;
                for (int i = -width; i <= width; i++) {
                    if (cp1[x + i] == 1) ncount++;
                }
            }
        }
        update_next_grid(x, y, yoffset+x, ncount);
        x++;

        // for the remaining cells compute the edge differences
        while (x <= maxcol) {
            int k = (x + y) & 1;
            if (k == 0) {
                int l = halfr;
                unsigned char* cp1 = cellptr;
                for (int j = -halfr; j < 0; j++, cp1 += outerwd) {
                    if (cp1[x + l] == 1) ncount++;
                    if (cp1[x + l + 1] == 1) ncount++;
                    l++;
                }

                // middle row
                if (cp1[x - l - 1] == 1) ncount--;
                if (cp1[x + l] == 1) ncount++;
                l++;
                cp1 += outerwd;

                for (int j = 1; j <= halfr; j++, cp1 += outerwd) {
                    l--;
                    if (cp1[x - l - 1] == 1) ncount--;
                    if (cp1[x - l] == 1) ncount--;
                }
            } else {
                int l = halfr + 1;
                unsigned char* cp1 = cellptr;
                for (int j = -halfr; j < 0; j++, cp1 += outerwd) {
                    if (cp1[x - l - 1] == 1) ncount--;
                    if (cp1[x - l] == 1) ncount--;
                    l++;
                }

                // middle row
                l--;
                if (cp1[x - l - 1] == 1) ncount--;
                if (cp1[x + l] == 1) ncount++;
                cp1 += outerwd;

                for (int j = 1; j <= halfr; j++, cp1 += outerwd) {
                    l--;
                    if (cp1[x + l] == 1) ncount++;
                    if (cp1[x + l + 1] == 1) ncount++;
                }
            }
            update_next_grid(x, y, yoffset+x, ncount);
            x++;
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Gaussian(int mincol, int minrow, int maxcol, int maxrow)
{
    int topoffset = range * outerwd;
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * outerwd;
        unsigned char* cellptr = currgrid + y * outerwd;
        
        for (int x = mincol; x <= maxcol; x++) {
            int ncount = 0;
            int inc = 0;
            int weight = 0;
            unsigned char* cp1 = cellptr - topoffset;
            unsigned char* cp2 = cellptr + topoffset;
            for (int j = -range; j < 0; j++, cp1 += outerwd, cp2 -= outerwd) {
                inc = j + range + 1;
                weight = inc;
                for (int i = -range; i <= 0; i++) {
                    if (cp1[x + i] == 1) ncount += weight;
                    weight += inc;
                }
                weight -= inc + inc;
                for (int i = 1; i <= range; i++) {
                    if (cp1[x + i] == 1) ncount += weight;
                    weight -= inc;
                }
                inc = j + range + 1;
                weight = inc;
                for (int i = -range; i <= 0; i++) {
                    if (cp2[x + i] == 1) ncount += weight;
                    weight += inc;
                }
                weight -= inc + inc;
                for (int i = 1; i <= range; i++) {
                    if (cp2[x + i] == 1) ncount += weight;
                    weight -= inc;
                }
            }
            inc = range + 1;
            weight = inc;
            for (int i = -range; i <= 0; i++) {
                if (cellptr[x + i] == 1) ncount += weight;
                weight += inc;
            }
            weight -= inc + inc;
            for (int i = 1; i <= range; i++) {
                if (cellptr[x + i] == 1) ncount += weight;
                weight -= inc;
            }
            if (cellptr[x]) ncount++;

            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Neumann(int mincol, int minrow, int maxcol, int maxrow)
{
    if (range == 1) {
        int outerwd2 = outerwd * 2;
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * outerwd;
            unsigned char* topy = currgrid + yoffset;
            for (int x = mincol; x <= maxcol; x++) {
                // count the state-1 neighbors within the current range
                // using the extended von Neumann neighborhood with no edge checks
                // (at range 1 a diamond is a cross: +)
                int ncount = 0;
                unsigned char* cellptr = topy + (x - 1);
                if (*cellptr++ == 1) ncount++;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr   == 1) ncount++;
                cellptr -= outerwd;
                if (*--cellptr == 1) ncount++;
                cellptr += outerwd2;
                if (*cellptr   == 1) ncount++;
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    } else {
        // range > 1
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * outerwd;
            int ymrange = y - range;
            int yprange = y + range;
            unsigned char* topy = currgrid + ymrange * outerwd;
            for (int x = mincol; x <= maxcol; x++) {
                // count the state-1 neighbors within the current range
                // using the extended von Neumann neighborhood (diamond) with no edge checks
                int ncount = 0;
                int xoffset = 0;
                unsigned char* rowptr = topy;
                for (int j = ymrange; j < y; j++) {
                    unsigned char* cellptr = rowptr + (x - xoffset);
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        if (*cellptr++ == 1) ncount++;
                    }
                    xoffset++;          // 0, 1, 2, ..., range
                    rowptr += outerwd;
                }
                // xoffset == range
                for (int j = y; j <= yprange; j++) {
                    unsigned char* cellptr = rowptr + (x - xoffset);
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        if (*cellptr++ == 1) ncount++;
                    }
                    xoffset--;          // range-1, ..., 2, 1, 0
                    rowptr += outerwd;
                }
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::do_gen(int mincol, int minrow, int maxcol, int maxrow)
{
    // check for B0 emulation
    unsigned char* saveb = births;
    unsigned char* saves = survivals;

    if (b0 && getGeneration().odd()) {
        births = altbirths;
        survivals = altsurvivals;
    }

    switch (ntype) {
        case 'M':
            if (unbounded) {
                if (colcounts) {
                    if (maxCellStates == 2) {
                        faster_Moore_unbounded2(mincol, minrow, maxcol, maxrow);
                    } else {
                        faster_Moore_unbounded(mincol, minrow, maxcol, maxrow);
                    }
                } else {
                    fast_Moore(mincol, minrow, maxcol, maxrow);
                }
            } else {
                if (colcounts) {
                    if (maxCellStates == 2) {
                        faster_Moore_bounded2(mincol, minrow, maxcol, maxrow);
                    } else {
                        faster_Moore_bounded(mincol, minrow, maxcol, maxrow);
                    }
                } else {
                    fast_Moore(mincol, minrow, maxcol, maxrow);
                }
            }
            break;

        case 'N':
            if (unbounded) {
                if (colcounts) {
                    faster_Neumann_unbounded(mincol, minrow, maxcol, maxrow);
                } else {
                    fast_Neumann(mincol, minrow, maxcol, maxrow);
                }
            } else {
                if (colcounts) {
                    faster_Neumann_bounded(mincol, minrow, maxcol, maxrow);
                } else {
                    fast_Neumann(mincol, minrow, maxcol, maxrow);
                }
            }
            break;

        case 'C':
        case '2':
            fast_Shaped(mincol, minrow, maxcol, maxrow);
            break;

        case 'A':
            fast_Asterisk(mincol, minrow, maxcol, maxrow);
            break;

        case '3':
            fast_Tripod(mincol, minrow, maxcol, maxrow);
            break;

        case 'W':
            fast_Weighted(mincol, minrow, maxcol, maxrow);
            break;

        case '@':
            fast_Custom(mincol, minrow, maxcol, maxrow);
            break;

        case '#':
            fast_Hash(mincol, minrow, maxcol, maxrow);
            break;

        case 'B':
            fast_Checker(mincol, minrow, maxcol, maxrow);
            break;

        case 'D':
            fast_Aligned(mincol, minrow, maxcol, maxrow);
            break;

        case 'H':
            fast_Hex(mincol, minrow, maxcol, maxrow);
            break;

        case 'X':
            fast_Saltire(mincol, minrow, maxcol, maxrow);
            break;

        case '*':
            fast_Star(mincol, minrow, maxcol, maxrow);
            break;

        case '+':
            fast_Cross(mincol, minrow, maxcol, maxrow);
            break;

        case 'L':
            fast_Triangular(mincol, minrow, maxcol, maxrow);
            break;

        case 'G':
            fast_Gaussian(mincol, minrow, maxcol, maxrow);
            break;

        default:
            lifefatal("unknown neighborhood in do_gen");
            break;
    }

    // reset births and survivals
    births = saveb;
    survivals = saves;
}

// -----------------------------------------------------------------------------

void ltlalgo::do_bounded_gen()
{
    // limit processing to rectangle where births/deaths can occur
    int mincol, minrow, maxcol, maxrow;
    bool torus = topology == 'T';
    mincol = minx - range;
    minrow = miny - range;
    maxcol = maxx + range;
    maxrow = maxy + range;

    // check if the limits are outside the grid edges
    if (mincol < 0) {
        mincol = 0;
        if (torus) maxcol = gwdm1;
    }
    if (maxcol > gwdm1) {
        maxcol = gwdm1;
        if (torus) mincol = 0;
    }
    if (minrow < 0) {
        minrow = 0;
        if (torus) maxrow = ghtm1;
    }
    if (maxrow > ghtm1) {
        maxrow = ghtm1;
        if (torus) minrow = 0;
    }

    // save pattern limits for clearing border cells at end
    int sminx = minx;
    int smaxx = maxx;
    int sminy = miny;
    int smaxy = maxy;

    if (torus) {
        // If a pattern edge is within range of a grid edge then copy cells
        // into appropriate border cells next to the opposite grid edge,
        // as illustrated in this example of a grid with range 1 (so border = 2).
        // The live cells at "a" will be copied to the "A" locations and the
        // live cell at corner "b" will be copied to the three "B" locations:
        //
        //   <-------------- outerwd -------------->
        //   o--------------------------------------  ^  o = outergrid1
        //   |                                     |  |
        //   |  <------------- gwd ------------->  |  |
        //   |  c--------------------------------  |  |  c = currgrid
        //   | B|       aa      ^              b|  |  |
        //   |  |               |               |  |  |
        //   |  |              ght              |  |outerht
        //   |  |               |               |  |  |
        //   |  |               v               |  |  |
        //   |  ---------------------------------  |  |
        //   | B        AA                     B   |  |
        //   |                                     |  |
        //   ---------------------------------------  v
        //
        if (miny < range) {
            // copy cells near top edge of currgrid to bottom border
            int numrows = range - miny;
            int numcols = maxx - minx + 1;
            unsigned char* src = currgrid + miny * outerwd + minx;
            unsigned char* dest = src + ght * outerwd;
            for (int row = 0; row < numrows; row++) {
                memcpy(dest, src, numcols);
                src += outerwd;
                dest += outerwd;
            }
            if (minx < range) {
                // copy cells near top left corner of currgrid to bottom right border
                numcols = range - minx;
                src = currgrid + miny * outerwd + minx;
                dest = src + ght * outerwd + gwd;
                for (int row = 0; row < numrows; row++) {
                    memcpy(dest, src, numcols);
                    src += outerwd;
                    dest += outerwd;
                }
            }
        }
        if (maxy + range > ghtm1) {
            // copy cells near bottom edge of currgrid to top border
            int numrows = maxy + range - ghtm1;
            int numcols = maxx - minx + 1;
            unsigned char* src = currgrid + (ght - range) * outerwd + minx;
            unsigned char* dest = src - ght * outerwd;
            for (int row = 0; row < numrows; row++) {
                memcpy(dest, src, numcols);
                src += outerwd;
                dest += outerwd;
            }
            if (maxx + range > gwdm1) {
                // copy cells near bottom right corner of currgrid to top left border
                numcols = maxx + range - gwdm1;
                src = currgrid + (ght - range) * outerwd + gwd - range;
                dest = src - ght * outerwd - gwd;
                for (int row = 0; row < numrows; row++) {
                    memcpy(dest, src, numcols);
                    src += outerwd;
                    dest += outerwd;
                }
            }
        }
        if (minx < range) {
            // copy cells near left edge of currgrid to right border
            int numrows = maxy - miny + 1;
            int numcols = range - minx;
            unsigned char* src = currgrid + miny * outerwd + minx;
            unsigned char* dest = src + gwd;
            for (int row = 0; row < numrows; row++) {
                memcpy(dest, src, numcols);
                src += outerwd;
                dest += outerwd;
            }
            if (maxy + range > ghtm1) {
                // copy cells near bottom left corner of currgrid to top right border
                numrows = maxy + range - ghtm1;
                src = currgrid + (ght - range) * outerwd + minx;
                dest = src - ght * outerwd + gwd;
                for (int row = 0; row < numrows; row++) {
                    memcpy(dest, src, numcols);
                    src += outerwd;
                    dest += outerwd;
                }
            }
        }
        if (maxx + range > gwdm1) {
            // copy cells near right edge of currgrid to left border
            int numrows = maxy - miny + 1;
            int numcols = maxx + range - gwdm1;
            unsigned char* src = currgrid + miny * outerwd + gwd - range;
            unsigned char* dest = src - gwd;
            for (int row = 0; row < numrows; row++) {
                memcpy(dest, src, numcols);
                src += outerwd;
                dest += outerwd;
            }
            if (miny < range) {
                // copy cells near top right corner of currgrid to bottom left border
                numrows = range - miny;
                src = currgrid + miny * outerwd + gwd - range;
                dest = src + ght * outerwd - gwd;
                for (int row = 0; row < numrows; row++) {
                    memcpy(dest, src, numcols);
                    src += outerwd;
                    dest += outerwd;
                }
            }
        }
    }
    
    // reset minx,miny,maxx,maxy for first birth or survivor in nextgrid
    empty_boundaries();
    
    // compute next generation based on neighborhood type
    do_gen(mincol, minrow, maxcol, maxrow);

    // if using one grid with a torus then clear border cells copied above
    if (colcounts && torus) {
        if (sminy < range) {
            // clear cells in bottom border
            int numrows = range - sminy;
            int numcols = smaxx - sminx + 1;
            unsigned char* src = currgrid + sminy * outerwd + sminx;
            unsigned char* dest = src + ght * outerwd;
            for (int row = 0; row < numrows; row++) {
                memset(dest, 0, numcols);
                dest += outerwd;
            }
            if (sminx < range) {
                // clear cells in bottom right border
                numcols = range - sminx;
                src = currgrid + sminy * outerwd + sminx;
                dest = src + ght * outerwd + gwd;
                for (int row = 0; row < numrows; row++) {
                    memset(dest, 0, numcols);
                    dest += outerwd;
                }
            }
        }
        if (smaxy + range > ghtm1) {
            // clear cells in top border
            int numrows = smaxy + range - ghtm1;
            int numcols = smaxx - sminx + 1;
            unsigned char* src = currgrid + (ght - range) * outerwd + sminx;
            unsigned char* dest = src - ght * outerwd;
            for (int row = 0; row < numrows; row++) {
                memset(dest, 0, numcols);
                dest += outerwd;
            }
            if (smaxx + range > gwdm1) {
                // clear cells in top left border
                numcols = smaxx + range - gwdm1;
                src = currgrid + (ght - range) * outerwd + gwd - range;
                dest = src - ght * outerwd - gwd;
                for (int row = 0; row < numrows; row++) {
                    memset(dest, 0, numcols);
                    dest += outerwd;
                }
            }
        }
        if (sminx < range) {
            // clear cells in right border
            int numrows = smaxy - sminy + 1;
            int numcols = range - sminx;
            unsigned char* src = currgrid + sminy * outerwd + sminx;
            unsigned char* dest = src + gwd;
            for (int row = 0; row < numrows; row++) {
                memset(dest, 0, numcols);
                dest += outerwd;
            }
            if (smaxy + range > ghtm1) {
                // clear cells in top right border
                numrows = smaxy + range - ghtm1;
                src = currgrid + (ght - range) * outerwd + sminx;
                dest = src - ght * outerwd + gwd;
                for (int row = 0; row < numrows; row++) {
                    memset(dest, 0, numcols);
                    dest += outerwd;
                }
            }
        }
        if (smaxx + range > gwdm1) {
            // clear cells in left border
            int numrows = smaxy - sminy + 1;
            int numcols = smaxx + range - gwdm1;
            unsigned char* src = currgrid + sminy * outerwd + gwd - range;
            unsigned char* dest = src - gwd;
            for (int row = 0; row < numrows; row++) {
                memset(dest, 0, numcols);
                dest += outerwd;
            }
            if (sminy < range) {
                // clear cells in bottom left border
                numrows = range - sminy;
                src = currgrid + sminy * outerwd + gwd - range;
                dest = src + ght * outerwd - gwd;
                for (int row = 0; row < numrows; row++) {
                    memset(dest, 0, numcols);
                    dest += outerwd;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

bool ltlalgo::do_unbounded_gen()
{
    int mincol = minx - range;
    int minrow = miny - range;
    int maxcol = maxx + range;
    int maxrow = maxy + range;

    if (mincol < range || maxcol > gwdm1-range || minrow < range || maxrow > ghtm1-range) {
        // pattern boundary is too close to a grid edge so expand the universe in that
        // direction, and possibly shrink the universe in the opposite direction        
        int inc = MAXRANGE * 2;
        int up    = minrow < range       ? inc : 0;
        int down  = maxrow > ghtm1-range ? inc : 0;
        int left  = mincol < range       ? inc : 0;
        int right = maxcol > gwdm1-range ? inc : 0;
        
        // check for possible shrinkage (pattern might be a spaceship)
        if (up > 0    && down == 0  && maxrow < ghtm1-range) down  = -(ghtm1-maxrow-range);
        if (down > 0  && up == 0    && minrow > range)       up    = -(minrow-range);
        if (left > 0  && right == 0 && maxcol < gwdm1-range) right = -(gwdm1-maxcol-range);
        if (right > 0 && left == 0  && mincol > range)       left  = -(mincol-range);
        
        const char* errmsg = resize_grids(up, down, left, right);
        if (errmsg) {
            lifewarning(errmsg);    // no need to check show_warning here
            return false;           // stop generating
        }
        
        mincol = minx - range;
        minrow = miny - range;
        maxcol = maxx + range;
        maxrow = maxy + range;
    }
        
    // reset minx,miny,maxx,maxy for first birth or survivor in nextgrid
    empty_boundaries();

    // compute next generation based on neighborhood type
    do_gen(mincol, minrow, maxcol, maxrow);

    return true;
}

// -----------------------------------------------------------------------------

// Do increment generations.

void ltlalgo::step()
{
    bigint t = increment;
    while (t != 0) {
        // check if anything is alive
        // note: b0 is emulated so zero population does not come alive
        if (population > 0) {
            int prevpop = population;
            
            // calculate the next generation in nextgrid
            if (unbounded) {
                if (!do_unbounded_gen()) {
                    // failed to resize universe so stop generating
                    poller->setInterrupted();
                    return;
                }
            } else {
                do_bounded_gen();
            }
        
            // swap outergrid1 and outergrid2 if using fast_* algo
            if (outergrid2) {
                unsigned char* temp = outergrid1;
                outergrid1 = outergrid2;
                outergrid2 = temp;
        
                // swap currgrid and nextgrid
                temp = currgrid;
                currgrid = nextgrid;
                nextgrid = temp;
            
                // kill all cells in outergrid2
                if (prevpop > 0) memset(outergrid2, 0, outerbytes);
            }
        }
    
        generation += bigint::one;
        
        // this is a safe place to check for user events
        if (poller->inner_poll()) return;
        
        t -= 1;
        // user might have changed increment 
        if (t > increment) t = increment;
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::save_cells()
{
    for (int y = miny; y <= maxy; y++) {
        int yoffset = y * outerwd;
        for (int x = minx; x <= maxx; x++) {
            unsigned char* currcell = currgrid + yoffset + x;
            if (*currcell) {
                cell_list.push_back(x + gleft);
                cell_list.push_back(y + gtop);
                cell_list.push_back(*currcell);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::restore_cells()
{
    clipped_cells.clear();
    for (size_t i = 0; i < cell_list.size(); i += 3) {
        int x = cell_list[i];
        int y = cell_list[i+1];
        int s = cell_list[i+2];
        // check if x,y is outside grid
        if (x < gleft || x > gright || y < gtop || y > gbottom) {
            // store clipped cells so that GUI code (eg. ClearOutsideGrid)
            // can remember them in case this rule change is undone
            clipped_cells.push_back(x);
            clipped_cells.push_back(y);
            clipped_cells.push_back(s);
        } else {
            setcell(x, y, s);
        }
    }
    cell_list.clear();
}

// -----------------------------------------------------------------------------

// Compute maximum number of neighbors for outer totalistic neighborhood and range.

int ltlalgo::max_neighbors(const int range, const char neighborhood, const int customcount, int* tshape) {
    int count = 0;
    int width = 0;
    int r2 = 0;
    int result = 0;

    switch(neighborhood) {
        case 'M':
            // Moore
            result = (range * 2 + 1) * (range * 2 + 1) - 1;
            break;

        case 'N':
            // von Neumann
            result = 2 * range * (range + 1);
            break;

        case 'C':
            // circular
            count = 0;
            r2 = range * range + range;
            for (int i = -range; i <= range; i++) {
                width = 0;
                while ((width + 1) * (width + 1) + (i * i) <= r2) {
                    width++;
                }
                tshape[i + range] = width;
                count += 2 * width + 1;
            }
            result = count - 1;
            break;

        case '+':
            // cross
            result = range * 4;
            break;

        case 'X':
            // saltire
            result = range * 4;
            break;

        case '*':
            // star
            result = range * 8;
            break;

        case '2':
            // L2
            count = 0;
            r2 = range * range;
            for (int i = -range; i <= range; i++) {
                width = 0;
                while ((width + 1) * (width + 1) + (i * i) <= r2) {
                    width++;
                }
                tshape[i + range] = width;
                count += 2 * width + 1;
            }
            result = count - 1;
            break;

        case 'H':
            // hexagonal
            result = (range * 2 + 1) * (range * 2 + 1) - (range * (range + 1)) - 1;
            break;

        case 'B':
            // checkerboard
            result = ((range * 2 + 1) * (range * 2 + 1) - 1) / 2;
            break;

        case 'D':
            // aligned checkerboard
            result = ((range * 2 + 1) * (range * 2 + 1) - 1) / 2;
            break;

        case '#':
            // hash
            result = range * 8;
            break;

        case '@':
            // custom
            result = customcount;
            break;

        case '3':
            // tripod
            result = range * 3;
            break;

        case 'A':
            // asterisk
            result = range * 6;
            break;

        case 'L':
            // triangular
            result = (range * 4 + 1) * (range * 2 + 1) - (range * 2 * range) - 1;
            break;

        case 'G':
            // gaussian
            result = (range + 1) * (range + 1) * (range + 1) * (range + 1);
            break;

        case 'W':
            // weighted
            result = customcount;
            break;
    }

    return result;
}

// -----------------------------------------------------------------------------

// Read weighted neighborhood.
// returns error message if invalid or NULL if valid
// updates c to contain neighborhood count
// updates gt if grid type is hexagonal or triangular
// updates nbrend to point to the next character in the rule string

const char* ltlalgo::read_weighted(const char *n, int r, int states, int &c, TGridType &gt, const char *&nbrend) {
    int i = 0;
    int j = 0;
    int l = (int) strlen(n);
    int needed1 = (r * 2 + 1) * (r * 2 + 1);
    int needed2 = needed1 + needed1;
    int maxsw = 0;
    bool valid = true;
    int* weightslist = NULL;
    int* stateweightslist = NULL;
    ptrdiff_t next = 0;

    // check string is at least long enough for single digit hex characters
    if (l < needed1) return "weighted neighborhood is too short";

    // read up to double digit hex characters
    valid = true;
    while (i < needed2 && valid) {
        if (n[i] && (strchr(HEXCHARACTERS, n[i]) != NULL)) {
            i++;
        } else {
            valid = false;
        }
    }

    // check for length 1
    if (i == needed1) {
        // allocate weights array
        weightslist = (int*) malloc(needed1 * sizeof(int));
        if (weightslist == NULL) lifefatal("Not enough memory for weights!");
        i = 0;
        while (i < needed1) {
            // the character is guaranteed to exist since we validated above
            next = strchr(HEXCHARACTERS, n[i]) - HEXCHARACTERS;
            // check for negative
            if ((next & 8) != 0) {
                next = -(next & 7);
            }
            weightslist[i] = (int) next;
            i++;
        }
    } else {
        // check for length 2
        if (i == needed2) {
            // allocates weights array (note: needed1 since 2 digits per entry)
            weightslist = (int*) malloc(needed1 * sizeof(int));
            if (weightslist == NULL) lifefatal("Not enough memory for weights!");
            i = 0;
            int j = 0;
            while (i < needed2) {
                // the character is guaranteed to exist since we validated above
                next = (strchr(HEXCHARACTERS, n[i]) - HEXCHARACTERS) << 4;
                i++;
                next |= strchr(HEXCHARACTERS, n[i]) - HEXCHARACTERS;
                if ((next & 128) != 0) {
                    next = -(next & 127);
                }
                i++;
                weightslist[j++] = (int) next;
            }
        } else {
            // invalid number of digits
            return "weighted neighborhood is too short";
        }
    }

    // check for optional state weights
    if (n[i] == ',') {
        i++;

        // check how long hex string is
        j = i;
        while (i < l && (strchr(HEXCHARACTERS, n[i]) != NULL)) {
            i++;
        }

        // ensure states are at least 2
        if (states < 2) states = 2;

        // check number of weights matches nmber of states in rule
        if ((i - j) != states) {
            free(weightslist);
            return "weighted states do not match states in rule";
        } else {
            // read state weights and compute the maximum
            stateweightslist = (int*) malloc(states * sizeof(int));
            if (stateweightslist == NULL) lifefatal("Not enough memory for state weights!");
            i = j;
            while (i < j + states) {
                // the character is guaranteed to exist since we validated above
                next = strchr(HEXCHARACTERS, n[i]) - HEXCHARACTERS;
                stateweightslist[i - j] = (int) next;
                if ((int) next > maxsw) {
                    maxsw = (int) next;
                }
                i++;
            }
        }
    } else {
        // no state weights so set maximum to 1
        maxsw = 1;
    }

    // sum weights ignoring negatives for maximum neighbor count
    c = 0;
    for (j = 0; j < needed1; j++) {
        if (weightslist[j] > 0) {
            c += weightslist[j] * maxsw;
        }
    }

    // remember the weights and state weights
    if (weights) free(weights);
    weights = weightslist;
    if (stateweights) free(stateweights);
    stateweights = stateweightslist;

    // check for hexagonal or triangular grid type postfix
    if (i < l) {
        if (tolower(n[i]) == 'h') {
            gt = HEX_GRID;
            i++;
        } else {
            if (tolower(n[i]) == 'l') {
                gt = TRI_GRID;
                i++;
            }
        }
    }

    // update next character position
    nbrend = n + i;

    // return success
    return NULL;
}

// -----------------------------------------------------------------------------

// Returns a count of the number of bits set in given int.

static int bitcount(int v) {
   int r = 0 ;
   while (v) {
      r++ ;
      v &= v - 1 ;
   }
   return r ;
}

// -----------------------------------------------------------------------------

// Read custom neighborhood.
// returns error message if invalid or NULL if valid
// updates c to contain neighborhood count
// updates gt if grid type is hexagonal or triangular
// updates nbrend to point to the next character in the rule string

const char* ltlalgo::read_custom(const char *n, int r, int &c, TGridType &gt, const char *&nbrend) {
    int i = 0;
    int l = (int) strlen(n);
    int neededlength = ((r * 2 + 1) * (r * 2 + 1) - 1) / 4;
    int count = 0;
    bool valid = true;
    const char* loc = NULL;

    // check string is at long enough
    if (l < neededlength) return "custom neighborhood is too short";

    // read hex characters
    valid = true;
    while (i < neededlength && valid) {
        loc = strchr(HEXCHARACTERS, n[i]);
        if (loc) {
            i++;
            count += bitcount((int) (loc - HEXCHARACTERS));
        } else {
            valid = false;
        }
    }

    // check if enough digits were read
    if (valid) {
        if (i != neededlength) {
            valid = false;
        } else {
            // check for hexagonal or triangular grid type postfix
            if (i < l) {
                if (tolower(n[i]) == 'h') {
                    gt = HEX_GRID;
                    i++;
                } else {
                    if (tolower(n[i]) == 'l') {
                        gt = TRI_GRID;
                        i++;
                    }
                }
            }
            c = count;
            nbrend = n + i;
        }
    }

    if (valid) {
        // neighborhood valid so build list of included cells in custom neighborhood
        const int width = r * 2 + 1;
        const int w2 = width * width;
        char* neighborhood = (char*) calloc(w2, sizeof(char));
        int* rowcount = (int *) calloc(width, sizeof(int));
        int midk = neededlength >> 1;
        int k = 0;
        int j = 0;
        int w = 0;
        int numinrow = 0;
        int item = 0;

        // first build simple 2d neighborhood grid from the hex digits
        i = 0;
        item = 0;
        while (j < width) {
            // get next 4 bits
            w = (int) (strchr(HEXCHARACTERS, n[k]) - HEXCHARACTERS);
            if (k == midk) {
                neighborhood[i] = 1;
                i++;
                numinrow++;
            }
            k++;

            // set neighborhood
            for (int l = 3; l >=0 ; l--) {
                if ((w & (1 << l)) != 0) {
                    neighborhood[i] = 1;
                    numinrow++;
                }
                i++;
                if ((i % width) == 0) {
                    rowcount[item] = numinrow;
                    item += 1;
                    numinrow = 0;
                    j++;
                }
            }
        }

        // allocate the neighbor cache (including middle cell)
        int* neighborcache = (int*) calloc(count + 1, sizeof(int));

        // populate the cache from the neighborhood grid
        count = 0;
        k = 0;
        for (j = 0; j < width; j++) {
            for (int i = 0; i < width; i++) {
                if (neighborhood[k++] > 0) {
                    neighborcache[count++] = (j << 16) | i;
                }
            }
        }

        // allocate the neighbor list
        int total = 0;
        for (int i = 0; i < width; i++) {
            // number of items in the row plus the count plus the row number
            if (rowcount[i] > 0) {
                total += rowcount[i] + 2;
            }
        }
        int* neighborlist = (int*) calloc(total, sizeof(int));

        // populate the list from each row in the cache
        k = 0;
        count = 0;
        for (int i = 0; i < width; i++) {
            // get the row
            if (rowcount[i] > 0) {
                item = neighborcache[k];
                neighborlist[count] = (item >> 16) - (width >> 1);
                neighborlist[count + 1] = rowcount[i];
                count += 2;
                for (j = 0; j < rowcount[i]; j++) {
                    neighborlist[count + j]  = (neighborcache[k] & 65535) - (width >> 1);
                    k++;
                }
                count += rowcount[i];
            }
        }

        // free the temporary buffers
        free(rowcount);
        free(neighborhood);
        free(neighborcache);
        
        // save the neighborhood
        if (customneighborhood) free(customneighborhood);
        customneighborhood = neighborlist;
        customlength = total;

        return NULL;
    } else {
        return "invalid custom neighborhood";
    }
}

// -----------------------------------------------------------------------------

// Convert birth or survival flags to string.
// note: string is allocated and must be freed by caller

char *ltlalgo::flags_string(const unsigned char *flags, int len) {
    int start = -1;
    int i = 0;
    int pass = 0;
    char *buffer = NULL;
    int bufsize = 0;
    char value[20];
    int vallen = 0;

    // do two passes, the first to size the buffer, the second to populate
    while (pass < 2) {
        bufsize = 0;
        i = 0;
        start = -1;

        // read the array looking for ranges
        while (i < len) {
            // check for set value
            if (flags[i] == 1) {
                // if no current run then set as start of run
                if (start == -1) {
                    start = i;
                }
            } else {
                // zero so check if current run being processed
                if (start != -1) {
                    // output current run
                    if (bufsize > 0) {
                        if (pass == 1) buffer[bufsize] = ',';
                        bufsize++;
                    }

                    sprintf(value, "%d%c", start, 0);
                    vallen = (int) strlen(value);
                    if (pass == 1) strcpy(buffer + bufsize, value);
                    bufsize += vallen;

                    if ((i - 1) != start) {
                        if (pass == 1) buffer[bufsize] = '-';
                        bufsize++;
                        sprintf(value, "%d%c", (i - 1), 0);
                        vallen = (int) strlen(value);
                        if (pass == 1) strcpy(buffer + bufsize, value);
                        bufsize += vallen;
                    }
    
                    // reset run
                    start = -1;
                }
            }
            // next item
            i++;
        }
    
        // check if still processing a run
        if (start != -1) {
            if (bufsize > 0) {
                if (pass == 1) buffer[bufsize] = ',';
                bufsize++;
            }
            sprintf(value, "%d%c", start, 0);
            vallen = (int) strlen(value);
            if (pass == 1) strcpy(buffer + bufsize, value);
            bufsize += vallen;

            if ((i - 1) != start) {
                if (pass == 1) buffer[bufsize] = '-';
                bufsize++;
                sprintf(value, "%d%c", (i - 1), 0);
                vallen = (int) strlen(value);
                if (pass == 1) strcpy(buffer + bufsize, value);
                bufsize += vallen;
            }
        }

        // next pass
        if (pass == 0) {
            buffer = (char*) calloc(bufsize + 1, sizeof(char));
            if (buffer == NULL) lifefatal("Not enough memory for flags buffer!");
        }
        pass++;
    }
    
    // return string buffer
    return buffer;
}

// -----------------------------------------------------------------------------

// Setup B0 emulation.

void ltlalgo::setup_b0_emulation(int maxn)
{
    unsigned char* bs = (unsigned char*) calloc(maxn + 1, sizeof(unsigned char));
    if (bs == NULL) lifefatal("Not enough memory for birth flags!");
    unsigned char* ss = (unsigned char*) calloc(maxn + 1, sizeof(unsigned char));
    if (ss == NULL) lifefatal("Not enough memory for survival flags!");

    // check for Smax
    if (survivals[maxn]) {
        // B0 with Smax: rule -> NOT(reverse(bits))
        for (int i = 0; i <= maxn; i++) {
            bs[i] = 1 - survivals[maxn - i];
            ss[i] = 1 - births[maxn - i];
        }
        memcpy(births, bs, maxn + 1);
        memcpy(survivals, ss, maxn + 1);
        free(bs);
        free(ss);

        // clear B0 flag since no alternating required
        b0 = false;
    } else {
        // B0 without Smax needs two rules
        memcpy(bs, births, maxn + 1);
        memcpy(ss, survivals, maxn + 1);

        // odd rule -> reverse(bits)
        for (int i = 0; i <= maxn; i++) {
            bs[i] = survivals[maxn - i];
            ss[i] = births[maxn - i];
        }

        // even rule -> NOT(bits)
        for (int i = 0; i <= maxn; i++) {
            births[i] = 1 - births[i];
            survivals[i] = 1 - survivals[i];
        }

        // save alternate rule
        altbirths = bs;
        altsurvivals = ss;
    }
}

// -----------------------------------------------------------------------------

// Switch to the given rule if it is valid.

const char *ltlalgo::setrule(const char *s)
{
    int r, c, m, s1, s2, b1, b2, endpos, maxn, min, max;
    int mins, maxs, minb, maxb;
    int custom = 0;
    char n;
    char rule[MAXRULESIZE];
    char urule[MAXRULESIZE];
    const char* nbrend = NULL;
    const char* nbrhd = NULL;
    const char* pos = NULL;
    bool valid;
    bool wasLtL = false;
    int l = (int) strlen(s);
    TGridType gridt = SQUARE_GRID;

    // check supplied rule string is not too long
    if (l >= MAXRULESIZE) {
      return "Rule name is too long." ;
    }

    // convert rule to upper case
    for (int i = 0; i <= l; i++) {
        urule[i] = (char) toupper(s[i]);
    }

    // check for LtL syntax
    if ((strstr(urule, "..") != 0) && sscanf(urule, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c%n",
                  &r, &c, &m, &s1, &s2, &b1, &b2, &n, &endpos) == 8) {
        // note middle cell value was read above and is in m

        // convert to HROT format
        sprintf(rule, "R%d,C%d,S%d-%d,B%d-%d,N%c%s", r, c, s1, s2, b1, b2, n, urule + endpos);
        wasLtL = true;
    } else {
        // try alternate LtL syntax as defined by Kellie Evans;
        // eg: 5,34,45,34,58 is equivalent to R5,C0,M1,S34..58,B34..45,NM
        if (sscanf(urule, "%d,%d,%d,%d,%d%n",
                      &r, &b1, &b2, &s1, &s2, &endpos) == 5) {
            // match so set other parameters
            c = 0;
            n = 'M';
            // middle cell is included
            m = 1;

            // convert to HROT format
            sprintf(rule, "R%d,C%d,S%d-%d,B%d-%d,N%c%s", r, c, s1, s2, b1, b2, n, urule + endpos);
            wasLtL = true;
        } else {
            // assume already in HROT format
            memcpy(rule, urule, l + 1);

            // middle cell is not included
            m = 0;
            wasLtL = false;
        }
    }

    // read range and number of states
    if (sscanf(rule, "R%d,C%d%n", &r, &c, &endpos) != 2) return "could not find R";

    // validate range and number of states
    if (r < 1) return "R value is too small";
    if (r > MAXRANGE) return "R value is too big";
    if (c < 0 || c > 256) return "C value must be from 0 to 256";

    // find neighborhood
    nbrhd = strstr(rule, ",N");
    if (nbrhd == NULL && wasLtL) return "neighborhood not found";

    // for non-LtL the neighborhood is optional and defaults to Moore
    if (nbrhd == NULL) {
        n = 'M';
    } else {
        // get the neighborhood type
        nbrhd += 2;
        n = (char) toupper(*nbrhd++);
    
        // validate neighborhood
        if (strchr(VALIDNEIGHBORHOODS, n) == NULL) return "invalid neighborhood specificed";
    
        // check for custom or weighted neighborhoods
        nbrend = nbrhd;
        if (n == '@') {
            const char *error = read_custom(nbrhd, r, custom, gridt, nbrend);
            if (error) return error;
        } else {
            if (n == 'W') {
                const char *error = read_weighted(nbrhd, r, c, custom, gridt, nbrend);
                if (error) return error;
            }
        }
    }
    
    // Gaussian has a much smaller maximum range
    if (n == 'G') {
	if (r > MAXGAUSSIANRANGE) return "R value is too big";
    }

    // compute neighborhood size without middle cell and populate tshape for relevant neighborhoods
    int tshape[2 * MAXRANGE + 1];
    memset(tshape, 0, (2 * MAXRANGE + 1) * sizeof(int));
    maxn = max_neighbors(r, n, custom, tshape);

    // double the range for triangular rules
    if (n == 'L') {
        r = r + r;
        if (r > MAXRANGE) return "R value is too big";
    }

    // set the min and max S and B values for validation
    // for totalistic (i.e. including middle cell) the ranges should be:
    // B: 0 to maxn
    // S: 1 to maxn + 1
    // however several patterns use B: 0 to maxn + 1 so we support that here
    minb = 0;
    maxb = maxn + m;
    mins = m;
    maxs = maxn + m;

    // check for survivals
    pos = rule + endpos;
    if (*pos != ',') {
        return "missing , before S";
    }
    pos++;
    if (*pos != 'S') {
        return "missing S";
    }
    pos++;

    // allocate survival flags
    unsigned char* ss = (unsigned char*) calloc(maxs + 1, sizeof(unsigned char));
    if (ss == NULL) lifefatal("Not enough memory for survival flags!");

    // decode survival values
    valid = true;
    min = -1;

    do {
        // read next survival range (x-y) or single value (x)
        if (sscanf(pos, "%d-%d%n", &min, &max, &endpos) != 2) {
            if (sscanf(pos, "%d%n", &min, &endpos) == 1) {
                max = min;
            } else {
                valid = false;
            }
        }
        if (valid) {
            // validate survival values
            if (min > max) {
                free(ss);
                return "S range min must be less than max";
            }
            if (min < mins || min > maxs || max < mins || max > maxs) {
                free(ss);
                return "S value out of range";
            }

            // populate survival list
            for (int i = min; i <= max; i++) {
                ss[i] = 1;
            }

            // move to next item
            pos += endpos;

            // check for separator
            if (*pos == ',') {
                pos++;
            } else {
                valid = false;
            }
        }
    } while(valid);

    // check if there were any survival counts
    if (min == -1) {
        // no counts so next character must be a comma
        if (*pos != ',') {
            free(ss);
            return "missing , before B";
        } else {
            // skip comma
            pos++;
        }
    } else {
        // survival counts were present and might have ended with comma
        if (*(pos - 1) != ',') {
            free(ss);
            return "missing , before B";
        }
    }

    // check for B
    if (*pos != 'B') {
        free(ss);
        return "missing B";
    }
    pos++;

    // allocate birth flags
    unsigned char* bs = (unsigned char*) calloc(maxb + 1, sizeof(unsigned char));
    if (bs == NULL) lifefatal("Not enough memory for birth flags!");

    valid = true;
    min = -1;

    do {
        // read next birth range (x-y) or single value (x)
        if (sscanf(pos, "%d-%d%n", &min, &max, &endpos) != 2) {
            if (sscanf(pos, "%d%n", &min, &endpos) == 1) {
                max = min;
            } else {
                valid = false;
            }
        }
        if (valid) {
            // validate birth values
            if (min > max) {
                free(bs);
                free(ss);
                return "B range min must be less than max";
            }
            if (min < minb || min > maxb || max < minb || max > maxb) {
                free(bs);
                free(ss);
                return "B value out of range";
            }

            // populate birth list
            for (int i = min; i <= max; i++) {
                bs[i] = 1;
            }

            // move to next item
            pos += endpos;

            // check for separator
            if (*pos == ',') {
                pos++;
            } else {
                valid = false;
            }
        }
    } while(valid);

    // check next section is neighborhood
    if (nbrhd) {
        if (min == -1 && *pos == ',') pos++;
        if (pos != nbrhd - 2) {
            free(bs);
            free(ss);
            return "bad characters before N";
        } else {
            pos = nbrend;
        }
    }

    // check for suffix
    if (*pos != 0 && *pos != ':') {
        free(bs);
        free(ss);
        return "bad suffix";
    }

    // decode suffix
    char t = 'T';
    int newwd = DEFAULTSIZE;
    int newht = DEFAULTSIZE;

    // check for explicit suffix like ":T200,100"
    const char* suffix = NULL;
    if (*pos == ':') suffix = pos;
    if (suffix && suffix[1] != 0) {
        if (suffix[1] == 'T' || suffix[1] == 't') {
            t = 'T';
        } else if (suffix[1] == 'P' || suffix[1] == 'p') {
            t = 'P';
        } else {
            free(bs);
            free(ss);
            return "bad topology in suffix (must be torus or plane)";
        }
        if (suffix[2] == 0) {
            // no dimensions specified
            suffix = NULL;
        } else {
            bool oneval = false;
            if (sscanf(suffix+2, "%d,%d%n", &newwd, &newht, &endpos) != 2) {
                if (sscanf(suffix+2, "%d%n", &newwd, &endpos) != 1) {
                    free(bs);
                    free(ss);
                    return "bad grid size";
                } else {
                    newht = newwd;
                    oneval = true;
                    if (suffix[endpos+2] != 0) {
                        if (suffix[endpos+2] == ',' && suffix[endpos+3] == 0) {
                            // allow dangling comma after width to be consistent with lifealgo::setgridsize
                        } else {
                            free(bs);
                            free(ss);
                            return "unexpected character in suffix";
                        }
                    }
                }
            }
            if (!oneval && suffix[endpos+2] != 0) {
                free(bs);
                free(ss);
                return "unexpected character in suffix";
            }   
        }
        if ((float)newwd * (float)newht > MAXCELLS) {
            free(bs);
            free(ss);
            return "grid size is too big";
        }
    } else {
        // no topology specified
        suffix = NULL;
    }

    // check for B0 and more than 2 states
    b0 = bs[0];
    if (b0 && c > 2) {
        free(bs);
        free(ss);
        return "B0 is not allowed with more than 2 states";
    }

    // the given rule is valid
    int oldrange = range;
    char oldtype = ntype;
    int scount = c;
    range = r;
    ntype = n;
    topology = t;
    if (births) free(births);
    births = bs;
    if (survivals) free(survivals);
    survivals = ss;
    if (shape) free(shape);
    shape = NULL;
    if (n == 'C' || n == '2' || n == 'L') {
        shape = (int*) calloc(2 * r + 1, sizeof(int));
        memcpy(shape, tshape, (2 * r + 1) * sizeof(int));
    }
    if (altbirths) free(altbirths);
    altbirths = NULL;
    if (altsurvivals) free(altsurvivals);
    altsurvivals = NULL;
    
    // set the grid_type so the GUI code can display circles or diamonds in icon mode
    // (no circular grid, so adopt a square grid for now)
    #define CIRC_GRID SQUARE_GRID
    switch(ntype) {
        case 'N':
            // von Neumann
            gridt = VN_GRID;
            break;

        case 'T':
        case 'H':
        case 'A':
            // Tripod, Hex or Asterisk
            gridt = HEX_GRID;
            break;

        case 'L':
            // Triangular
            gridt = TRI_GRID;
            break;

        case 'C':
        case '2':
            // Circular or L2/Euclidean
            gridt = CIRC_GRID;
            break;

        default:
            // do nothing since grid type was defaulted to SQUARE_GRID
            // and may have been updated by read_custom or read_weighted
            break;
    }
    grid_type = gridt;

    if (suffix) {
        // use a bounded universe
        int minsize = 2 * range;
        if (newwd < minsize) newwd = minsize;
        if (newht < minsize) newht = minsize;
        
        // if the new size is different or range has changed or ntype has changed
        // or the old universe is unbounded then we need to create new grids
        if (gwd != newwd || ght != newht || range != oldrange || ntype != oldtype || unbounded) {
            if (population > 0) {
                save_cells();       // store the current pattern in cell_list
            }
            // free the current grids and allocate new ones
            free(outergrid1);
            if (outergrid2) {
                free(outergrid2);
                outergrid2 = NULL;
            }
            create_grids(newwd, newht);
            if (cell_list.size() > 0) {
                // restore the pattern (if the new grid is smaller then any live cells
                // outside the grid will be saved in clipped_cells)
                restore_cells();
            }
        }
        
        // tell GUI code not to call CreateBorderCells and DeleteBorderCells
        unbounded = false;
        
        // set bounded grid dimensions used by GUI code
        gridwd = gwd;
        gridht = ght;
    } else {
        // no suffix given so use an unbounded universe
        unbounded = true;
        
        // set unbounded grid dimensions used by GUI code
        gridwd = 0;
        gridht = 0;
        
        // check if previous universe was bounded
        if (gwd < outerwd) {
            // we could call resize_grids(-border,-border,-border,-border)
            // to remove the outer border but there's a (slight) danger
            // the call will fail due to lack of memory, so safer to use
            // the current grids and just shift the pattern position
            if (population > 0) {
                // shift pattern boundaries
                minx += border;
                maxx += border;
                miny += border;
                maxy += border;
            }
            currgrid = outergrid1;
            nextgrid = outergrid2;
            gwd = outerwd;
            ght = outerht;
            // set grid coordinates of cell at bottom right corner of grid
            gwdm1 = gwd - 1;
            ghtm1 = ght - 1;
            // adjust cell coordinates of grid edges
            gtop -= border;
            gleft -= border;
            gbottom = gtop + ghtm1;
            gright = gleft + gwdm1;
            // set bigint versions of grid edges (used by GUI code)
            gridtop = gtop;
            gridleft = gleft;
            gridbottom = gbottom;
            gridright = gright;
        }

        // reallocate colcounts if ntype has changed
        if (ntype != oldtype) {
            allocate_colcounts();
        }
        
        if (colcounts == NULL && outergrid2 == NULL) {
            // this can happen if previous rule used NM and was unbounded,
            // and new rule uses NN and is unbounded and range <= SMALL_NN_RANGE
            outergrid2 = (unsigned char*) calloc(outerbytes, sizeof(unsigned char));
            if (outergrid2 == NULL) lifefatal("Not enough memory for nextgrid!");
            nextgrid = outergrid2;
        }

        if (colcounts && outergrid2) {
            // faster_* calls don't use outergrid2, so we deallocate it and
            // reset it to NULL (also necessary for test in step() loop)
            free(outergrid2);
            outergrid2 = NULL;
            nextgrid = NULL;
        }
    }

    // set the number of cell states
    if (scount > 2) {
        // show history
        maxCellStates = scount;
    } else {
        maxCellStates = 2;
        scount = 0;         // show C0 in canonical rule
    }

    // set the canonical rule
    int canonr = (ntype == 'L' ? range >> 1 : range);
    if (wasLtL) {
        if (unbounded) {
            if (ntype == '@' || ntype == 'W') {
                // null terminate neighborhood string
                *(char*)nbrend = 0;
                sprintf(canonrule, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c%s",
                                canonr, scount, m, s1, s2, b1, b2, ntype, nbrhd);
            } else {
                sprintf(canonrule, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c",
                                canonr, scount, m, s1, s2, b1, b2, ntype);
            }
        } else {
            if (ntype == '@' || ntype == 'W') {
                // null terminate neighborhood string
                *(char*)nbrend = 0;
                sprintf(canonrule, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c%s:%c%d,%d",
                                canonr, scount, m, s1, s2, b1, b2, ntype, nbrhd, topology, gwd, ght);
            } else {
                sprintf(canonrule, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c:%c%d,%d",
                                canonr, scount, m, s1, s2, b1, b2, ntype, topology, gwd, ght);
            }
        }
    } else {
        // note: flags_string() returns a malloc'd string so it needs to be freed
        char *blist = flags_string(births, maxb + 1);
        char *slist = flags_string(survivals, maxs + 1);

        // blist and slist will fit in canonical rule since they are no longer than the specified rule
        // and possibly smaller in canonical form
        if (unbounded) {
            switch (ntype) {
                case 'M':
                    sprintf(canonrule, "R%d,C%d,S%s,B%s", canonr, scount, slist, blist);
                    break;
                case '@':
                case 'W':
                    // null terminate neighborhood string
                    *(char*)nbrend = 0;
                    sprintf(canonrule, "R%d,C%d,S%s,B%s,N%c%s", canonr, scount, slist, blist, ntype, nbrhd);
                    break;
                default:
                    sprintf(canonrule, "R%d,C%d,S%s,B%s,N%c", canonr, scount, slist, blist, ntype);
                    break;
            }
        } else {
            switch (ntype) {
                case 'M':
                    sprintf(canonrule, "R%d,C%d,S%s,B%s:%c%d,%d", canonr, scount, slist, blist, topology, gwd, ght);
                    break;
                case '@':
                case 'W':
                    // null terminate neighborhood string
                    *(char*)nbrend = 0;
                    sprintf(canonrule, "R%d,C%d,S%s,B%s,N%c%s:%c%d,%d", canonr, scount, slist, blist, ntype, nbrhd, topology, gwd, ght);
                    break;
                default:
                    sprintf(canonrule, "R%d,C%d,S%s,B%s,N%c:%c%d,%d", canonr, scount, slist, blist, ntype, topology, gwd, ght);
                    break;
            }
        }

        // free the strings
        free(blist);
        free(slist);
    }
    
    // setup B0 emulation if needed
    if (b0) {
        setup_b0_emulation(maxn);
    }

    // algos assume totalistic neighborhoods so if the middle cell was not specified
    // we need to adjust the survival list by + 1
    // exception is for Weighted where the middle cell specification is ignored since
    // it is explicitly defined in the neighborhood
    if (m == 0 && ntype != 'W') {
        unsigned char* ss = (unsigned char*) calloc(maxs + 2, sizeof(unsigned char));
        if (ss == NULL) lifefatal("Not enough memory to grow survival flags!");
        memcpy(ss + 1, survivals, maxs + 1);
        free(survivals);
        survivals = ss;

        // check for B0 alternate
        if (b0) {
            unsigned char* ss = (unsigned char*) calloc(maxs + 2, sizeof(unsigned char));
            if (ss == NULL) lifefatal("Not enough memory to grow survival flags!");
            memcpy(ss + 1, altsurvivals, maxs + 1);
            free(altsurvivals);
            altsurvivals = ss;
        }
    }

    return 0;
}

// -----------------------------------------------------------------------------

const char* ltlalgo::getrule()
{
   return canonrule;
}

// -----------------------------------------------------------------------------

const char* ltlalgo::DefaultRule()
{
    return DEFAULTRULE;
}

// -----------------------------------------------------------------------------

static lifealgo *creator() { return new ltlalgo(); }

void ltlalgo::doInitializeAlgoInfo(staticAlgoInfo& ai)
{
    ai.setAlgorithmName("Larger than Life");
    ai.setAlgorithmCreator(&creator);
    ai.setDefaultBaseStep(10);
    ai.setDefaultMaxMem(0);
    ai.minstates = 2;
    ai.maxstates = 256;
    // init default color scheme
    ai.defgradient = true;              // use gradient
    ai.defr1 = 255;                     // start color = yellow
    ai.defg1 = 255;
    ai.defb1 = 0;
    ai.defr2 = 255;                     // end color = red
    ai.defg2 = 0;
    ai.defb2 = 0;
    // if not using gradient then set all states to white
    for (int i=0; i<256; i++) {
        ai.defr[i] = ai.defg[i] = ai.defb[i] = 255;
    }
}

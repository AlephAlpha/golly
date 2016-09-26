                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

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
#ifndef GENERALGO_H
#define GENERALGO_H
#include "ghashbase.h"
/**
 *   Our Generations algo class.
 */
class generationsalgo : public ghashbase {
public:
   generationsalgo() ;
   virtual ~generationsalgo() ;
   virtual state slowcalc(state nw, state n, state ne, state w, state c,
                          state e, state sw, state s, state se) ;
   virtual const char* setrule(const char* s) ;
   virtual const char* getrule() ;
   virtual const char* DefaultRule() ;
   virtual int NumCellStates() ;
   static void doInitializeAlgoInfo(staticAlgoInfo &) ;

   // we need 2 tables to support B0-not-Smax rule emulation
   // where max is 8, 6 or 4 depending on the neighborhood
   char rule0[ALL3X3] ;      // rule table for even gens if rule has B0 but not Smax,
                             // or for all gens if rule has no B0, or it has B0 *and* Smax
   char rule1[ALL3X3] ;      // rule table for odd gens if rule has B0 but not Smax
   bool alternate_rules ;    // set by setrule; true if rule has B0 but not Smax

   enum neighborhood_masks {
      MOORE = 0x1ff,         // all 8 neighbors
      HEXAGONAL = 0x1bb,     // ignore NE and SW neighbors
      VON_NEUMANN = 0x0ba    // 4 orthogonal neighbors
   } ;

   bool isHexagonal() const { return neighbormask == HEXAGONAL ; }
   bool isVonNeumann() const { return neighbormask == VON_NEUMANN ; }

private:
   char canonrule[MAXRULESIZE] ;      // canonical version of valid rule passed into setrule
   neighborhood_masks neighbormask ;  // neighborhood masks in 3x3 table
   bool totalistic ;                  // is rule totalistic?
   int neighbors ;                    // number of neighbors
   int rulebits ;                     // bitmask of neighbor counts used (9 birth, 9 survival)
   int letter_bits[18] ;              // bitmask for non-totalistic letters used
   int negative_bit ;                 // bit in letters bits mask indicating negative
   int survival_offset ;              // bit offset in rulebits for survival
   int max_letters[18] ;              // maximum number of letters per neighbor count
   const int *order_letters[18] ;     // canonical letter order per neighbor count
   const char *valid_rule_letters ;   // all valid letters
   const char *rule_letters[4] ;      // valid rule letters per neighbor count
   const int *rule_neighborhoods[4] ; // isotropic neighborhoods per neighbor count
   char rule3x3[ALL3X3] ;             // all 3x3 cell mappings 012345678->4'
   const char *neighbor4swap ;        // swap table for 4 neighbor isotropic letters for B0 rules

   void initRule() ;
   void setTotalistic(int value, bool survival) ;
   int flipBits(int x) ;
   int rotateBits90Clockwise(int x) ;
   void setSymmetrical512(int x, int b) ;
   void setSymmetrical(int value, bool survival, int lindex, int normal) ;
   void setTotalisticRuleFromString(const char *rule, bool survival) ;
   void setRuleFromString(const char *rule, bool survival) ;
   void createRuleMap(const char *birth, const char *survival) ;
   void createB0SmaxRuleMap(const char *birth, const char *survival) ;
   void createB0EvenRuleMap(const char *birth, const char *survival) ;
   void createB0OddRuleMap(const char *birth, const char *survival) ;
   void createCanonicalName() ;
   void removeChar(char *string, char skip) ;
   bool lettersValid(const char *part) ;
   int addLetters(int count, int p) ;
} ;

#endif

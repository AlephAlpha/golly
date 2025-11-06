// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "generationsalgo.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if defined(WIN32) || defined(WIN64)
#define strncasecmp _strnicmp
#endif

using namespace std ;

int generationsalgo::NumCellStates() {
   return maxCellStates ;

}

static const char *DEFAULTRULE = "12/34/3" ;
const char* generationsalgo::DefaultRule() {
   return DEFAULTRULE ;
}

state generationsalgo::slowcalc(state nw, state n, state ne, state w, state c,
                                state e, state sw, state s, state se) {
   // result
   state result = 0 ;

   // get the lookup table
   char *lookup = rule3x3 ;

   // create array index
   int index = ((nw == 1) ? 256 : 0) | ((n == 1) ? 128 : 0) | ((ne == 1) ? 64 : 0)
      | ((w == 1) ? 32 : 0) | ((c == 1) ? 16 : 0) | ((e == 1) ? 8 : 0)
      | ((sw == 1) ? 4 : 0) | ((s == 1) ? 2 : 0) | ((se == 1) ? 1 : 0) ;

   // lookup the next generation
   if (c <= 1 && lookup[index]) {
      result = 1 ;
   }
   else {
      if (c > 0 && c + 1 < maxCellStates) {
         result = c + 1 ;
      }
      else {
         result = 0 ;
      }
   }

   return result ;
}

static lifealgo *creator() { return new generationsalgo() ; }

void generationsalgo::doInitializeAlgoInfo(staticAlgoInfo &ai) {
   ghashbase::doInitializeAlgoInfo(ai) ;
   ai.setAlgorithmName("Generations") ;
   ai.setAlgorithmCreator(&creator) ;
   ai.minstates = 2 ;
   ai.maxstates = 256 ;
   // init default color scheme
   ai.defgradient = true ;              // use gradient
   ai.defr1 = 255 ;                     // start color = red
   ai.defg1 = 0 ;
   ai.defb1 = 0 ;
   ai.defr2 = 255 ;                     // end color = yellow
   ai.defg2 = 255 ;
   ai.defb2 = 0 ;
   // if not using gradient then set all states to white
   for (int i=0 ; i<256 ; i++) {
      ai.defr[i] = ai.defg[i] = ai.defb[i] = 255 ;
   }
}

generationsalgo::generationsalgo() {
   int i ;

   // base64 encoding characters
   base64_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" ;

   // all valid rule letters
   valid_rule_letters = "012345678ceaiknjqrytwz-" ;

   // rule letters per neighbor count
   rule_letters[0] = "ce" ;
   rule_letters[1] = "ceaikn" ;
   rule_letters[2] = "ceaiknjqry" ;
   rule_letters[3] = "ceaiknjqrytwz" ;

   // isotropic neighborhoods per neighbor count
   static int entry0[2] = { 1, 2 } ;
   static int entry1[6] = { 5, 10, 3, 40, 33, 68 } ;
   static int entry2[10] = { 69, 42, 11, 7, 98, 13, 14, 70, 41, 97 } ;
   static int entry3[13] = { 325, 170, 15, 45, 99, 71, 106, 102, 43, 101, 105, 78, 108 } ;
   rule_neighborhoods[0] = entry0 ;
   rule_neighborhoods[1] = entry1 ;
   rule_neighborhoods[2] = entry2 ;
   rule_neighborhoods[3] = entry3 ;

   // bit offset for suvival part of rule
   survival_offset = 9 ;

   // bit in letter bit mask indicating negative
   negative_bit = 13 ; 

   // maximum number of letters per neighbor count
   max_letters[0] = 0 ;
   max_letters[1] = (int) strlen(rule_letters[0]) ;
   max_letters[2] = (int) strlen(rule_letters[1]) ;
   max_letters[3] = (int) strlen(rule_letters[2]) ;
   max_letters[4] = (int) strlen(rule_letters[3]) ;
   max_letters[5] = max_letters[3] ;
   max_letters[6] = max_letters[2] ;
   max_letters[7] = max_letters[1] ;
   max_letters[8] = max_letters[0] ;
   for (i = 0 ; i < survival_offset ; i++) {
      max_letters[i + survival_offset] = max_letters[i] ;
   }

   // canonical letter order per neighbor count
   static int order0[1] = { 0 } ;
   static int order1[2] = { 0, 1 } ;
   static int order2[6] = { 2, 0, 1, 3, 4, 5 } ;
   static int order3[10] = { 2, 0, 1, 3, 6, 4, 5, 7, 8, 9 } ;
   static int order4[13] = { 2, 0, 1, 3, 6, 4, 5, 7, 8, 10, 11, 9, 12 } ;
   order_letters[0] = order0 ;
   order_letters[1] = order1 ;
   order_letters[2] = order2 ;
   order_letters[3] = order3 ;
   order_letters[4] = order4 ;
   order_letters[5] = order3 ;
   order_letters[6] = order2 ;
   order_letters[7] = order1 ;
   order_letters[8] = order0 ;
   for (i = 0 ; i < survival_offset ; i++) {
      order_letters[i + survival_offset] = order_letters[i] ;
   }

   // initialize
   initRule() ;
}

generationsalgo::~generationsalgo() {
}

// returns a count of the number of bits set in given int
static int bitcount(int v) {
   int r = 0 ;
   while (v) {
      r++ ;
      v &= v - 1 ;
   }
   return r ;
}

// initialize
void generationsalgo::initRule() {
   // default to Moore neighbourhood totalistic rule
   neighbormask = MOORE ; 
   neighbors = 8 ;
   totalistic = true ;
   using_map = false ;

   // we may need this to be >2 here so it's recognized as multistate
   maxCellStates = 3 ;

   // one bit for each neighbor count
   // s = survival, b = birth
   // bit:     17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
   // meaning: s8 s7 s6 s5 s4 s3 s2 s1 s0 b8 b7 b6 b5 b4 b3 b2 b1 b0
   rulebits = 0 ;

   // one bit for each letter per neighbor count
   // N = negative bit
   // bit:     13 12 11 10  9  8  7  6  5  4  3  2  1  0
   // meaning:  N  z  w  t  y  r  q  j  n  k  i  a  e  c
   memset(letter_bits, 0, sizeof(letter_bits)) ;

   // canonical rule string
   memset(canonrule, 0, sizeof(canonrule)) ;
}

// set 3x3 grid based on totalistic value
void generationsalgo::setTotalistic(int value, bool survival) {
   int mask = 0 ;
   int nbrs = 0 ;
   int nhood = 0 ;
   int i = 0 ;
   int j = 0 ;
   int offset = 0 ;

   // check if this value has already been processed
   if (survival) {
      offset = survival_offset ;
   }
   if ((rulebits & (1 << (value + offset))) == 0) {
       // update the rulebits
       rulebits |= 1 << (value + offset) ;

       // update the mask if survival
       if (survival) {
          mask = 0x10 ;
       }

       // fill the array based on totalistic value
       for (i = 0 ; i < ALL3X3 ; i += 32) {
          for (j = 0 ; j < 16 ; j++) {
             nbrs = 0 ;
             nhood = (i+j) & neighbormask ;
             while (nhood > 0) {
                nbrs += (nhood & 1) ;
                nhood >>= 1 ;
             }
             if (value == nbrs) {
                rule3x3[i+j+mask] = 1 ;
             }
          }
      }
   }
}

// flip bits
int generationsalgo::flipBits(int x) {
   return ((x & 0x07) << 6) | ((x & 0x1c0) >> 6) | (x & 0x38) ;
}

// rotate 90
int generationsalgo::rotateBits90Clockwise(int x) {
   return ((x & 0x4) << 6) | ((x & 0x20) << 2) | ((x & 0x100) >> 2)
                        | ((x & 0x2) << 4) | (x & 0x10) | ((x & 0x80) >> 4)
                        | ((x & 0x1) << 2) | ((x & 0x8) >> 2) | ((x & 0x40) >> 6) ;
}

// set symmetrical neighborhood into 3x3 map
void generationsalgo::setSymmetrical512(int x, int b) {
   int y = x ;
   int i = 0 ;

   // process each of the 4 rotations
   for (i = 0 ; i < 4 ; i++) {
      rule3x3[y] = (char) b ;
      y = rotateBits90Clockwise(y) ;
   }

   // flip
   y = flipBits(y) ;

   // process each of the 4 rotations
   for (i = 0 ; i < 4 ; i++) {
      rule3x3[y] = (char) b ;
      y = rotateBits90Clockwise(y) ;
   }
}

// set symmetrical neighborhood
void generationsalgo::setSymmetrical(int value, bool survival, int lindex, int normal) {
   int xorbit = 0 ;
   int nindex = value - 1 ;
   int x = 0 ;
   int offset = 0 ;

   // check for homogeneous bits
   if (value == 0 || value == 8) {
      setTotalistic(value, survival) ;
   }
   else {
      // update the rulebits
      if (survival) {
         offset = survival_offset ;
      }
      rulebits |= 1 << (value + offset) ;

      // reflect the index if in second half
      if (nindex > 3) {
         nindex = 6 - nindex ;
         xorbit = 0x1ef ;
      }

      // update the letterbits
      letter_bits[value + offset] |= 1 << lindex ;

      if (!normal) {
         // set the negative bit
         letter_bits[value + offset] |= 1 << negative_bit ;
      }

      // lookup the neighborhood
      x = rule_neighborhoods[nindex][lindex] ^ xorbit ;
      if (survival) {
         x |= 0x10 ;
      }
      setSymmetrical512(x, normal) ;
   }
}

// set totalistic birth or survival rule from a string
void generationsalgo::setTotalisticRuleFromString(const char *rule, bool survival) {
   char current ;

   // process each character in the rule string
   while ( *rule ) {
      current = *rule ;
      rule++ ;

      // convert the digit to an integer
      current -= '0' ;

      // set totalistic
      setTotalistic(current, survival) ;
   }
}

// set rule from birth or survival string
void generationsalgo::setRuleFromString(const char *rule, bool survival) {
   // current and next character
   char current ;
   char next ;

   // whether character normal or inverted
   int normal = 1 ;

   // letter index
   char *letterindex = 0 ;
   int lindex = 0 ;
   int nindex = 0 ;

   // process each character
   while ( *rule ) {
      current = *rule ;
      rule++ ;

      // find the index in the valid character list
      letterindex = strchr((char*) valid_rule_letters, current) ;
      lindex = letterindex ? int(letterindex - valid_rule_letters) : -1 ;

      // check if it is a digit
      if (lindex >= 0 && lindex <= 8) {
         // determine what follows the digit
         next = *rule ;
         nindex = -1 ;
         if (next) {
            letterindex = strchr((char*) rule_letters[3], next) ;
            if (letterindex) {
               nindex = int(letterindex - rule_letters[3]) ;
            }
         }

         // is the next character a digit or minus?
         if (nindex == -1) {
            setTotalistic(lindex, survival) ;
         }

         // check for inversion
         normal = 1 ;
         if (next == '-') {
            rule++ ;
            next = *rule ;

            // invert following character meanings
            normal = 0 ;
         }

         // process non-totalistic characters
         if (next) {
            letterindex = strchr((char*) rule_letters[3], next) ;
            nindex = -1 ;
            if (letterindex) {
               nindex = int(letterindex - rule_letters[3]) ;
            }
            while (nindex >= 0) {
               // set symmetrical
               setSymmetrical(lindex, survival, nindex, normal) ;

               // get next character
               rule++ ;
               next = *rule ;
               nindex = -1 ;
               if (next) {
                  letterindex = strchr((char*) rule_letters[3], next) ;
                  if (letterindex) {
                     nindex = int(letterindex - rule_letters[3]) ;
                  }
               }
            }
         }
      }
   }
}

// create the rule map from the base64 encoded map
void generationsalgo::createRuleMapFromMAP(const char *base64) {
   // set the number of characters to read
   int power2 = 1 << (neighbors + 1) ;
   int fullchars = power2 / 6 ;
   int remainbits = power2 % 6 ;

   // create an array to read the MAP bits
   char bits[ALL3X3] ;

   // decode the base64 string
   int i = 0 ;
   char c = 0 ;
   int j = 0 ;
   const char *index = 0 ;

   for (i = 0 ; i < fullchars ; i++) {
      // convert character to base64 index
      index = strchr(base64_characters, *base64) ;
      base64++ ;
      c = index ? (char)(index - base64_characters) : 0 ;

      // decode the character
      bits[j] = c >> 5 ;
      j++ ;
      bits[j] = (c >> 4) & 1 ;
      j++ ;
      bits[j] = (c >> 3) & 1 ;
      j++ ;
      bits[j] = (c >> 2) & 1 ;
      j++ ;
      bits[j] = (c >> 1) & 1 ;
      j++ ;
      bits[j] = c & 1 ;
      j++ ;
   }

   // decode remaining bits from final character
   if (remainbits > 0) {
      index = strchr(base64_characters, *base64) ;
      c = index ? (char)(index - base64_characters) : 0 ;
      int b = 5 ;

      while (remainbits > 0) {
          bits[j] = (c >> b) & 1 ;
          b-- ;
          j++ ;
          remainbits-- ;
      }
   }

   // copy into rule array using the neighborhood mask
   int k, m ;
   for (i = 0 ; i < ALL3X3 ; i++) {
      k = 0 ;
      m = neighbors ;
      for (j = 8 ; j >= 0 ; j--) {
         if (neighbormask & (1 << j)) {
             if (i & (1 << j)) {
                k |= (1 << m) ;
             }
             m-- ;
         }
      }
      rule3x3[i] = bits[k] ;
   }
}

// create the rule map from birth and survival strings
void generationsalgo::createRuleMap(const char *birth, const char *survival) {
   // clear the rule array
   memset(rule3x3, 0, ALL3X3) ;

   // check for totalistic rule
   if (totalistic) {
      // set the totalistic birth rule
      setTotalisticRuleFromString(birth, false) ;

      // set the totalistic surivival rule
      setTotalisticRuleFromString(survival, true) ;
   }
   else {
      // set the non-totalistic birth rule
      setRuleFromString(birth, false) ;

      // set the non-totalistic survival rule
      setRuleFromString(survival, true) ;
   }
}

// add canonical letter representation
int generationsalgo::addLetters(int count, int p) {
   int bits ;            // bitmask of letters defined at this count
   int negative = 0 ;    // whether negative
   int setbits ;         // how many bits are defined
   int maxbits ;         // maximum number of letters at this count
   int letter = 0 ;
   int j ;

   // check if letters are defined for this neighbor count
   if (letter_bits[count]) {
      // get the bit mask
      bits = letter_bits[count] ;

      // check for negative
      if (bits & (1 << negative_bit)) {
         // letters are negative
         negative = 1 ;
         bits &= ~(1 << negative_bit) ;
      }

      // compute the number of bits set
      setbits = bitcount(bits) ;

      // get the maximum number of allowed letters at this neighbor count
      maxbits = max_letters[count] ;

      // do not invert if not negative and seven letters
      if (!(!negative && setbits == 7 && maxbits == 13)) {
         // if maximum letters minus number used is greater than number used then invert
         if (setbits + negative > (maxbits >> 1)) {
            // invert maximum letters for this count
            bits = ~bits & ((1 << maxbits) - 1) ;
            if (bits) {
               negative = !negative ;
            }
         }
      }

      // if negative and no letters then remove neighborhood count
      if (negative && !bits) {
         canonrule[p] = 0 ;
         p-- ;
      }
      else {
         // check whether to output minus
         if (negative) {
            canonrule[p++] = '-' ;
         }

         // add defined letters
         for (j = 0 ; j < maxbits ; j++) {
            // lookup the letter in order
            letter = order_letters[count][j] ;
            if (bits & (1 << letter)) {
               canonrule[p++] = rule_letters[3][letter] ;
            }
         }
      }
   }

   return p ;
}

// AKT: store valid rule in canonical format for getrule()
void generationsalgo::createCanonicalName(const char *base64) {
   int p = 0 ;
   int np = 0 ;
   int i = 0 ;

   // the canonical version of a rule containing letters
   // might be simply totalistic
   bool stillnontotalistic = false ;

   // check for map rule
   if (using_map) {
      // output map header
      canonrule[p++] = 'M' ;
      canonrule[p++] = 'A' ;
      canonrule[p++] = 'P' ;

      // compute number of base64 characters
      int power2 = 1 << (neighbors + 1) ;
      int fullchars = power2 / 6 ;
      int remainbits = power2 % 6 ;

      // copy base64 part
      for (i = 0 ; i < fullchars ; i++) {
         if (*base64) {
            canonrule[p++] = *base64 ;
            base64++ ;
         }
      }

      // copy final bits of last character
      if (*base64) {
         const char *index = strchr(base64_characters, *base64) ;
         int c = index ? (char)(index - base64_characters) : 0 ;
         int k = 0 ;
         int m = 5 ;
         for (i = 0 ; i < remainbits ; i++) {
            k |= c & (1 << m) ;
            m-- ;
         }
         canonrule[p++] = base64_characters[k] ;
      }
   }
   else {
      // output survival part
      for (i = 0 ; i <= neighbors ; i++) {
         if (rulebits & (1 << (survival_offset+i))) {
            canonrule[p++] = '0' + (char)i ;
   
            // check for non-totalistic
            if (!totalistic) {
               // add any defined letters
               np = addLetters(survival_offset + i, p) ;
   
               // check if letters were added
               if (np != p) {
                  if (np > p) {
                     stillnontotalistic = true ;
                  }
                  p = np ;
               }
            }
         }
      }
   
      // add slash
      canonrule[p++] = '/' ;
   
      // output birth part
      for (i = 0 ; i <= neighbors ; i++) {
         if (rulebits & (1 << i)) {
            canonrule[p++] = '0' + (char)i ;
   
            // check for non-totalistic
            if (!totalistic) {
               // add any defined letters
               np = addLetters(i, p) ;
        
               // check if letters were added
               if (np != p) {
                  if (np > p) {
                     stillnontotalistic = true ;
                  }
                  p = np ;
               }
            }
         }
      }
   }

   // add slash
   canonrule[p++] = '/' ;

   // output state count
   char states[4] ;
   memset(states, 0, sizeof(states)) ;
   sprintf(states, "%d", maxCellStates) ;
   i = 0 ;
   while (states[i]) canonrule[p++] = states[i++] ;

   // check if non-totalistic became totalistic
   if (!totalistic && !stillnontotalistic) {
      totalistic = true ;
   }

   // add neighborhood if not MAP rule
   if (!using_map) {
      if (neighbormask == HEXAGONAL) canonrule[p++] = 'H' ;
      if (neighbormask == VON_NEUMANN) canonrule[p++] = 'V' ;
   }

   // check for bounded grid
   if (gridwd > 0 || gridht > 0) {
      // algo->setgridsize() was successfully called above, so append suffix
      const char* bounds = canonicalsuffix() ;
      i = 0 ;
      while (bounds[i]) canonrule[p++] = bounds[i++] ;
   }

   // null terminate
   canonrule[p] = 0 ;

   // set canonical rule
   ghashbase::setrule(canonrule) ;
}

// remove character from a string in place
void generationsalgo::removeChar(char *string, char skip) {
   int src = 0 ;
   int dst = 0 ;
   char c = string[src++] ;

   // copy characters other than skip
   while ( c ) {
      if (c != skip) {
         string[dst++] = c ;
      }
      c = string[src++] ;
   }

   // ensure null terminated
   string[dst] = 0 ;
}

// check whether non-totalistic letters are valid for defined neighbor counts
bool generationsalgo::lettersValid(const char *part) {
   char c ;
   char *cptr = NULL ;
   int nindex = 0 ;
   int currentCount = -1 ;
   bool negative = false ;
   bool usedNormal[9] ;
   bool usedNegative[9] ;

   // clear used flags
   for (int i = 0; i < 9; i += 1) {
      usedNormal[i] = false ;
      usedNegative[i] = false ;
   }

   // get next character
   while ( *part ) {
      c = *part ;
      if (c >= '0' && c <= '8') {
         currentCount = c - '0' ;
         nindex = currentCount - 1 ;
         if (nindex > 3) {
            nindex = 6 - nindex ;
         }
         negative = false ;
      } else {
         // check for minus
         if (c == '-') {
            negative = true ;
         } else {
            // not valid if 0 or 8
            if (currentCount == 0 || currentCount == 8) {
               return false ;
            }

            // check against valid rule letters for this neighbor count
            cptr = strchr((char*) rule_letters[nindex], c) ;
            if (cptr == NULL) {
               return false ;
            } else {
               // mark letter used at this neighbor count
               if (negative) {
                  usedNegative[currentCount] = true ;
               } else {
                  usedNormal[currentCount] = true ;
               }

               // check for both negative and positive used
               if (usedNormal[currentCount] && usedNegative[currentCount]) {
                  return false ;
               }
            }
         }
      }
      part++ ;
   }

   return true ;
}

// set rule
const char *generationsalgo::setrule(const char *rulestring) {
   char *r = (char *)rulestring ;
   char tidystring[MAXRULESIZE] ;  // tidy version of rule string
   char *t = (char *)tidystring ;
   char *end = r + strlen(r) ;     // end of rule string
   char c ;
   char *charpos = 0 ;
   int digit ;
   int maxdigit = 0 ;              // maximum digit value found
   char *colonpos = 0 ;            // position of colon
   char *slashpos = 0 ;            // position of first slash
   char *slashpos2 = 0 ;           // position of second slash
   char *bpos = 0 ;                // position of b
   char *spos = 0 ;                // position of s
   bool underscore_used = false ;  // whether underscore used
   int num_states = 0 ;            // number of cell states

   // initialize rule type
   initRule() ;

   // check if rule is too long
   if (strlen(rulestring) > (size_t) MAXRULESIZE) {
      return "Rule name is too long." ;
   }

   // check for colon
   colonpos = strchr(r, ':') ;
   if (colonpos) {
      // only process up to the colon
      end = colonpos ;
   }

   // skip any whitespace
   while (*r == ' ') {
      r++ ;
   }

   // check for map
   if (strncasecmp(r, "map", 3) == 0) {
      // attempt to decode map
      r += 3 ;
      bpos = r ;

      // terminate at the colon if one is present
      if (colonpos) *colonpos = 0 ;

      // check the length of the map
      int maplen = (int) strlen(r) ;

      // find the last slash
      char *lastslash = strrchr(r, '/') ;

      // replace the colon if one was present
      if (colonpos) *colonpos = ':' ;

      // check if there was a final slash
      if (lastslash == NULL) {
          return "Generations rule needs number of states." ;
      }

      // length is up to the final slash
      maplen = (int) (lastslash - r) ;

      // check if there is base64 padding
      if (maplen > 2 && !strncmp(r + maplen - 2, "==", 2)) {
         // remove padding
         maplen -= 2 ;
      }

      // check if the map length is valid for Moore, Hexagonal or von Neumann neighborhoods
      if (!(maplen == MAP512LENGTH || maplen == MAP128LENGTH || maplen == MAP32LENGTH)) {
          return "MAP rule needs 6, 22 or 86 base64 characters." ;
      }

      // validate the characters
      spos = r + maplen ;
      while (r < spos) {
         if (!strchr(base64_characters, *r)) {
             return "MAP contains illegal base64 character." ;
         }
         r++ ;
      }

      // set the neighborhood based on the map length
      if (maplen == MAP128LENGTH) {
         neighbormask = HEXAGONAL ;
         neighbors = 6 ;
      } else {
         if (maplen == MAP32LENGTH) {
            neighbormask = VON_NEUMANN ;
            neighbors = 4 ;
         }
      }

      // read number of states
      r = lastslash + 1 ;
      c = *r ;
      while (c) {
         if (c >= '0' && c <= '9') {
            num_states = num_states * 10 + (c - '0') ;
            r++ ;
            c = *r ;
         }
         else {
            c = 0 ;
         }
      }

      // check number of cell states is valid
      if (num_states < 2) {
         return "Number of states too low in Generations rule." ;
      }
      if (num_states > 256) {
         return "Number of states too high in Generations rule." ;
      }

      // check for trailing characters
      if (*r && r != colonpos) {
         return "Illegal trailing characters after MAP." ;
      }

      // set the number of cell states
      maxCellStates = num_states ;

      // map looks valid
      using_map = true ;
   }
   else {
      // create lower case version of rule name without spaces
      while (r < end) {
         // get the next character and convert to lowercase
         c = (char) tolower(*r) ;
   
         // process the character
         switch (c) {
         // birth
         case 'b':
            if (slashpos2) {
               // b in state part
               return "B not valid after second slash." ;
            }
            if (bpos) {
               // multiple b found
               return "Only one B allowed." ;
            }
            bpos = t ;
            *t = c ;
            t++ ;
            break ;
   
         // survival
         case 's':
            if (slashpos2) {
               // s in state part
               return "S not valid after second slash." ;
            }
            if (spos) {
               // multiple s found
               return "Only one S allowed." ;
            }
            spos = t ;
            *t = c ;
            t++ ;
            break ;
   
         // underscore
         case '_':
            underscore_used = true ;
            // fall through...
   
         // slash
         case '/':
            if (slashpos) {
               // multiple slashes found
               if (slashpos2) {
                  return "Only two slashes allowed." ;
               }
               else {
                  slashpos2 = t ;
                  *t = c ;
                  t++ ;
               }
            }
            else {
               slashpos = t ;
               *t = c ;
               t++ ;
            }
            break ;
           
         // hex
         case 'h':
            if (neighbormask != MOORE) {
               // multiple neighborhoods specified
               return "Only one neighborhood allowed." ;
            }
            neighbormask = HEXAGONAL ;
            neighbors = 6 ;
            *t = c ;
            t++ ;
            break ;
   
         // von neumann
         case 'v':
            if (neighbormask != MOORE) {
               // multiple neighborhoods specified
               return "Only one neighborhood allowed." ;
            }
            neighbormask = VON_NEUMANN ;
            neighbors = 4 ;
            *t = c ;
            t++ ;
            break ;
   
         // minus
         case '-':
            // check if previous character is a digit
            if (t == tidystring || *(t-1) < '0' || *(t-1) > '8') {
               // minus can only follow a digit
               return "Minus can only follow a digit." ;
            }
            *t = c ;
            t++ ;
            totalistic = false ;
            break ;
   
         // other characters
         default:
            // ignore space
            if (c != ' ') {
               // check for state count after final slash
               if (slashpos2) {
                  if (c >= '0' && c <= '9') {
                     num_states = num_states * 10 + (c - '0') ;
                  }
                  else {
                     return "Bad character found." ;
                  }
               }
               else {
                  // check character is valid
                  charpos = strchr((char*) valid_rule_letters, c) ;
                  if (charpos) {
                     // copy character
                     *t = c ; 
                     t++ ;
      
                     // check if totalistic (i.e. found a valid non-digit)
                     digit = int(charpos - valid_rule_letters) ;
                     if (digit > 8) {
                        totalistic = false ;
                     }
                     else {
                        // update maximum digit found
                        if (digit > maxdigit) {
                           maxdigit = digit ;
                        }
                     }
                  }
                  else {
                     return "Bad character found." ;
                  }
               }
            }
            break ;
         }
   
         // next character
         r++ ;
      }
   
      // ensure null terminated
      *t = 0 ;
   
      // don't allow empty rule string
      t = tidystring ;
      if (*t == 0) {
         return "Rule cannot be empty string." ;
      }
   
      // check for two slashes
      if (!slashpos || !slashpos2) {
         return "Rule must contain two slashes." ;
      }
   
      // check number of cell states is valid
      if (num_states < 2) {
         return "Number of states too low in Generations rule." ;
      }

      if (num_states > 256) {
         return "Number of states too high in Generations rule." ;
      }
   
      // set the number of cell states
      maxCellStates = num_states ;
   
      // underscore only valid for non-totalistic rules
      if (underscore_used && totalistic) {
         return "Underscore not valid for totalistic rules, use slash." ;
      }
   
      // if neighborhood specified then must be last character
      if (neighbormask != MOORE) {
         size_t len = strlen(t) ;
         if (len) {
            c = t[len - 1] ;
            if (!((c == 'h') || (c == 'v'))) {
               return "Neighborhood must be at end of rule." ;
            }
            // remove character
            t[len - 1] = 0 ;
         }
      }
   
      // at least one of slash, b or s must be present
      if (!(slashpos || bpos || spos)) {
         return "Rule must contain a slash or B or S." ;
      }

      // digits can not be greater than the number of neighbors for the defined neighborhood
      if (maxdigit > neighbors) {
         return "Digit greater than neighborhood allows." ;
      }
   
      // terminate the rule string at the second slash
      *slashpos2 = 0 ;
   
      // if slash present and both b and s then one must be each side of the slash
      if (slashpos && bpos && spos) {
         if ((bpos < slashpos && spos < slashpos) || (bpos > slashpos && spos > slashpos)) {
            return "B and S must be either side of slash." ;
         }
      }
   
      // slash exists so set determine which part is b and which is s
      *slashpos = 0 ;
   
      // check if b or s are defined
      if (bpos || spos) {
         // check for birth first
         if ((bpos && bpos < slashpos) || (spos && spos > slashpos)) {
            // birth then survival
            bpos = t ;
            spos = slashpos + 1 ;
         }
         else {
            // survival then birth
            bpos = slashpos + 1 ;
            spos = t ;
         }

         // remove b or s from rule parts
         removeChar(bpos, 'b') ;
         removeChar(spos, 's') ;
      }
      else {
         // no b or s so survival first
         spos = t ;
         bpos = slashpos + 1 ;
      }

      // if not totalistic and a part exists it must start with a digit
      if (!totalistic) {
         // check birth
         c = *bpos ;
         if (c && (c < '0' || c > '8')) {
            return "Non-totalistic birth must start with a digit." ;
         }

         // check survival
         c = *spos ;
         if (c && (c < '0' || c > '8')) {
            return "Non-totalistic survival must start with a digit." ;
         }

         // one of birth or survival must be at the start of the rule
         if (!(bpos == tidystring || spos == tidystring)) {
            return "Invalid characters at start of rule." ;
         }
      }
   
      // if not totalistic then neighborhood must be Moore
      if (!totalistic && neighbormask != MOORE) {
         return "Non-totalistic only supported with Moore neighborhood." ;
      }
   
      // validate letters used against each specified neighbor count
      if (!lettersValid(bpos)) {
         return "Letter not valid for birth neighbor count." ;
      }

      if (!lettersValid(spos)) {
         return "Letter not valid for survival neighbor count." ;
      }
   }

   // AKT: check for rule suffix like ":T200,100" to specify a bounded universe
   if (colonpos) {
      const char* err = setgridsize(colonpos) ;
      if (err) return err ;
   } else {
      // universe is unbounded
      gridwd = 0 ;
      gridht = 0 ;
   }

   // check for map
   if (using_map) {
      // generate the 3x3 map from the 512bit map
      createRuleMapFromMAP(bpos) ;
   }
   else {
      // generate the 3x3 map from the birth and survival rules
      createRuleMap(bpos, spos) ;
   }

   // check for B0 rules
   if (rule3x3[0]) {
      return "Generations does not support B0." ;
   }

   // save the canonical rule name
   createCanonicalName(bpos) ;

   // set grid_type
   if (neighbormask == HEXAGONAL)
      grid_type = HEX_GRID;
   else if (neighbormask == VON_NEUMANN)
      grid_type = VN_GRID;
   else // neighbormask == MOORE
      grid_type = SQUARE_GRID;

   // exit with success
   return 0 ;
}

const char* generationsalgo::getrule() {
   return canonrule ;
}

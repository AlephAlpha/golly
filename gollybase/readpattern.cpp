// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "readpattern.h"
#include "lifealgo.h"
#include "liferules.h"      // for MAXRULESIZE
#include "util.h"           // for linereader, lifewarning and *progress calls
#include <cstdio>
#ifdef ZLIB
#include <zlib.h>
#if ZLIB_VERNUM < 0x1240
    // we need gzoffset
    error("Golly requires zlib 1.2.4 or later");
#endif
#endif
#include <cstdlib>
#include <cstring>

#define LINESIZE 20000
#define CR 13
#define LF 10

bool getedges = false;              // find pattern edges?
bigint top, left, bottom, right;    // the pattern edges

#ifdef __APPLE__
#define BUFFSIZE 4096       // 4K is best for Mac OS X
#else
#define BUFFSIZE 8192       // 8K is best for Windows and other platforms???
#endif

#ifdef ZLIB
gzFile zinstream ;
#else
FILE *pattfile ;
#endif

char filebuff[BUFFSIZE];
int buffpos, bytesread, prevchar;

long filesize;              // length of file in bytes

// use buffered getchar instead of slow fgetc
// don't override the "getchar" name which is likely to be a macro
int mgetchar() {
   if (buffpos == BUFFSIZE) {
      double filepos;
      #ifdef ZLIB
         bytesread = gzread(zinstream, filebuff, BUFFSIZE);
         filepos = gzoffset(zinstream);
      #else
         bytesread = fread(filebuff, 1, BUFFSIZE, pattfile);
         filepos = ftell(pattfile);
      #endif
      buffpos = 0;
      lifeabortprogress(filepos / filesize, "");
   }
   if (buffpos >= bytesread) return EOF;
   return filebuff[buffpos++];
}

// use getline instead of fgets so we can handle DOS/Mac/Unix line endings
char *getline(char *line, int maxlinelen) {
   int i = 0;
   while (i < maxlinelen) {
      int ch = mgetchar();
      if (isaborted()) return NULL;
      switch (ch) {
         case CR:
            prevchar = CR;
            line[i] = 0;
            return line;
         case LF:
            if (prevchar != CR) {
               prevchar = LF;
               line[i] = 0;
               return line;
            }
            // if CR+LF (DOS) then ignore the LF
            break;
         case EOF:
            if (i == 0) return NULL;
            line[i] = 0;
            return line;
         default:
            prevchar = ch;
            line[i++] = (char) ch;
            break;
      }
   }
   line[i] = 0;      // silently truncate long line
   return line;
}

const char *SETCELLERROR = "Impossible; set cell error for state 1" ;

// Read a text pattern like "...ooo$$$ooo" where '.', ',' and chars <= ' '
// represent dead cells, '$' represents 10 dead cells, and all other chars
// represent live cells.
const char *readtextpattern(lifealgo &imp, char *line) {
   int x=0, y=0;
   char *p;

   do {
      for (p = line; *p; p++) {
         if (*p == '.' || *p == ',' || *p <= ' ') {
            x++;
         } else if (*p == '$') {
            x += 10;
         } else {
            if (imp.setcell(x, y, 1) < 0) {
               return SETCELLERROR ;
            }
            x++;
         }
      }
      y++ ;
      if (getedges && right.toint() < x - 1) right = x - 1;
      x = 0;
   } while (getline(line, LINESIZE));

   if (getedges) bottom = y - 1;
   return 0 ;
}

/*
 *   Parse "#CXRLE key=value key=value ..." line and extract values.
 */
void ParseXRLELine(char *line, int *xoff, int *yoff, bool *sawpos, bigint &gen) {
   char *key = line;
   while (key) {
      // set key to start of next key word
      while (*key && *key != ' ') key++;
      while (*key == ' ') key++;
      if (*key == 0) return;

      // set value to pos of char after next '='
      char *value = key;
      while (*value && *value != '=') value++;
      if (*value == 0) return;
      value++;

      if (strncmp(key, "Pos", 3) == 0) {
         // extract Pos=int,int
         sscanf(value, "%d,%d", xoff, yoff);
         *sawpos = true;

      } else if (strncmp(key, "Gen", 3) == 0) {
         // extract Gen=bigint
         char *p = value;
         while (*p >= '0' && *p <= '9') p++;
         char savech = *p;
         *p = 0;
         gen = bigint(value);
         *p = savech;
         value = p;
      }

      key = value;
   }
}

/*
 *   Read an RLE pattern into given life algorithm implementation.
 */
const char *readrle(lifealgo &imp, char *line, size_t nwsindex) {
   int n=0, x=0, y=0 ;
   char *p ;
   char *ruleptr;
   const char *errmsg;
   int wd=0, ht=0, xoff=0, yoff=0;
   bigint gen = bigint::zero;
   bool sawpos = false;             // xoff and yoff set in ParseXRLELine?
   bool sawrule = false;            // saw explicit rule?
   char *buffer = line;             // save pointer to line buffer

   // skip any whitespace at the beginning of the line
   line += nwsindex;

   // parse any #CXRLE line(s) at start
   while (strncmp(line, "#CXRLE", 6) == 0) {
      ParseXRLELine(line, &xoff, &yoff, &sawpos, gen);
      imp.setGeneration(gen);
      line = buffer;   // reset line to the start of the buffer
      if (getline(line, LINESIZE) == NULL) return 0;

      // skip any whitespace at the beginning of the line
      nwsindex = strspn(line, " \t");
      line += nwsindex;
   }

   do {
      // skip whitepsace at the beginning of the line
      nwsindex = strspn(line, " \t");
      line += nwsindex;

      if (line[0] == '#') {
         // only attempt to decode rule if rule not already seen
         if (line[1] == 'r' && !sawrule) {
            ruleptr = line;
            ruleptr += 2;
            while (*ruleptr && *ruleptr <= ' ') ruleptr++;
            p = ruleptr;
            while (*p > ' ') p++;
            *p = 0;
            errmsg = imp.setrule(ruleptr);
            if (errmsg) return errmsg;
            sawrule = true;
         }
         // there's a slight ambiguity here for extended RLE when a line
         // starts with 'x'; we only treat it as a dimension line if the
         // next char is whitespace or '=', since 'x' will only otherwise
         // occur as a two-char token followed by an upper case alphabetic.
      } else if (line[0] == 'x' && (line[1] <= ' ' || line[1] == '=') && !sawrule) {
         // extract wd and ht
         p = line;
         while (*p && *p != '=') p++;
         p++;
         sscanf(p, "%d", &wd);
         while (*p && *p != '=') p++;
         p++;
         sscanf(p, "%d", &ht);

         while (*p && *p != 'r') p++;
         if (strncmp(p, "rule", 4) == 0) {
            p += 4;
            while (*p && (*p <= ' ' || *p == '=')) p++;
            ruleptr = p;
            while (*p > ' ') p++;
            // remove any comma at end of rule
            if (p[-1] == ',') p--;
            *p = 0;
            errmsg = imp.setrule(ruleptr);
            if (errmsg) return errmsg;
            sawrule = true;
         }

         if (!sawrule) {
            // if no rule given then try Conway's Life; if it fails then
            // return error so Golly will look for matching algo
            errmsg = imp.setrule("B3/S23");
            if (errmsg) return errmsg;
         }

         // imp.setrule() has set imp.gridwd and imp.gridht
         if (!sawpos && (imp.gridwd > 0 || imp.gridht > 0)) {
            if (wd > 0 && (wd <= (int)imp.gridwd || imp.gridwd == 0) &&
                ht > 0 && (ht <= (int)imp.gridht || imp.gridht == 0)) {
                // pattern size is known and fits within the bounded grid
                // so position pattern in the middle of the grid
                xoff = -int(wd / 2);
                yoff = -int(ht / 2);
            } else {
                // position pattern at top left corner of bounded grid
                // to try and ensure the entire pattern will fit
                xoff = -int(imp.gridwd / 2);
                yoff = -int(imp.gridht / 2);
            }
         }

         if (getedges) {
            top = yoff;
            left = xoff;
            bottom = yoff + ht - 1;
            right = xoff + wd - 1;
         }
      } else {
         int gwd = (int)imp.gridwd;
         int ght = (int)imp.gridht;
         for (p=line; *p; p++) {
            char c = *p;
            // check for whitespace
            if (c == ' ' || c == '\t') {
               // ignore whitespace unless during or after count definition
               if (n != 0) {
                  return "Illegal whitespace after count";
               }
            } else {
               if ('0' <= c && c <= '9') {
                  if (c == '0' && n == 0) {
                     return "Leading zero in count";
                  }
                  n = n * 10 + c - '0';
               } else {
                  if (n == 0)
                     n = 1;
                  if (c == 'b' || c == '.') {
                     x += n;
                  } else if (c == '$') {
                     x = 0;
                     y += n;
                  } else if (c == '!') {
                     return 0;
                  } else if (('o' <= c && c <= 'y') || ('A' <= c && c <= 'X')) {
                     int state = -1;
                     if (c == 'o')
                        state = 1;
                     else if (c < 'o') {
                        state = c - 'A' + 1;
                     } else {
                        state = 24 * (c - 'p' + 1);
                        p++;
                        c = *p;
                        if ('A' <= c && c <= 'X') {
                           state = state + c - 'A' + 1;
                        } else {
                           // return "Illegal multi-char state";
                           // be more forgiving so we can read non-standard rle files like
                           // those at http://home.interserv.com/~mniemiec/lifepage.htm
                           state = 1;
                           p--;
                        }
                     }
                     // write run of cells to grid checking cells are within any bounded grid
                     if (ght == 0 || y < ght) {
                        while (n-- > 0) {
                           if (gwd == 0 || x < gwd) {
                              if (imp.setcell(xoff + x, yoff + y, state) < 0)
                                 return "Cell state out of range for this algorithm";
                           }
                           x++;
                        }
                     }
                  }
                  n = 0;
               }
            }
         }

         // check if count still in progress when end of line reached
         if (n > 0) {
            return "Illegal whitespace after count";
         }
      }

      line = buffer;   // reset line to the start of the buffer
   } while (getline(line, LINESIZE));

   return 0;
}

/*
 *   This ugly bit of code will go undocumented.  It reads Alan Hensel's
 *   PC Life format, either 1.05 or 1.06.
 */
const char *readpclife(lifealgo &imp, char *line) {
   int x=0, y=0 ;
   int leftx = x ;
   char *p ;
   char *ruleptr;
   const char *errmsg;
   bool sawrule = false;            // saw explicit rule?

   do {
      if (line[0] == '#') {
         if (line[1] == 'P') {
            if (!sawrule) {
               // if no rule given then try Conway's Life; if it fails then
               // return error so Golly will look for matching algo
               errmsg = imp.setrule("B3/S23");
               if (errmsg) return errmsg;
               sawrule = true;      // in case there are many #P lines
            }
            sscanf(line + 2, " %d %d", &x, &y) ;
            leftx = x ;
         } else if (line[1] == 'N') {
            errmsg = imp.setrule("B3/S23");
            if (errmsg) return errmsg;
            sawrule = true;
         } else if (line[1] == 'R') {
            ruleptr = line;
            ruleptr += 2;
            while (*ruleptr && *ruleptr <= ' ') ruleptr++;
            p = ruleptr;
            while (*p > ' ') p++;
            *p = 0;
            errmsg = imp.setrule(ruleptr);
            if (errmsg) return errmsg;
            sawrule = true;
         }
      } else if (line[0] == '-' || ('0' <= line[0] && line[0] <= '9')) {
         sscanf(line, "%d %d", &x, &y) ;
         if (imp.setcell(x, y, 1) < 0)
            return SETCELLERROR ;
      } else if (line[0] == '.' || line[0] == '*') {
         for (p = line; *p; p++) {
            if (*p == '*') {
               if (imp.setcell(x, y, 1) < 0)
                  return SETCELLERROR ;
            }
            x++ ;
         }
         x = leftx ;
         y++ ;
      }
   } while (getline(line, LINESIZE));

   return 0;
}

/*
 *   This routine reads David Bell's dblife format.
 */
const char *readdblife(lifealgo &imp, char *line) {
   int n=0, x=0, y=0;
   char *p;

   while (getline(line, LINESIZE)) {
      if (line[0] != '!') {
         // parse line like "23.O15.3O15.3O15.O4.4O"
         n = x = 0;
         for (p=line; *p; p++) {
            if ('0' <= *p && *p <= '9') {
               n = n * 10 + *p - '0';
            } else {
               if (n == 0) n = 1;
               if (*p == '.') {
                  x += n;
               } else if (*p == 'O') {
                  while (n-- > 0)
                     if (imp.setcell(x++, y, 1) < 0)
                        return SETCELLERROR ;
               } else {
                  // ignore dblife commands like "5k10h@"
               }
               n = 0;
            }
         }
         y++;
      }
   }
   return 0;
}

//
// Read Mirek Wojtowicz's MCell format.
// See http://psoup.math.wisc.edu/mcell/ca_files_formats.html for details.
//
const char *readmcell(lifealgo &imp, char *line) {
   int x = 0, y = 0;
   int wd = 0, ht = 0;              // bounded if > 0
   int wrapped = 0;                 // plane if 0, torus if 1
   char *p;
   const char *errmsg;
   bool sawrule = false;            // saw explicit rule?
   bool extendedHL = false;         // special-case rule translation for extended HistoricalLife rules
   bool useltl = false;             // using a Larger than Life rule?
   char ltlrule[MAXRULESIZE];       // the Larger than Life rule (without any suffix)
   int defwd = 0, defht = 0;        // default grid size for Larger than Life
   int Lcount = 0;                  // number of #L lines seen

   while (getline(line, LINESIZE)) {
      if (line[0] == '#') {
         if (line[1] == 'L' && line[2] == ' ') {
            if (!sawrule) {
               // if no rule given then try Conway's Life; if it fails then
               // return error so Golly will look for matching algo
               errmsg = imp.setrule("B3/S23");
               if (errmsg) return errmsg;
               sawrule = true;
            }

            Lcount++;
            if (Lcount == 1 && useltl) {
                // call setrule again with correct width and height so we don't need
                // yet another setrule call at the end
                char rule[MAXRULESIZE+32];
                if (wd == 0 && ht == 0) {
                    // no #BOARD line was seen so use default size saved earlier
                    wd = defwd;
                    ht = defht;
                }
                if (wrapped)
                    sprintf(rule, "%s:T%d,%d", ltlrule, wd, ht);
                else
                    sprintf(rule, "%s:P%d,%d", ltlrule, wd, ht);
                errmsg = imp.setrule(rule);
                if (errmsg) return errmsg;
            }

            int n = 0;
            for (p=line+3; *p; p++) {
               char c = *p;
               if ('0' <= c && c <= '9') {
                  n = n * 10 + c - '0';
               } else if (c > ' ') {
                  if (n == 0)
                     n = 1;
                  if (c == '.') {
                     x += n;
                  } else if (c == '$') {
                     x = -(wd/2);
                     y += n;
                  } else {
                     int state = 0;
                     if ('a' <= c && c <= 'j') {
                        state = 24 * (c - 'a' + 1);
                        p++;
                        c = *p;
                     }
                     if ('A' <= c && c <= 'X') {
                        state = state + c - 'A' + 1;
                        if (extendedHL) {
                           // adjust marked states for LifeHistory
                           if (state == 8) state = 4;
                           else if (state == 3) state = 5;
                           else if (state == 5) state = 3;
                        }
                     } else {
                        return "Illegal multi-char state";
                     }
                     while (n-- > 0) {
                        if (imp.setcell(x, y, state) < 0) {
                           // instead of returning an error message like "Cell state out of range"
                           // it's nicer to convert the state to 1 (this is what MCell seems to do
                           // for patterns like Bug1.mcl in its LtL collection)
                           imp.setcell(x, y, 1);
                        }
                        x++;
                     }
                  }
                  n = 0;
               }
            }

         // look for Larger than Life
         } else if (strncmp(line, "#GAME Larger than Life", 22) == 0) {
            useltl = true;

         // look for bounded universe
         } else if (strncmp(line, "#BOARD ", 7) == 0) {
            sscanf(line + 7, "%dx%d", &wd, &ht);
            // write pattern in top left corner initially
            // (pattern will be shifted to middle of grid below)
            x = -(wd/2);
            y = -(ht/2);

         // look for topology
         } else if (strncmp(line, "#WRAP ", 6) == 0) {
            sscanf(line + 6, "%d", &wrapped);

         // we allow lines like "#GOLLY WireWorld"
         } else if (!sawrule && (strncmp(line, "#GOLLY", 6) == 0 ||
                                 strncmp(line, "#RULE", 5) == 0) ) {
             if (strncmp(line, "#RULE 1,0,1,0,0,0,1,0,0,0,0,0,0,2,2,1,1,2,2,2,2,2,0,2,2,2,1,2,2,2,2,2", 69) == 0) {
                // standard HistoricalLife rule -- all states transfer directly to LifeHistory
                if (strncmp(line, "#RULE 1,0,1,0,0,0,1,0,0,0,0,0,0,2,2,1,1,2,2,2,2,2,0,2,2,2,1,2,2,2,2,2,", 70) == 0) {
                   // special case:  Brice Due's extended HistoricalLife rules have
                   // non-contiguous states (State 8 but no State 4, 6, or 7)
                   // that need translating to work in LifeHistory)
                   extendedHL = true;
                }
                errmsg = imp.setrule("LifeHistory");
                if (errmsg) return errmsg;
                sawrule = true;
             } else if (strncmp(line, "#RULE 1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,1", 40) == 0) {
                errmsg = imp.setrule("B3/S23");
                if (errmsg) errmsg = imp.setrule("Life");
                if (errmsg) return errmsg;
                sawrule = true;
             } else {
               char *ruleptr = line;
               // skip "#GOLLY" or "#RULE"
               ruleptr += line[1] == 'G' ? 6 : 5;
               while (*ruleptr && *ruleptr <= ' ') ruleptr++;
               p = ruleptr;
               while (*p > ' ') p++;
               *p = 0;
               errmsg = imp.setrule(ruleptr);
               if (errmsg) return errmsg;
               if (useltl) {
                  // save suffix-less rule string for later use when we see first #L line
                  sprintf(ltlrule, "%s", ruleptr);
                  // also save default grid size in case there is no #BOARD line
                  defwd = imp.gridwd;
                  defht = imp.gridht;
               }
               sawrule = true;
            }
         }
      }
   }

   if (wd > 0 || ht > 0) {
      if (useltl) {
         // setrule has already been called above
      } else {
         // grid is bounded, so append suitable suffix to rule
         char rule[MAXRULESIZE];
         if (wrapped)
            sprintf(rule, "%s:T%d,%d", imp.getrule(), wd, ht);
         else
            sprintf(rule, "%s:P%d,%d", imp.getrule(), wd, ht);
         errmsg = imp.setrule(rule);
         if (errmsg) {
            // should never happen
            lifewarning("Bug in readmcell code!");
            return errmsg;
         }
      }
      // shift pattern to middle of bounded grid
      imp.endofpattern();
      if (!imp.isEmpty()) {
         imp.findedges(&top, &left, &bottom, &right);
         // pattern is currently in top left corner so shift down and right
         // (note that we add 1 to wd and ht to get same position as MCell)
         int shiftx = (wd + 1 - (right.toint() - left.toint() + 1)) / 2;
         int shifty = (ht + 1 - (bottom.toint() - top.toint() + 1)) / 2;
         if (shiftx > 0 || shifty > 0) {
            for (y = bottom.toint(); y >= top.toint(); y--) {
               for (x = right.toint(); x >= left.toint(); x--) {
                  int state = imp.getcell(x, y);
                  if (state > 0) {
                     imp.setcell(x, y, 0);
                     imp.setcell(x+shiftx, y+shifty, state);
                  }
               }
            }
         }
      }
   }

   return 0;
}

long getfilesize(const char *filename) {
   long flen = 0;
   FILE *f = fopen(filename, "r");
   if (f != 0) {
      fseek(f, 0L, SEEK_END);
      flen = ftell(f);
      fclose(f);
   }
   return flen;
}

// This function guesses whether `line' is the start of a headerless Life RLE
// pattern.  It is used to distinguish headerless RLE from plain text patterns.
static bool isplainrle(const char *line) {

   // Find end of line, or terminating '!' character, whichever comes first:
   const char *end = line;
   while (*end && *end != '!') ++end;

   // Verify that '!' (if present) is the final printable character:
   if (*end == '!') {
      for (const char *p = end + 1; *p; ++p) {
         if ((unsigned)*p > ' ') {
            return false;
         }
      }
   }

   // Ensure line consists of valid tokens:
   bool prev_digit = false, have_digit = false;
   for (const char *p = line; p != end; ++p) {
      if ((unsigned)*p <= ' ') {
         if (prev_digit) return false;  // space inside token!
      } else if (*p >= '0' && *p <= '9') {
         prev_digit = have_digit = true;
      } else if (*p == 'b' || *p == 'o' || *p == '$') {
         prev_digit = false;
      } else {
         return false;  // unsupported printable character encountered!
      }
   }
   if (prev_digit) return false;  // end of line inside token!

   // Everything seems parseable; assume this is RLE if either we saw some
   // digits, or the pattern ends with a '!', both of which are unlikely to
   // occur in plain text patterns:
   return have_digit || *end == '!';
}

static void LookForLocalRule(const char *filename, lifealgo &imp) {
    // if filename ends with ".gz" then the following fopen fails
    // (which is good, because we don't want to support compressed files)
    FILE* rlefile = fopen(filename, "rt");
    if (rlefile == 0) return;
    
    #ifdef WIN32
        // on Windows we need to reopen the file in binary mode so ftell
        // works correctly if the file doesn't end lines with CR+LF
        // (which is the case when clipboard data is saved in a temporary file)
        fclose(rlefile);
        rlefile = fopen(filename, "rb");
        if (rlefile == 0) return; // should never happen but play safe
    #endif
    
    // look in given RLE file for a line starting with @RULE name
    char linebuf[LINESIZE + 1];
    linereader reader(rlefile);
    while (reader.fgets(linebuf, LINESIZE)) {
        if (strncmp(linebuf, "@RULE ", 6) == 0) {            
            imp.local_rule = linebuf+6;            
            imp.local_file = filename;
            imp.RULE_offset = ftell(rlefile) - strlen(linebuf) - 1;
            break;
        }
    }
    reader.close(); // closes rlefile
}

const char *loadpattern(const char *filename, lifealgo &imp) {
   char line[LINESIZE + 1] ;
   const char *errmsg = 0;
   size_t nwsindex = 0;   // index of first non-whitespace character in line

   // these only change if an RLE file contains @RULE data
   imp.RULE_offset = 0L;
   imp.local_file.clear();
   imp.local_rule.clear();

   // set rule to Conway's Life (default if explicit rule isn't supplied,
   // such as a text pattern like "...ooo$$$ooo")
   const char *err = imp.setrule("B3/S23") ;
   if (err) {
      // try "Life" in case given algo is RuleLoader and a
      // Life.rule file exists (nicer for loading lexicon patterns)
      err = imp.setrule("Life");
      if (err) {
         // if given algo doesn't support B3/S23 or Life then the only sensible
         // choice left is to use the algo's default rule
         imp.setrule( imp.DefaultRule() ) ;
      }
   }

   if (getedges)
      lifebeginprogress("Reading from clipboard");
   else
      lifebeginprogress("Reading pattern file");

   // skip any blank or whitespace only lines at start to avoid problem
   // when copying pattern from Internet Explorer or forums
   while (getline(line, LINESIZE) && line[strspn(line, " \t")] == 0);

   // get the index of the first non-whitespace character
   nwsindex = strspn(line, " \t");

   // test for 'i' to cater for #LLAB comment in LifeLab file
   if (line[0] == '#' && line[1] == 'L' && line[2] == 'i') {
      errmsg = readpclife(imp, line) ;
      if (errmsg == 0) {
         imp.endofpattern() ;
         if (getedges && !imp.isEmpty()) {
            imp.findedges(&top, &left, &bottom, &right) ;
         }
      }

   } else if (line[0] == '#' && line[1] == 'P' && line[2] == ' ') {
      // WinLifeSearch creates clipboard patterns similar to
      // Life 1.05 format but without the header line
      errmsg = readpclife(imp, line) ;
      if (errmsg == 0) {
         imp.endofpattern() ;
         if (getedges && !imp.isEmpty()) {
            imp.findedges(&top, &left, &bottom, &right) ;
         }
      }

   } else if (line[0] == '#' && line[1] == 'M' && line[2] == 'C' &&
              line[3] == 'e' && line[4] == 'l' && line[5] == 'l' ) {
      errmsg = readmcell(imp, line) ;
      if (errmsg == 0) {
         imp.endofpattern() ;
         if (getedges && !imp.isEmpty()) {
            imp.findedges(&top, &left, &bottom, &right) ;
         }
      }

   } else if (line[nwsindex] == '#' || line[nwsindex] == 'x') {

      // only look for @RULE data if imp is a RuleLoader algo
      if (imp.isRuleLoader()) LookForLocalRule(filename, imp);

      errmsg = readrle(imp, line, nwsindex);
      if (errmsg == 0) {
         imp.endofpattern() ;
         if (getedges && !imp.isEmpty()) {
            // readrle has set top,left,bottom,right based on the info given in
            // the header line and possibly a "#CXRLE Pos=..." line, but in case
            // that info is incorrect we find the true pattern edges and expand
            // top/left/bottom/right if necessary to avoid truncating the pattern
            bigint t, l, b, r ;
            imp.findedges(&t, &l, &b, &r) ;
            if (t < top) top = t ;
            if (l < left) left = l ;
            if (b > bottom) bottom = b ;
            if (r > right) right = r ;
         }
      }

   } else if (line[0] == '!') {
      errmsg = readdblife(imp, line) ;
      if (errmsg == 0) {
         imp.endofpattern() ;
         if (getedges && !imp.isEmpty()) {
            imp.findedges(&top, &left, &bottom, &right) ;
         }
      }

   } else if (line[0] == '[') {
      errmsg = imp.readmacrocell(line) ;
      if (errmsg == 0) {
         imp.endofpattern() ;
         if (getedges && !imp.isEmpty()) {
            imp.findedges(&top, &left, &bottom, &right) ;
         }
      }

   } else if (isplainrle(line + nwsindex)) {
      errmsg = readrle(imp, line, nwsindex);
      if (errmsg == 0) {
         imp.endofpattern() ;
         if (getedges && !imp.isEmpty()) {
            imp.findedges(&top, &left, &bottom, &right) ;

            // for headerless rle set the top and left to 0 to allow
            // initial blank rows and/or columns when pasting
            top = bigint::zero;
            left = bigint::zero;
         }
      }

   } else {
      // read a text pattern like "...ooo$$$ooo"
      errmsg = readtextpattern(imp, line) ;
      if (errmsg == 0) {
         imp.endofpattern() ;
         // if getedges is true then readtextpattern has set top,left,bottom,right
      }
   }

   lifeendprogress();
   return errmsg ;
}

const char *build_err_str(const char *filename) {
   static char file_err_str[2048];
   sprintf(file_err_str, "Can't open pattern file:\n%s", filename);
   return file_err_str;
}

const char *readpattern(const char *filename, lifealgo &imp) {
   filesize = getfilesize(filename);
#ifdef ZLIB
   zinstream = gzopen(filename, "rb") ;      // rb needed on Windows
   if (zinstream == 0)
      return build_err_str(filename) ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return build_err_str(filename) ;
#endif
   buffpos = BUFFSIZE;                       // for 1st getchar call
   prevchar = 0;                             // for 1st getline call
   const char *errmsg = loadpattern(filename, imp) ;
#ifdef ZLIB
   gzclose(zinstream) ;
#else
   fclose(pattfile) ;
#endif
   return errmsg ;
}

const char *readclipboard(const char *filename, lifealgo &imp,
                          bigint *t, bigint *l, bigint *b, bigint *r) {
   filesize = getfilesize(filename);
#ifdef ZLIB
   zinstream = gzopen(filename, "rb") ;      // rb needed on Windows
   if (zinstream == 0)
      return "Can't open clipboard file!" ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return "Can't open clipboard file!" ;
#endif
   buffpos = BUFFSIZE;                       // for 1st getchar call
   prevchar = 0;                             // for 1st getline call

   top = 0;
   left = 0;
   bottom = 0;
   right = 0;
   getedges = true;
   const char *errmsg = loadpattern(filename, imp);
   getedges = false;
   *t = top;
   *l = left;
   *b = bottom;
   *r = right;
   // make sure we return a valid rect
   if (bottom < top) *b = top;
   if (right < left) *r = left;

#ifdef ZLIB
   gzclose(zinstream) ;
#else
   fclose(pattfile) ;
#endif
   return errmsg ;
}

const char *readcomments(const char *filename, char **commptr)
{
   *commptr = (char *)malloc(maxcommlen);
   if (*commptr == NULL) {
      return "Not enough memory for comments!";
   }
   char *cptr = *commptr;
   cptr[0] = 0;                              // safer to init to empty string

   filesize = getfilesize(filename);
#ifdef ZLIB
   zinstream = gzopen(filename, "rb") ;      // rb needed on Windows
   if (zinstream == 0)
      return build_err_str(filename) ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return build_err_str(filename) ;
#endif
   char line[LINESIZE + 1] ;
   buffpos = BUFFSIZE;                       // for 1st getchar call
   prevchar = 0;                             // for 1st getline call
   int commlen = 0;
   size_t nwsindex = 0;                      // index of first non-whitespace character in line

   // loading comments is likely to be quite fast so no real need to
   // display the progress dialog, but getchar calls lifeabortprogress
   // so safer just to assume the progress dialog might appear
   lifebeginprogress("Loading comments");

   // skip any blank lines at start to avoid problem when copying pattern
   // from Internet Explorer
   while (getline(line, LINESIZE) && line[0] == 0) ;

   // get the index of the first non-whitespace character
   nwsindex = strspn(line, " \t");

   // test for 'i' to cater for #LLAB comment in LifeLab file
   if (line[0] == '#' && line[1] == 'L' && line[2] == 'i') {
      // extract comment lines from Life 1.05/1.06 file
      int linecount = 0;
      while (linecount < 10000) {
         linecount++;
         if (line[0] == '#' &&
               !(line[1] == 'P' && line[2] == ' ') &&
               !(line[1] == 'N' && line[2] == 0)) {
            int linelen = (int)strlen(line);
            if (commlen + linelen + 1 > maxcommlen) break;
            strcpy(cptr + commlen, line);
            commlen += linelen;
            cptr[commlen] = '\n';      // getline strips off eol char(s)
            commlen++;
         }
         if (getline(line, LINESIZE) == NULL) break;
      }

   } else if (line[0] == '#' && line[1] == 'M' && line[2] == 'C' &&
              line[3] == 'e' && line[4] == 'l' && line[5] == 'l' ) {
      // extract "#D ..." lines from MCell file
      while (getline(line, LINESIZE)) {
         if (line[0] != '#') break;
         if (line[1] == 'L' && line[2] == ' ') break;
         if (line[1] == 'D' && (line[2] == ' ' || line[2] == 0)) {
            int linelen = (int)strlen(line);
            if (commlen + linelen + 1 > maxcommlen) break;
            strcpy(cptr + commlen, line);
            commlen += linelen;
            cptr[commlen] = '\n';         // getline strips off eol char(s)
            commlen++;
         }
      }

   } else if (line[nwsindex] == '#' || line[nwsindex] == 'x') {
      // extract comment lines from RLE file
      while (line[nwsindex] == '#') {
         int linelen = (int)strlen(line);
         if (commlen + linelen + 1 > maxcommlen) break;
         strcpy(cptr + commlen, line + nwsindex);
         commlen += linelen;
         cptr[commlen] = '\n';         // getline strips off eol char(s)
         commlen++;
         if (getline(line, LINESIZE) == NULL) break;
         nwsindex = strspn(line, " \t");
      }
      // also look for any lines after "!" but only if file is < 1MB
      // (ZLIB doesn't seem to provide a fast way to go to eof)
      if (filesize < 1024*1024) {
         bool foundexcl = false;
         while (getline(line, LINESIZE)) {
            if (strrchr(line, '!')) { foundexcl = true; break; }
         }
         if (foundexcl) {
            while (getline(line, LINESIZE)) {
               int linelen = (int)strlen(line);
               if (commlen + linelen + 1 > maxcommlen) break;
               strcpy(cptr + commlen, line);
               commlen += linelen;
               cptr[commlen] = '\n';      // getline strips off eol char(s)
               commlen++;
            }
         }
      }

   } else if (line[0] == '!') {
      // extract "!..." lines from dblife file
      while (line[0] == '!') {
         int linelen = (int)strlen(line);
         if (commlen + linelen + 1 > maxcommlen) break;
         strcpy(cptr + commlen, line);
         commlen += linelen;
         cptr[commlen] = '\n';            // getline strips off eol char(s)
         commlen++;
         if (getline(line, LINESIZE) == NULL) break;
      }

   } else if (line[0] == '[') {
      // extract "#C..." lines from macrocell file
      while (getline(line, LINESIZE)) {
         if (line[0] != '#') break;
         if (line[1] == 'C') {
            int linelen = (int)strlen(line);
            if (commlen + linelen + 1 > maxcommlen) break;
            strcpy(cptr + commlen, line);
            commlen += linelen;
            cptr[commlen] = '\n';         // getline strips off eol char(s)
            commlen++;
         }
      }

   } else {
      // no comments in text pattern file
   }

   lifeendprogress();
   if (commlen == maxcommlen) commlen--;
   cptr[commlen] = 0;

#ifdef ZLIB
   gzclose(zinstream) ;
#else
   fclose(pattfile) ;
#endif
   return 0 ;
}

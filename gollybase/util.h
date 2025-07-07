// This file is part of Golly.
// See docs/License.html for the copyright notice.

/**
 *   Basic utility classes for things like fatal errors.
 */
#ifndef UTIL_H
#define UTIL_H
#include <cstdio> // for FILE *

void lifefatal(const char *s) ;
void lifewarning(const char *s) ;
void lifestatus(const char *s) ;
void lifebeginprogress(const char *dlgtitle) ;
bool lifeabortprogress(double fracdone, const char *newmsg) ;
void lifeendprogress() ;
const char *lifegetuserrules() ;
const char *lifegetrulesdir() ;
const char *lifegettempdir() ;
bool isaborted() ;
FILE *getdebugfile() ;
/**
 *   Sick of line ending woes.  This class takes care of this for us.
 */
class linereader {
public:
   linereader(FILE *f) ;
   char *fgets(char *buf, int maxlen) ;
   bool truncated ; // true if fgets had to truncate buf
   void setfile(FILE *f) ;
   void setcloseonfree() ;
   int close() ;
   ~linereader() ;
private:
   int lastchar ;
   int closeonfree ;
   FILE *fp ;
} ;
/**
 *   To substitute your own routines, use the following class.
 */
class lifeerrors {
public:
   virtual void fatal(const char *s) = 0 ;
   virtual void warning(const char *s) = 0 ;
   virtual void status(const char *s) = 0 ;
   virtual void beginprogress(const char *dlgtitle) = 0 ;
   virtual bool abortprogress(double fracdone, const char *newmsg) = 0 ;
   virtual void endprogress() = 0 ;
   virtual const char *getuserrules() = 0 ;
   virtual const char *getrulesdir() = 0 ;
   virtual const char *gettempdir() = 0 ;
   static void seterrorhandler(lifeerrors *obj) ;
   bool aborted ;
} ;
/**
 *   If a fast popcount routine is available, this macro indicates its
 *   availability.  The popcount should be a 32-bit popcount.  The
 *   __builtin_popcount by gcc and clang works fine on any platform.
 *   The __popcount intrinsic on Visual Studio does *not* without a
 *   CPUID check, so we don't do fast popcounts yet on Windows.
 */
#ifdef __GNUC__
#define FASTPOPCOUNT __builtin_popcount
#endif
#ifdef __clang__
#define FASTPOPCOUNT __builtin_popcount
#endif
/**
 *   A routine to get the number of seconds elapsed since an arbitrary
 *   point, as a double.
 */
double gollySecondCount() ;
/*
 *   Performance data.  We keep running values here.  We can copy this
 *   to "mark" variables, and then report performance for deltas.
 */
struct hperf {
   void clear() {
      fastNodeInc = 0 ;
      nodesCalculated = 0 ;
      depthSum = 0 ;
      timeStamp = gollySecondCount() ;
      genval = 0 ;
      frames = 0 ;
      halfNodes = 0 ;
   }
   void report(hperf&, int verbose) ;
   void reportStep(hperf&, hperf&, double genval, int verbose) ;
   int fastinc(int depth, int half) {
      depthSum += depth ;
      if (half)
         halfNodes++ ;
      if ((++fastNodeInc & reportMask) == 0)
         return 1 ;
      else
         return 0 ;
   }
   double getReportInterval() {
      return reportInterval ;
   }
   void setReportInterval(double v) {
      reportInterval = v ;
   }
   int fastNodeInc ;
   double frames ;
   double nodesCalculated ;
   double halfNodes ;
   double depthSum ;
   double timeStamp ;
   double genval ;
   static int reportMask ;
   static double reportInterval ;
} ;
#endif

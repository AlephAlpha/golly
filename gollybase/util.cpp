// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "util.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/**
 *   For now error just uses stderr.
 */
class baselifeerrors : public lifeerrors {
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
      return "" ;
   }
   virtual const char *gettempdir() {
      return "" ;
   }
} ;

baselifeerrors baselifeerrors ;
lifeerrors *errorhandler = &baselifeerrors ;

void lifeerrors::seterrorhandler(lifeerrors *o) {
  if (o == 0)
    errorhandler = &baselifeerrors ;
  else
    errorhandler = o ;
}

void lifefatal(const char *s) {
   errorhandler->fatal(s) ;
}

void lifewarning(const char *s) {
   errorhandler->warning(s) ;
}

void lifestatus(const char *s) {
   errorhandler->status(s) ;
}

void lifebeginprogress(const char *dlgtitle) {
   errorhandler->beginprogress(dlgtitle) ;
}

bool lifeabortprogress(double fracdone, const char *newmsg) {
   return errorhandler->aborted |=
      errorhandler->abortprogress(fracdone, newmsg) ;
}

bool isaborted() {
   return errorhandler->aborted ;
}

void lifeendprogress() {
   errorhandler->endprogress() ;
}

const char *lifegetuserrules() {
   return errorhandler->getuserrules() ;
}

const char *lifegetrulesdir() {
   return errorhandler->getrulesdir() ;
}

const char *lifegettempdir() {
   return errorhandler->gettempdir() ;
}

static FILE *f ;
FILE *getdebugfile() {
  if (f == 0)
    f = fopen("trace.txt", "w") ;
  return f ;
}
/**
 *   Manage reading lines from a FILE* without worrying about
 *   line terminates.  Note that the fgets() routine does not
 *   insert any line termination characters at all.
 */
linereader::linereader(FILE *f) {
   setfile(f) ;
}
void linereader::setfile(FILE *f) {
   fp = f ;
   lastchar = 0 ;
   closeonfree = 0 ;    // AKT: avoid crash on Linux
}
void linereader::setcloseonfree() {
   closeonfree = 1 ;
}
int linereader::close() {
   if (fp) {
      int r = fclose(fp) ;
      fp = 0 ;
      return r ;
   }
   return 0 ;
}
linereader::~linereader() {
   if (closeonfree)
      close() ;
}
const int LF = 10 ;
const int CR = 13 ;
char *linereader::fgets(char *buf, int maxlen) {
   truncated = false ;
   int i = 0 ;
   for (;;) {
      if (i+1 >= maxlen) {
         truncated = true ;
         buf[i] = 0 ;
         return buf ;
      }
      int c = getc(fp) ;
      switch (c) {
      case EOF:
         if (i == 0)
            return 0 ;
         buf[i] = 0 ;
         return buf ;
      case LF:
         if (lastchar != CR) {
            lastchar = LF ;
            buf[i] = 0 ;
            return buf ;
         }
         break ;
      case CR:
         lastchar = CR ;
         buf[i] = 0 ;
         return buf ;
      default:
         lastchar = c ;
         buf[i++] = (char)c ;
         break ;
      }
   }
}

#ifdef _WIN32
static double freq = 0.0;
double gollySecondCount() {
   LARGE_INTEGER now;
   if (freq == 0.0) {
      LARGE_INTEGER f;
      QueryPerformanceFrequency(&f);
      freq = (double)f.QuadPart;
      if (freq <= 0.0) freq = 1.0;	// play safe and avoid div by 0
   }
   QueryPerformanceCounter(&now);
   return (now.QuadPart) / freq;
}
#else
double gollySecondCount() {
   struct timeval tv ;
   gettimeofday(&tv, 0) ;
   return tv.tv_sec + 0.000001 * tv.tv_usec ;
}
#endif

/*
 *   Reporting.
 *   The node count listed here wants to be big to reduce the number
 *   of "get times" we do (which can be fairly expensive on some systems),
 *   but it wants to be small in case something is going wrong like swapping
 *   and we want to tell the user that the node rate has dropped.
 *   A value of 65K is a reasonable compromise.
 */
int hperf::reportMask = ((1<<16)-1) ; // node count between checks
/*
 *   How frequently do we update the status bar?  Every two seconds
 *   should be reasonable.  If we set this to zero, then that disables
 *   performance reporting.
 */
double hperf::reportInterval = 2 ; // time between reports
/*
 *   Static buffer for status updates.
 */
char perfstatusline[200] ;
void hperf::report(hperf &mark, int verbose) {
   double ts = gollySecondCount() ;
   double elapsed = ts - mark.timeStamp ;
   if (reportInterval == 0 || elapsed < reportInterval)
      return ;
   timeStamp = ts ;
   nodesCalculated += fastNodeInc ;
   fastNodeInc = 0 ;
   if (verbose) {
      double nodeCount = nodesCalculated - mark.nodesCalculated ;
      double halfFrac = 0 ;
      if (nodeCount > 0)
         halfFrac = (halfNodes - mark.halfNodes) / nodeCount ;
      double depthDelta = depthSum - mark.depthSum ;
      sprintf(perfstatusline, "RATE noderate %g depth %g half %g",
                       nodeCount/elapsed, 1+depthDelta/nodeCount, halfFrac) ;
      lifestatus(perfstatusline) ;
   }
   mark = *this ;
}
void hperf::reportStep(hperf &mark, hperf &ratemark, double newGen, int verbose) {
   nodesCalculated += fastNodeInc ;
   fastNodeInc = 0 ;
   frames++ ;
   timeStamp = gollySecondCount() ;
   double elapsed = timeStamp - mark.timeStamp ;
   if (reportInterval == 0 || elapsed < reportInterval)
      return ;
   if (verbose) {
      double inc = newGen - mark.genval ;
      if (inc == 0)
         inc = 1e30 ;
      double nodeCount = nodesCalculated - mark.nodesCalculated ;
      double halfFrac = 0 ;
      if (nodeCount > 0)
         halfFrac = (halfNodes - mark.halfNodes) / nodeCount ;
      double depthDelta = depthSum - mark.depthSum ;
      double genspersec = inc / elapsed ;
      double nodespergen = nodeCount / inc ;
      double fps = (frames - mark.frames) / elapsed ;
      sprintf(perfstatusline,
          "PERF gps %g nps %g fps %g depth %g half %g npg %g nodes %g",
          genspersec, nodeCount/elapsed, fps, 1+depthDelta/nodeCount, halfFrac,
          nodespergen, nodeCount) ;
      lifestatus(perfstatusline) ;
   }
   genval = newGen ;
   mark = *this ;
   ratemark = *this ;
}

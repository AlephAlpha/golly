// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include <sys/time.h>   // for gettimeofday
#include <algorithm>    // for std::transform
#include <ctype.h>      // for tolower
#include <random>

#include "lifepoll.h"   // for lifepoll
#include "util.h"       // for linereader

#include "prefs.h"      // for allowbeep, tempdir
#include "utils.h"

#ifdef ANDROID_GUI
    #include "jnicalls.h"    // for AndroidWarning, AndroidBeep, UpdateStatus, etc
#endif

#ifdef WEB_GUI
    #include "webcalls.h"   // for WebWarning, WebBeep, UpdateStatus, etc
#endif

#ifdef IOS_GUI
    #import <AudioToolbox/AudioToolbox.h>   // for AudioServicesPlaySystemSound, etc
    #import "PatternViewController.h"       // for UpdateStatus, etc
#endif

// -----------------------------------------------------------------------------

int event_checker = 0;      // if > 0 then we're in gollypoller.checkevents()

// -----------------------------------------------------------------------------

void SetColor(gColor& color, unsigned char red, unsigned char green, unsigned char blue)
{
    color.r = red;
    color.g = green;
    color.b = blue;
}

// -----------------------------------------------------------------------------

void SetRect(gRect& rect, int x, int y, int width, int height)
{
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
}

// -----------------------------------------------------------------------------

#ifdef IOS_GUI

// need the following to make YesNo/Warning dialogs modal:

@interface ModalAlertDelegate : NSObject <UIAlertViewDelegate>
{
    NSInteger returnButt;
}

@property () NSInteger returnButt;

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex;

@end

@implementation ModalAlertDelegate

@synthesize returnButt;

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
    returnButt = buttonIndex;
}

@end

#endif // IOS_GUI

// -----------------------------------------------------------------------------

bool YesNo(const char* msg)
{
    Beep();

#ifdef ANDROID_GUI
    return AndroidYesNo(msg);
#endif

#ifdef WEB_GUI
    return WebYesNo(msg);
#endif

#ifdef IOS_GUI
    ModalAlertDelegate *md = [[ModalAlertDelegate alloc] init];
    md.returnButt = -1;

    UIAlertView *a = [[UIAlertView alloc] initWithTitle:@"Warning"
                                                message:[NSString stringWithCString:msg encoding:NSUTF8StringEncoding]
                                               delegate:md
                                      cancelButtonTitle:@"No"
                                      otherButtonTitles:@"Yes", nil];
    [a show];

    // wait for user to hit button
    // (this actually works in the few places it is called)
    while (md.returnButt == -1) {
        event_checker++;
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        event_checker--;
    }

    return md.returnButt != 0;
#endif // IOS_GUI
}

// -----------------------------------------------------------------------------

void Warning(const char* msg)
{
    Beep();

#ifdef ANDROID_GUI
    AndroidWarning(msg);
#endif

#ifdef WEB_GUI
    WebWarning(msg);
#endif

#ifdef IOS_GUI
    ModalAlertDelegate *md = [[ModalAlertDelegate alloc] init];
    md.returnButt = -1;

    UIAlertView *a = [[UIAlertView alloc] initWithTitle:@"Warning"
                                                message:[NSString stringWithCString:msg encoding:NSUTF8StringEncoding]
                                               delegate:md
                                      cancelButtonTitle:@"OK"
                                      otherButtonTitles:nil];
    [a show];

    /* this will usually cause the app to freeze so we don't do it!
    // wait for user to hit button
    while (md.returnButt == -1) {
        event_checker++;
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        event_checker--;
    }
    */
#endif // IOS_GUI
}

// -----------------------------------------------------------------------------

void Fatal(const char* msg)
{
    Beep();

#ifdef ANDROID_GUI
    AndroidFatal(msg);
#endif

#ifdef WEB_GUI
    WebFatal(msg);
#endif

#ifdef IOS_GUI
    NSLog(@"Fatal error: %s", msg);
    // no point displaying an alert dialog (it won't appear)
    exit(1);
#endif // IOS_GUI
}

// -----------------------------------------------------------------------------

void Beep()
{
    if (!allowbeep) return;

#ifdef ANDROID_GUI
    AndroidBeep();
#endif

#ifdef WEB_GUI
    WebBeep();
#endif

#ifdef IOS_GUI
    static SystemSoundID beepID = 0;
    if (beepID == 0) {
        // get the path to the sound file
        NSString* path = [[NSBundle mainBundle] pathForResource:@"beep" ofType:@"aiff"];
        if (path) {
            NSURL* url = [NSURL fileURLWithPath:path];
            OSStatus err = AudioServicesCreateSystemSoundID((__bridge CFURLRef)url, &beepID);
            if (err == kAudioServicesNoError && beepID > 0) {
                // play the sound
                AudioServicesPlaySystemSound(beepID);
            }
        }
    } else {
        // assume we got the sound
        AudioServicesPlaySystemSound(beepID);
    }
#endif // IOS_GUI
}

// -----------------------------------------------------------------------------

double TimeInSeconds()
{
    struct timeval trec;
    gettimeofday(&trec, 0);
    return double(trec.tv_sec) + double(trec.tv_usec) / 1.0e6;
}

// -----------------------------------------------------------------------------

std::string CreateTempFileName(const char* prefix)
{
    /*
    std::string tmplate = tempdir;
    tmplate += prefix;
    tmplate += ".XXXXXX";
    std::string path = mktemp((char*)tmplate.c_str());
    */

    // simpler to ignore prefix and create /tmp/0, /tmp/1, /tmp/2, etc
    char n[32];
    static int nextname = 0;
    sprintf(n, "%d", nextname++);
    std::string path = tempdir + n;

    return path;
}

// -----------------------------------------------------------------------------

bool FileExists(const std::string& filepath)
{
    FILE* f = fopen(filepath.c_str(), "r");
    if (f) {
        fclose(f);
        return true;
    } else {
        return false;
    }
}

// -----------------------------------------------------------------------------

void RemoveFile(const std::string& filepath)
{
#ifdef ANDROID_GUI
    AndroidRemoveFile(filepath);
#endif

#ifdef WEB_GUI
    WebRemoveFile(filepath);
#endif

#ifdef IOS_GUI
    if ([[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithCString:filepath.c_str() encoding:NSUTF8StringEncoding]
                                                   error:NULL] == NO) {
        // should never happen
        Warning("RemoveFile failed!");
    };
#endif
}

// -----------------------------------------------------------------------------

bool CopyFile(const std::string& inpath, const std::string& outpath)
{
#if defined(ANDROID_GUI) || defined(WEB_GUI)
    FILE* infile = fopen(inpath.c_str(), "r");
    if (infile) {
        // read entire file into contents
        std::string contents;
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(infile);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            contents += linebuf;
            contents += "\n";
        }
        reader.close();
        // fclose(infile) has been called

        // write contents to outpath
        FILE* outfile = fopen(outpath.c_str(), "w");
        if (outfile) {
            if (fputs(contents.c_str(), outfile) == EOF) {
                fclose(outfile);
                Warning("CopyFile failed to copy contents to output file!");
                return false;
            }
            fclose(outfile);
        } else {
            Warning("CopyFile failed to open output file!");
            return false;
        }

        return true;
    } else {
        Warning("CopyFile failed to open input file!");
        return false;
    }
#endif // ANDROID_GUI or WEB_GUI

#ifdef IOS_GUI
    if (FileExists(outpath)) {
        RemoveFile(outpath);
    }
    return [[NSFileManager defaultManager] copyItemAtPath:[NSString stringWithCString:inpath.c_str() encoding:NSUTF8StringEncoding]
                                                   toPath:[NSString stringWithCString:outpath.c_str() encoding:NSUTF8StringEncoding]
                                                    error:NULL];
#endif // IOS_GUI
}

// -----------------------------------------------------------------------------

bool MoveFile(const std::string& inpath, const std::string& outpath)
{
#ifdef ANDROID_GUI
    return AndroidMoveFile(inpath, outpath);
#endif

#ifdef WEB_GUI
    return WebMoveFile(inpath, outpath);
#endif

#ifdef IOS_GUI
    if (FileExists(outpath)) {
        RemoveFile(outpath);
    }
    return [[NSFileManager defaultManager] moveItemAtPath:[NSString stringWithCString:inpath.c_str() encoding:NSUTF8StringEncoding]
                                                   toPath:[NSString stringWithCString:outpath.c_str() encoding:NSUTF8StringEncoding]
                                                    error:NULL];
#endif
}

// -----------------------------------------------------------------------------

void FixURLPath(std::string& path)
{
    // replace "%..." with suitable chars for a file path (eg. %20 is changed to space)

#ifdef ANDROID_GUI
    AndroidFixURLPath(path);
#endif

#ifdef WEB_GUI
    WebFixURLPath(path);
#endif

#ifdef IOS_GUI
    NSString* newpath = [[NSString stringWithCString:path.c_str() encoding:NSUTF8StringEncoding]
                         stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    if (newpath) path = [newpath cStringUsingEncoding:NSUTF8StringEncoding];
#endif
}

// -----------------------------------------------------------------------------

bool IsHTMLFile(const std::string& filename)
{
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"htm") == 0 ||
             strcasecmp(ext.c_str(),"html") == 0 );
}

// -----------------------------------------------------------------------------

bool IsTextFile(const std::string& filename)
{
    if (!IsHTMLFile(filename)) {
        // if non-html file name contains "readme" then assume it's a text file
        std::string basename = filename;
        size_t lastsep = basename.rfind('/');
        if (lastsep != std::string::npos) {
            basename = basename.substr(lastsep+1);
        }
        std::transform(basename.begin(), basename.end(), basename.begin(), tolower);
        if (basename.find("readme") != std::string::npos) return true;
    }
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"txt") == 0 ||
             strcasecmp(ext.c_str(),"doc") == 0 );
}

// -----------------------------------------------------------------------------

bool IsZipFile(const std::string& filename)
{
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"zip") == 0 ||
             strcasecmp(ext.c_str(),"gar") == 0 );
}

// -----------------------------------------------------------------------------

bool IsRuleFile(const std::string& filename)
{
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"rule") == 0 );
}

// -----------------------------------------------------------------------------

bool IsScriptFile(const std::string& filename)
{
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"lua") == 0 ||
             strcasecmp(ext.c_str(),"py") == 0 );
}

// -----------------------------------------------------------------------------

bool EndsWith(const std::string& str, const std::string& suffix)
{
    // return true if str ends with suffix
    size_t strlen = str.length();
    size_t sufflen = suffix.length();
    return (strlen >= sufflen) && (str.rfind(suffix) == strlen - sufflen);
}

// -----------------------------------------------------------------------------

// let gollybase modules process events

class golly_poll : public lifepoll
{
public:
    virtual int checkevents();
    virtual void updatePop();
};

int golly_poll::checkevents()
{
    if (event_checker > 0) return isInterrupted();
    event_checker++;

#ifdef ANDROID_GUI
    AndroidCheckEvents();
#endif

#ifdef WEB_GUI
    WebCheckEvents();
#endif

#ifdef IOS_GUI
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate date]];
#endif

    event_checker--;
    return isInterrupted();
}

void golly_poll::updatePop()
{
    UpdateStatus();
}

// -----------------------------------------------------------------------------

golly_poll gollypoller;    // create instance

lifepoll* Poller()
{
    return &gollypoller;
}

void PollerReset()
{
    gollypoller.resetInterrupted();
}

void PollerInterrupt()
{
    gollypoller.setInterrupted();
}

// -----------------------------------------------------------------------------

int golly_rand()
{
   static std::random_device dev;
   static std::mt19937 rng(dev());
   static std::uniform_int_distribution<std::mt19937::result_type>
                                                      dist31(0,0x7fffffff);
   return dist31(rng);
}   

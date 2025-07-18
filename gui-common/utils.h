// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _UTILS_H_
#define _UTILS_H_

#include <string>       // for std::string

class lifepoll;

// Various types and utility routines:

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} gColor;               // a color in RGB space

typedef struct {
    int x;
    int y;
    int width;
    int height;
} gRect;                // a rectangle

void SetColor(gColor& color, unsigned char red, unsigned char green, unsigned char blue);
// Set given gColor to given RGB values.

void SetRect(gRect& rect, int x, int y, int width, int height);
// Set given gRect to given location and size.

void Warning(const char* msg);
// Beep and display message in a modal dialog.

bool YesNo(const char* msg);
// Similar to Warning, but there are 2 buttons: Yes and No.
// Returns true if Yes button is hit.

void Fatal(const char* msg);
// Beep, display message in a modal dialog, then exit app.

void Beep();
// Play beep sound, depending on user setting.

double TimeInSeconds();
// Get time of day, in seconds (accuracy in microsecs).

std::string CreateTempFileName(const char* prefix);
// Return path to a unique temporary file.

bool FileExists(const std::string& filepath);
// Does given file exist?

void RemoveFile(const std::string& filepath);
// Delete given file.

bool CopyFile(const std::string& inpath, const std::string& outpath);
// Return true if input file is successfully copied to output file.
// If the output file existed it is replaced.

bool MoveFile(const std::string& inpath, const std::string& outpath);
// Return true if input file is successfully moved to output file.
// If the output file existed it is replaced.

void FixURLPath(std::string& path);
// Replace "%..." with suitable chars for a file path (eg. %20 is changed to space).

bool IsHTMLFile(const std::string& filename);
// Return true if the given file's extension is .htm or .html
// (ignoring case).

bool IsTextFile(const std::string& filename);
// Return true if the given file's extension is .txt or .doc,
// or if it's not a HTML file and its name contains "readme"
// (ignoring case).

bool IsZipFile(const std::string& filename);
// Return true if the given file's extension is .zip or .gar
// (ignoring case).

bool IsRuleFile(const std::string& filename);
// Return true if the given file's extension is .rule (ignoring case).

bool IsScriptFile(const std::string& filename);
// Return true if the given file is a Lua or Python script.
// It simply checks if the file's extension is .lua or .py
// (ignoring case).

bool EndsWith(const std::string& str, const std::string& suffix);
// Return true if given string ends with given suffix.

lifepoll* Poller();
void PollerReset();
void PollerInterrupt();
extern int event_checker;
// Poller is used by gollybase modules to process events.
// If event_checker > 0 then we've been called from the event checking code.

int golly_rand();
// Return a random int with 31 bits of randomness.

#endif

// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXUTILS_H_
#define _WXUTILS_H_

// Various utility routines:

void Note(const wxString& msg, bool showCancel = true);
// Display given message in a modal dialog.
// If called from a script then an optional Cancel button
// can be used to abort the script.

void Warning(const wxString& msg, bool showCancel = true);
// Beep and display message in a modal dialog.
// If called from a script then an optional Cancel button
// can be used to abort the script.

void Fatal(const wxString& msg);
// Beep, display message in a modal dialog, then exit app.

void Beep();
// Play beep sound, depending on preference setting.

bool GetString(const wxString& title, const wxString& prompt,
               const wxString& instring, wxString& outstring);
// Display a dialog box to get a string from the user.
// Returns false if user hits Cancel button.

bool GetInteger(const wxString& title, const wxString& prompt,
                int inval, int minval, int maxval, int* outval);
// Display a dialog box to get an integer value from the user.
// Returns false if user hits Cancel button.

int SaveChanges(const wxString& query, const wxString& msg);
// Ask user if changes should be saved and return following result:
// 2 if user selects Yes/Save button,
// 1 if user selects No/Don't Save button,
// 0 if user selects Cancel button.

void BeginProgress(const wxString& dlgtitle);
// Call at the start of a lengthy task.  The cursor changes to indicate
// the app is busy but the progress dialog won't appear immediately.

bool AbortProgress(double fraction_done, const wxString& newmsg);
// Call frequently while the task is being carried out.  The progress
// dialog only appears if the task is likely to take more than a few secs.
// Pass in a fraction from 0.0 to 1.0 indicating how much has been done,
// or any negative value to show an indeterminate progress gauge.
// The given string can be used to display extra information.
// The call returns true if the user cancels the progress dialog.

void EndProgress();
// Call when the task has finished (even if it was aborted).

void DrawRect(wxDC& dc, wxRect& rect);
// Draw given rectangle using the current brush and pen.

void FillRect(wxDC& dc, wxRect& rect, wxBrush& brush);
// Fill given rectangle using given brush.

void CreatePaleBitmap(const wxBitmap& inmap, wxBitmap& outmap);
// Create a pale gray version of given bitmap.

bool IsScriptFile(const wxString& filename);
// Return true if the given file is a Lua or Python script.
// It simply checks if the file's extension is .lua or .py
// (ignoring case).

bool IsHTMLFile(const wxString& filename);
// Return true if the given file's extension is .htm or .html
// (ignoring case).

bool IsTextFile(const wxString& filename);
// Return true if the given file's extension is .txt or .doc,
// or if it's not a HTML file and its name contains "readme"
// (ignoring case).

bool IsZipFile(const wxString& filename);
// Return true if the given file's extension is .zip or .gar
// (ignoring case).

bool IsRuleFile(const wxString& filename);
// Return true if the given file's extension is .rule (ignoring case).

int golly_rand();
// Return a random int with 31 bits of randomness.

// Following macro is used to create a wxBitmap from included XPM data:

#define XPM_BITMAP(name) wxBitmap(name##_xpm)

#endif // _WXUTILS_H_

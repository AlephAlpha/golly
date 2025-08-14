// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "wx/file.h"       // for wxFile
#include "wx/filename.h"   // for wxFileName
#include "wx/menuitem.h"   // for SetText
#include "wx/clipbrd.h"    // for wxTheClipboard
#include "wx/dataobj.h"    // for wxTextDataObject
#include "wx/zipstrm.h"    // for wxZipEntry, wxZipInputStream
#include "wx/wfstream.h"   // for wxFFileInputStream

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"   // for readpattern
#include "writepattern.h"  // for writepattern, pattern_format

#include "wxgolly.h"       // for wxGetApp, statusptr, viewptr, bigview
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for SavePrefs, allowundo, userrules, etc
#include "wxrule.h"        // for GetRuleName
#include "wxinfo.h"        // for GetInfoFrame
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxscript.h"      // for RunScript, inscript, scripttitle
#include "wxmain.h"        // for MainFrame, etc
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxalgos.h"       // for CreateNewUniverse, algo_type, algoinfo, etc
#include "wxlayer.h"       // for currlayer, etc
#include "wxoverlay.h"     // for curroverlay
#include "wxhelp.h"        // for ShowHelp, LoadRule
#include "wxtimeline.h"    // for InitTimelineFrame, ToggleTimelineBar, etc

#ifdef __WXMAC__
    // convert path to decomposed UTF8 so fopen will work
    #define FILEPATH path.fn_str()
#else
    #define FILEPATH path.mb_str(wxConvLocal)
#endif

// File menu functions:

// -----------------------------------------------------------------------------

wxString MainFrame::GetBaseName(const wxString& path)
{
    // extract basename from given path
    return path.AfterLast(wxFILE_SEP_PATH);
}

// -----------------------------------------------------------------------------

void MainFrame::SetWindowTitle(const wxString& filename)
{
    if ( !scripttitle.IsEmpty() ) {
        // script has called settitle command
        return;
    }

    if ( !filename.IsEmpty() ) {
        // remember current file name
        currlayer->currname = filename;
        // show currname in current layer's menu item
        UpdateLayerItem(currindex);
    }
    
    if (inscript) {
        // avoid window title flashing; eg. script might be switching layers
        ShowTitleLater();
        return;
    }
    
    wxString prefix = wxEmptyString;
    
    // display asterisk if pattern has been modified
    if (currlayer->dirty) prefix += wxT("*");
    
    int cid = currlayer->cloneid;
    while (cid > 0) {
        // display one or more "=" chars to indicate this is a cloned layer
        prefix += wxT("=");
        cid--;
    }
    
    wxString rule = GetRuleName( wxString(currlayer->algo->getrule(),wxConvLocal) );
    if (rule.Length() > 63) {
        // nicer to mid-truncate a really long rule
        wxString prefix = rule.Left(30);
        wxString suffix = rule.Right(30);
        rule = prefix;
        rule += wxT(" ... ");   // using spaces makes it more obvious than "..." or an ellipsis
        rule += suffix;
    }

    wxString wtitle;
#ifdef __WXMAC__
    wtitle.Printf(_("%s%s [%s]"),
                  prefix.c_str(), currlayer->currname.c_str(), rule.c_str());
#else
    wtitle.Printf(_("%s%s [%s] - Golly"),
                  prefix.c_str(), currlayer->currname.c_str(), rule.c_str());
#endif
    
    SetTitle(wtitle);
}

// -----------------------------------------------------------------------------

void MainFrame::CreateUniverse()
{
    // save current rule
    wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    
    // delete old universe and create new one of same type
    delete currlayer->algo;
    currlayer->algo = CreateNewUniverse(currlayer->algtype);
    
    // ensure new universe uses same rule (and thus same # of cell states)
    RestoreRule(oldrule);
    
    // increment has been reset to 1 but that's probably not always desirable
    // so set increment using current step size
    SetGenIncrement();
}

// -----------------------------------------------------------------------------

void MainFrame::NewPattern(const wxString& title)
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(wxID_NEW);
        Stop();
        return;
    }
    
    if (askonnew && currlayer->dirty && !SaveCurrentLayer()) return;
    
    if (inscript) stop_after_script = true;
    currlayer->savestart = false;
    currlayer->startgen = 0;
    currlayer->startfile.Clear();
    currlayer->currfile.Clear();
    
    // reset step size before CreateUniverse calls SetGenIncrement
    currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
    currlayer->currexpo = 0;
    
    // create new, empty universe of same type and using same rule
    CreateUniverse();
    
    // reset timing info used in DisplayTimingInfo
    endtime = begintime = 0;
    
    // clear all undo/redo history
    currlayer->undoredo->ClearUndoRedo();
    
    if (newremovesel) currlayer->currsel.Deselect();
    if (newcurs) currlayer->curs = newcurs;
    viewptr->SetPosMag(bigint::zero, bigint::zero, newmag);
    
    // best to restore true origin
    if (currlayer->originx != bigint::zero || currlayer->originy != bigint::zero) {
        currlayer->originx = 0;
        currlayer->originy = 0;
        statusptr->SetMessage(origin_restored);
    }
    
    // restore default colors for current algo/rule
    UpdateLayerColors();
    
    MarkLayerClean(title);     // calls SetWindowTitle
    UpdateEverything();
}

// -----------------------------------------------------------------------------

static bool IsImageFile(const wxString& path)
{
    wxString ext = path.AfterLast('.');
    // if path has no extension then ext == path
    if (ext == path) return false;
    
    // supported extensions match image handlers added in GollyApp::OnInit()
    return  ext.IsSameAs(wxT("bmp"),false) ||
            ext.IsSameAs(wxT("gif"),false) ||
            ext.IsSameAs(wxT("png"),false) ||
            ext.IsSameAs(wxT("tif"),false) ||
            ext.IsSameAs(wxT("tiff"),false) ||
            ext.IsSameAs(wxT("icons"),false) ||
            // we don't actually support JPEG files but let LoadImage handle them
            ext.IsSameAs(wxT("jpg"),false) ||
            ext.IsSameAs(wxT("jpeg"),false);
}

// -----------------------------------------------------------------------------

// only called by LoadPattern
bool MainFrame::LoadImage(const wxString& path)
{
    // don't try to load JPEG file
    wxString ext = path.AfterLast('.');
    if ( ext.IsSameAs(wxT("jpg"),false) ||
        ext.IsSameAs(wxT("jpeg"),false) ) {
        Warning(_("Golly cannot import JPEG data, only BMP/GIF/PNG/TIFF."));
        // pattern will be empty
        return true;
    }
    
    wxImage image;
    if ( image.LoadFile(path) ) {
        // don't change the current rule here -- that way the image can
        // be loaded into any algo
        unsigned char maskr, maskg, maskb;
        bool hasmask = image.GetOrFindMaskColour(&maskr, &maskg, &maskb);
        int wd = image.GetWidth();
        int ht = image.GetHeight();
        unsigned char* idata = image.GetData();
        int x, y;
        lifealgo* curralgo = currlayer->algo;
        for (y = 0; y < ht; y++) {
            for (x = 0; x < wd; x++) {
                long pos = (y * wd + x) * 3;
                unsigned char r = idata[pos];
                unsigned char g = idata[pos+1];
                unsigned char b = idata[pos+2];
                if ( hasmask && r == maskr && g == maskg && b == maskb ) {
                    // treat transparent pixel as a dead cell
                } else if ( r < 255 || g < 255 || b < 255 ) {
                    // treat non-white pixel as a live cell
                    curralgo->setcell(x, y, 1);
                }
            }
        }
        curralgo->endofpattern();
    } else {
        Warning(_("Could not load image from file!"));
    }
    return true;
}

// -----------------------------------------------------------------------------

void MainFrame::LoadPattern(const wxString& path, const wxString& newtitle,
                            bool updatestatus, bool updateall)
{
    if (insideYield > 0) return; // avoid recursion (probably unnecessary here, but play safe)

    if ( !wxFileName::FileExists(path) ) {
        Warning(_("The file does not exist:\n") + path);
        return;
    }
    
    // newtitle is only empty if called from ResetPattern/RestorePattern
    if (!newtitle.IsEmpty()) {
        if (askonload && currlayer->dirty && !SaveCurrentLayer()) return;
        
        if (inscript) stop_after_script = true;
        currlayer->savestart = false;
        currlayer->startfile = path;
        currlayer->currfile = path;
        
        // reset step size now in case UpdateStatus is called below
        currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
        currlayer->currexpo = 0;
        
        if (GetInfoFrame()) {
            // comments will no longer be relevant so close info window
            GetInfoFrame()->Close(true);
        }
        
        // reset timing info used in DisplayTimingInfo
        endtime = begintime = 0;
        
        // clear all undo/redo history
        currlayer->undoredo->ClearUndoRedo();
    }
    
    if (!showbanner) statusptr->ClearMessage();
    
    // set nopattupdate BEFORE UpdateStatus() call so we see gen=0 and pop=0;
    // in particular, it avoids getPopulation being called which would
    // slow down hlife pattern loading
    viewptr->nopattupdate = true;
    
    if (updatestatus) {
        // update all of status bar so we don't see different colored lines;
        // on Mac, DrawView also gets called if there are pending updates
        UpdateStatus();
    }
    
    // save current algo and rule
    algo_type oldalgo = currlayer->algtype;
    wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    
    // delete old universe and create new one of same type
    delete currlayer->algo;
    currlayer->algo = CreateNewUniverse(currlayer->algtype);
    
    if (!newtitle.IsEmpty() && !inscript) {
        // show new file name in window title but no rule (which readpattern can change);
        // nicer if user can see file name while loading a very large pattern
        SetTitle(_("Loading ") + newtitle);
    }
    
    if (IsImageFile(path)) {
        // ensure new universe uses same rule
        RestoreRule(oldrule);
        LoadImage(path);
        viewptr->nopattupdate = false;
    } else {
        const char* err = readpattern(FILEPATH, *currlayer->algo);
        if (err) {
            wxString bigerr = _("File could not be loaded by any algorithm.");
            wxString algoname = wxString(GetAlgoName(currlayer->algtype), wxConvLocal);
            bigerr += wxString::Format(_("\n\nError from %s:\n"), algoname.c_str());
            bigerr += wxString(err, wxConvLocal);
            
            // cycle thru all other algos until readpattern succeeds
            for (int i = 0; i < NumAlgos(); i++) {
                if (i != oldalgo) {
                    currlayer->algtype = i;
                    delete currlayer->algo;
                    currlayer->algo = CreateNewUniverse(currlayer->algtype);
                    // readpattern will call setrule
                    err = readpattern(FILEPATH, *currlayer->algo);
                    if (err) {
                        algoname = wxString(GetAlgoName(currlayer->algtype), wxConvLocal);
                        bigerr += wxString::Format(_("\n\nError from %s:\n"), algoname.c_str());
                        bigerr += wxString(err, wxConvLocal);
                    } else {
                        break;
                    }
                }
            }
            viewptr->nopattupdate = false;
            if (err) {
                // no algo could read pattern so restore original algo and rule
                currlayer->algtype = oldalgo;
                delete currlayer->algo;
                currlayer->algo = CreateNewUniverse(currlayer->algtype);
                RestoreRule(oldrule);
                // also show full path to file (useful when debugging!)
                bigerr += wxString::Format(_("\n\nFile path:\n%s"), wxString(FILEPATH,wxConvLocal).c_str());
                Warning(bigerr);
            }
        }
        viewptr->nopattupdate = false;
    }
    
    if (!newtitle.IsEmpty()) {
        MarkLayerClean(newtitle);     // calls SetWindowTitle
        
        if (TimelineExists()) {
            // we've loaded a .mc file with a timeline so go to 1st frame
            InitTimelineFrame();
            if (!showtimeline) ToggleTimelineBar();
            // switch to the base step and exponent used to record the timeline
            pair<int, int> be = currlayer->algo->getbaseexpo();
            currlayer->currbase = be.first;
            currlayer->currexpo = be.second;
        } else {
            // restore default base step for current algo
            // (currlayer->currexpo was set to 0 above)
            currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
        }
        SetGenIncrement();
        
        // restore default colors for current algo/rule
        UpdateLayerColors();
        
        if (openremovesel) currlayer->currsel.Deselect();
        if (opencurs) currlayer->curs = opencurs;
        
        viewptr->FitInView(1);
        currlayer->startgen = currlayer->algo->getGeneration();     // might be > 0
        if (updateall) UpdateEverything();
        showbanner = false;
    } else {
        // ResetPattern/RestorePattern does the update
    }
}

// -----------------------------------------------------------------------------

void MainFrame::CheckBeforeRunning(const wxString& scriptpath, bool remember,
                                   const wxString& zippath)
{
    bool ask;
    if (zippath.IsEmpty()) {
        // script was downloaded via "get:" link (script is in downloaddir --
        // see GetURL in wxhelp.cpp) so always ask user if it's okay to run
        ask = true;
    } else {
        // script is included in zip file (scriptpath starts with tempdir) so only
        // ask user if zip file was downloaded via "get:" link
        ask = zippath.StartsWith(downloaddir);
    }
    
    if (ask) {
        UpdateEverything();     // in case OpenZipFile called LoadPattern

        // create our own dialog with a View button???  probably no need now that
        // user can ctrl/right-click on link to open script in their text editor
        wxString msg = scriptpath + _("\n\nClick \"No\" if the script is from an untrusted source.");
        int answer = wxMessageBox(msg, _("Do you want to run this script?"),
                                  wxICON_QUESTION | wxYES_NO | wxNO_DEFAULT,
                                  wxGetActiveWindow());
        switch (answer) {
            case wxYES: break;
            case wxNO:  return;
            default:    return;  // No
        }
    }
    
    // also do this???
    // save script info (download path or zip path + script entry) in list of safe scripts
    // (stored in prefs file) so we can search for this script and not ask again
    
    Raise();
    if (remember) AddRecentScript(scriptpath);
    RunScript(scriptpath);
}

// -----------------------------------------------------------------------------

bool MainFrame::ExtractZipEntry(const wxString& zippath,
                                const wxString& entryname,
                                const wxString& outfile)
{
    wxFFileInputStream instream(zippath);
    if (!instream.Ok()) {
        Warning(_("Could not create input stream for zip file:\n") + zippath);
        return false;
    }
    wxZipInputStream zip(instream);
    
    wxZipEntry* entry;
    while ((entry = zip.GetNextEntry()) != NULL) {
        wxString thisname = entry->GetName();
        if (thisname == entryname) {
            // we've found the desired entry so copy entry data to given output file
            wxFileOutputStream outstream(outfile);
            if (outstream.Ok()) {
                // read and write in chunks so we can show a progress dialog
                const int BUFFER_SIZE = 4000;
                char buf[BUFFER_SIZE];
                size_t incount = 0;
                size_t outcount = 0;
                size_t lastread, lastwrite;
                double filesize = (double) entry->GetSize();
                if (filesize <= 0.0) filesize = -1.0;        // show indeterminate progress
                
                BeginProgress(_("Extracting file"));
                while (true) {
                    zip.Read(buf, BUFFER_SIZE);
                    lastread = zip.LastRead();
                    if (lastread == 0) break;
                    outstream.Write(buf, lastread);
                    lastwrite = outstream.LastWrite();
                    incount += lastread;
                    outcount += lastwrite;
                    if (incount != outcount) {
                        Warning(_("Error occurred while writing file:\n") + outfile);
                        break;
                    }
                    char msg[128];
                    sprintf(msg, "File size: %.2f MB", double(incount) / 1048576.0);
                    if (AbortProgress((double)incount / filesize, wxString(msg,wxConvLocal))) {
                        outcount = 0;
                        break;
                    }
                }
                EndProgress();
                
                if (incount == outcount) {
                    // successfully copied entry data to outfile
                    delete entry;
                    return true;
                } else {
                    // delete incomplete outfile
                    if (wxFileExists(outfile)) wxRemoveFile(outfile);
                }
            } else {
                Warning(_("Could not open output stream for file:\n") + outfile);
            }
            delete entry;
            return false;
        }
        delete entry;
    }
    
    // should not get here
    Warning(_("Could not find zip file entry:\n") + entryname);
    return false;
}

// -----------------------------------------------------------------------------

static bool RuleInstalled(wxZipInputStream& zip, const wxString& rulepath)
{
    wxFileOutputStream outstream(rulepath);
    bool ok = outstream.Ok();
    if (ok) {
        zip.Read(outstream);
        ok = (outstream.GetLastError() == wxSTREAM_NO_ERROR);
    }
    return ok;
}

// -----------------------------------------------------------------------------

void MainFrame::OpenZipFile(const wxString& zippath)
{
    // Process given zip file in the following manner:
    // - If it contains any .rule files then extract and install those
    //   files into userrules (the user's rules directory).
    // - If the zip file is "complex" (contains any folders, rule files, text files,
    //   or more than one pattern, or more than one script), build a temporary html
    //   file with clickable links to each file entry and show it in the help window.
    // - If the zip file contains at most one pattern and at most one script (both
    //   at the root level) then load the pattern (if present) and then run the script
    //   (if present and if allowed).
    
    const wxString indent = wxT("&nbsp;&nbsp;&nbsp;&nbsp;");
    bool dirseen = false;
    bool diffdirs = (userrules != rulesdir);
    wxString firstdir = wxEmptyString;
    wxString lastpattern = wxEmptyString;
    wxString lastscript = wxEmptyString;
    int patternseps = 0;                    // # of separators in lastpattern
    int scriptseps = 0;                     // # of separators in lastscript
    int patternfiles = 0;
    int scriptfiles = 0;
    int textfiles = 0;                      // includes html files
    int rulefiles = 0;
    wxSortedArrayString rulelist;           // list of installed .rule files
    
    // note that we need to create the HTML file with UTF-8 encoding
    // in case zippath contains non-ASCII characters
    wxString contents = wxT("<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">");
    contents += wxT("<title>") + GetBaseName(zippath);
    contents += wxT("</title>\n");
    contents += wxT("<body bgcolor=\"#FFFFCE\">\n");
    contents += wxT("<p>\n");
    contents += wxT("Zip file: ");
    contents += zippath;
    contents += wxT("<p>\n");
    contents += wxT("Contents:<br>\n");
    
    wxFFileInputStream instream(zippath);
    if (!instream.Ok()) {
        Warning(_("Could not create input stream for zip file:\n") + zippath);
        return;
    }
    wxZipInputStream zip(instream);
    
    // examine each entry in zip file and build contents string;
    // also install any .rule files
    wxZipEntry* entry;
    while ((entry = zip.GetNextEntry()) != NULL) {
        wxString name = entry->GetName();      
        if (name.StartsWith(wxT("__MACOSX")) || name.EndsWith(wxT(".DS_Store"))) {
            // ignore meta-data stuff in zip file created on Mac
        } else {
            // indent depending on # of separators in name
            unsigned int sepcount = 0;
            unsigned int i = 0;
            unsigned int len = (unsigned int)name.length();
            while (i < len) {
                if (name[i] == wxFILE_SEP_PATH) sepcount++;
                i++;
            }
            // check if 1st directory has multiple separators (eg. in jslife.zip)
            if (entry->IsDir() && !dirseen && sepcount > 1) {
                firstdir = name.BeforeFirst(wxFILE_SEP_PATH);
                contents += firstdir;
                contents += wxT("<br>\n");
            }
            for (i = 1; i < sepcount; i++) contents += indent;
            
            if (entry->IsDir()) {
                // remove terminating separator from directory name
                name = name.BeforeLast(wxFILE_SEP_PATH);
                name = name.AfterLast(wxFILE_SEP_PATH);
                if (dirseen && name == firstdir) {
                    // ignore dir already output earlier (eg. in jslife.zip)
                } else {
                    contents += name;
                    contents += wxT("<br>\n");
                }
                dirseen = true;
                
            } else {
                // entry is for some sort of file
                wxString filename = name.AfterLast(wxFILE_SEP_PATH);
                if (dirseen) contents += indent;
                if (filename.EndsWith(wxT(".table")) || filename.EndsWith(wxT(".tree")) ||
                    filename.EndsWith(wxT(".colors")) || filename.EndsWith(wxT(".icons"))) {
                    // these files are no longer supported and are simply ignored
                    contents += filename;
                    contents += indent;
                    contents += "[ignored]";
                } else {
                    // user can extract file via special "unzip:" link
                    contents += wxT("<a href=\"unzip:");
                    contents += zippath;
                    contents += wxT(":");
                    contents += name;
                    contents += wxT("\">");
                    contents += filename;
                    contents += wxT("</a>");
                    
                    if ( IsRuleFile(filename) ) {
                        // extract and install .rule file into userrules
                        wxString outfile = userrules + filename;
                        if (RuleInstalled(zip, outfile)) {
                            // file successfully installed
                            rulelist.Add(filename);
                            contents += indent;
                            contents += wxT("[installed]");
                            if (diffdirs) {
                                // check if this file overrides similarly named file in rulesdir
                                wxString clashfile = rulesdir + filename;
                                if (wxFileExists(clashfile)) {
                                    contents += indent;
                                    contents += wxT("(overrides file in Rules folder)");
                                }
                            }
                        } else {
                            // file could not be installed
                            contents += indent;
                            contents += wxT("[NOT installed]");
                            // file is probably incomplete so best to delete it
                            if (wxFileExists(outfile)) wxRemoveFile(outfile);
                        }
                        rulefiles++;
                        
                    } else if ( IsHTMLFile(filename) || IsTextFile(filename) ) {
                        textfiles++;
                        
                    } else if ( IsScriptFile(filename) ) {
                        scriptfiles++;
                        lastscript = name;
                        scriptseps = sepcount;
                        
                    } else {
                        patternfiles++;
                        lastpattern = name;
                        patternseps = sepcount;
                    }
                }
                contents += wxT("<br>\n");
            }
        }
        delete entry;
    }  // end while
    
    if (rulefiles > 0) {
        contents += wxT("<p>Files marked as \"[installed]\" have been stored in your rules folder:<br>\n");
        contents += userrules;
        contents += wxT("\n");
    }
    contents += wxT("\n</body></html>");
    
    if (dirseen || rulefiles > 0 || textfiles > 0 || patternfiles > 1 || scriptfiles > 1) {
        // complex zip, so write contents to a temporary html file and display it in help window;
        // use a unique file name so user can go back/forwards
        wxString htmlfile = wxFileName::CreateTempFileName(tempdir + wxT("zip_contents_"));
        wxRemoveFile(htmlfile);
        htmlfile += wxT(".html");
        wxFile outfile(htmlfile, wxFile::write);
        if (outfile.IsOpened()) {
            outfile.Write(contents);
            outfile.Close();
            ShowHelp(htmlfile);
        } else {
            Warning(_("Could not create html file:\n") + htmlfile);
        }
    }
    
    if (patternfiles <= 1 && scriptfiles <= 1 && patternseps == 0 && scriptseps == 0) {
        // load lastpattern (if present), then run lastscript (if present);
        // the script might be a long-running one that allows user interaction,
        // so it's best to run it AFTER calling ShowHelp above
        if (patternfiles == 1) {
            wxString tempfile = tempdir + lastpattern.AfterLast(wxFILE_SEP_PATH);
            if (ExtractZipEntry(zippath, lastpattern, tempfile)) {
                Raise();
                // don't call AddRecentPattern(tempfile) here; OpenFile has added
                // zippath to recent patterns
                LoadPattern(tempfile, GetBaseName(tempfile), true, scriptfiles == 0);
            }
        }
        if (scriptfiles == 1) {
            wxString tempfile = tempdir + lastscript.AfterLast(wxFILE_SEP_PATH);
            if (ExtractZipEntry(zippath, lastscript, tempfile)) {
                // run script depending on safety check
                CheckBeforeRunning(tempfile, false, zippath);
            } else {
                // should never happen but play safe
                UpdateEverything();
            }
        }
    }
}

// -----------------------------------------------------------------------------

static bool RuleCanBeFound(const wxString& path)
{
    // ensure given path to .rule file is a full path
    wxString fullpath = path;
    wxFileName fname(fullpath);
    if (!fname.IsAbsolute()) fullpath = gollydir + path;
    
    // check that .rule file is in userrules or rulesdir
    wxString dir = fullpath.BeforeLast(wxFILE_SEP_PATH);
    dir += wxFILE_SEP_PATH;
    if (dir == userrules || dir == rulesdir) {
        return true;
    } else {
        wxString msg = _("You need to move ");
        msg += fullpath.AfterLast(wxFILE_SEP_PATH);
        msg += _(" into your rules folder (");
        msg += userrules;
        msg += _(") so the RuleLoader algorithm can find it.");
        Warning(msg);
        return false;
    }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenFile(const wxString& path, bool remember)
{
    if (IsHTMLFile(path)) {
        // show HTML file in help window
        ShowHelp(path);
        return;
    }

    if (IsTextFile(path)) {
        // open text file in user's preferred text editor
        EditFile(path);
        return;
    }

    if (generating) {
        command_pending = true;
        // assume remember is true (should only be false if called from a script)
        if ( IsScriptFile(path) ) {
            AddRecentScript(path);
            cmdevent.SetId(ID_RUN_RECENT + 1);
        } else {
            AddRecentPattern(path);
            cmdevent.SetId(ID_OPEN_RECENT + 1);
        }
        Stop();
        return;
    }

    // note that pass_file_events is false if OpenFile was called from GSF_open
    if (inscript && pass_file_events) {
        // ensure path is a full path
        wxString newpath = path;
        wxFileName fname(newpath);
        if (!fname.IsAbsolute()) newpath = gollydir + path;
        PassFileToScript(newpath);
        return;
    }
    
    if (!inscript && path.EndsWith(wxT(".rle3"))) {
        if (remember) AddRecentPattern(path);
        
        // create a full path in rle3path for GSF_getevent to use
        rle3path = path;
        wxFileName fname(rle3path);
        if (!fname.IsAbsolute()) rle3path = gollydir + path;
        
        // start up 3D.lua (it will get a file event containing rle3path)
        wxString path3D = gollydir + wxT("Scripts");
        path3D += wxFILE_SEP_PATH;
        path3D += wxT("Lua");
        path3D += wxFILE_SEP_PATH;
        path3D += wxT("3D.lua");
        RunScript(path3D);
        return;
    }

    if (IsScriptFile(path)) {
        // execute script
        if (remember) AddRecentScript(path);
        RunScript(path);

    } else if (IsZipFile(path)) {
        // process zip file
        if (remember) AddRecentPattern(path);   // treat it like a pattern
        OpenZipFile(path);

    } else if (IsRuleFile(path)) {
        // switch to rule, but only if it's in rulesdir or userrules
        if (RuleCanBeFound(path))
            LoadRule(path.AfterLast(wxFILE_SEP_PATH).BeforeLast('.'));

    } else {
        // load pattern
        if (remember) AddRecentPattern(path);
        
        // ensure path is a full path because a script might want to reset() to it
        // (in which case the cwd is the script's directory, not gollydir)
        wxString newpath = path;
        wxFileName fname(newpath);
        if (!fname.IsAbsolute()) newpath = gollydir + path;
        
        LoadPattern(newpath, GetBaseName(path));
    }
}

// -----------------------------------------------------------------------------

void MainFrame::AddRecentPattern(const wxString& inpath)
{
    if (inpath.IsEmpty()) return;
    wxString path = inpath;
    if (path.StartsWith(gollydir)) {
        // remove gollydir from start of path
        path.erase(0, gollydir.length());
    }
    
    // duplicate any ampersands so they appear in menu
    path.Replace(wxT("&"), wxT("&&"));
    
    // put given path at start of patternSubMenu
    int id = patternSubMenu->FindItem(path);
    if ( id == wxNOT_FOUND ) {
        if ( numpatterns < maxpatterns ) {
            // add new path
            numpatterns++;
            id = ID_OPEN_RECENT + numpatterns;
            patternSubMenu->Insert(numpatterns - 1, id, path);
        } else {
            // replace last item with new path
            wxMenuItem* item = patternSubMenu->FindItemByPosition(maxpatterns - 1);
            item->SetItemLabel(path);
            id = ID_OPEN_RECENT + maxpatterns;
        }
    }
    // path exists in patternSubMenu
    if ( id > ID_OPEN_RECENT + 1 ) {
        // move path to start of menu
        wxMenuItem* item;
        while ( id > ID_OPEN_RECENT + 1 ) {
            wxMenuItem* previtem = patternSubMenu->FindItem(id - 1);
            wxString prevpath = wxMenuItem::GetLabelText(previtem->GetItemLabel());
            item = patternSubMenu->FindItem(id);
            prevpath.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
            item->SetItemLabel(prevpath);
            id--;
        }
        item = patternSubMenu->FindItem(id);
        item->SetItemLabel(path);
    }
    
    wxMenuBar* mbar = GetMenuBar();
    if (mbar) {
        // set keyboard shortcut for most recent pattern
        int item = ID_OPEN_RECENT + 1;
        wxString label = wxMenuItem::GetLabelText(mbar->GetLabel(item));
        label.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
        mbar->SetLabel(item, label + GetAccelerator(DO_OPENRECENT));
    }
}

// -----------------------------------------------------------------------------

void MainFrame::AddRecentScript(const wxString& inpath)
{
    if (inpath.IsEmpty()) return;
    wxString path = inpath;
    if (path.StartsWith(gollydir)) {
        // remove gollydir from start of path
        path.erase(0, gollydir.length());
    }
    
    // duplicate any ampersands so they appear in menu
    path.Replace(wxT("&"), wxT("&&"));
    
    // put given path at start of scriptSubMenu
    int id = scriptSubMenu->FindItem(path);
    if ( id == wxNOT_FOUND ) {
        if ( numscripts < maxscripts ) {
            // add new path
            numscripts++;
            id = ID_RUN_RECENT + numscripts;
            scriptSubMenu->Insert(numscripts - 1, id, path);
        } else {
            // replace last item with new path
            wxMenuItem* item = scriptSubMenu->FindItemByPosition(maxscripts - 1);
            item->SetItemLabel(path);
            id = ID_RUN_RECENT + maxscripts;
        }
    }
    // path exists in scriptSubMenu
    if ( id > ID_RUN_RECENT + 1 ) {
        // move path to start of menu
        wxMenuItem* item;
        while ( id > ID_RUN_RECENT + 1 ) {
            wxMenuItem* previtem = scriptSubMenu->FindItem(id - 1);
            wxString prevpath = wxMenuItem::GetLabelText(previtem->GetItemLabel());
            item = scriptSubMenu->FindItem(id);
            prevpath.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
            item->SetItemLabel(prevpath);
            id--;
        }
        item = scriptSubMenu->FindItem(id);
        item->SetItemLabel(path);
    }
    
    wxMenuBar* mbar = GetMenuBar();
    if (mbar) {
        // set keyboard shortcut for most recent script
        int item = ID_RUN_RECENT + 1;
        wxString label = wxMenuItem::GetLabelText(mbar->GetLabel(item));
        label.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
        mbar->SetLabel(item, label + GetAccelerator(DO_RUNRECENT));
    }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenPattern()
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(wxID_OPEN);
        Stop();
        return;
    }
    
    wxString filetypes = _("All files (*)|*");
    filetypes +=         _("|RLE (*.rle)|*.rle");
    filetypes +=         _("|RLE3 (*.rle3)|*.rle3");
    filetypes +=         _("|Macrocell (*.mc)|*.mc");
    filetypes +=         _("|Gzip (*.gz)|*.gz");
    filetypes +=         _("|Life 1.05/1.06 (*.lif)|*.lif");
    filetypes +=         _("|dblife (*.l)|*.l");
    filetypes +=         _("|MCell (*.mcl)|*.mcl");
    filetypes +=         _("|Zip (*.zip;*.gar)|*.zip;*.gar");
    filetypes +=         _("|BMP (*.bmp)|*.bmp");
    filetypes +=         _("|GIF (*.gif)|*.gif");
    filetypes +=         _("|PNG (*.png)|*.png");
    filetypes +=         _("|TIFF (*.tiff;*.tif)|*.tiff;*.tif");
    
    wxFileDialog opendlg(this, _("Choose a pattern"),
                         opensavedir, wxEmptyString, filetypes,
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    int button = opendlg.ShowModal();
    if (viewptr) viewptr->ResetMouseDown();
    if (button == wxID_OK) {
        wxFileName fullpath( opendlg.GetPath() );
        opensavedir = fullpath.GetPath();
        OpenFile( opendlg.GetPath() );
    }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenScript()
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(ID_RUN_SCRIPT);
        Stop();
        return;
    }
    
    wxString filetypes = _("Lua or Python (*.lua;*.py)|*.lua;*.py");
    filetypes +=         _("|Lua (*.lua)|*.lua");
    filetypes +=         _("|Python (*.py)|*.py");

    wxFileDialog opendlg(this, _("Choose a script"),
                         rundir, wxEmptyString, filetypes,
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    int button = opendlg.ShowModal();
    if (viewptr) viewptr->ResetMouseDown();
    if (button == wxID_OK) {
        wxFileName fullpath( opendlg.GetPath() );
        rundir = fullpath.GetPath();
        AddRecentScript( opendlg.GetPath() );
        RunScript( opendlg.GetPath() );
    }
}

// -----------------------------------------------------------------------------

bool MainFrame::CopyTextToClipboard(const wxString& text)
{
    bool result = true;
    if (wxTheClipboard->Open()) {
        if ( !wxTheClipboard->SetData(new wxTextDataObject(text)) ) {
            Warning(_("Could not copy text to clipboard!"));
            result = false;
        }
        wxTheClipboard->Close();
    } else {
        Warning(_("Could not open clipboard!"));
        result = false;
    }
    return result;
}

// -----------------------------------------------------------------------------

bool MainFrame::GetTextFromClipboard(wxTextDataObject* textdata)
{
    bool gotdata = false;
    
    if ( wxTheClipboard->Open() ) {
        if ( wxTheClipboard->IsSupported( wxDF_UNICODETEXT ) ) {
            gotdata = wxTheClipboard->GetData( *textdata );
            if (!gotdata) {
                statusptr->ErrorMessage(_("Could not get clipboard text!"));
            }
            
        } else if ( wxTheClipboard->IsSupported( wxDF_BITMAP ) ) {
            wxBitmapDataObject bmapdata;
            gotdata = wxTheClipboard->GetData( bmapdata );
            if (gotdata) {
                // convert bitmap data to text data
                wxString str;
                wxBitmap bmap = bmapdata.GetBitmap();
                wxImage image = bmap.ConvertToImage();
                if (image.Ok()) {
                    /* there doesn't seem to be any mask or alpha info, at least on Mac
                    if (bmap.GetMask() != NULL) Warning(_("Bitmap has mask!"));
                    if (image.HasMask()) Warning(_("Image has mask!"));
                    if (image.HasAlpha()) Warning(_("Image has alpha!"));
                    */
                    int wd = image.GetWidth();
                    int ht = image.GetHeight();
                    unsigned char* idata = image.GetData();
                    int x, y;
                    for (y = 0; y < ht; y++) {
                        for (x = 0; x < wd; x++) {
                            long pos = (y * wd + x) * 3;
                            if ( idata[pos] < 255 || idata[pos+1] < 255 || idata[pos+2] < 255 ) {
                                // non-white pixel is a live cell
                                str += 'o';
                            } else {
                                // white pixel is a dead cell
                                str += '.';
                            }
                        }
                        str += '\n';
                    }
                    textdata->SetText(str);
                } else {
                    statusptr->ErrorMessage(_("Could not convert clipboard bitmap!"));
                    gotdata = false;
                }
            } else {
                statusptr->ErrorMessage(_("Could not get clipboard bitmap!"));
            }
            
        } else {
            statusptr->ErrorMessage(_("No data in clipboard."));
        }
        wxTheClipboard->Close();
        
    } else {
        statusptr->ErrorMessage(_("Could not open clipboard!"));
    }
    
    return gotdata;
}

// -----------------------------------------------------------------------------

bool MainFrame::ClipboardContainsRule()
{
    wxTextDataObject data;
    if (!GetTextFromClipboard(&data)) return false;
    
    wxString cliptext = data.GetText();
    if (!cliptext.StartsWith(wxT("@RULE "))) return false;
    
    // extract rule name
    wxString rulename;
    int i = 6;
    while (cliptext[i] > ' ') {
        rulename += cliptext[i];
        i++;
    }
    
    // check if rulename.rule already exists
    wxString rulepath = userrules + rulename;
    rulepath += wxT(".rule");
    if (wxFileExists(rulepath)) {
        wxString question = _("Do you want to replace the existing ") + rulename;
        question += _(".rule with the version in the clipboard?");
        int answer = wxMessageBox(question, _("Replace existing .rule file?"),
                                  wxICON_QUESTION | wxYES_NO | wxNO_DEFAULT, wxGetActiveWindow());
        if (answer == wxNO) {
            // don't overwrite existing .rule file
            return true;
        }
    }
    
    // create rulename.rule in user-specific rules folder
    wxFile rulefile(rulepath, wxFile::write);
    if (!rulefile.IsOpened()) {
        Warning(_("Could not open .rule file for writing:\n") + rulepath);
        return true;
    }
    if (!rulefile.Write(data.GetText())) {
        Warning(_("Could not write clipboard data to .rule file!"));
        rulefile.Close();
        return true;
    }
    rulefile.Close();
    statusptr->DisplayMessage(_("Created ") + rulepath);
    
    // now switch to the newly created rule
    LoadRule(rulename);

    return true;
}

// -----------------------------------------------------------------------------

bool MainFrame::ClipboardContainsRLE3()
{
    wxTextDataObject data;
    if (!GetTextFromClipboard(&data)) return false;

    wxString cliptext = data.GetText();
    if (!cliptext.StartsWith(wxT("3D version"))) return false;

    // copy clipboard data to a temporary .rle3 file
    wxString filepath = tempdir + wxT("clipboard.rle3");
    wxFile rle3file(filepath, wxFile::write);
    if (!rle3file.IsOpened()) {
        Warning(_("Could not open clipboard.rle3 for writing!"));
        return true;
    }
    if (!rle3file.Write(data.GetText())) {
        Warning(_("Could not write clipboard data to file!"));
        rle3file.Close();
        return true;
    }
    rle3file.Close();

    // set rle3path for GSF_getevent
    rle3path = filepath;

    // start up 3D.lua (it will get a file event containing rle3path)
    wxString path3D = gollydir + wxT("Scripts");
    path3D += wxFILE_SEP_PATH;
    path3D += wxT("Lua");
    path3D += wxFILE_SEP_PATH;
    path3D += wxT("3D.lua");
    RunScript(path3D);

    return true;
}

// -----------------------------------------------------------------------------

void MainFrame::OpenClipboard()
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(ID_OPEN_CLIP);
        Stop();
        return;
    }
    
    // if clipboard text starts with "@RULE rulename" then install rulename.rule
    // and switch to that rule
    if (ClipboardContainsRule()) return;
    
    // if clipboard text starts with "3D version" then start up 3D.lua
    // and load the RLE3 pattern
    if (ClipboardContainsRLE3()) return;
    
    // load and view pattern data stored in clipboard
    wxTextDataObject data;
    if (GetTextFromClipboard(&data)) {
        // copy clipboard data to tempstart so we can handle all formats
        // supported by readpattern
        wxFile outfile(currlayer->tempstart, wxFile::write);
        if ( outfile.IsOpened() ) {
            outfile.Write( data.GetText() );
            outfile.Close();
            LoadPattern(currlayer->tempstart, _("clipboard"));
            // do NOT delete tempstart -- it can be reloaded by ResetPattern
            // or used by ShowPatternInfo
        } else {
            statusptr->ErrorMessage(_("Could not create tempstart file!"));
        }
    }
}

// -----------------------------------------------------------------------------

static wxString nonscript = _("nonscript");

wxString MainFrame::GetScriptFileName(const wxString& text)
{
    // examine given text to see if it contains Lua or Python code:
    // if "--" or "local" or "require" is at start of line then return luafile,
    // if "import" or "from" is at start of line then return pythonfile,
    // otherwise return nonscript so caller can display a suitable message
    int linelen = 0;
    
    // need to be careful converting Unicode wxString to char*
    wxCharBuffer buff = text.mb_str(wxConvUTF8);
    const char* p = (const char*) buff;
    while (*p) {
        switch (*p) {
            case '#':
                // probably a comment, so ignore rest of line
                // (note that MCell files have all lines starting with #)
                while (*p && *p != 13 && *p != 10) p++;
                linelen = 0;
                if (*p) p++;
                break;
            case 13: case 10:
                linelen = 0;
                p++;
                break;
            case '-':
                if (linelen == 1 && p[-1] == '-') return luafile;   // Lua comment at start of line
                linelen++;
                p++;
                break;
            case ' ':
                // look for language-specific keyword at start of line
                if (linelen == 5 && strncmp(p-5,"local",5) == 0) return luafile;
                if (linelen == 7 && strncmp(p-7,"require",7) == 0) return luafile;
                
                if (linelen == 4 && strncmp(p-4,"from",4) == 0) return pythonfile;
                if (linelen == 6 && strncmp(p-6,"import",6) == 0) return pythonfile;
                // don't break
            default:
                if (linelen == 0 && (*p == ' ' || *p == 9)) {
                    // ignore spaces/tabs at start of line
                } else {
                    linelen++;
                }
                p++;
        }
    }
    
    // clipboard does not contain a valid Lua/Python script
    return nonscript;
}

// -----------------------------------------------------------------------------

void MainFrame::RunClipboard()
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(ID_RUN_CLIP);
        Stop();
        return;
    }
    
    // run script stored in clipboard
    wxTextDataObject data;
    if (GetTextFromClipboard(&data)) {
        // scriptfile depends on whether the clipboard data contains Lua or Python code
        wxString scriptfile = GetScriptFileName( data.GetText() );
        if (scriptfile == nonscript) {
            Warning(_("The clipboard does not contain a Lua or Python script.") +
                    _("  If it contains a pattern, try using Open Clipboard instead."));
        } else {
            // copy clipboard data to scriptfile
            wxFile outfile(scriptfile, wxFile::write);
            if (outfile.IsOpened()) {
                outfile.Write( data.GetText() );
                outfile.Close();
                RunScript(scriptfile);
            } else {
                Warning(_("Could not create script file!"));
            }
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenRecentPattern(int id)
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(id);
        Stop();
        return;
    }
    
    wxMenuItem* item = patternSubMenu->FindItem(id);
    if (item) {
        wxString path = item->GetItemLabelText();
        
        // if path isn't absolute then prepend Golly directory
        wxFileName fname(path);
        if (!fname.IsAbsolute()) path = gollydir + path;
        
        // path might be a zip file so call OpenFile rather than LoadPattern
        OpenFile(path);
    }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenRecentScript(int id)
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(id);
        Stop();
        return;
    }
    
    wxMenuItem* item = scriptSubMenu->FindItem(id);
    if (item) {
        wxString path = item->GetItemLabelText();
        
        // if path isn't absolute then prepend Golly directory
        wxFileName fname(path);
        if (!fname.IsAbsolute()) path = gollydir + path;
        
        AddRecentScript(path);
        RunScript(path);
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ClearMissingPatterns()
{
    int pos = 0;
    while (pos < numpatterns) {
        wxMenuItem* item = patternSubMenu->FindItemByPosition(pos);
        wxString path = item->GetItemLabelText();
        
        // if path isn't absolute then prepend Golly directory
        wxFileName fname(path);
        if (!fname.IsAbsolute()) path = gollydir + path;
        
        if (wxFileExists(path)) {
            // keep this item
            pos++;
        } else {
            // remove this item by shifting up later items
            int nextpos = pos + 1;
            while (nextpos < numpatterns) {
                wxMenuItem* nextitem = patternSubMenu->FindItemByPosition(nextpos);
                wxString label = nextitem->GetItemLabelText();
                label.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
                item->SetItemLabel(label);
                item = nextitem;
                nextpos++;
            }
            // delete last item
            patternSubMenu->Delete(item);
            numpatterns--;
        }
    }
    
    wxMenuBar* mbar = GetMenuBar();
    if (mbar) {
        mbar->Enable(ID_OPEN_RECENT, numpatterns > 0);
        if (numpatterns > 0) {
            // set keyboard shortcut for most recent pattern
            int item = ID_OPEN_RECENT + 1;
            wxString label = wxMenuItem::GetLabelText(mbar->GetLabel(item));
            label.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
            mbar->SetLabel(item, label + GetAccelerator(DO_OPENRECENT));
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ClearMissingScripts()
{
    int pos = 0;
    while (pos < numscripts) {
        wxMenuItem* item = scriptSubMenu->FindItemByPosition(pos);
        wxString path = item->GetItemLabelText();
        
        // if path isn't absolute then prepend Golly directory
        wxFileName fname(path);
        if (!fname.IsAbsolute()) path = gollydir + path;
        
        if (wxFileExists(path)) {
            // keep this item
            pos++;
        } else {
            // remove this item by shifting up later items
            int nextpos = pos + 1;
            while (nextpos < numscripts) {
                wxMenuItem* nextitem = scriptSubMenu->FindItemByPosition(nextpos);
                wxString label = nextitem->GetItemLabelText();
                label.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
                item->SetItemLabel(label);
                item = nextitem;
                nextpos++;
            }
            // delete last item
            scriptSubMenu->Delete(item);
            numscripts--;
        }
    }
    
    wxMenuBar* mbar = GetMenuBar();
    if (mbar) {
        mbar->Enable(ID_RUN_RECENT, numscripts > 0);
        if (numscripts > 0) {
            // set keyboard shortcut for most recent script
            int item = ID_RUN_RECENT + 1;
            wxString label = wxMenuItem::GetLabelText(mbar->GetLabel(item));
            label.Replace(wxT("&"), wxT("&&")); // duplicate any ampersands
            mbar->SetLabel(item, label + GetAccelerator(DO_RUNRECENT));
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ClearAllPatterns()
{
    while (numpatterns > 0) {
        patternSubMenu->Delete( patternSubMenu->FindItemByPosition(0) );
        numpatterns--;
    }
    wxMenuBar* mbar = GetMenuBar();
    if (mbar) mbar->Enable(ID_OPEN_RECENT, false);
}

// -----------------------------------------------------------------------------

void MainFrame::ClearAllScripts()
{
    while (numscripts > 0) {
        scriptSubMenu->Delete( scriptSubMenu->FindItemByPosition(0) );
        numscripts--;
    }
    wxMenuBar* mbar = GetMenuBar();
    if (mbar) mbar->Enable(ID_RUN_RECENT, false);
}

// -----------------------------------------------------------------------------

const char* MainFrame::WritePattern(const wxString& path,
                                    pattern_format format,
                                    output_compression compression,
                                    int top, int left, int bottom, int right)
{
    // if the format is RLE_format and the grid is bounded then force XRLE_format so that
    // position info is recorded (this position will be used when the file is read)
    if (format == RLE_format && (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0))
        format = XRLE_format;
    const char* err = writepattern(FILEPATH, *currlayer->algo, format,
                                   compression, top, left, bottom, right);    
    return err;
}

// -----------------------------------------------------------------------------

bool MainFrame::SavePattern()
{
    if (generating) {
        command_pending = true;
        cmdevent.SetId(wxID_SAVE);
        Stop();
        return false;
    }
    
    if (warn_on_save && currlayer->dirty && currlayer->algo->getGeneration() > currlayer->startgen &&
        !TimelineExists()) {
        wxString msg = _("Saving this generation will not save the changes you made earlier, ");
        msg +=         _("so you might want to select Reset or Undo and save those changes.");
        msg +=         _("\n\n(This warning can be disabled in Preferences > Layer.)");
        Warning(msg);
    }

    wxString filetypes;
    int MCindex, RLEindex;
    
    // initially all formats are not allowed (use any -ve number)
    MCindex = RLEindex = -1;
    
    // adding "*.gz" to the file types avoids a duplication bug in the wxOSX app
    wxString MCfiles, RLEfiles;
    MCfiles = _("Macrocell (*.mc)|*.mc");
    MCfiles += _("|Compressed Macrocell (*.mc.gz)|*.mc.gz;*.gz");
    if (savexrle) {
        RLEfiles = _("Extended RLE (*.rle)|*.rle");
        RLEfiles += _("|Compressed Extended RLE (*.rle.gz)|*.rle.gz;*.gz");
    } else {
        RLEfiles = _("RLE (*.rle)|*.rle");
        RLEfiles += _("|Compressed RLE (*.rle.gz)|*.rle.gz;*.gz");
    }
    
    bigint top, left, bottom, right;
    int itop, ileft, ibottom, iright;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    
    if (currlayer->algo->hyperCapable()) {
        // algorithm uses hashlife
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            // too big so only allow saving as MC file
            itop = ileft = ibottom = iright = 0;
            filetypes = MCfiles;
            MCindex = 0;
        } else {
            // allow saving as MC or RLE file
            itop = top.toint();
            ileft = left.toint();
            ibottom = bottom.toint();
            iright = right.toint();
            filetypes = MCfiles;
            filetypes += _("|");
            filetypes += RLEfiles;
            MCindex = 0;
            RLEindex = 1;
        }
    } else {
        // allow saving file only if pattern is small enough
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            statusptr->ErrorMessage(_("Pattern is outside +/- 10^9 boundary."));
            return false;
        }
        itop = top.toint();
        ileft = left.toint();
        ibottom = bottom.toint();
        iright = right.toint();
        filetypes = RLEfiles;
        RLEindex = 0;
    }
    
    wxFileDialog savedlg( this, _("Save pattern"),
                         opensavedir, currlayer->currname, filetypes,
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT );
    int button = savedlg.ShowModal();
    if (viewptr) viewptr->ResetMouseDown();
    if (button == wxID_OK) {
        wxFileName fullpath( savedlg.GetPath() );
        opensavedir = fullpath.GetPath();
        wxString ext = fullpath.GetExt();
        pattern_format format;
        output_compression compression = no_compression;
        // detect if user supplied a compression suffix (.gz)
        if ( ext.IsSameAs(wxT("gz"),false) ) {
            compression = gzip_compression;
            ext = wxFileName(fullpath.GetName()).GetExt();
        }
        // if user supplied a known extension then use that format if it is
        // allowed, otherwise use current format specified in filter menu
        if ( ext.IsSameAs(wxT("rle"),false) && RLEindex >= 0 ) {
            format = savexrle ? XRLE_format : RLE_format;
        } else if ( ext.IsSameAs(wxT("mc"),false) && MCindex >= 0 ) {
            format = MC_format;
        } else if ( savedlg.GetFilterIndex()/2 == MCindex ) {
            format = MC_format;
            if (savedlg.GetFilterIndex()%2) compression = gzip_compression;
        } else if ( savedlg.GetFilterIndex()/2 == RLEindex ) {
            format = savexrle ? XRLE_format : RLE_format;
            if (savedlg.GetFilterIndex()%2) compression = gzip_compression;
        } else {
            statusptr->ErrorMessage(_("Bug in SavePattern!"));
            return false;
        }
        
        const char* err = WritePattern(savedlg.GetPath(), format, compression,
                                       itop, ileft, ibottom, iright);
        if (err) {
            statusptr->ErrorMessage(wxString(err,wxConvLocal));
        } else {
            statusptr->DisplayMessage(_("Pattern saved in file: ") + savedlg.GetPath());
            AddRecentPattern(savedlg.GetPath());
            SaveSucceeded(savedlg.GetPath());
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------

// called by script command to save current pattern to given file
const char* MainFrame::SaveFile(const wxString& path, const wxString& fileformat, bool remember)
{
    bigint top, left, bottom, right;
    int itop, ileft, ibottom, iright;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    
    wxString format = fileformat.Lower();
    output_compression compression = no_compression;
    if (format.EndsWith(wxT(".gz"))) {
        compression = gzip_compression;
    }
    
    // check that given file format is valid
    pattern_format pattfmt;
    if (format.StartsWith(wxT("rle"))) {
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            return "Pattern is too big to save as RLE.";
        }
        pattfmt = savexrle ? XRLE_format : RLE_format;
        itop = top.toint();
        ileft = left.toint();
        ibottom = bottom.toint();
        iright = right.toint();
    } else if (format.StartsWith(wxT("mc"))) {
        if (!currlayer->algo->hyperCapable()) {
            return "Macrocell format is not supported by the current algorithm.";
        }
        pattfmt = MC_format;
        // writepattern will ignore itop, ileft, ibottom, iright
        itop = ileft = ibottom = iright = 0;
    } else {
        return "Unknown pattern format.";
    }
    
    const char* err = WritePattern(path, pattfmt, compression,
                                   itop, ileft, ibottom, iright);
    if (!err) {
        if (remember) AddRecentPattern(path);
        SaveSucceeded(path);
    }
    
    return err;
}

// -----------------------------------------------------------------------------

void MainFrame::SaveSucceeded(const wxString& path)
{
    // save old info for RememberNameChange
    wxString oldname = currlayer->currname;
    wxString oldfile = currlayer->startfile;
    bool oldsave = currlayer->savestart;
    bool olddirty = currlayer->dirty;
    
    if (allowundo && !currlayer->stayclean && inscript) {
        SavePendingChanges();
    }
    
    if ( currlayer->algo->getGeneration() == currlayer->startgen ) {
        // no need to save starting pattern (ResetPattern can load startfile)
        currlayer->savestart = false;
        currlayer->startfile = path;
        currlayer->currfile = path;
    }
    
    // set dirty flag false and update currlayer->currname
    MarkLayerClean(GetBaseName(path));
    
    if (allowundo && !currlayer->stayclean) {
        currlayer->undoredo->RememberNameChange(oldname, oldfile, oldsave, olddirty);
    }
}

// -----------------------------------------------------------------------------

void MainFrame::SaveOverlay()
{
    if (showoverlay && curroverlay->GetOverlayData()) {
        wxFileDialog savedlg(this, _("Save overlay as PNG file"),
                             overlaydir, _("overlay.png"), _("PNG (*.png)|*.png"),
                             wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        int button = savedlg.ShowModal();
        if (viewptr) viewptr->ResetMouseDown();
        if (button == wxID_OK) {
            wxString pngpath = savedlg.GetPath();
            wxFileName fullpath(pngpath);
            overlaydir = fullpath.GetPath();
            curroverlay->SaveOverlay(pngpath);
        }
    }
}

// -----------------------------------------------------------------------------

static wxString oldfiledir;

void MainFrame::ToggleShowFiles()
{
    if (splitwin->IsSplit()) dirwinwd = splitwin->GetSashPosition();
    showfiles = !showfiles;
    if (splitwin->IsSplit()) {
        // hide left pane
        splitwin->Unsplit(filectrl);
        oldfiledir = filedir;
    } else {
        splitwin->SplitVertically(filectrl, RightPane(), dirwinwd);
    }
    if (showfiles && filedir != oldfiledir) {
        // show new file directory
        BuildTree();
    }
    viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void MainFrame::SetFolder()
{
    wxDirDialog dirdlg(this, _("Choose a new file folder"), filedir, wxDD_NEW_DIR_BUTTON);
    int button = dirdlg.ShowModal();
    if (viewptr) viewptr->ResetMouseDown();
    if (button == wxID_OK) {
        SetFileDir(dirdlg.GetPath());
    }
}

// -----------------------------------------------------------------------------

void MainFrame::SetFileDir(const wxString& newdir)
{
    if (filedir != newdir) {
        filedir = newdir;
        if (showfiles) {
            // show new file directory
            BuildTree();
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ShowPrefsDialog(const wxString& page)
{
    if (viewptr->waitingforclick) return;
    
    if (generating) {
        command_pending = true;
        cmdevent.SetId(wxID_PREFERENCES);
        Stop();
        return;
    }
    
    if (inscript) {
        // safe to allow prefs dialog while script is running???
        // if so, maybe we need some sort of warning like this:
        // Warning(_("The currently running script might clobber any changes you make."));
    }
    
    int oldtileborder = tileborder;
    int oldcontrolspos = controlspos;
    
    if (ChangePrefs(page)) {
        // user hit OK button
        
        // if maxpatterns was reduced then we may need to remove some paths
        while (numpatterns > maxpatterns) {
            numpatterns--;
            patternSubMenu->Delete( patternSubMenu->FindItemByPosition(numpatterns) );
        }
        
        // if maxscripts was reduced then we may need to remove some paths
        while (numscripts > maxscripts) {
            numscripts--;
            scriptSubMenu->Delete( scriptSubMenu->FindItemByPosition(numscripts) );
        }
        
        // randomfill might have changed
        SetRandomFillPercentage();
        
        // if mindelay/maxdelay changed then may need to change minexpo and currexpo
        UpdateStepExponent();
        
        // maximum memory might have changed
        for (int i = 0; i < numlayers; i++) {
            Layer* layer = GetLayer(i);
            AlgoData* ad = algoinfo[layer->algtype];
            if (ad->algomem >= 0)
                layer->algo->setMaxMemory(ad->algomem);
        }
        
        // tileborder might have changed
        if (tilelayers && numlayers > 1 && tileborder != oldtileborder) {
            int wd, ht;
            bigview->GetClientSize(&wd, &ht);
            wd = wd * int(scalefactor);
            ht = ht * int(scalefactor);
            // wd or ht might be < 1 on Windows
            if (wd < 1) wd = 1;
            if (ht < 1) ht = 1;
            ResizeLayers(wd, ht);
        }
        
        // position of translucent controls might have changed
        if (controlspos != oldcontrolspos) {
            int wd, ht;
            if (tilelayers && numlayers > 1) {
                for (int i = 0; i < numlayers; i++) {
                    Layer* layer = GetLayer(i);
                    layer->tilewin->GetClientSize(&wd, &ht);
                    wd = wd * int(scalefactor);
                    ht = ht * int(scalefactor);
                    layer->tilewin->SetViewSize(wd, ht);
                }
            }
            bigview->GetClientSize(&wd, &ht);
            wd = wd * int(scalefactor);
            ht = ht * int(scalefactor);
            bigview->SetViewSize(wd, ht);
        }
        
        SavePrefs();
    }
    
    // safer to update everything even if user hit Cancel
    UpdateEverything();
}

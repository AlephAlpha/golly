This file contains instructions for how to build Golly, a simulator
for Conway's Game of Life and other cellular automata.  For the latest
news please visit our web site:

   http://golly.sourceforge.net/


How to install wxWidgets
------------------------

If you want to build Golly from source code then you'll have to install
wxWidgets first.  Visit http://www.wxwidgets.org/downloads/ and grab
the appropriate source archive for your platform:

   wxMSW   - for Windows (get the installer)
   wxMac   - for Mac OS X
   wxGTK   - for Linux with GTK+

Golly should compile with wxWidgets 2.7.0 or later, but it's best to
use the latest stable version.


Building wxWidgets on Windows
-----------------------------

If you've installed wxWidgets to (say) C:\wxWidgets 
then, from a Visual Studio command prompt:

   cd \wxWidgets\build\msw
   nmake -f makefile.vc BUILD=release RUNTIME_LIBS=static UNICODE=1 DEBUG_INFO=0 DEBUG_FLAG=0

or, on 64-bit Windows:  (from a Visual Studio 64-bit command prompt)

   nmake -f makefile.vc BUILD=release RUNTIME_LIBS=static UNICODE=1 DEBUG_INFO=0 DEBUG_FLAG=0 TARGET_CPU=AMD64

   
Building wxWidgets on Mac OS X
------------------------------

Unpack the wxMac source archive wherever you like, start up Terminal
and type these commands (using the correct version number):

   cd /path/to/wxMac-2.8.0
   mkdir build-osx
   cd build-osx
   ../configure --with-mac --disable-shared --enable-unicode \
                --enable-universal_binary
   make


Building wxWidgets on Linux with GTK+
-------------------------------------

Unpack the wxGTK source archive wherever you like, start up a terminal
session and type these commands (using the correct version number):

   cd /path/to/wxGTK-2.8.0
   mkdir build-gtk
   cd build-gtk
   ../configure --with-gtk --disable-shared --enable-unicode
   make
   sudo make install
   sudo ldconfig

This installs the wx libraries in a suitable directory.  It also
installs the wx-config program which will be called by makefile-gtk
to set the appropriate compile and link options for building Golly.


How to install Perl and Python
------------------------------

Golly uses Perl and Python for scripting, so you'll need to make
sure both are installed.  Mac OS X users don't have to do anything
because Perl and Python are already installed.

If you are running Linux, you probably have Perl installed.
Type "perl -v" at the command line to print out the version.
Golly's code should compile happily with Perl 5.8.x or later, but
support for interpreter threads is required.

Windows users are advised to download the ActivePerl installer
from http://www.activestate.com/Products/ActivePerl/.

Windows and Linux users can download a Python installer from
http://www.python.org/download/.


How to build Golly
------------------

Once wxWidgets, Perl and Python are installed, building Golly should
be easy:

On 32-bit Windows: (from a Visual Studio command prompt)

   nmake -f makefile-win BUILD=release RUNTIME_LIBS=static UNICODE=1 DEBUG_INFO=0 DEBUG_FLAG=0

On 64-bit Windows: (from a Visual Studio 64-bit command prompt)

   nmake -f makefile-win BUILD=release RUNTIME_LIBS=static UNICODE=1 DEBUG_INFO=0 DEBUG_FLAG=0 TARGET_CPU=AMD64
   
On Mac OS 10.4 or later:

   make -f makefile-mac
   
On Linux with GTK+:
   
   make -f makefile-gtk

Important notes:

- You first need to edit local-win.mk and makefile-mac to specify where
  wxWidgets is installed.  Change the WX_DIR path near the start of
  the file.  Also make sure WX_RELEASE specifies the first two digits
  of your wxWidgets version.

- In local-win.mk you need to include the headers for Perl and Python,
  so change the paths for PERL_INCLUDE and PYTHON_INCLUDE if necessary.
  
- On Linux you may need to add some development packages. For example, 
  from a default Ubuntu install (at the time of writing) you will need
  to install the following packages: libgtk2.0-dev, python2.6-dev (for 
  GTK and Python respectively).

- On Linux the CXXFLAGS and LDFLAGS environmental variables may be
  used to append to (and override) the package default flags.
  Additionally, GOLLYDIR specifies an absolute directory path to look
  for the application data files.  For system-wide installation, it
  probably makes sense to set GOLLYDIR to /usr/share/golly and install
  the Help, Patterns, Scripts and Rules directories in there.

Aternative methods for building Golly are also available, as discussed
in the following two sections.


Building Golly using configure
------------------------------

Golly can be built using the GNU build system by first running the
configure script to create an appropriate Makefile for your operating
system and then running make to build Golly.  For example, for a
system-wide installation, you might run:

   ./configure
   make
   make install

The configure script offers various options to customize building and
installation. For an overview of the available options, run:

   ./configure --help

If you obtained the Golly source code from the CVS repository instead
of a source release, you need to generate the configure script first,
by running:

   ./autogen.sh

Note that this requires autoconf, automake and Perl 5 are installed.


Building Golly using CMake
--------------------------

The CMakeLists.txt file included in the source distribution allows
Golly to be built using CMake.  Visit http://www.cmake.org/ to
download a suitable installer for your operating system.

Once CMake is installed, you can build Golly using these commands:

   cd /path/to/golly/src     (location of CMakeLists.txt)
   mkdir cmakedir            (use any name for the directory)
   cd cmakedir
   cmake ..
   make                      (or nmake on Windows)

CMake also comes with a GUI application if you'd prefer not to
use the command line.

Notes:

- Although wxWidgets comes pre-installed on Mac OS X, it tends to be
  out-of-date and inappropriate for building Golly, so CMakeLists.txt
  sets wxWidgets_CONFIG_EXECUTABLE to /usr/local/bin/wx-config
  where /usr/local/bin is the default location for wx-config if you
  do "sudo make install" after building the wxWidgets libraries.

- Also on Mac OS X, PERL_INCLUDE_PATH and PERL_LIBRARY are overridden
  to avoid statically linking the Perl library.  Their settings assume
  Perl 5.10 so you might need to change the version numbers.

- On Linux, the zlib library is statically linked and its location is
  set to /usr/lib/libz.a, so you might need to change that path.


How to build bgolly (the batch mode version)
--------------------------------------------

Golly can also be run as a command line program without any GUI.
To build this "batch mode" version, just specify bgolly as the target
of the make command.  Note that you don't need to install wxWidgets,
Perl or Python to build bgolly.


Source code road map
--------------------

If you'd like to modify Golly then the following notes should help you
get started.  Each module is described in (roughly) top-down order,
and some key routines are mentioned.

The GUI code is implemented in these wx* modules:

wxgolly.*

   Defines the GollyApp class.
   GollyApp::OnInit() is where it all starts.

wxmain.*

   Defines the MainFrame class for the main window.
   MainFrame::OnMenu() handles all menu commands.
   MainFrame::UpdateEverything() updates all parts of the GUI.

wxfile.cpp

   Implements various File menu functions.
   MainFrame::NewPattern() creates a new, empty universe.
   MainFrame::LoadPattern() reads in a pattern file.

wxcontrol.cpp

   Implements various Control menu functions.
   MainFrame::GeneratePattern() runs the current pattern.
   MainFrame::ChangeAlgorithm() switches to a new algorithm.

wxtimeline.*

   Users can record/play a sequence of steps called a "timeline".
   CreateTimelineBar() creates timeline bar below the viewport window.
   StartStopRecording() starts or stops recording a timeline.
   DeleteTimeline() deletes an existing timeline.

wxrule.*

   Users can change the current rule.
   ChangeRule() opens the Set Rule dialog.

wxedit.*

   Implements edit bar functions.
   CreateEditBar() creates the edit bar above the viewport window.
   ToggleEditBar() shows/hides the edit bar.

wxselect.*

   Defines the Selection class for operations on selections.
   Selection::CopyToClipboard() copies the selection to the clipboard.
   Selection::RandomFill() randomly fills the current selection.
   Selection::Rotate() rotates the current selection.
   Selection::Flip() flips the current selection.

wxview.*

   Defines the PatternView class for the viewport window.
   PatternView::ProcessKey() processes keyboard shortcuts.
   PatternView::ProcessClick() processes mouse clicks.

wxrender.*

   Implements rendering routines for updating the viewport.
   DrawView() draws the pattern, grid lines, selection, etc.

wxalgos.*

   Implements support for multiple algorithms.
   InitAlgorithms() initializes all algorithms and algoinfo data.
   CreateNewUniverse() creates a new universe of given type.

wxlayer.*

   Defines the Layer class and implements Layer menu functions.
   AddLayer() adds a new, empty layer.
   DeleteLayer() deletes the current layer.
   SetLayerColors() lets user change the current layer's colors.

wxundo.*

   Defines the UndoRedo class for unlimited undo/redo.
   UndoRedo::RememberCellChanges() saves cell state changes.
   UndoRedo::UndoChange() undoes a recent change.
   UndoRedo::RedoChange() redoes an undone change.

wxstatus.*

   Implements a status bar at the top of the main window.
   StatusBar::DrawStatusBar() shows gen count, pop count, etc.
   StatusBar::DisplayMessage() shows message in bottom line.

wxhelp.*

   Implements a modeless help window.
   ShowHelp() displays a given .html file.

wxinfo.*

   Implements a modeless info window.
   ShowInfo() displays the comments in a given pattern file.

wxscript.*

   Implements the high-level scripting interface.
   RunScript() runs a given script file.

wxperl.*

   Implements Perl script support.
   RunPerlScript() runs a given .pl file.

wxpython.*

   Implements Python script support.
   RunPythonScript() runs a given .py file.

wxprefs.*

   Routines for loading, saving and changing user preferences.
   GetPrefs() loads data from GollyPrefs file.
   SavePrefs() writes data to GollyPrefs file.
   ChangePrefs() opens the Preferences dialog.

wxutils.*

   Implements various utility routines.
   Warning() displays message in modal dialog.
   Fatal() displays message and exits app.

The following non-wx modules are sufficient to build bgolly.
They also define abstract viewport and rendering interfaces used
by the above GUI code:

bgolly.cpp

   The batch mode program.
   main() does all the option parsing.

platform.h

   Platform specific defines (eg. 64-bit changes).

lifealgo.*

   Defines abstract Life algorithm operations:
   lifealgo::setcell() sets given cell to given state.
   lifealgo::getcell() gets state of given cell.
   lifealgo::nextcell() finds next live cell in current row.
   lifealgo::step() advances pattern by current increment.
   lifealgo::fit() fits pattern within given viewport.
   lifealgo::draw() renders pattern in given viewport.

liferules.*

   Defines routines for setting/getting rules.
   liferules::setrule() parses and validates a given rule string.
   liferules::getrule() returns the current rule in canonical form.

lifepoll.*

   Allows lifealgo routines to do event processing.
   lifepoll::checkevents() processes any pending events.

viewport.*

   Defines abstract viewport operations:
   viewport::zoom() zooms into a given location.
   viewport::unzoom() zooms out from a given location.
   viewport::setmag() sets the magnification.
   viewport::move() scrolls view by given number of pixels.

liferender.*

   Defines abstract routines for rendering a pattern:
   liferender::killrect() fills an area with the dead cell color.
   liferender::pixblit() draws an area with at least one live cell.

qlifealgo.*

   Implements QuickLife, a fast, conventional algorithm.

hlifealgo.*

   Implements HashLife, a super fast hashing algorithm.

ghashbase.*

   Defines an abstract class so other algorithms can use hashlife
   in a multi-state universe.

generationsalgo.*

   Implements the Generations family of rules.

jvnalgo.*

   Implements John von Neumann's 29-state CA as well as some
   32-state variants by Renato Nobili and Tim Hutton.

ruletable_algo.*

   Implements the RuleTable algorithm which loads externally
   specified rules stored in .table files.

ruletreealgo.*

   Implements the RuleTree algorithm which loads externally
   specified rules stored in .tree files.

qlifedraw.cpp

   Implements rendering routines for QuickLife.

hlifedraw.cpp

   Implements rendering routines for HashLife.

ghashdraw.cpp

   Implements rendering routines for all algos that use ghashbase.

readpattern.*

   Reads pattern files in a variety of formats.
   readpattern() loads a pattern into the given universe.
   readcomments() extracts comments from the given file.

writepattern.*

   Saves the current pattern in a file.
   writepattern() saves the pattern in a specified format.

bigint.*

   Implements operations on arbitrarily large integers.

util.*

   Utilities for displaying errors and progress info.
   warning() displays error message.
   fatal() displays error message and exits.


Have fun, and please let us know if you make any changes!

Andrew Trevorrow <andrew@trevorrow.com>
Tomas Rokicki <rokicki@gmail.com>

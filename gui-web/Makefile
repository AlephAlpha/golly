# Makefile for creating web app version of Golly using Emscripten.
# Type "emmake make" to build golly.html, golly.js, golly.data, golly.wasm.
# 
# To test webGolly in Chrome/Firefox:
# in Terminal: python -m SimpleHTTPServer 8888
# in browser: localhost:8888/golly.html

SOURCES = \
    ../gollybase/bigint.cpp \
    ../gollybase/generationsalgo.cpp \
    ../gollybase/ghashbase.cpp \
    ../gollybase/ghashdraw.cpp \
    ../gollybase/hlifealgo.cpp \
    ../gollybase/hlifedraw.cpp \
    ../gollybase/jvnalgo.cpp \
    ../gollybase/lifealgo.cpp \
    ../gollybase/lifepoll.cpp \
    ../gollybase/liferender.cpp \
    ../gollybase/liferules.cpp \
    ../gollybase/ltlalgo.cpp \
    ../gollybase/ltldraw.cpp \
    ../gollybase/qlifealgo.cpp \
    ../gollybase/qlifedraw.cpp \
    ../gollybase/readpattern.cpp \
    ../gollybase/ruleloaderalgo.cpp \
    ../gollybase/ruletable_algo.cpp \
    ../gollybase/ruletreealgo.cpp \
    ../gollybase/superalgo.cpp \
    ../gollybase/util.cpp \
    ../gollybase/viewport.cpp \
    ../gollybase/writepattern.cpp \
    ../gui-common/algos.cpp \
    ../gui-common/control.cpp \
    ../gui-common/file.cpp \
    ../gui-common/layer.cpp \
    ../gui-common/prefs.cpp \
    ../gui-common/render.cpp \
    ../gui-common/select.cpp \
    ../gui-common/status.cpp \
    ../gui-common/undo.cpp \
    ../gui-common/utils.cpp \
    ../gui-common/view.cpp \
    ../gui-common/MiniZip/ioapi.c \
    ../gui-common/MiniZip/unzip.c \
    ../gui-common/MiniZip/zip.c \
    zlib/adler32.c \
    zlib/compress.c \
    zlib/crc32.c \
    zlib/deflate.c \
    zlib/gzclose.c \
    zlib/gzlib.c \
    zlib/gzread.c \
    zlib/gzwrite.c \
    zlib/infback.c \
    zlib/inffast.c \
    zlib/inflate.c \
    zlib/inftrees.c \
    zlib/trees.c \
    zlib/uncompr.c \
    zlib/zutil.c \
    webcalls.cpp \
    main.cpp

OBJECTS = \
    ../gollybase/bigint.o \
    ../gollybase/generationsalgo.o \
    ../gollybase/ghashbase.o \
    ../gollybase/ghashdraw.o \
    ../gollybase/hlifealgo.o \
    ../gollybase/hlifedraw.o \
    ../gollybase/jvnalgo.o \
    ../gollybase/lifealgo.o \
    ../gollybase/lifepoll.o \
    ../gollybase/liferender.o \
    ../gollybase/liferules.o \
    ../gollybase/ltlalgo.o \
    ../gollybase/ltldraw.o \
    ../gollybase/qlifealgo.o \
    ../gollybase/qlifedraw.o \
    ../gollybase/readpattern.o \
    ../gollybase/ruleloaderalgo.o \
    ../gollybase/ruletable_algo.o \
    ../gollybase/ruletreealgo.o \
    ../gollybase/superalgo.o \
    ../gollybase/util.o \
    ../gollybase/viewport.o \
    ../gollybase/writepattern.o \
    ../gui-common/algos.o \
    ../gui-common/control.o \
    ../gui-common/file.o \
    ../gui-common/layer.o \
    ../gui-common/prefs.o \
    ../gui-common/render.o \
    ../gui-common/select.o \
    ../gui-common/status.o \
    ../gui-common/undo.o \
    ../gui-common/utils.o \
    ../gui-common/view.o \
    ../gui-common/MiniZip/ioapi.o \
    ../gui-common/MiniZip/unzip.o \
    ../gui-common/MiniZip/zip.o \
    zlib/adler32.o \
    zlib/compress.o \
    zlib/crc32.o \
    zlib/deflate.o \
    zlib/gzclose.o \
    zlib/gzlib.o \
    zlib/gzread.o \
    zlib/gzwrite.o \
    zlib/infback.o \
    zlib/inffast.o \
    zlib/inflate.o \
    zlib/inftrees.o \
    zlib/trees.o \
    zlib/uncompr.o \
    zlib/zutil.o \
    webcalls.o \
    main.o

CFLAGS = -Izlib -Wall -Wno-deprecated-non-prototype -Wno-implicit-function-declaration -O2

CXXFLAGS = -Izlib -I. -I../gollybase -I../gui-common -I../gui-common/MiniZip \
    -DVERSION=4.3 -DZLIB -DWEB_GUI -Wall -Wextra -Wno-unused-parameter -Wno-deprecated \
    -Wno-unused-but-set-variable -O2

TARGET = golly.html

all: $(TARGET)

$(TARGET): $(OBJECTS) jslib.js shell.html
	$(CXX) $(CXXFLAGS) --js-library jslib.js --shell-file shell.html \
    --preload-file ../Patterns@/Patterns \
    --preload-file ../Rules@/Rules \
    --preload-file Help \
    --preload-file ../gui-common/Help/intro.html@/Help/intro.html \
    --preload-file ../gui-common/Help/tips.html@/Help/tips.html \
    --preload-file ../gui-common/Help/archives.html@/Help/archives.html \
    --preload-file ../gui-common/Help/refs.html@/Help/refs.html \
    --preload-file ../gui-common/Help/formats.html@/Help/formats.html \
    --preload-file ../gui-common/Help/bounded.html@/Help/bounded.html \
    --preload-file ../gui-common/Help/credits.html@/Help/credits.html \
    --preload-file ../gui-common/Help/algos.html@/Help/algos.html \
    --preload-file ../gui-common/Help/Algorithms@/Help/Algorithms \
    --preload-file ../gui-common/Help/Lexicon@/Help/Lexicon \
    -s TOTAL_MEMORY=320mb \
    -s EXPORTED_FUNCTIONS="[ '_main', '_ClearStatus', '_NewUniverse', '_ResizeCanvas', \
    '_SetViewport', '_StartStop', '_Next', '_Step', '_GoSlower', '_GoFaster', '_StepBy1', \
    '_Undo', '_Redo', '_Fit', '_ZoomOut', '_ZoomIn', '_Scale1to1', '_Reset', \
    '_ToggleIcons', '_ModeChanged', '_StateChanged', '_DoMenuItem', '_UpdateMenuItems', \
    '_Paste', '_PasteAction', '_SelectionAction', '_OpenClickedFile', '_OpenClipboard', \
    '_ToggleAutoFit', '_OverCanvas', '_OnKeyChanged', '_OnMouseWheel', '_Info', '_CloseInfo', \
    '_Help', '_DoHelpClick', '_HelpBack', '_HelpNext', '_HelpContents', '_CloseHelp', \
    '_FileCreated', '_CancelProgress', '_CancelOpen', '_SaveFile', '_CancelSave', \
    '_StorePrefs', '_CancelPrefs', '_SaveLocalPrefs', '_FullScreen', '_ExitFullScreen', \
    '_UnsavedChanges' ]" \
    -s EXPORTED_RUNTIME_METHODS="[ 'ccall', 'cwrap' ]" \
    -o $@ $(LDFLAGS) $(OBJECTS)

# Include these link-time settings above to catch errors:
# --profiling -s ASSERTIONS=2 -s SAFE_HEAP=1

clean:
	$(RM) $(OBJECTS) $(TARGET) golly.js golly.data golly.wasm

depend:
	@$(CXX) $(CXXFLAGS) -MM $(SOURCES)

# type "emmake make depend" to generate these dependencies:

bigint.o: ../gollybase/bigint.cpp ../gollybase/bigint.h \
  ../gollybase/util.h
generationsalgo.o: ../gollybase/generationsalgo.cpp \
  ../gollybase/generationsalgo.h ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
ghashbase.o: ../gollybase/ghashbase.cpp ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
ghashdraw.o: ../gollybase/ghashdraw.cpp ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
hlifealgo.o: ../gollybase/hlifealgo.cpp ../gollybase/hlifealgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
hlifedraw.o: ../gollybase/hlifedraw.cpp ../gollybase/hlifealgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
jvnalgo.o: ../gollybase/jvnalgo.cpp ../gollybase/jvnalgo.h \
  ../gollybase/ghashbase.h ../gollybase/lifealgo.h ../gollybase/bigint.h \
  ../gollybase/viewport.h ../gollybase/liferender.h \
  ../gollybase/lifepoll.h ../gollybase/readpattern.h \
  ../gollybase/platform.h ../gollybase/liferules.h ../gollybase/util.h
lifealgo.o: ../gollybase/lifealgo.cpp ../gollybase/lifealgo.h \
  ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h ../gollybase/util.h
lifepoll.o: ../gollybase/lifepoll.cpp ../gollybase/lifepoll.h \
  ../gollybase/util.h
liferender.o: ../gollybase/liferender.cpp ../gollybase/liferender.h \
  ../gollybase/util.h
liferules.o: ../gollybase/liferules.cpp ../gollybase/liferules.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h ../gollybase/util.h
ltlalgo.o: ../gollybase/ltlalgo.cpp ../gollybase/ltlalgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
ltldraw.o: ../gollybase/ltldraw.cpp ../gollybase/ltlalgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
qlifealgo.o: ../gollybase/qlifealgo.cpp ../gollybase/qlifealgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
qlifedraw.o: ../gollybase/qlifedraw.cpp ../gollybase/qlifealgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
readpattern.o: ../gollybase/readpattern.cpp ../gollybase/readpattern.h \
  ../gollybase/bigint.h ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/platform.h ../gollybase/liferules.h ../gollybase/util.h \
  zlib/zlib.h zlib/zconf.h
ruleloaderalgo.o: ../gollybase/ruleloaderalgo.cpp \
  ../gollybase/ruleloaderalgo.h ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h \
  ../gollybase/ruletable_algo.h ../gollybase/ruletreealgo.h
ruletable_algo.o: ../gollybase/ruletable_algo.cpp \
  ../gollybase/ruletable_algo.h ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
ruletreealgo.o: ../gollybase/ruletreealgo.cpp ../gollybase/ruletreealgo.h \
  ../gollybase/ghashbase.h ../gollybase/lifealgo.h ../gollybase/bigint.h \
  ../gollybase/viewport.h ../gollybase/liferender.h \
  ../gollybase/lifepoll.h ../gollybase/readpattern.h \
  ../gollybase/platform.h ../gollybase/liferules.h ../gollybase/util.h
superalgo.o: ../gollybase/superalgo.cpp ../gollybase/superalgo.h \
  ../gollybase/ghashbase.h ../gollybase/lifealgo.h ../gollybase/bigint.h \
  ../gollybase/viewport.h ../gollybase/liferender.h \
  ../gollybase/lifepoll.h ../gollybase/readpattern.h \
  ../gollybase/platform.h ../gollybase/liferules.h ../gollybase/util.h
util.o: ../gollybase/util.cpp ../gollybase/util.h
viewport.o: ../gollybase/viewport.cpp ../gollybase/viewport.h \
  ../gollybase/bigint.h ../gollybase/lifealgo.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h
writepattern.o: ../gollybase/writepattern.cpp ../gollybase/writepattern.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h ../gollybase/util.h \
  zlib/zlib.h zlib/zconf.h
algos.o: ../gui-common/algos.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/util.h \
  ../gollybase/generationsalgo.h ../gollybase/ghashbase.h \
  ../gollybase/ltlalgo.h ../gollybase/jvnalgo.h ../gollybase/superalgo.h \
  ../gollybase/ruleloaderalgo.h ../gollybase/ruletable_algo.h \
  ../gollybase/ruletreealgo.h ../gui-common/utils.h \
  ../gui-common/prefs.h ../gui-common/layer.h ../gui-common/algos.h \
  ../gui-common/select.h
control.o: ../gui-common/control.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/util.h \
  ../gollybase/writepattern.h ../gui-common/utils.h \
  ../gui-common/prefs.h ../gui-common/status.h ../gui-common/file.h \
  ../gui-common/algos.h ../gui-common/layer.h ../gui-common/select.h \
  ../gui-common/view.h ../gui-common/undo.h ../gui-common/control.h \
  webcalls.h
file.o: ../gui-common/file.cpp webcalls.h ../gui-common/MiniZip/zip.h \
  zlib/zlib.h zlib/zconf.h ../gui-common/MiniZip/ioapi.h \
  ../gui-common/MiniZip/unzip.h ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/util.h \
  ../gollybase/ruleloaderalgo.h ../gollybase/ghashbase.h \
  ../gollybase/ruletable_algo.h ../gollybase/ruletreealgo.h \
  ../gollybase/writepattern.h ../gui-common/utils.h \
  ../gui-common/prefs.h ../gui-common/status.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/control.h \
  ../gui-common/view.h ../gui-common/undo.h ../gui-common/file.h
layer.o: ../gui-common/layer.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/util.h ../gui-common/utils.h \
  ../gui-common/prefs.h ../gui-common/algos.h ../gui-common/control.h \
  ../gui-common/select.h ../gui-common/file.h \
  ../gollybase/writepattern.h ../gui-common/view.h ../gui-common/undo.h \
  ../gui-common/layer.h
prefs.o: ../gui-common/prefs.cpp ../gollybase/lifealgo.h \
  ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h ../gollybase/util.h \
  ../gui-common/utils.h ../gui-common/status.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/prefs.h \
  webcalls.h
render.o: ../gui-common/render.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gui-common/utils.h ../gui-common/prefs.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/view.h \
  ../gui-common/render.h
select.o: ../gui-common/select.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gui-common/utils.h ../gui-common/prefs.h ../gui-common/status.h \
  ../gui-common/undo.h ../gui-common/select.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/view.h ../gui-common/control.h \
  ../gui-common/file.h ../gollybase/writepattern.h webcalls.h
status.o: ../gui-common/status.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gui-common/utils.h ../gui-common/prefs.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/view.h \
  ../gui-common/status.h webcalls.h
undo.o: ../gui-common/undo.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/writepattern.h ../gui-common/select.h \
  ../gui-common/utils.h ../gui-common/view.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/prefs.h ../gui-common/control.h \
  ../gui-common/file.h ../gui-common/undo.h
utils.o: ../gui-common/utils.cpp ../gollybase/lifepoll.h \
  ../gollybase/util.h ../gui-common/prefs.h ../gui-common/utils.h \
  webcalls.h
view.o: ../gui-common/view.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/util.h ../gui-common/utils.h \
  ../gui-common/prefs.h ../gui-common/status.h ../gui-common/render.h \
  ../gui-common/undo.h ../gui-common/select.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/control.h ../gui-common/file.h \
  ../gollybase/writepattern.h ../gui-common/view.h webcalls.h
ioapi.o: ../gui-common/MiniZip/ioapi.c zlib/zlib.h zlib/zconf.h \
  ../gui-common/MiniZip/ioapi.h
unzip.o: ../gui-common/MiniZip/unzip.c zlib/zlib.h zlib/zconf.h \
  ../gui-common/MiniZip/unzip.h ../gui-common/MiniZip/ioapi.h \
  ../gui-common/MiniZip/crypt.h
zip.o: ../gui-common/MiniZip/zip.c zlib/zlib.h zlib/zconf.h \
  ../gui-common/MiniZip/zip.h ../gui-common/MiniZip/ioapi.h \
  ../gui-common/MiniZip/crypt.h
adler32.o: zlib/adler32.c zlib/zutil.h zlib/zlib.h zlib/zconf.h
compress.o: zlib/compress.c zlib/zlib.h zlib/zconf.h
crc32.o: zlib/crc32.c zlib/zutil.h zlib/zlib.h zlib/zconf.h zlib/crc32.h
deflate.o: zlib/deflate.c zlib/deflate.h zlib/zutil.h zlib/zlib.h \
  zlib/zconf.h
gzclose.o: zlib/gzclose.c zlib/gzguts.h zlib/zlib.h zlib/zconf.h
gzlib.o: zlib/gzlib.c zlib/gzguts.h zlib/zlib.h zlib/zconf.h
gzread.o: zlib/gzread.c zlib/gzguts.h zlib/zlib.h zlib/zconf.h
gzwrite.o: zlib/gzwrite.c zlib/gzguts.h zlib/zlib.h zlib/zconf.h
infback.o: zlib/infback.c zlib/zutil.h zlib/zlib.h zlib/zconf.h \
  zlib/inftrees.h zlib/inflate.h zlib/inffast.h zlib/inffixed.h
inffast.o: zlib/inffast.c zlib/zutil.h zlib/zlib.h zlib/zconf.h \
  zlib/inftrees.h zlib/inflate.h zlib/inffast.h
inflate.o: zlib/inflate.c zlib/zutil.h zlib/zlib.h zlib/zconf.h \
  zlib/inftrees.h zlib/inflate.h zlib/inffast.h zlib/inffixed.h
inftrees.o: zlib/inftrees.c zlib/zutil.h zlib/zlib.h zlib/zconf.h \
  zlib/inftrees.h
trees.o: zlib/trees.c zlib/deflate.h zlib/zutil.h zlib/zlib.h \
  zlib/zconf.h zlib/trees.h
uncompr.o: zlib/uncompr.c zlib/zlib.h zlib/zconf.h
zutil.o: zlib/zutil.c zlib/zutil.h zlib/zlib.h zlib/zconf.h zlib/gzguts.h
webcalls.o: webcalls.cpp ../gollybase/util.h ../gui-common/utils.h \
  ../gui-common/algos.h ../gollybase/lifealgo.h ../gollybase/bigint.h \
  ../gollybase/viewport.h ../gollybase/liferender.h \
  ../gollybase/lifepoll.h ../gollybase/readpattern.h \
  ../gollybase/platform.h ../gui-common/prefs.h ../gui-common/layer.h \
  ../gui-common/select.h ../gui-common/control.h ../gui-common/file.h \
  ../gollybase/writepattern.h ../gui-common/view.h \
  ../gui-common/status.h ../gui-common/undo.h webcalls.h
main.o: main.cpp ../gollybase/bigint.h ../gollybase/lifealgo.h \
  ../gollybase/viewport.h ../gollybase/liferender.h \
  ../gollybase/lifepoll.h ../gollybase/readpattern.h \
  ../gollybase/platform.h ../gollybase/qlifealgo.h \
  ../gollybase/liferules.h ../gollybase/hlifealgo.h ../gollybase/util.h \
  ../gollybase/generationsalgo.h ../gollybase/ghashbase.h \
  ../gollybase/ltlalgo.h ../gollybase/jvnalgo.h ../gollybase/superalgo.h \
  ../gollybase/ruleloaderalgo.h ../gollybase/ruletable_algo.h \
  ../gollybase/ruletreealgo.h ../gui-common/algos.h \
  ../gui-common/utils.h ../gui-common/prefs.h ../gui-common/layer.h \
  ../gui-common/select.h ../gui-common/control.h ../gui-common/file.h \
  ../gollybase/writepattern.h ../gui-common/view.h \
  ../gui-common/status.h ../gui-common/undo.h ../gui-common/render.h \
  webcalls.h

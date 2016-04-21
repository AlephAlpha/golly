-- Based on text_test.py from PLife (http://plife.sourceforge.net/).
--
-- Universal* font described by Eric Angelini on SeqFan list
-- Construction universality* demonstrated by Dean Hickerson
--
-- Integers can be written in this font that stabilize into
-- (almost*) any Life pattern known to be glider-constructible.
--
-- * Possible caveats involve a very small percentage
-- (possibly zero percent!) of glider constructions -- in
-- particular, constructions that require large numbers
-- of very closely spaced gliders.  The existence of objects
-- whose constructions require non-integer-constructable
-- closely-packed fleets of gliders is an open question;
-- no such objects are currently known.

local g = golly()
local gpt = require "gplus.text"

g.new("Eric Angelini integer glider gun")
g.setrule("B3/S23")

local all = gpt.maketext([[
411-31041653546-11-31041653546-1144444444444444444444444444444
31041653546-11-31041653546-11444444444444444-31041653546-11
31041653546-11444444444444444444-31041653546-11-31041653546
111444444444444-15073-114444-5473-11444444444-2474640508-444444
2474640508-444444444444444444444-2474640508-444444-2474640508-4444
2474640508-444444-2474640508-4444444444444444444444-2474640508
444444-2474640508-414-7297575-114-9471155613-414444444444
31041653546-11-2534474376-1-9471155613-414444444444-31041653546-114
7297575-114-9471155613-414444444444-31041653546-114-7297575-118
9471155613-414444444444-31041653546-18-6703-1444-107579-1114444
2474640508-51-947742-414444444441-947742-414444444444-2474640508
51-947742-414444444441-947742-41444-2474640508-51-947742-414
2474640508-414444444444444-2474640508-51-947742-414444444441
947742-414]], "EAlvetica")

all.display("")

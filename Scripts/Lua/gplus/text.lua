-- This module is loaded if a script calls require "gplus.text".

local g = golly()
local gp = require "gplus"

local m = {}

--------------------------------------------------------------------------------

-- Eric Angelini integer font

local eafont = {}
eafont['0'] = g.parse("3o$obo$obo$obo$3o!", 0, -5)
eafont['0'].width = 4
eafont['1'] = g.parse("o$o$o$o$o!", 0, -5)
eafont['1'].width = 2
eafont['2'] = g.parse("3o$2bo$3o$o$3o!", 0, -5)
eafont['2'].width = 4
eafont['3'] = g.parse("3o$2bo$3o$2bo$3o!", 0, -5)
eafont['3'].width = 4
eafont['4'] = g.parse("obo$obo$3o$2bo$2bo!", 0, -5)
eafont['4'].width = 4
eafont['5'] = g.parse("3o$o$3o$2bo$3o!", 0, -5)
eafont['5'].width = 4
eafont['6'] = g.parse("3o$o$3o$obo$3o!", 0, -5)
eafont['6'].width = 4
eafont['7'] = g.parse("3o$2bo$2bo$2bo$2bo!", 0, -5)
eafont['7'].width = 4
eafont['8'] = g.parse("3o$obo$3o$obo$3o!", 0, -5)
eafont['8'].width = 4
eafont['9'] = g.parse("3o$obo$3o$2bo$3o!", 0, -5)
eafont['9'].width = 4
eafont[' '] = g.parse("", 0, 0)
eafont[' '].width = 2
eafont['-'] = g.parse("", 0, 0)
eafont['-'].width = 0

--------------------------------------------------------------------------------

-- Snakial font (all chars are stable Life patterns)

local sfont = {}
sfont['0'] = g.parse("2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$o5bo$bo5bo$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o!", 0, -14)
sfont['0'].width = 10
sfont['1'] = g.parse("2o$bo$o$2o2$2o$bo$o$2o2$2o$bo$o$2o!", 1, -14)
sfont['1'].width = 6
sfont['2'] = g.parse("2b2obo$2bob2o$6b2o$6bo$7bo$6b2o$2b2obo$2bob2o$2o$o$bo$2o$2b2obo$2bob2o!", 0, -14)
sfont['2'].width = 10
sfont['3'] = g.parse("2obo$ob2o$4b2o$4bo$5bo$4b2o$2obo$ob2o$4b2o$4bo$5bo$4b2o$2obo$ob2o!", 0, -14)
sfont['3'].width = 8
sfont['4'] = g.parse("2o3b2o$2o3b2o2$2o3b2o$obobobo$2bobo$b2obo$5b2o$6bo$5bo$5b2o$6bo$5bo$5b2o!", 0, -14)
sfont['4'].width = 9
sfont['5'] = g.parse("2b2obo$2bob2o$2o$o$bo$2o$2b2obo$2bob2o$6b2o$6bo$7bo$6b2o$2b2obo$2bob2o!", 0, -14)
sfont['5'].width = 10
sfont['6'] = g.parse("2b2obo$2bob2o$2o$o$bo$2o$2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o!", 0, -14)
sfont['6'].width = 10
sfont['7'] = g.parse("ob2o$2obo$4b2o$5bo$4bo$4b2o$2b2o$3bo$2bo$2b2o$2o$bo$o$2o!", 0, -14)
sfont['7'].width = 8
sfont['8'] = g.parse("2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o!", 0, -14)
sfont['8'].width = 10
sfont['9'] = g.parse("2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o$6b2o$6bo$7bo$6b2o$2b2obo$2bob2o!", 0, -14)
sfont['9'].width = 10
sfont['-'] = g.parse("2obo$ob2o!", 0, -8)
sfont['-'].width = 6

--------------------------------------------------------------------------------

-- mono-spaced ASCII font

local mfont = {}
mfont[' '] = g.parse("")
mfont['!'] = g.parse("2bo$2bo$2bo$2bo$2bo2$2bo!")
mfont['"'] = g.parse("bobo$bobo$bobo!")
mfont['#'] = g.parse("bobo$bobo$5o$bobo$5o$bobo$bobo!")
mfont['$'] = g.parse("b3o$obobo$obo$b3o$2bobo$obobo$b3o!")
mfont['%'] = g.parse("2o2bo$2o2bo$3bo$2bo$bo$o2b2o$o2b2o!")
mfont['&'] = g.parse("b2o$o2bo$o2bo$b2o$o2bo$o2bo$b2obo!")
mfont["'"] = g.parse("2bo$2bo$2bo!")
mfont['('] = g.parse("3bo$2bo$2bo$2bo$2bo$2bo$3bo!")
mfont[')'] = g.parse("bo$2bo$2bo$2bo$2bo$2bo$bo!")
mfont['*'] = g.parse("$obobo$b3o$5o$b3o$obobo!")
mfont['+'] = g.parse("$2bo$2bo$5o$2bo$2bo!")
mfont[','] = g.parse("6$2bo$2bo$bo!")
mfont['-'] = g.parse("3$5o!")
mfont['.'] = g.parse("6$2bo!")
mfont['/'] = g.parse("3bo$3bo$2bo$2bo$2bo$bo$bo!")
mfont['0'] = g.parse("b3o$o3bo$o2b2o$obobo$2o2bo$o3bo$b3o!")
mfont['1'] = g.parse("2bo$b2o$2bo$2bo$2bo$2bo$b3o!")
mfont['2'] = g.parse("b3o$o3bo$4bo$3bo$2bo$bo$5o!")
mfont['3'] = g.parse("b3o$o3bo$4bo$2b2o$4bo$o3bo$b3o!")
mfont['4'] = g.parse("3bo$2b2o$bobo$o2bo$5o$3bo$3bo!")
mfont['5'] = g.parse("5o$o$o$b3o$4bo$o3bo$b3o!")
mfont['6'] = g.parse("b3o$o$o$4o$o3bo$o3bo$b3o!")
mfont['7'] = g.parse("5o$4bo$3bo$2bo$bo$o$o!")
mfont['8'] = g.parse("b3o$o3bo$o3bo$b3o$o3bo$o3bo$b3o!")
mfont['9'] = g.parse("b3o$o3bo$o3bo$b4o$4bo$4bo$b3o!")
mfont[':'] = g.parse("2$2bo4$2bo!")
mfont[';'] = g.parse("2$2bo4$2bo$2bo$bo!")
mfont['<'] = g.parse("$3bo$2bo$bo$2bo$3bo!")
mfont['='] = g.parse("2$5o2$5o!")
mfont['>'] = g.parse("$bo$2bo$3bo$2bo$bo!")
mfont['?'] = g.parse("b3o$o3bo$4bo$2b2o$2bo2$2bo!")
mfont['@'] = g.parse("b3o$o3bo$ob3o$obobo$ob2o$o$b3o!")
mfont['A'] = g.parse("b3o$o3bo$o3bo$5o$o3bo$o3bo$o3bo!")
mfont['B'] = g.parse("4o$o3bo$o3bo$4o$o3bo$o3bo$4o!")
mfont['C'] = g.parse("b3o$o3bo$o$o$o$o3bo$b3o!")
mfont['D'] = g.parse("4o$o3bo$o3bo$o3bo$o3bo$o3bo$4o!")
mfont['E'] = g.parse("5o$o$o$3o$o$o$5o!")
mfont['F'] = g.parse("5o$o$o$3o$o$o$o!")
mfont['G'] = g.parse("b3o$o3bo$o$o2b2o$o3bo$o3bo$b3o!")
mfont['H'] = g.parse("o3bo$o3bo$o3bo$5o$o3bo$o3bo$o3bo!")
mfont['I'] = g.parse("b3o$2bo$2bo$2bo$2bo$2bo$b3o!")
mfont['J'] = g.parse("2b3o$3bo$3bo$3bo$3bo$o2bo$b2o!")
mfont['K'] = g.parse("o3bo$o2bo$obo$2o$obo$o2bo$o3bo!")
mfont['L'] = g.parse("o$o$o$o$o$o$5o!")
mfont['M'] = g.parse("o3bo$2ob2o$obobo$obobo$o3bo$o3bo$o3bo!")
mfont['N'] = g.parse("o3bo$2o2bo$obobo$o2b2o$o3bo$o3bo$o3bo!")
mfont['O'] = g.parse("b3o$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!")
mfont['P'] = g.parse("4o$o3bo$o3bo$4o$o$o$o!")
mfont['Q'] = g.parse("b3o$o3bo$o3bo$o3bo$obobo$o2bo$b2obo!")
mfont['R'] = g.parse("4o$o3bo$o3bo$4o$o2bo$o3bo$o3bo!")
mfont['S'] = g.parse("b3o$o3bo$o$b3o$4bo$o3bo$b3o!")
mfont['T'] = g.parse("5o$2bo$2bo$2bo$2bo$2bo$2bo!")
mfont['U'] = g.parse("o3bo$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!")
mfont['V'] = g.parse("o3bo$o3bo$o3bo$o3bo$o3bo$bobo$2bo!")
mfont['W'] = g.parse("o3bo$o3bo$o3bo$obobo$obobo$2ob2o$o3bo!")
mfont['X'] = g.parse("o3bo$o3bo$bobo$2bo$bobo$o3bo$o3bo!")
mfont['Y'] = g.parse("o3bo$o3bo$bobo$2bo$2bo$2bo$2bo!")
mfont['Z'] = g.parse("5o$4bo$3bo$2bo$bo$o$5o!")
mfont['['] = g.parse("2b2o$2bo$2bo$2bo$2bo$2bo$2b2o!")
mfont['\\'] = g.parse("bo$bo$2bo$2bo$2bo$3bo$3bo!")
mfont[']'] = g.parse("b2o$2bo$2bo$2bo$2bo$2bo$b2o!")
mfont['^'] = g.parse("2bo$bobo$o3bo!")
mfont['_'] = g.parse("6$5o!")
mfont['`'] = g.parse("o$bo!")
mfont['a'] = g.parse("2$b4o$o3bo$o3bo$o3bo$b4o!")
mfont['b'] = g.parse("o$o$4o$o3bo$o3bo$o3bo$4o!")
mfont['c'] = g.parse("2$b4o$o$o$o$b4o!")
mfont['d'] = g.parse("4bo$4bo$b4o$o3bo$o3bo$o3bo$b4o!")
mfont['e'] = g.parse("2$b3o$o3bo$5o$o$b4o!")
mfont['f'] = g.parse("2b2o$bo2bo$bo$3o$bo$bo$bo!")
mfont['g'] = g.parse("2$b3o$o3bo$o3bo$o3bo$b4o$4bo$b3o!")
mfont['h'] = g.parse("o$o$ob2o$2o2bo$o3bo$o3bo$o3bo!")
mfont['i'] = g.parse("$2bo2$2bo$2bo$2bo$2b2o!")
mfont['j'] = g.parse("$3bo2$3bo$3bo$3bo$3bo$o2bo$b2o!")
mfont['k'] = g.parse("o$o$o2bo$obo$3o$o2bo$o3bo!")
mfont['l'] = g.parse("b2o$2bo$2bo$2bo$2bo$2bo$2b2o!")
mfont['m'] = g.parse("2$bobo$obobo$obobo$o3bo$o3bo!")
mfont['n'] = g.parse("2$4o$o3bo$o3bo$o3bo$o3bo!")
mfont['o'] = g.parse("2$b3o$o3bo$o3bo$o3bo$b3o!")
mfont['p'] = g.parse("2$4o$o3bo$o3bo$o3bo$4o$o$o!")
mfont['q'] = g.parse("2$b4o$o3bo$o3bo$o3bo$b4o$4bo$4bo!")
mfont['r'] = g.parse("2$ob2o$2o2bo$o$o$o!")
mfont['s'] = g.parse("2$b4o$o$b3o$4bo$4o!")
mfont['t'] = g.parse("$2bo$5o$2bo$2bo$2bo$3b2o!")
mfont['u'] = g.parse("2$o3bo$o3bo$o3bo$o3bo$b4o!")
mfont['v'] = g.parse("2$o3bo$o3bo$o3bo$bobo$2bo!")
mfont['w'] = g.parse("2$o3bo$o3bo$obobo$2ob2o$o3bo!")
mfont['x'] = g.parse("2$o3bo$bobo$2bo$bobo$o3bo!")
mfont['y'] = g.parse("2$o3bo$o3bo$o3bo$o3bo$b4o$4bo$b3o!")
mfont['z'] = g.parse("2$5o$3bo$2bo$bo$5o!")
mfont['{'] = g.parse("3bo$2bo$2bo$bo$2bo$2bo$3bo!")
mfont['|'] = g.parse("2bo$2bo$2bo$2bo$2bo$2bo$2bo!")
mfont['}'] = g.parse("bo$2bo$2bo$3bo$2bo$2bo$bo!")
mfont['~'] = g.parse("2$bo$obobo$3bo!")

for key, _ in pairs(mfont) do
    mfont[key].width = 6
end

--------------------------------------------------------------------------------

-- convert given string to a pattern using one of the above fonts

function m.maketext(s, font)
    font = font or "Snakial"
    local p = gp.pattern()
    local x = 0
    local f, unknown

    if string.lower(font) == "mono" then
        f = mfont
        unknown = '?'
    elseif string.lower(font:sub(1,2)) == "ea" then
        f = eafont
        unknown = '-'
    else
        f = sfont
        unknown = '-'
    end
    
    for ch in string.gmatch(s, ".[\128-\191]*") do
        local char = ch
        if f[char] == nil then char = unknown end
        p = p + gp.pattern(g.transform(f[char], x, 0))
        x = x + f[char].width
    end
    
    return p
end

--------------------------------------------------------------------------------

return m

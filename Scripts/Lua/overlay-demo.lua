--[[
This script demonstrates the overlay's capabilities.
Authors: Andrew Trevorrow (andrew@trevorrow.com) and Chris Rowett (crowett@gmail.com).
--]]

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local split = gp.split
local int = gp.int
local op = require "oplus"
local maketext = op.maketext
local pastetext = op.pastetext

local ov = g.overlay

math.randomseed(os.time())  -- init seed for math.random

-- minor optimizations
local rand = math.random
local floor = math.floor
local sin = math.sin
local cos = math.cos
local abs = math.abs

local wd, ht            -- overlay's current width and height
local toggle = 0        -- for toggling alpha blending
local align = "right"   -- for text alignment
local transbg = 0       -- for text transparent background
local shadow = 0        -- for text shadow

local demofont = "font 11 default-bold"

-- buttons for main menu (created in create_menu_buttons)
local batch_button
local blend_button
local animation_button
local cellview_button
local copy_button
local cursor_button
local line_button
local set_button
local fill_button
local load_button
local mouse_button
local multiline_button
local pos_button
local render_button
local replace_button
local save_button
local sound_button
local text_button
local transition_button

local extra_layer = false
local return_to_main_menu = false

--------------------------------------------------------------------------------

local function create_overlay()
    ov("create 1000 1000")
    -- main_menu() will resize the overlay to just fit buttons and text
end

--------------------------------------------------------------------------------

local function demotext(x, y, text)
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    maketext(text)
    pastetext(x, y)
    ov("blend "..oldblend)
    ov("font "..oldfont)
end

--------------------------------------------------------------------------------

local function repeat_test(extratext, palebg)
    extratext = extratext or ""
    palebg = palebg or false

    -- explain how to repeat test or return to the main menu
    local text = "Hit the space bar to repeat this test"..extratext..".\n"
    if g.os() == "Mac" then
        text = text.."Click or hit the return key to return to the main menu."
    else
        text = text.."Click or hit the enter key to return to the main menu."
    end
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    if palebg then
        -- draw black text
        ov(op.black)
        local w, h = maketext(text)
        pastetext(10, ht - 10 - h)
    else
        -- draw white text with a black shadow
        ov(op.white)
        local w, h = maketext(text, nil, op.white, 2, 2)
        local x = 10
        local y = ht - 10 - h
        pastetext(x, y)
    end
    ov("blend "..oldblend)
    ov("font "..oldfont)

    g.update()

    while true do
        local event = g.getevent()
        if event:find("^oclick") or event == "key enter none" or event == "key return none" then
            -- return to main menu rather than repeat test
            return_to_main_menu = true
            return false
        elseif event == "key space none" then
            -- repeat current test
            return true
        else
            -- might be a keyboard shortcut
            g.doevent(event)
        end
    end
end

--------------------------------------------------------------------------------

local function ms(t)
    return string.format("%.2fms", t)
end

--------------------------------------------------------------------------------

local day = 1

local function test_transitions()
    -- create a clip from the menu screen
    local oldblend = ov("blend 0")
    ov(op.black)
    ov("line 0 0 "..(wd - 1).." 0 "..(wd - 1).." "..(ht - 1).." 0 "..(ht - 1).." 0 0")
    ov("copy 0 0 "..wd.." "..ht.." bg")

    -- create the background clip
    ov(op.blue)
    ov("fill")
    local oldfont = ov("font 100 mono")
    ov(op.yellow)
    local w,h = maketext("Golly")
    ov("blend 1")
    pastetext(floor((wd - w) / 2), floor((ht - h) / 2))
    ov("copy 0 0 "..wd.." "..ht.." fg")
    ov("blend 0")
    local pause = 0

    ::restart::
    ov("paste 0 0 bg")
    ov("update")
    local t = g.millisecs()
    while g.millisecs() - t < pause do end

    -- monday: exit stage left
    if day == 1 then
        for x = 0, wd, 10 do
            t = g.millisecs()
            ov("paste 0 0 fg")
            ov("paste "..-x.." 0 bg")
            ov("update")
            while g.millisecs() - t < 15 do end
        end
    -- tuesday: duck and cover
    elseif day == 2 then
        ov(op.white)
        for y = 0, ht, 10 do
            t = g.millisecs()
            ov("paste 0 0 fg")
            ov("paste 0 "..y.." bg")
            ov("update")
            while g.millisecs() - t < 15 do end
        end
    -- wednesday: slide to the right
    elseif day == 3 then
        for y = 0, ht, 8 do
            ov("copy 0 "..y.." "..wd.." 8 bg"..y)
        end
        local d
        local p
        for x = 0, wd * 2, 20 do
            t = g.millisecs()
            ov("paste 0 0 fg")
            d = 0
            for y = 0, ht, 8 do
                 p = x + 10 * d - wd
                 if p < 0 then p = 0 end
                 ov("paste "..p.." "..y.." bg"..y)
                 d = d + 1
            end
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        for y = 0, ht, 8 do
            ov("delete bg"..y)
        end
    -- thursday: as if by magic
    elseif day == 4 then
        ov("paste 0 0 fg")
        ov("copy 0 0 "..wd.." "..ht.." blend")
        for a = 0, 255, 5 do
            t = g.millisecs()
            ov("blend 0")
            ov("paste 0 0 bg")
            ov("blend 1")
            ov("rgba 0 0 0 "..a)
            ov("target blend")
            ov("replace *r *g *b *")
            ov("target")
            ov("paste 0 0 blend")
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        ov("delete blend")
    -- friday: you spin me round
    elseif day == 5 then
        local x, y, r
        local deg2rad = 57.3
        for a = 0, 360, 6 do
            t = g.millisecs()
            r = wd / 360 * a
            x = floor(r * sin(a / deg2rad))
            y = floor(r * cos(a / deg2rad))
            ov("paste 0 0 fg")
            ov("paste "..x.." "..y.." bg")
            ov("update")
            while g.millisecs() - t < 15 do end
       end
    -- saturday: through the square window
    elseif day == 6 then
        for x = 1, wd / 2, 4 do
            t = g.millisecs()           
            local y = x * (ht / wd)
            ov("blend 0")
            ov("paste 0 0 bg")
            ov("rgba 0 0 0 0")
            ov("fill "..floor(wd / 2 - x).." "..floor(ht / 2 - y).." "..(x * 2).." "..floor(y * 2))
            ov("copy 0 0 "..wd.." "..ht.." trans")
            ov("paste 0 0 fg")
            ov("blend 1")
            ov("paste 0 0 trans")
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        ov("delete trans")
    -- sunday: people in glass houses
    elseif day == 7 then
        local box = {}
        local n = 1
        local tx, ty
        for y = 0, ht, 16 do
            for x = 0, wd, 16 do
                tx = x + rand(0, floor(wd / 8)) - wd / 16
                ty = ht + rand(0, floor(ht / 2))
                local entry = {}
                entry[1] = x
                entry[2] = y
                entry[3] = tx
                entry[4] = ty
                box[n] = entry
                ov("copy "..x.." "..y.." 16 16 sprite"..n)
                n = n + 1
            end
        end
        for i = 0, 100 do
            t = g.millisecs()
            local a = i / 100
            local x, y
            ov("paste 0 0 fg")
            for n = 1, #box do
                x = box[n][1]
                y = box[n][2]
                tx = box[n][3]
                ty = box[n][4]
                ov("paste "..floor(x * (1 - a) + tx * a).." "..floor(y * (1 - a) + ty * a).." sprite"..n)
            end
            ov("update")
            while g.millisecs() - t < 15 do end
        end
        n = 1
        for y = 0, ht, 16 do
            for x = 0, wd, 16 do
                ov("delete sprite"..n)
                n = n + 1
            end
        end
    end

    -- next day
    day = day + 1
    if day == 8 then
        day = 1
    end

    ov("paste 0 0 fg")
    ov("update")
    pause = 300
    if repeat_test(" using a different transition", false) then goto restart end

    -- restore settings
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov("delete fg")
    ov("delete bg")
end

--------------------------------------------------------------------------------

local curs = 0

local function test_cursors()
    ::restart::

    local cmd
    curs = curs + 1
    if curs ==  1 then cmd = "cursor pencil" end
    if curs ==  2 then cmd = "cursor pick" end
    if curs ==  3 then cmd = "cursor cross" end
    if curs ==  4 then cmd = "cursor hand" end
    if curs ==  5 then cmd = "cursor zoomin" end
    if curs ==  6 then cmd = "cursor zoomout" end
    if curs ==  7 then cmd = "cursor arrow" end
    if curs ==  8 then cmd = "cursor current" end
    if curs ==  9 then cmd = "cursor wait" end
    if curs == 10 then cmd = "cursor hidden" curs = 0 end
    ov(cmd)

    ov(op.white)
    ov("fill")
    -- create a transparent hole
    ov("rgba 0 0 0 0")
    ov("fill 100 100 100 100")

    if cmd == "cursor current" then
        cmd = cmd.."\n\n".."The overlay cursor matches Golly's current cursor."
    else
        cmd = cmd.."\n\n".."The overlay cursor will change to Golly's current cursor\n"..
                           "if it moves outside the overlay or over a transparent pixel:"
    end
    ov(op.black)
    demotext(10, 10, cmd)

    if repeat_test(" using a different cursor", true) then goto restart end
    curs = 0
end

--------------------------------------------------------------------------------

local pos = 0

local function test_positions()
    ::restart::

    pos = pos + 1
    if pos == 1 then ov("position topleft") end
    if pos == 2 then ov("position topright") end
    if pos == 3 then ov("position bottomright") end
    if pos == 4 then ov("position bottomleft") end
    if pos == 5 then ov("position middle") pos = 0 end

    ov(op.white)
    ov("fill")
    ov("rgba 0 0 255 128")
    ov("fill 1 1 -2 -2")

    local text =
[[The overlay can be positioned in the middle
of the current layer, or at any corner.]]
    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    local w, h
    local fontsize = 30
    ov(op.white)
    -- reduce fontsize until text nearly fills overlay width
    repeat
        fontsize = fontsize - 1
        ov("font "..fontsize)
        w, h = maketext(text)
    until w <= wd - 10
    ov(op.black)
    maketext(text, "shadow")
    local x = int((wd - w) / 2)
    local y = int((ht - h) / 2)
    pastetext(x+2, y+2, op.identity, "shadow")
    pastetext(x, y)
    ov("blend "..oldblend)
    ov("font "..oldfont)

    if repeat_test(" using a different position") then goto restart end
    pos = 0
end

--------------------------------------------------------------------------------

local replace = 1
local replacements = {
    [1] = { op.yellow, "replace 255 0 0 255", "replace red pixels with yellow" },
    [2] = { "rgba 255 255 0 128", "replace 0 0 0 255", "replace black with semi-transparent yellow" },
    [3] = { op.yellow, "replace !255 0 0 255", "replace non-red pixels with yellow" },
    [4] = { "", "replace *g *r *# *#", "swap red and green components" },
    [5] = { "rgba 0 0 0 128", "replace *# *# *# *", "make all pixels semi-transparent" },
    [6] = { "", "replace *r- *g- *b- *#", "invert r g b components" },
    [7] = { "", "replace *# *# *# *#-", "make transparent pixels opaque and vice versa" },
    [8] = { op.yellow, "replace * * * !255", "replace non-opaque pixels with yellow" },
    [9] = { "rgba 255 255 255 255", "replace *a *a *a *", "convert alpha to grayscale" },
    [10] = { op.yellow, "replace 0 255 0 !255", "replace non-opaque green with yellow" },
    [11] = { op.yellow, "replace * * * *", "fill (replace any pixel with yellow)" },
    [12] = { "", "replace *# *# *# *#", "no-op (replace pixels with clip pixels)" },
    [13] = { "rgba 0 0 0 128", "replace *# *# *# *", "make whole overlay semi-transparent", true },
    [14] = { "", "replace *#+64 *#+64 *#+64 *#", "make pixels brighter" },
    [15] = { "", "replace *# *# *# *#-64", "make pixels more transparent" },
    [16] = { "", "replace *#++ *#++ *#++ *#", "fade to white using increment", true, true },
    [17] = { "", "replace *#-4 *#-4 *#-4 *#", "fast fade to black", true, true }
}

local function test_replace()
    ::restart::

    -- create clip
    local oldblend = ov("blend 0")
    ov("rgba 0 0 0 0")
    ov("fill")
    ov("blend 1")
    ov(op.black)
    ov("fill 20 20 192 256")
    ov(op.red)
    ov("fill 84 84 128 128")
    ov(op.blue)
    ov("fill 148 148 64 64")
    ov("rgba 0 255 0 128")
    ov("fill 64 64 104 104")
    ov("rgba 255 255 255 64")
    ov("fill 212 20 64 64")
    ov("rgba 255 255 255 128")
    ov("fill 212 84 64 64")
    ov("rgba 255 255 255 192")
    ov("fill 212 148 64 64")
    ov("rgba 255 255 255 255")
    ov("fill 212 212 64 64")
    ov("blend 0")
    ov("rgba 255 255 0 0")
    ov("fill 84 212 64 64")
    ov("rgba 0 255 0 128")
    ov("fill 20 212 64 64")
    ov("blend 1")
    ov(op.white)
    ov("line 20 20 275 20 275 275 20 275 20 20")
    ov("copy 20 20 256 256 clip")

    -- create the background with some text
    local oldfont = ov("font 24 mono")
    ov("rgba 0 0 192 255")
    ov("fill")
    ov("rgba 192 192 192 255")
    local w, h = maketext("Golly")
    ov("blend 1")
    for y = 0, ht - 70, h do
        for x = 0, wd, w do
            pastetext(x, y)
        end
    end

    -- draw the clip
    ov("paste 20 20 clip")

    -- replace clip
    local drawcol = replacements[replace][1]
    local replacecmd = replacements[replace][2]
    if drawcol ~= "" then
        -- set RGBA color
        ov(drawcol)
    end
    -- execute replace and draw clip
    local replaced = 0
    local t1 = g.millisecs()
    if replacements[replace][4] ~= true then
        ov("target clip")
    end
    if replacements[replace][5] == true then
        replaced = 1
        while replaced > 0 do
            local t = g.millisecs()
            replaced = tonumber(ov(replacecmd))
            ov("update")
            while g.millisecs() - t < 15 do end
        end
    else
        replaced = tonumber(ov(replacecmd))
    end
    t1 = g.millisecs() - t1
    ov("target ")
    if replacements[replace][4] ~= true then
        ov("paste "..(wd - 276).." 20 clip")
    end

    -- draw replacement text background
    ov("blend 1")
    ov("rgba 0 0 0 192")
    ov("fill 0 300 "..wd.. " 144")

    -- draw test name
    ov("font 14 mono")
    ov(op.white)
    local testname = "Test "..replace..": "..replacements[replace][3]
    w, h = maketext(testname, nil, nil, 2, 2)
    pastetext(floor((wd - w) / 2), 310)

    -- draw test commands
    ov("font 22 mono")
    if drawcol ~= "" then
        ov(op.yellow)
        w, h = maketext(drawcol, nil, nil, 2, 2)
        pastetext(floor((wd - w) / 2), 340)
    end
    ov(op.yellow)
    w, h = maketext(replacecmd, nil, nil, 2, 2)
    pastetext(floor((wd - w) / 2), 390)

    -- next replacement
    replace = replace + 1
    if replace > #replacements then
        replace = 1
    end

    -- restore settings
    ov("blend "..oldblend)
    ov("font "..oldfont)

    g.show("Time to replace: "..ms(t1).."  Pixels replaced: "..replaced)
    if repeat_test(" with different options") then goto restart end
    ov("target")
end

--------------------------------------------------------------------------------

local function test_copy_paste()
    ::restart::

    local t1 = g.millisecs()

    -- tile the overlay with a checkerboard pattern
    local sqsize = rand(5, 300)
    local tilesize = sqsize * 2

    -- create the 1st tile (2x2 squares) in the top left corner
    ov(op.white)
    ov("fill 0 0 "..tilesize.." "..tilesize)
    ov(op.red)
    ov("fill 1 1 "..(sqsize-1).." "..(sqsize-1))
    ov("fill "..(sqsize+1).." "..(sqsize+1).." "..(sqsize-1).." "..(sqsize-1))
    ov(op.black)
    ov("fill "..(sqsize+1).." 1 "..(sqsize-1).." "..(sqsize-1))
    ov("fill 1 "..(sqsize+1).." "..(sqsize-1).." "..(sqsize-1))
    ov("copy 0 0 "..tilesize.." "..tilesize.." tile")

    -- tile the top row
    for x = tilesize, wd, tilesize do
        ov("paste "..x.." 0 tile")
    end

    -- copy the top row and use it to tile the remaining rows
    ov("copy 0 0 "..wd.." "..tilesize.." row")
    for y = tilesize, ht, tilesize do
        ov("paste 0 "..y.." row")
    end

    ov("delete tile")
    ov("delete row")

    g.show("Time to test copy and paste: "..ms(g.millisecs()-t1))

    if repeat_test(" with different sized tiles") then goto restart end
end

--------------------------------------------------------------------------------

local function bezierx(t, x0, x1, x2, x3)
    local cX = 3 * (x1 - x0)
    local bX = 3 * (x2 - x1) - cX
    local aX = x3 - x0 - cX - bX

    -- compute x position
    local x = (aX * math.pow(t, 3)) + (bX * math.pow(t, 2)) + (cX * t) + x0;
    return x
end

--------------------------------------------------------------------------------

local themenum = 1

local function test_cellview()
    -- create a new layer
    g.addlayer()
    extra_layer = true
    g.setalgo("QuickLife")
    g.setrule("b3/s23")

    -- resize overlay to cover entire layer
    wd, ht = g.getview( g.getlayer() )
    ov("resize "..wd.." "..ht)

    -- create a new layer with 50% random fill
    local size = 512
    g.select( {0, 0, size, size} )
    g.randfill(50)
    g.select( {} )

    -- save settings
    local oldblend = ov("blend 0")
    local oldalign = ov("textoption align left")

    -- create text clips
    local exitclip = "exit"
    local oldfont = ov("font 16 roman")
    local exitw = maketext("Click or press any key to return to the main menu.", exitclip, op.white, 2, 2)
    ov("blend 1")

    -- create a cellview (width and height must be multiples of 16)
    local w16 = size & ~15
    local h16 = size & ~15
    ov("cellview 0 0 "..w16.." "..h16)

    -- set the theme
    ov(op.themes[themenum])
    themenum = themenum + 1
    if themenum > #op.themes then
        themenum = 0
    end

    -- initialize the camera
    local angle = 0
    local zoom = 6
    local x = size / 2
    local y = size / 2
    local depth = 0
    local startangle = angle
    local startzoom = zoom
    local startx = x
    local starty = y
    local startdepth = depth
    local targetangle = angle
    local targetzoom = zoom
    local targetx = x
    local targety = y
    local targetdepth = depth
    local anglesteps = 600
    local zoomsteps = 400
    local xysteps = 200
    local depthsteps = 800
    local anglestep = 0
    local zoomstep = 0
    local xystep = 0
    local depthstep = 0
    local firstdepth = true
    local firstxy = true
    local firstangle = true
    local firstzoom = true

    -- create the world view clip
    local worldsize = 256
    ov("create "..worldsize.." "..worldsize.." world")

    -- animate the cells
    local running = true
    while running do
        local t1 = g.millisecs()

        -- stop when key pressed or mouse button clicked
        local event = g.getevent()
        if event:find("^key") or event:find("^oclick") then
            running = false
        end

        -- next generation
        g.run(1)

        -- update cell view from pattern
        ov("updatecells")

        -- set the camera
        ov("camera angle "..angle)
        ov("camera zoom "..zoom)
        ov("camera xy "..x.." "..y)

        -- set the layer depth and number of layers
        ov("celloption depth "..depth)
        ov("celloption layers 6")

        -- update the camera zoom
        if zoomstep == zoomsteps then
            zoomstep = 0
            zoomsteps = rand(600, 800)
            startzoom = zoom
            if rand() > 0.25 or firstzoom then
                targetzoom = rand(2, 10)
                firstzoom = false
            else
                targetzoom = startzoom
            end
        else
            zoom = startzoom + bezierx(zoomstep / zoomsteps, 0, 0, 1, 1) * (targetzoom - startzoom)
            zoomstep = zoomstep + 1
        end

        -- update the camera angle
        if anglestep == anglesteps then
            anglestep = 0
            anglesteps = rand(600, 800)
            startangle = angle
            if rand() > 0.25 or firstangle then
                targetangle = rand(0, 359)
                firstangle = false
            else
                targetangle = startangle
            end
        else
            angle = startangle + bezierx(anglestep / anglesteps, 0, 0, 1, 1) * (targetangle - startangle)
            anglestep = anglestep + 1
        end

        -- update the camera position
        if xystep == xysteps then
            xystep = 0
            xysteps = rand(1000, 1200)
            startx = x
            starty = y
            if rand() > 0.25 or firstxy then
                targetx = rand(0, w16 - 1)
                targety = rand(0, h16 - 1)
                firstxy = false
            else
                targetx = startx
                targety = starty
            end
        else
            x = startx + bezierx(xystep / xysteps, 0, 0, 1, 1) * (targetx - startx)
            y = starty + bezierx(xystep / xysteps, 0, 0, 1, 1) * (targety - starty)
            xystep = xystep + 1
        end

        -- update the layer depth
        if depthstep == depthsteps then
            depthstep = 0
            depthsteps = rand(600, 800)
            startdepth = depth
            if rand() > 0.5 or firstdepth then
                targetdepth = rand()
                firstdepth = false
            else
                targetdepth = 0
            end
        else
            depth = startdepth + bezierx(depthstep / depthsteps, 0, 0, 1, 1) * (targetdepth - startdepth)
            depthstep = depthstep + 1
        end

        -- draw the cell view
        ov("blend 0")
        ov("drawcells")

        -- draw the world view
        ov("target world")
        ov("camera angle 0")
        ov("camera zoom 1")
        ov("camera xy "..floor(size / 2).." "..floor(size / 2))
        ov("celloption layers 1")
        ov("drawcells")
        ov("rgba 128 128 128 255")
        ov("line 0 0 "..(worldsize - 1).." 0")
        ov("line 0 0 0 "..(worldsize - 1))
        ov("target")
        ov("paste "..(wd - worldsize).." "..(ht - worldsize).." world")

        -- draw exit message
        ov("blend 1")
        pastetext(floor((wd - exitw) / 2), 20, op.identity, exitclip)

        g.show("Time to test cellview: "..ms(g.millisecs()-t1))

        -- update the overlay
        ov("update")
        while g.millisecs() - t1 < 15 do end
    end

    -- free clips
    ov("delete world")
    ov("delete "..exitclip)

    -- restore settings
    ov("textoption align "..oldalign)
    ov("font "..oldfont)
    ov("blend "..oldblend)

    -- delete the layer
    g.dellayer()
    extra_layer = false

    return_to_main_menu = true
end

--------------------------------------------------------------------------------

local function test_animation()
    -- resize overlay to cover entire layer
    wd, ht = g.getview( g.getlayer() )
    ov("resize "..wd.." "..ht)

    local t1, t2

    -- grid size
    local tilewd = 48
    local tileht = 48

    -- glider
    local glider = {
        [1] = { 1, 1, 1, 0, 0, 1, 0, 1, 0 },
        [2] = { 0, 1, 0, 0, 1, 1, 1, 0, 1 },
        [3] = { 0, 1, 1, 1, 0, 1, 0, 0, 1 },
        [4] = { 1, 1, 0, 0, 1, 1, 1, 0, 0 }
    }
    local adjustx = { 0, 0, 0, 1 };
    local adjusty = { 0, -1, 0, 0 };

    -- save settings
    local oldblend = ov("blend 0")
    local oldalign = ov("textoption align left")

    -- create text clips
    local exitclip = "exit"
    local oldfont = ov("font 12 roman")
    local exitw = maketext("Click or press any key to return to the main menu.", exitclip, op.white, 2, 2)
    local gollyopaqueclip = "clip1"
    local gollytranslucentclip = "clip2"
    ov("font 200 mono")
    local bannertext = "Golly 2.9"
    ov("rgba 255 192 32 144")
    local w, h = maketext(bannertext, gollyopaqueclip)
    ov("rgba 255 192 32 255")
    maketext(bannertext, gollytranslucentclip)

    local creditstext = [[
Golly 2.9


© 2016 The Golly Gang:

Tom Rokicki, Andrew Trevorrow, Tim Hutton, Dave Greene,
Jason Summers, Maks Verver, Robert Munafo, Chris Rowett.




CREDITS




The Pioneers


John Conway
for creating the Game of Life

Martin Gardner
for popularizing the topic in Scientific American




The Programmers


Tom Rokicki
for the complicated stuff

Andrew Trevorrow
for the cross-platform GUI and overlay

Chris Rowett
for rendering and overlay improvements

Tim Hutton
for the RuleTable algorithm

Dave Greene

Jason Summers

Maks Verver

Robert Munafo




The Beta Testers


Thanks to all the bug hunters for their
reports and suggestions, especially:

Dave Greene

Gabriel Nivasch

Dean Hickerson

Brice Due

David Eppstein

Tony Smith

Alan Hensel

Dennis Langdeau

Bill Gosper

Mark Jeronimus

Eric Goldstein




Thanks to


Bill Gosper
for the idea behind the HashLife algorithm

Alan Hensel
for QuickLife ideas and non-totalistic algorithm

David Eppstein
for the B0 rule emulation idea

Eugene Langvagen
for inspiring Golly's scripting capabilities

Stephen Silver
for the wonderful Life Lexicon

Nathaniel Johnston
for the brilliant LifeWiki and the online archive

Julian Smart and all wxWidgets developers
for wxWidgets

Guido van Rossum
for Python

Roberto Ierusalimschy and all Lua developers
for Lua




Pattern Collection


Dave Greene and Alan Hensel

Thanks to everybody who allowed us to distribute
their fantastic patterns, especially:

Nick Gotts

Gabriel Nivasch

David Eppstein

Jason Summers

Stephen Morley

Dean Hickerson

Brice Due

William R. Buckley

David Moore

Mark Owen

Tim Hutton

Renato Nobili

Adam P. Goucher

David Bell
]]
    local oldalign = ov("textoption align center")
    ov("font 14 roman")

    local creditsclip = "credits"
    local credwidth, credheight = maketext(creditstext, creditsclip, "rgba 128 255 255 255", 2, 2)

    -- create graduated background
    local level

    for y = 0, ht / 2 do
        level = 32 + floor(176 * (y * 2 / ht))
        ov("rgba 0 0 "..level.." 255")
        ov("line 0 "..y.." "..wd.." "..y)
        ov("line 0 "..(ht - y).." "..wd.." "..(ht -y))
    end

    -- save background
    local bgclip = "bg"
    ov("copy 0 0 0 0 "..bgclip)

    -- create stars
    local starx = {}
    local stary = {}
    local stard = {}
    local numstars = 1000
    for i = 1, numstars do
        starx[i] = rand(0, wd - 1)
        stary[i] = rand(0, ht - 1)
        stard[i] = floor((i - 1) / 10) / 100
    end

    local textx = wd
    local texty
    local gridx = 0
    local offset
    local running = true
    local gliderx = 0
    local glidery = floor(ht / tileht)
    local gliderframe = 1
    local x, y
    local lastframe = -1
    local credity = ht
    local creditx = floor((wd - credwidth) / 2)
    local credpos

    -- main loop
    while running do
        t2 = g.millisecs()

        -- measure frame draw time
        t1 = g.millisecs()

        -- stop when key pressed or mouse button clicked
        local event = g.getevent()
        if event:find("^key") or event:find("^oclick") then
            running = false
        end

        -- draw background
        ov("blend 0")
        ov("paste 0 0 "..bgclip)

        local timebg = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw gridlines
        offset = floor(gridx)
        ov("rgba 64 64 64 255")
        for i = 0, wd, tilewd do
           ov("line "..(i + offset).." 0 "..(i + offset).." "..ht)
        end
        for i = 0, ht, tileht do
           ov("line 0 "..(i + offset).." "..wd.." "..(i + offset))
        end

        local timegrid = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw stars
        local level = 50
        local i = 1
        ov("rgba "..level.." "..level.." "..level.." 255")
        local lastd = stard[i]
        local coords = ""

        for i = 1, numstars do
            if (stard[i] ~= lastd) then
                if coords ~= "" then
                    ov("set"..coords)
                    coords = ""
                end
                ov("rgba "..level.." "..level.." "..level.." 255")
                level = level + 2
                lastd = stard[i]
            end
            starx[i] = starx[i] + lastd
            if starx[i] > wd then
                starx[i] = 0
                stary[i] = rand(0, ht - 1)
            end
            x = floor(starx[i])
            y = floor(stary[i])
            coords = coords.." "..x.." "..y
        end
        if coords ~= "" then
            ov("set"..coords)
        end

        local timestars = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw glider
        ov(op.white)
        local gx, gy
        local frame = floor(gliderframe)
        if frame == 5 then
            gliderframe = 1
            frame = 1
        end
        if frame ~= lastframe then
            gliderx = gliderx + adjustx[frame]
            glidery = glidery + adjusty[frame]
            lastframe = frame
            if glidery < -3 then
                glidery = floor(ht / tileht)
            end
            if gliderx > wd / tilewd then
                gliderx = -3 
            end
        end
        gliderframe = gliderframe + 0.05

        for gy = 0, 2 do
            for gx = 0, 2 do
                if glider[frame][3 * gy + gx + 1] == 1 then
                    x = (gliderx + gx) * tilewd + offset
                    y = (glidery + gy) * tileht + offset
                    ov("fill "..(x + 1).." "..(y + 1).." "..(tilewd - 1).." "..(tileht - 1))
                end
            end
        end

        local timeglider = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw bouncing scrolling text
        ov("blend 1")
        texty = floor(((ht - h) / 2 + (100 * sin(textx / 100))))
        pastetext(textx, texty, op.identity, gollytranslucentclip)

        texty = floor(((ht - h) / 2 - (100 * sin(textx / 100))))
        pastetext(textx, texty, op.identity, gollyopaqueclip)

        local timegolly = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw credits
        credpos = floor(credity)
        pastetext(creditx, credpos, op.identity, creditsclip)
        credity = credity - .5
        if credity < -credheight then
            credity = ht
        end

        local timecredits = g.millisecs() - t1
        t1 = g.millisecs()

        -- draw exit message
        pastetext(floor((wd - exitw) / 2), 20, op.identity, exitclip)

        -- update display
        ov("update")

        local timeupdate = g.millisecs() - t1

        -- move grid
        gridx = gridx + 0.2
        if gridx >= tilewd then
            gridx = 0
            gliderx = gliderx + 1
            glidery = glidery + 1
        end

        -- move text
        textx = textx - 2
        if textx <= -w then
            textx = wd
        end

        -- wait until at least 15ms have elapsed
        t1 = g.millisecs()
        local timewait = t1
        while (t1 - t2 < 15) do
            t1 = g.millisecs()
        end
        timewait = t1 - timewait

        -- display frame time
        local frametime = g.millisecs() - t2

        g.show("Time: frame "..ms(frametime)..
               "  bg "..ms(timebg).."  stars "..ms(timestars)..
               "  glider "..ms(timeglider).."  grid "..ms(timegrid)..
               "  golly "..ms(timegolly).."  credits "..ms(timecredits)..
               "  update "..ms(timeupdate).."  wait "..ms(timewait))
    end

    -- free clips
    ov("delete "..exitclip)
    ov("delete "..gollytranslucentclip)
    ov("delete "..gollyopaqueclip)
    ov("delete "..creditsclip)
    ov("delete "..bgclip)

    -- restore settings
    ov("textoption align "..oldalign)
    ov("font "..oldfont)
    ov("blend "..oldblend)

    -- no point calling repeat_test()
    return_to_main_menu = true
end

--------------------------------------------------------------------------------

local loaddir = g.getdir("app").."Help/images/"

local function test_load()
    ::restart::

    ov(op.yellow)
    ov("fill")
    g.update()

    -- prompt user to load a BMP/GIF/PNG/TIFF file
    local filetypes = "Image files (*.bmp;*.gif;*.png;*.tiff)|*.bmp;*.gif;*.png;*.tiff"
    local filepath = g.opendialog("Load an image file", filetypes, loaddir, "")
    if #filepath > 0 then
        -- center image in overlay by first loading the file completely outside it
        -- so we get the image dimensions without changing the overlay
        local imgsize = ov("load "..wd.." "..ht.." "..filepath)
        local iw, ih = split(imgsize)
        ov("load "..int((wd-iw)/2).." "..int((ht-ih)/2).." "..filepath)
        g.show("Image width and height: "..imgsize)

        -- update loaddir by stripping off the file name
        local pathsep = g.getdir("app"):sub(-1)
        loaddir = filepath:gsub("[^"..pathsep.."]+$","")
    end

    if repeat_test(" and load another image", true) then goto restart end
end

--------------------------------------------------------------------------------

local savedir = g.getdir("data")

local function test_save()
    ::restart::

    -- create gradient from one random pale color to another
    local r1, g1, b1, r2, g2, b2
    repeat
        r1 = rand(128,255)
        g1 = rand(128,255)
        b1 = rand(128,255)
        r2 = rand(128,255)
        g2 = rand(128,255)
        b2 = rand(128,255)
    until abs(r1-r2) + abs(g1-g2) + abs(b1-b2) > 128
    local rfrac = (r2 - r1) / ht;
    local gfrac = (g2 - g1) / ht;
    local bfrac = (b2 - b1) / ht;
    for y = 0, ht-1 do
        local rval = int(r1 + y * rfrac + 0.5)
        local gval = int(g1 + y * gfrac + 0.5)
        local bval = int(b1 + y * bfrac + 0.5)
        ov("rgba "..rval.." "..gval.." "..bval.." 255")
        ov("line 0 "..y.." "..wd.." "..y)
    end

    -- create a transparent hole in the middle
    ov("rgba 0 0 0 0")
    ov("fill "..int((wd-100)/2).." "..int((ht-100)/2).." 100 100")

    g.update()

    -- prompt for file name and location
    local pngpath = g.savedialog("Save overlay as PNG file", "PNG (*.png)|*.png",
                                 savedir, "overlay.png")
    if #pngpath > 0 then
        -- save overlay in given file
        ov("save 0 0 "..wd.." "..ht.." "..pngpath)
        g.show("Overlay was saved in "..pngpath)

        -- update savedir by stripping off the file name
        local pathsep = g.getdir("app"):sub(-1)
        savedir = pngpath:gsub("[^"..pathsep.."]+$","")
    end

    if repeat_test(" and save a different overlay", true) then goto restart end
end

--------------------------------------------------------------------------------

local function test_set()
    local maxx = wd - 1
    local maxy = ht - 1
    local flakes = 10000

    -- create the exit message
    local oldfont = ov(demofont)
    local text
    if g.os() == "Mac" then
        text = "Click or hit the return key to return to the main menu."
    else
        text = "Click or hit the enter key to return to the main menu."
    end
    local w, h = maketext(text, nil, op.white, 2, 2)

    -- create the golly text
    ov("font 100 mono")
    ov(op.yellow)
    local gw, gh = maketext("Golly", "gollyclip")

    -- fill the background with graduated blue to black
    local oldblend = ov("blend 0")
    local c
    for i = 0, ht - 1 do
        c = floor(i / ht * 128)
        ov("rgba 0 0 "..c.." 255")
        ov("line 0 "..i.." "..(wd - 1).." "..i)
    end

    -- draw golly text
    ov("blend 1")
    local texty = ht - gh - h
    pastetext(floor((wd - gw) / 2), texty, op.identity, "gollyclip")

    -- create the background clip
    ov("copy 0 0 "..wd.." "..ht.." bg")
    ov("update")

    -- read the screen
    local rgba
    local screen = {}
    for i = 0, ht - 1 do
        local row = {}
        if i < texty or i > texty + gh then
            for j = 0, wd - 1 do
                row[j] = true
            end
        else
            for j = 0, wd - 1 do
                rgba = ov("get "..j.." "..i)
                local r, g, b = split(rgba)
                if tonumber(r) == 0 and tonumber(g) == 0 then
                    row[j] = true
                else
                    row[j] = false
                end
            end
        end
        screen[i] = row
    end

    -- initialize flake positions
    local maxpos = -20 * maxy
    local x  = {}
    local y  = {}
    local dy = {}
    local xy = {}
    for i = 1, flakes do
        x[i] = rand(0, maxx)
        local yval = 0
        for j = 1, 10 do
            yval = yval + rand(0, 20 * maxy)
        end
        y[i] = floor(-(yval / 10))
        if y[i] > maxpos then
            maxpos = y[i]
        end
        dy[i] = rand() / 5 + 0.8
    end
    for i = 1, flakes do
        y[i] = y[i] - maxpos
    end

    -- loop until key pressed or mouse clicked
    while not return_to_main_menu do
        local event = g.getevent()
        if event:find("^oclick") or event == "key enter none" or event == "key return none" then
            -- return to main menu
            return_to_main_menu = true
        end

        -- draw the background
        local t1 = g.millisecs()
        ov("blend 0")
        ov("paste 0 0 bg")

        -- time drawing the pixels
        ov(op.white)
        local drawn = 0
        local lastx, lasty, newx, newy, diry
        local m = 1
        for i = 1, flakes do
            lastx = x[i]
            lasty = y[i]
            diry = dy[i]
            newx = lastx
            newy = lasty
            if lasty >= 0 and lasty < ht - 1 then
                if floor(lasty) == floor(lasty + diry) then
                    newy = lasty + diry
                else
                    if screen[floor(lasty + diry)][lastx] == true then
                        newy = lasty + diry
                    else
                        if rand() < 0.05 then
                            local dx = 1
                            if rand() < 0.5 then
                                dx = -1
                            end
                            if lastx + dx >= 0 and lastx + dx < wd then
                                if screen[floor(lasty + diry)][lastx + dx] == true then
                                   newx = lastx + dx
                                   newy = lasty + diry
                                end
                            end
                        end
                    end
                    screen[floor(lasty)][lastx] = true
                    screen[floor(newy)][newx] = false
                end
            elseif lasty < 0 then
                newy = lasty + diry
            end
            x[i] = newx
            y[i] = newy
            if newy >= 0 and newy < ht then
                xy[m] = newx
                xy[m + 1] = floor(newy)
                m = m + 2
                drawn = drawn + 1
            end
        end
        if m > 1 then
            ov("set "..table.concat(xy, " "))
        end

        -- display elapsed time
        g.show("Time to draw "..drawn.." pixels "..ms(g.millisecs() - t1))

        -- draw the exit message
        ov("blend 1")
        pastetext(floor((wd - w) / 2), 10)

        -- wait for frame time
        while g.millisecs() - t1 < 15 do
        end

        -- update display
        ov("update")
    end

    -- free clips and restore settings
    ov("delete gollyclip")
    ov("delete bg")
    ov("blend "..oldblend)
    ov("font "..oldfont)
end

--------------------------------------------------------------------------------

local function dot(x, y)
    local oldrgba = ov(op.red)
    ov("set "..x.." "..y)
    ov("rgba "..oldrgba)
end

--------------------------------------------------------------------------------

local function draw_rect(x0, y0, x1, y1)
    local oldrgba = ov(op.red)
    local oldwidth = ov("lineoption width 1")
    ov("line "..x0.." "..y0.." "..x1.." "..y0.." "..x1.." "..y1.." "..x0.." "..y1.." "..x0.." "..y0)
    ov("rgba "..oldrgba)
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function draw_line(x0, y0, x1, y1)
    ov("line "..x0.." "..y0.." "..x1.." "..y1)
end

--------------------------------------------------------------------------------

local function draw_ellipse(x, y, w, h)
    ov("ellipse "..x.." "..y.." "..w.." "..h)
    -- enable next call to check that ellipse is inside given rectangle
    -- draw_rect(x, y, x+w-1, y+h-1)
end

--------------------------------------------------------------------------------

local function radial_lines(x0, y0, length)
    draw_line(x0, y0, x0+length, y0)
    draw_line(x0, y0, x0, y0+length)
    draw_line(x0, y0, x0, y0-length)
    draw_line(x0, y0, x0-length, y0)
    for angle = 15, 75, 15 do
        local rad = angle * math.pi/180
        local xd = int(length * cos(rad))
        local yd = int(length * sin(rad))
        draw_line(x0, y0, x0+xd, y0+yd)
        draw_line(x0, y0, x0+xd, y0-yd)
        draw_line(x0, y0, x0-xd, y0+yd)
        draw_line(x0, y0, x0-xd, y0-yd)
    end
end

--------------------------------------------------------------------------------

local function vertical_lines(x, y)
    local oldwidth = ov("lineoption width 0.5")
    local len = 30
    local gap = 12
    draw_line(x, y, x, y+len)      dot(x,y)  dot(x,y+len)  x = x+gap
    for w = 1, 8 do
        ov("lineoption width "..w)
        draw_line(x, y, x, y+len)  dot(x,y)  dot(x,y+len)  x = x+gap
    end
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function diagonal_lines(x, y)
    local oldwidth = ov("lineoption width 0.5")
    local len = 30
    local gap = 12
    draw_line(x, y, x+len, y+len)      dot(x,y)  dot(x+len,y+len)  x = x+gap
    for w = 1, 8 do
        ov("lineoption width "..w)
        draw_line(x, y, x+len, y+len)  dot(x,y)  dot(x+len,y+len)  x = x+gap
    end
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function nested_ellipses(x, y)
    -- start with a circle
    local w = 91
    local h = 91
    draw_ellipse(x, y, w, h)
    for i = 1, 3 do
        draw_ellipse(x-i*10, y-i*15, w+i*20, h+i*30)
        draw_ellipse(x+i*10, y+i*15, w-i*20, h-i*30)
    end
end

--------------------------------------------------------------------------------

local function show_magnified_pixels(x, y)
    local radius = 5
    local numcols = radius*2+1
    local numrows = numcols
    local magsize = 14
    local boxsize = (1+magsize)*numcols+1

    -- get pixel colors
    local color = {}
    for i = 1, numrows do
        color[i] = {}
        for j = 1, numcols do
            color[i][j] = ov("get "..(x-radius-1+j).." "..(y-radius-1+i))
        end
    end

    -- save area in top left corner big enough to draw the magnifying glass
    local outersize = int(math.sqrt(boxsize*boxsize+boxsize*boxsize) + 0.5)
    ov("copy 0 0 "..outersize.." "..outersize.." outer_bg")
    local oldrgba = ov("rgba 0 0 0 0")
    local oldblend = ov("blend 0")
    ov("fill 0 0 "..outersize.." "..outersize)

    -- draw gray background (ie. grid lines around pixels)
    ov(op.gray)
    local xpos = int((outersize-boxsize)/2)
    local ypos = int((outersize-boxsize)/2)
    ov("fill "..xpos.." "..ypos.." "..boxsize.." "..boxsize)

    -- draw magnified pixels
    for i = 1, numrows do
        for j = 1, numcols do
            if #color[i][j] > 0 then
                ov("rgba "..color[i][j])
                local x = xpos+1+(j-1)*(magsize+1)
                local y = ypos+1+(i-1)*(magsize+1)
                ov("fill "..x.." "..y.." "..magsize.." "..magsize)
            end
        end
    end

    -- erase outer ring
    local oldwidth = ov("lineoption width "..int((outersize-boxsize)/2))
    ov("rgba 0 0 0 0")
    draw_ellipse(0, 0, outersize, outersize)

    -- surround with a gray circle
    ov(op.gray)
    ov("lineoption width 4")
    ov("blend 1")
    draw_ellipse(xpos-2, ypos-2, boxsize+4, boxsize+4)
    ov("blend 0")

    ov("copy 0 0 "..outersize.." "..outersize.." mag_box")

    -- restore background saved above
    ov("paste 0 0 outer_bg")
    ov("delete outer_bg")

    -- draw magnified circle with center at x,y
    xpos = int(x-outersize/2)
    ypos = int(y-outersize/2)
    ov("blend 1")
    ov("paste "..xpos.." "..ypos.." mag_box")
    ov("delete mag_box")

    -- restore settings
    ov("rgba "..oldrgba)
    ov("blend "..oldblend)
    ov("lineoption width "..oldwidth)
end

--------------------------------------------------------------------------------

local function test_lines()
    -- this test requires a bigger overlay
    local owd = 800
    local oht = 600
    ov("resize "..owd.." "..oht)

    ov(op.white)
    ov("fill")
    ov(op.black)

    local oldblend = ov("blend 0")
    local oldwidth = ov("lineoption width 1")

    -- non-antialiased lines (linewd = 1)
    radial_lines(100, 100, 50)

    -- antialiased lines (linewd = 1)
    ov("blend 1")
    radial_lines(220, 100, 50)
    ov("blend 0")

    -- thick non-antialiased lines
    ov("lineoption width 3")            -- 2 is same as 1!!!??? (2.5 is ok)
    radial_lines(100, 220, 50)
    vertical_lines(50, 300)
    diagonal_lines(50, 350)

    -- thick antialiased lines
    ov("blend 1")
    radial_lines(220, 220, 50)
    vertical_lines(170, 300)
    diagonal_lines(170, 350)
    ov("blend 0")

    -- non-antialiased ellipses (linewd = 1)
    ov("lineoption width 1")
    nested_ellipses(350, 100)

    -- antialiased ellipses (linewd = 1)
    ov("blend 1")
    nested_ellipses(520, 100)
    ov("blend 0")

    -- thick non-antialiased ellipses
    ov("lineoption width 3")
    nested_ellipses(350, 300)

    -- thick antialiased ellipses
    ov("blend 1")
    nested_ellipses(520, 300)
    ov("blend 0")
    ov("lineoption width 1")

    -- test overlapping translucent colors
    ov("blend 1")
    ov("lineoption width 20")
    local oldrgba = ov("rgba 0 255 0 128")
    draw_ellipse(50, 450, 100, 100)
    ov("rgba 255 0 0 128")
    draw_line(50, 450, 150, 550)
    ov("rgba 0 0 255 128")
    draw_line(150, 450, 50, 550)
    ov("blend 0")
    ov("lineoption width 1")
    ov("rgba "..oldrgba)

    -- draw filled ellipses using fill_ellipse function from oplus
    -- (no border if given border width is 0)
    op.fill_ellipse(370, 450, 50, 100, 0, "rgba 0 0 255 128")
    ov("rgba 200 200 200 128") -- translucent gray border
    op.fill_ellipse(300, 450, 100, 80, 15, op.green)
    ov("rgba "..oldrgba)
    op.fill_ellipse(200, 450, 140, 99, 2, "rgba 255 255 0 128")

    -- draw rounded rectangles using round_rect function from oplus
    op.round_rect(670, 50, 60, 40,  10,  3, "")
    op.round_rect(670, 100, 60, 40, 20,  0, "rgba 0 0 255 128")
    op.round_rect(670, 150, 60, 21,  3,  0, op.blue)
    ov("rgba 200 200 200 128") -- translucent gray border
    op.round_rect(700, 30, 70, 200, 35, 10, "rgba 255 255 0 128")
    ov("rgba "..oldrgba)

    -- draw some non-rounded rectangles (radius is 0)
    op.round_rect(670, 200, 10, 8, 0, 3, "")
    op.round_rect(670, 210, 10, 8, 0, 1, op.red)
    op.round_rect(670, 220, 10, 8, 0, 0, op.green)
    op.round_rect(670, 230, 10, 8, 0, 0, "")        -- nothing is drawn

    -- draw solid circles (non-antialiased and antialiased)
    ov("lineoption width 10")
    draw_ellipse(450, 500, 20, 20)
    ov("blend 1")
    draw_ellipse(480, 500, 20, 20)
    ov("blend 0")
    ov("lineoption width 1")

    -- draw solid ellipses (non-antialiased and antialiased)
    ov("lineoption width 11")
    draw_ellipse(510, 500, 21, 40)
    ov("blend 1")
    draw_ellipse(540, 500, 21, 40)
    ov("blend 0")
    ov("lineoption width 1")

    -- create a circular hole with fuzzy edges
    ov("rgba 255 255 255 0")
    ov("blend 0")
    ov("lineoption width 25")
    local x, y, w, h = 670, 470, 50, 50
    draw_ellipse(x, y, w, h)
    ov("lineoption width 1")
    local a = 0
    for i = 1, 63 do
        a = a + 4
        ov("rgba 255 255 255 "..a)
        -- we need to draw 2 offset ellipses to ensure all pixels are altered
        x = x-1
        w = w+2
        draw_ellipse(x, y, w, h)
        y = y-1
        h = h+2
        draw_ellipse(x, y, w, h)
    end
    ov("lineoption width 1")
    ov("rgba "..oldrgba)

    -- create and draw the exit message
    local oldfont = ov(demofont)
    ov("blend 1")
    local text
    if g.os() == "Mac" then
        text = "Click or hit the return key to return to the main menu."
    else
        text = "Click or hit the enter key to return to the main menu."
    end
    ov(op.black)
    local w,h = maketext(text)
    pastetext(10, oht - h - 10)
    maketext("Hit the M key to toggle the magnifying glass.")
    pastetext(10, 10)
    ov("font "..oldfont)

    g.update()

    ov("blend 0")
    ov("copy 0 0 0 0 bg")
    local showing_magnifier = false
    local display_magnifier = true
    local prevx, prevy

    -- loop until enter/return key pressed or mouse clicked
    while true do
        local event = g.getevent()
        if event:find("^oclick") or event == "key enter none" or event == "key return none" then
            -- tidy up and return to main menu
            ov("blend "..oldblend)
            ov("lineoption width "..oldwidth)
            ov("delete bg")
            return_to_main_menu = true
            return
        elseif event == "key m none" then
            -- toggle magnifier
            display_magnifier = not display_magnifier
            if showing_magnifier and not display_magnifier then
                ov("paste 0 0 bg")
                g.show("")
                g.update()
                showing_magnifier = false
            elseif display_magnifier and not showing_magnifier then
                -- force it to appear if mouse is in overlay
                prevx = -1
            end
        else
            -- might be a keyboard shortcut
            g.doevent(event)
        end

        -- track mouse and magnify pixels under cursor
        local xy = ov("xy")
        if #xy > 0 then
            local x, y = split(xy)
            x = tonumber(x)
            y = tonumber(y)
            if x ~= prevx or y ~= prevy then
                prevx = x
                prevy = y
                ov("paste 0 0 bg")
                if display_magnifier then
                    -- first show position and color of x,y pixel in status bar
                    g.show("xy: "..x.." "..y.."  rgba: "..ov("get "..x.." "..y))
                    show_magnified_pixels(x, y)
                    g.update()
                    showing_magnifier = true
                end
            end
        elseif showing_magnifier then
            ov("paste 0 0 bg")
            g.show("")
            g.update()
            showing_magnifier = false
        end
    end
end

--------------------------------------------------------------------------------

local function test_multiline_text()
    ::restart::

    -- resize overlay to cover entire layer
    wd, ht = g.getview( g.getlayer() )
    ov("resize "..wd.." "..ht)

    local oldfont = ov("font 10 mono-bold")   -- use a mono-spaced font

    -- draw solid background
    local oldblend = ov("blend 0")
    ov(op.blue)
    ov("fill")

    local textstr =
[[
"To be or not to be, that is the question;
Whether 'tis nobler in the mind to suffer
The slings and arrows of outrageous fortune,
Or to take arms against a sea of troubles,
And by opposing, end them. To die, to sleep;
No more; and by a sleep to say we end
The heart-ache and the thousand natural shocks
That flesh is heir to — 'tis a consummation
Devoutly to be wish'd. To die, to sleep;
To sleep, perchance to dream. Ay, there's the rub,
For in that sleep of death what dreams may come,
When we have shuffled off this mortal coil,
Must give us pause. There's the respect
That makes calamity of so long life,
For who would bear the whips and scorns of time,
Th'oppressor's wrong, the proud man's contumely,
The pangs of despised love, the law's delay,
The insolence of office, and the spurns
That patient merit of th'unworthy takes,
When he himself might his quietus make
With a bare bodkin? who would fardels bear,
To grunt and sweat under a weary life,
But that the dread of something after death,
The undiscovered country from whose bourn
No traveller returns, puzzles the will,
And makes us rather bear those ills we have
Than fly to others that we know not of?
Thus conscience does make cowards of us all,
And thus the native hue of resolution
Is sicklied o'er with the pale cast of thought,
And enterprises of great pitch and moment
With this regard their currents turn awry,
And lose the name of action.
Soft you now! The fair Ophelia! Nymph,
In thy Orisons be all my sins remembered.

Test non-ASCII: áàâäãåçéèêëíìîïñóòôöõúùûüæøœÿ
                ÁÀÂÄÃÅÇÉÈÊËÍÌÎÏÑÓÒÔÖÕÚÙÛÜÆØŒŸ
]]

    -- toggle the column alignments and transparency
    if align == "left" then
       align = "center"
    else 
        if align == "center" then
            align = "right"
        else
            if align == "right" then
                align = "left"
                transbg = 1 - transbg
                if transbg == 1 then
                    shadow = 1 - shadow
                end
            end
        end
    end

    -- set text alignment
    local oldalign = ov("textoption align "..align)

    -- set the text foreground color
    ov("rgba 255 255 255 255")

    -- set the text background color
    local oldbackground
    local transmsg
    if transbg == 1 then
        oldbackground = ov("textoption background 0 0 0 0")
        transmsg = "transparent background"
    else
        oldbackground = ov("textoption background 0 128 128 255")
        transmsg = "opaque background     "
    end

    local t1 = g.millisecs()

    ov("blend "..transbg)

    -- create the text clip
    if shadow == 0 then
        maketext(textstr)
    else
        maketext(textstr, nil, nil, 2, 2)
    end

    -- paste the clip onto the overlay
    local t2 = g.millisecs()
    t1 = t2 - t1
    pastetext(0, 0)

    -- output timing and drawing options
    local shadowmsg
    if shadow == 1 then
        shadowmsg = "on"
    else
        shadowmsg = "off"
    end
    g.show("Time to test multiline text: maketext "..ms(t1)..
           "  pastetext "..ms(g.millisecs() - t2)..
           "  align "..string.format("%-6s", align)..
           "  "..transmsg.."  shadow "..shadowmsg)

    -- restore old settings
    ov("textoption background "..oldbackground)
    ov("textoption align "..oldalign)
    ov("font "..oldfont)
    ov("blend "..oldblend)

    if repeat_test(" with different text options") then goto restart end
end

--------------------------------------------------------------------------------

local function test_text()
    ::restart::

    local t1 = g.millisecs()

    local oldfont, oldblend, w, h, descent, nextx

    oldblend = ov("blend 0")
    ov(op.white) -- white background
    ov("fill")
    ov(op.black) -- black text

    ov("blend 1")
    maketext("FLIP Y")
    pastetext(20, 30)
    pastetext(20, 30, op.flip_y)

    maketext("FLIP X")
    pastetext(110, 30)
    pastetext(110, 30, op.flip_x)

    maketext("FLIP BOTH")
    pastetext(210, 30)
    pastetext(210, 30, op.flip)

    maketext("ROTATE CW")
    pastetext(20, 170)
    pastetext(20, 170, op.rcw)

    maketext("ROTATE ACW")
    pastetext(20, 140)
    pastetext(20, 140, op.racw)

    maketext("SWAP XY")
    pastetext(150, 170)
    pastetext(150, 170, op.swap_xy)

    maketext("SWAP XY FLIP")
    pastetext(150, 140)
    pastetext(150, 140, op.swap_xy_flip)

    oldfont = ov("font 7 default")
    w, h, descent = maketext("tiny")
    pastetext(300, 30 - h + descent)
    nextx = 300 + w + 5

    ov("font "..oldfont)    -- restore previous font
    w, h, descent = maketext("normal")
    pastetext(nextx, 30 - h + descent)
    nextx = nextx + w + 5

    ov("font 20 default-bold")
    w, h, descent = maketext("Big")
    pastetext(nextx, 30 - h + descent)

    ov("font 10 default-bold")
    w = maketext("bold")
    pastetext(300, 40)
    nextx = 300 + w + 5

    ov("font 10 default-italic")
    maketext("italic")
    pastetext(nextx, 40)

    ov("font 10 mono")
    w, h, descent = maketext("mono")
    pastetext(300, 80 - h + descent)
    nextx = 300 + w + 5

    ov("font 12")   -- just change font size
    w, h, descent = maketext("mono12")
    pastetext(nextx, 80 - h + descent)

    ov("font 10 mono-bold")
    w = maketext("mono-bold")
    pastetext(300, 90)

    ov("font 10 mono-italic")
    maketext("mono-italic")
    pastetext(300, 105)

    ov("font 10 roman")
    maketext("roman")
    pastetext(300, 130)

    ov("font 10 roman-bold")
    w = maketext("roman-bold")
    pastetext(300, 145)

    ov("font 10 roman-italic")
    maketext("roman-italic")
    pastetext(300, 160)

    ov("font "..oldfont)    -- restore previous font

    ov(op.red)
    w, h, descent = maketext("RED")
    pastetext(300, 200 - h + descent)
    nextx = 300 + w + 5

    ov(op.green)
    w, h, descent = maketext("GREEN")
    pastetext(nextx, 200 - h + descent)
    nextx = nextx + w + 5

    ov(op.blue)
    w, h, descent = maketext("BLUE")
    pastetext(nextx, 200 - h + descent)

    ov(op.yellow)
    w, h = maketext("Yellow on black [] gjpqy")
    ov(op.black)
    ov("fill 300 210 "..w.." "..h)
    pastetext(300, 210)

    ov("blend 0")
    ov(op.yellow)       ov("fill 0   250 100 100")
    ov(op.cyan)         ov("fill 100 250 100 100")
    ov(op.magenta)      ov("fill 200 250 100 100")
    ov("rgba 0 0 0 0")  ov("fill 300 250 100 100")
    ov("blend 1")

    ov(op.black)
    maketext("The quick brown fox jumps over 123 dogs.")
    pastetext(10, 270)

    ov(op.white)
    maketext("SPOOKY")
    pastetext(310, 270)

    oldfont = ov("font "..rand(10,150).." default-bold")
    ov("rgba 255 0 0 40")   -- translucent red text
    w, h, descent = maketext("Golly")
    local gollyclip = pastetext(10, 10)

    -- draw box around text
    ov("line 10 10 "..(w-1+10).." 10 "..(w+1+10).." "..(h-1+10).." 10 "..(h-1+10).." 10 10")
    -- show baseline
    ov("line 10 "..(h-1+10-descent).." "..(w-1+10).." "..(h-1+10-descent))

    -- draw minimal bounding rect over text
    local xoff, yoff, minwd, minht = op.minbox(gollyclip, w, h)
    ov("rgba 0 0 255 20")
    ov("fill "..(xoff+10).." "..(yoff+10).." "..minwd.." "..minht)

    -- restore blend state and font
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov(op.black)

    g.show("Time to test text: "..ms(g.millisecs()-t1))

    if repeat_test(" with a different sized \"Golly\"", true) then goto restart end
end

--------------------------------------------------------------------------------

local function test_fill()
    ::restart::

    ov(op.white)
    ov("fill")

    toggle = 1 - toggle
    if toggle > 0 then
        ov("blend 1") -- turn on alpha blending
    end

    local maxx = wd-1
    local maxy = ht-1
    local t1 = g.millisecs()
    for i = 1, 1000 do
        ov("rgba "..rand(0,255).." "..rand(0,255).." "..rand(0,255).." "..rand(0,255))
        ov("fill "..rand(0,maxx).." "..rand(0,maxy).." "..rand(100).." "..rand(100))
    end
    g.show("Time to fill one thousand rectangles: "..ms(g.millisecs()-t1).."  blend "..toggle)
    ov("rgba 0 0 0 0")
    ov("fill 10 10 100 100") -- does nothing when alpha blending is on

    if toggle > 0 then
        ov("blend 0") -- turn off alpha blending
    end

    if repeat_test(" with a different blend setting") then goto restart end
end

--------------------------------------------------------------------------------

local target = 1

local function test_target()
    -- set overlay as the rendering target
    local oldtarget = ov("target")
    local oldfont = ov("font 16 mono")
    local oldblend = ov("blend 0")

    ::restart::

    target = 1 - target

    -- fill the overlay with black
    ov("blend 0")
    ov(op.black)
    ov("fill")

    -- create a 300x300 clip
    ov("create 300 300 clip")

    -- change the clip contents to yellow
    ov("target clip")
    ov(op.yellow)
    ov("fill")

    -- either draw to the overlay or clip
    if target == 0 then
        ov("target clip")
    else
        ov("target")
    end

    -- draw red, green and blue squares
    ov(op.red)
    ov("fill 0 0 "..wd.." 50")

    ov(op.green)
    ov("fill 0 50 "..wd.." 50")

    ov(op.blue)
    ov("fill 0 100 "..wd.. " 50")

    -- draw circle
    ov("blend 1")
    ov(op.white)
    op.fill_ellipse(0, 0, 100, 100, 1, op.magenta)

    -- draw some lines
    ov(op.cyan)
    for x = 0, wd, 20 do
        ov("line 0 0 "..x.." "..ht)
    end

    -- draw text label
    local textstring = "Clip"
    if target ~= 0 then
        textstring = "Overlay"
    end
    ov(op.black)
    maketext(textstring, nil, op.white, 2, 2)
    pastetext(0, 0)

    -- set overlay as the target
    ov("target")

    -- paste the clip
    ov("blend 0")
    ov("paste 200 0 clip")

    if repeat_test(" with a different target") then goto restart end

    -- free clip and restore previous target
    ov("delete clip")
    ov("target "..oldtarget)
    ov("font "..oldfont)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local batchsize = 2
local maxbatch  = 512

local function test_batch()
    ::restart::

    ov(op.black)
    ov("fill")

    -- udpate the batch size
    batchsize = batchsize * 2
    if batchsize > maxbatch then
        batchsize = 4
    end

    -- random coordinates
    local xy    = {}
    local xywh  = {}
    local items = batchsize
    local reps  = floor(20 * maxbatch / batchsize)

    -- create random positions
    local m = 1
    for i = 1, items do
        xy[m] = rand(0, wd - 1)
        xy[m + 1] = rand(0, ht - 1)
        xywh[m] = xy[m] - 2
        xywh[m + 1] = (xy[m + 1] - 2).." 5 5"
        m = m + 2
    end

    -- create list of all coordinates
    local xylist   = table.concat(xy, " ").." "..xy[1].." "..xy[2]
    local xywhlist = table.concat(xywh, " ")

    -- draw random lines
    ov(op.green)

    -- time draw one at a time
    local t3 = g.millisecs()
    for i = 1, reps do
        m = 1
        for i = 1, items do
            if i == items then
                ov("line "..xy[m].." "..xy[m + 1].." "..xy[1].." "..xy[2])
            else
                ov("line "..xy[m].." "..xy[m + 1].." "..xy[m + 2].." "..xy[m + 3])
            end
            m = m + 2
        end
    end
    t3 = g.millisecs() - t3

    -- time drawing all at once
    local t4 = g.millisecs()
    for i = 1, reps do
        ov("line "..xylist)
    end
    t4 = g.millisecs() - t4

    -- draw random rectangles
    ov(op.red)

    -- time draw one at a time
    local t5 = g.millisecs()
    for i = 1, reps do
        m = 1
        for i = 1, items do
            ov("fill "..xywh[m].." "..xywh[m + 1])
            m = m + 2
        end
    end
    t5 = g.millisecs() - t5

    -- time drawing all at once
    local t6 = g.millisecs()
    for i = 1, reps do
        ov("fill "..xywhlist)
    end
    t6 = g.millisecs() - t6

    -- draw random pixels
    ov(op.white)

    -- time drawing one at a time
    ov(op.white)
    local t1 = g.millisecs()
    for i = 1, reps do
        m = 1
        for i = 1, items do
            ov("set "..xy[m].." "..xy[m + 1])
            m = m + 2
        end
    end
    t1 = g.millisecs() - t1

    -- time drawing all at once
    local t2 = g.millisecs()
    for i = 1, reps do
        ov("set "..xylist)
    end
    t2 = g.millisecs() - t2

    g.show("reps: "..reps.."  items: "..items.."  pixels: single "..ms(t1).." batch "..ms(t2).."  lines: single "..ms(t3).." batch "..ms(t4).."  rectangles: single "..ms(t5).." batch "..ms(t6))

    -- create batch string
    if repeat_test(" with a different batch size") then goto restart end
end
 
--------------------------------------------------------------------------------

local function test_blending()
    ::restart::

    ov(op.white)
    ov("fill")

    toggle = 1 - toggle
    if toggle > 0 then
        ov("blend 1")           -- turn on alpha blending
    end

    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    ov(op.black)
    maketext("Alpha blending is turned on or off using the blend command:")
    pastetext(10, 10)
    maketext("blend "..oldblend)
    pastetext(40, 50)
    maketext("The blend command also controls antialiasing of lines and ellipses:")
    pastetext(10, 300)
    ov("blend "..oldblend)
    ov("font "..oldfont)

    ov("rgba 0 255 0 128")      -- 50% translucent green
    ov("fill 40 70 100 100")

    ov("rgba 255 0 0 128")      -- 50% translucent red
    ov("fill 80 110 100 100")

    ov("rgba 0 0 255 128")      -- 50% translucent blue
    ov("fill 120 150 100 100")

    ov(op.black)
    radial_lines(100, 400, 50)
    draw_ellipse(200, 350, 200, 100)
    draw_ellipse(260, 360, 80, 80)
    draw_ellipse(290, 370, 20, 60)
    -- draw a solid circle by setting the line width to the radius
    local oldwidth = ov("lineoption width 50")
    draw_ellipse(450, 350, 100, 100)
    ov("lineoption width "..oldwidth)

    if toggle > 0 then
        ov("blend 0")           -- turn off alpha blending
    end

    if repeat_test(" with a different blend setting", true) then goto restart end
end

--------------------------------------------------------------------------------

local function test_sound()
    local oldblend = ov("blend 0")
    ov(op.blue)
    ov("fill")
    ov("update")

    -- draw exit message
    ov("blend 1")
    ov(op.white)
    local oldfont = ov(demofont)
    local exitw = maketext("Click or press enter to return to the main menu.", nil, nil, 2, 2)
    pastetext(floor((wd - exitw) / 2), 500)

    -- draw commands
    ov("font 22 mono")
    ov(op.yellow)
    local w, h = maketext("sound play audio.wav", nil, nil, 2, 2)
    pastetext(floor((wd - w) / 2), 100)
    w, h = maketext("sound loop audio.wav", nil, nil, 2, 2)
    pastetext(floor((wd - w) / 2), 200)
    w, h = maketext("sound stop", nil, nil, 2, 2)
    pastetext(floor((wd - w) / 2), 300)

    -- draw controls
    ov("font 16 mono")
    ov(op.white)
    w, h = maketext("Press P to play sound", nil, nil, 2, 2)
    pastetext(floor((wd - w) / 2), 70)
    w, h = maketext("Press L to loop sound", nil, nil, 2, 2)
    pastetext(floor((wd - w) / 2), 170)
    w, h = maketext("Press S to stop sound", nil, nil, 2, 2)
    pastetext(floor((wd - w) / 2), 270)

    -- update screen then copy background
    ov("update")
    local bgclip = "bg"
    ov("copy 0 0 0 0 "..bgclip)

    local soundname = "oplus/sounds/levelcompleteloop.wav"
    local running = true

    -- main loop
    local command = "stop"
    while running do
        -- check for input
        local event = g.getevent()
        if event:find("^oclick") or event == "key enter none" or event == "key return none" then
            running = false
        elseif event == "key p none" then
            command = "play"
            ov("sound play "..soundname)
        elseif event == "key l none" then
            command = "loop"
            ov("sound loop "..soundname)
        elseif event == "key s none" then
            command = "stop"
            ov("sound stop")
        end

        -- draw background
        ov("blend 0")
        ov("paste 0 0 "..bgclip)
        -- draw last command
        ov("blend 1")
        ov(op.cyan)
        w, h = maketext("Last command: "..command, nil, nil, 2, 2)
        pastetext(floor((wd - w) / 2), 400)
        ov("update")
    end

    -- stop any sounds before exit
    ov("sound stop")

    -- no point calling repeat_test()
    return_to_main_menu = true
    ov("delete "..bgclip)
    ov("font "..oldfont)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local function test_mouse()
    ::restart::

    ov(op.black)
    ov("fill")
    ov(op.white)

    local oldfont = ov(demofont)
    local oldblend = ov("blend 1")
    local w, h
    if g.os() == "Mac" then
        maketext("Click and drag to draw.\n"..
                 "Option-click to flood.\n"..
                 "Control-click or right-click to change the color.")
        w, h = maketext("Hit the space bar to restart this test.\n"..
                        "Hit the return key to return to the main menu.", "botlines")
    else
        maketext("Click and drag to draw.\n"..
                 "Alt-click to flood. "..
                 "Control-click or right-click to change the color.")
        w, h = maketext("Hit the space bar to restart this test.\n"..
                        "Hit the enter key to return to the main menu.", "botlines")
    end
    pastetext(10, 10)
    pastetext(10, ht - 10 - h, op.identity, "botlines")
    ov("blend "..oldblend)
    ov("font "..oldfont)

    ov("cursor pencil")
    g.update()

    local mousedown = false
    local prevx, prevy
    while true do
        local event = g.getevent()
        if event == "key space none" then
            goto restart
        end
        if event == "key enter none" or event == "key return none" then
            return_to_main_menu = true
            return
        end
        if event:find("^oclick") then
            local _, x, y, button, mods = split(event)
            if mods == "alt" then
                ov("flood "..x.." "..y)
            else
                if mods == "ctrl" or button == "right" then
                    ov("rgba "..rand(0,255).." "..rand(0,255).." "..rand(0,255).." 255")
                end
                ov("set "..x.." "..y)
                mousedown = true
                prevx = x
                prevy = y
            end
            g.update()
        elseif event:find("^mup") then
            mousedown = false
        elseif #event > 0 then
            g.doevent(event)
        end

        local xy = ov("xy")
        if #xy > 0 then
            local x, y = split(xy)
            g.show("pixel at "..x..","..y.." = "..ov("get "..x.." "..y))
            if mousedown and (x ~= prevx or y ~= prevy) then
                ov("line "..prevx.." "..prevy.." "..x.." "..y)
                prevx = x
                prevy = y
                g.update()
            end
        else
            g.show("mouse is outside overlay")
        end
    end

    -- above loop checks for enter/return/space key
    -- if repeat_test() then goto restart end
end

--------------------------------------------------------------------------------

local function create_menu_buttons()
    local longest = "Text and Transforms"
    blend_button = op.button(       longest, test_blending)
    animation_button = op.button(   longest, test_animation)
    batch_button = op.button(       longest, test_batch)
    cellview_button = op.button(    longest, test_cellview)
    copy_button = op.button(        longest, test_copy_paste)
    cursor_button = op.button(      longest, test_cursors)
    set_button = op.button(         longest, test_set)
    fill_button = op.button(        longest, test_fill)
    line_button = op.button(        longest, test_lines)
    load_button = op.button(        longest, test_load)
    mouse_button = op.button(       longest, test_mouse)
    multiline_button = op.button(   longest, test_multiline_text)
    pos_button = op.button(         longest, test_positions)
    render_button = op.button(      longest, test_target)
    replace_button = op.button(     longest, test_replace)
    save_button = op.button(        longest, test_save)
    sound_button = op.button(       longest, test_sound)
    text_button = op.button(        longest, test_text)
    transition_button = op.button(  longest, test_transitions)

    -- change labels without changing button widths
    blend_button.setlabel(       "Alpha Blending", false)
    animation_button.setlabel(   "Animation", false)
    batch_button.setlabel(       "Batch Draw", false)
    cellview_button.setlabel(    "Cell View", false)
    copy_button.setlabel(        "Copy and Paste", false)
    cursor_button.setlabel(      "Cursors", false)
    set_button.setlabel(         "Drawing Pixels", false)
    fill_button.setlabel(        "Filling Rectangles", false)
    line_button.setlabel(        "Lines and Ellipses", false)
    load_button.setlabel(        "Loading Images", false)
    mouse_button.setlabel(       "Mouse Tracking", false)
    multiline_button.setlabel(   "Multi-line Text", false)
    pos_button.setlabel(         "Overlay Positions", false)
    render_button.setlabel(      "Render Target", false)
    replace_button.setlabel(     "Replacing Pixels", false)
    save_button.setlabel(        "Saving the Overlay", false)
    sound_button.setlabel(       "Sounds", false)
    text_button.setlabel(        "Text and Transforms", false)
    transition_button.setlabel(  "Transitions", false)
end

--------------------------------------------------------------------------------

local function main_menu()
    local numbutts = 19
    local buttwd = blend_button.wd
    local buttht = blend_button.ht
    local buttgap = 10
    local hgap = 20
    local textgap = 10

    local oldfont = ov("font 24 default-bold")
    ov(op.black)
    local w1, h1 = maketext("Welcome to the overlay!", "bigtext")
    ov(demofont)
    local w2, h2 = maketext("Click on a button to see what's possible.", "smalltext")
    local textht = h1 + textgap + h2

    -- resize overlay to fit buttons and text
    wd = hgap + buttwd + hgap + w1 + hgap
    ht = hgap + numbutts * buttht + (numbutts-1) * buttgap + hgap
    ov("resize "..wd.." "..ht)

    ov("position middle")
    ov("cursor arrow")
    ov(op.gray)
    ov("fill")
    ov(op.white)
    ov("fill 2 2 -4 -4")

    local x = hgap
    local y = hgap

    blend_button.show(x, y)         y = y + buttgap + buttht
    animation_button.show(x, y)     y = y + buttgap + buttht
    batch_button.show(x, y)         y = y + buttgap + buttht
    cellview_button.show(x, y)      y = y + buttgap + buttht
    copy_button.show(x, y)          y = y + buttgap + buttht
    cursor_button.show(x, y)        y = y + buttgap + buttht
    set_button.show(x, y)           y = y + buttgap + buttht
    fill_button.show(x, y)          y = y + buttgap + buttht
    line_button.show(x, y)          y = y + buttgap + buttht
    load_button.show(x, y)          y = y + buttgap + buttht
    mouse_button.show(x, y)         y = y + buttgap + buttht
    multiline_button.show(x, y)     y = y + buttgap + buttht
    pos_button.show(x, y)           y = y + buttgap + buttht
    render_button.show(x, y)        y = y + buttgap + buttht
    replace_button.show(x, y)       y = y + buttgap + buttht
    save_button.show(x, y)          y = y + buttgap + buttht
    sound_button.show(x, y)         y = y + buttgap + buttht
    text_button.show(x, y)          y = y + buttgap + buttht
    transition_button.show(x, y)

    local oldblend = ov("blend 1")

    x = hgap + buttwd + hgap
    y = int((ht - textht) / 2)
    pastetext(x, y, op.identity, "bigtext")
    x = x + int((w1 - w2) / 2)
    y = y + h1 + textgap
    pastetext(x, y, op.identity, "smalltext")

    ov("blend "..oldblend)
    ov("font "..oldfont)

    g.update()
    g.show(" ") -- clear any timing info

    -- wait for user to click a button
    return_to_main_menu = false
    while true do
        local event = op.process( g.getevent() )
        if return_to_main_menu then
            return
        end
        if #event > 0 then
            -- might be a keyboard shortcut
            g.doevent(event)
        end
    end
end

--------------------------------------------------------------------------------

local function main()
    create_overlay()
    create_menu_buttons()
    while true do
        main_menu()
    end
end

--------------------------------------------------------------------------------

local oldoverlay = g.setoption("showoverlay", 1)
local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons
local oldtile = g.setoption("tilelayers", 0)
local oldstack = g.setoption("stacklayers", 0)

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

-- delete the overlay and restore settings saved above
ov("delete")
g.setoption("showoverlay", oldoverlay)
g.setoption("showbuttons", oldbuttons)
g.setoption("tilelayers", oldtile)
g.setoption("stacklayers", oldstack)
if extra_layer then g.dellayer() end

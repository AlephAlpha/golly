-- This script shows how to use the geturl command.

local g = golly()

local prompt = [[
Download a file from a given URL.
It will be saved in your download folder
(as specified in Preferences > File) and
then opened according to the file's type.

Note that a .zip file can be unzipped by
appending "?unzip" to the URL.
]]

local urls = {
    "http://golly.sourceforge.net/patterns/waterbear.mc",
    "http://golly.sourceforge.net/patterns/caterpillar.mc.gz",
    "http://golly.sourceforge.net/patterns/JustFriends.zip",
    "http://golly.sourceforge.net/patterns/JustFriends.zip?unzip",
    "https://conwaylife.com/patterns/oca.zip?unzip",
    "http://golly.sourceforge.net/index.html",
    "https://sourceforge.net/projects/golly/files/golly/golly-5.0/README.txt",
    "https://www.trevorrow.com/golly/spaces%20in%20name.rle",
    "https://www.trevorrow.com/golly/popplot.png",
}

local url = g.getstring(prompt, urls[math.random(1,#urls)], "Download URL")
if url == "" then g.exit() end

-- extract filename from end of url
local filename = url:match("[^/]*$") or ""
if filename == "" then g.exit("The given URL does not specify a file!") end

-- if necessary, remove "?unzip" from end of .zip filename
local unzip = false
if filename:find("%.zip%?unzip$") then
    filename = filename:sub(1,-7)
    unzip = true
end

-- replace any "%20" with space
filename = filename:gsub("%%20", " ")

-- save file in user's download folder
local dirpath = g.getdir("download")
local filepath = dirpath..filename

--[[ g.geturl will overwrite an existing file so might be safer to ask if that's ok
local f = io.open(filepath,"rb")
if f then
    io.close(f)
    g.warn("OK to overwrite existing file?\n"..filepath)
    -- clicking Cancel will abort script
end
]]

if g.geturl(url, dirpath) then
    if unzip then
        g.note("Unzipped "..filepath, false) -- no Cancel button
    else
        g.show("Opening "..filepath)
        g.open(filepath)
    end
else
    g.exit("The download failed!")
end

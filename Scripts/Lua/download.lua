-- Download a file from a given URL and open it.

local g = golly()

local prompt = [[
Download a file from a given URL.
It will be saved in your download folder
(as specified in Preferences > File) and
then opened according to the file's type.
]]

local urls = {
    "http://golly.sourceforge.net/patterns/waterbear.mc",
    "http://golly.sourceforge.net/patterns/caterpillar.mc.gz",
    "http://golly.sourceforge.net/patterns/JustFriends.zip",
    "http://golly.sourceforge.net/index.html",
    "https://sourceforge.net/projects/golly/files/golly/golly-5.0/README.txt",
    "https://www.wxwidgets.org/downloads/logos/blocks.png"
}

local url = g.getstring(prompt, urls[math.random(1,#urls)], "Download URL")
if url == "" then g.exit() end

-- extract filename from end of url
local filename = url:match("[^/]*$") or ""
if filename == "" then g.exit("The given URL does not specify a file!") end

-- save file in user's download folder
local savepath = g.getdir("download")..filename

-- g.geturl will overwrite an existing file so safer to ask if that's ok
local f = io.open(savepath,"rb")
if f then
    io.close(f)
    g.warn("OK to overwrite existing file?\n"..savepath)
    -- clicking Cancel will abort script
end

if g.geturl(url, savepath) then
    g.show("Opening "..savepath)
    g.open(savepath)
else
    g.exit("The download failed!")
end

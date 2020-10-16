-- Fast [Rule][History|Super] to [Rule] converter by Michael Simkin,
--   intended to be mapped to a keyboard shortcut, e.g., Alt+J
-- Sanity checks and Lua translation by Dave Greene, May 2017
-- Creates special rule and runs it for one generation, then switches to Life 
-- Replace 2k + 1-> 1 and 2k -> 0
-- Preserves step and generation count

local g = golly()

local rule = g.getrule()
local algo = g.getalgo()

-- deal with bounded-universe syntax appropriately
ind = string.find(rule, ":")
suffix = ""
baserule = rule
if ind then
    suffix = rule:sub(ind)
    baserule = rule:sub(1,ind-1)
end

-- No effect if the current rule is not a Super algo rule
if algo ~= "Super" then g.exit("The current rule is not a [Rule]History or [Rule]Super rule.") end

-- If rulestring contains "Super" suffix, remove it and continue
if baserule:sub(-5) == "Super" then baserule = baserule:sub(1,#baserule-5) end

-- If rulestring contains "History" suffix, remove it and continue
if baserule:sub(-7) == "History" then baserule = baserule:sub(1, #baserule-7) end

ruletext = [[@RULE SuperToStandard
@TABLE
n_states:26
neighborhood:oneDimensional
symmetries:none
var a={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25}
var b={a}
var c={2,4,6,8,10,12,14,16,18,20,22,24}
var d={3,5,7,9,11,13,15,17,19,21,23,25}
c,a,b,0
d,a,b,1]]
   
local function CreateRule()
    local fname = g.getdir("rules").."SuperToStandard.rule"
    local f=io.open(fname,"r")
    if f~=nil then
        io.close(f)  -- rule already exists
    else 
        local f = io.open(fname, "w")
        if f then
            f:write(ruletext)
            f:close()
        else
            g.warn("Can't save SuperToStandard rule in filename:\n"..filename)
        end
    end
end
      
CreateRule()
g.setrule("SuperToStandard")
g.run(1)
step = g.getstep()

g.setrule(baserule .. suffix)
g.setalgo("HashLife")
g.setstep(step)
g.setgen("-1")
SaveDemo = setmetatable({}, { __index = ActorScript })
SaveDemo.__index = SaveDemo

-- The save-game sample: turns its entity, saves the game on frame 30 and loads that save on
-- frame 60, so the entity jumps back to where it was on frame 30 and turns on from there.
Properties = {
    speed = { type = "number", default = 90.0, tooltip = "Degrees per second about y" },
    slot = { type = "string", default = "SaveDemo", tooltip = "The save slot written and read" },
}

local SAVE_FRAME = 30
local LOAD_FRAME = 60

function SaveDemo:new()
    return setmetatable(ActorScript:new(), SaveDemo)
end

function SaveDemo:OnUpdate(dt)
    local transform = self.entity.transform
    local angles = transform.eulerAngles
    angles.y = angles.y + self.speed * dt
    transform.eulerAngles = angles

    if Time.frame == SAVE_FRAME then
        self.savedAngle = angles.y
        Log.info("SaveDemo: saving slot " .. self.slot .. " at frame " .. Time.frame .. ": " .. tostring(Save.write(self.slot)))
    elseif Time.frame == LOAD_FRAME then
        Log.info("SaveDemo: loading slot " .. self.slot .. " at frame " .. Time.frame .. ": " .. tostring(Save.load(self.slot)))
    end
end

-- What the save keeps beside the world: the angle the entity had when it saved.
function SaveDemo:OnSave()
    return { angle = self.savedAngle }
end

-- The loaded instance, in place of OnStart: the world is the saved one again.
function SaveDemo:OnLoad(state)
    local angle = self.entity.transform.eulerAngles.y
    if math.abs(angle - state.angle) < 0.001 then
        Log.info("SaveDemo: loaded; the entity is back at " .. string.format("%.1f", angle) .. " degrees, as saved")
    else
        Log.error("SaveDemo: loaded at " .. angle .. " degrees, but the save was made at " .. state.angle)
    end
end

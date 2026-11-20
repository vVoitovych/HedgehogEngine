LoadingScreen = setmetatable({}, { __index = ActorScript })
LoadingScreen.__index = LoadingScreen

-- The loading screen sample: loads Pbr.yaml in the background while this scene keeps rendering,
-- drives the bar and its label from the load's progress, and is replaced by Pbr.yaml at the swap.
function LoadingScreen:new()
    return setmetatable(ActorScript:new(), LoadingScreen)
end

local TARGET = "Scenes/Pbr.yaml"
local BAR_WIDTH = 800

function LoadingScreen:OnStart()
    self.fill = Scene.find("BarFill"):getUiRect()
    self.label = Scene.find("Percent"):getUiText()
    self.done = false
    Log.info("LoadingScreen: loading " .. TARGET .. " in the background: " .. tostring(Scene.loadAsync(TARGET)))
end

function LoadingScreen:OnUpdate(dt)
    local progress = Scene.loadProgress()
    self.fill.width = BAR_WIDTH * progress
    self.label.text = string.format("%d%%", math.floor(progress * 100))
    if not self.done and progress >= 1 then
        -- The background work is done; the scene is swapped in at the end of this frame.
        self.done = true
        Log.info("LoadingScreen: " .. TARGET .. " is loaded")
    end
end

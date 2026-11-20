OptionsMenu = setmetatable({}, { __index = ActorScript })
OptionsMenu.__index = OptionsMenu

-- The player settings sample: each value button cycles its setting, which waits until APPLY;
-- SAVE writes what is applied, and the rebound key, to the player's folder; REVERT reads the file back.
-- Escape (UiCancel) returns to the game-flow menu.
function OptionsMenu:new()
    return setmetatable(ActorScript:new(), OptionsMenu)
end

local VOLUMES = { 0, 0.25, 0.5, 0.75, 1 }
local FRAME_CAPS = { 0, 30, 60, 120, 144, 240 }
local WINDOW_MODES = { "Windowed", "Borderless", "Fullscreen" }

-- The entry after current in list (the first when current is not in it).
local function nextIn(list, current)
    for i, value in ipairs(list) do
        if value == current or (type(value) == "number" and math.abs(value - current) < 0.01) then
            return list[i % #list + 1]
        end
    end
    return list[1]
end

local function percent(volume)
    return string.format("%d%%", math.floor(volume * 100 + 0.5))
end

-- The project's size first, then each size the monitor offers.
local function nextResolution()
    local sizes = { { 0, 0 } }
    for _, mode in ipairs(PlayerSettings.getResolutions()) do
        sizes[#sizes + 1] = { mode.width, mode.height }
    end
    local index = 1
    for i, size in ipairs(sizes) do
        if size[1] == PlayerSettings.width and size[2] == PlayerSettings.height then
            index = i % #sizes + 1
        end
    end
    PlayerSettings.width, PlayerSettings.height = sizes[index][1], sizes[index][2]
end

function OptionsMenu:row(name, change, describe)
    local text = Scene.find(name .. "Value"):getUiText()
    self.rows[#self.rows + 1] = { text = text, describe = describe }
    Scene.find(name .. "Value"):getUiButton():onClick(function()
        change()
        self:refresh()
    end)
end

function OptionsMenu:button(name, action)
    Scene.find(name .. "Button"):getUiButton():onClick(function()
        self.status.text = action()
        Log.info("OptionsMenu: " .. self.status.text)
        self:refresh()
    end)
end

function OptionsMenu:refresh()
    for _, row in ipairs(self.rows) do
        row.text.text = row.describe()
    end
end

function OptionsMenu:OnStart()
    self.rows = {}
    self.status = Scene.find("Status"):getUiText()

    self:row("Resolution", nextResolution, function()
        if PlayerSettings.width == 0 then
            return "Project's"
        end
        return PlayerSettings.width .. " x " .. PlayerSettings.height
    end)
    self:row("WindowMode", function()
        PlayerSettings.windowMode = nextIn(WINDOW_MODES, PlayerSettings.windowMode)
    end, function() return PlayerSettings.windowMode end)
    self:row("VSync", function() PlayerSettings.vsync = not PlayerSettings.vsync end,
        function() return PlayerSettings.vsync and "On" or "Off" end)
    self:row("FrameCap", function() PlayerSettings.maxFps = nextIn(FRAME_CAPS, PlayerSettings.maxFps) end,
        function() return PlayerSettings.maxFps == 0 and "No cap" or (PlayerSettings.maxFps .. " FPS") end)
    self:row("MasterVolume", function()
        PlayerSettings.masterVolume = nextIn(VOLUMES, PlayerSettings.masterVolume)
    end, function() return percent(PlayerSettings.masterVolume) end)
    self:row("MusicVolume", function()
        PlayerSettings.setBusVolume("Music", nextIn(VOLUMES, PlayerSettings.getBusVolume("Music")))
    end, function() return percent(PlayerSettings.getBusVolume("Music")) end)
    self:row("Quality", function()
        PlayerSettings.quality = nextIn(PlayerSettings.getQualityLevels(), PlayerSettings.quality)
    end, function() return PlayerSettings.quality end)
    self:row("SubmitKey", function() Input.listen("UiSubmit", 1) end, function()
        if Input.isListening() then
            return "Press a key (Esc cancels)"
        end
        return Input.getBindings("UiSubmit")[1].name
    end)

    self:button("Apply", function()
        PlayerSettings.apply()
        return "Applied"
    end)
    self:button("Save", function()
        local saved = PlayerSettings.save() and Input.saveBindings()
        return saved and "Saved" or "Could not save"
    end)
    self:button("Revert", function()
        return PlayerSettings.revert() and "Reverted to the saved settings" or "Nothing saved yet"
    end)

    Events.subscribe("InputRebound", function(rebound)
        self.status.text = rebound.cancelled and "Kept the Submit key" or ("Submit is now " .. rebound.binding.name)
        self:refresh()
    end)
    self:refresh()
end

function OptionsMenu:OnUpdate(dt)
    -- While a key is being listened for every action reads up, so Escape there only cancels.
    if Input.wasPressed("UiCancel") then
        Scene.load("Scenes/GameFlowMenu.yaml")
    end
end

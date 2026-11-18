GameFlowLevel = setmetatable({}, { __index = ActorScript })
GameFlowLevel.__index = GameFlowLevel

-- The game-flow sample's level: a locked cursor turns the camera, Escape goes back to the menu.
Properties = {
    sensitivity = { type = "number", default = 0.15, tooltip = "Degrees per pixel of mouse movement" },
    yaw = { type = "number", default = 0.0, tooltip = "Degrees about the world's up axis" },
    pitch = { type = "number", default = 0.0, tooltip = "Degrees above the horizon" },
}

local MAX_PITCH = 85

function GameFlowLevel:new()
    return setmetatable(ActorScript:new(), GameFlowLevel)
end

function GameFlowLevel:OnStart()
    -- GameState outlives the scene, so this counts how often the menu sent the player here.
    GameState.visits = (GameState.visits or 0) + 1
    Log.info("GameFlowLevel: visit " .. GameState.visits)
    Scene.find("VisitsLabel"):getUiText().text = "Visit " .. GameState.visits .. " - Esc: menu"
    Cursor.lock()
end

function GameFlowLevel:OnUpdate(dt)
    if Input.wasPressed("UiCancel") then
        Scene.load("Scenes/GameFlowMenu.yaml")
        return
    end

    local dx, dy = Input.pointerDelta()
    if dx == 0 and dy == 0 then
        return
    end
    self.yaw = self.yaw - dx * self.sensitivity
    self.pitch = math.max(-MAX_PITCH, math.min(MAX_PITCH, self.pitch - dy * self.sensitivity))
    -- The world is Z-up and the camera looks down its local -Z: a quarter turn about x faces it along +Y.
    self.entity.transform.rotation = Quat.axisAngle(Vector3(0, 0, 1), self.yaw)
        * Quat.axisAngle(Vector3(1, 0, 0), 90 + self.pitch)
end

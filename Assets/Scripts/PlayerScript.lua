PlayerScript = setmetatable({}, { __index = ActorScript })
PlayerScript.__index = PlayerScript

-- Shown in the inspector and saved with the scene; each entity's values are on its self.
Properties = {
    speed = { type = "number", default = 1.0, tooltip = "Degrees per second" },
    clockWise = true,
}

function PlayerScript:new()
    local self = setmetatable(ActorScript:new(), PlayerScript)
    return self
end

-- Turns the entity about its z axis at speed degrees per second.
function PlayerScript:OnUpdate(dt)
    local transform = self.entity.transform
    local rot = transform.eulerAngles
    if self.clockWise then
        rot.z = rot.z + self.speed * dt
    else
        rot.z = rot.z - self.speed * dt
    end
    transform.eulerAngles = rot
end

PlayerScript = setmetatable({}, { __index = ActorScript })
PlayerScript.__index = PlayerScript

speed = 1.0
clockWise = true;

function PlayerScript:new()
    local self = setmetatable(ActorScript:new(), PlayerScript)
    return self
end

-- Turns the entity about its z axis at speed degrees per second.
function PlayerScript:OnUpdate(dt)
    local transform = self.entity.transform
    local rot = transform.eulerAngles
    if clockWise then
        rot.z = rot.z + speed * dt
    else
        rot.z = rot.z - speed * dt
    end
    transform.eulerAngles = rot
end




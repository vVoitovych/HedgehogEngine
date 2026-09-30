PlayerScript = setmetatable({}, { __index = ActorScript })
PlayerScript.__index = PlayerScript

speed = 1.0
clockWise = true;

function PlayerScript:new()
    local self = setmetatable(ActorScript:new(), PlayerScript)
    return self
end

function PlayerScript:OnUpdate(dt)
    local rot = GetRotation()
    if clockWise then
        rot.z = rot.z + speed * dt
    else
        rot.z = rot.z - speed * dt
    end
    SetRotation(rot)
end




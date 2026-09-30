PlayerScript = setmetatable({}, { __index = ActorScript })
PlayerScript.__index = PlayerScript

speed = 1.0
clockWise = true;

function PlayerScript:new()
    local self = setmetatable(ActorScript:new(), PlayerScript)
    return self
end

function PlayerScript:OnUpdate(dt)
    local transform = self.entity.transform
    local angles = transform.eulerAngles
    if clockWise then
        angles.z = angles.z + speed * dt
    else
        angles.z = angles.z - speed * dt
    end
    transform.eulerAngles = angles
end




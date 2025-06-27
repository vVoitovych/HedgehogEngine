PlayerScript = setmetatable({}, { __index = ActorScript })
PlayerScript.__index = PlayerScript

function PlayerScript:new()
    local self = setmetatable(ActorScript:new(), PlayerScript)
    return self
end

-- function PlayerScript:OnEnable()
--     print("Player enabled")
-- end

function PlayerScript:OnUpdate(dt)
    local rot = GetRotation()
    rot.z = rot.z + 0.5 * dt
    SetRotation(rot)
end

-- function PlayerScript:OnDisable()
--     print("Player disabled")
-- end



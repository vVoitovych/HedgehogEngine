ActorScript = {}
ActorScript.__index = ActorScript

function ActorScript:new()
    local self = setmetatable({}, ActorScript)
    return self
end

function ActorScript:OnEnable()
    print("Base enabled")
end

function ActorScript:OnDisable()
    print("Base disabled")
end

function ActorScript:OnUpdate(dt)
end


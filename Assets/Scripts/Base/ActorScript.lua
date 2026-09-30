ActorScript = {}
ActorScript.__index = ActorScript

function ActorScript:new()
    local self = setmetatable({}, ActorScript)
    return self
end

function ActorScript:OnEnable()
    Log.info("Base enabled")
end

function ActorScript:OnDisable()
    Log.info("Base disabled")
end

function ActorScript:OnUpdate(dt)
end


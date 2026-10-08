Paddle = setmetatable({}, { __index = ActorScript })
Paddle.__index = Paddle

-- The kinematic paddle of the physics sample: swings along x about where it starts, moving its
-- transform every fixed step, which its body follows and pushes dynamic bodies with.
Properties = {
    speed = { type = "number", default = 2.0, min = 0.0, tooltip = "Metres per second as it passes its start" },
    range = { type = "number", default = 2.5, min = 0.1, tooltip = "How far it swings either way, in metres" },
}

function Paddle:new()
    return setmetatable(ActorScript:new(), Paddle)
end

function Paddle:OnStart()
    self.startX = self.entity.transform.position.x
end

function Paddle:OnFixedUpdate(dt)
    local position = self.entity.transform.position
    position.x = self.startX + self.range * math.sin(Time.time * self.speed / self.range)
    self.entity.transform.position = position
end

function Paddle:OnCollisionEnter(other, contact)
    if not self.touched then
        self.touched = true
        Log.info("Paddle: pushed " .. other.name)
    end
end

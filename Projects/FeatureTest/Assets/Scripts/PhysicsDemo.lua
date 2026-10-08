PhysicsDemo = setmetatable({}, { __index = ActorScript })
PhysicsDemo.__index = PhysicsDemo

-- The physics sample, on the ball: pushes it as Play starts, logs the first thing it lands on and
-- each time it enters a trigger zone, and on frame 10 casts a ray straight down onto the stack.
Properties = {
    push = { type = "vector3", default = Vector3(0, 1, 0), tooltip = "The impulse given to the ball as Play starts" },
    rayFrom = { type = "vector3", default = Vector3(-3, 0, 10), tooltip = "Where the frame-10 ray starts; it points down" },
}

local RAY_FRAME = 10

function PhysicsDemo:new()
    return setmetatable(ActorScript:new(), PhysicsDemo)
end

function PhysicsDemo:OnStart()
    self.entity:getRigidBody():addImpulse(self.push)
end

function PhysicsDemo:OnUpdate(dt)
    if Time.frame == RAY_FRAME then
        local hit = Physics.raycast(self.rayFrom, Vector3(0, 0, -1))
        Log.info("PhysicsDemo: the ray down hit " .. (hit and hit.entity.name or "nothing"))
    end
end

-- Every bounce is a new contact; only the first landing is logged.
function PhysicsDemo:OnCollisionEnter(other, contact)
    if not self.landed then
        self.landed = true
        Log.info("PhysicsDemo: ball landed on " .. other.name)
    end
end

function PhysicsDemo:OnTriggerEnter(other)
    Log.info("PhysicsDemo: " .. self.entity.name .. " entered the zone " .. other.name)
end

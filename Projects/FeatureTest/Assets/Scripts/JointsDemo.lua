JointsDemo = setmetatable({}, { __index = ActorScript })
JointsDemo.__index = JointsDemo

-- The joints sample, on the door: on frame 30 it finds the chain's links with an overlap and the door
-- with a sphere cast, then pushes the door open and logs once when its hinge passes 45 degrees.
Properties = {
    pushTime = { type = "number", default = 0.5, min = 0, tooltip = "When the door is pushed, in seconds of Play" },
    push = { type = "number", default = 3, min = 0, tooltip = "The push's impulse, newton seconds" },
}

function JointsDemo:new()
    return setmetatable(ActorScript:new(), JointsDemo)
end

function JointsDemo:OnStart()
    self.frame = 0
    self.time = 0
end

function JointsDemo:OnUpdate(dt)
    self.frame = self.frame + 1
    self.time = self.time + dt
    if self.frame == 30 then
        local links = Physics.overlapBox(Vector3(0, 0, 2.7), Vector3(0.5, 0.5, 1.2), nil)
        Log.info("JointsDemo: overlap found " .. #links .. " links")
        local hit = Physics.sphereCast(Vector3(-4.95, -3, 1.5), 0.2, Vector3(0, 1, 0), 10)
        if hit then
            Log.info("JointsDemo: sphere cast hit " .. hit.entity.name)
        end
    end
    if not self.pushed and self.time >= self.pushTime then
        self.pushed = true
        self.entity:getRigidBody():addImpulse(Vector3(0, self.push, 0))
    end
    if not self.opened and math.abs(self.entity:getJoint().angle) > 45 then
        self.opened = true
        Log.info("JointsDemo: door opened past 45 degrees")
    end
end

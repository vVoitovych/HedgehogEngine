AnimationDemo = setmetatable({}, { __index = ActorScript })
AnimationDemo.__index = AnimationDemo

-- The advanced animation sample, on the blend tree strip. It sweeps the strip's Blend parameter
-- (Animations/Locomotion.animctrl, a 1D tree of Bend and Lift) from 0 to 1 and back, puts the
-- Walker strip, which applies root motion, back where it started each time Reach has carried it
-- 1 m, and moves a Target in a circle that the Arm's hand reaches for with an IK goal.
Properties = {
    sweepTime = { type = "number", default = 1.0, min = 0.1, tooltip = "Seconds from Blend 0 to 1" },
    radius = { type = "number", default = 0.4, min = 0.0, tooltip = "The target circle's radius" },
}

function AnimationDemo:new()
    return setmetatable(ActorScript:new(), AnimationDemo)
end

function AnimationDemo:OnStart()
    self.time = 0
    self.logged = {}
    self.walker = Scene.find("Walker")
    self.walkerStart = self.walker.transform.position
    self.arm = Scene.find("Arm")
    self.target = Scene.find("Target")
    self.center = self.target.transform.position
end

function AnimationDemo:Once(key, line)
    if not self.logged[key] then
        self.logged[key] = true
        Log.info("AnimationDemo: " .. line)
    end
end

function AnimationDemo:OnUpdate(dt)
    self.time = self.time + dt

    -- Blend sweeps 0 -> 1 -> 0.
    local phase = (self.time / self.sweepTime) % 2
    local blend = phase <= 1 and phase or 2 - phase
    if phase >= 1 and not self.logged.blend then
        blend = 1
        self:Once("blend", "blend at 1")
    end
    self.entity:getAnimator():setFloat("Blend", blend)

    -- Reach moves the walker 1 m a loop; back to its start each time.
    local walked = self.walker.transform.position - self.walkerStart
    if walked:length() >= 0.999 then
        self:Once("root", "root moved 1 m")
        self.walker.transform.position = self.walkerStart
    end

    -- The hand was drawn at the goal set last frame; the target moves on.
    local hand = self.arm:getAnimator():getJointPosition("Hand")
    if self.goal and hand and (hand - self.goal):length() < 0.01 then
        self:Once("hand", "hand on target")
    end
    local angle = self.time * 2
    self.goal = self.center + Vector3(math.cos(angle) * self.radius, math.sin(angle) * self.radius, 0)
    self.target.transform.position = self.goal
    self.arm:getAnimator():setIkGoal("Hand", self.goal)
end

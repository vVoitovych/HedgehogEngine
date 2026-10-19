StateMachineDemo = setmetatable({}, { __index = ActorScript })
StateMachineDemo.__index = StateMachineDemo

-- The animator controller sample: drives Animations/TwoBoneStrip.animctrl through its
-- parameters. It flips the Lifted bool every few seconds, so the strip crossfades between its
-- Bending and Lifting states, sets the Wave trigger once, and logs every state change.
Properties = {
    interval = { type = "number", default = 2.0, min = 0.1, tooltip = "Seconds between flips of Lifted" },
    waveAt = { type = "number", default = 1.0, min = 0.0, tooltip = "Seconds after Play to set the Wave trigger" },
}

function StateMachineDemo:new()
    return setmetatable(ActorScript:new(), StateMachineDemo)
end

function StateMachineDemo:OnStart()
    self.elapsed = 0
    self.waved = false
    Events.subscribe("AnimatorStateChanged", function(event)
        if event.entity == self.entity then
            Log.info("StateMachineDemo: " .. (event.from or "(start)") .. " -> " .. event.to)
        end
    end)
end

function StateMachineDemo:OnUpdate(dt)
    local animator = self.entity:getAnimator()
    self.elapsed = self.elapsed + dt
    if not self.waved and self.elapsed >= self.waveAt then
        self.waved = true
        animator:setTrigger("Wave")
    end
    if self.elapsed >= self.interval then
        self.elapsed = self.elapsed - self.interval
        animator:setBool("Lifted", not animator:getParameter("Lifted"))
    end
end

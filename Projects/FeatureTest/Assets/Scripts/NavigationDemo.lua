NavigationDemo = setmetatable({}, { __index = ActorScript })
NavigationDemo.__index = NavigationDemo

-- The navigation sample, on each agent: walks through the maze and up the ramp to the platform,
-- then back to where it started, and again, logging its first arrival at the platform once.

function NavigationDemo:new()
    return setmetatable(ActorScript:new(), NavigationDemo)
end

function NavigationDemo:OnStart()
    self.agent = self.entity:getNavAgent()
    self.home = self.entity.transform.position
    local platform = Scene.find("Platform").transform.position
    self.platform = Vector3(platform.x, platform.y, platform.z + 0.5) -- its top
    self.target = self.platform
    self.walking = false
end

function NavigationDemo:OnUpdate(dt)
    -- The navmesh loads with the scene's first frames, so keep asking until a path is found.
    if not self.walking then
        self.walking = self.agent:setDestination(self.target)
    end
end

function NavigationDemo:OnNavAgentArrived()
    if self.target == self.platform then
        if not self.logged then
            self.logged = true
            Log.info("NavigationDemo: " .. self.entity.name .. " arrived at Platform")
        end
        self.target = self.home
    else
        self.target = self.platform
    end
    self.walking = false
end

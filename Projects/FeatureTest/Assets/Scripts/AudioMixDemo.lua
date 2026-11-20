AudioMixDemo = setmetatable({}, { __index = ActorScript })
AudioMixDemo.__index = AudioMixDemo

-- The mixer sample: halves the Music bus a second into Play and puts it back half a second later,
-- logging both. Bus volumes outlive Stop, so it is put back on Stop too.
function AudioMixDemo:new()
    return setmetatable(ActorScript:new(), AudioMixDemo)
end

function AudioMixDemo:OnStart()
    self.time = 0
    self.halved = false
    self.restored = false
end

function AudioMixDemo:OnUpdate(dt)
    self.time = self.time + dt
    if not self.halved and self.time >= 1.0 then
        self.halved = true
        Audio.setBusVolume("Music", 0.5)
        Log.info("AudioMixDemo: Music bus at", Audio.getBusVolume("Music"))
    end
    if self.halved and not self.restored and self.time >= 1.5 then
        self.restored = true
        Audio.setBusVolume("Music", 1)
        Log.info("AudioMixDemo: Music bus back at", Audio.getBusVolume("Music"))
    end
end

function AudioMixDemo:OnDestroy()
    if self.halved and not self.restored then
        Audio.setBusVolume("Music", 1)
    end
end

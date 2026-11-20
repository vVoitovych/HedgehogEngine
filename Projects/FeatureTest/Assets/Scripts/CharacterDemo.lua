CharacterDemo = setmetatable({}, { __index = ActorScript })
CharacterDemo.__index = CharacterDemo

-- The character sample, on the player: walks along +x through the trigger zone, up the ramp, over the
-- platform and down its steps into the crate, jumping at 3 s, and logs each first once.
Properties = {
    speed = { type = "number", default = 3, min = 0, max = 10, tooltip = "The walk's speed along +x, m/s" },
    jumpTime = { type = "number", default = 3, min = 0, tooltip = "When it jumps, in seconds of Play" },
    jumpSpeed = { type = "number", default = 5, min = 0, tooltip = "The jump's speed up, m/s" },
}

function CharacterDemo:new()
    return setmetatable(ActorScript:new(), CharacterDemo)
end

function CharacterDemo:OnStart()
    self.character = self.entity:getCharacterController()
    self.time = 0
    self.character:move(Vector3(self.speed, 0, 0))
end

function CharacterDemo:OnUpdate(dt)
    self.time = self.time + dt
    local character = self.character
    if not self.grounded and character:isGrounded() and character.groundEntity then
        self.grounded = true
        Log.info("CharacterDemo: grounded on " .. character.groundEntity.name)
    end
    if not self.jumped and self.time >= self.jumpTime then
        self.jumped = true
        character:jump(self.jumpSpeed)
        Log.info("CharacterDemo: jumped")
    end
end

function CharacterDemo:OnTriggerEnter(other)
    Log.info("CharacterDemo: entered the zone " .. other.name)
end

CharacterCrate = setmetatable({}, { __index = ActorScript })
CharacterCrate.__index = CharacterCrate

-- The character sample's crate: logs the first time the player walks into it.
function CharacterCrate:new()
    return setmetatable(ActorScript:new(), CharacterCrate)
end

function CharacterCrate:OnCollisionEnter(other, contact)
    if not self.pushed and other:isValid() and other.name == "Player" then
        self.pushed = true
        Log.info("CharacterDemo: pushed " .. self.entity.name)
    end
end

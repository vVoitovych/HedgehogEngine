function OnEnable() 
    print("Script started")
end  

function OnDisable() 
    print("Script stoped")
end  

function OnUpdate(dt) 
    local rotation = GetRotation()
    rotation.z = rottation.z + 0.1 * dt
    SetRotation(rotation)
end


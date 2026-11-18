GameFlowMenu = setmetatable({}, { __index = ActorScript })
GameFlowMenu.__index = GameFlowMenu

-- The game-flow sample's menu: PLAY loads the level, QUIT ends the game (stops Play in the editor).
function GameFlowMenu:new()
    return setmetatable(ActorScript:new(), GameFlowMenu)
end

function GameFlowMenu:OnStart()
    -- The level locks the cursor, and a lock outlives Scene.load.
    Cursor.unlock()

    Scene.find("PlayButton"):getUiButton():onClick(function()
        Log.info("GameFlowMenu: Play")
        Scene.load("Scenes/GameFlowLevel.yaml")
    end)
    Scene.find("QuitButton"):getUiButton():onClick(function()
        Log.info("GameFlowMenu: Quit")
        Application.quit()
    end)
end

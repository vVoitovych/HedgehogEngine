LodGrid = setmetatable({}, { __index = ActorScript })
LodGrid.__index = LodGrid

-- The levels of detail sample: spawns a grid of helmets receding from the camera as Play starts, so
-- the near rows draw the full mesh and the far ones its simplified levels.
Properties = {
    helmet = AssetRef("Prefab", "Prefabs/Helmet.prefab"),
    columns = { type = "number", default = 20, min = 1, max = 40, tooltip = "Helmets across" },
    rows = { type = "number", default = 20, min = 1, max = 40, tooltip = "Rows away from the camera" },
    spacing = { type = "number", default = 2.5, min = 1, tooltip = "Metres between helmets across" },
    rowSpacing = { type = "number", default = 5, min = 1, tooltip = "Metres between rows" },
}

function LodGrid:new()
    return setmetatable(ActorScript:new(), LodGrid)
end

function LodGrid:OnStart()
    local turn = Vector3(90, 0, 0)
    local left = -(self.columns - 1) * self.spacing / 2
    local count = 0
    for row = 0, self.rows - 1 do
        for column = 0, self.columns - 1 do
            local position = Vector3(left + column * self.spacing, row * self.rowSpacing, 1)
            if Prefab.instantiate(self.helmet, position, turn):isValid() then
                count = count + 1
            end
        end
    end
    Log.info("LodGrid: spawned " .. count .. " helmets")
end

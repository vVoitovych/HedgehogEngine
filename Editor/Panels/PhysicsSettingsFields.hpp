#pragma once

namespace HedgehogSettings
{
    struct PhysicsSettings;
}

namespace Editor
{
    // The Settings window's Physics section: gravity, the 16 physics layer names and the collision
    // matrix as a triangle of checkboxes (row i, column j <= i: whether layers i and j collide).
    // Edits the settings in place; the window's Save engine settings button writes them.
    void DrawPhysicsSettingsFields(HedgehogSettings::PhysicsSettings& physics);
}

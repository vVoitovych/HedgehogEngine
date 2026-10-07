# FeatureTest

The engine's sample project: a scene for every feature, each small enough to read. It is the
project the dev tree opens when none is named (`Editor.exe`, `--game-mode`, `--benchmark`,
`Game.exe`, the tests and `Scripts\PackageGame.bat`).

| Scene (`Assets/Scenes/`) | Shows |
|---|---|
| `Default.yaml` | The startup scene: meshes, materials and textures, lights and shadows, and a script (`Scripts/PlayerScript.lua`) turning the room with properties set in the inspector. |
| `Animated.yaml` | Skinned animation: two animated `TwoBoneStrip`s (`Models/Animated`) beside a rigid cube. |
| `Hud.yaml` | Game UI: a canvas with a translucent top bar, a logo, a title, a health bar and a button, its text in the engine's Karla font. |
| `Spinner.yaml` | A plugin: the Spinner sample plugin (`Plugins/Spinner`, enabled in `Project.yaml`) turning one cube beside a still one. |
| `Audio.yaml` | Audio: a cube playing the looping `Audio/Tone440.wav` to the right of the camera, which carries the audio listener. |
| `Prefabs.yaml` | Prefabs: two instances of `Prefabs/LampPost.prefab`, the second overriding its lamp's colour and intensity. |
| `Saves.yaml` | Save games: `Scripts/SaveDemo.lua` turns its cube, saves on frame 30 and loads that save on frame 60, logging both; the cube jumps back to where it was saved. |
| `benchmark.yaml` | The renderer benchmark: a 5×5 grid of DamagedHelmets (`Editor.exe --benchmark`, see `PERFORMANCE.md`). |
| `test.yaml` | A small mesh-and-material test scene. |

Every scene here is played by ScriptingTest's `test_feature_test_scenes.cpp`: it loads with the
project's plugins, references only files that exist, plays 120 frames without an error or warning,
and Stop restores it. A scene added to `Assets/Scenes` is covered with no change to the test.

Try one with `Binaries\windows-x86_64\Debug\Editor.exe --game-mode 600 Audio.yaml`, or open it in
the editor from the Project panel. Third-party content here is listed in `THIRD_PARTY_NOTICES.md`
at the repository root.

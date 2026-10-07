# Editor UI icons

Small line icons the editor draws in its menu bar, toolbar, panel tabs, hierarchy and inspector
(`Editor/Panels/EditorIcons`). Each is a 32x32 PNG, white on transparent, drawn at 16 px and
tinted at draw time, so one picture serves every state. `hedgehog_logo.png` keeps its colours.

## Sources

Every icon except the logo is from [Lucide](https://lucide.dev) `lucide-static` 1.51.0
(ISC licence; the Feather-derived icons also MIT). The licence is in `LICENSE-lucide.txt`, verbatim.

| File | Lucide icon |
|------|-------------|
| `play.png` | `play` |
| `pause.png` | `pause` |
| `stop.png` | `square` |
| `settings.png` | `settings` |
| `plus.png` | `plus` |
| `search.png` | `search` |
| `more.png` | `ellipsis-vertical` |
| `hierarchy.png` | `list-tree` |
| `scene.png` | `globe` |
| `game.png` | `gamepad-2` |
| `inspector.png` | `sliders-horizontal` |
| `project.png` | `folder-open` |
| `console.png` | `terminal` |
| `game_object.png` | `circle-dashed` |
| `folder.png` | `folder` |
| `camera.png` | `video` |
| `light.png` | `sun` |
| `mesh.png` | `box` |
| `transform.png` | `move-3d` |
| `material.png` | `palette` |
| `script.png` | `file-code` |
| `animator.png` | `activity` |
| `ui_canvas.png` | `frame` |
| `ui_rect.png` | `square-dashed` |
| `ui_image.png` | `image` |
| `ui_text.png` | `type` |
| `ui_button.png` | `mouse-pointer-click` |
| `audio_source.png` | `volume-2` |
| `audio_listener.png` | `ear` |
| `rigid_body.png` | `weight` |
| `collider.png` | `scan` |

`hedgehog_logo.png` is the project's own `Editor/editor.ico`, rendered at 32x32.

## How they were made

Each SVG came from `https://unpkg.com/lucide-static@1.51.0/icons/<name>.svg`, was sized to 32x32,
put in a page with `color: #fff` (Lucide strokes with `currentColor`) and rendered by headless Edge:

```
msedge --headless --disable-gpu --hide-scrollbars --default-background-color=00000000 --window-size=32,32 --screenshot=<file>.png <page>.html
```

While another Edge is running, add `--user-data-dir=<a temp folder>`, or the command hands the page
to that browser and writes no file.

The logo was rendered the same way from an `<img>` of `editor.ico`.

## The Content panel's pictures

The 64x64 full-colour pictures one folder up (`Editor/Resources/Icons/*.png`, drawn by
`Editor/Panels/ContentIcons`) were generated with ChatGPT.

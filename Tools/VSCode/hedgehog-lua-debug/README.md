# Hedgehog Lua Debugger for VS Code

Debug the Lua scripts of a running HedgehogEngine editor or game: breakpoints, step over/in/out,
the call stack, locals, upvalues, globals and tables, hover and watch evaluation of names and
field paths, and the engine's log in the Debug Console. The engine itself is the debug adapter
(`HedgehogLuaDebug`); this extension only registers the `hedgehog-lua` debug type and connects VS
Code to it.

## Setup

1. Enable the debugger in `engine_settings.yaml` (it is off by default, and then costs nothing):

   ```yaml
   lua_debugger:
     enabled: true
     port: 4711
   ```

   The engine listens on `127.0.0.1` only, and takes one debugger at a time.

2. Install this folder as an extension: in VS Code run **Developer: Install Extension from
   Location...** and pick `Tools/VSCode/hedgehog-lua-debug`, or copy the folder into
   `%USERPROFILE%\.vscode\extensions`. It has no dependencies and needs no build.

3. Add a launch configuration (`.vscode/launch.json` in the repository):

   ```json
   {
       "version": "0.2.0",
       "configurations": [
           { "type": "hedgehog-lua", "request": "attach", "name": "Attach to Hedgehog", "port": 4711 }
       ]
   }
   ```

## Use

Start the editor (scripts run in Play mode) or `Game.exe`, then run **Attach to Hedgehog**. Set
breakpoints in the project's `Assets/Scripts/*.lua` (`Projects/FeatureTest/Assets/Scripts` in the dev tree); Play shows as the `Lua` thread starting, Stop as it exiting.

While a script is stopped the whole frame waits inside it: the window keeps handling events, its
title ends with "Paused in debugger", and it does not redraw until you continue.

`evaluate` (hover, watch, Debug Console) accepts names and field paths only (`self.speed`,
`t.items[2]`, `t['key']`), so it never runs script code. Values can be set to numbers, quoted
strings and booleans.

Limitations: a coroutine created before the first breakpoint was set does not stop on breakpoints
or steps; conditional breakpoints, logpoints and exception breakpoints are not supported.

## Checking without VS Code

`dap_check.py` speaks the protocol the way VS Code does: with the debugger enabled and the game
running,

```
python Tools/VSCode/hedgehog-lua-debug/dap_check.py Projects/FeatureTest/Assets/Scripts/PlayerScript.lua 22
```

attaches, stops at the line, prints the stack and locals, steps once, continues and detaches.

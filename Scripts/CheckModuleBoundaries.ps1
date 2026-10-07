# Checks the module boundaries (RENDERING.md section 9, CLAUDE.md) on every C++ source and header.
#
#   1. No cross-module src/ include: a module's src/ is private, so an include string that names
#      another module's src/ folder ("RHI/src/...", "../../RHI/src/...") is a violation. A test
#      project (a module's tests/ folder) counts as its module.
#   2. HedgehogRenderer never includes the ECS, the engine module or ImGui: no file under
#      HedgehogEngine/HedgehogRenderer/ may include "ECS/...", "HedgehogEngine/HedgehogEngine/...",
#      "HedgehogEngine/api/...", "imgui.h" or "imgui_impl_*".
#   3. The engine module never runs scripts itself (ADR-022): no file under
#      HedgehogEngine/HedgehogEngine/, tests included, may include Lua ("Lua/...", "lua.h",
#      "lualib.h", "lauxlib.h", "lua.hpp"), sol2 ("sol/...") or HedgehogScripting
#      ("HedgehogScripting/..."), the library above it that owns scripting.
#   4. The game never includes ImGui: no file under HedgehogRuntime/ or Game/ may include
#      "imgui.h" or "imgui_impl_*", so the Game executable links no editor UI.
#   5. Plugins are engine-side: no file under Plugins/ may include ImGui ("imgui.h",
#      "imgui_impl_*"), Lua ("Lua/...", "lua.h", "lualib.h", "lauxlib.h", "lua.hpp"), sol2
#      ("sol/...") or HedgehogScripting ("HedgehogScripting/...").
#   6. Jolt Physics stays inside HedgehogPhysics: only files under HedgehogPhysics/src/ may include
#      "Jolt/..." (ThirdParty/, where Jolt itself lives, is never checked), so no public header and
#      no other module ever names a Jolt type.
#
# All rules match #include strings, never file-system paths: the path
# HedgehogEngine/HedgehogCommon/... contains "HedgehogEngine/" and must not match rule 2.
#
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\CheckModuleBoundaries.ps1 [-Root <dir>] [-SelfTest]
# Exits 0 when the tree is clean, 1 with one line per violation otherwise. -SelfTest instead
# plants each kind of violation in a throwaway tree and checks that each one is caught and that
# the tree passes without them. Runs on Windows PowerShell 5.1 and PowerShell 7 (CI).

param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

$ExcludedDirs = @('ThirdParty', 'Vendor', 'Binaries', '.git', '.claude', '.vs')
$SourceExtensions = @('.cpp', '.hpp', '.h', '.inl', '.c')

$IncludePattern     = '^\s*#\s*include\s*[<"]([^>"]+)[>"]'
$SrcIncludePattern  = '(?:^|[/\\])([A-Za-z0-9_]+)[/\\]src[/\\]'
$RendererForbidden  = @(
    @{ Pattern = '^ECS[/\\]';                          Reason = 'the renderer must not include the ECS' },
    @{ Pattern = '(^|[/\\])HedgehogEngine[/\\]HedgehogEngine[/\\]'; Reason = 'the renderer must not include the engine module' },
    @{ Pattern = '^HedgehogEngine[/\\]api[/\\]';       Reason = 'the renderer must not include the engine module' },
    @{ Pattern = '(^|[/\\])imgui\.h$';                 Reason = 'the renderer must not include ImGui' },
    @{ Pattern = '(^|[/\\])imgui_impl_[^/\\]*$';       Reason = 'the renderer must not include ImGui' }
)
$RuntimeForbidden   = @(
    @{ Pattern = '(^|[/\\])imgui\.h$';                 Reason = 'the game must not include ImGui' },
    @{ Pattern = '(^|[/\\])imgui_impl_[^/\\]*$';       Reason = 'the game must not include ImGui' }
)
$PluginForbidden    = @(
    @{ Pattern = '(^|[/\\])imgui\.h$';                 Reason = 'a plugin must not include ImGui' },
    @{ Pattern = '(^|[/\\])imgui_impl_[^/\\]*$';       Reason = 'a plugin must not include ImGui' },
    @{ Pattern = '(^|[/\\])Lua[/\\]';                  Reason = 'a plugin must not include Lua' },
    @{ Pattern = '(^|[/\\])(lua\.h|lualib\.h|lauxlib\.h|lua\.hpp)$'; Reason = 'a plugin must not include Lua' },
    @{ Pattern = '(^|[/\\])sol[/\\]';                  Reason = 'a plugin must not include sol2' },
    @{ Pattern = '(^|[/\\])HedgehogScripting[/\\]';    Reason = 'a plugin must not depend on HedgehogScripting' }
)
$JoltForbidden      = @(
    @{ Pattern = '(^|[/\\])Jolt[/\\]';                 Reason = 'only HedgehogPhysics/src/ may include Jolt' }
)
$EngineForbidden    = @(
    @{ Pattern = '(^|[/\\])Lua[/\\]';                  Reason = 'the engine module must not include Lua' },
    @{ Pattern = '(^|[/\\])(lua\.h|lualib\.h|lauxlib\.h|lua\.hpp)$'; Reason = 'the engine module must not include Lua' },
    @{ Pattern = '(^|[/\\])sol[/\\]';                  Reason = 'the engine module must not include sol2' },
    @{ Pattern = '(^|[/\\])HedgehogScripting[/\\]';    Reason = 'the engine module must not depend on HedgehogScripting above it' }
)

function Test-Excluded([string]$relativePath)
{
    $first = ($relativePath -split '[/\\]')[0]
    return $ExcludedDirs -contains $first
}

function Test-Under([string]$path, [string]$folder)
{
    return $path.StartsWith($folder + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}

# The module a file belongs to: the nearest folder holding a Build-*.lua, with a tests/ folder
# standing for its parent module.
function Get-ModuleName([string]$directory, [string]$root, [hashtable]$moduleCache)
{
    if ($moduleCache.ContainsKey($directory)) { return $moduleCache[$directory] }
    $current = $directory
    $module  = $null
    while ($current -and $current.Length -ge $root.Length)
    {
        if (Get-ChildItem -LiteralPath $current -Filter 'Build-*.lua' -File -ErrorAction SilentlyContinue)
        {
            $leaf = Split-Path -Leaf $current
            if ($leaf -eq 'tests') { $leaf = Split-Path -Leaf (Split-Path -Parent $current) }
            $module = $leaf
            break
        }
        $current = Split-Path -Parent $current
    }
    $moduleCache[$directory] = $module
    return $module
}

# Every violation under root, one line each, and the number of files checked.
function Get-Violations([string]$root)
{
    $root         = (Resolve-Path $root).Path
    $moduleCache  = @{}
    $rendererRoot = Join-Path (Join-Path $root 'HedgehogEngine') 'HedgehogRenderer'
    $engineRoot   = Join-Path (Join-Path $root 'HedgehogEngine') 'HedgehogEngine'
    $runtimeRoot  = Join-Path $root 'HedgehogRuntime'
    $gameRoot     = Join-Path $root 'Game'
    $pluginsRoot  = Join-Path $root 'Plugins'
    $physicsSrc   = Join-Path (Join-Path $root 'HedgehogPhysics') 'src'
    $violations   = New-Object System.Collections.Generic.List[string]

    $files = @(Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $SourceExtensions -contains $_.Extension.ToLowerInvariant() -and
            -not (Test-Excluded ($_.FullName.Substring($root.Length).TrimStart('\', '/')))
        })

    foreach ($file in $files)
    {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\', '/')
        $module   = Get-ModuleName $file.DirectoryName $root $moduleCache
        $rules    = @()
        if (Test-Under $file.FullName $rendererRoot) { $rules += $RendererForbidden }
        if (Test-Under $file.FullName $engineRoot)   { $rules += $EngineForbidden }
        if ((Test-Under $file.FullName $runtimeRoot) -or (Test-Under $file.FullName $gameRoot)) { $rules += $RuntimeForbidden }
        if (Test-Under $file.FullName $pluginsRoot)  { $rules += $PluginForbidden }
        if (-not (Test-Under $file.FullName $physicsSrc)) { $rules += $JoltForbidden }

        $lineNumber = 0
        foreach ($line in [IO.File]::ReadAllLines($file.FullName))
        {
            $lineNumber++
            $match = [regex]::Match($line, $IncludePattern)
            if (-not $match.Success) { continue }
            $include = $match.Groups[1].Value

            $src = [regex]::Match($include, $SrcIncludePattern)
            if ($src.Success -and $module -and $src.Groups[1].Value -ne $module)
            {
                $violations.Add("${relative}:${lineNumber}: #include `"$include`" reaches into module $($src.Groups[1].Value)'s src/ (from $module)")
            }

            # One line per include, however many patterns it matches.
            foreach ($rule in $rules)
            {
                if ($include -match $rule.Pattern)
                {
                    $violations.Add("${relative}:${lineNumber}: #include `"$include`": $($rule.Reason)")
                    break
                }
            }
        }
    }

    return @{ Violations = $violations; FileCount = $files.Count }
}

# Plants one violation of each kind in a throwaway tree shaped like the repository and checks
# that each is caught on its own, and that the tree passes without them.
function Invoke-SelfTest
{
    $temp    = Join-Path ([IO.Path]::GetTempPath()) ('boundary_selftest_' + [Guid]::NewGuid().ToString('N'))
    $modules = @('HedgehogEngine\HedgehogEngine', 'HedgehogEngine\HedgehogRenderer', 'Editor', 'HedgehogScripting', 'HedgehogRuntime', 'Game', 'Plugins\Spinner', 'HedgehogPhysics')
    $cases   = @(
        @{ Module = 'HedgehogEngine\HedgehogEngine';   Include = 'ThirdParty/Lua/lua/lua.h' },
        @{ Module = 'HedgehogEngine\HedgehogEngine';   Include = 'lauxlib.h' },
        @{ Module = 'HedgehogEngine\HedgehogEngine';   Include = 'sol/sol.hpp' },
        @{ Module = 'HedgehogEngine\HedgehogEngine';   Include = 'HedgehogScripting/api/ScriptSystem.hpp' },
        @{ Module = 'HedgehogEngine\HedgehogRenderer'; Include = 'ECS/api/ECS.hpp' },
        @{ Module = 'HedgehogEngine\HedgehogRenderer'; Include = 'imgui.h' },
        @{ Module = 'HedgehogRuntime';                 Include = 'imgui.h' },
        @{ Module = 'HedgehogRuntime';                 Include = 'backends/imgui_impl_glfw.h' },
        @{ Module = 'Game';                            Include = 'imgui.h' },
        @{ Module = 'Plugins\Spinner';                 Include = 'imgui.h' },
        @{ Module = 'Plugins\Spinner';                 Include = 'backends/imgui_impl_vulkan.h' },
        @{ Module = 'Plugins\Spinner';                 Include = 'lua.h' },
        @{ Module = 'Plugins\Spinner';                 Include = 'sol/sol.hpp' },
        @{ Module = 'Plugins\Spinner';                 Include = 'HedgehogScripting/api/ScriptSystem.hpp' },
        @{ Module = 'Editor';                          Include = 'HedgehogEngine/RHI/src/Vulkan/VulkanDevice.hpp' },
        @{ Module = 'HedgehogPhysics\api';             Include = 'Jolt/Jolt.h' },
        @{ Module = 'HedgehogEngine\HedgehogEngine';   Include = 'Jolt/Physics/Body/Body.h' }
    )
    $failures = 0
    try
    {
        foreach ($module in $modules)
        {
            $dir = Join-Path $temp $module
            New-Item -ItemType Directory -Force -Path $dir | Out-Null
            Set-Content -LiteralPath (Join-Path $dir ('Build-' + (Split-Path -Leaf $module) + '.lua')) -Value '' -Encoding Ascii
            # An include every module may make.
            Set-Content -LiteralPath (Join-Path $dir 'Clean.cpp') -Value '#include "HedgehogMath/api/Vector.hpp"' -Encoding Ascii
        }
        # The Editor may include ImGui; it owns it.
        Set-Content -LiteralPath (Join-Path (Join-Path $temp 'Editor') 'Gui.cpp') -Value '#include "imgui.h"' -Encoding Ascii
        # The scripting library may include sol2 and Lua; that is where they belong.
        Set-Content -LiteralPath (Join-Path (Join-Path $temp 'HedgehogScripting') 'Sol.cpp') -Value '#include "sol/sol.hpp"' -Encoding Ascii
        Set-Content -LiteralPath (Join-Path (Join-Path $temp 'HedgehogScripting') 'Lua.cpp') -Value '#include "lua.h"' -Encoding Ascii
        # HedgehogPhysics' src/ may include Jolt; its api/ is where a planted Jolt include goes.
        foreach ($folder in @('src', 'api'))
        {
            New-Item -ItemType Directory -Force -Path (Join-Path (Join-Path $temp 'HedgehogPhysics') $folder) | Out-Null
        }
        Set-Content -LiteralPath (Join-Path (Join-Path (Join-Path $temp 'HedgehogPhysics') 'src') 'World.cpp') -Value '#include "Jolt/Jolt.h"' -Encoding Ascii

        $clean = Get-Violations $temp
        if ($clean.Violations.Count -ne 0)
        {
            Write-Output "Self-test FAILED: the clean tree reported $($clean.Violations.Count) violation(s)."
            $clean.Violations | ForEach-Object { Write-Output "  $_" }
            $failures++
        }

        foreach ($case in $cases)
        {
            $planted = Join-Path (Join-Path $temp $case.Module) 'Planted.cpp'
            Set-Content -LiteralPath $planted -Value ('#include "' + $case.Include + '"') -Encoding Ascii
            $result = Get-Violations $temp
            if ($result.Violations.Count -ne 1)
            {
                Write-Output "Self-test FAILED: #include `"$($case.Include)`" in $($case.Module) gave $($result.Violations.Count) violation(s), expected 1."
                $failures++
            }
            else
            {
                Write-Output "  caught: $($result.Violations[0])"
            }
            Remove-Item -LiteralPath $planted
        }
    }
    finally
    {
        Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
    }

    if ($failures -gt 0) { exit 1 }
    Write-Output "Module boundary self-test passed ($($cases.Count) planted violations caught, clean tree passes)."
    exit 0
}

if ($SelfTest) { Invoke-SelfTest }

$result = Get-Violations $Root
if ($result.Violations.Count -gt 0)
{
    Write-Output "Module boundary check FAILED: $($result.Violations.Count) violation(s)."
    $result.Violations | ForEach-Object { Write-Output "  $_" }
    exit 1
}

Write-Output "Module boundary check passed ($($result.FileCount) files)."
exit 0

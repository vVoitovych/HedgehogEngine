# Checks the module boundaries (RENDERING.md section 9, CLAUDE.md) on every C++ source and header.
#
#   1. No cross-module src/ include: a module's src/ is private, so an include string that names
#      another module's src/ folder ("RHI/src/...", "../../RHI/src/...") is a violation. A test
#      project (a module's tests/ folder) counts as its module.
#   2. HedgehogRenderer never includes the ECS, the engine module or ImGui: no file under
#      HedgehogEngine/HedgehogRenderer/ may include "ECS/...", "HedgehogEngine/HedgehogEngine/...",
#      "HedgehogEngine/api/...", "imgui.h" or "imgui_impl_*".
#
# Both rules match #include strings, never file-system paths: the path
# HedgehogEngine/HedgehogCommon/... contains "HedgehogEngine/" and must not match rule 2.
#
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\CheckModuleBoundaries.ps1 [-Root <dir>]
# Exits 0 when the tree is clean, 1 with one line per violation otherwise. Runs on Windows
# PowerShell 5.1 and PowerShell 7 (CI).

param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path $Root).Path

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

function Test-Excluded([string]$relativePath)
{
    $first = ($relativePath -split '[/\\]')[0]
    return $ExcludedDirs -contains $first
}

# The module a file belongs to: the nearest folder holding a Build-*.lua, with a tests/ folder
# standing for its parent module.
$moduleCache = @{}
function Get-ModuleName([string]$directory)
{
    if ($moduleCache.ContainsKey($directory)) { return $moduleCache[$directory] }
    $current = $directory
    $module  = $null
    while ($current -and $current.Length -ge $Root.Length)
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

$rendererRoot = Join-Path (Join-Path $Root 'HedgehogEngine') 'HedgehogRenderer'
$violations   = New-Object System.Collections.Generic.List[string]

$files = Get-ChildItem -LiteralPath $Root -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
        $SourceExtensions -contains $_.Extension.ToLowerInvariant() -and
        -not (Test-Excluded ($_.FullName.Substring($Root.Length).TrimStart('\', '/')))
    }

foreach ($file in $files)
{
    $relative   = $file.FullName.Substring($Root.Length).TrimStart('\', '/')
    $module     = Get-ModuleName $file.DirectoryName
    $inRenderer = $file.FullName.StartsWith($rendererRoot + [IO.Path]::DirectorySeparatorChar,
                                            [StringComparison]::OrdinalIgnoreCase)

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

        if ($inRenderer)
        {
            foreach ($rule in $RendererForbidden)
            {
                if ($include -match $rule.Pattern)
                {
                    $violations.Add("${relative}:${lineNumber}: #include `"$include`": $($rule.Reason)")
                }
            }
        }
    }
}

if ($violations.Count -gt 0)
{
    Write-Output "Module boundary check FAILED: $($violations.Count) violation(s)."
    $violations | ForEach-Object { Write-Output "  $_" }
    exit 1
}

Write-Output "Module boundary check passed ($($files.Count) files)."
exit 0

# Fails (exit 1) if any first-party source includes another module's private src/ headers.
# Own-module src/ includes are allowed; api/ includes are always allowed.
# Wired into Scripts\RunTests.bat and .github/workflows/build.yml.

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

# A module is any directory (top-level or under HedgehogEngine/) with a Build-*.lua file.
$moduleDirs = @()
$moduleDirs += Get-ChildItem -Path $repoRoot -Directory |
    Where-Object { ($_.Name -ne "ThirdParty") -and ($_.Name -ne "Vendor") -and
                   (Test-Path (Join-Path $_.FullName "Build-*.lua")) }
$moduleDirs += Get-ChildItem -Path (Join-Path $repoRoot "HedgehogEngine") -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName "Build-*.lua") }

$moduleNames = $moduleDirs | ForEach-Object { $_.Name } | Sort-Object -Unique

# Matches: #include "X/src/..." or #include "HedgehogEngine/X/src/..."
$includePattern = '#include\s+"(?:HedgehogEngine/)?([A-Za-z0-9_]+)/src/'

$violations = @()

foreach ($moduleDir in $moduleDirs)
{
    $ownModule = $moduleDir.Name
    $sources = Get-ChildItem -Path $moduleDir.FullName -Recurse -File -Include "*.hpp", "*.cpp"

    foreach ($file in $sources)
    {
        $lineNumber = 0
        foreach ($line in [System.IO.File]::ReadLines($file.FullName))
        {
            $lineNumber++
            $match = [regex]::Match($line, $includePattern)
            if (-not $match.Success) { continue }

            $targetModule = $match.Groups[1].Value
            if ($targetModule -eq $ownModule) { continue }
            if ($moduleNames -notcontains $targetModule) { continue }

            $relativePath = $file.FullName.Substring($repoRoot.Length + 1)
            $violations += "{0}:{1}: {2}" -f $relativePath, $lineNumber, $line.Trim()
        }
    }
}

if ($violations.Count -gt 0)
{
    Write-Host "[ERROR] Module boundary violations (cross-module src/ includes):"
    $violations | ForEach-Object { Write-Host "  $_" }
    Write-Host "Only a module's own src/ headers may be included; use the target module's api/ headers instead."
    exit 1
}

Write-Host "Module boundaries OK: no cross-module src/ includes."
exit 0

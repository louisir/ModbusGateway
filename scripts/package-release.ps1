[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$QtBin = "",
    [string]$Make = "",
    [string]$Repo = "",
    [string]$NotesFile = "",
    [int]$Jobs = 2,
    [switch]$Clean,
    [switch]$Publish
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir
$ProjectFile = Join-Path $RootDir "ModbusGateway.pro"
$TargetName = "ModbusGateway"
$Platform = "win64"

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Find-CommandPath {
    param(
        [string]$Name,
        [string[]]$Candidates = @()
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return ""
}

function Find-FirstFile {
    param(
        [string]$Path,
        [string]$Filter
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }

    $item = Get-ChildItem -Path $Path -Filter $Filter -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if ($item) {
        return $item.FullName
    }

    return ""
}

function Get-NextVersion {
    $tags = & git -C $RootDir tag --list "v[0-9]*"
    $versions = @()

    foreach ($tag in $tags) {
        if ($tag -match '^v(\d+)\.(\d+)(?:\.(\d+))?$') {
            $patch = 0
            if ($Matches[3]) {
                $patch = [int]$Matches[3]
            }

            $versions += [pscustomobject]@{
                Tag = $tag
                Major = [int]$Matches[1]
                Minor = [int]$Matches[2]
                Patch = $patch
                HasPatch = [bool]$Matches[3]
            }
        }
    }

    if ($versions.Count -eq 0) {
        return "v0.1"
    }

    $latest = $versions | Sort-Object Major, Minor, Patch -Descending | Select-Object -First 1
    if ($latest.HasPatch) {
        return "v$($latest.Major).$($latest.Minor).$($latest.Patch + 1)"
    }

    return "v$($latest.Major).$($latest.Minor + 1)"
}

function Invoke-GitOutput {
    param([string[]]$Arguments)

    return ((& git -C $RootDir @Arguments 2>$null) -join "`n").Trim()
}

function Get-OriginRepo {
    $remote = Invoke-GitOutput @("remote", "get-url", "origin")
    if (-not $remote) {
        return ""
    }

    if ($remote -match 'github\.com[:/](.+?/.+?)(?:\.git)?$') {
        return $Matches[1]
    }

    return ""
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory = $RootDir
    )

    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code $LASTEXITCODE`: $FilePath $($Arguments -join ' ')"
        }
    }
    finally {
        Pop-Location
    }
}

function Create-ReleaseNotes {
    param(
        [string]$Path,
        [string]$Version
    )

    $content = @"
# ModbusGateway $Version

- Build: Windows $Platform package generated with Qt deployment dependencies.
- Includes the current Modbus TCP/RTU gateway fixes and bilingual README.
- See README.md for usage, build instructions, and current limitations.
"@

    Set-Content -LiteralPath $Path -Value $content -Encoding UTF8
}

function Publish-WithGitHubApi {
    param(
        [string]$Repo,
        [string]$Version,
        [string]$ZipPath,
        [string]$NotesPath
    )

    $token = $env:GH_TOKEN
    if (-not $token) {
        $token = $env:GITHUB_TOKEN
    }
    if (-not $token) {
        return $false
    }

    $headers = @{
        Authorization = "Bearer $token"
        Accept = "application/vnd.github+json"
        "X-GitHub-Api-Version" = "2022-11-28"
    }
    $notes = Get-Content -Raw -LiteralPath $NotesPath
    $body = @{
        tag_name = $Version
        name = "ModbusGateway $Version"
        body = $notes
        draft = $false
        prerelease = $false
    } | ConvertTo-Json

    $release = Invoke-RestMethod `
        -Method Post `
        -Uri "https://api.github.com/repos/$Repo/releases" `
        -Headers $headers `
        -Body $body `
        -ContentType "application/json"

    $assetName = Split-Path -Leaf $ZipPath
    $uploadUri = $release.upload_url -replace '\{\?name,label\}', "?name=$([Uri]::EscapeDataString($assetName))"
    Invoke-RestMethod `
        -Method Post `
        -Uri $uploadUri `
        -Headers $headers `
        -ContentType "application/zip" `
        -InFile $ZipPath | Out-Null

    return $true
}

function Publish-WithGitHubCli {
    param(
        [string]$Version,
        [string]$ZipPath,
        [string]$NotesPath
    )

    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if (-not $gh) {
        return $false
    }

    Invoke-Native $gh.Source @(
        "release",
        "create",
        $Version,
        $ZipPath,
        "--title",
        "ModbusGateway $Version",
        "--notes-file",
        $NotesPath
    )
    return $true
}

if (-not (Test-Path -LiteralPath $ProjectFile)) {
    throw "Project file not found: $ProjectFile"
}

if (-not $Version) {
    $Version = Get-NextVersion
}
if ($Version -notmatch '^v\d+\.\d+(?:\.\d+)?$') {
    throw "Version must look like v0.4 or v1.2.3. Current value: $Version"
}

$DistDir = Join-Path $RootDir "dist"
$BuildDir = Join-Path $RootDir "build_release"
$StageDir = Join-Path $DistDir "$TargetName-$Version-$Platform"
$ZipPath = Join-Path $DistDir "$TargetName-$Version-$Platform.zip"

if ($Clean) {
    Write-Step "Cleaning previous package output"
    Remove-Item -LiteralPath $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $StageDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $ZipPath -Force -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force -Path $DistDir, $BuildDir, $StageDir | Out-Null

if ($QtBin) {
    $qmake = Join-Path $QtBin "qmake.exe"
    $windeployqt = Join-Path $QtBin "windeployqt.exe"
}
else {
    $qmake = Find-CommandPath "qmake.exe" @(
        "C:\Qt\6.8.3\mingw_64\bin\qmake.exe",
        (Find-FirstFile "C:\Qt" "qmake.exe")
    )
    $windeployqt = Find-CommandPath "windeployqt.exe" @(
        "C:\Qt\6.8.3\mingw_64\bin\windeployqt.exe",
        (Find-FirstFile "C:\Qt" "windeployqt.exe")
    )
}

if (-not (Test-Path -LiteralPath $qmake)) {
    throw "qmake.exe not found. Pass -QtBin C:\Qt\<version>\mingw_64\bin."
}
if (-not (Test-Path -LiteralPath $windeployqt)) {
    throw "windeployqt.exe not found. Pass -QtBin C:\Qt\<version>\mingw_64\bin."
}

if ($Make) {
    $makeTool = $Make
}
else {
    $makeTool = Find-CommandPath "mingw32-make.exe" @(
        "C:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe",
        (Find-FirstFile "C:\Qt\Tools" "mingw32-make.exe")
    )
}
if (-not (Test-Path -LiteralPath $makeTool)) {
    throw "mingw32-make.exe not found. Pass -Make <path-to-mingw32-make.exe>."
}

$env:Path = "$(Split-Path -Parent $qmake);$(Split-Path -Parent $makeTool);$env:Path"

Write-Step "Building $TargetName $Version"
Invoke-Native $qmake @($ProjectFile, "CONFIG+=release") $BuildDir
Invoke-Native $makeTool @("-j$Jobs") $BuildDir

$ExePath = Join-Path $BuildDir "release\$TargetName.exe"
if (-not (Test-Path -LiteralPath $ExePath)) {
    $ExePath = Join-Path $BuildDir "$TargetName.exe"
}
if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Built executable not found."
}

Write-Step "Staging files"
Remove-Item -LiteralPath $StageDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
Copy-Item -LiteralPath $ExePath -Destination $StageDir -Force
Copy-Item -LiteralPath (Join-Path $RootDir "README.md") -Destination $StageDir -Force
Copy-Item -LiteralPath (Join-Path $RootDir "LICENSE") -Destination $StageDir -Force

Write-Step "Running windeployqt"
$StageExe = Join-Path $StageDir "$TargetName.exe"
Invoke-Native $windeployqt @("--release", "--compiler-runtime", $StageExe) $RootDir

Write-Step "Creating zip package"
Remove-Item -LiteralPath $ZipPath -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $ZipPath -Force

if (-not $NotesFile) {
    $NotesFile = Join-Path $DistDir "release-notes-$Version.md"
    Create-ReleaseNotes $NotesFile $Version
}

Write-Host ""
Write-Host "Package created:"
Write-Host "  $ZipPath"

if ($Publish) {
    Write-Step "Publishing git tag"

    $dirty = Invoke-GitOutput @("status", "--porcelain")
    if ($dirty) {
        throw "Working tree is not clean. Commit or stash changes before publishing."
    }

    $existingTag = Invoke-GitOutput @("tag", "--list", $Version)
    if (-not $existingTag) {
        Invoke-Native "git" @("-C", $RootDir, "tag", "-a", $Version, "-m", "Release $Version")
    }

    $currentBranch = Invoke-GitOutput @("branch", "--show-current")
    if ($currentBranch) {
        Invoke-Native "git" @("-C", $RootDir, "push", "origin", $currentBranch)
    }
    Invoke-Native "git" @("-C", $RootDir, "push", "origin", $Version)

    if (-not $Repo) {
        $Repo = Get-OriginRepo
    }
    if (-not $Repo) {
        Write-Warning "Could not detect GitHub repo. Tag was pushed, but GitHub Release was not created."
        exit 0
    }

    Write-Step "Creating GitHub Release"
    $published = Publish-WithGitHubCli $Version $ZipPath $NotesFile
    if (-not $published) {
        $published = Publish-WithGitHubApi $Repo $Version $ZipPath $NotesFile
    }

    if ($published) {
        Write-Host "GitHub Release created: https://github.com/$Repo/releases/tag/$Version"
    }
    else {
        Write-Warning "Tag was pushed, but GitHub Release was not created because neither gh CLI nor GH_TOKEN/GITHUB_TOKEN is available."
        Write-Warning "Run this after authentication: gh release create $Version `"$ZipPath`" --title `"ModbusGateway $Version`" --notes-file `"$NotesFile`""
    }
}

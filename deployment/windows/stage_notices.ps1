param(
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [Parameter(Mandatory=$true)][string]$StageDir,
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$OpenSslRoot,
    [string]$QtSource = $env:BRICKSUITE_WINDOWS_QT_SOURCE
)
$ErrorActionPreference = 'Stop'

# Collect before copying: a missing notice must prevent installer generation.
$files = @{}
function Notice-Hash([string]$Path) {
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hash.ComputeHash([IO.File]::ReadAllBytes($Path))) }
    finally { $hash.Dispose() }
}
function Add-Notice([string]$Source, [string]$Relative) {
    if (!(Test-Path -LiteralPath $Source -PathType Leaf) -or (Get-Item -LiteralPath $Source).Length -eq 0) {
        throw "Required notice missing or empty: $Source"
    }
    $files[$Relative] = [IO.Path]::GetFullPath($Source)
}
function Add-Tree([string]$Source, [string]$Relative) {
    if (!(Test-Path -LiteralPath $Source -PathType Container)) { throw "Required notice directory missing: $Source" }
    $root = (Get-Item -LiteralPath $Source).FullName
    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File) {
        Add-Notice $file.FullName ($Relative + '/' + $file.FullName.Substring($root.Length + 1).Replace('\','/'))
    }
}

foreach ($name in @('LICENSE','Libraries/libressl/COPYING','submodules/zlib/LICENSE',
        'submodules/libzip/LICENSE','submodules/cpp-base64/LICENSE','submodules/fast_float/LICENSE-MIT')) {
    Add-Notice "$BuildDir/deployment/licenses/lib3mf/$name" "lib3mf/$name"
}
foreach ($name in @('LICENSE.txt','COPYING','COPYING.LESSER','include/mcut/internal/cdt/LICENSE.txt',
        'BrickSuite-integration.md','BrickSuiteMcutPortability.cmake','BrickSuiteMcutLinuxQueue.cmake')) {
    Add-Notice "$BuildDir/deployment/licenses/MCUT/$name" "MCUT/$name"
}
Add-Notice "$OpenSslRoot/share/licenses/openssl/LICENSE" 'OpenSSL/LICENSE'

$cache = Get-Content -LiteralPath "$BuildDir/CMakeCache.txt"
$compilerLine = @($cache | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:[^=]+=' })
if ($compilerLine.Count -ne 1) { throw 'Cannot determine compiler notice source from CMakeCache.txt' }
$compilerRoot = Split-Path (Split-Path ($compilerLine[0] -split '=',2)[1])
foreach ($name in @('gcc/COPYING.RUNTIME','gcc/COPYING3','gcc/COPYING3.LIB',
        'mingw-w64/COPYING','winpthreads/COPYING')) {
    Add-Notice "$compilerRoot/licenses/$name" "MinGW/$name"
}
foreach ($name in @('gcc','mingw-w64','winpthreads')) { Add-Tree "$compilerRoot/licenses/$name" "MinGW/$name" }

if (!$QtSource) { $QtSource = Join-Path (Split-Path $QtRoot) 'Src' }
$qtVersionFile = "$QtRoot/lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake"
$versionMatch = [regex]::Match((Get-Content -LiteralPath $qtVersionFile -Raw), 'set\(PACKAGE_VERSION "([0-9.]+)"\)')
if (!$versionMatch.Success) { throw 'Cannot determine deployed Qt version' }
$version = $versionMatch.Groups[1].Value
foreach ($module in @('qtbase','qtsvg','qtimageformats','qtwebsockets')) {
    $root = [IO.Path]::GetFullPath("$QtSource/$module")
    $config = Get-Content -LiteralPath "$root/.cmake.conf" -Raw
    if ($config -notmatch ('QT_REPO_MODULE_VERSION\s+"' + [regex]::Escape($version) + '"')) {
        throw "Qt notice source version mismatch: $module (expected $version)"
    }
    $attributions = @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter qt_attribution.json)
    # qtwebsockets has no third-party attribution entries in this Qt release.
    if (!$attributions.Count -and $module -ne 'qtwebsockets') { throw "Missing Qt attribution metadata: $module" }
    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File) {
        if ($file.Name -match '^(LICENSE|LICENCE|COPYING|COPYRIGHT|NOTICE|AUTHORS)' -and $file.Length) {
            Add-Notice $file.FullName ("Qt/$module/" + $file.FullName.Substring($root.Length + 1).Replace('\','/'))
        }
    }
    foreach ($file in $attributions) {
        Add-Notice $file.FullName ("Qt/$module/" + $file.FullName.Substring($root.Length + 1).Replace('\','/'))
        foreach ($entry in @(Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json)) {
            foreach ($name in @($entry.LicenseFile)) {
                foreach ($part in ($name -split ',')) {
                    if (!$part.Trim()) { continue }
                    $path = [IO.Path]::GetFullPath((Join-Path $file.DirectoryName $part.Trim()))
                    $sourceRoot = [IO.Path]::GetFullPath($QtSource).TrimEnd('\','/')
                    if (!$path.StartsWith($sourceRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
                        throw "Qt attribution escapes source tree: $path"
                    }
                    Add-Notice $path ('Qt/' + $path.Substring($sourceRoot.Length + 1).Replace('\','/'))
                }
            }
        }
    }
}
foreach ($name in @('LGPL-3.0-only.txt','GPL-3.0-only.txt','GPL-2.0-only.txt')) {
    Add-Notice "$QtSource/LICENSES/$name" "Qt/LICENSES/$name"
}
Add-Tree "$QtSource/LICENSES" 'Qt/LICENSES'

# These optional SDK runtimes are copied by the unchanged windeployqt defaults.
if (Test-Path -LiteralPath "$QtRoot/bin/opengl32sw.dll") {
    $knownMesa = 'B04DE4541863BC7D8879040A78889C4849C1B1DA2784C4630F734C146C2998CE'
    if ((Notice-Hash "$QtRoot/bin/opengl32sw.dll").Replace('-','') -ne $knownMesa) {
        throw 'Software OpenGL runtime changed: review its matching notices before packaging'
    }
    $rendererNotices = "$PSScriptRoot/../../third_party/windows-software-opengl"
    foreach ($name in @('Mesa-LICENSE.txt','LLVM-LICENSE.TXT','LLVM-COPYRIGHT.regex','LLVM-MD5-NOTICE.txt','README.md')) {
        Add-Notice "$rendererNotices/$name" "SoftwareOpenGL/$name"
    }
}
if (Test-Path -LiteralPath "$QtRoot/bin/d3dcompiler_47.dll") {
    $installerRoot = Split-Path (Split-Path $QtRoot)
    Add-Notice "$installerRoot/Licenses/sdk_license" 'WindowsSDK/sdk_license'
}

foreach ($relative in ($files.Keys | Sort-Object)) {
    $target = Join-Path "$StageDir/licenses" $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
    Copy-Item -LiteralPath $files[$relative] -Destination $target -Force
    if ((Notice-Hash $target) -ne (Notice-Hash $files[$relative])) {
        throw "Staged notice verification failed: $relative"
    }
}
$files.Keys | Sort-Object | Set-Content -LiteralPath "$StageDir/licenses/manifest.txt" -Encoding UTF8
Write-Output "Staged and verified $($files.Count) third-party notice files."

param(
    [Parameter(Mandatory = $true)][string] $QtDirectory,
    [string] $InnoCompiler = '',
    [string] $BuildDirectory = '',
    [string] $OutputDirectory = '',
    [switch] $SkipBuild,
    [switch] $SkipTests
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!$BuildDirectory) { $BuildDirectory = Join-Path $sourceRoot 'build-release' }
if (!$OutputDirectory) { $OutputDirectory = Join-Path $sourceRoot 'dist' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$QtDirectory = [IO.Path]::GetFullPath($QtDirectory)
if (!(Test-Path -LiteralPath (Join-Path $QtDirectory 'bin\windeployqt.exe'))) { throw 'QtDirectory must contain a Windows Qt installation with windeployqt.' }
if (!$InnoCompiler) {
    $compilerCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($compilerCommand) { $InnoCompiler = $compilerCommand.Source }
    else { $InnoCompiler = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe' }
}
if (!(Test-Path -LiteralPath $InnoCompiler -PathType Leaf)) { throw 'Provide -InnoCompiler with the path to ISCC.exe.' }
$versionMatch = [regex]::Match([IO.File]::ReadAllText((Join-Path $sourceRoot 'CMakeLists.txt')), 'project\(tree_md VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
if (!$versionMatch.Success) { throw 'Cannot determine the Tmd release version.' }
$version = $versionMatch.Groups[1].Value
$env:PATH = (Join-Path $QtDirectory 'bin') + ';' + $env:PATH
if (!$SkipBuild) {
    & cmake -S $sourceRoot -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtDirectory" -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build $BuildDirectory --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
}
$executable = Join-Path $BuildDirectory 'tree_md.exe'
if (!(Test-Path -LiteralPath $executable)) { throw 'The application executable is missing.' }
if (!$SkipTests) {
    & (Join-Path $QtDirectory 'bin\windeployqt.exe') --release --compiler-runtime --no-translations --no-system-d3d-compiler --no-opengl-sw $executable
    if ($LASTEXITCODE -ne 0) { throw 'Test runtime deployment failed.' }
    Copy-Item -LiteralPath (Join-Path $QtDirectory 'bin\Qt6Test.dll') -Destination $BuildDirectory -Force
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $QtDirectory 'plugins\platforms'
    & ctest --test-dir $BuildDirectory --output-on-failure -j 1
    if ($LASTEXITCODE -ne 0) { throw 'Regression tests failed; no release package was created.' }
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$packageRoot = Join-Path $sourceRoot 'package'
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
$staging = Join-Path $packageRoot ('release-' + $version + '-' + [guid]::NewGuid().ToString('N'))
$portableRoot = Join-Path $staging 'portable'
$installedRoot = Join-Path $staging 'installed'
New-Item -ItemType Directory -Path $portableRoot | Out-Null
Copy-Item -LiteralPath $executable -Destination $portableRoot
& (Join-Path $QtDirectory 'bin\windeployqt.exe') --release --compiler-runtime --no-translations --no-system-d3d-compiler --no-opengl-sw --dir $portableRoot (Join-Path $portableRoot 'tree_md.exe')
if ($LASTEXITCODE -ne 0) { throw 'Application runtime deployment failed.' }
foreach ($file in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','CHANGELOG.md')) { Copy-Item -LiteralPath (Join-Path $sourceRoot $file) -Destination $portableRoot }
foreach ($folder in @('docs','licenses','examples')) { Copy-Item -LiteralPath (Join-Path $sourceRoot $folder) -Destination $portableRoot -Recurse }
# Only the reviewed starter library is distributed; working md_data is never packaged.
Copy-Item -LiteralPath (Join-Path $sourceRoot 'examples\md_data') -Destination $portableRoot -Recurse
Copy-Item -LiteralPath (Join-Path $sourceRoot 'examples\md_photo') -Destination $portableRoot -Recurse
Copy-Item -LiteralPath $portableRoot -Destination $installedRoot -Recurse
foreach ($folder in @('md_data','md_photo')) {
    $from = [IO.Path]::GetFullPath((Join-Path $installedRoot $folder))
    $to = [IO.Path]::GetFullPath((Join-Path $staging ('installed-seed-' + $folder)))
    if (!$from.StartsWith([IO.Path]::GetFullPath($staging) + '\', [StringComparison]::OrdinalIgnoreCase) -or !$to.StartsWith([IO.Path]::GetFullPath($staging) + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected staging move target.' }
    Move-Item -LiteralPath $from -Destination $to
}
$portableName = 'Tmd-' + $version + '-Windows-x64-Portable.zip'
$portableZip = Join-Path $OutputDirectory $portableName
if (Test-Path -LiteralPath $portableZip) { throw 'The portable release already exists. Choose a fresh output directory.' }
[IO.Compression.ZipFile]::CreateFromDirectory($portableRoot, $portableZip, [IO.Compression.CompressionLevel]::Optimal, $false)
& $InnoCompiler "/DAppVersion=$version" "/DPayloadDir=$installedRoot" "/DOutputDir=$OutputDirectory" (Join-Path $sourceRoot 'installer\Tmd.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
$setupName = 'Tmd-' + $version + '-Windows-x64-Setup.exe'
$lines = foreach ($name in @($setupName, $portableName)) {
    ((Get-FileHash -LiteralPath (Join-Path $OutputDirectory $name) -Algorithm SHA256).Hash.ToLowerInvariant()) + '  ' + $name
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'SHA256SUMS.txt'), (($lines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
$manifest = [ordered]@{ version = $version; portableDirectory = $portableRoot; installedPayload = $installedRoot; outputDirectory = $OutputDirectory; setup = $setupName; portable = $portableName }
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'build-manifest.json') -Encoding utf8
$manifest | ConvertTo-Json

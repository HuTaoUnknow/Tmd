param([switch] $CheckOnly)

$ErrorActionPreference = 'Stop'
try {
    $projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $executable = Join-Path $projectRoot 'build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug/tree_md.exe'
    $qtRoot = 'D:/PC/EXE/QtCreator/6.11.1/mingw_64'
    $compilerBin = 'D:/PC/EXE/QtCreator/Tools/mingw1310_64/bin'
    foreach ($required in @($executable, (Join-Path $qtRoot 'bin/Qt6Widgets.dll'), (Join-Path $qtRoot 'plugins/platforms/qwindows.dll'), (Join-Path $compilerBin 'libstdc++-6.dll'))) {
        if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing Debug runtime file: $required" }
    }
    $env:PATH = (Join-Path $qtRoot 'bin') + ';' + $compilerBin + ';' + $env:PATH
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $qtRoot 'plugins/platforms'
    $env:QT_QPA_PLATFORM = 'windows'
    if ($CheckOnly) {
        [pscustomobject]@{ executable = $executable; workingDirectory = $projectRoot; platformPlugins = $env:QT_QPA_PLATFORM_PLUGIN_PATH }
        return
    }
    Start-Process -FilePath $executable -WorkingDirectory $projectRoot | Out-Null
} catch {
    if (!$CheckOnly) {
        Add-Type -AssemblyName System.Windows.Forms
        [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Tmd Debug', 'OK', 'Error') | Out-Null
    }
    throw
}

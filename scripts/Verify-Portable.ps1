param(
    [Parameter(Mandatory)]
    [string] $Executable,
    [ValidateSet("MT", "MD")]
    [string] $MsvcRuntime = "MT"
)

$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $Executable).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) {
    throw 'Visual Studio with x64 C++ tools is required for dependency inspection.'
}
$dumpbin = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe') |
    Sort-Object { [version] $_.Directory.Parent.Parent.Parent.Name } -Descending |
    Select-Object -First 1
if (!$dumpbin) {
    throw 'Could not locate dumpbin.exe.'
}

$report = & $dumpbin.FullName /dependents $source
if ($LASTEXITCODE -ne 0) {
    throw 'dumpbin /dependents failed.'
}
$dlls = @($report | ForEach-Object {
    if ($_ -match '^\s+([a-zA-Z0-9_.-]+\.dll)\s*$') { $Matches[1] }
} | Sort-Object -Unique)
if (!$dlls.Count) {
    throw 'No DLL imports were found in the dependency report.'
}

$systemDirectory = Join-Path $env:SystemRoot 'System32'
Write-Host ('PE imports (including delay imports listed by dumpbin) ' + ($dlls -join ', '))
$vcRuntimePattern = '^(vcruntime|msvcp|msvcr|vcomp|concrt|vcamp|mfc|atl)\d.*\.dll$'
$vcRuntimeImports = @($dlls | Where-Object { $_ -match $vcRuntimePattern })
if ($MsvcRuntime -eq 'MT' -and $vcRuntimeImports.Count) {
    throw ('Unexpected Visual C++ redistributable dependencies ' + ($vcRuntimeImports -join ', '))
}
if ($MsvcRuntime -eq 'MD' -and !$vcRuntimeImports.Count) {
    throw 'The /MD build is missing the expected Visual C++ runtime imports.'
}
foreach ($dll in $vcRuntimeImports) {
    $installed = Get-Item -LiteralPath (Join-Path $systemDirectory $dll)
    Write-Host ("Installed Visual C++ runtime {0} ({1})" -f $dll, $installed.VersionInfo.FileVersion)
}
foreach ($dll in $dlls) {
    if ($dll -notmatch '^(api|ext)-ms-win-' -and
        !(Test-Path -LiteralPath (Join-Path $systemDirectory $dll) -PathType Leaf)) {
        throw "DLL dependency missing from System32: $dll"
    }
}

$root = Join-Path ([System.IO.Path]::GetTempPath()) ('SimpleDAW-portable-' + [guid]::NewGuid())
$exeDirectory = Join-Path $root 'Standalone executable'
$workingDirectory = Join-Path $root 'Different working directory'
$originalPath = $env:PATH
$originalAcl = $null
$process = $null
try {
    New-Item -ItemType Directory -Path $exeDirectory, $workingDirectory | Out-Null
    $standalone = Join-Path $exeDirectory 'SimpleDAW.exe'
    Copy-Item -LiteralPath $source -Destination $standalone
    $originalAcl = Get-Acl -LiteralPath $exeDirectory
    $restrictedAcl = Get-Acl -LiteralPath $exeDirectory
    $identity = [System.Security.Principal.WindowsIdentity]::GetCurrent().User
    $denyWrite = [System.Security.AccessControl.FileSystemAccessRule]::new(
        $identity, 'Write', 'ContainerInherit, ObjectInherit', 'None', 'Deny')
    $restrictedAcl.AddAccessRule($denyWrite)
    Set-Acl -LiteralPath $exeDirectory -AclObject $restrictedAcl

    $probe = Join-Path $exeDirectory 'write-probe'
    try {
        [System.IO.File]::WriteAllText($probe, 'probe')
        throw 'The standalone executable directory is writable.'
    }
    catch [System.UnauthorizedAccessException] {
        Write-Host 'Confirmed write access is denied in the executable directory.'
    }

    $env:PATH = $systemDirectory
    $process = Start-Process -FilePath $standalone -ArgumentList '--smoke-test' -WorkingDirectory $workingDirectory -PassThru
    if (!$process.WaitForExit(60000) -or $process.ExitCode -ne 0) {
        throw 'The isolated project round-trip smoke test failed or timed out.'
    }
    Write-Host 'Standalone smoke test passed from an unrelated working directory.'

    $process = Start-Process -FilePath $standalone -WorkingDirectory $workingDirectory -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        $process.Refresh()
        if ($process.HasExited) {
            throw "Normal startup exited before creating a window (exit code $($process.ExitCode))."
        }
        if ($process.MainWindowTitle -eq 'SimpleDAW v0.21') {
            break
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($process.MainWindowTitle -ne 'SimpleDAW v0.21') {
        throw 'The main SimpleDAW window did not appear.'
    }
    if (!$process.CloseMainWindow() -or !$process.WaitForExit(10000) -or $process.ExitCode -ne 0) {
        throw 'The standalone window did not close cleanly.'
    }
    Write-Host 'Normal startup displayed the SimpleDAW window and closed cleanly.'
}
finally {
    $env:PATH = $originalPath
    if ($process -and !$process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
    if ($originalAcl) {
        Set-Acl -LiteralPath $exeDirectory -AclObject $originalAcl
    }
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}

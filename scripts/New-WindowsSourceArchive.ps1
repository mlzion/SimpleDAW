param(
    [Parameter(Mandatory)] [string] $BinaryBuildDirectory,
    [Parameter(Mandatory)] [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build = (Resolve-Path -LiteralPath $BinaryBuildDirectory).Path
$output = (New-Item -Path $OutputDirectory -ItemType Directory -Force).FullName
$stage = Join-Path $output 'stage'
$archive = Join-Path $output 'SimpleDAW-Windows-x64-source.tar.gz'
New-Item -Path $stage -ItemType Directory | Out-Null

$sources = [ordered] @{
    SimpleDAW = $project
    JUCE = Join-Path $build '_deps/juce-src'
    Tracktion = Join-Path $build '_deps/tracktion-src'
}

$revisions = foreach ($name in $sources.Keys) {
    $source = $sources[$name]
    $commit = git -C $source rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw "Could not resolve the $name revision." }

    $member = Join-Path $output "$name.tar"
    git -C $source archive --format=tar --prefix="$name/" --output=$member $commit
    if ($LASTEXITCODE -ne 0) { throw "Could not archive $name." }
    tar.exe -xf $member -C $stage
    if ($LASTEXITCODE -ne 0) { throw "Could not stage $name." }
    Remove-Item -LiteralPath $member
    "$name @ $commit"
}

$revisions | Set-Content -LiteralPath (Join-Path $stage 'SOURCE-REVISIONS.txt')
Copy-Item -LiteralPath (Join-Path $stage 'SimpleDAW/docs/corresponding-source.md') -Destination (Join-Path $stage 'BUILD.md')
tar.exe -czf $archive -C $stage .
if ($LASTEXITCODE -ne 0) { throw 'Could not create the matching source archive.' }
Remove-Item -LiteralPath $stage -Recurse
$revisions | Write-Output
Write-Output ('Source archive SHA256 ' + (Get-FileHash -LiteralPath $archive).Hash)

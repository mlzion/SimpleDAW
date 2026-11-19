param([Parameter(Mandatory)] [string] $Destination)

$ErrorActionPreference = 'Stop'
$release = '20251118'
$name = "llvm-mingw-$release-ucrt-x86_64"
$expected = 'D14A2022C095B83404B64A249406478D852821D31BEAA32ABE31C932832B3538'
$directory = (New-Item -Path $Destination -ItemType Directory -Force).FullName
$zip = Join-Path $directory "$name.zip"

Invoke-WebRequest -Uri "https://github.com/mstorsjo/llvm-mingw/releases/download/$release/$name.zip" -OutFile $zip
if ((Get-FileHash -LiteralPath $zip).Hash -ne $expected) {
    throw 'The LLVM/MinGW package checksum did not match the pinned release.'
}
Expand-Archive -LiteralPath $zip -DestinationPath $directory
Remove-Item -LiteralPath $zip
Join-Path $directory "$name/bin"

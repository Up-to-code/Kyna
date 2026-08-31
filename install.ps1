[CmdletBinding()]
param(
    [string]$Prefix = (Join-Path $env:LOCALAPPDATA "Kyna"),
    [string]$Version = "",
    [ValidateSet("stable", "preview")][string]$Channel = "stable",
    [switch]$NonInteractive,
    [switch]$NoPathUpdate
)
$ErrorActionPreference = "Stop"
if ($Channel -eq "preview" -and -not $Version) {
    throw "Preview installs require -Version and never use the stable latest URL."
}
$architecture = switch ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture) {
    "X64" { "x86_64" }
    "Arm64" { "arm64" }
    default { throw "Unsupported Windows architecture." }
}
$asset = "kyna-windows-$architecture.zip"
if ($env:KYNA_RELEASE_BASE_URL) {
    # Integration tests serve assets from a disposable loopback server.
    # Production installs leave this unset and always use GitHub over HTTPS.
    $base = $env:KYNA_RELEASE_BASE_URL.TrimEnd("/")
} elseif ($Version) {
    $tag = if ($Version.StartsWith("v")) { $Version } else { "v$Version" }
    $base = "https://github.com/Up-to-code/Kyna/releases/download/$tag"
} else {
    $base = "https://github.com/Up-to-code/Kyna/releases/latest/download"
}
$temporary = Join-Path ([IO.Path]::GetTempPath()) ("kyna-" + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Path $temporary | Out-Null
    $archive = Join-Path $temporary $asset
    $sums = Join-Path $temporary "SHA256SUMS"
    Invoke-WebRequest "$base/$asset" -OutFile $archive -UseBasicParsing
    Invoke-WebRequest "$base/SHA256SUMS" -OutFile $sums -UseBasicParsing
    $line = Get-Content $sums | Where-Object { $_ -match "\s\*?$([regex]::Escape($asset))$" } | Select-Object -First 1
    if (-not $line) { throw "$asset is missing from SHA256SUMS." }
    $expected = ($line -split "\s+")[0].ToLowerInvariant()
    $actual = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw "Checksum verification failed." }
    $unpacked = Join-Path $temporary "unpacked"
    Expand-Archive $archive $unpacked
    $source = Get-ChildItem $unpacked -Recurse -Filter "ky.exe" | Where-Object { $_.Directory.Name -eq "bin" } | Select-Object -First 1
    if (-not $source) { throw "Archive does not contain bin/ky.exe." }
    $bin = Join-Path $Prefix "bin"
    New-Item -ItemType Directory -Force -Path $bin | Out-Null
    $destination = Join-Path $bin "ky.exe"
    if (Test-Path $destination) { Copy-Item $destination "$destination.previous" -Force }
    $alias = Join-Path $bin "kyna.exe"
    if (Test-Path $alias) { Copy-Item $alias "$alias.previous" -Force }
    Copy-Item $source.FullName "$destination.new" -Force
    Move-Item "$destination.new" $destination -Force
    Copy-Item $destination $alias -Force
    $installedFiles = [Collections.Generic.List[string]]::new()
    $installedFiles.Add("bin/ky.exe")
    $installedFiles.Add("bin/kyna.exe")
    if (Test-Path "$destination.previous") { $installedFiles.Add("bin/ky.exe.previous") }
    if (Test-Path "$alias.previous") { $installedFiles.Add("bin/kyna.exe.previous") }
    foreach ($dependency in Get-ChildItem $source.Directory.FullName -Filter "*.dll") {
        $dependencyDestination = Join-Path $bin $dependency.Name
        if (Test-Path $dependencyDestination) { Copy-Item $dependencyDestination "$dependencyDestination.previous" -Force }
        Copy-Item $dependency.FullName "$dependencyDestination.new" -Force
        Move-Item "$dependencyDestination.new" $dependencyDestination -Force
        $installedFiles.Add("bin/$($dependency.Name)")
        if (Test-Path "$dependencyDestination.previous") { $installedFiles.Add("bin/$($dependency.Name).previous") }
    }
    $packageRoot = $source.Directory.Parent
    $shareSource = Join-Path $packageRoot.FullName "share/kyna"
    $shareDestination = Join-Path $Prefix "share/kyna"
    New-Item -ItemType Directory -Force -Path $shareDestination | Out-Null
    if (Test-Path $shareSource) { Copy-Item (Join-Path $shareSource "*") $shareDestination -Recurse -Force }
    $installedFiles | Set-Content (Join-Path $shareDestination "install-manifest.txt") -Encoding Ascii
    if (-not $NoPathUpdate) {
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        if (($userPath -split ";") -notcontains $bin) {
            $updated = if ($userPath) { "$userPath;$bin" } else { $bin }
            [Environment]::SetEnvironmentVariable("Path", $updated, "User")
            $env:Path = "$env:Path;$bin"
            Write-Host "Added $bin to the user PATH. Open a new terminal to use it everywhere."
        }
    }
    Write-Host "Installed ky.exe and the kyna.exe compatibility alias in $bin"
} finally {
    if (Test-Path $temporary) { Remove-Item $temporary -Recurse -Force }
}

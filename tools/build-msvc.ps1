<#
.SYNOPSIS
  Setup MSVC environment via Developer PowerShell and run build steps.

.PARAMETER Mode
  one of: full | fast | clean

#>
param (
  [Parameter(Mandatory=$true)]
  [ValidateSet("full","fast","clean")]
  [string]$Mode
)

# Locate the latest Visual Studio install via vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallPath = & $vswhere -latest -prerelease -products * -property installationPath

if (-not $vsInstallPath) {
  Write-Error "Visual Studio not found via vswhere"
  exit 1
}

# Import the DevShell module (searching recursively)
$devShellDll = Get-ChildItem -Path $vsInstallPath -Recurse -Filter "Microsoft.VisualStudio.DevShell.dll" |
               Sort-Object -Property FullName -Descending |
               Select-Object -First 1

Import-Module $devShellDll.FullName

# Enter the DevShell environment
Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'

# Now run build logic
switch ($Mode) {
    'full' {
        Remove-Item -Recurse -Force .\build_msvc -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force .\build_msvc | Out-Null
        Set-Location .\build_msvc
        cmake .. 
        cmake --build . --config Release
        Set-Location ..
    }
    'fast' {
        Set-Location .\build_msvc
        cmake --build . --config Release
        Set-Location ..
    }
    'clean' {
        Remove-Item -Recurse -Force .\build_msvc -ErrorAction SilentlyContinue
    }
}

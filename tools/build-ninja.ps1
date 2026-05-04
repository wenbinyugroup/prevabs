<#
.SYNOPSIS
  Setup MSVC environment via Developer PowerShell and run build steps
  using the Ninja generator so that compile_commands.json gets generated.

.PARAMETER Mode
  one of: full | fast | clean
#>

param (
  [Parameter(Mandatory=$true)]
  [ValidateSet("full","fast","clean")]
  [string]$Mode
)

# Locate Visual Studio via vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallPath = & $vswhere -latest -prerelease -products * -property installationPath
if (-not $vsInstallPath) {
  Write-Error "Visual Studio not found via vswhere"
  exit 1
}

# Import the Developer Shell
$devShellDll = Get-ChildItem -Path $vsInstallPath -Recurse -Filter "Microsoft.VisualStudio.DevShell.dll" |
               Sort-Object -Property FullName -Descending |
               Select-Object -First 1
Import-Module $devShellDll.FullName

# Enter MSVC environment
Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'

# Build steps
switch ($Mode) {

    'full' {
        Remove-Item -Recurse -Force .\build_ninja -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force .\build_ninja | Out-Null
        Set-Location .\build_ninja

        # Configure with Ninja; generate compile_commands.json
        cmake .. -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

        # Build with the Ninja backend
        cmake --build . --config Release

        Set-Location ..
    }

    'fast' {
        Set-Location .\build_ninja
        cmake --build . --config Release
        Set-Location ..
    }

    'clean' {
        Remove-Item -Recurse -Force .\build_ninja -ErrorAction SilentlyContinue
    }
}

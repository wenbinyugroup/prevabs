(section-install)=
# Download and Installation

Download: <https://github.com/wenbinyugroup/prevabs/releases>

## Prerequisites

Download and install VABS (with valid license) if you intend to run VABS analyses.
In the instructions below, `VABS_DIR` refers to the path to the VABS executable.
VABS manual can be found and downloaded here {cite}`vabs`.

PreVABS itself does not require Gmsh to be installed separately — its meshing capabilities are provided by the Gmsh library which is bundled with the PreVABS distribution.
Installing the standalone Gmsh GUI from <https://gmsh.info/> is still recommended if you want to inspect generated `.geo_unrolled` / `.msh` files outside of the PreVABS workflow.
In the instructions below, `GMSH_DIR` refers to the path to the Gmsh executable (only needed if you install Gmsh separately).

## Install binary

Obtain a PreVABS release archive (or build from source — see the project README) and unpack it to any location.
In the instructions below, `PREVABS_DIR` refers to the path to the PreVABS executable;

Add those paths to executables to the system environment variable `PATH`;

### On Windows:

Open Environment Variables editor.
Edit user variables for your account or edit system variables if you have the administrator access.
Add `VABS_DIR`, `GMSH_DIR`, and `PREVABS_DIR` to the variable `PATH`.

### On Linux:

In the shell, type:

```bash
export PATH=$VABS_DIR:$GMSH_DIR:$PREVABS_DIR:$PATH
```

To make this effective each time starting the bash, you can add this command to the bash startup file, which may be `~/.bashrc`, `~/.bash_profile`, `~/.profile`, or `~/bash_login`;


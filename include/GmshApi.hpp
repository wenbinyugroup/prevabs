#ifndef GMSH_API_HPP
#define GMSH_API_HPP

// The Windows SDK C API is ABI-stable across MSVC versions; route the C++
// calls through gmsh.h_cwrap instead of relying on the native C++ ABI.
#if defined(_WIN32)
#include <gmsh.h_cwrap>
#else
#include <gmsh.h>
#endif

#endif

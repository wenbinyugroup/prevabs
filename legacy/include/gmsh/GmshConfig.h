// Gmsh - Copyright (C) 1997-2017 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to the public mailing list <gmsh@onelab.info>.

#ifndef _GMSH_CONFIG_H_
#define _GMSH_CONFIG_H_

/* #undef HAVE_3M */
/* #undef HAVE_ACIS */
#define HAVE_ANN
#define HAVE_BAMG
#define HAVE_BFGS
#define HAVE_BLAS
#define HAVE_BLOSSOM
/* #undef HAVE_CAIRO */
#define HAVE_CHACO
/* #undef HAVE_COMPRESSED_IO */
#define HAVE_DLOPEN
#define HAVE_DINTEGRATION
/* #undef HAVE_FLTK */
/* #undef HAVE_FOURIER_MODEL */
#define HAVE_GMM
#define HAVE_GMP
#define HAVE_KBIPACK
#define HAVE_LAPACK
/* #undef HAVE_LIBCGNS */
/* #undef HAVE_LIBJPEG */
/* #undef HAVE_LIBPNG */
/* #undef HAVE_LIBZ */
#define HAVE_LINUX_JOYSTICK
#define HAVE_MATHEX
/* #undef HAVE_MED */
#define HAVE_MESH
#define HAVE_METIS
#define HAVE_MMG3D
/* #undef HAVE_MPEG_ENCODE */
/* #undef HAVE_MPI */
/* #undef HAVE_MUMPS */
#define HAVE_NATIVE_FILE_CHOOSER
#define HAVE_NETGEN
/* #undef HAVE_NUMPY */
/* #undef HAVE_NO_INTPTR_T */
/* #undef HAVE_NO_SOCKLEN_T */
/* #undef HAVE_NO_STDINT_H */
/* #undef HAVE_NO_VSNPRINTF */
/* #undef HAVE_OCC */
/* #undef HAVE_OCC_CAF */
#define HAVE_ONELAB
/* #undef HAVE_ONELAB2 */
#define HAVE_ONELAB_METAMODEL
/* #undef HAVE_UDT */
/* #undef HAVE_OPENGL */
#define HAVE_OPTHOM
/* #undef HAVE_OSMESA */
#define HAVE_PARSER
#define HAVE_PETSC
/* #undef HAVE_PETSC4PY */
#define HAVE_PLUGINS
#define HAVE_POST
/* #undef HAVE_POPPLER */
/* #undef HAVE_QT */
/* #undef HAVE_REVOROPT */
/* #undef HAVE_SALOME */
/* #undef HAVE_SGEOM */
/* #undef HAVE_SLEPC */
#define HAVE_SOLVER
#define HAVE_TAUCS
#define HAVE_TETGEN
#define HAVE_VORO3D
/* #undef HAVE_ZIPPER */

#define GMSH_CONFIG_OPTIONS " 64Bit Ann Bamg Bfgs Blas(Generic) Blossom C++11 Chaco DIntegration Dlopen GMP Gmm Kbipack Lapack(Generic) LinuxJoystick MathEx Mesh Metis Mmg3d NativeFileChooser Netgen ONELAB ONELABMetamodel OptHom PETSc Parser Plugins Post Solver Taucs TetGen/BR Tetgen1.5 Voro3D"



#endif

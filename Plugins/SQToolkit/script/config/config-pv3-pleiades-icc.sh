#!/bin/bash

ICC_PATH=/nasa/intel/cce/10.1.021
MPI_PATH=/nasa/sgi/mpt/1.26
MESA_PATH=/u/bloring/apps/Mesa-7.5.1-Intel

cmake \
  -DCMAKE_C_COMPILER=$ICC_PATH/bin/icc \
  -DCMAKE_CXX_COMPILER=$ICC_PATH/bin/icpc \
  -DCMAKE_LINKER=$ICC_PATH/bin/icpc \
  -DCMAKE_CXX_FLAGS=-Wno-deprecated \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DPARAVIEW_BUILD_QT_GUI=OFF \
  -DPARAVIEW_USE_MPI=ON \
  -DMPI_INCLUDE_PATH=$MPI_PATH/include \
  -DMPI_LIBRARY=$MPI_PATH/lib/libmpi.so \
  -DVTK_OPENGL_HAS_OSMESA=ON \
  -DOPENGL_INCLUDE_DIR=$MESA_PATH/include \
  -DOPENGL_gl_LIBRARY=$MESA_PATH/lib64/libGL.so \
  -DOPENGL_glu_LIBRARY=$MESA_PATH/lib64/libGLU.so \
  -DOPENGL_xmesa_INCLUDE_DIR=$MESA_PATH/include \
  -DOSMESA_INCLUDE_DIR=$MESA_PATH/include \
  -DOSMESA_LIBRARY=$MESA_PATH/lib64/libOSMesa.so \
  $*


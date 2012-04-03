PROJECT(paraviewjni)

FIND_PACKAGE(SWIG 2.0 REQUIRED)
FIND_PACKAGE(JNI REQUIRED)
INCLUDE(${SWIG_USE_FILE})

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  FIND_PACKAGE(GTK2 REQUIRED)
  SET(LIBS ${LIBS} ${GTK2_LIBRARIES})
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

#include Cubit related directories
INCLUDE_DIRECTORIES(${paraviewjni_SOURCE_DIR}
                    ${JNI_INCLUDE_DIRS}
                    ${GTK2_INCLUDE_DIRS}
                    ${VTK_INCLUDE_DIR}
                    ${PARAVIEW_INCLUDE_DIRS})

option(ADD_TO_ECLIPSE_WORKSPACE "Install ParaView JNI in eclipse workspace" OFF)
if (ADD_TO_ECLIPSE_WORKSPACE)
  SET(ECLIPSE_WORKSPACE "" CACHE PATH "Path to top Eclipse workspace")
  SET(ECLIPSE_PROJECT_NAME "" CACHE PATH "Name of Eclipse project")
endif()

set(JNI_SRCS vtkGenericRenderWindowAndInteractorWidget.cxx vtkGenericRenderWindowAndInteractorWidget.h)
set(DOT_I vtkGenericRenderWindowAndInteractorWidgetJNI.i)

# create the swig custom command
SET(SWIG_MODULE_paraviewjni_EXTRA_DEPS 
    ${ParaView_SOURCE_DIR}/Utilities/VTKJavaWrapping/jni/vtkGenericRenderWindowAndInteractorWidget.h
  )

STRING(REGEX REPLACE "\\." "/" PARAVIEW_JNI_PACKAGE_NAME_PATH ${PARAVIEW_JNI_PACKAGE_NAME})

# Create the custom swig command.
SET(CMAKE_SWIG_FLAGS "-package" ${PARAVIEW_JNI_PACKAGE_NAME})
IF(CMAKE_CONFIGURATION_TYPES)
  SET(CMAKE_SWIG_OUTDIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR})
ELSE(CMAKE_CONFIGURATION_TYPES)
  SET(CMAKE_SWIG_OUTDIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
ENDIF(CMAKE_CONFIGURATION_TYPES)
SET_SOURCE_FILES_PROPERTIES(${DOT_I} PROPERTIES CPLUSPLUS ON)
SWIG_ADD_MODULE(paraviewjni java ${DOT_I} ${JNI_SRCS})

SWIG_LINK_LIBRARIES(paraviewjni vtkRendering vtkGraphics vtkPVServerManager)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SWIG_LINK_LIBRARIES(paraviewjni gtk-x11-2.0)
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

if (ECLIPSE_WORKSPACE)
  if (WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(JNI_ARCH "win64")
    else()
      set(JNI_ARCH "win32")
    endif()
  elseif(APPLE)
    set(JNI_ARCH "mac")
  elseif(UNIX)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(JNI_ARCH "linux64")
    else()
      set(JNI_ARCH "linux32")
    endif()
  endif()
  install(TARGETS paraviewjni 
          LIBRARY DESTINATION "${ECLIPSE_WORKSPACE}/${ECLIPSE_PROJECT_NAME}/natives/${JNI_ARCH}" COMPONENT Runtime)
   IF(CMAKE_CONFIGURATION_TYPES)
    FOREACH(CONFIG ${CMAKE_CONFIGURATION_TYPES})
        INSTALL(
            CODE "FILE( GLOB _GeneratedJavaSources \"${CMAKE_CURRENT_BINARY_DIR}/${CONFIG}/*.java\" )"
            CODE "FILE( INSTALL \${_GeneratedJavaSources} DESTINATION \"${ECLIPSE_WORKSPACE}/${ECLIPSE_PROJECT_NAME}/src/${PARAVIEW_JNI_PACKAGE_NAME_PATH}\" )"
        )
    ENDFOREACH(CONFIG)
   ELSE(CMAKE_CONFIGURATION_TYPES)
        INSTALL(
            CODE "FILE( GLOB _GeneratedJavaSources \"${CMAKE_CURRENT_BINARY_DIR}/*.java\" )"
            CODE "FILE( INSTALL \${_GeneratedJavaSources} DESTINATION \"${ECLIPSE_WORKSPACE}/${ECLIPSE_PROJECT_NAME}/src/${PARAVIEW_JNI_PACKAGE_NAME_PATH}\" )"
        )
   ENDIF(CMAKE_CONFIGURATION_TYPES)
endif()


IF(JAVA_COMPILE)
  SET(VTK_JAVA_DEPENDENCIES "")
  INCLUDE(${ParaView_BINARY_DIR}/ParaViewCore/ServerManager/JavaDependencies.cmake OPTIONAL)
  INCLUDE(${ParaView_BINARY_DIR}/ParaViewCore/ServerImplementation/JavaDependencies.cmake OPTIONAL)
  INCLUDE(${ParaView_BINARY_DIR}/ParaViewCore/VTKExtensions/JavaDependencies.cmake OPTIONAL)
  INCLUDE(${ParaView_BINARY_DIR}/ParaViewCore/Common/JavaDependencies.cmake OPTIONAL)
  INCLUDE(${ParaView_BINARY_DIR}/ParaViewCore/ClientServerCore/JavaDependencies.cmake OPTIONAL)

  SET(VTK_JAVA_NEW_STYLE 0)
  IF(COMMAND VTK_GENERATE_JAVA_DEPENDENCIES)
    IF(VTK_JAVA_DEPENDENCIES)
      SET(VTK_JAVA_NEW_STYLE 1)
    ENDIF(VTK_JAVA_DEPENDENCIES)
  ENDIF(COMMAND VTK_GENERATE_JAVA_DEPENDENCIES)

  IF(NOT VTK_JAR_PATH)
    SET(VTK_JAR_PATH "${ParaView_BINARY_DIR}/bin/")
  ENDIF(NOT VTK_JAR_PATH)

  IF(VTK_JAVA_NEW_STYLE)
    SET(CLASS_FILES ${VTK_JAVA_DEPENDENCIES})
    STRING(REGEX REPLACE "\\.java" ".class;" CLASS_FILES ${VTK_JAVA_DEPENDENCIES})
    ADD_CUSTOM_TARGET(ParaViewJava ALL)
    ADD_CUSTOM_TARGET(ParaViewJavaJar ALL)

    SET(KITS ServerManager ServerImplementation VTKExtensions Common ClientServerCore)

    SET(CMAKE_SKIP_RPATH 0)
    ADD_EXECUTABLE(ParaViewJavaExecutable ParaViewJava.cxx)
    SET(JAVA_LIBRARIES)

    SET(VTK_PARAVIEW_BUILD_ALL_CONTENT "")
    SET(VTK_PARAVIEW_BUILD_ALL_DEPENDS "")

    FOREACH(kit ${KITS})
      IF(APPLE)
        SET(src ${VTK_JAR_PATH}libvtkPV${kit}Java.dylib)
        SET(tgt ${VTK_JAR_PATH}libvtkPV${kit}Java.jnilib)
        ADD_CUSTOM_COMMAND(OUTPUT ${tgt}
                           COMMAND ln
                           ARGS -sf ${src} ${tgt}
                           DEPENDS ${src})
        SET(JAVA_LIBRARIES ${JAVA_LIBRARIES} ${tgt})
      ENDIF(APPLE)
      TARGET_LINK_LIBRARIES(ParaViewJavaExecutable vtkPV${kit}Java)
      SET(VTK_JAVA_DEPENDENCIES "")
      ADD_CUSTOM_TARGET(ParaViewJava${kit} ALL)
      SET(VTK_PARAVIEW_BUILD_ALL_CONTENT
        "${VTK_PARAVIEW_BUILD_ALL_CONTENT}\n    vtk.vtkParaViewJava${kit}Driver.Initialize(args);")
      SET(VTK_PARAVIEW_BUILD_ALL_DEPENDS
        ${VTK_PARAVIEW_BUILD_ALL_DEPENDS} 
        ${ParaView_BINARY_DIR}/VTK/java/vtk/vtkParaViewJava${kit}Driver.java)
      INCLUDE(${ParaView_BINARY_DIR}/ParaViewCore/${kit}/JavaDependencies.cmake OPTIONAL)
      VTK_GENERATE_JAVA_DEPENDENCIES(ParaViewJava${kit} ${VTK_JAVA_DEPENDENCIES})
      # Make sure all .java files are build before any .class files are built.
      FOREACH(otherKit ${KITS})
        ADD_DEPENDENCIES(ParaViewJava${kit} vtkPV${otherKit}JavaJavaClasses)
      ENDFOREACH(otherKit)
    ENDFOREACH(kit)

    CONFIGURE_FILE(${ParaView_SOURCE_DIR}/VTK/Wrapping/Java/vtkBuildAllDriver.java.in
      ${ParaView_BINARY_DIR}/VTK/java/vtk/vtkParaViewBuildAllDriver.java)
    ADD_CUSTOM_TARGET(ParaViewBuildAll ALL)

    ADD_CUSTOM_COMMAND(
      TARGET ParaViewBuildAll
      OUTPUTS ${ParaView_BINARY_DIR}/VTK/java/vtk/vtkParaViewBuildAllDriver.class
      DEPENDS ${VTK_PARAVIEW_BUILD_ALL_DEPENDS}
      SOURCE ${ParaView_BINARY_DIR}/VTK/java/vtk/vtkParaViewBuildAllDriver.java
      COMMAND ${JAVA_COMPILE}
      ARGS -classpath  ${VTK_JAVA_HOME}/.. -d ${VTK_JAVA_HOME}/.. 
      ${ParaView_BINARY_DIR}/VTK/java/vtk/vtkParaViewBuildAllDriver.java
      )

    ADD_CUSTOM_COMMAND(
      TARGET ParaViewBuildAll
      SOURCE ParaViewBuildAll
      DEPENDS ${ParaView_BINARY_DIR}/VTK/java/vtk/vtkParaViewBuildAllDriver.class
      )

    ADD_DEPENDENCIES(ParaViewJavaJar ParaViewBuildAll)

    # Make sure all the VTKJava${kit} projects build *before* VTKBuildAll.
    # (All individual generated .java files should be compiled into .class
    # files before compiling the "driver" sources...)
    #
    FOREACH(kit ${KITS})
      ADD_DEPENDENCIES(ParaViewBuildAll ParaViewJava${kit})
      ADD_DEPENDENCIES(ParaViewBuildAll vtk${kit}Java)
    ENDFOREACH(kit)

    ADD_DEPENDENCIES(ParaViewJavaServerManager ParaViewJavaServerImplementation)
    ADD_DEPENDENCIES(ParaViewJavaServerImplementation ParaViewJavaClientServerCore)
    ADD_DEPENDENCIES(ParaViewJavaVTKExtensions ParaViewJavaCommon)
    ADD_DEPENDENCIES(ParaViewJavaClientServerCore ParaViewJavaVTKExtensions)

    CONFIGURE_FILE(${ParaView_SOURCE_DIR}/Utilities/VTKJavaWrapping/paraview/vtkClientServerStream.java
                   ${VTK_JAVA_HOME}/vtkClientServerStream.java
                   COPYONLY)

    ADD_DEPENDENCIES(ParaViewJava ParaViewJavaJar)
    
    ADD_CUSTOM_COMMAND(SOURCE ${VTK_JAVA_HOME}/vtkObject.class
      COMMAND ${JAVA_ARCHIVE}
      ARGS -uvf  "${VTK_JAR_PATH}/vtk.jar"
      -C ${ParaView_BINARY_DIR}/VTK/java
      vtk
      TARGET ParaViewJavaJar
      DEPENDS ${CLASS_FILES} ${JAVA_LIBRARIES}
      OUTPUTS ${VTK_JAR_PATH}/vtk.jar
      COMMENT "Java Archive")

    ADD_CUSTOM_COMMAND(SOURCE ParaViewJavaJar
      TARGET ParaViewJavaJar
      DEPENDS ${VTK_JAR_PATH}/vtk.jar)

  ELSE(VTK_JAVA_NEW_STYLE)

    ADD_CUSTOM_TARGET(ParaViewJava ALL)

    ADD_CUSTOM_COMMAND(SOURCE ${VTK_JAVA_HOME}/VTKJavaWrapped
      COMMAND ${JAVA_COMPILE} 
      ARGS ${VTK_JAVA_HOME}/vtk*.java
      TARGET ParaViewJava
      OUTPUTS ${VTK_JAVA_HOME}/vtkObject.class)

    IF(JAVA_ARCHIVE)
      ADD_CUSTOM_COMMAND(SOURCE ${VTK_JAVA_HOME}/vtkObject.class
        COMMAND ${CMAKE_COMMAND}
        ARGS -E chdir 
        ${ParaView_BINARY_DIR}/java ${JAVA_ARCHIVE}
        -uvf "\"${VTK_JAR_PATH}/vtk.jar\""
        vtk
        TARGET ParaViewJava
        DEPENDS ${VTK_JAVA_HOME}/vtkObject.class
        OUTPUTS ${VTK_JAR_PATH}/vtk.jar)

    ENDIF(JAVA_ARCHIVE)
  ENDIF(VTK_JAVA_NEW_STYLE)

  IF(NOT PV_INSTALL_NO_RUNTIME)
    INSTALL(FILES
      ${VTK_JAR_PATH}/vtk.jar
      DESTINATION ${PV_INSTALL_LIB_DIR_CM24}
      COMPONENT RuntimeLibraries)
  ENDIF(NOT PV_INSTALL_NO_RUNTIME)
ENDIF(JAVA_COMPILE)

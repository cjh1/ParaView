ADD_EXECUTABLE(ParaViewCoreServerImplementationPrintSelf ParaViewCoreServerImplementationPrintSelf.cxx)
ADD_TEST(ParaViewCoreServerImplementationPrintSelf
  ${CXX_TEST_PATH}/ParaViewCoreServerImplementationPrintSelf)
TARGET_LINK_LIBRARIES(ParaViewCoreServerImplementationPrintSelf vtkPVServerImplementation)

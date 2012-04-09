//
// This wraps vtkGenericRenderWindowAndInteractorWidget for
// subclassing in Java to create an interactive render window.
//

%include "std_string.i"
#pragma SWIG nowarn=325,473,503

%module(directors="1") vtkGenRenWinInterWidget

#define VTK_RENDERING_EXPORT
#define VTK_COMMON_EXPORT
#define SV_EXPORT

%include "vtkSetGet.h"

%feature("director")   vtkGenericRenderWindowAndInteractorWidget;
%{
    #include "vtkGenericRenderWindowAndInteractorWidget.h"
%}
%include "vtkGenericRenderWindowAndInteractorWidget.h"

%feature("director")   vtkObject;
%feature("director")   vtkObjectBase;
%{
#include "vtkObjectBase.h"
#include "vtkObject.h"
%}
%include "vtkObjectBase.h"
%include "vtkObject.h"

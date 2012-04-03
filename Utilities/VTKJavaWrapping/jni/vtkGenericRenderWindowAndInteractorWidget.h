
/*=========================================================================

  Program:   ParaView
  Module:    vtkGenericRenderWindowAndInteractorWidget.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkGenericRenderWindowAndInteractorWidget - SWIG wrapped class for Java display of vtkSMRenderViewProxy.
// .SECTION Description
//  The vtkGenericRenderWindowAndInteractorWidget class provides an interface to an instance of
//  vtkSMRenderViewProxy through the JNI interface provided by SWIG directors.  This class can
//  be subclassed in Java to pass window events and user interaction events back to this class, 
//  allowing ParaView to render inside of Java windows.
// .SECTION Thanks
//  authors Karl Merkley and Clint Stimpson, with modifications for ParaView by Thomas Otahal

#ifndef __vtkGenericRenderWindowAndInteractorWidget_h
#define __vtkGenericRenderWindowAndInteractorWidget_h

#include "vtkSmartPointer.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkGenericOpenGLRenderWindow.h"
#include <string>

class vtkPVGenericRenderWindowInteractor;
class vtkCallbackCommand;
class vtkObject;

class vtkGenericRenderWindowAndInteractorWidget
{
public:
  // Creates an instance of this class with sessionID, which is the session
  // ID of the current ParaView client server connection.  The constructor
  // will create an instance of vtkSMRenderViewProxy using this session ID.
  vtkGenericRenderWindowAndInteractorWidget(int sessionID);
  virtual ~vtkGenericRenderWindowAndInteractorWidget();

  // Event handling
  void draw(){rvproxy->StillRender();}
  void resize(int x, int y);
  void keyPress(int keyCode, int stateMask);
  void keyRelease(int keyCode, int stateMask);
  void mouseUp(int button, int count, int stateMask, int x, int y);
  void mouseDown(int button, int count, int stateMask, int x, int y);
  void mouseMove(int x, int y);
  void mouseWheel(int x);
  void onEnter();
  void onLeave();

  // These methods must be either callbacks or overloaded.  If called from 
  // Java, SWIG provides directors to overload these.
  virtual void Frame() = 0;
  virtual void MakeCurrent() = 0;
  virtual bool IsCurrent() = 0;
  virtual bool IsDirect() = 0;

  // Timer related code
  virtual void CreateTimer( int timerId, int timerType, long duration ) = 0;
  virtual void DestroyTimer( int platformTimerId ) = 0;

  // Data access
  vtkGenericOpenGLRenderWindow* get_render_window(){return vtkWin;}

  const char* GetRenderViewProxyGlobalID();

  static void load_graphics(vtkGenericRenderWindowAndInteractorWidget* gvtkw);
  static void unload_graphics(vtkGenericRenderWindowAndInteractorWidget* gvtkw);
  void timeout();
protected:
  void Start();
  void End();
  // Handle all the callbacks
  static void CallbackHandler(vtkObject* caller, unsigned long vtk_event, void* client_data, void* call_data);

private:
  vtkSmartPointer<vtkGenericOpenGLRenderWindow> vtkWin;
  vtkSmartPointer<vtkPVGenericRenderWindowInteractor> vtkIRen;
  vtkSMRenderViewProxy* rvproxy;

  // Handle the VTK callbacks
  vtkSmartPointer<vtkCallbackCommand> mCallbackHandler;

};

#endif

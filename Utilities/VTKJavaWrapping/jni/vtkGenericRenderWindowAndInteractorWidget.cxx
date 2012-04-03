
#include "vtkGenericRenderWindowAndInteractorWidget.h"
#include "vtkObjectFactory.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkProcessModule.h"
#include "vtkSMSession.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkPVGenericRenderWindowInteractor.h"
#include "vtkCallbackCommand.h"

vtkGenericRenderWindowAndInteractorWidget::vtkGenericRenderWindowAndInteractorWidget(int sessionID)
{
  if(!vtkProcessModule::GetProcessModule())
    {
    cerr << "vtkProcessModule is NULL!" << endl;
    return;
    }

  vtkSMSession* session = vtkSMSession::SafeDownCast(vtkProcessModule::GetProcessModule()->GetSession((vtkIdType) sessionID));

  if(!session)
    {
    cerr << "vtkSMSession ID " << sessionID << " is NULL!" << endl;
    return;
    }

  this->rvproxy = vtkSMRenderViewProxy::SafeDownCast(session->GetSessionProxyManager()->NewProxy("views","RenderView"));

  if(!rvproxy)
    {
    cerr << "vtkSMRenderViewProxy is NULL!" << endl;
    return;
    }

  session->GetSessionProxyManager()->RegisterProxy("views", rvproxy->GetGlobalIDAsString(), rvproxy);

  vtkWin.TakeReference(vtkGenericOpenGLRenderWindow::SafeDownCast(this->rvproxy->GetRenderWindow()));
  vtkIRen.TakeReference(this->rvproxy->GetInteractor());
  vtkIRen->SetEnableDelayedSwitchToNonInteractiveRendering(0);

  mCallbackHandler = vtkSmartPointer<vtkCallbackCommand>::New();

  mCallbackHandler->SetCallback(vtkGenericRenderWindowAndInteractorWidget::CallbackHandler);
  mCallbackHandler->SetClientData(this);

  vtkWin->RemoveAllObservers();
  vtkWin->AddObserver(vtkCommand::StartEvent, mCallbackHandler);
  vtkWin->AddObserver(vtkCommand::EndEvent, mCallbackHandler);
  vtkWin->AddObserver(vtkCommand::WindowFrameEvent, mCallbackHandler);
  vtkWin->AddObserver(vtkCommand::WindowIsCurrentEvent, mCallbackHandler);
  vtkWin->AddObserver(vtkCommand::WindowMakeCurrentEvent, mCallbackHandler);
  vtkWin->AddObserver(vtkCommand::UserEvent, mCallbackHandler);
}

vtkGenericRenderWindowAndInteractorWidget::~vtkGenericRenderWindowAndInteractorWidget()
{
  if(this->rvproxy)
    this->rvproxy->Delete();
}

const char* vtkGenericRenderWindowAndInteractorWidget::GetRenderViewProxyGlobalID()
{
  if(this->rvproxy)
    return(this->rvproxy->GetGlobalIDAsString());
  else
    return(NULL);
}

void vtkGenericRenderWindowAndInteractorWidget::Start()
{
  MakeCurrent();
  
  vtkWin->PushState();
  vtkWin->OpenGLInit();
}

void vtkGenericRenderWindowAndInteractorWidget::End()
{
  vtkWin->PopState();
}

/*! handle resize event
 */
void vtkGenericRenderWindowAndInteractorWidget::resize(int w, int h)
{
  if(!this->vtkWin)
  {
    return;
  }

  this->vtkWin->SetSize(w, h);
  this->vtkIRen->UpdateSize(w, h);
}

void vtkGenericRenderWindowAndInteractorWidget::CallbackHandler(vtkObject* caller, unsigned long vtk_event, void* client_data, void* call_data)
{
  vtkGenericRenderWindowAndInteractorWidget* self = reinterpret_cast<vtkGenericRenderWindowAndInteractorWidget*>(client_data);

  if (vtk_event == vtkCommand::WindowIsCurrentEvent)
  {
    bool* ptr = reinterpret_cast<bool*>(call_data);
    *ptr = self->IsCurrent();
  }
  else if (vtk_event == vtkCommand::WindowMakeCurrentEvent)
  {
    self->MakeCurrent();
  }
  else if (vtk_event == vtkCommand::WindowFrameEvent)
  {
    self->Frame();
  }
  else if (vtk_event == vtkCommand::StartEvent)
  {
    self->Start();
  }
  else if (vtk_event == vtkCommand::EndEvent) 
  {
    self->End();
  }
  else if(vtk_event == vtkCommand::UserEvent)
  {
    int* ptr = reinterpret_cast<int*>(call_data);
    *ptr = self->IsDirect();
  }

}

// Event handling all works by setting up the event information and then
// firing off the appropriate event.
void vtkGenericRenderWindowAndInteractorWidget::keyPress(int keyCode, int stateMask)
{


  // TODO: this assumes that a keyCode will map to a char.  Maybe it should just recieve the char?
  // TODO: I have no idea what the real mask is here!
  int ctrl  = stateMask & (1 << 18);
  int shift = stateMask & (1 << 17);

  vtkIRen->SetEventInformation(0,0, ctrl, shift, (char) keyCode);

  // Special handling of keys that need Java GUI
  vtkIRen->KeyPressEvent();

  switch( keyCode )
  { 
  case 127:
    //bool status = false;
    //status = MessageBox( "Do you want to delete the entity?");
    //PRINT_INFO("User answer: %b\n",status);
    break;

  default:
    break;
  }
  

}

void vtkGenericRenderWindowAndInteractorWidget::keyRelease(int keyCode, int stateMask)
{
  // TODO: I have no idea what the real mask is here!
  int ctrl  = stateMask & (1 << 18);
  int shift = stateMask & (1 << 17);

  vtkIRen->SetEventInformation(0,0,ctrl, shift, (char) keyCode);

  // fire off the event 
  vtkIRen->KeyReleaseEvent();
}

void vtkGenericRenderWindowAndInteractorWidget::mouseUp(int button, int count, int stateMask, int x, int y)
{
  // TODO: I have no idea what the real mask is here!
  int ctrl  = stateMask & (1 << 18);
  int shift = stateMask & (1 << 17);

  vtkIRen->SetEventInformation(x, y, ctrl, shift, 0, count);

  switch (button)
  {
  case 3:
    vtkIRen->RightButtonReleaseEvent();
    break;
  case 2:
    vtkIRen->MiddleButtonReleaseEvent();
    break;
  case 1:
    vtkIRen->LeftButtonReleaseEvent();
    break;
  default:
    break;
  }
}

void vtkGenericRenderWindowAndInteractorWidget::mouseDown(int button, int count, int stateMask, int x, int y)
{
  int ctrl  = stateMask & (1 << 18);
  int shift = stateMask & (1 << 17);

  vtkIRen->SetEventInformation(x, y, ctrl, shift, 0, count);

  switch (button)
  {
  case 3:
    vtkIRen->RightButtonPressEvent();
    break;
  case 2:
    vtkIRen->MiddleButtonPressEvent();
    break;
  case 1:
    vtkIRen->LeftButtonPressEvent();
    break;
  default:
    break;
  }
}

void vtkGenericRenderWindowAndInteractorWidget::mouseMove(int x, int y)
{
    timeout();

  vtkIRen->SetEventInformation(x, y);  
  vtkIRen->MouseMoveEvent(); 

}

void vtkGenericRenderWindowAndInteractorWidget::mouseWheel(int x)
{
  if (x > 0)
    vtkIRen->MouseWheelForwardEvent();
  else if (x < 0)
    vtkIRen->MouseWheelBackwardEvent();
}

void vtkGenericRenderWindowAndInteractorWidget::load_graphics(vtkGenericRenderWindowAndInteractorWidget* gvtkw)
{

}

void vtkGenericRenderWindowAndInteractorWidget::unload_graphics(vtkGenericRenderWindowAndInteractorWidget* gvtkw)
{

}


void vtkGenericRenderWindowAndInteractorWidget::timeout() 
{
  
}
 
void vtkGenericRenderWindowAndInteractorWidget::onEnter()
{

}


void vtkGenericRenderWindowAndInteractorWidget::onLeave()
{

}

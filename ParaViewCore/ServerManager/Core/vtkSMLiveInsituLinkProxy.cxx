/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMLiveInsituLinkProxy.h"

#include "vtkClientServerStream.h"
#include "vtkCommand.h"
#include "vtkLiveInsituLink.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPVXMLElement.h"
#include "vtkPVXMLParser.h"
#include "vtkSMMessage.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMSessionProxyManager.h"

#include <vtksys/ios/sstream>

class vtkSMLiveInsituLinkProxy::vtkInternals
{
public:
  typedef std::map<std::string, vtkSmartPointer<vtkSMProxy> >
    ExtractProxiesType;
  ExtractProxiesType ExtractProxies;
};

vtkStandardNewMacro(vtkSMLiveInsituLinkProxy);
//----------------------------------------------------------------------------
vtkSMLiveInsituLinkProxy::vtkSMLiveInsituLinkProxy() :
  Internals(new vtkInternals())
{
  this->StateDirty = false;
}

//----------------------------------------------------------------------------
vtkSMLiveInsituLinkProxy::~vtkSMLiveInsituLinkProxy()
{
  delete this->Internals;
  this->Internals = NULL;
}

//----------------------------------------------------------------------------
vtkSMSessionProxyManager* vtkSMLiveInsituLinkProxy::GetInsituProxyManager()
{
  return this->InsituProxyManager;
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::SetInsituProxyManager(
  vtkSMSessionProxyManager* pxm)
{
  this->InsituProxyManager = pxm;
  if (pxm)
    {
    pxm->AddObserver(vtkCommand::PropertyModifiedEvent,
      this, &vtkSMLiveInsituLinkProxy::MarkStateDirty);
    }
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::LoadState(
  const vtkSMMessage* msg, vtkSMProxyLocator* locator)
{
  if (msg->HasExtension(ChatMessage::author)
    && msg->HasExtension(ChatMessage::txt))
    {
    cout << "Received message" << endl;
    switch (msg->GetExtension(ChatMessage::author))
      {
    case vtkLiveInsituLink::CONNECTED:
      this->InsituConnected(msg->GetExtension(ChatMessage::txt).c_str());
      break;

    case vtkLiveInsituLink::NEXT_TIMESTEP_AVAILABLE:
      this->NewTimestepAvailable();
      break;
      }
    }
  else
    {
    this->Superclass::LoadState(msg, locator);
    }
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::CreateVTKObjects()
{
  if (this->ObjectsCreated)
    {
    return;
    }

  this->Superclass::CreateVTKObjects();
  if (this->ObjectsCreated)
    {
    vtkClientServerStream stream;
    stream << vtkClientServerStream::Invoke
           << VTKOBJECT(this)
           << "SetProxyId"
           << static_cast<unsigned int>(this->GetGlobalID())
           << vtkClientServerStream::End;
    this->ExecuteStream(stream);
    }
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::InsituConnected(const char* state)
{
  // new insitu session connected.
  // Obtain the state from vtkLiveInsituLink instance.
  vtkNew<vtkPVXMLParser> parser;
  if (parser->Parse(state))
    {
    this->InsituProxyManager->LoadXMLState(parser->GetRootElement());
    this->StateDirty = false;
    }
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::NewTimestepAvailable()
{
  // Mark all extract producer proxies as dirty.
  vtkInternals::ExtractProxiesType::iterator iter;
  for (iter = this->Internals->ExtractProxies.begin();
    iter != this->Internals->ExtractProxies.end();
    ++iter)
    {
    iter->second->MarkModified(iter->second.GetPointer());
    }

  if (this->StateDirty)
    {
    cout << "Push new state to server." << endl;
    // push new state.
    vtkPVXMLElement* root = this->InsituProxyManager->SaveXMLState();
    vtksys_ios::ostringstream data;
    root->PrintXML(data, vtkIndent());
    root->Delete();

    vtkClientServerStream stream;
    stream << vtkClientServerStream::Invoke
           << VTKOBJECT(this)
           << "UpdateInsituXMLState"
           << data.str().c_str()
           << vtkClientServerStream::End;
    this->ExecuteStream(stream);

    this->StateDirty = false;
    }
}

//----------------------------------------------------------------------------
bool vtkSMLiveInsituLinkProxy::HasExtract(
    const char* reg_group, const char* reg_name, int port_number)
{
  vtksys_ios::ostringstream key;
  key << reg_group << ":" << reg_name << ":" << port_number;
  return (this->Internals->ExtractProxies.find(key.str()) !=
    this->Internals->ExtractProxies.end());
}

//----------------------------------------------------------------------------
vtkSMProxy* vtkSMLiveInsituLinkProxy::CreateExtract(
  const char* reg_group, const char* reg_name, int port_number)
{
  vtkSMProxy* proxy = this->GetSessionProxyManager()->NewProxy("sources",
    "PVTrivialProducer");
  proxy->UpdateVTKObjects();

  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
    << VTKOBJECT(this)
    << "RegisterExtract"
    << VTKOBJECT(proxy)
    << reg_group << reg_name << port_number
    << vtkClientServerStream::End;
  this->ExecuteStream(stream);

  vtksys_ios::ostringstream key;
  key << reg_group << ":" << reg_name << ":" << port_number;
  this->Internals->ExtractProxies[key.str()] = proxy;
  proxy->Delete();

  return proxy;
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

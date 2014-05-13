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
#include "vtkCommand.h"
#include "vtkLiveInsituLink.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPVCatalystSessionCore.h"
#include "vtkPVDataInformation.h"
#include "vtkPVXMLElement.h"
#include "vtkPVXMLParser.h"
#include "vtkSMMessage.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMSession.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMStateLoader.h"

#include <vtksys/ios/sstream>
//#define vtkSMLiveInsituLinkProxyDebugMacro(x) cerr << __LINE__ << " " x << endl;
#define vtkSMLiveInsituLinkProxyDebugMacro(x)

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
  this->LiveChangedCalled = false;
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
    this->CatalystSessionCore =
        vtkPVCatalystSessionCore::SafeDownCast(pxm->GetSession()->GetSessionCore());
    pxm->AddObserver(vtkCommand::PropertyModifiedEvent,
      this, &vtkSMLiveInsituLinkProxy::MarkStateDirty);
    }
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::LoadState(
  const vtkSMMessage* msg, vtkSMProxyLocator* locator)
{
  std::vector<vtkTypeUInt32> sourceProxyToMarkModified;
  if(msg->HasExtension(ProxyState::xml_group) &&
      msg->GetExtension(ProxyState::xml_group) == "Catalyst_Communication")
    {
    this->LiveChangedCalled = false;
    int numberOfUserData = msg->ExtensionSize(ProxyState::user_data);
    for(int i = 0; i < numberOfUserData; ++i)
      {
      const ProxyState_UserData& user_data =
        msg->GetExtension(ProxyState::user_data, i);

      // Process User data
      if (user_data.key() == "LiveAction")
        {
        const Variant& value = user_data.variant(0);
        vtkSMLiveInsituLinkProxyDebugMacro(<< "Received message");
        switch (value.integer(0))
          {
        case vtkLiveInsituLink::CONNECTED:
          this->InsituConnected(value.txt(0).c_str());
          this->InvokeEvent(vtkCommand::ConnectionCreatedEvent);
          break;

        case vtkLiveInsituLink::NEXT_TIMESTEP_AVAILABLE:
          this->NewTimestepAvailable();
          break;

        case vtkLiveInsituLink::DISCONNECTED:
          vtkSMLiveInsituLinkProxyDebugMacro(<< "Catalyst disconnected!!!");
          this->InvokeEvent(vtkCommand::ConnectionClosedEvent);
          break;
          }
        }
      else if (user_data.key() == "UpdateDataInformation" && this->CatalystSessionCore)
        {
        int nbVars = user_data.variant_size();
        for(int varIdx = 0; varIdx < nbVars; ++varIdx)
          {
          const Variant& value = user_data.variant(varIdx);
          vtkTypeUInt32 proxyId = value.proxy_global_id(0);
          unsigned int port = value.port_number(0);
          vtkClientServerStream stream;
          stream.SetData((const unsigned char*)value.binary(0).c_str(), value.binary(0).size());
          vtkNew<vtkPVDataInformation> info;
          info->CopyFromStream(&stream);
          vtkTypeUInt32 realId =
              this->CatalystSessionCore->RegisterDataInformation(
                proxyId, port, info.GetPointer());

          // Add to proxy list that needs a refresh...
          sourceProxyToMarkModified.push_back(realId);
          }
        }
      else if (user_data.key() == "IdMapping" && this->CatalystSessionCore)
        {
        const Variant& mapTable = user_data.variant(0);
        int size = mapTable.idtype_size();
        vtkTypeUInt32* map = new vtkTypeUInt32[size];
        for(int idx=0; idx < size; idx += 2)
          {
          map[idx+1] = mapTable.idtype(idx+0);
          map[idx+0] = mapTable.idtype(idx+1);
          }
        this->CatalystSessionCore->UpdateIdMap(map, size);
        delete[] map;
        }
      }
    }
  else
    {
    this->Superclass::LoadState(msg, locator);
    }

  // Update source proxy that have new data information
  for(size_t i = 0; i < sourceProxyToMarkModified.size(); ++i)
    {
    vtkObject* obj = this->InsituProxyManager->GetSession()->GetRemoteObject(sourceProxyToMarkModified[i]);
    vtkSMSourceProxy* source = vtkSMSourceProxy::SafeDownCast(obj);
    if(source)
      {
      source->MarkModified(NULL);
      source->UpdatePipeline();
      }
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
    vtkNew<vtkSMStateLoader> loader;
    loader->SetSessionProxyManager(this->InsituProxyManager);
    loader->KeepIdMappingOn();
    this->InsituProxyManager->LoadXMLState(parser->GetRootElement(), loader.GetPointer());

    // Update session core id mapping
    int size = 0;
    vtkTypeUInt32* mapArray = loader->GetMappingArray(size);
    this->CatalystSessionCore->ResetIdMap();
    this->CatalystSessionCore->UpdateIdMap(mapArray, size);

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

  this->PushUpdatedState();

  this->InvokeEvent(vtkCommand::UpdateEvent);
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::PushUpdatedState()
{
  if (this->StateDirty)
    {
    vtkSMLiveInsituLinkProxyDebugMacro(<< "Push new state to server.");
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
    vtkSMLiveInsituLinkProxyDebugMacro(<< "Push new state to server--done");
    this->ExecuteStream(stream);

    this->StateDirty = false;
    }
}

void vtkSMLiveInsituLinkProxy::MarkStateDirty()
{
  this->StateDirty = true;
  vtkSMLiveInsituLinkProxyDebugMacro(<< "MarkStateDirty");
  this->PushUpdatedState();
  this->LiveChanged();
}


//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::LiveChanged()
{
  int simulationPaused =
    vtkSMPropertyHelper(this, "SimulationPaused").GetAsInt();
  if (! this->LiveChangedCalled && simulationPaused)
    {
    vtkSMLiveInsituLinkProxyDebugMacro(<< "LiveChanged");
    this->InvokeCommand("LiveChanged");
    this->LiveChangedCalled = true;
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
  this->LiveChanged();

  return proxy;
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::RemoveExtract(vtkSMProxy* proxy)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke
    << VTKOBJECT(this)
    << "UnRegisterExtract"
    << VTKOBJECT(proxy)
    << vtkClientServerStream::End;
  this->ExecuteStream(stream);

  vtkInternals::ExtractProxiesType::iterator iter;
  for (iter = this->Internals->ExtractProxies.begin();
    iter != this->Internals->ExtractProxies.end();
    ++iter)
    {
    if (iter->second == proxy)
      {
      this->Internals->ExtractProxies.erase(iter);
      break;
      }
    }
  this->LiveChanged();
}

//----------------------------------------------------------------------------
void vtkSMLiveInsituLinkProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

/*=========================================================================

  Program:   ParaView
  Module:    vtkGlyph3DRepresentation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkGlyph3DRepresentation.h"

#include "vtkCompositePolyDataMapper2.h"
#include "vtkDataObject.h"
#include "vtkGlyph3DMapper.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMPIMoveData.h"
#include "vtkObjectFactory.h"
#include "vtkPVArrowSource.h"
#include "vtkPVLODActor.h"
#include "vtkPVRenderView.h"
#include "vtkPVUpdateSuppressor.h"
#include "vtkQuadricClustering.h"
#include "vtkRenderer.h"
#include "vtkUnstructuredDataDeliveryFilter.h"

vtkStandardNewMacro(vtkGlyph3DRepresentation);
//----------------------------------------------------------------------------
vtkGlyph3DRepresentation::vtkGlyph3DRepresentation()
{
  this->SetNumberOfInputPorts(2);

  this->GlyphMapper = vtkGlyph3DMapper::New();
  this->LODGlyphMapper = vtkGlyph3DMapper::New();
  this->GlyphActor = vtkPVLODActor::New();

  this->DataCollector = vtkUnstructuredDataDeliveryFilter::New();
  this->DataCollector->SetOutputDataType(VTK_POLY_DATA);

  this->LODDataCollector = vtkUnstructuredDataDeliveryFilter::New();
  this->LODDataCollector->SetOutputDataType(VTK_POLY_DATA);

  this->GlyphUpdateSuppressor = vtkPVUpdateSuppressor::New();
  this->LODGlyphUpdateSuppressor = vtkPVUpdateSuppressor::New();

  this->DummySource = vtkPVArrowSource::New();

  this->GlyphMapper->SetInputConnection(0,
    this->Mapper->GetInputConnection(0, 0));
  this->LODGlyphMapper->SetInputConnection(0,
    this->LODMapper->GetInputConnection(0, 0));

  this->GlyphUpdateSuppressor->SetInputConnection(
    this->DataCollector->GetOutputPort());
  this->GlyphMapper->SetInputConnection(
    1, this->GlyphUpdateSuppressor->GetOutputPort());

  this->LODGlyphUpdateSuppressor->SetInputConnection(
    this->LODDataCollector->GetOutputPort());
  this->LODGlyphMapper->SetInputConnection(
    1, this->LODGlyphUpdateSuppressor->GetOutputPort());

  this->GlyphActor->SetMapper(this->GlyphMapper);
  this->GlyphActor->SetLODMapper(this->LODGlyphMapper);
  this->GlyphActor->SetProperty(this->Property);

  // The glyph geometry is always cloned.
  vtkInformation* info = vtkInformation::New();
  info->Set(vtkPVRenderView::DATA_DISTRIBUTION_MODE(),
    vtkMPIMoveData::CLONE);
  this->DataCollector->ProcessViewRequest(info);
  this->LODDataCollector->ProcessViewRequest(info);
  info->Delete();

  this->MeshVisibility = true;
  this->SetMeshVisibility(false);

  this->GlyphMapper->SetInterpolateScalarsBeforeMapping(0);
  this->LODGlyphMapper->SetInterpolateScalarsBeforeMapping(0);
}

//----------------------------------------------------------------------------
vtkGlyph3DRepresentation::~vtkGlyph3DRepresentation()
{
  this->GlyphMapper->Delete();
  this->LODGlyphMapper->Delete();
  this->GlyphActor->Delete();
  this->GlyphUpdateSuppressor->Delete();
  this->LODGlyphUpdateSuppressor->Delete();

  this->DataCollector->Delete();
  this->LODDataCollector->Delete();
  this->DummySource->Delete();
}

//----------------------------------------------------------------------------
bool vtkGlyph3DRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->AddActor(this->GlyphActor);
    }
  return this->Superclass::AddToView(view);
}

//----------------------------------------------------------------------------
bool vtkGlyph3DRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->RemoveActor(this->GlyphActor);
    }
  return this->Superclass::RemoveFromView(view);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetVisibility(bool val)
{
  this->Superclass::SetVisibility(val);
  this->GlyphActor->SetVisibility(val?  1 : 0);
  this->Actor->SetVisibility((val && this->MeshVisibility)? 1 : 0);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetMeshVisibility(bool val)
{
  this->MeshVisibility = val;
  this->Actor->SetVisibility((val && this->MeshVisibility)? 1 : 0);
}

//----------------------------------------------------------------------------
int vtkGlyph3DRepresentation::FillInputPortInformation(int port,
  vtkInformation *info)
{
  if (port == 0)
    {
    return this->Superclass::FillInputPortInformation(port, info);
    }
  else if (port == 1)
    {
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPolyData");
    return 1;
    }

  return 0;
}

//----------------------------------------------------------------------------
int vtkGlyph3DRepresentation::RequestData(vtkInformation* request,
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  this->DataCollector->Modified();
  this->LODDataCollector->Modified();

  if (inputVector[1]->GetNumberOfInformationObjects()==1)
    {
    this->DataCollector->SetInputConnection(this->GetInternalOutputPort(1));
    this->LODDataCollector->SetInputConnection(this->GetInternalOutputPort(1));
    }
  else
    {
    this->DataCollector->RemoveAllInputs();
    this->LODDataCollector->RemoveAllInputs();
    this->DataCollector->SetInputConnection(this->DummySource->GetOutputPort());
    this->LODDataCollector->SetInputConnection(this->DummySource->GetOutputPort());
    }

  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkGlyph3DRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
    {
    return false;
    }

  if (request_type == vtkPVView::REQUEST_PREPARE_FOR_RENDER())
    {
    // In REQUEST_PREPARE_FOR_RENDER, we need to ensure all our data-deliver
    // filters have their states updated as requested by the view.

    // this is where we will look to see on what nodes are we going to render and
    // render set that up.
    if (this->Actor->GetEnableLOD())
      {
      this->LODDataCollector->ProcessViewRequest(inInfo);
      if (this->LODGlyphUpdateSuppressor->GetForcedUpdateTimeStamp() <
        this->LODDataCollector->GetMTime())
        {
        outInfo->Set(vtkPVRenderView::NEEDS_DELIVERY(), 1);
        }
      }
    else
      {
      this->DataCollector->ProcessViewRequest(inInfo);
      if (this->GlyphUpdateSuppressor->GetForcedUpdateTimeStamp() <
        this->DataCollector->GetMTime())
        {
        outInfo->Set(vtkPVRenderView::NEEDS_DELIVERY(), 1);
        }
      }
    }
  else if (request_type == vtkPVView::REQUEST_DELIVERY())
    {
    if (this->Actor->GetEnableLOD())
      {
      this->LODDataCollector->Modified();
      this->LODGlyphUpdateSuppressor->ForceUpdate();
      }
    else
      {
      this->DataCollector->Modified();
      this->GlyphUpdateSuppressor->ForceUpdate();
      }
    }

  return true;
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::UpdateColoringParameters()
{
  this->Superclass::UpdateColoringParameters();
  
  if (this->Mapper->GetScalarVisibility() == 0 ||
    this->Mapper->GetScalarMode() != VTK_SCALAR_MODE_USE_POINT_FIELD_DATA)
    {
    // we are not coloring the glyphs with scalars.
    const char* null = NULL;
    this->GlyphMapper->SetScalarVisibility(0);
    this->LODGlyphMapper->SetScalarVisibility(0);
    this->GlyphMapper->SelectColorArray(null);
    this->LODGlyphMapper->SelectColorArray(null);
    return;
    }

  this->GlyphMapper->SetScalarVisibility(1);
  this->GlyphMapper->SelectColorArray(this->ColorArrayName);
  this->GlyphMapper->SetUseLookupTableScalarRange(1);
  this->GlyphMapper->SetScalarMode(
    VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);

  this->LODGlyphMapper->SetScalarVisibility(1);
  this->LODGlyphMapper->SelectColorArray(this->ColorArrayName);
  this->LODGlyphMapper->SetUseLookupTableScalarRange(1);
  this->LODGlyphMapper->SetScalarMode(
    VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//**************************************************************************
// Overridden to forward to vtkGlyph3DMapper
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetLookupTable(vtkScalarsToColors* val)
{
  this->GlyphMapper->SetLookupTable(val);
  this->LODGlyphMapper->SetLookupTable(val);
  this->Superclass::SetLookupTable(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetMapScalars(int val)
{
  this->GlyphMapper->SetColorMode(val);
  this->LODGlyphMapper->SetColorMode(val);
  this->Superclass::SetMapScalars(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetInterpolateScalarsBeforeMapping(int val)
{
  // The GlyphMapper does not support InterpolateScalarsBeforeMapping==1. So
  // leave it at 0.
  this->Superclass::SetInterpolateScalarsBeforeMapping(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetStatic(int val)
{
  this->GlyphMapper->SetStatic(val);
  this->LODGlyphMapper->SetStatic(val);
  this->Superclass::SetStatic(val);
}

//**************************************************************************
// Forwarded to vtkGlyph3DMapper
//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetMaskArray(const char* val)
{
  this->GlyphMapper->SetMaskArray(val);
  this->LODGlyphMapper->SetMaskArray(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetScaleArray(const char* val)
{
  this->GlyphMapper->SetScaleArray(val);
  this->LODGlyphMapper->SetScaleArray(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetOrientationArray(const char* val)
{
  this->GlyphMapper->SetOrientationArray(val);
  this->LODGlyphMapper->SetOrientationArray(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetScaling(bool val)
{
  this->GlyphMapper->SetScaling(val);
  this->LODGlyphMapper->SetScaling(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetScaleMode(int val)
{
  this->GlyphMapper->SetScaleMode(val);
  this->LODGlyphMapper->SetScaleMode(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetScaleFactor(double val)
{
  this->GlyphMapper->SetScaleFactor(val);
  this->LODGlyphMapper->SetScaleFactor(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetOrient(bool val)
{
  this->GlyphMapper->SetOrient(val);
  this->LODGlyphMapper->SetOrient(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetOrientationMode(int val)
{
  this->GlyphMapper->SetOrientationMode(val);
  this->LODGlyphMapper->SetOrientationMode(val);
}

//----------------------------------------------------------------------------
void vtkGlyph3DRepresentation::SetMasking(bool val)
{
  this->GlyphMapper->SetMasking(val);
  this->LODGlyphMapper->SetMasking(val);
}

//----------------------------------------------------------------------------
double* vtkGlyph3DRepresentation::GetBounds()
{
  return this->GlyphMapper->GetBounds();
}
